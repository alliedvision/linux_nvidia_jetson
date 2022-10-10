/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_WORKER_H
#define NVGPU_WORKER_H

#include <nvgpu/atomic.h>
#include <nvgpu/cond.h>
#include <nvgpu/list.h>
#include <nvgpu/lock.h>
#include <nvgpu/thread.h>
#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_worker;

/**
 * @file
 *
 * nvgpu_worker is a fifo based produce-consumer worker for the
 * nvgpu driver. Its meant to provide a generic implementation with hooks
 * to allow each application to implement specific use cases for producing
 * and consuming the work. The user of this API enqueues new work and awakens
 * the worker thread. On being awakened, the worker thread checks for pending
 * work or a user provided terminating condition. The generic poll
 * implementation also provides for early terminating conditions as well as
 * pre and post processing hooks. Below is the generic interface of
 * nvgpu_worker consume function.
 *
 * static int nvgpu_worker_poll_work(void *arg)
 * {
 * 	struct nvgpu_worker *worker = (struct nvgpu_worker *)arg;
 * 	int get = 0;
 *
 * 	worker->ops->pre_process(worker);
 *
 * 	while (!nvgpu_thread_should_stop(&worker->poll_task)) {
 * 		int ret;
 *
 * 		ret = NVGPU_COND_WAIT_INTERRUPTIBLE(
 * 				&worker->wq,
 * 				nvgpu_worker_pending(worker, get) ||
 * 				worker->ops->wakeup_condition(worker),
 * 				worker->ops->wakeup_timeout(worker));
 *
 * 		if (worker->ops->wakeup_early_exit(worker)) {
 * 			break;
 * 		}
 *
 * 		if (ret == 0) {
 * 			worker->ops->process_item(worker, &get);
 * 		}
 *
 * 		worker->ops->post_process(worker);
 * 	}
 * 	return 0;
 * }
 */

/**
 * @defgroup worker
 * @ingroup unit-common-utils
 * @{
 */

/**
 * Operations that can be done to a #nvgpu_worker
 */
struct nvgpu_worker_ops {
	/**
	 * @brief This interface is used to pass any callback to be invoked the
	 * first time the background thread is launched.
	 *
	 * Can be set to NULL if not applicable for this worker.
	 *
	 * @param worker [in]	The worker
	 */
	void (*pre_process)(struct nvgpu_worker *worker);

	/**
	 * @brief This interface is used to pass any callback for early
	 * terminating the worker thread after the thread has been awakened.
	 *
	 * @param worker [in]	The worker
	 *
	 * Can be set to NULL if not applicable for this worker.
	 *
	 * @return true if the thread should exit. false otherwise.
	 */
	bool (*wakeup_early_exit)(struct nvgpu_worker *worker);

	/**
	 * @brief This interface is used to pass any post processing callback
	 * for the worker thread after wakeup. The worker thread executes this
	 * callback every time before sleeping again.
	 *
	 * Can be set to NULL if not applicable for this worker.
	 *
	 * @param worker [in]	The worker
	 */
	void (*wakeup_post_process)(struct nvgpu_worker *worker);

	/**
	 * @brief This interface is used to handle each of the individual
	 * work_item just after the background thread has been awakened. This
	 * should always point to a valid callback function.
	 *
	 * @param worker [in]	The worker
	 */
	void (*wakeup_process_item)(struct nvgpu_list_node *work_item);

	/**
	 * @brief Any additional condition that is 'OR'ed with
	 * worker_pending_items.
	 *
	 * @param worker [in]	The worker
	 *
	 * Can be set to NULL if not applicable for this worker.
	 *
	 * @return true if the worker should wakeup. false otherwise.
	 */
	bool (*wakeup_condition)(struct nvgpu_worker *worker);

	/**
	 * @brief Used to pass any timeout value for wakeup.
	 *
	 * Can be set to NULL if not applicable for this worker.
	 *
	 * @param worker [in]	The worker
	 *
	 * @return The timeout value for waking up the worker.
	 */
	u32 (*wakeup_timeout)(struct nvgpu_worker *worker);
};

/**
 * Metadata object describing a worker.
 */
struct nvgpu_worker {
	/**
	 * The GPU struct
	 */
	struct gk20a *g;
	/**
	 * Name of the worker thread
	 */
	char thread_name[64];
	/**
	 * Track number of queue entries
	 */
	nvgpu_atomic_t put;
	/**
	 * Thread for worker
	 */
	struct nvgpu_thread poll_task;
	/**
	 * cond structure for waiting/waking worker threads
	 */
	struct nvgpu_cond wq;
	/**
	 * List of work items
	 */
	struct nvgpu_list_node items;
	/**
	 * Lock for access to the work \a items list
	 */
	struct nvgpu_spinlock items_lock;
	/**
	 * Mutex for controlled starting of the worker thread
	 */
	struct nvgpu_mutex start_lock;
	/**
	 * Worker ops functions
	 */
	const struct nvgpu_worker_ops *ops;
};

/**
 * @brief Generic check if the worker should stop because the thread stopped
 * running.
 *
 * This function returns true if the running status of the underlying worker
 * thread is set to zero. This indicates that the worker thread is no longer
 * running. Invokes the function #nvgpu_thread_should_stop() with variable
 * \a poll_task in #nvgpu_worker struct passed as parameter.
 *
 * @param worker [in] The worker struct. Function does not perform any validation
 *		      of the parameter.
 *
 * @return Boolean value indicating if the thread should stop or not.
 *
 * @retval TRUE if the worker thread should stop.
 * @retval FALSE if the worker thread can continue running.
 */
bool nvgpu_worker_should_stop(struct nvgpu_worker *worker);

/**
 * @brief Append a work item to the worker's list.
 *
 * This adds work item to the end of the list and wakes the worker
 * up immediately. If the work item already existed in the list, it's not added,
 * because in that case it has been scheduled already but has not yet been
 * processed.
 * - Checks if the thread associated with \a poll_task in #nvgpu_worker is
 *   already started. If not, try to start the thread. Return -1 if the thread
 *   creation fails.
 * - Function #nvgpu_spinlock_acquire() is called with variable \a
 *   items_lock in #nvgpu_worker as parameter to acquire the lock before
 *   trying to add the work item.
 * - #nvgpu_list_empty is called with \a work_item as parameter to check if the
 *   item is already added or not.
 * - If the item is already added, invoke the function #nvgpu_spinlock_release
 *   with variable \a items_lock in #nvgpu_worker as parameter to release the
 *   lock, and return -1.
 * - Invokes #nvgpu_list_add_tail with \a work_item and \a items in
 *   #nvgpu_worker as parameters to add the work item, if it is not already
 *   present in the list.
 * - Function #nvgpu_spinlock_release() is invoked with variable \a items_lock
 *   in #nvgpu_worker as parameter to release the lock acquired.
 * The worker thread is woken up after the addition of the work item.
 *
 * @param worker [in] The worker. Function does not perform any validation
 *		      of the parameter.
 * @param worker [in] The work item for the worker to work on. Function does
 *		      not perform any validation of the parameter.
 *
 * @return Integer value indicating the status of enqueue operation.
 *
 * @retval 0 on success.
 * @retval -1 on failure.
 */
int nvgpu_worker_enqueue(struct nvgpu_worker *worker,
		struct nvgpu_list_node *work_item);

/**
 * @brief This API is used to initialize the worker with a name that's a
 * conjunction of the two parameters: {worker_name}_{gpu_name}.
 *
 * Uses the library function \a strncat to populate the variable \a thread_name
 * in #nvgpu_worker. The input parameters \a worker_name and \a gpu_name are
 * used as parameters to \a strncat in multiple stages to populate the name of
 * the worker thread.
 *
 * @param worker [in] The worker. Function does not perform any
 *		      validation of the parameter.
 * @param worker_name [in] The name of the worker. Function does not perform
 *			   any validation of the parameter.
 * @param gpu_name [in] The name of the GPU. Function does not perform any
 *			validation of the parameter.
 */
void nvgpu_worker_init_name(struct nvgpu_worker *worker,
		const char* worker_name, const char *gpu_name);

/**
 * @brief Initialize the worker's metadata and start the background thread.
 *
 * - Invokes the function #nvgpu_atomic_set with variable \a put in
 *   #nvgpu_worker and 0 as parameters to initialize the atomic variable.
 * - Invokes the function #nvgpu_cond_init() with variable \a wq in
 *   #nvgpu_worker as parameter to initialize the condition variable.
 * - Invokes the function #nvgpu_init_list_node with variable \a items in
 *   #nvgpu_worker as parameter to initialize the list.
 * - Invokes the function #nvgpu_spinlock_init() with variable \a items_lock in
 *   #nvgpu_worker as parameter to initialize the spin lock.
 * - Invokes the function #nvgpu_mutex_init() with variable \a start_lock in
 *   #nvgpu_worker as parameter to initialize the mutex variable.
 * - Starts the thread associated with the \a worker. #nvgpu_thread_create()
 *   is the function used internally to create the worker thread, the
 *   parameters passed are \a poll_task in #nvgpu_worker, \a worker, a static
 *   callback function associated with the thread and \a thread_name in
 *   #nvgpu_worker. Any error value from the function #nvgpu_thread_create()
 *   is returned by this function as it is.
 *  On a successful completion of this function, 0 is returned.
 *
 * @param g [in] The GPU super structure. Function does not perform any
 *		 validation of the parameter.
 * @param worker [in] The worker. Function does not perform any validation of
 *		      the parameter.
 * @param worker_ops [in] The worker ops specific for this worker. Function
 *			  does not perform any validation of the parameter.
 *
 * @return 0 for success, < 0 for error. This function internally invokes
 * the thread creation API for worker thread. The error codes generated by the
 * thread creation API will be returned by this function.
 *
 * @retval 0 on success.
 * @retval EINVAL invalid thread attribute object.
 * @retval EAGAIN insufficient system resources to create thread.
 * @retval EFAULT error occurred trying to access the buffers or the
 * start routine provided for thread creation.
 */
int nvgpu_worker_init(struct gk20a *g, struct nvgpu_worker *worker,
		const struct nvgpu_worker_ops *worker_ops);

/**
 * @brief Stop the background thread associated with the worker.
 *
 * - Invokes the function #nvgpu_mutex_acquire() with variable \a start_lock in
 *   #nvgpu_worker as parameter to acquire the lock.
 * - Invokes the function #nvgpu_thread_stop() with variable \a poll_task in
 *   #nvgpu_worker as parameter to stop the poll task associated with the
 *   worker.
 * - Invokes the function #nvgpu_mutex_release() with variable \a start_lock in
 *   #nvgpu_worker as parameter to release the lock.
 *
 * @param worker [in] The worker. Function does not perform any validation of
 *		      the parameter.
 */
void nvgpu_worker_deinit(struct nvgpu_worker *worker);

#endif /* NVGPU_WORKER_H */
