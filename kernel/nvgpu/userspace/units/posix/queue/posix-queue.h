/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

/**
 * @addtogroup SWUTS-posix-queue
 * @{
 *
 * Software Unit Test Specification for posix-queue
 */

#ifndef __UNIT_POSIX_QUEUE_H__
#define __UNIT_POSIX_QUEUE_H__

/**
 * Test specification for: test_nvgpu_queue_alloc_and_free
 *
 * Description: Functionalities of posix queue such as allocating and freeing
 * of the message queue are tested.
 *
 * Test Type: Feature, Error guessing, Boundary values
 *
 * Targets: nvgpu_queue_alloc, nvgpu_queue_free
 *
 * Input: None
 *
 * Steps:
 * - Pass NULL nvgpu_queue pointer as argument to nvgpu_queue_alloc() API and
 *   check that the API returns -EINVAL error.
 * - Pass zero size queue length as argument to nvgpu_queue_alloc() API and
 *   check that the API returns -EINVAL error.
 * - Pass INT64_MAX size queue length as argument to nvgpu_queue_alloc() API
 *   and check that the API returns -EINVAL error.
 * - Inject fault so that immediate call to nvgpu_kzalloc() API would fail.
 * - Check that when the nvgpu_queue_alloc() API is called with valid arguments,
 *   it would fail by returning -ENOMEM error.
 * - Remove the injected fault in nvgpu_kzalloc() API.
 * - Pass below valid arguments to nvgpu_queue_alloc() API and check that the
 *   API returns success.
 *   - Valid pointer to struct nvgpu_queue
 *   - Queue size which is not power of 2
 * - Free the allocated queue by caling nvgpu_queue_free() API.
 * - Pass below valid arguments to nvgpu_queue_alloc() API and check that the
 *   API returns success.
 *   - Valid pointer to struct nvgpu_queue
 *   - Queue size which is power of 2
 * - Free the allocated queue by caling nvgpu_queue_free() API.
 * - Pass below valid arguments to nvgpu_queue_alloc() API and check that the
 *   API returns success.
 *   - Valid pointer to struct nvgpu_queue
 *   - Queue size equal to INT32_MAX
 * - Free the allocated queue by caling nvgpu_queue_free() API.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_queue_alloc_and_free(struct unit_module *m, struct gk20a *g,
				void *args);

/**
 * Test specification for: test_nvgpu_queue_in
 *
 * Description: Functionalities of posix queue such as allocating queue and
 * enqueueing messages into the queue are tested.
 *
 * Test Type: Feature, Error guessing, Boundary values
 *
 * Targets: nvgpu_queue_alloc, nvgpu_queue_in, nvgpu_queue_in_locked,
 * nvgpu_queue_unused, nvgpu_queue_available
 *
 * Input: None
 *
 * Steps:
 * - Pass below valid arguments to nvgpu_queue_alloc() API and check that the
 *   API returns success.
 *   - Valid pointer to struct nvgpu_queue
 *   - Queue size which is power of 2 and less than INT_MAX, exact value used
 *     is 16.
 * - Enqueue message of length BUF_LEN calling nvgpu_queue_in() API and check
 *   that the API returns 0.
 * - Update In and Out indexes and enqueue message of length BUF_LEN such
 *   that we wrap around the Queue while enqueuing the message using
 *   nvgpu_queue_in() API. Check that the API returns 0.
 * - Reset In and Out indexes and enqueue message of length BUF_LEN with
 *   the lock using nvgpu_queue_in_locked() API. Check that the API returns 0.
 * - Enqueue message of length BUF_LEN again using nvgpu_queue_in_locked()
 *   API. Check that the API returns error -ENOMEM.
 * - Set In and Out index to UINT32_MAX - BUf_LEN/2, which indicates that the
 *   queue is empty and try to enqueue BUF_LEN size message. This should cause
 *   a wrap around of In index, but the API should be able to handle it and
 *   return 0 to indicate successful enqueue operation.
 * - Enqueue message of length BUF_LEN again using nvgpu_queue_in() API. Check
 *   that the API returns error -ENOMEM.
 * - Reset In and Out indexes and enqueue message of length BUF_LEN using
 *   nvgpu_queue_in_locked() API with lock parameter passed as NULL. This test
 *   is to increase the code coverage. Check that the API returns 0.
 * - Enqueue message of length BUF_LEN again using nvgpu_queue_in_locked()
 *   API with lock parameter as NULL. Check that the API returns error -ENOMEM.
 * - Free the allocated resources.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_queue_in(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_queue_out
 *
 * Description: Functionalities of posix queue such as allocating queue and
 * dequeuing messages from the queue are tested.
 *
 * Test Type: Feature, Error guessing, Boundary values
 *
 * Targets: nvgpu_queue_alloc, nvgpu_queue_out, nvgpu_queue_out_locked
 *
 * Input: None
 *
 * Steps:
 * - Pass below valid arguments to nvgpu_queue_alloc() API and check that the
 *   API returns success.
 *   - Valid pointer to struct nvgpu_queue
 *   - Queue size which is power of 2 and less than INT_MAX. Exact value used
 *     is 16.
 * - Dequeue message of length BUF_LEN from the empty queue calling
 *   nvgpu_queue_out() API and check that the API returns -ENOMEM error.
 * - Dequeue message of length BUF_LEN from the empty queue calling
 *   nvgpu_queue_out_locked() API and check that the API returns -ENOMEM
 *   error.
 * - Dequeue message of length BUF_LEN from the empty queue calling
 *   nvgpu_queue_out_locked() API with lock parameter passed as NULL and check
 *   that the API returns -ENOMEM error. This is for code coverage.
 * - Set In index as BUF_LEN and dequeue message of length BUF_LEN by
 *   calling nvgpu_queue_out() API and check that the API returns 0.
 * - Set In index as BUF_LEN and dequeue message of length BUF_LEN by
 *   calling nvgpu_queue_out_locked() API and check that the API returns 0.
 * - Set In index as BUF_LEN and dequeue message of length BUF_LEN by
 *   calling nvgpu_queue_out_locked() API with lock parameter passed as NULL
 *   and check that the API returns 0. This is for code coverage.
 * - Set In index as 0 and Out index as (UINT32_MAX - BUF_LEN). This
 *   indicates a condition were In index has wrapped around due to an enqueue
 *   operation. Use nvgpu_queue_out API to dequeue message of length BUF_LEN.
 *   The dequeue operation should successfully return 0.
 * - Repeat the above step to test API nvgpu_queue_out_locked.
 * - Set In index as (BUF_LEN/2 - 1) and Out index as (UINT32_MAX - BUF_LEN/2).
 *   This indicates a condition were In index has wrapped around due to an
 *   enqueue operation. Use nvgpu_queue_out_locked API to dequeue message of
 *   length BUF_LEN. This will cover the wrap around condition for Out index.
 *   The dequeue operation should successfully return 0.
 * - Do fault injection so that immediate call to nvgpu_queue_out_locked() API
 *   would return error.
 * - Invoke nvgpu_queue_out_locked() API and check that API returns -1 error.
 * - Remove the injected fault.
 * - Free the allocated resources.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_queue_out(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_queue_available
 *
 * Description: Test the functionality of the function which returns the
 * available data in the queue.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_queue_available
 *
 * Input: None
 *
 * Steps:
 * - The following combinations of Out and In index values are provided to the
 *   public API,
 * - Out and In are populated with same value. Expected return value is 0.
 * - Out is populated with a value less than In. The difference is less than
 *   the size allocated for the queue. Expected return value is the difference
 *   between In and Out values indicating the number of bytes of data present
 *   in the queue.
 * - Out is populated with a value greater than In. This scenario can happen
 *   when In index is wrapped around explicitly. The API should handle this
 *   scenario and return the valid number of bytes present in the queue. Out
 *   and In value are selected so as the size of the queue is not violated.
 *
 * Output: Returns PASS if the steps above returns expected values, FAIL
 * otherwise.
 */
int test_nvgpu_queue_available(struct unit_module *m, struct gk20a *g,
							   void *args);

#endif /* __UNIT_POSIX_QUEUE_H__ */
/*
 * @}
 */
