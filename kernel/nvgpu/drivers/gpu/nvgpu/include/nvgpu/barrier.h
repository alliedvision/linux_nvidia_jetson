/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

/* This file contains NVGPU_* high-level abstractions for various
 * memor-barrier operations available in linux/kernel. Every OS
 * should provide their own OS specific calls under this common API
 */

#ifndef NVGPU_BARRIER_H
#define NVGPU_BARRIER_H

#ifdef __KERNEL__
#include <nvgpu/linux/barrier.h>
#elif defined(__NVGPU_POSIX__)
#include <nvgpu/posix/barrier.h>
#else
#include <nvgpu_rmos/include/barrier.h>
#endif

#define nvgpu_mb()	nvgpu_mb_impl()
#define nvgpu_rmb()	nvgpu_rmb_impl()
#define nvgpu_wmb()	nvgpu_wmb_impl()

#define nvgpu_smp_mb()	nvgpu_smp_mb_impl()
#define nvgpu_smp_rmb()	nvgpu_smp_rmb_impl()
#define nvgpu_smp_wmb()	nvgpu_smp_wmb_impl()

/**
 * @brief Compilers can do optimizations assuming there is a single thread
 * executing the code. For example, a variable read in a loop from one thread
 * may not see the update from another thread because compiler has assumed that
 * it's value cannot change from the one initialized before the loop. There are
 * other possibilities like multiple references to a variable when the code
 * assumes that it should see a constant value. In general, this macro should
 * never be used by nvgpu driver code, and many of the current uses in driver
 * are likely wrong.
 * For more info see: URL = lwn.net/Articles/508991/
 *
 * @param x [in] variable that needs to turn into a volatile type temporarily.
 */
#define NV_READ_ONCE(x)		(*((volatile typeof(x) *)&x))

/**
 * @brief Ensure ordered writes.
 *
 * @param x    Variable to be updated.
 * @param y    Value to be written.
 *
 * Prevent compiler optimizations from mangling writes.
 */
#define NV_WRITE_ONCE(x, y)	(*((volatile typeof(x) *)(&(x))) = (y))

/*
 * Sometimes we want to prevent speculation.
 */
#ifdef __NVGPU_PREVENT_UNTRUSTED_SPECULATION
#define nvgpu_speculation_barrier()	nvgpu_speculation_barrier_impl()
#else
#define nvgpu_speculation_barrier()
#endif

#endif /* NVGPU_BARRIER_H */
