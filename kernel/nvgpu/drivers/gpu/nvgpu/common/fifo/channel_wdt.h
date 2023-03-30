/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_COMMON_FIFO_CHANNEL_WDT_H
#define NVGPU_COMMON_FIFO_CHANNEL_WDT_H

#include <nvgpu/types.h>

struct nvgpu_channel;

#ifdef CONFIG_NVGPU_CHANNEL_WDT
struct nvgpu_worker;

void nvgpu_channel_launch_wdt(struct nvgpu_channel *ch);
void nvgpu_channel_worker_poll_init(struct nvgpu_worker *worker);
void nvgpu_channel_worker_poll_wakeup_post_process_item(
		struct nvgpu_worker *worker);
u32 nvgpu_channel_worker_poll_wakeup_condition_get_timeout(
		struct nvgpu_worker *worker);
#else
static inline void nvgpu_channel_launch_wdt(struct nvgpu_channel *ch)
{
	(void)ch;
}
#endif /* CONFIG_NVGPU_CHANNEL_WDT */

#endif /* NVGPU_COMMON_FIFO_CHANNEL_WDT_H */
