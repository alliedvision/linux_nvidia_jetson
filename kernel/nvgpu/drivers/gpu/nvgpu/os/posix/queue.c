/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/posix/queue.h>
#include <nvgpu/kmem.h>
#include <nvgpu/types.h>
#include <nvgpu/barrier.h>
#include <nvgpu/log2.h>
#include <nvgpu/static_analysis.h>
#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
#include <nvgpu/posix/posix-fault-injection.h>
#endif

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
struct nvgpu_posix_fault_inj *nvgpu_queue_out_get_fault_injection(void)
{
	struct nvgpu_posix_fault_inj_container *c =
			nvgpu_posix_fault_injection_get_container();

	return &c->queue_out_fi;
}
#endif

unsigned int nvgpu_queue_available(struct nvgpu_queue *queue)
{
	u32 ret;

	if (queue->in >= queue->out) {
		ret = queue->in - queue->out;
	} else {
		ret = (UINT32_MAX - queue->out) + (queue->in + 1U);
	}

	return ret;
}

static unsigned int nvgpu_queue_unused(struct nvgpu_queue *queue)
{
	return nvgpu_safe_sub_u32(nvgpu_safe_add_u32(queue->mask, 1U),
			nvgpu_queue_available(queue));
}

int nvgpu_queue_alloc(struct nvgpu_queue *queue, unsigned int size)
{
	if (queue == NULL) {
		return -EINVAL;
	}

	if ((size == 0U) || (size > (unsigned int)INT32_MAX)){
		return -EINVAL;
	}
	/*
	 * size should alway be a power of 2, if not
	 * round to next higher power of 2.
	 */
	if ((size & (size - 1U)) != 0U) {
		size = nvgpu_safe_cast_u64_to_u32(roundup_pow_of_two(size));
	}

	queue->data = nvgpu_kzalloc(NULL, size);
	if (queue->data == NULL) {
		return -ENOMEM;
	}

	queue->in = 0;
	queue->out = 0;
	queue->mask = nvgpu_safe_sub_u32(size, 1U);

	return 0;
}

void nvgpu_queue_free(struct nvgpu_queue *queue)
{
	nvgpu_kfree(NULL, queue->data);
	queue->in = 0;
	queue->out = 0;
	queue->mask = 0;
}

static void posix_queue_copy_in(struct nvgpu_queue *queue, const void *src,
		unsigned int len, unsigned int off)
{
	unsigned int size = queue->mask + 1U;
	unsigned int l;
	void *tdata;

	off &= queue->mask;
	tdata = (void *)(queue->data + off);

	l = min(nvgpu_safe_sub_u32(size, off), len);
	len = nvgpu_safe_sub_u32(len, l);

	(void)memcpy(tdata, src, l);
	(void)memcpy((void *)queue->data,
			(const void *)((unsigned const char *)src + l), len);
}

static int posix_queue_in_common(struct nvgpu_queue *queue, const void *src,
		unsigned int len, struct nvgpu_mutex *lock)
{
	unsigned int l;

	if (lock != NULL) {
		nvgpu_mutex_acquire(lock);
	}

	l = nvgpu_queue_unused(queue);

	if (len > l) {
		if (lock != NULL) {
			nvgpu_mutex_release(lock);
		}
		return -ENOMEM;
	}

	posix_queue_copy_in(queue, src, len, queue->in);
	/*
	 * make sure that data in queue is updated.
	 */
	nvgpu_smp_wmb();

	if ((UINT_MAX - queue->in) < len) {
		queue->in = (len - (UINT_MAX - queue->in)) - 1U;
	} else {
		queue->in = queue->in + len;
	}

	if (lock != NULL) {
		nvgpu_mutex_release(lock);
	}

	return 0;
}

#ifdef CONFIG_NVGPU_NON_FUSA
int nvgpu_queue_in(struct nvgpu_queue *queue, const void *buf,
		unsigned int len)
{
	return posix_queue_in_common(queue, buf, len, NULL);
}
#endif

int nvgpu_queue_in_locked(struct nvgpu_queue *queue, const void *buf,
		unsigned int len, struct nvgpu_mutex *lock)
{
	return posix_queue_in_common(queue, buf, len, lock);
}

static void posix_queue_copy_out(struct nvgpu_queue *queue, void *dst,
		unsigned int len, unsigned int off)
{
	unsigned int size = queue->mask + 1U;
	unsigned int l;
	void *fdata;

	off &= queue->mask;
	fdata = (void *)(queue->data + off);

	l = min(len, nvgpu_safe_sub_u32(size, off));
	len = nvgpu_safe_sub_u32(len, l);
	(void)memcpy(dst, fdata, l);
	(void)memcpy((void *)((unsigned char *)dst + l),
			(const void *)queue->data, len);
}

static int nvgpu_queue_out_common(struct nvgpu_queue *queue, void *buf,
		unsigned int len, struct nvgpu_mutex *lock)
{
	unsigned int l;

	if (lock != NULL) {
		nvgpu_mutex_acquire(lock);
	}

	l = nvgpu_queue_available(queue);
	if (l < len) {
		if (lock != NULL) {
			nvgpu_mutex_release(lock);
		}
		return -ENOMEM;
	}
	posix_queue_copy_out(queue, buf, len, queue->out);
	/*
	 * make sure data in destination buffer is updated.
	 */
	nvgpu_smp_wmb();

	if ((UINT_MAX - queue->out) < len) {
		queue->out = (len - (UINT_MAX - queue->out)) - 1U;
	} else {
		queue->out = queue->out + len;
	}

	if (lock != NULL) {
		nvgpu_mutex_release(lock);
	}

	return 0;
}

#ifdef CONFIG_NVGPU_NON_FUSA
int nvgpu_queue_out(struct nvgpu_queue *queue, void *buf,
		unsigned int len)
{
	return nvgpu_queue_out_common(queue, buf, len, NULL);
}
#endif

int nvgpu_queue_out_locked(struct nvgpu_queue *queue, void *buf,
		unsigned int len, struct nvgpu_mutex *lock)
{
#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
				nvgpu_queue_out_get_fault_injection())) {
		return -1;
	}
#endif

	return nvgpu_queue_out_common(queue, buf, len, lock);
}
