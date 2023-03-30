/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/timers.h>

void nvgpu_timeout_init_cpu_timer(struct gk20a *g, struct nvgpu_timeout *timeout,
		       u32 duration_ms)
{
	int err = nvgpu_timeout_init_flags(g, timeout, duration_ms,
					   NVGPU_TIMER_CPU_TIMER);

	nvgpu_assert(err == 0);
}

void nvgpu_timeout_init_cpu_timer_sw(struct gk20a *g, struct nvgpu_timeout *timeout,
		       u32 duration_ms)
{
	int err = nvgpu_timeout_init_flags(g, timeout, duration_ms,
					   NVGPU_TIMER_CPU_TIMER |
					   NVGPU_TIMER_NO_PRE_SI);

	nvgpu_assert(err == 0);
}

void nvgpu_timeout_init_retry(struct gk20a *g, struct nvgpu_timeout *timeout,
		       u32 duration_count)
{
	int err = nvgpu_timeout_init_flags(g, timeout, duration_count,
					   NVGPU_TIMER_RETRY_TIMER);

	nvgpu_assert(err == 0);
}

