/* SPDX-License-Identifier: MIT
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

#ifndef __LOG2_LINUX_H__
#define __LOG2_LINUX_H__

#include <linux/log2.h>

/**
 * @brief Integer logarithm for base 2.
 *
 * Calculates the log to the base 2 of input value \a x and returns the
 * integer value of the same.
 * Macro performs validation of the parameter.
 *
 * @param x [in]	The number to get the log for.
 *
 * @return Integer value of log to the base 2 of input \a x.
 */

#define nvgpu_ilog2(x)	({	\
				unsigned long result;	\
				nvgpu_assert(x > 0ULL);	\
				result = nvgpu_safe_cast_s32_to_u64(ilog2(x));	\
				result;				\
			})
#endif