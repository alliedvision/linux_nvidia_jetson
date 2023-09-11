/*
 * NVHOST queue management for T194
 *
 * Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>

#include <linux/nvhost.h>

#include "nvpva_syncpt.h"
#include "nvpva_queue.h"
#include "pva_bit_helpers.h"
#include "pva.h"

#define CMDBUF_SIZE	4096

/**
 * @brief Describe a task pool struct
 *
 * Array of fixed task memory is allocated during queue_alloc call.
 * The memory will be shared for various task based on availability
 *
 * dma_addr		Physical address of task memory pool
 * aux_dma_addr		Physical address of task aux memory pool
 * va			Virtual address of the task memory pool
 * aux_va			Virtual address of the task memory pool
 * kmem_addr		Kernel memory for task struct
 * lock			Mutex lock for the array access.
 * alloc_table		Keep track of the index being assigned
 *			and freed for a task
 * max_task_cnt		Maximum task count that can be supported.
 *
 */
struct nvpva_queue_task_pool {
	dma_addr_t dma_addr;
	dma_addr_t aux_dma_addr;
	void *va;
	void *aux_va;
	void *kmem_addr[MAX_PVA_SEG_COUNT_PER_QUEUE];
	struct mutex lock;

	unsigned long alloc_table[NUM_POOL_ALLOC_SUB_TABLES];
	unsigned int max_task_cnt;
};

static int nvpva_queue_task_pool_alloc(struct platform_device *pdev,
				       struct platform_device *pprim_dev,
				       struct platform_device *paux_dev,
				       struct nvpva_queue *queue,
				       unsigned int num_tasks)
{
	int err = 0;
	unsigned int i;
	unsigned int num_segments = num_tasks/MAX_PVA_TASK_COUNT_PER_QUEUE_SEG;
	struct nvpva_queue_task_pool *task_pool;
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct pva *pva = pdata->private_data;
	u64 mem_size;

	task_pool = queue->task_pool;
	memset(task_pool->kmem_addr, 0, sizeof(task_pool->kmem_addr));

	/* Allocate the kernel memory needed for the task */
	if (queue->task_kmem_size) {
		for (i = 0; i < num_segments; i++) {
			task_pool->kmem_addr[i] =
				kcalloc(MAX_PVA_TASK_COUNT_PER_QUEUE_SEG,
					queue->task_kmem_size, GFP_KERNEL);
			if (!task_pool->kmem_addr[i]) {
				nvpva_err(&pdev->dev,
					   "failed to allocate " \
					   "task_pool->kmem_addr");
				err = -ENOMEM;
				goto err_alloc_task_pool;
			}
		}
	}

	mem_size = queue->task_dma_size * num_tasks;
	if (queue->task_dma_size !=  mem_size / num_tasks) {
		nvpva_err(&pdev->dev, "mem size too large");
		err = -EINVAL;
		goto err_alloc_task_pool;
	}

	/* Allocate memory for the task itself */
	task_pool->va = dma_alloc_attrs(&pprim_dev->dev,
				mem_size,
				&task_pool->dma_addr, GFP_KERNEL,
				0);

	if (task_pool->va == NULL) {
		nvpva_err(&pdev->dev, "failed to allocate task_pool->va");
		err = -ENOMEM;
		goto err_alloc_task_pool;
	}

	mem_size = queue->aux_dma_size * num_tasks;
	if (queue->aux_dma_size !=  mem_size / num_tasks) {
		nvpva_err(&pdev->dev, "mem size too large");
		err = -EINVAL;
		goto err_alloc_aux_task_pool;
	}

	/* Allocate aux memory for the task itself */
	task_pool->aux_va = dma_alloc_attrs(&paux_dev->dev,
				mem_size,
				&task_pool->aux_dma_addr, GFP_KERNEL,
				0);

	if (task_pool->aux_va == NULL) {
		nvpva_err(&pdev->dev, "failed to allocate task_pool->aux_va");
		err = -ENOMEM;
		goto err_alloc_aux_task_pool;
	}

	nvpva_dbg_info(pva,
		       "task_pool->dma_addr = %llx, task_pool->auxdma_addr = %llx",
		       (u64)task_pool->dma_addr, (u64)task_pool->aux_dma_addr);

	task_pool->max_task_cnt = num_tasks;
	mutex_init(&task_pool->lock);

	return err;

err_alloc_aux_task_pool:
	dma_free_attrs(&pprim_dev->dev,
			queue->task_dma_size * task_pool->max_task_cnt,
			task_pool->va, task_pool->dma_addr,
			0);
err_alloc_task_pool:
	for (i = 0; i < num_segments; i++) {
		if (task_pool->kmem_addr[i] == NULL)
			continue;

		kfree(task_pool->kmem_addr[i]);
		task_pool->kmem_addr[i] = NULL;
	}

	return err;
}

static void nvpva_queue_task_free_pool(struct platform_device *pdev,
				       struct nvpva_queue *queue)
{
	unsigned int i;
	unsigned int segments;
	u64 mem_size;
	struct nvpva_queue_task_pool *task_pool =
		(struct nvpva_queue_task_pool *)queue->task_pool;

	segments = task_pool->max_task_cnt/MAX_PVA_TASK_COUNT_PER_QUEUE_SEG;

	mem_size = queue->task_dma_size * task_pool->max_task_cnt;
	if (queue->task_dma_size  !=  mem_size / task_pool->max_task_cnt) {
		nvpva_err(&pdev->dev, "mem size too large");
		return;
	}

	dma_free_attrs(&queue->vm_pprim_dev->dev,
			mem_size,
			task_pool->va, task_pool->dma_addr,
			0);

	mem_size = queue->aux_dma_size * task_pool->max_task_cnt;
	if (queue->aux_dma_size  !=  mem_size / task_pool->max_task_cnt) {
		nvpva_err(&pdev->dev, "mem size too large");
		return;
	}

	dma_free_attrs(&queue->vm_paux_dev->dev,
			mem_size,
			task_pool->aux_va, task_pool->aux_dma_addr,
			0);
	for (i = 0; i < segments; i++)
		kfree(task_pool->kmem_addr[i]);

	memset(task_pool->alloc_table, 0, sizeof(task_pool->alloc_table));
	task_pool->max_task_cnt = 0U;
}

static int nvpva_queue_dump(struct nvpva_queue_pool *pool,
		struct nvpva_queue *queue,
		struct seq_file *s)
{
	if (pool->ops && pool->ops->dump)
		pool->ops->dump(queue, s);

	return 0;
}

static int queue_dump(struct seq_file *s, void *data)
{
	struct nvpva_queue_pool *pool = s->private;
	unsigned long queue_id;
	u32 i;

	mutex_lock(&pool->queue_lock);
	for (i = 0; i < NUM_POOL_ALLOC_SUB_TABLES; i++)
		for_each_set_bit(queue_id,
				 &pool->alloc_table[i],
				 pool->max_queue_cnt)
			nvpva_queue_dump(pool,
					&pool->queues[64 * i + queue_id], s);

	mutex_unlock(&pool->queue_lock);

	return 0;
}

static int queue_expose_open(struct inode *inode, struct file *file)
{
	return single_open(file, queue_dump, inode->i_private);
}

static const struct file_operations queue_expose_operations = {
	.open = queue_expose_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

struct nvpva_queue_pool *nvpva_queue_init(struct platform_device *pdev,
					struct platform_device *paux_dev,
					struct nvpva_queue_ops *ops,
					unsigned int num_queues)
{
	struct nvhost_device_data *pdata;
	struct nvpva_queue_pool *pool;
	struct nvpva_queue *queues;
	struct nvpva_queue *queue;
	struct nvpva_queue_task_pool *task_pool;
	unsigned int i;
	int err;

	pool = kzalloc(sizeof(struct nvpva_queue_pool), GFP_KERNEL);
	if (pool == NULL) {
		err = -ENOMEM;
		goto fail_alloc_pool;
	}

	queues = kcalloc(num_queues, sizeof(struct nvpva_queue), GFP_KERNEL);
	if (queues == NULL) {
		err = -ENOMEM;
		goto fail_alloc_queues;
	}

	task_pool = kcalloc(num_queues,
			sizeof(struct nvpva_queue_task_pool), GFP_KERNEL);
	if (task_pool == NULL) {
		nvpva_err(&pdev->dev, "failed to allocate task_pool");
		err = -ENOMEM;
		goto fail_alloc_task_pool;
	}

	pdata = platform_get_drvdata(pdev);

	/* initialize pool and queues */
	pool->pdev = pdev;
	pool->pprim_dev = paux_dev;
	pool->ops = ops;
	pool->queues = queues;
	memset(pool->alloc_table, 0, sizeof(pool->alloc_table));
	pool->max_queue_cnt = num_queues;
	pool->queue_task_pool = task_pool;
	mutex_init(&pool->queue_lock);

	debugfs_create_file("queues", 0444,
			pdata->debugfs, pool,
			&queue_expose_operations);


	for (i = 0; i < num_queues; i++) {
		queue = &queues[i];
		queue->id = i;
		queue->pool = pool;
		queue->task_pool = (void *)&task_pool[i];
		queue->batch_id = 0U;
		nvpva_queue_get_task_size(queue);
	}

	return pool;

fail_alloc_task_pool:
	kfree(pool->queues);
fail_alloc_queues:
	kfree(pool);
fail_alloc_pool:
	return ERR_PTR(err);
}

void nvpva_queue_deinit(struct nvpva_queue_pool *pool)
{
	if (!pool)
		return;

	kfree(pool->queue_task_pool);
	kfree(pool->queues);
	kfree(pool);
	pool = NULL;
}

void nvpva_queue_abort_all(struct nvpva_queue_pool *pool)
{
	u32 id;
	u32 i;

	mutex_lock(&pool->queue_lock);
	for (i = 0; i < NUM_POOL_ALLOC_SUB_TABLES; i++)
		for_each_set_bit(id,
				 &pool->alloc_table[i],
				 pool->max_queue_cnt)
			nvpva_queue_abort(&pool->queues[64 * i + id]);

	mutex_unlock(&pool->queue_lock);
}

static void nvpva_queue_release(struct kref *ref)
{
	struct nvpva_queue *queue = container_of(ref, struct nvpva_queue,
						kref);
	struct nvpva_queue_pool *pool = queue->pool;

	struct nvhost_device_data *pdata = platform_get_drvdata(pool->pdev);
	struct pva *pva = pdata->private_data;

	nvpva_dbg_fn(pva, "");

	/* release allocated resources */
	nvpva_syncpt_put_ref_ext(pool->pdev, queue->syncpt_id);

	/* free the task_pool */
	if (queue->task_dma_size)
		nvpva_queue_task_free_pool(pool->pdev, queue);

	/* free the queue mutex */
	mutex_destroy(&queue->tail_lock);

	/* ..and mark the queue free */
	mutex_lock(&pool->queue_lock);
	clear_bit(queue->id%64, &pool->alloc_table[queue->id/64]);
	mutex_unlock(&pool->queue_lock);
}

void nvpva_queue_put(struct nvpva_queue *queue)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(queue->pool->pdev);
	struct pva *pva = pdata->private_data;

	nvpva_dbg_fn(pva, "");
	kref_put(&queue->kref, nvpva_queue_release);
}

void nvpva_queue_get(struct nvpva_queue *queue)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(queue->pool->pdev);
	struct pva *pva = pdata->private_data;

	nvpva_dbg_fn(pva, "");
	kref_get(&queue->kref);
}

struct nvpva_queue *nvpva_queue_alloc(struct nvpva_queue_pool *pool,
				      struct platform_device *paux_dev,
				      unsigned int num_tasks)
{
	struct platform_device *pdev = pool->pdev;
	struct nvpva_queue *queues = pool->queues;
	struct nvpva_queue *queue;
	int index = 0;
	int err = 0;
	u32 syncpt_val;

	mutex_lock(&pool->queue_lock);

	index = rmos_find_first_zero_bit((u32 *) pool->alloc_table,
					  pool->max_queue_cnt);

	/* quit if we found a queue */
	if (index >= pool->max_queue_cnt) {
		dev_err(&pdev->dev, "failed to get free Queue\n");
		err = -ENOMEM;
		goto err_alloc_queue;
	}

	/* reserve the queue */
	queue = &queues[index];
	set_bit(index%64, &pool->alloc_table[index/64]);

	/* allocate a syncpt for the queue */
	queue->syncpt_id = nvpva_get_syncpt_client_managed(pdev, "pva_syncpt");
	if (queue->syncpt_id == 0) {
		dev_err(&pdev->dev, "failed to get syncpt\n");
		err = -ENOMEM;
		goto err_alloc_syncpt;
	}

	if (nvhost_syncpt_read_ext_check(pdev,
					 queue->syncpt_id,
					 &syncpt_val) != 0) {
		err = -EIO;
		goto err_read_syncpt;
	}

	atomic_set(&queue->syncpt_maxval, syncpt_val);

	/* initialize queue ref count and sequence*/
	kref_init(&queue->kref);
	queue->sequence = 0;

	/* initialize task list */
	INIT_LIST_HEAD(&queue->tasklist);
	mutex_init(&queue->list_lock);

	/* initialize task list */
	queue->attr = NULL;
	mutex_init(&queue->attr_lock);

	mutex_unlock(&pool->queue_lock);

	queue->vm_pdev = pdev;
	queue->vm_pprim_dev = pool->pprim_dev;

	mutex_init(&queue->tail_lock);
	queue->vm_paux_dev = paux_dev;

	if (queue->task_dma_size) {
		err = nvpva_queue_task_pool_alloc(queue->vm_pdev,
						  queue->vm_pprim_dev,
						  queue->vm_paux_dev,
						  queue,
						  num_tasks);
		if (err < 0)
			goto err_alloc_task_pool;
	}

	return queue;

err_alloc_task_pool:
	mutex_lock(&pool->queue_lock);
err_read_syncpt:
	nvpva_syncpt_put_ref_ext(pool->pdev, queue->syncpt_id);
err_alloc_syncpt:
	clear_bit(queue->id%64, &pool->alloc_table[queue->id/64]);
err_alloc_queue:
	mutex_unlock(&pool->queue_lock);
	return ERR_PTR(err);
}

int nvpva_queue_abort(struct nvpva_queue *queue)
{
	struct nvpva_queue_pool *pool = queue->pool;

	if (pool->ops && pool->ops->abort)
		return pool->ops->abort(queue);

	return 0;
}

int nvpva_queue_submit(struct nvpva_queue *queue, void *task_arg)
{
	struct nvpva_queue_pool *pool = queue->pool;

	if (pool->ops && pool->ops->submit)
		return pool->ops->submit(queue, task_arg);

	return 0;
}

int nvpva_queue_set_attr(struct nvpva_queue *queue, void *arg)
{
	struct nvpva_queue_pool *pool = queue->pool;

	if (pool->ops && pool->ops->set_attribute)
		return pool->ops->set_attribute(queue, arg);

	return 0;
}

struct nvpva_queue_task {
	struct platform_device *host1x_pdev;

	struct nvpva_queue *queue;

	dma_addr_t dma_addr;
	u32 *cpu_addr;
};

int nvpva_queue_get_task_size(struct nvpva_queue *queue)
{
	struct nvpva_queue_pool *pool = queue->pool;

	if (pool->ops && pool->ops->get_task_size)
		pool->ops->get_task_size(&queue->task_dma_size,
					 &queue->task_kmem_size,
					 &queue->aux_dma_size);

	return 0;
}

int nvpva_queue_alloc_task_memory(
			struct nvpva_queue *queue,
			struct nvpva_queue_task_mem_info *task_mem_info)
{
	int err = 0;
	unsigned int index;
	unsigned int hw_offset;
	unsigned int sw_offset;
	unsigned int seg_base;
	unsigned int seg_index;
	size_t	aux_hw_offset;
	struct platform_device *pdev = queue->pool->pdev;
	struct nvpva_queue_task_pool *task_pool =
		(struct nvpva_queue_task_pool *)queue->task_pool;

	mutex_lock(&task_pool->lock);

	index = rmos_find_first_zero_bit((u32 *) task_pool->alloc_table,
					 task_pool->max_task_cnt);

	/* quit if pre-allocated task array is not free */
	if (index >= task_pool->max_task_cnt) {
		dev_err(&pdev->dev,
			"failed to get Task Pool Memory\n");
		err = -EAGAIN;
		goto err_alloc_task_mem;
	}

	/* assign the task array */
	seg_index = index%MAX_PVA_TASK_COUNT_PER_QUEUE_SEG;
	seg_base = (index/MAX_PVA_TASK_COUNT_PER_QUEUE_SEG);
	set_bit(index%64, &task_pool->alloc_table[index/64]);
	hw_offset = index * queue->task_dma_size;
	aux_hw_offset = index * queue->aux_dma_size;
	sw_offset = seg_index * queue->task_kmem_size;
	task_mem_info->kmem_addr =
		(void *)((u8 *)task_pool->kmem_addr[seg_base] + sw_offset);
	task_mem_info->va = (void *)((u8 *)task_pool->va + hw_offset);
	task_mem_info->dma_addr = task_pool->dma_addr + hw_offset;
	task_mem_info->aux_va = (void *)((u8 *)task_pool->aux_va + aux_hw_offset);
	if ((U64_MAX - task_pool->aux_dma_addr) < task_pool->aux_dma_addr) {
		err = -EFAULT;
		goto err_alloc_task_mem;
	}

	task_mem_info->aux_dma_addr = task_pool->aux_dma_addr + aux_hw_offset;
	task_mem_info->pool_index = index;

err_alloc_task_mem:
	mutex_unlock(&task_pool->lock);

	return err;
}

void nvpva_queue_free_task_memory(struct nvpva_queue *queue, int index)
{
	unsigned int hw_offset;
	unsigned int sw_offset;
	unsigned int seg_index;
	unsigned int seg_base;

	u8 *task_kmem, *task_dma_va;
	struct nvpva_queue_task_pool *task_pool =
			(struct nvpva_queue_task_pool *)queue->task_pool;

	/* clear task kernel and dma virtual memory contents*/
	seg_index = index%MAX_PVA_TASK_COUNT_PER_QUEUE_SEG;
	seg_base = (index/MAX_PVA_TASK_COUNT_PER_QUEUE_SEG);
	hw_offset = index * queue->task_dma_size;
	sw_offset = seg_index * queue->task_kmem_size;
	task_kmem = (u8 *)task_pool->kmem_addr[seg_base] + sw_offset;
	task_dma_va = (u8 *)task_pool->va + hw_offset;

	memset(task_kmem, 0, queue->task_kmem_size);
	memset(task_dma_va, 0, queue->task_dma_size);

	mutex_lock(&task_pool->lock);
	clear_bit(index%64, &task_pool->alloc_table[index/64]);
	mutex_unlock(&task_pool->lock);
}
