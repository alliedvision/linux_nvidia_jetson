/*
 * Copyright (c) 2021 NVIDIA Corporation.  All rights reserved.
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

#ifndef NVGPU_NVS_IMPL_H
#define NVGPU_NVS_IMPL_H

#include <nvgpu/kmem.h>
#include <nvgpu/string.h>
#include <nvgpu/timers.h>
#include <nvgpu/log.h>

#define nvs_malloc(sched, size)					\
	nvgpu_kmalloc((struct gk20a *)(sched)->priv, (size))

#define nvs_free(sched, ptr)					\
	nvgpu_kfree((struct gk20a *)(sched)->priv, (ptr))

#define nvs_memset(ptr, value, length)				\
	memset((ptr), (value), (length))

#define nvs_timestamp()						\
	nvgpu_current_time_ns()

#define nvs_log(sched, fmt, args...)				\
	nvgpu_log((struct gk20a *)(sched)->priv,		\
		  gpu_dbg_nvs_internal, (fmt), ##args)


#endif
