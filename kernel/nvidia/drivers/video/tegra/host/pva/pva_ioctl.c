/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/nvhost.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/circ_buf.h>
#include <linux/nospec.h>
#include <asm/ioctls.h>
#include <asm/barrier.h>
#include <linux/kref.h>
#include <uapi/linux/nvdev_fence.h>
#include <uapi/linux/nvpva_ioctl.h>
#include <linux/firmware.h>

#include "pva.h"
#include "pva_queue.h"
#include "nvpva_buffer.h"
#include "pva_vpu_exe.h"
#include "pva_vpu_app_auth.h"
#include "pva_system_allow_list.h"
#include "nvpva_client.h"
/**
 * @brief pva_private - Per-fd specific data
 *
 * pdev		Pointer the pva device
 * queue	Pointer the struct nvpva_queue
 * buffer	Pointer to the struct nvpva_buffer
 */
struct pva_private {
	struct pva *pva;
	struct nvpva_queue *queue;
	struct pva_cb *vpu_print_buffer;
	struct nvpva_client_context *client;
};

static int copy_part_from_user(void *kbuffer, size_t kbuffer_size,
			       struct nvpva_ioctl_part part)
{
	int err = 0;
	int copy_ret;

	if (part.size == 0)
		goto out;

	if (kbuffer_size < part.size) {
		pr_err("pva: failed to copy from user due to size too large: %llu > %lu",
		       part.size, kbuffer_size);
		err = -EINVAL;
		goto out;
	}
	copy_ret =
		copy_from_user(kbuffer, (void __user *)part.addr, part.size);
	if (copy_ret) {
		err = -EFAULT;
		goto out;
	}
out:
	return err;
}

static struct pva_cb *pva_alloc_cb(struct device *dev, uint32_t size)
{
	int err;
	struct pva_cb *cb;

	if ((size == 0) || (((size - 1) & size) != 0)) {
		dev_err(dev, "invalid circular buffer size: %u; it must be 2^N.", size);
		err = -EINVAL;
		goto out;
	}

	cb = kzalloc(sizeof(*cb), GFP_KERNEL);
	if (IS_ERR_OR_NULL(cb)) {
		err = PTR_ERR(cb);
		goto out;
	}

	cb->size = size;
	cb->buffer_va =
		dma_alloc_coherent(dev, cb->size, &cb->buffer_addr, GFP_KERNEL);

	if (IS_ERR_OR_NULL(cb->buffer_va)) {
		err = PTR_ERR(cb->buffer_va);
		goto free_mem;
	}

	cb->head_va = dma_alloc_coherent(dev, sizeof(uint32_t), &cb->head_addr,
					 GFP_KERNEL);
	if (IS_ERR_OR_NULL(cb->head_va)) {
		err = PTR_ERR(cb->head_va);
		goto free_buffer;
	}

	cb->tail_va = dma_alloc_coherent(dev, sizeof(uint32_t), &cb->tail_addr,
					 GFP_KERNEL);
	if (IS_ERR_OR_NULL(cb->tail_va)) {
		err = PTR_ERR(cb->tail_va);
		goto free_head;
	}

	cb->err_va = dma_alloc_coherent(dev, sizeof(uint32_t), &cb->err_addr,
					 GFP_KERNEL);
	if (IS_ERR_OR_NULL(cb->err_va)) {
		err = PTR_ERR(cb->err_va);
		goto free_tail;
	}

	*cb->head_va = 0;
	cb->tail = 0;
	*cb->tail_va = cb->tail;
	*cb->err_va = 0;
	return cb;

free_tail:
	dma_free_coherent(dev, sizeof(uint32_t), cb->tail_va, cb->tail_addr);
free_head:
	dma_free_coherent(dev, sizeof(uint32_t), cb->head_va, cb->head_addr);
free_buffer:
	dma_free_coherent(dev, cb->size, cb->buffer_va, cb->buffer_addr);
free_mem:
	kfree(cb);
out:
	return ERR_PTR(err);
}

static void pva_free_cb(struct device *dev, struct pva_cb *cb)
{
	dma_free_coherent(dev, sizeof(uint32_t), cb->tail_va, cb->tail_addr);
	dma_free_coherent(dev, sizeof(uint32_t), cb->head_va, cb->head_addr);
	dma_free_coherent(dev, sizeof(uint32_t), cb->err_va, cb->err_addr);
	dma_free_coherent(dev, cb->size, cb->buffer_va, cb->buffer_addr);
	kfree(cb);
}


/**
 * @brief	Copy a single task from userspace to kernel space
 *
 * This function copies fields from ioctl_task and performs a deep copy
 * of the task to kernel memory. At the same time, input values shall
 * be validated. This allows using all the fields without manually performing
 * copies of the structure and performing checks later.
 *
 * @param ioctl_task	Pointer to a userspace task that is copied
 *				to kernel memory
 * @param task		Pointer to a task that should be created
 * @return		0 on Success or negative error code
 *
 */
static int pva_copy_task(struct nvpva_ioctl_task *ioctl_task,
			 struct pva_submit_task *task)
{
	int err = 0;
	u32 i;
	struct pva_elf_image *image = NULL;

	nvpva_dbg_fn(task->pva, "");
	/*
	 * These fields are clear-text in the task descriptor. Just
	 * copy them.
	 */
	task->exe_id = ioctl_task->exe_id;
	task->l2_alloc_size = ioctl_task->l2_alloc_size;
	task->symbol_payload_size = ioctl_task->symbol_payload.size;
	task->flags = ioctl_task->flags;
	if (task->exe_id < NVPVA_NOOP_EXE_ID)
		image = get_elf_image(&task->client->elf_ctx, task->exe_id);

	task->is_system_app = (image != NULL) && image->is_system_app;

#define IOCTL_ARRAY_SIZE(field_name)                                           \
	(ioctl_task->field_name.size / sizeof(task->field_name[0]))

	task->num_prefences = IOCTL_ARRAY_SIZE(prefences);
	task->num_user_fence_actions = IOCTL_ARRAY_SIZE(user_fence_actions);
	task->num_input_task_status = IOCTL_ARRAY_SIZE(input_task_status);
	task->num_output_task_status = IOCTL_ARRAY_SIZE(output_task_status);
	task->num_dma_descriptors = IOCTL_ARRAY_SIZE(dma_descriptors);
	task->num_dma_channels = IOCTL_ARRAY_SIZE(dma_channels);
	task->num_symbols = IOCTL_ARRAY_SIZE(symbols);

#undef IOCTL_ARRAY_SIZE

	err = copy_part_from_user(&task->prefences, sizeof(task->prefences),
				  ioctl_task->prefences);
	if (err)
		goto out;

	err = copy_part_from_user(&task->user_fence_actions,
				  sizeof(task->user_fence_actions),
				  ioctl_task->user_fence_actions);
	if (err)
		goto out;

	err = copy_part_from_user(&task->input_task_status,
				  sizeof(task->input_task_status),
				  ioctl_task->input_task_status);
	if (err)
		goto out;

	err = copy_part_from_user(&task->output_task_status,
				  sizeof(task->output_task_status),
				  ioctl_task->output_task_status);
	if (err)
		goto out;

	err = copy_part_from_user(&task->dma_descriptors,
				  sizeof(task->dma_descriptors),
				  ioctl_task->dma_descriptors);
	if (err)
		goto out;

	err = copy_part_from_user(&task->dma_channels,
				  sizeof(task->dma_channels),
				  ioctl_task->dma_channels);
	if (err)
		goto out;

	if (task->is_system_app)
		err = copy_part_from_user(&task->dma_misr_config,
					  sizeof(task->dma_misr_config),
					  ioctl_task->dma_misr_config);
	else
		task->dma_misr_config.enable = 0;

	if (err)
		goto out;

	err = copy_part_from_user(&task->hwseq_config,
				  sizeof(task->hwseq_config),
				  ioctl_task->hwseq_config);
	if (err)
		goto out;

	err = copy_part_from_user(&task->symbols, sizeof(task->symbols),
				  ioctl_task->symbols);
	if (err)
		goto out;

	err = copy_part_from_user(&task->symbol_payload,
				  sizeof(task->symbol_payload),
				  ioctl_task->symbol_payload);
	if (err)
		goto out;

	/* Parse each postfence provided by user in 1D array and store into
	 * internal 2D array representation wrt type of fence and number of
	 * fences of each type for further processing
	 */
	for (i = 0; i < task->num_user_fence_actions; i++) {
		struct nvpva_fence_action *fence = &task->user_fence_actions[i];
		enum nvpva_fence_action_type fence_type = fence->type;
		u8 num_fence;

		if ((fence_type == 0U) ||
		    (fence_type >= NVPVA_MAX_FENCE_TYPES)) {
			task_err(task, "invalid fence type at index: %u", i);
			err = -EINVAL;
			goto out;
		}

		/* Ensure that the number of postfences for each type are within
		 * limit
		 */
		num_fence = task->num_pva_fence_actions[fence_type];
		if (num_fence >= NVPVA_TASK_MAX_FENCEACTIONS) {
			task_err(task, "too many fences for type: %u",
				 fence_type);
			err = -EINVAL;
			goto out;
		}

		task->pva_fence_actions[fence_type][num_fence] = *fence;
		task->num_pva_fence_actions[fence_type] += 1;
	}

	/* Check for valid HWSeq trigger mode */
	if ((task->hwseq_config.hwseqTrigMode != NVPVA_HWSEQTM_VPUTRIG) &&
	    (task->hwseq_config.hwseqTrigMode != NVPVA_HWSEQTM_DMATRIG)) {
		task_err(task, "invalid hwseq trigger mode: %d",
			 task->hwseq_config.hwseqTrigMode);
		err = -EINVAL;
		goto out;
	}

#undef COPY_FIELD

out:
	return err;
}

/**
 * @brief	Submit a task to PVA
 *
 * This function takes the given list of tasks, converts
 * them into kernel internal representation and submits
 * them to the task queue. On success, it populates
 * the post-fence structures in userspace and returns 0.
 *
 * @param priv	PVA Private data
 * @param arg	ioctl data
 * @return	0 on Success or negative error code
 *
 */
static int pva_submit(struct pva_private *priv, void *arg)
{
	struct nvpva_ioctl_submit_in_arg *ioctl_tasks_header =
		(struct nvpva_ioctl_submit_in_arg *)arg;
	struct nvpva_ioctl_task *ioctl_tasks = NULL;
	struct pva_submit_tasks *tasks_header;
	int err = 0;
	unsigned long rest;
	int i, j;
	uint32_t num_tasks;

	num_tasks = ioctl_tasks_header->tasks.size / sizeof(*ioctl_tasks);
	/* Sanity checks for the task heaader */
	if (num_tasks > NVPVA_SUBMIT_MAX_TASKS) {
		err = -EINVAL;
		dev_err(&priv->pva->pdev->dev,
			"exceeds maximum number of tasks: %u > %u", num_tasks,
			NVPVA_SUBMIT_MAX_TASKS);
		goto out;
	}

	num_tasks = array_index_nospec(num_tasks, NVPVA_SUBMIT_MAX_TASKS + 1);
	if (ioctl_tasks_header->version > 0) {
		err = -ENOSYS;
		goto out;
	}


	/* Allocate memory for the UMD representation of the tasks */
	ioctl_tasks = kzalloc(ioctl_tasks_header->tasks.size, GFP_KERNEL);
	if (ioctl_tasks == NULL) {
		pr_err("pva: submit: allocation for tasks failed");
		err = -ENOMEM;
		goto out;
	}

	tasks_header = kzalloc(sizeof(struct pva_submit_tasks), GFP_KERNEL);
	if (tasks_header == NULL) {
		pr_err("pva: submit: allocation for tasks_header failed");
		kfree(ioctl_tasks);
		err = -ENOMEM;
		goto out;
	}

	/* Copy the tasks from userspace */
	rest = copy_from_user(ioctl_tasks,
			      (void __user *)ioctl_tasks_header->tasks.addr,
			      ioctl_tasks_header->tasks.size);

	if (rest > 0) {
		err = -EFAULT;
		pr_err("pva: failed to copy tasks");
		goto free_ioctl_tasks;
	}

	tasks_header->num_tasks = 0;

	/* Go through the tasks and make a KMD representation of them */
	for (i = 0; i < num_tasks; i++) {
		struct pva_submit_task *task;
		struct nvpva_queue_task_mem_info task_mem_info;
		long timeout_jiffies = usecs_to_jiffies(
			ioctl_tasks_header->submission_timeout_us);

		/* Allocate memory for the task and dma */
		err = down_timeout(&priv->queue->task_pool_sem,
				   timeout_jiffies);
		if (err) {
			pr_err("pva: timeout when allocating task buffer");
			/* UMD expects this error code */
			err = -EAGAIN;
			goto free_tasks;
		}
		err = nvpva_queue_alloc_task_memory(priv->queue,
						     &task_mem_info);
		task = task_mem_info.kmem_addr;

		WARN_ON((err < 0) || !task);

		/* initialize memory to 0 */
		(void)memset(task_mem_info.kmem_addr, 0,
			     priv->queue->task_kmem_size);
		(void)memset(task_mem_info.va, 0, priv->queue->task_dma_size);

		/* Obtain an initial reference */
		kref_init(&task->ref);
		INIT_LIST_HEAD(&task->node);

		tasks_header->tasks[i] = task;
		tasks_header->num_tasks += 1;

		task->dma_addr = task_mem_info.dma_addr;
		task->aux_dma_addr = task_mem_info.aux_dma_addr;
		task->va = task_mem_info.va;
		task->aux_va = task_mem_info.aux_va;
		task->pool_index = task_mem_info.pool_index;

		task->pva = priv->pva;
		task->queue = priv->queue;
		task->client = priv->client;

		/* setup ownership */
		err = nvhost_module_busy(task->pva->pdev);
		if (err)
			goto free_tasks;

		nvpva_client_context_get(task->client);

		err = pva_copy_task(ioctl_tasks + i, task);
		if (err)
			goto free_tasks;

		if (priv->pva->vpu_printf_enabled)
			task->stdout = priv->vpu_print_buffer;
	}

	/* Populate header structure */
	tasks_header->execution_timeout_us =
		ioctl_tasks_header->execution_timeout_us;

	/* TODO: submission timeout */
	/* ..and submit them */
	err = nvpva_queue_submit(priv->queue, tasks_header);

	if (err < 0)
		goto free_tasks;

	/* Copy fences back to userspace */
	for (i = 0; i < tasks_header->num_tasks; i++) {
		struct pva_submit_task *task = tasks_header->tasks[i];
		u32 n_copied[NVPVA_MAX_FENCE_TYPES] = {};
		struct nvpva_fence_action __user *action_fences =
			(struct nvpva_fence_action __user *)ioctl_tasks[i]
				.user_fence_actions.addr;

		/* Copy return postfences in the same order as that provided in
		 * input
		 */
		for (j = 0; j < task->num_user_fence_actions; j++) {
			struct nvpva_fence_action *fence =
				&task->user_fence_actions[j];
			enum nvpva_fence_action_type fence_type = fence->type;

			*fence = task->pva_fence_actions[fence_type]
							[n_copied[fence_type]];
			n_copied[fence_type] += 1;
		}

		rest = copy_to_user(action_fences, task->user_fence_actions,
				    ioctl_tasks[i].user_fence_actions.size);

		if (rest) {
			nvpva_warn(&priv->pva->pdev->dev,
				    "Failed to copy pva fences to userspace");
			err = -EFAULT;
			goto free_tasks;
		}
	}

free_tasks:

	for (i = 0; i < tasks_header->num_tasks; i++) {
		struct pva_submit_task *task = tasks_header->tasks[i];
		/* Drop the reference */
		kref_put(&task->ref, pva_task_free);
	}

free_ioctl_tasks:

	kfree(ioctl_tasks);
	kfree(tasks_header);

out:
	return err;
}

static int pva_pin(struct pva_private *priv, void *arg)
{
	int err = 0;
	struct dma_buf *dmabuf[1];
	struct nvpva_pin_in_arg *in_arg = (struct nvpva_pin_in_arg *)arg;
	struct nvpva_pin_out_arg *out_arg = (struct nvpva_pin_out_arg *)arg;

	dmabuf[0] = dma_buf_get(in_arg->pin.handle);
	if (IS_ERR_OR_NULL(dmabuf[0])) {
		dev_err(&priv->pva->pdev->dev, "invalid handle to pin: %u",
			in_arg->pin.handle);
		err = -EFAULT;
		goto out;
	}

	err = nvpva_buffer_pin(priv->client->buffers,
			       &dmabuf[0],
			       &in_arg->pin.offset,
			       &in_arg->pin.size,
			       in_arg->pin.segment,
			       1,
			       &out_arg->pin_id,
			       &out_arg->error_code);
	dma_buf_put(dmabuf[0]);
out:
	return err;
}

static int pva_unpin(struct pva_private *priv, void *arg)
{
	int err = 0;
	struct nvpva_unpin_in_arg *in_arg = (struct nvpva_unpin_in_arg *)arg;

	nvpva_buffer_unpin_id(priv->client->buffers, &in_arg->pin_id, 1);

	return err;
}

static int
pva_authenticate_vpu_app(struct pva *pva,
			 struct pva_vpu_auth_s *auth,
			 uint8_t *data,
			 u32 size,
			 bool is_sys)
{
	int err = 0;

	if (!auth->pva_auth_enable)
		goto out;

	mutex_lock(&auth->allow_list_lock);
	if (!auth->pva_auth_allow_list_parsed) {
		if (is_sys)
			err = pva_auth_allow_list_parse_buf(pva->pdev,
				auth, pva_auth_allow_list_sys,
				pva_auth_allow_list_sys_len);
		else
			err = pva_auth_allow_list_parse(pva->pdev, auth);

		if (err) {
			nvpva_warn(&pva->pdev->dev,
					"allow list parse failed");
			mutex_unlock(&auth->allow_list_lock);
			goto out;
		}
	}

	mutex_unlock(&auth->allow_list_lock);
	err = pva_vpu_check_sha256_key(pva,
				       auth->vpu_hash_keys,
				       data,
				       size);
	if (err != 0)
		nvpva_dbg_fn(pva, "app authentication failed");
out:
	return err;
}

static int pva_register_vpu_exec(struct pva_private *priv, void *arg)
{
	struct	nvpva_vpu_exe_register_in_arg *reg_in =
			(struct nvpva_vpu_exe_register_in_arg *)arg;
	struct	nvpva_vpu_exe_register_out_arg *reg_out =
			(struct nvpva_vpu_exe_register_out_arg *)arg;
	struct pva_elf_image	*image;
	void			*exec_data = NULL;
	uint16_t		exe_id;
	bool			is_system = false;
	uint64_t		data_size;
	int			err = 0;

	data_size = reg_in->exe_data.size;
	exec_data = kmalloc(data_size, GFP_KERNEL);
	if (exec_data == NULL) {
		nvpva_err(&priv->pva->pdev->dev,
				"failed to allocate memory for elf");
		err = -ENOMEM;
		goto out;
	}

	err = copy_part_from_user(exec_data, data_size,
				  reg_in->exe_data);
	if (err) {
		nvpva_err(&priv->pva->pdev->dev,
			"failed to copy vpu exe data");
		goto free_mem;
	}

	err = pva_authenticate_vpu_app(priv->pva,
				       &priv->pva->pva_auth,
				       (uint8_t *)exec_data,
				       data_size,
				       false);
	if (err != 0) {
		err = pva_authenticate_vpu_app(priv->pva,
					       &priv->pva->pva_auth_sys,
					       (uint8_t *)exec_data,
					       data_size,
					       true);
		if (err != 0)
			goto free_mem;

		is_system = true;
	}

	err = pva_load_vpu_app(&priv->client->elf_ctx, exec_data,
				data_size, &exe_id,
				is_system,
				priv->pva->version);

	if (err) {
		nvpva_err(&priv->pva->pdev->dev,
			  "failed to register vpu app");
		goto free_mem;
	}

	reg_out->exe_id = exe_id;
	image = get_elf_image(&priv->client->elf_ctx, exe_id);
	reg_out->num_of_symbols = image->num_symbols -
				  image->num_sys_symbols;
	reg_out->symbol_size_total = image->symbol_size_total;

free_mem:

	if (exec_data != NULL)
		kfree(exec_data);
out:
	return err;
}

static int pva_unregister_vpu_exec(struct pva_private *priv, void *arg)
{
	struct nvpva_vpu_exe_unregister_in_arg *unreg_in =
		(struct nvpva_vpu_exe_unregister_in_arg *)arg;
	return pva_release_vpu_app(&priv->client->elf_ctx,
			unreg_in->exe_id, false);
}

static int pva_get_symbol_id(struct pva_private *priv, void *arg)
{
	struct nvpva_get_symbol_in_arg *symbol_in =
		(struct nvpva_get_symbol_in_arg *)arg;
	struct nvpva_get_symbol_out_arg *symbol_out =
		(struct nvpva_get_symbol_out_arg *)arg;
	char *symbol_buffer;
	int err = 0;
	uint64_t name_size = symbol_in->name.size;
	struct pva_elf_symbol symbol = {0};

	if (name_size > ELF_MAX_SYMBOL_LENGTH) {
		nvpva_warn(&priv->pva->pdev->dev, "symbol size too large:%llu",
			   symbol_in->name.size);
		name_size = ELF_MAX_SYMBOL_LENGTH;
	}

	symbol_buffer = kmalloc(name_size, GFP_KERNEL);
	if (symbol_buffer == NULL) {
		err = -ENOMEM;
		goto out;
	}

	err = copy_from_user(symbol_buffer,
			     (void __user *)symbol_in->name.addr,
			     name_size);
	if (err) {
		nvpva_err(&priv->pva->pdev->dev,
			   "failed to copy all name from user");
		goto free_mem;
	}

	if (symbol_buffer[name_size - 1] != '\0') {
		nvpva_warn(&priv->pva->pdev->dev,
			   "symbol name not terminated with NULL");
		symbol_buffer[name_size - 1] = '\0';
	}

	err = pva_get_sym_info(&priv->client->elf_ctx, symbol_in->exe_id,
				symbol_buffer, &symbol);
	if (err) {
		goto free_mem;
	}

	symbol_out->symbol.id = symbol.symbolID;
	symbol_out->symbol.size = symbol.size;
	symbol_out->symbol.isPointer =
		(symbol.type == (uint32_t)VMEM_TYPE_POINTER) ? 1U : 0U;
free_mem:
	kfree(symbol_buffer);
out:
	return err;
}

static int pva_get_symtab(struct pva_private *priv, void *arg)
{
	struct nvpva_get_sym_tab_in_arg *sym_tab_in =
		(struct nvpva_get_sym_tab_in_arg *)arg;

	int err = 0;
	struct nvpva_sym_info *sym_tab_buffer;
	u64 tab_size;

	err = pva_get_sym_tab_size(&priv->client->elf_ctx,
				   sym_tab_in->exe_id,
				   &tab_size);
	if (err)
		goto out;

	if (sym_tab_in->tab.size < tab_size) {
		nvpva_err(&priv->pva->pdev->dev,
			   "symbol table size smaller than needed:%llu",
			   sym_tab_in->tab.size);
		err = -EINVAL;
		goto out;
	}

	sym_tab_buffer = kmalloc(tab_size, GFP_KERNEL);
	if (sym_tab_buffer == NULL) {
		err = -ENOMEM;
		goto out;
	}

	err = pva_get_sym_tab(&priv->client->elf_ctx,
			  sym_tab_in->exe_id,
			  sym_tab_buffer);
	if (err)
		goto free_mem;

	err = copy_to_user((void __user *)sym_tab_in->tab.addr,
		     sym_tab_buffer,
		     tab_size);

free_mem:
	kfree(sym_tab_buffer);
out:
	return err;
}

/* Maximum VPU print buffer size is 16M */
#define MAX_VPU_PRINT_BUFFER_SIZE (16 * (1 << 20))
static int pva_set_vpu_print_buffer_size(struct pva_private *priv, void *arg)
{
	union nvpva_set_vpu_print_buffer_size_args *in_arg =
		(union nvpva_set_vpu_print_buffer_size_args *)arg;
	uint32_t buffer_size = in_arg->in.size;
	struct device *dev = &priv->pva->aux_pdev->dev;
	int err = 0;

	if (buffer_size > MAX_VPU_PRINT_BUFFER_SIZE) {
		dev_err(&priv->pva->pdev->dev,
			"requested VPU print buffer too large: %u > %u\n",
			buffer_size, MAX_VPU_PRINT_BUFFER_SIZE);
		err = -EINVAL;
		goto out;
	}

	mutex_lock(&priv->queue->list_lock);
	if (!list_empty(&priv->queue->tasklist)) {
		dev_err(&priv->pva->pdev->dev,
			"can't set VPU print buffer size when there's unfinished tasks\n");
		err = -EAGAIN;
		goto unlock;
	}

	if (priv->vpu_print_buffer != NULL) {
		pva_free_cb(dev, priv->vpu_print_buffer);
		priv->vpu_print_buffer = NULL;
	}

	if (buffer_size == 0)
		goto unlock;

	priv->vpu_print_buffer = pva_alloc_cb(dev, buffer_size);

	if (IS_ERR(priv->vpu_print_buffer)) {
		err = PTR_ERR(priv->vpu_print_buffer);
		priv->vpu_print_buffer = NULL;
	}

unlock:
	mutex_unlock(&priv->queue->list_lock);
out:
	return err;
}

static ssize_t pva_read_cb(struct pva_cb *cb, u8 __user *buffer,
			   size_t buffer_size)
{
	const u32 tail = cb->tail;
	const u32 head = *cb->head_va;
	const u32 size = cb->size;
	ssize_t ret = 0;
	u32 transfer1_size;
	u32 transfer2_size;

	/*
	 * Check if overflow happened, and if so, report it.
	 */
	if (*cb->err_va != 0) {
		pr_warn("pva: VPU print buffer overflowed!\n");
		ret = -ENOSPC;
		goto out;
	}

	transfer1_size = CIRC_CNT_TO_END(head, tail, size);
	if (transfer1_size <= buffer_size) {
		buffer_size -= transfer1_size;
	} else {
		transfer1_size = buffer_size;
		buffer_size = 0;
	}

	transfer2_size =
		CIRC_CNT(head, tail, size) - CIRC_CNT_TO_END(head, tail, size);
	if (transfer2_size <= buffer_size) {
		buffer_size -= transfer2_size;
	} else {
		transfer2_size = buffer_size;
		buffer_size = 0;
	}

	if (transfer1_size > 0) {
		unsigned long failed_count;

		failed_count = copy_to_user(buffer, cb->buffer_va + tail,
					    transfer1_size);
		if (failed_count > 0) {
			pr_err("pva: VPU print buffer: write to user buffer 1 failed\n");
			ret = -EFAULT;
			goto out;
		}
	}

	if (transfer2_size > 0) {
		unsigned long failed_count;

		failed_count = copy_to_user(&buffer[transfer1_size],
					    cb->buffer_va, transfer2_size);
		if (failed_count > 0) {
			pr_err("pva: VPU print buffer: write to user buffer 2 failed\n");
			ret = -EFAULT;
			goto out;
		}
	}

	cb->tail =
		(cb->tail + transfer1_size + transfer2_size) & (cb->size - 1);

	/*
	 * Update tail so that firmware knows the content is consumed; Memory
	 * barrier is needed here because the update should only be visible to
	 * firmware after the content is read.
	 */
	mb();
	*cb->tail_va = cb->tail;
	ret = transfer1_size + transfer2_size;

out:
	return ret;
}

static long pva_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct pva_private *priv = file->private_data;
	u8 buf[NVPVA_IOCTL_MAX_SIZE] __aligned(sizeof(u64));
	int err = 0;
	int err2 = 0;

	nvpva_dbg_fn(priv->pva, "");

	if ((_IOC_TYPE(cmd) != NVPVA_IOCTL_MAGIC) ||
	    (_IOC_NR(cmd) == 0) ||
	    (_IOC_NR(cmd) > NVPVA_IOCTL_NUMBER_MAX) ||
	    (_IOC_SIZE(cmd) > sizeof(buf)))
		return -ENOIOCTLCMD;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd))) {
			dev_err(&priv->pva->pdev->dev,
				"failed copy ioctl buffer from user; size: %u",
				_IOC_SIZE(cmd));
			return -EFAULT;
		}
	}

	switch (cmd) {
	case NVPVA_IOCTL_GET_SYMBOL_ID:
		err = pva_get_symbol_id(priv, buf);
		break;
	case NVPVA_IOCTL_GET_SYM_TAB:
		err = pva_get_symtab(priv, buf);
		break;
	case NVPVA_IOCTL_REGISTER_VPU_EXEC:
		err = pva_register_vpu_exec(priv, buf);
		break;
	case NVPVA_IOCTL_UNREGISTER_VPU_EXEC:
		err = pva_unregister_vpu_exec(priv, buf);
		break;
	case NVPVA_IOCTL_PIN:
		err = pva_pin(priv, buf);
		break;
	case NVPVA_IOCTL_UNPIN:
		err = pva_unpin(priv, buf);
		break;
	case NVPVA_IOCTL_SUBMIT:
		err = pva_submit(priv, buf);
		break;
	case NVPVA_IOCTL_SET_VPU_PRINT_BUFFER_SIZE:
		err = pva_set_vpu_print_buffer_size(priv, buf);
		break;
	default:
		err2 = -ENOIOCTLCMD;
		break;
	}

	if ((err2 == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err2 = copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd));

	err = (err == 0) ? err2 : err;

	return err;
}

static int pva_open(struct inode *inode, struct file *file)
{
	struct nvhost_device_data *pdata = container_of(
		inode->i_cdev, struct nvhost_device_data, ctrl_cdev);
	struct platform_device *pdev = pdata->pdev;
	struct pva *pva = pdata->private_data;
	struct pva_private *priv;
	int err = 0;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (priv == NULL) {
		err = -ENOMEM;
		goto err_alloc_priv;
	}

	file->private_data = priv;
	priv->pva = pva;
	priv->client = nvpva_client_context_alloc(pdev, pva, current->pid);
	if (priv->client == NULL) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "failed to allocate client context");
		goto err_alloc_context;
	}

	priv->queue = nvpva_queue_alloc(pva->pool,
					priv->client->cntxt_dev,
					MAX_PVA_TASK_COUNT_PER_QUEUE);

	if (IS_ERR(priv->queue)) {
		err = PTR_ERR(priv->queue);
		goto err_alloc_queue;
	}

	sema_init(&priv->queue->task_pool_sem, MAX_PVA_TASK_COUNT_PER_QUEUE);
	err = nvhost_module_busy(pva->pdev);
	if (err < 0) {
		dev_err(&pva->pdev->dev, "error in powering up pva %d",
			err);
		goto err_device_busy;
	}

	return nonseekable_open(inode, file);

err_device_busy:
	nvpva_queue_put(priv->queue);
err_alloc_queue:
	nvpva_client_context_put(priv->client);
err_alloc_context:
	nvhost_module_remove_client(pdev, priv);
	kfree(priv);
err_alloc_priv:
	return err;
}

static void pva_queue_flush(struct pva *pva, struct nvpva_queue *queue)
{
	u32 flags = PVA_CMD_INT_ON_ERR | PVA_CMD_INT_ON_COMPLETE;
	struct pva_cmd_status_regs status = {};
	struct pva_cmd_s cmd = {};
	int err = 0;
	u32 nregs;

	nregs = pva_cmd_abort_task(&cmd, queue->id, flags);
	err = nvhost_module_busy(pva->pdev);
	if (err < 0) {
		dev_err(&pva->pdev->dev, "error in powering up pva %d",
			err);
		goto err_out;
	}

	err = pva->version_config->submit_cmd_sync(pva, &cmd, nregs, queue->id,
						   &status);
	nvhost_module_idle(pva->pdev);
	if (err < 0) {
		dev_err(&pva->pdev->dev, "failed to issue FW abort command: %d",
			err);
		goto err_out;
	}
	/* Ensure that response is valid */
	if (status.error != PVA_ERR_NO_ERROR) {
		dev_err(&pva->pdev->dev, "PVA FW Abort rejected: %d",
			status.error);
	}

err_out:
	return;
}

static int pva_release(struct inode *inode, struct file *file)
{
	struct pva_private *priv = file->private_data;
	bool queue_empty;
	int i;

	flush_workqueue(priv->pva->task_status_workqueue);
	mutex_lock(&priv->queue->list_lock);
	queue_empty = list_empty(&priv->queue->tasklist);
	mutex_unlock(&priv->queue->list_lock);
	if (!queue_empty) {
		/* Cancel remaining tasks */
		nvpva_dbg_info(priv->pva, "cancel remaining tasks");
		pva_queue_flush(priv->pva, priv->queue);
	}

	/* make sure all tasks have been finished */
	for (i = 0; i < MAX_PVA_TASK_COUNT_PER_QUEUE; i++) {
		if (down_killable(&priv->queue->task_pool_sem) != 0) {
			nvpva_err(
				&priv->pva->pdev->dev,
				"interrupted while waiting %d tasks\n",
				MAX_PVA_TASK_COUNT_PER_QUEUE - i);
			pva_abort(priv->pva);
			break;
		}
	}

	nvhost_module_idle(priv->pva->pdev);

	/* Release reference to client */
	nvpva_client_context_put(priv->client);

	/*
	 * Release handle to the queue (on-going tasks have their
	 * own references to the queue
	 */
	nvpva_queue_put(priv->queue);

	/* Free VPU print buffer if allocated */
	if (priv->vpu_print_buffer != NULL) {
		pva_free_cb(&priv->pva->pdev->dev, priv->vpu_print_buffer);
		priv->vpu_print_buffer = NULL;
	}

	/* Finally, release the private data */
	kfree(priv);

	return 0;
}

static ssize_t pva_read_vpu_print_buffer(struct file *file,
					 char __user *user_buffer,
					 size_t buffer_size, loff_t *off)
{
	struct pva_private *priv = file->private_data;
	ssize_t ret;

	mutex_lock(&priv->queue->list_lock);

	if (priv->vpu_print_buffer != NULL) {
		ret = pva_read_cb(priv->vpu_print_buffer, user_buffer,
				  buffer_size);
	} else {
		pr_warn("pva: VPU print buffer size needs to be specified\n");
		ret = -EIO;
	}

	mutex_unlock(&priv->queue->list_lock);

	return ret;
}

const struct file_operations tegra_pva_ctrl_ops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = pva_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = pva_ioctl,
#endif
	.open = pva_open,
	.release = pva_release,
	.read = pva_read_vpu_print_buffer,
};
