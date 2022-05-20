/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_BITOPS_H
#define NVGPU_BITOPS_H

#include <nvgpu/types.h>
#include <nvgpu/bug.h>
#include <nvgpu/utils.h>

/*
 * Explicit sizes for bit definitions. Please use these instead of BIT().
 */
#define BIT8(i)		(U8(1) << U8(i))
#define BIT16(i)	(U16(1) << U16(i))
#define BIT32(i)	(U32(1) << U32(i))
#define BIT64(i)	(U64(1) << U64(i))

#ifdef __KERNEL__
#include <nvgpu/linux/bitops.h>
#else
#include <nvgpu/posix/bitops.h>
#endif

/*
 * BITS_PER_BYTE is U64 data type.
 * Casting U64 to U32 results in certc_violation.
 * To avoid violation, define BITS_PER_BYTE_U32 as U32 data type
 */
#define BITS_PER_BYTE_U32	8U

#endif /* NVGPU_BITOPS_H */
