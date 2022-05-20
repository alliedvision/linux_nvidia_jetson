/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asm-generic/errno-base.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include "pva_dma.h"
#include <linux/delay.h>
#include <asm/ioctls.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/nvhost.h>
#include <linux/cvnas.h>

#include <trace/events/nvhost.h>

#ifdef CONFIG_EVENTLIB
#include <linux/keventlib.h>
#include <uapi/linux/nvdev_fence.h>
#include <uapi/linux/nvhost_events.h>
#endif

#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include "nvhost_syncpt_unit_interface.h"
#include "nvhost_gos.h"
#include <linux/seq_file.h>
#include "pva.h"
#include "nvhost_buffer.h"
#include "nvpva_queue.h"
#include "pva_mailbox.h"
#include "pva_queue.h"
#include "dev.h"
#include "pva_regs.h"
#include "t194/hardware_t194.h"
#include "pva-vpu-perf.h"
#include "pva-interface.h"
#include "pva_vpu_exe.h"
#include "nvpva_client.h"

#include <uapi/linux/nvpva_ioctl.h>
#include <trace/events/nvhost_pva.h>

static void pva_task_dump(struct pva_submit_task *task)
{
	int i;

	nvhost_dbg_info("task=%p, exe_id=%u", task, task->exe_id);

	for (i = 0; i < task->num_input_task_status; i++)
		nvhost_dbg_info("input task status %d: pin_id=%u, offset=%u", i,
				task->input_task_status[i].pin_id,
				task->input_task_status[i].offset);

	for (i = 0; i < task->num_output_task_status; i++)
		nvhost_dbg_info("output task status %d: pin_id=%u, offset=%u",
				i, task->output_task_status[i].pin_id,
				task->output_task_status[i].offset);

	for (i = 0; i < task->num_user_fence_actions; i++)
		nvhost_dbg_info("fence action %d: type=%u", i,
				task->user_fence_actions[i].type);
}

static void pva_task_get_memsize(size_t *dma_size, size_t *kmem_size)
{
	/* Align task addr to 64bytes boundary for DMA use*/
	*dma_size = ALIGN(sizeof(struct pva_hw_task) + 64, 64);
	*kmem_size = sizeof(struct pva_submit_task);
}

static inline void nvpva_fetch_task_status_info(struct pva *pva,
						struct pva_task_error_s *info)
{
	struct pva_task_error_s *err_array = pva->priv_circular_array.va;
	struct pva_task_error_s *src_va =
		&err_array[pva->circular_array_rd_pos];
	const u32 len = MAX_PVA_TASK_COUNT;

	pva->circular_array_rd_pos += 1;
	WARN_ON(pva->circular_array_rd_pos > len);
	if (pva->circular_array_rd_pos >= len)
		pva->circular_array_rd_pos = 0;

	/* Cache coherency is guaranteed by DMA API */
	(void)memcpy(info, src_va, sizeof(struct pva_task_error_s));
	/* clear it for debugging */
	(void)memset(src_va, 0, sizeof(struct pva_task_error_s));
}

static void pva_task_unpin_mem(struct pva_submit_task *task)
{
	u32 i;

	for (i = 0; i < task->num_pinned; i++) {
		struct pva_pinned_memory *mem = &task->pinned_memory[i];

		nvhost_buffer_submit_unpin(task->client->buffers, &mem->dmabuf,
					   1);
		dma_buf_put(mem->dmabuf);
	}
	task->num_pinned = 0;
}

struct pva_pinned_memory *pva_task_pin_mem(struct pva_submit_task *task,
					   u32 dmafd)
{
	int err;
	struct pva_pinned_memory *mem;

	if (task->num_pinned >= ARRAY_SIZE(task->pinned_memory)) {
		task_err(task, "too many objects to pin");
		err = -ENOMEM;
		goto err_out;
	}

	if (!dmafd) {
		task_err(task, "pin_id is 0");
		err = -EFAULT;
		goto err_out;
	}

	mem = &task->pinned_memory[task->num_pinned];
	mem->fd = dmafd;
	mem->dmabuf = dma_buf_get(dmafd);
	if (IS_ERR_OR_NULL(mem->dmabuf)) {
		task_err(task, "can't get dmabuf from pin_id: %ld",
			 PTR_ERR(mem->dmabuf));
		err = -EFAULT;
		goto err_out;
	}
	err = nvhost_buffer_submit_pin(task->client->buffers, &mem->dmabuf, 1,
				       &mem->dma_addr, &mem->size, &mem->heap);
	if (err) {
		task_err(task, "submit pin failed; Is the handled pinned?");
		goto err_out;
	}

	task->num_pinned += 1;
	return mem;
err_out:
	return ERR_PTR(err);
}

/* pin fence and return its dma addr */
static int pva_task_pin_fence(struct pva_submit_task *task,
			      struct nvpva_submit_fence *fence,
			      dma_addr_t *addr)
{
	int err = 0;

	switch (fence->type) {
	case NVPVA_FENCE_OBJ_SEM: {
		struct pva_pinned_memory *mem;

		mem = pva_task_pin_mem(task, fence->obj.sem.mem.pin_id);
		if (IS_ERR(mem)) {
			task_err(task, "sempahore submit pin failed");
			err = PTR_ERR(mem);
		} else
			*addr = mem->dma_addr + fence->obj.sem.mem.offset;
		break;
	}
	case NVPVA_FENCE_OBJ_SYNCPT: {
		dma_addr_t syncpt_addr = nvhost_syncpt_gos_address(
			task->pva->pdev, fence->obj.syncpt.id);
		if (!syncpt_addr)
			syncpt_addr = nvhost_syncpt_address(
				task->queue->vm_pdev, fence->obj.syncpt.id);
		if (syncpt_addr)
			*addr = syncpt_addr;
		else {
			task_err(
				task,
				"%s: can't get syncpoint address", __func__);
			err = -EINVAL;
		}
		break;
	}
	default:
		err = -EINVAL;
		task_err(task, "%s: unsupported fence type: %d",
			 __func__, fence->type);
		break;
	}
	return err;
}

static int get_fence_value(struct nvpva_submit_fence *fence, u32 *val)
{
	int err = 0;

	switch (fence->type) {
	case NVPVA_FENCE_OBJ_SYNCPT:
		*val = fence->obj.syncpt.value;
		break;
	case NVPVA_FENCE_OBJ_SEM:
		*val = fence->obj.sem.value;
		break;
	default:
		err = -EINVAL;
		pr_err("%s: unsupported fence type: %d",
		       __func__, fence->type);
		break;
	}
	return err;
}

static inline void
pva_task_write_fence_action_op(struct pva_task_action_s *op,
				uint8_t action,
				uint64_t fence_addr,
				uint32_t val,
				uint64_t time_stamp_addr)
{
	op->action	= action;
	op->args.ptr.p	= fence_addr;
	op->args.ptr.v	= val;
	op->args.ptr.t	= time_stamp_addr;
}

static inline void pva_task_write_status_action_op(struct pva_task_action_s *op,
						   uint8_t action,
						   uint64_t addr,
						   uint16_t val)
{
	op->action = action;
	op->args.status.p = addr;
	op->args.status.status = val;
}

static inline void pva_task_write_stats_action_op(struct pva_task_action_s *op,
						  uint8_t action, uint64_t addr)
{
	op->action = action;
	op->args.statistics.p = addr;
}

static int pva_task_process_fence_actions(struct pva_submit_task *task,
					  struct pva_hw_task *hw_task)
{
	int err = 0;
	u32 i;
	u32 fence_type;
	u8 *action_counter;
	u8 action_code;
	struct pva_task_action_s *fw_actions;
	struct pva_task_action_s *current_fw_actions;

	for (fence_type = NVPVA_FENCE_SOT_R5;
	     fence_type < NVPVA_MAX_FENCE_TYPES; fence_type++) {
		switch (fence_type) {
		case NVPVA_FENCE_SOT_R5:
			fw_actions = &hw_task->preactions[0];
			action_code = TASK_ACT_PTR_WRITE_SOT_R;
			action_counter = &hw_task->task.num_preactions;
			break;
		case NVPVA_FENCE_SOT_VPU:
			fw_actions = &hw_task->preactions[0];
			action_code = TASK_ACT_PTR_WRITE_SOT_V;
			action_counter = &hw_task->task.num_preactions;
			break;
		case NVPVA_FENCE_EOT_R5:
			fw_actions = &hw_task->postactions[0];
			action_code = TASK_ACT_PTR_WRITE_EOT_R;
			action_counter = &hw_task->task.num_postactions;
			break;
		case NVPVA_FENCE_EOT_VPU:
			fw_actions = &hw_task->postactions[0];
			action_code = TASK_ACT_PTR_WRITE_EOT_V;
			action_counter = &hw_task->task.num_postactions;
			break;
		case NVPVA_FENCE_POST:
			fw_actions = &hw_task->postactions[0];
			action_code = TASK_ACT_PTR_WRITE_EOT;
			action_counter = &hw_task->task.num_postactions;
			break;
		default:
			task_err(task, "unknown fence action type");
			err = -EINVAL;
			goto out;
		}

		for (i = 0; i < task->num_pva_fence_actions[fence_type]; i++) {
			struct nvpva_fence_action *fence_action =
			    &task->pva_fence_actions[fence_type][i];
			dma_addr_t gos_addr = 0;
			dma_addr_t fence_addr;
			u32 fence_value;
			dma_addr_t timestamp_addr;
			switch (fence_action->fence.type) {
			case NVPVA_FENCE_OBJ_SYNCPT:
			{
				u32 id = task->queue->syncpt_id;
				fence_action->fence.obj.syncpt.id = id;
				fence_addr = nvhost_syncpt_address(
				    task->queue->vm_pdev, id);
				gos_addr = nvhost_syncpt_gos_address(
				    task->pva->pdev, id);
				if (fence_addr == 0) {
					err = -EFAULT;
					goto out;
				}
				task->fence_num += 1;
				task->syncpt_thresh += 1;
				fence_value = 1;
				fence_action->fence.obj.syncpt.value =
				    task->syncpt_thresh;
				break;
			}
			case NVPVA_FENCE_OBJ_SEM:
			{
				err = pva_task_pin_fence(task,
							 &fence_action->fence,
							 &fence_addr);
				if (err)
					goto out;
				task->sem_num += 1;
				task->sem_thresh += 1;
				fence_value = task->sem_thresh;
				fence_action->fence.obj.sem.value = fence_value;
				break;
			}
			default:
				task_err(task, "unknown fence action object");
				err = -EINVAL;
				goto out;
			}
			if (fence_action->timestamp_buf.pin_id) {
				struct pva_pinned_memory *mem;
				mem = pva_task_pin_mem(
				    task,
				    fence_action->timestamp_buf.pin_id);
				if (IS_ERR(mem)) {
					err = PTR_ERR(mem);
					task_err(
					    task,
					    "failed to pin timestamp buffer");
					goto out;
				}
				timestamp_addr =
				    mem->dma_addr +
				    fence_action->timestamp_buf.offset;
			} else {
				timestamp_addr = 0;
			}
			current_fw_actions = &fw_actions[*action_counter];
			pva_task_write_fence_action_op(current_fw_actions,
						       action_code, fence_addr,
						       fence_value,
						       timestamp_addr);
			*action_counter = *action_counter + 1;
			if (gos_addr) {
				current_fw_actions =
					&fw_actions[*action_counter];
				pva_task_write_fence_action_op(
				    current_fw_actions, action_code,
				    gos_addr,
				    fence_action->fence.obj.syncpt.value,
				    timestamp_addr);
				*action_counter = *action_counter + 1;
			}
		}
	}
out:
	return err;
}
static int pva_task_process_prefences(struct pva_submit_task *task,
				      struct pva_hw_task *hw_task)
{
	u32 i;
	int err;
	struct pva_task_action_s *fw_preactions = NULL;
	for (i = 0; i < task->num_prefences; i++) {
		struct nvpva_submit_fence *fence = &task->prefences[i];
		dma_addr_t fence_addr;
		u32 fence_val;
		err = pva_task_pin_fence(task, fence, &fence_addr);
		if (err)
			goto out;
		if (fence_addr == 0) {
			err = -EINVAL;
			goto out;
		}
		err = get_fence_value(fence, &fence_val);
		if (err)
			goto out;
		fw_preactions =
			&hw_task->preactions[hw_task->task.num_preactions];
		pva_task_write_fence_action_op(fw_preactions,
						TASK_ACT_PTR_BLK_GTREQL,
						fence_addr, fence_val, 0);
		++hw_task->task.num_preactions;
	}
out:
	return err;
}
static int pva_task_process_input_status(struct pva_submit_task *task,
					 struct pva_hw_task *hw_task)
{
	u32 i;
	int err = 0;
	struct pva_task_action_s *fw_preactions = NULL;

	for (i = 0; i < task->num_input_task_status; i++) {
		struct nvpva_mem *status = &task->input_task_status[i];
		struct pva_pinned_memory *mem =
		    pva_task_pin_mem(task, status->pin_id);
		dma_addr_t status_addr;
		if (IS_ERR(mem)) {
			err = PTR_ERR(mem);
			goto out;
		}

		status_addr = mem->dma_addr + status->offset;

		fw_preactions =
			&hw_task->preactions[hw_task->task.num_preactions];
		pva_task_write_status_action_op(fw_preactions,
						(uint8_t)TASK_ACT_READ_STATUS,
						status_addr, 0U);
		++hw_task->task.num_preactions;
	}
out:
	return err;
}
static int pva_task_process_output_status(struct pva_submit_task *task,
					  struct pva_hw_task *hw_task)
{
	u32 i;
	int err = 0;
	dma_addr_t stats_addr;
	struct pva_task_action_s *fw_postactions = NULL;

	for (i = 0; i < task->num_output_task_status; i++) {
		struct nvpva_mem *status = &task->output_task_status[i];
		struct pva_pinned_memory *mem =
		    pva_task_pin_mem(task, status->pin_id);
		dma_addr_t status_addr;
		if (IS_ERR(mem)) {
			err = PTR_ERR(mem);
			goto out;
		}
		status_addr = mem->dma_addr + status->offset;
		fw_postactions = &hw_task->postactions[hw_task->task.num_postactions];
		pva_task_write_status_action_op(fw_postactions,
						(uint8_t)TASK_ACT_WRITE_STATUS,
						status_addr,
						1U /* PVA task error code */);
		++hw_task->task.num_postactions;
	}
	stats_addr = task->dma_addr + offsetof(struct pva_hw_task, statistics);
	fw_postactions = &hw_task->postactions[hw_task->task.num_postactions];
	pva_task_write_stats_action_op(fw_postactions,
				       (uint8_t)TASK_ACT_PVA_STATISTICS,
				       stats_addr);
	++hw_task->task.num_postactions;

out:
	return err;
}
static int pva_task_write_vpu_parameter(struct pva_submit_task *task,
					struct pva_hw_task *hw_task)
{
	int err = 0;
	struct pva_elf_image *elf = NULL;
	struct nvpva_pointer_symbol *ptrSym = NULL;
	u32 symbolId = 0U;
	dma_addr_t symbol_payload = 0U;
	u32 size = 0U;
	u32 i;
	if (task->exe_id == NVPVA_NOOP_EXE_ID)
		return err;
	elf = get_elf_image(&task->client->elf_ctx, task->exe_id);
	if (task->num_symbols > elf->num_symbols) {
		task_err(task, "invalid number of symbols");
		return -EINVAL;
	}
	memcpy(hw_task->sym_payload, task->symbol_payload,
	       task->symbol_payload_size);
	symbol_payload =
	    task->dma_addr + offsetof(struct pva_hw_task, sym_payload);
	for (i = 0U; i < task->num_symbols; i++) {
		symbolId = task->symbols[i].symbol.id;
		size = elf->sym[symbolId].size;
		if (task->symbols[i].symbol.size != size) {
			task_err(task, "size does not match symbol:%s",
				 elf->sym[symbolId].symbol_name);
			return -EINVAL;
		}
		if (task->symbols[i].config == NVPVA_SYMBOL_POINTER) {
			struct pva_pinned_memory *mem;
			ptrSym = (struct nvpva_pointer_symbol *)
					 (hw_task->sym_payload +
					 task->symbols[i].offset);
			mem = pva_task_pin_mem(task, ptrSym->base);
			if (IS_ERR(mem)) {
				err = PTR_ERR(mem);
				task_err(task, "failed to pin symbol pointer");
				return -EINVAL;
			}
			ptrSym->base = mem->dma_addr;
			ptrSym->size = mem->size;
			size = sizeof(struct nvpva_pointer_symbol);
		}
		hw_task->param_list[i].addr = elf->sym[symbolId].addr;
		hw_task->param_list[i].size = size;
		hw_task->param_list[i].param_base =
		    symbol_payload + task->symbols[i].offset;
	}
	/* Write info for VPU instance data parameter, if available in elf */
	for (i = 0U; i < elf->num_symbols; i++) {
		if (strcmp(elf->sym[i].symbol_name,
			   PVA_SYS_INSTANCE_DATA_V1_SYMBOL) == 0) {
			hw_task->param_list[task->num_symbols].addr =
			    elf->sym[i].addr;
			hw_task->param_list[task->num_symbols].size =
			    elf->sym[i].size;
			hw_task->param_list[task->num_symbols].param_base =
			    PVA_SYS_INSTANCE_DATA_V1_IOVA;
			++task->num_symbols;
		}
	}
	hw_task->task.parameter_base =
	    task->dma_addr + offsetof(struct pva_hw_task, param_list);
	hw_task->task.num_parameters = task->num_symbols;
	err = pva_task_acquire_ref_vpu_app(&task->client->elf_ctx,
					   task->exe_id);
	if (err) {
		task_err(task,
			 "unable to acquire ref count for app with id = %u",
			 task->exe_id);
	}
	task->pinned_app = true;
	return err;
}
static int set_flags(struct pva_submit_task *task, struct pva_hw_task *hw_task)
{
	int err = 0;
	uint32_t flags = task->flags;
	if (flags & NVPVA_PRE_BARRIER_TASK_TRUE)
		hw_task->task.flags |= PVA_TASK_FL_SYNC_TASKS;
	if (flags & NVPVA_AFFINITY_VPU0)
		hw_task->task.flags |= PVA_TASK_FL_VPU0;
	if (flags & NVPVA_AFFINITY_VPU1)
		hw_task->task.flags |= PVA_TASK_FL_VPU1;
	if ((flags & NVPVA_AFFINITY_VPU_ANY) == 0) {
		err = -EINVAL;
		task_err(task, "incorrect vpu affinity");
		goto out;
	}
	if (task->pva->vpu_debug_enabled)
		hw_task->task.flags |= PVA_TASK_FL_VPU_DEBUG;

	if (task->special_access)
		hw_task->task.flags |= PVA_TASK_FL_SPECIAL_ACCESS;
	if (flags & NVPVA_ERR_MASK_ILLEGAL_INSTR)
		hw_task->task.flags |= PVA_TASK_FL_ERR_MASK_ILLEGAL_INSTR;
	if (flags & NVPVA_ERR_MASK_DIVIDE_BY_0)
		hw_task->task.flags |= PVA_TASK_FL_ERR_MASK_DIVIDE_BY_0;
	if (flags & NVPVA_ERR_MASK_FP_NAN)
		hw_task->task.flags |= PVA_TASK_FL_ERR_MASK_FP_NAN;
out:
	return err;
}
static int pva_task_write(struct pva_submit_task *task)
{
	struct pva_hw_task *hw_task;
	u32 pre_ptr, post_ptr;
	int err = 0;
	if (!pva_vpu_elf_is_registered(&task->client->elf_ctx, task->exe_id) &&
	    (task->exe_id != NVPVA_NOOP_EXE_ID)) {
		task_err(task, "invalid exe id: %d", task->exe_id);
		return -EINVAL;
	}
	/* Task start from the memory base */
	hw_task = task->va;
	pre_ptr = 0;
	post_ptr = 0;
	/* process pre & post actions */
	err = pva_task_process_prefences(task, hw_task);
	if (err)
		goto out;
	err = pva_task_process_input_status(task, hw_task);
	if (err)
		goto out;
	err = pva_task_process_output_status(task, hw_task);
	if (err)
		goto out;
	err = pva_task_process_fence_actions(task, hw_task);
	if (err)
		goto out;

	err = pva_task_write_dma_info(task, hw_task);
	if (err)
		goto out;
	err = pva_task_write_dma_misr_info(task, hw_task);
	if (err)
		goto out;
	err = pva_task_write_vpu_parameter(task, hw_task);
	if (err)
		goto out;
	hw_task->task.next = 0U;
	hw_task->task.preactions = task->dma_addr + offsetof(struct pva_hw_task,
							 preactions);
	hw_task->task.postactions = task->dma_addr + offsetof(struct pva_hw_task,
							  postactions);
	hw_task->task.runlist_version = PVA_RUNLIST_VERSION_ID;
	hw_task->task.sid_index = 0;
	err = set_flags(task, hw_task);
	if (err)
		goto out;
	hw_task->task.bin_info =
	    phys_get_bin_info(&task->client->elf_ctx, task->exe_id);
out:
	return err;
}
void pva_task_free(struct kref *ref)
{
	struct pva_submit_task *task =
	    container_of(ref, struct pva_submit_task, ref);
	struct nvpva_queue *my_queue = task->queue;
	pva_task_unpin_mem(task);
	if (task->pinned_app)
		pva_task_release_ref_vpu_app(&task->client->elf_ctx,
						     task->exe_id);

	nvhost_module_idle(task->pva->pdev);
	nvpva_client_context_put(task->client);
	/* Release memory that was allocated for the task */
	nvpva_queue_free_task_memory(task->queue, task->pool_index);
	up(&my_queue->task_pool_sem);
}

static void update_one_task(struct pva *pva)
{
	struct platform_device *pdev = pva->pdev;
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvpva_queue *queue;
	struct pva_task_error_s task_info;
	struct pva_submit_task *task;
	struct pva_hw_task *hw_task;
	struct pva_task_statistics_s *stats;
	bool found;
	u64 vpu_time = 0u;
	u64 r5_overhead = 0u;
	const u32 tsc_ticks_to_us = 31;
	nvpva_fetch_task_status_info(pva, &task_info);
	WARN_ON(!task_info.valid);
	WARN_ON(task_info.queue >= MAX_PVA_QUEUE_COUNT);
	queue = &pva->pool->queues[task_info.queue];
	/* find the finished task; since two tasks can be scheduled at the same
 * time, the finished one is not necessarily the first one
 */
	found = false;
	mutex_lock(&queue->list_lock);
	/* since we are only taking one entry out, we don't need to use the safe
 * version
 */
	list_for_each_entry(task, &queue->tasklist, node) {
		if (task->dma_addr == task_info.addr) {
			list_del(&task->node);
				found = true;
				break;
		}
	}
	mutex_unlock(&queue->list_lock);
	if (!found) {
		pr_err("pva: unexpected task: queue:%u, valid:%u, error:%u, vpu:%u",
		       task_info.queue, task_info.valid, task_info.error,
		       task_info.vpu);
		return;
	}
	WARN_ON(task_info.error == PVA_ERR_BAD_TASK ||
		task_info.error == PVA_ERR_BAD_TASK_ACTION_LIST);
	hw_task = task->va;
	stats = &hw_task->statistics;
	vpu_time = (stats->vpu_complete_time - stats->vpu_start_time);
	r5_overhead = ((stats->complete_time - stats->queued_time) - vpu_time);
	r5_overhead = r5_overhead / tsc_ticks_to_us;
	trace_nvhost_task_timestamp(dev_name(&pdev->dev), pdata->class,
				    queue->syncpt_id, task->syncpt_thresh,
				    stats->vpu_assigned_time,
				    stats->complete_time);
	nvhost_eventlib_log_task(pdev, queue->syncpt_id, task->syncpt_thresh,
				 stats->vpu_assigned_time,
				 stats->complete_time);
	nvhost_dbg_info(
	    "Completed task %p (0x%llx), start_time=%llu, end_time=%llu",
	    task, (u64)task->dma_addr, stats->vpu_assigned_time,
	    stats->complete_time);
	trace_nvhost_pva_task_stats(
	    pdev->name, stats->queued_time, stats->head_time,
	    stats->input_actions_complete, stats->vpu_assigned_time,
	    stats->vpu_start_time, stats->vpu_complete_time,
	    stats->complete_time, stats->vpu_assigned, r5_overhead);
		/* Not linked anymore so drop the reference */
		kref_put(&task->ref, pva_task_free);
}

void pva_task_update(struct work_struct *work)
{
	struct pva *pva = container_of(work, struct pva, task_update_work);
	int n_tasks = atomic_read(&pva->n_pending_tasks);
	int i;
	atomic_sub(n_tasks, &pva->n_pending_tasks);
	for (i = 0; i < n_tasks; i++)
		update_one_task(pva);
}
static void
pva_queue_dump(struct nvpva_queue *queue, struct seq_file *s)
{
	struct pva_submit_task *task;
	int i = 0;
	seq_printf(s, "Queue %u, Tasks\n", queue->id);
	mutex_lock(&queue->list_lock);
	list_for_each_entry(task, &queue->tasklist, node) {
		seq_printf(s, "    #%u: exe_id = %u\n", i++, task->exe_id);
	}
	mutex_unlock(&queue->list_lock);
}
static int pva_task_submit_mmio_ccq(struct pva_submit_task *task, u8 batchsize)
{
	u32 flags = PVA_CMD_INT_ON_ERR;
	int err = 0;
	/* Construct submit command */
	err = task->pva->version_config->ccq_send_task(
	    task->pva, task->queue->id, task->dma_addr, batchsize, flags);
	return err;
}
static int pva_task_submit_mailbox(struct pva_submit_task *task, u8 batchsize)
{
	struct nvpva_queue *queue = task->queue;
	struct pva_cmd_status_regs status;
	struct pva_cmd_s cmd;
	u32 flags, nregs;
	int err = 0;
	/* Construct submit command */
	flags = PVA_CMD_INT_ON_ERR | PVA_CMD_INT_ON_COMPLETE;
	nregs = pva_cmd_submit_batch(&cmd, queue->id, task->dma_addr, batchsize,
				     flags);
	/* Submit request to PVA and wait for response */
	err = pva_mailbox_send_cmd_sync(task->pva, &cmd, nregs, &status);
	if (err < 0) {
		nvhost_warn(&task->pva->pdev->dev, "Failed to submit task: %d",
			    err);
		goto out;
	}
	if (status.error != PVA_ERR_NO_ERROR) {
		nvhost_warn(&task->pva->pdev->dev, "PVA task rejected: %u",
			    status.error);
		err = -EINVAL;
		goto out;
	}
out:
	return err;
}
u32 nvhost_syncpt_dec_max_ext(struct platform_device *dev, u32 id, u32 dec)
{
	struct nvhost_master *master = nvhost_get_host(dev);
	struct nvhost_syncpt *sp =
	    nvhost_get_syncpt_owner_struct(id, &master->syncpt);
	return (u32)atomic_sub_return(dec, &sp->max_val[id]);
}

static int pva_task_submit(const struct pva_submit_tasks *task_header)
{
	struct pva_submit_task *first_task = task_header->tasks[0];
	struct platform_device *host1x_pdev =
	    to_platform_device(first_task->pva->pdev->dev.parent);
	struct nvpva_queue *queue = first_task->queue;
	u64 timestamp;
	int err = 0;
	u32 i;
	u8 batchsize = task_header->num_tasks - 1U;
		nvhost_dbg_info("submitting %u tasks; batchsize: %u",
			task_header->num_tasks, batchsize);
	/*
	 * TSC timestamp is same as CNTVCT. Task statistics are being
	 * reported in TSC ticks.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	timestamp = arch_timer_read_counter();
#else
	timestamp = arch_counter_get_cntvct();
#endif

	for (i = 0; i < task_header->num_tasks; i++) {
		struct pva_submit_task *task = task_header->tasks[i];
		/* take the reference until task is finished */
		kref_get(&task->ref);

		(void)nvhost_syncpt_incr_max_ext(host1x_pdev, queue->syncpt_id,
						 task->fence_num);
		task->client->curr_sema_value += task->sem_num;

		mutex_lock(&queue->list_lock);
		list_add_tail(&task->node, &queue->tasklist);
		mutex_unlock(&queue->list_lock);
	}

	/* Choose the submit policy based on the mode */
	switch (first_task->pva->submit_task_mode) {
	case PVA_SUBMIT_MODE_MAILBOX:
		err = pva_task_submit_mailbox(first_task, batchsize);
		break;

	case PVA_SUBMIT_MODE_MMIO_CCQ:
		err = pva_task_submit_mmio_ccq(first_task, batchsize);
		break;
	}

	if (err) {
		/* assume no task has been submitted to firmware from now on */
		pr_err("pva: failed to submit %u tasks",
		       task_header->num_tasks);
		goto remove_tasks;
	}

	return 0;

remove_tasks:
	for (i = 0; i < task_header->num_tasks; i++) {
		struct pva_submit_task *task = task_header->tasks[i];

		mutex_lock(&queue->list_lock);
		list_del(&task->node);
		mutex_unlock(&queue->list_lock);

		(void)nvhost_syncpt_dec_max_ext(host1x_pdev, queue->syncpt_id,
						task->fence_num);
		task->client->curr_sema_value -= task->sem_num;

		kref_put(&task->ref, pva_task_free);
	}

	return err;
}

static int set_timer_flags(const struct pva_submit_tasks *task_header)
{
	struct pva_hw_task *hw_task = NULL;

	if (task_header->execution_timeout_us > 0U) {
		hw_task = task_header->tasks[0]->va;
		hw_task->task.flags |= PVA_TASK_FL_TIMER_START;
		hw_task->task.timeout = task_header->execution_timeout_us;
		if (!(hw_task->task.flags & PVA_TASK_FL_SYNC_TASKS))
			return -EINVAL;

		hw_task = task_header->tasks[task_header->num_tasks - 1]->va;
		hw_task->task.flags |= PVA_TASK_FL_TIMER_STOP;
		if (!(hw_task->task.flags & PVA_TASK_FL_SYNC_TASKS))
			return -EINVAL;
	}
	return 0;
}

static int
nvpva_task_config_l2sram_window(const struct pva_submit_tasks *task_header,
				u32 l2s_start_index, u32 l2s_end_index,
				u32 l2sram_max_size)
{
	struct pva_submit_task *task = NULL;
	struct pva_hw_task *hw_task = NULL;
	u32 task_num;

	for (task_num = l2s_start_index; task_num <= l2s_end_index;
	     task_num++) {
		task = task_header->tasks[task_num];
		hw_task = task->va;

		hw_task->task.l2sram_size = l2sram_max_size;
		if (task_num < l2s_end_index)
			hw_task->task.flags |= PVA_TASK_FL_KEEP_L2RAM;
	}
	hw_task = task_header->tasks[l2s_start_index]->va;
	if ((hw_task->task.flags & PVA_TASK_FL_SYNC_TASKS) == 0U)
		return -EINVAL;

	hw_task = task_header->tasks[l2s_end_index]->va;
	if ((hw_task->task.flags & PVA_TASK_FL_SYNC_TASKS) == 0U)
		return -EINVAL;

	return 0;
}

static int update_batch_tasks(const struct pva_submit_tasks *task_header)
{
	struct pva_submit_task *task = NULL;
	int err = 0;
	u32 task_num;
	u32 l2s_start_index, l2s_end_index;
	u32 l2sram_max_size = 0U;
	u32 invalid_index = task_header->num_tasks + 1U;

	l2s_start_index = invalid_index;
	l2s_end_index = invalid_index;

	for (task_num = 0; task_num < task_header->num_tasks; task_num++) {
		task = task_header->tasks[task_num];
		if (task->l2_alloc_size > 0) {
			if (l2s_start_index == invalid_index)
				l2s_start_index = task_num;

			l2s_end_index = task_num;
			if (l2sram_max_size < task->l2_alloc_size)
				l2sram_max_size = task->l2_alloc_size;

		} else if (l2s_end_index != invalid_index) {
			/* An L2SRAM window is found within the batch which
			 * needs to be sanitized
			 */
			err = nvpva_task_config_l2sram_window(task_header,
							      l2s_start_index,
							      l2s_end_index,
							      l2sram_max_size);
			if (err) {
				task_err(task, "bad L2SRAM window found");
				break;
			}

			l2s_start_index = invalid_index;
			l2s_end_index = invalid_index;
			l2sram_max_size = 0;
		}
	}

	/* Last L2SRAM window in batch may need to be sanitized */
	if ((err == 0) && (l2s_end_index != invalid_index)) {
		err = nvpva_task_config_l2sram_window(task_header,
						      l2s_start_index,
						      l2s_end_index,
						      l2sram_max_size);
		if (err)
			task_err(task, "bad L2SRAM window found");
	}
	return err;
}

static int pva_queue_submit(struct nvpva_queue *queue, void *args)
{
	const struct pva_submit_tasks *task_header = args;
	int err = 0;
	int i;
	uint32_t thresh, sem_thresh;
	struct pva_hw_task *prev_hw_task = NULL;
	struct platform_device *host1x_pdev =
		to_platform_device(queue->vm_pdev->dev.parent);
	struct nvpva_client_context *client = task_header->tasks[0]->client;

	mutex_lock(&client->sema_val_lock);
	thresh = nvhost_syncpt_read_maxval(host1x_pdev, queue->syncpt_id);
	sem_thresh = client->curr_sema_value;
	for (i = 0; i < task_header->num_tasks; i++) {
		struct pva_submit_task *task = task_header->tasks[i];
		task->fence_num = 0;
		task->syncpt_thresh = thresh;

		task->sem_num = 0;
		task->sem_thresh = sem_thresh;

		/* First, dump the task that we are submitting */
		pva_task_dump(task);

		/* Write the task data */
		err = pva_task_write(task);
		if (err)
			goto unlock;

		thresh = task->syncpt_thresh;
		sem_thresh = task->sem_thresh;

		if (prev_hw_task)
			prev_hw_task->task.next = task->dma_addr;

		prev_hw_task = task->va;
	}
	err = set_timer_flags(task_header);
	if (err)
		goto unlock;

	/* Update L2SRAM flags for T23x */
	if (task_header->tasks[0]->pva->version == PVA_HW_GEN2) {
		err = update_batch_tasks(task_header);
		if (err)
			goto unlock;
	}

	err = pva_task_submit(task_header);
	if (err) {
		dev_err(&queue->vm_pdev->dev, "failed to submit task");
		goto unlock;
	}
unlock:
	mutex_unlock(&client->sema_val_lock);
	return err;
}

static struct pva_pinned_memory *find_pinned_mem(struct pva_submit_task *task,
						 int fd)
{
	u32 i;

	for (i = 0; i < task->num_pinned; i++)
		if (task->pinned_memory[i].fd == fd)
			return &task->pinned_memory[i];
	return NULL;
}

static void pva_queue_cleanup_semaphore(struct pva_submit_task *task,
					struct nvpva_submit_fence *fence)
{
	u8 *dmabuf_cpuva;
	u32 *fence_cpuva;
	struct pva_pinned_memory *mem;

	if (fence->type != NVPVA_FENCE_OBJ_SEM)
		goto out;

	WARN_ON((fence->obj.sem.mem.offset % 4) != 0);

	mem = find_pinned_mem(task, fence->obj.sem.mem.pin_id);
	if (mem == NULL) {
		task_err(task, "can't find pinned semaphore for cleanup");
		goto out;
	}

	dmabuf_cpuva = dma_buf_vmap(mem->dmabuf);

	if (!dmabuf_cpuva)
		goto out;

	fence_cpuva = (void *)&dmabuf_cpuva[fence->obj.sem.mem.offset];
	*fence_cpuva = fence->obj.sem.value;

	dma_buf_vunmap(mem->dmabuf, dmabuf_cpuva);
out:
	return;
}

static void pva_queue_cleanup_status(struct pva_submit_task *task,
				     struct nvpva_mem *status_h)
{
	struct pva_pinned_memory *mem;
	u8 *dmabuf_cpuva;
	struct pva_gen_task_status_s *status_ptr;

	mem = find_pinned_mem(task, status_h->pin_id);
	if (mem == NULL) {
		task_err(task, "can't find pinned status for cleanup");
		goto out;
	}

	dmabuf_cpuva = dma_buf_vmap(mem->dmabuf);
	if (!dmabuf_cpuva)
		goto out;

	status_ptr = (void *)&dmabuf_cpuva[status_h->offset];
	status_ptr->status = PVA_ERR_BAD_TASK_STATE;
	status_ptr->info32 = PVA_ERR_VPU_BAD_STATE;

	dma_buf_vunmap(mem->dmabuf, dmabuf_cpuva);
out:
	return;
}

static void pva_queue_cleanup(struct nvpva_queue *queue,
			      struct pva_submit_task *task)
{
	struct platform_device *pdev = queue->pool->pdev;
	unsigned int i, fence_type;

	/* Write task status first */
	for (i = 0; i < task->num_output_task_status; i++)
		pva_queue_cleanup_status(task, &task->output_task_status[i]);

	/* Finish up non-syncpoint fences */
	for (fence_type = NVPVA_FENCE_SOT_R5;
	     fence_type < NVPVA_MAX_FENCE_TYPES; fence_type++) {
		for (i = 0; i < task->num_pva_fence_actions[fence_type]; i++)
			pva_queue_cleanup_semaphore(
				task,
				&task->pva_fence_actions[fence_type][i].fence);
	}

	/* Finish syncpoint increments to release waiters */
	for (i = 0; i < task->fence_num; i++)
		nvhost_syncpt_cpu_incr_ext(pdev, queue->syncpt_id);
}

static int pva_queue_abort(struct nvpva_queue *queue)
{
	struct pva_submit_task *task, *n;

	mutex_lock(&queue->list_lock);

	list_for_each_entry_safe(task, n, &queue->tasklist, node) {
		pva_queue_cleanup(queue, task);
		list_del(&task->node);
		kref_put(&task->ref, pva_task_free);
	}

	mutex_unlock(&queue->list_lock);

	return 0;
}

struct nvpva_queue_ops pva_queue_ops = {
	.abort = pva_queue_abort,
	.submit = pva_queue_submit,
	.get_task_size = pva_task_get_memsize,
	.dump = pva_queue_dump,
	.set_attribute = NULL,
};
