/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/semaphore.h>
#include <nvgpu/cond.h>
#include <nvgpu/fence.h>
#include <nvgpu/fence_sema.h>
#include "fence_priv.h"

static int nvgpu_fence_semaphore_wait(struct nvgpu_fence_type *f, u32 timeout)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	if (!nvgpu_semaphore_is_acquired(pf->semaphore)) {
		return 0;
	}

	return NVGPU_COND_WAIT_INTERRUPTIBLE(
		pf->semaphore_wq,
		!nvgpu_semaphore_is_acquired(pf->semaphore),
		timeout);
}

static bool nvgpu_fence_semaphore_is_expired(struct nvgpu_fence_type *f)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	return !nvgpu_semaphore_is_acquired(pf->semaphore);
}

static void nvgpu_fence_semaphore_release(struct nvgpu_fence_type *f)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	if (pf->semaphore != NULL) {
		nvgpu_semaphore_put(pf->semaphore);
	}
}

static const struct nvgpu_fence_ops nvgpu_fence_semaphore_ops = {
	.wait = nvgpu_fence_semaphore_wait,
	.is_expired = nvgpu_fence_semaphore_is_expired,
	.release = nvgpu_fence_semaphore_release,
};

/* This function takes ownership of the semaphore as well as the os_fence */
void nvgpu_fence_from_semaphore(
		struct nvgpu_fence_type *f,
		struct nvgpu_semaphore *semaphore,
		struct nvgpu_cond *semaphore_wq,
		struct nvgpu_os_fence os_fence)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	nvgpu_fence_init(f, &nvgpu_fence_semaphore_ops, os_fence);

	pf->semaphore = semaphore;
	pf->semaphore_wq = semaphore_wq;
}
