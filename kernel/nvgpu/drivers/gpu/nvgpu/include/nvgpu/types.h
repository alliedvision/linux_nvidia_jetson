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
#ifndef NVGPU_TYPES_H
#define NVGPU_TYPES_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/limits.h>
#else
#include <nvgpu/posix/types.h>
#endif

#include <nvgpu/cov_whitelist.h>

/*
 * These macros exist to make integer literals used in certain arithmetic
 * operations explicitly large enough to hold the results of that operation.
 * The following is an example of this.
 *
 * In MISRA the destination for a bitwise shift must be able to hold the number
 * of bits shifted. Otherwise the results are undefined. For example:
 *
 *   256U << 20U
 *
 * This is valid C code but the results of this _may_ be undefined if the size
 * of an unsigned by default is less than 24 bits (i.e 16 bits). The MISRA
 * checker sees the 256U and determines that the 256U fits in a 16 bit data type
 * (i.e a u16). Since a u16 has 16 bits, which is less than 20, this is an
 * issue.
 *
 * Of course most compilers these days use 32 bits for the default unsigned type
 * this is not a requirement. Moreover this same problem could exist like so:
 *
 *   0xfffffU << 40U
 *
 * The 0xfffffU is a 32 bit unsigned type; but we are shifting 40 bits which
 * overflows the 32 bit data type. So in this case we need an explicit cast to
 * 64 bits in order to prevent undefined behavior.
 */

/**
 * Type cast \a x to unsigned char.
 */
#define U8(x)	((u8)(x))

/**
 * Type cast \a x to unsigned short.
 */
#define U16(x)	((u16)(x))

/**
 * Type cast \a x to unsigned int.
 */
#define U32(x)	((u32)(x))

/**
 * Type cast \a x to unsigned long long.
 */
#define U64(x)	((u64)(x))

/**
 * Type cast \a x to signed char.
 */
#define S8(x)	((s8)(x))

/**
 * Type cast \a x to signed short.
 */
#define S16(x)	((s16)(x))

/**
 * Type cast \a x to signed int.
 */
#define S32(x)	((s32)(x))

/**
 * Type cast \a x to signed long long.
 */
#define S64(x)	((s64)(x))

/* Linux uses U8_MAX, U32_MAX, etc instead of UCHAR_MAX, UINT32_MAX. We define
 * them here for non-Linux OSes
 */
#if !defined(__KERNEL__) && !defined(U8_MAX)
/**
 * Maximum value for unsigned 8 bit.
 */
#define U8_MAX		U8(0xff)

/**
 * Maximum value for unsigned 16 bit.
 */
#define U16_MAX		U16(0xffff)

/**
 * Maximum value for unsigned 32 bit.
 */
#define U32_MAX		U32(~U32(0))

/**
 * Maximum value for unsigned 64 bit.
 */
#define U64_MAX		U64(~U64(0))
#endif

NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 2, NVGPU_MISRA(Rule, 21_1), "Bug 2799838")
#if defined(__KERNEL__) && !defined(UCHAR_MAX)
/* Linux doesn't define these max values, and we can't use limits.h */

/**
 * Maximum value for unsigned char.
 */
#define UCHAR_MAX U8_MAX

/**
 * Maximum value for signed char.
 */
#define SCHAR_MAX (U8_MAX/2)

/**
 * Minimum value for signed char.
 */
#define SCHAR_MIN (-SCHAR_MAX - 1)
#endif
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 21_1))

#endif /* NVGPU_TYPES_H */
