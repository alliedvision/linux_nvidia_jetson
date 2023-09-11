/*
 * Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/host1x.h>

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

#include <linux/seq_file.h>
#include <uapi/linux/nvpva_ioctl.h>
#include <trace/events/nvhost_pva.h>

#include "pva.h"
#include "nvpva_buffer.h"
#include "nvpva_queue.h"
#include "pva_mailbox.h"
#include "pva_queue.h"
#include "pva_regs.h"

#include "pva-vpu-perf.h"
#include "pva-interface.h"
#include "pva_vpu_exe.h"
#include "nvpva_client.h"
#include "nvpva_syncpt.h"

void *pva_dmabuf_vmap(struct dma_buf *dmabuf)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	struct iosys_map map = {0};
#else
	struct dma_buf_map map = {0};
#endif
	/* Linux v5.11 and later kernels */
	if (dma_buf_vmap(dmabuf, &map))
		return NULL;

	return map.vaddr;
#else
	/* Linux v5.10 and earlier kernels */
	return dma_buf_vmap(dmabuf);
#endif
}

void pva_dmabuf_vunmap(struct dma_buf *dmabuf, void *addr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(addr);
#else
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(addr);
#endif
	/* Linux v5.11 and later kernels */
	dma_buf_vunmap(dmabuf, &map);
#else
	/* Linux v5.10 and earlier kernels */
	dma_buf_vunmap(dmabuf, addr);
#endif
}

static void pva_task_dump(struct pva_submit_task *task)
{
	int i;

	nvpva_dbg_info(task->pva, "task=%p, exe_id=%u", task, task->exe_id);

	for (i = 0; i < task->num_input_task_status; i++)
		nvpva_dbg_info(task->pva, "input task status %d: pin_id=%u, offset=%u", i,
				task->input_task_status[i].pin_id,
				task->input_task_status[i].offset);

	for (i = 0; i < task->num_output_task_status; i++)
		nvpva_dbg_info(task->pva, "output task status %d: pin_id=%u, offset=%u",
				i, task->output_task_status[i].pin_id,
				task->output_task_status[i].offset);

	for (i = 0; i < task->num_user_fence_actions; i++)
		nvpva_dbg_info(task->pva, "fence action %d: type=%u", i,
				task->user_fence_actions[i].type);
}

static void pva_task_get_memsize(size_t *dma_size,
				 size_t *kmem_size,
				 size_t *aux_dma_size)
{
	/* Align task addr to 64bytes boundary for DMA use*/
	*dma_size = ALIGN(sizeof(struct pva_hw_task) + 64, 64);
	*kmem_size = sizeof(struct pva_submit_task);
	*aux_dma_size = NVPVA_TASK_MAX_PAYLOAD_SIZE;
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

		nvpva_buffer_submit_unpin_id(task->client->buffers,
					    &mem->id, 1);
	}

	task->num_pinned = 0;
}

struct pva_pinned_memory *pva_task_pin_mem(struct pva_submit_task *task,
					   u32 id)
{
	int err;
	struct pva_pinned_memory *mem;

	if (task->num_pinned >= ARRAY_SIZE(task->pinned_memory)) {
		task_err(task, "too many objects to pin");
		err = -ENOMEM;
		goto err_out;
	}

	if (id == 0) {
		task_err(task, "pin id  is 0");
		err = -EFAULT;
		goto err_out;
	}

	mem = &task->pinned_memory[task->num_pinned];
	mem->id = id;
	err = nvpva_buffer_submit_pin_id(task->client->buffers, &mem->id, 1,
					 &mem->dmabuf, &mem->dma_addr,
					 &mem->size, &mem->heap);
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
static int
pva_task_pin_fence(struct pva_submit_task *task,
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
		dma_addr_t syncpt_addr = nvpva_syncpt_address(
				task->queue->vm_pdev, fence->obj.syncpt.id,
				false);
		nvpva_dbg_info(task->pva,
			       "id = %d, syncpt addr = %llx",
			       fence->obj.syncpt.id,
			       syncpt_addr);

		if (syncpt_addr) {
			*addr = syncpt_addr;
		} else {
			task_err(task,
				"%s: can't get syncpoint address",
				__func__);
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

static int
get_fence_value(struct nvpva_submit_fence *fence, u32 *val)
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

static inline void
pva_task_write_status_action_op(struct pva_task_action_s *op,
				uint8_t action,
				uint64_t addr,
				uint16_t val)
{
	op->action = action;
	op->args.status.p = addr;
	op->args.status.status = val;
}

static inline void
pva_task_write_stats_action_op(struct pva_task_action_s *op,
			       uint8_t action,
			       uint64_t addr)
{
	op->action = action;
	op->args.statistics.p = addr;
}

static  int
pva_task_process_fence_actions(struct pva_submit_task *task,
			       struct pva_hw_task *hw_task)
{
	int err = 0;
	u32 i;
	u32 fence_type;
	u32 ts_flag = 0;
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
			ts_flag = PVA_TASK_FL_SOT_R_TS;
			break;
		case NVPVA_FENCE_SOT_VPU:
			fw_actions = &hw_task->preactions[0];
			action_code = TASK_ACT_PTR_WRITE_SOT_V;
			action_counter = &hw_task->task.num_preactions;
			ts_flag = PVA_TASK_FL_SOT_V_TS;
			break;
		case NVPVA_FENCE_EOT_R5:
			fw_actions = &hw_task->postactions[0];
			action_code = TASK_ACT_PTR_WRITE_EOT_R;
			action_counter = &hw_task->task.num_postactions;
			ts_flag = PVA_TASK_FL_EOT_R_TS;
			break;
		case NVPVA_FENCE_EOT_VPU:
			fw_actions = &hw_task->postactions[0];
			action_code = TASK_ACT_PTR_WRITE_EOT_V;
			action_counter = &hw_task->task.num_postactions;
			ts_flag = PVA_TASK_FL_EOT_V_TS;
			break;
		case NVPVA_FENCE_POST:
			fw_actions = &hw_task->postactions[0];
			action_code = TASK_ACT_PTR_WRITE_EOT;
			action_counter = &hw_task->task.num_postactions;
			ts_flag = 0;
			break;
		default:
			task_err(task, "unknown fence action type");
			err = -EINVAL;
			goto out;
		}

		for (i = 0; i < task->num_pva_fence_actions[fence_type]; i++) {
			struct nvpva_fence_action *fence_action =
			    &task->pva_fence_actions[fence_type][i];
			dma_addr_t fence_addr = 0;
			u32 fence_value;
			dma_addr_t timestamp_addr;
			switch (fence_action->fence.type) {
			case NVPVA_FENCE_OBJ_SYNCPT:
			{
				u32 id = task->queue->syncpt_id;
				fence_action->fence.obj.syncpt.id = id;
				fence_addr = nvpva_syncpt_address(
				    task->queue->vm_pdev, id, true);
				nvpva_dbg_info(task->pva,
					       "id = %d, fence_addr = %llx ",
					       task->queue->syncpt_id,
					       fence_addr);

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
				hw_task->task.flags |= ts_flag;
			} else {
				timestamp_addr = 0;
			}

			current_fw_actions = &fw_actions[*action_counter];
			pva_task_write_fence_action_op(current_fw_actions,
						       action_code, fence_addr,
						       fence_value,
						       timestamp_addr);
			*action_counter = *action_counter + 1;
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
		dma_addr_t fence_addr = 0;
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
	u8 i;
	int err = 0;
	struct pva_task_action_s *fw_preactions = NULL;

	for (i = 0; i < task->num_input_task_status; i++) {
		struct nvpva_mem *status;
		struct pva_pinned_memory *mem;
		dma_addr_t status_addr;

		status = &task->input_task_status[i];
		mem = pva_task_pin_mem(task, status->pin_id);
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
		dma_addr_t status_addr;
		struct nvpva_mem *status = &task->output_task_status[i];
		struct pva_pinned_memory *mem;

		mem = pva_task_pin_mem(task, status->pin_id);
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
	if ((task->pva->stats_enabled)
	  || (task->pva->profiling_level > 0)) {
		pva_task_write_stats_action_op(fw_postactions,
					       (uint8_t)TASK_ACT_PVA_STATISTICS,
					       stats_addr);
		hw_task->task.flags |= PVA_TASK_FL_STATS_ENABLE;
		++hw_task->task.num_postactions;
	}
out:
	return err;
}
static int
pva_task_write_vpu_parameter(struct pva_submit_task *task,
			     struct pva_hw_task *hw_task)
{
	int err = 0;
	struct pva_elf_image *elf = NULL;
	struct nvpva_pointer_symbol *sym_ptr = NULL;
	struct nvpva_pointer_symbol_ex *sym_ptr_ex = NULL;
	u32 symbolId = 0U;
	dma_addr_t symbol_payload = 0U;
	u32 size = 0U;
	u32 i;
	u32 index = 0;

	u32 head_index = 0U;
	u8 *headPtr = NULL;
	u32 head_size = 0U;
	u32 head_count = 0U;

	u32 tail_index = 0U;
	u8 *tailPtr = NULL;
	u32 tail_count = 0U;

	if ((task->exe_id == NVPVA_NOOP_EXE_ID) || (task->num_symbols == 0U))
		goto out;

	tail_index = ((u32)task->num_symbols - 1U);
	elf = get_elf_image(&task->client->elf_ctx, task->exe_id);
	if (task->num_symbols > elf->num_symbols) {
		task_err(task, "invalid number of symbols");
		err = -EINVAL;
		goto out;
	}

	if (task->symbol_payload_size == 0U) {
		task_err(task, "Empty Symbol payload");
		err = -EINVAL;
		goto out;
	}

	symbol_payload = task->aux_dma_addr;

	headPtr = (u8 *)(task->aux_va);
	tailPtr = (u8 *)(task->aux_va + task->symbol_payload_size);

	for (i = 0U; i < task->num_symbols; i++) {
		symbolId = task->symbols[i].symbol.id;
		size = elf->sym[symbolId].size;
		if (task->symbols[i].symbol.size != size) {
			task_err(task, "size does not match symbol:%s",
				 elf->sym[symbolId].symbol_name);
			err = -EINVAL;
			goto out;
		}

		if (task->symbols[i].config == NVPVA_SYMBOL_POINTER) {
			struct pva_pinned_memory *mem;

			memcpy(headPtr, (task->symbol_payload + task->symbols[i].offset),
				sizeof(struct nvpva_pointer_symbol));
			sym_ptr = (struct nvpva_pointer_symbol *)(headPtr);
			mem = pva_task_pin_mem(task, PVA_LOW32(sym_ptr->base));
			if (IS_ERR(mem)) {
				err = PTR_ERR(mem);
				task_err(task, "failed to pin symbol pointer");
				err = -EINVAL;
				goto out;
			}

			sym_ptr->base = mem->dma_addr;
			sym_ptr->size = mem->size;
			size = sizeof(struct nvpva_pointer_symbol);
		} else if (task->symbols[i].config == NVPVA_SYMBOL_POINTER_EX) {
			struct pva_pinned_memory *mem;

			memcpy(headPtr, (task->symbol_payload + task->symbols[i].offset),
				sizeof(struct nvpva_pointer_symbol_ex));
			sym_ptr_ex = (struct nvpva_pointer_symbol_ex *)(headPtr);
			mem = pva_task_pin_mem(task, PVA_LOW32(sym_ptr_ex->base));
			if (IS_ERR(mem)) {
				err = PTR_ERR(mem);
				task_err(task, "failed to pin symbol pointer");
				err = -EINVAL;
				goto out;
			}

			sym_ptr_ex->base = mem->dma_addr;
			sym_ptr_ex->size = mem->size;
			size = sizeof(struct nvpva_pointer_symbol_ex);
		} else if (size < PVA_DMA_VMEM_COPY_THRESHOLD) {
			(void)memcpy(headPtr,
				(task->symbol_payload + task->symbols[i].offset),
				size);
		} else if ((uintptr_t)(tailPtr) < ((uintptr_t)(headPtr) + size)) {
			task_err(task, "Symbol payload overflow");
			err = -EINVAL;
			goto out;
		} else {
			tailPtr = (tailPtr - size);
			(void)memcpy(tailPtr,
				(task->symbol_payload + task->symbols[i].offset),
				size);
			hw_task->param_list[tail_index].param_base =
						(pva_iova)(symbol_payload +
						((uintptr_t)(tailPtr) -
						(uintptr_t)(task->aux_va)));
			index = tail_index;
			tail_index--;
			tail_count++;
			hw_task->param_list[index].addr = elf->sym[symbolId].addr;
			hw_task->param_list[index].size = size;
			continue;
		}

		hw_task->param_list[head_index].param_base = (pva_iova)(symbol_payload +
						((uintptr_t)(headPtr) -
						 (uintptr_t)(task->aux_va)));
		index = head_index;
		if ((uintptr_t)(headPtr) > ((uintptr_t)(tailPtr) - size)) {
			task_err(task, "Symbol payload overflow");
			err = -EINVAL;
			goto out;
		} else {
			headPtr = (headPtr + size);
			head_index++;
			head_size += size;
			head_count++;
			hw_task->param_list[index].addr = elf->sym[symbolId].addr;
			hw_task->param_list[index].size = size;
		}
	}

	/* Write info for VPU instance data parameter, if available in elf */
	for (i = 0U; i < elf->num_symbols; i++) {
		if (elf->sym[i].is_sys) {
			hw_task->param_list[task->num_symbols].addr =
			    elf->sym[i].addr;
			hw_task->param_list[task->num_symbols].size =
			    elf->sym[i].size;
			hw_task->param_list[task->num_symbols].param_base =
			    PVA_SYS_INSTANCE_DATA_V1_IOVA;
			++task->num_symbols;
		}
	}

	hw_task->param_info.small_vpu_param_data_iova =
			(head_size != 0U) ? symbol_payload : 0UL;

	hw_task->param_info.small_vpu_parameter_data_size = head_size;

	hw_task->param_info.large_vpu_parameter_list_start_index = head_count;
	hw_task->param_info.vpu_instance_parameter_list_start_index =
			(head_count + tail_count);

	hw_task->param_info.parameter_data_iova = task->dma_addr
			+ offsetof(struct pva_hw_task, param_list);

	hw_task->task.num_parameters = task->num_symbols;

	hw_task->task.parameter_info_base = task->dma_addr
					    + offsetof(struct pva_hw_task, param_info);

	err = pva_task_acquire_ref_vpu_app(&task->client->elf_ctx,
					   task->exe_id);
	if (err) {
		task_err(task,
			 "unable to acquire ref count for app with id = %u",
			 task->exe_id);
	}

	task->pinned_app = true;
out:
	return err;
}

static int set_flags(struct pva_submit_task *task, struct pva_hw_task *hw_task)
{
	int err = 0;
	uint32_t flags = task->flags;

	if (flags & NVPVA_PRE_BARRIER_TASK_TRUE)
		hw_task->task.flags |= PVA_TASK_FL_SYNC_TASKS;
	if (flags & NVPVA_GR_CHECK_EXE_FLAG)
		hw_task->task.flags |= PVA_TASK_FL_GR_CHECK;
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
	hw_task->task.sid_index = task->client->sid_index;
	err = set_flags(task, hw_task);
	if (err)
		goto out;

	hw_task->task.bin_info =
	    phys_get_bin_info(&task->client->elf_ctx, task->exe_id);

	if (task->stdout) {
		hw_task->stdout_cb_info.buffer = task->stdout->buffer_addr;
		hw_task->stdout_cb_info.head = task->stdout->head_addr;
		hw_task->stdout_cb_info.tail = task->stdout->tail_addr;
		hw_task->stdout_cb_info.err = task->stdout->err_addr;
		hw_task->stdout_cb_info.buffer_size = task->stdout->size;
		hw_task->task.stdout_info =
			task->dma_addr +
			offsetof(struct pva_hw_task, stdout_cb_info);
	} else
		hw_task->task.stdout_info = 0;

out:

	return err;
}
#ifdef CONFIG_EVENTLIB

static void
pva_eventlib_fill_fence(struct nvdev_fence *dst_fence,
			struct nvpva_submit_fence *src_fence)
{
	static u32 obj_type[] = {NVDEV_FENCE_TYPE_SYNCPT,
				 NVDEV_FENCE_TYPE_SEMAPHORE,
				 NVDEV_FENCE_TYPE_SEMAPHORE_TS,
				 NVDEV_FENCE_TYPE_SYNC_FD};

	memset(dst_fence, 0, sizeof(struct nvdev_fence));
	dst_fence->type = obj_type[src_fence->type];
	switch (src_fence->type) {
	case NVPVA_FENCE_OBJ_SYNCPT:
		dst_fence->syncpoint_index = src_fence->obj.syncpt.id;
		dst_fence->syncpoint_value = src_fence->obj.syncpt.value;
		break;
	case NVPVA_FENCE_OBJ_SEM:
	case NVPVA_FENCE_OBJ_SEMAPHORE_TS:
		dst_fence->semaphore_handle = src_fence->obj.sem.mem.pin_id;
		dst_fence->semaphore_offset = src_fence->obj.sem.mem.offset;
		dst_fence->semaphore_value  = src_fence->obj.sem.value;
		break;
	case NVPVA_FENCE_OBJ_SYNC_FD:
		break;
	default:
		break;
	}
}

static void
pva_eventlib_record_r5_states(struct platform_device *pdev,
			      u32 syncpt_id,
			      u32 syncpt_thresh,
			      struct pva_task_statistics_s *stats,
			      struct pva_submit_task *task)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_pva_task_state state;
	struct nvdev_fence post_fence;
	struct nvpva_submit_fence *fence;
	u8 i;

	if ((task->pva->profiling_level == 0) || (!pdata->eventlib_id))
		return;

	/* Record task postfences */
	for (i = 0 ; i < task->num_pva_fence_actions[NVPVA_FENCE_POST]; i++) {
		fence = &(task->pva_fence_actions[NVPVA_FENCE_POST][i].fence);
		pva_eventlib_fill_fence(&post_fence, fence);
		nvhost_eventlib_log_fences(pdev,
					   syncpt_id,
					   syncpt_thresh,
					   &post_fence,
					   1,
					   NVDEV_FENCE_KIND_POST,
					   stats->complete_time);
	}

	state.class_id		= pdata->class;
	state.syncpt_id		= syncpt_id;
	state.syncpt_thresh	= syncpt_thresh;
	state.vpu_id		= stats->vpu_assigned;
	state.queue_id		= stats->queue_id;
	state.iova		= task->dma_addr;

	keventlib_write(pdata->eventlib_id,
			&state,
			sizeof(state),
			stats->vpu_assigned == 0 ? NVHOST_PVA_VPU0_BEGIN
						 : NVHOST_PVA_VPU1_BEGIN,
			stats->vpu_start_time);

	keventlib_write(pdata->eventlib_id,
			&state,
			sizeof(state),
			stats->vpu_assigned == 0 ? NVHOST_PVA_VPU0_END
						 : NVHOST_PVA_VPU1_END,
			stats->vpu_complete_time);
	keventlib_write(pdata->eventlib_id,
			&state,
			sizeof(state),
			NVHOST_PVA_PREPARE_END,
			stats->vpu_start_time);
	keventlib_write(pdata->eventlib_id,
			&state,
			sizeof(state),
			NVHOST_PVA_POST_BEGIN,
			stats->vpu_complete_time);

	if (task->pva->profiling_level >= 2) {
		keventlib_write(pdata->eventlib_id,
				&state,
				sizeof(state),
				NVHOST_PVA_QUEUE_BEGIN,
				stats->queued_time);

		keventlib_write(pdata->eventlib_id,
				&state,
				sizeof(state),
				NVHOST_PVA_QUEUE_END,
				stats->vpu_assigned_time);

		keventlib_write(pdata->eventlib_id,
				&state,
				sizeof(state),
				NVHOST_PVA_PREPARE_BEGIN,
				stats->vpu_assigned_time);

		keventlib_write(pdata->eventlib_id,
				&state,
				sizeof(state),
				NVHOST_PVA_POST_END,
				stats->complete_time);
	}
}
#else
static void
pva_eventlib_fill_fence(struct nvdev_fence *dst_fence,
			struct nvpva_submit_fence *src_fence)
{
}
static void
pva_eventlib_record_r5_states(struct platform_device *pdev,
			      u32 syncpt_id,
			      u32 syncpt_thresh,
			      struct pva_task_statistics_s *stats,
			      struct pva_submit_task *task)
{
}
#endif

void pva_task_free(struct kref *ref)
{
	struct pva_submit_task *task =
	    container_of(ref, struct pva_submit_task, ref);
	struct nvpva_queue *my_queue = task->queue;

	mutex_lock(&my_queue->tail_lock);
	if (my_queue->hw_task_tail == task->va)
		my_queue->hw_task_tail = NULL;

	if (my_queue->old_tail == task->va)
		my_queue->old_tail = NULL;

	mutex_unlock(&my_queue->tail_lock);

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
	u32 vpu_assigned = 0;

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
		if (task->pool_index == task_info.task_id) {
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
	hw_task = (struct pva_hw_task *)task->va;
	stats = &hw_task->statistics;
	if (!task->pva->stats_enabled)
		goto prof;

	vpu_assigned = (stats->vpu_assigned & 0x1);
	vpu_time = (stats->vpu_complete_time - stats->vpu_start_time);
	r5_overhead = ((stats->complete_time - stats->queued_time) - vpu_time);
	r5_overhead = r5_overhead / tsc_ticks_to_us;

	trace_nvhost_pva_task_timestamp(dev_name(&pdev->dev),
				    pdata->class,
				    queue->syncpt_id,
				    task->local_sync_counter,
				    stats->vpu_assigned_time,
				    stats->complete_time);
	nvpva_dbg_info(pva, "Completed task %p (0x%llx), "
			"start_time=%llu, "
			"end_time=%llu",
			task,
			(u64)task->dma_addr,
			stats->vpu_assigned_time,
			stats->complete_time);
	trace_nvhost_pva_task_stats(pdev->name,
				    stats->queued_time,
				    stats->head_time,
				    stats->input_actions_complete,
				    stats->vpu_assigned_time,
				    stats->vpu_start_time,
				    stats->vpu_complete_time,
				    stats->complete_time,
				    stats->vpu_assigned,
				    r5_overhead);
prof:
	if (task->pva->profiling_level == 0)
		goto out;

	nvhost_eventlib_log_task(pdev,
				 queue->syncpt_id,
				 task->local_sync_counter,
				 stats->vpu_assigned_time,
				 stats->complete_time);
	pva_eventlib_record_r5_states(pdev,
				      queue->syncpt_id,
				      task->local_sync_counter,
				      stats,
				      task);
out:
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
		nvpva_warn(&task->pva->pdev->dev, "Failed to submit task: %d",
			    err);
		goto out;
	}

	if (status.error != PVA_ERR_NO_ERROR) {
		nvpva_warn(&task->pva->pdev->dev, "PVA task rejected: %u",
			    status.error);
		err = -EINVAL;
		goto out;
	}

out:

	return err;
}

static void nvpva_syncpt_dec_max(struct nvpva_queue *queue, u32 val)
{
	atomic_sub(val, &queue->syncpt_maxval);
}

static void nvpva_syncpt_incr_max(struct nvpva_queue *queue, u32 val)
{
	atomic_add(val, &queue->syncpt_maxval);
}

static u32 nvpva_syncpt_read_max(struct nvpva_queue *queue)
{
	return (u32)atomic_read(&queue->syncpt_maxval);
}

static int pva_task_submit(const struct pva_submit_tasks *task_header)
{
	struct pva_submit_task *first_task = task_header->tasks[0];
	struct nvpva_queue *queue = first_task->queue;
	u64 timestamp;
	int err = 0;
	u32 i;
	u8 batchsize = task_header->num_tasks - 1U;
	nvpva_dbg_info(first_task->pva, "submitting %u tasks; batchsize: %u",
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
		struct pva_hw_task *hw_task = task->va;

		/* take the reference until task is finished */
		kref_get(&task->ref);

		nvpva_syncpt_incr_max(queue, task->fence_num);
		task->client->curr_sema_value += task->sem_num;

		mutex_lock(&queue->list_lock);
		list_add_tail(&task->node, &queue->tasklist);
		mutex_unlock(&queue->list_lock);

		hw_task->task.queued_time = timestamp;
	}

	/*
	 * TSC timestamp is same as CNTVCT. Task statistics are being
	 * reported in TSC ticks.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	timestamp = arch_timer_read_counter();
#else
	timestamp = arch_counter_get_cntvct();
#endif

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

	if (first_task->pva->profiling_level == 0)
		goto out;

	for (i = 0; i < task_header->num_tasks; i++) {
		u32 j;
		struct nvdev_fence pre_fence;
		struct pva_submit_task *task = task_header->tasks[i];

		for (j = 0; j < task->num_prefences; j++) {
			pva_eventlib_fill_fence(&pre_fence,
						&task->prefences[j]);
			nvhost_eventlib_log_fences(task->pva->pdev,
						   queue->syncpt_id,
						   task->local_sync_counter,
						   &pre_fence,
						   1,
						   NVDEV_FENCE_KIND_PRE,
						   timestamp);
		}

		nvhost_eventlib_log_submit(task->pva->pdev,
					   queue->syncpt_id,
					   task->local_sync_counter,
					   timestamp);
	}
out:
	return 0;

remove_tasks:
	for (i = 0; i < task_header->num_tasks; i++) {
		struct pva_submit_task *task = task_header->tasks[i];

		mutex_lock(&queue->list_lock);
		list_del(&task->node);
		mutex_unlock(&queue->list_lock);

		nvpva_syncpt_dec_max(queue, task->fence_num);
		task->client->curr_sema_value -= task->sem_num;

		kref_put(&task->ref, pva_task_free);
	}

	return err;
}

static void
set_task_parameters(const struct pva_submit_tasks *task_header)
{
	struct pva_submit_task *task = task_header->tasks[0];
	struct pva_hw_task *hw_task = task->va;
	struct nvpva_queue *queue = task->queue;

	u8 status_interface = 0U;
	u32 flag = 0;
	u64 batch_id;
	u16 idx;

	/* Storing to local variable to update in task
	 * Increment the batch ID and let it overflow
	 * after it reached U8_MAX
	 */
	batch_id = (queue->batch_id++);

	if (task_header->execution_timeout_us > 0U) {
		hw_task = task_header->tasks[0]->va;
		hw_task->task.timer_ref_cnt = task_header->num_tasks;
		hw_task->task.timeout = task_header->execution_timeout_us;
		flag = PVA_TASK_FL_DEC_TIMER;
	}

	/* In T19x, there is only 1 CCQ, so the response should come there
	 * irrespective of the queue ID. In T23x, there are 8 CCQ FIFO's
	 * thus the response should come in the correct CCQ
	 */
	if ((task->pva->submit_task_mode == PVA_SUBMIT_MODE_MMIO_CCQ)
	   && (task_header->tasks[0]->pva->version == PVA_HW_GEN2))
		status_interface = (task->queue->id + 1U);

	for (idx = 0U; idx < task_header->num_tasks; idx++) {
		task = task_header->tasks[idx];
		hw_task = task->va;
		WARN_ON(task->pool_index > 0xFF);
		hw_task->task.task_id = task->pool_index;
		hw_task->task.status_interface = status_interface;
		hw_task->task.batch_id = batch_id;

		hw_task->task.flags |= flag;
	}

}

static void
nvpva_task_config_l2sram_window(const struct pva_submit_tasks *task_header,
				u32 start_index, u32 end_index,
				u32 size)
{
	struct pva_hw_task *hw_task = NULL;
	u32    task_num;

	hw_task = task_header->tasks[start_index]->va;
	hw_task->task.l2sram_ref_cnt = (end_index - start_index) + 1U;
	for (task_num = start_index; task_num <= end_index; task_num++) {
		hw_task = task_header->tasks[task_num]->va;
		hw_task->task.l2sram_size = size;
		hw_task->task.flags |= PVA_TASK_FL_DEC_L2SRAM;
	}
}

static void
update_batch_tasks(const struct pva_submit_tasks *task_header)
{
	struct pva_submit_task *task = NULL;
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
			nvpva_task_config_l2sram_window(task_header,
							l2s_start_index,
							l2s_end_index,
							l2sram_max_size);
			l2s_start_index = invalid_index;
			l2s_end_index = invalid_index;
			l2sram_max_size = 0;
		}
	}

	/* Last L2SRAM window in batch may need to be sanitized */
	if (l2s_end_index != invalid_index) {
		nvpva_task_config_l2sram_window(task_header,
						l2s_start_index,
						l2s_end_index,
						l2sram_max_size);
	}
}

static int pva_queue_submit(struct nvpva_queue *queue, void *args)
{
	const struct pva_submit_tasks *task_header = args;
	int err = 0;
	int i;
	uint32_t thresh, sem_thresh;
	struct pva_hw_task *prev_hw_task = NULL;
	struct nvpva_client_context *client = task_header->tasks[0]->client;

	mutex_lock(&client->sema_val_lock);
	thresh = nvpva_syncpt_read_max(queue);
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
		queue->local_sync_counter += (1 +  task->fence_num);
		task->local_sync_counter = queue->local_sync_counter;
		if (prev_hw_task)
			prev_hw_task->task.next = task->dma_addr;

		prev_hw_task = task->va;
	}

	set_task_parameters(task_header);

	/* Update L2SRAM flags for T23x */
	if (task_header->tasks[0]->pva->version == PVA_HW_GEN2)
		update_batch_tasks(task_header);

	mutex_lock(&queue->tail_lock);

	/* Once batch is ready, link it to the FW queue*/
	if (queue->hw_task_tail)
		queue->hw_task_tail->task.next = task_header->tasks[0]->dma_addr;

	/* Hold a reference to old tail in case submission fails*/
	queue->old_tail = queue->hw_task_tail;

	queue->hw_task_tail = prev_hw_task;
	mutex_unlock(&queue->tail_lock);

	err = pva_task_submit(task_header);
	if (err) {
		dev_err(&queue->vm_pdev->dev, "failed to submit task");
		mutex_lock(&queue->tail_lock);
		queue->hw_task_tail = queue->old_tail;
		mutex_unlock(&queue->tail_lock);
	}
unlock:
	mutex_unlock(&client->sema_val_lock);
	return err;
}

static struct pva_pinned_memory *find_pinned_mem(struct pva_submit_task *task,
						 int id)
{
	u32 i;

	for (i = 0; i < task->num_pinned; i++)
		if (task->pinned_memory[i].id == id)
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

	dmabuf_cpuva = pva_dmabuf_vmap(mem->dmabuf);

	if (!dmabuf_cpuva)
		goto out;

	fence_cpuva = (void *)&dmabuf_cpuva[fence->obj.sem.mem.offset];
	*fence_cpuva = fence->obj.sem.value;

	pva_dmabuf_vunmap(mem->dmabuf, dmabuf_cpuva);
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

	dmabuf_cpuva = pva_dmabuf_vmap(mem->dmabuf);
	if (!dmabuf_cpuva)
		goto out;

	status_ptr = (void *)&dmabuf_cpuva[status_h->offset];
	status_ptr->status = PVA_ERR_BAD_TASK_STATE;
	status_ptr->info32 = PVA_ERR_VPU_BAD_STATE;

	pva_dmabuf_vunmap(mem->dmabuf, dmabuf_cpuva);
out:
	return;
}

static void pva_queue_cleanup(struct nvpva_queue *queue,
			      struct pva_submit_task *task)
{
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

	/* Finish syncpoint increments to release waiters */
	nvhost_syncpt_set_min_update(queue->vm_pdev, queue->syncpt_id,
				     atomic_read(&queue->syncpt_maxval));
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
