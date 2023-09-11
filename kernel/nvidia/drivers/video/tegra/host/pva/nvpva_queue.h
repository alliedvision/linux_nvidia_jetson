/*
 * NVPVA Queue management header for T194 and T234
 *
 * Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef __NVPVA_NVPVA_QUEUE_H__
#define __NVPVA_NVPVA_QUEUE_H__

#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>

#define NUM_POOL_ALLOC_SUB_TABLES	4

struct nvpva_queue_task_pool;
/** @brief Holds PVA HW task which can be submitted to PVA R5 FW */
struct pva_hw_task;

/**
 * @brief	Describe a allocated task mem struct
 *
 * kmem_addr	Address for the task kernel memory
 * dma_addr	Physical address of task memory
 * aux_dma_addr	Physical address of aux task memory
 * va		Virtual address of the task memory
 * aux_va	Virtual address of the aux task memory
 * pool_index	Index to the allocated task memory
 *
 * This is keep track of the memory details of the task
 * struct that is being shared between kernel and firmware.
 */
struct nvpva_queue_task_mem_info {
	void *kmem_addr;
	dma_addr_t dma_addr;
	dma_addr_t aux_dma_addr;
	void *va;
	void *aux_va;
	int pool_index;
};
/**
 * @brief		Information needed in a Queue
 *
 * pool			pointer queue pool
 * kref			struct kref for reference count
 * syncpt_id		Host1x syncpt id
 * id			Queue id
 * list_lock		mutex for tasks lists control
 * tasklist		Head of tasks list
 * sequence		monotonically incrementing task id per queue
 * task_pool		pointer to struct for task memory pool
 * task_dma_size	dma size used in hardware for a task
 * task_kmem_size	kernel memory size for a task
 * aux_dma_size		kernel memory size for a task aux buffer
 * attr			queue attribute associated with the host module
 *
 */
struct nvpva_queue {
	struct nvpva_queue_task_pool *task_pool;
	struct nvpva_queue_pool *pool;
	struct kref kref;
	u32 id;

	/*wait list for task mem requester*/
	struct semaphore task_pool_sem;

	/* Host1x resources */
	struct nvhost_channel *channel;
	struct platform_device *vm_pdev;
	struct platform_device *vm_pprim_dev;
	struct platform_device *vm_paux_dev;
	u32 syncpt_id;
	u32 local_sync_counter;
	atomic_t syncpt_maxval;

	size_t task_dma_size;
	size_t task_kmem_size;
	size_t aux_dma_size;

	u32 sequence;

	struct mutex attr_lock;
	void *attr;

	struct mutex list_lock;
	struct list_head tasklist;

	/*! Mutex for exclusive access of tail task submit */
	struct mutex tail_lock;
	struct pva_hw_task *old_tail;
	struct pva_hw_task *hw_task_tail;

	u64 batch_id;
};

/**
 * @brief	hardware specific queue callbacks
 *
 * dump			dump the task information
 * abort		abort all tasks from a queue
 * submit		submit the given list of tasks to hardware
 * get_task_size	get the dma size needed for the task in hw
 *			and the kernel memory size needed for task.
 *
 */
struct nvpva_queue_ops {
	void (*dump)(struct nvpva_queue *queue, struct seq_file *s);
	int (*abort)(struct nvpva_queue *queue);
	int (*submit)(struct nvpva_queue *queue, void *task_arg);
	void (*get_task_size)(size_t *dma_size,
			      size_t *kmem_size,
			      size_t *aux_dma_size);
	int (*set_attribute)(struct nvpva_queue *queue, void *arg);
};

/**
 * @brief	Queue pool data structure to hold queue table
 *
 * pdev			Pointer to the Queue client device
 * ops			Pointer to hardware specific queue ops
 * queues		Queues available for the client
 * queue_lock		Mutex for the bitmap of reserved queues
 * alloc_table		Bitmap of allocated queues
 * max_queue_cnt	Max number queues available for client
 * queue_task_pool	Pointer to the task memory pool for queues.
 *
 */
struct nvpva_queue_pool {
	struct platform_device *pdev;
	struct platform_device *pprim_dev;
	struct nvpva_queue_ops *ops;
	struct nvpva_queue *queues;
	struct mutex queue_lock;
	unsigned long alloc_table[NUM_POOL_ALLOC_SUB_TABLES];
	unsigned int max_queue_cnt;
	void *queue_task_pool;
};

/**
 * @brief	Initialize queue structures
 *
 * This function allocates and initializes queue data structures.
 *
 * @param pdev		Pointer to the Queue client device
 * @param paux_dev	Pointer to the Queue client aux device
 * @param ops		Pointer to device speicific callbacks
 * @param num_queues	Max number queues available for client
 * @return		pointer to queue pool
 *
 */
struct nvpva_queue_pool *nvpva_queue_init(struct platform_device *pdev,
					struct platform_device *paux_dev,
					struct nvpva_queue_ops *ops,
					unsigned int num_queues);

/**
 * @brief	De-initialize queue structures
 *
 * This function free's all queue data structures.
 *
 * @param pool	pointer to queue pool
 * @return	void
 *
 */
void nvpva_queue_deinit(struct nvpva_queue_pool *pool);

/**
 * @brief	Release reference of a queue
 *
 * This function releases reference for a queue.
 *
 * @param queue	Pointer to an allocated queue.
 * @return	void
 *
 */
void nvpva_queue_put(struct nvpva_queue *queue);

/**
 * @brief	Get reference on a queue.
 *
 * This function used to get a reference to an already allocated queue.
 *
 * @param queue	Pointer to an allocated queue.
 * @return	None
 *
 */
void nvpva_queue_get(struct nvpva_queue *queue);

/**
 * @brief	Allocate a queue for client.
 *
 * This function allocates a queue from the pool to client for the user.
 *
 * @param pool		Pointer to a queue pool table
 * @param paux_dev	pointer to auxiliary dev
 * @param num_tasks	Max number of tasks per queue
 *
 * @return		Pointer to a queue struct on success
 *			or negative error on failure.
 *
 */
struct nvpva_queue *nvpva_queue_alloc(struct nvpva_queue_pool *pool,
				      struct platform_device *paux_dev,
				      unsigned int num_tasks);

/**
 * @brief		Abort all active queues
 *
 * @param pool		Pointer to a queue pool table
 */
void nvpva_queue_abort_all(struct nvpva_queue_pool *pool);

/**
 * @brief	Abort tasks within a client queue
 *
 * This function aborts all tasks from the given clinet queue. If there is no
 * active tasks, the function call is no-op.
 * It is expected to be called when an active device fd gets closed.
 *
 * @param queue	Pointer to an allocated queue
 * @return	None
 *
 */
int nvpva_queue_abort(struct nvpva_queue *queue);

/**
 * @brief	submits the given list of tasks to hardware
 *
 * This function submits the given list of tasks to hardware.
 * The submit structure is updated with the fence values as appropriate.
 *
 * @param queue		Pointer to an allocated queue
 * @param submit	Submit the given list of tasks to hardware
 * @return		0 on success or negative error code on failure.
 *
 */
int nvpva_queue_submit(struct nvpva_queue *queue, void *submit);

/**
 * @brief	Get the Task Size needed
 *
 * This function get the needed memory size for the task. This memory is
 * shared memory between kernel and firmware
 *
 * @param queue	Pointer to an allocated queue
 * @return	Size of the task
 *
 */
int nvpva_queue_get_task_size(struct nvpva_queue *queue);

/**
 * @brief	Allocate a memory from task memory pool
 *
 * This function helps to assign a task memory from
 * the preallocated task memory pool. This memory is shared memory between
 * kernel and firmware
 *
 * @queue		Pointer to an allocated queue
 * @task_mem_info	Pointer to nvpva_queue_task_mem_info struct
 *
 * @return	0 on success, otherwise a negative error code is returned
 *
 */
int nvpva_queue_alloc_task_memory(
			struct nvpva_queue *queue,
			struct nvpva_queue_task_mem_info *task_mem_info);

/**
 * @brief	Free the assigned task memory
 *
 * This function helps to unset the assigned task memory
 *
 * @param queue	Pointer to an allocated queue
 * @param index	Index of the assigned task pool memory
 * @return	void
 *
 */
void nvpva_queue_free_task_memory(struct nvpva_queue *queue, int index);

/**
 * @brief	Sets the attribute to the queue
 *
 * This function set the attribute of the queue with the arguments passed
 *
 * @param queue		Pointer to an allocated queue
 * @param arg		The structure which consists of the id and value
 * @return		0 on success or negative error code on failure.
 *
 */
int nvpva_queue_set_attr(struct nvpva_queue *queue, void *arg);

#endif
