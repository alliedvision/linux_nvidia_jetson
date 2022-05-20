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

#ifndef NVGPU_TRACE_H
#define NVGPU_TRACE_H

struct gk20a;

#ifdef __KERNEL__

#ifdef CONFIG_NVGPU_TRACE
#include <trace/events/gk20a.h>
#endif
void nvgpu_trace_intr_stall_start(struct gk20a *g);
void nvgpu_trace_intr_stall_done(struct gk20a *g);
void nvgpu_trace_intr_thread_stall_start(struct gk20a *g);
void nvgpu_trace_intr_thread_stall_done(struct gk20a *g);

#elif defined(__NVGPU_POSIX__)

#ifdef CONFIG_NVGPU_TRACE
#include <nvgpu/posix/trace_gk20a.h>
#endif /* CONFIG_NVGPU_TRACE */
static inline void nvgpu_trace_intr_stall_start(struct gk20a *g) {}
static inline void nvgpu_trace_intr_stall_done(struct gk20a *g) {}
static inline void nvgpu_trace_intr_thread_stall_start(struct gk20a *g) {}
static inline void nvgpu_trace_intr_thread_stall_done(struct gk20a *g) {}

#else

#ifdef CONFIG_NVGPU_TRACE
#include <nvgpu/posix/trace_gk20a.h>
#endif
static inline void nvgpu_trace_intr_stall_start(struct gk20a *g) {}
static inline void nvgpu_trace_intr_stall_done(struct gk20a *g) {}
void nvgpu_trace_intr_thread_stall_start(struct gk20a *g);
void nvgpu_trace_intr_thread_stall_done(struct gk20a *g);

#endif /* __KERNEL__ */

#endif /* NVGPU_TRACE_H */
