/*
 *
 * Nvgpu Channel Synchronization Abstraction (Semaphore)
 *
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_CHANNEL_SYNC_SEMAPHORE_H
#define NVGPU_CHANNEL_SYNC_SEMAPHORE_H

#include <nvgpu/types.h>
#include <nvgpu/channel_sync.h>

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT

struct nvgpu_channel;
struct nvgpu_channel_sync_semaphore;

/*
 * Converts a valid struct nvgpu_channel_sync ptr to
 * struct nvgpu_channel_sync_semaphore ptr else return NULL.
 */
struct nvgpu_channel_sync_semaphore *
nvgpu_channel_sync_to_semaphore(struct nvgpu_channel_sync *sync);

/*
 * Returns the underlying hw semaphore.
 */
struct nvgpu_hw_semaphore *
nvgpu_channel_sync_semaphore_hw_sema(
		struct nvgpu_channel_sync_semaphore *sema);

/*
 * Constructs an instance of struct nvgpu_channel_sync_semaphore and
 * returns a pointer to the struct nvgpu_channel_sync associated with it.
 */
struct nvgpu_channel_sync *
nvgpu_channel_sync_semaphore_create(struct nvgpu_channel *c);

#endif

#endif /* NVGPU_CHANNEL_SYNC_SEMAPHORE_H */
