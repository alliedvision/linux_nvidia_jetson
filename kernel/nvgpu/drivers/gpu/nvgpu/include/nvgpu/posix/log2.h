/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_POSIX_LOG2_H
#define NVGPU_POSIX_LOG2_H

#include <nvgpu/bitops.h>

/**
 * @brief Integer logarithm for base 2.
 *
 * Calculates the log to the base 2 of input value \a x and returns the
 * integer value of the same.
 * Macro does not perform any validation of the parameter.
 *
 * @param x [in]	The number to get the log for.
 *
 * @return Integer value of log to the base 2 of input \a x.
 */
#define nvgpu_ilog2(x)	({						\
				unsigned long fls_val =	nvgpu_fls(x);	\
									\
				nvgpu_assert(fls_val > 0ULL);		\
				fls_val = fls_val - 1U;			\
				fls_val;				\
			})

/**
 * @brief Round up to the power of two.
 *
 * Rounds up the input number \a x to power of two and returns.
 *
 * @param x [in]	Number to round up.
 * 			  - 0 is not a valid value.
 *
 * @return Input value \a x rounded up to the power of two.
 */
#define roundup_pow_of_two(x)						\
			({						\
				unsigned long ret = 0U;			\
									\
				if ((x) == 0UL) {			\
					BUG();				\
				} else {				\
					ret = 1UL <<			\
						nvgpu_fls((x) - 1UL);	\
				}					\
				ret;					\
			})

/**
 * @brief Round down to the power of two.
 *
 * Rounds down the input number \a x to power of two and returns.
 *
 * @param x [in]	Number to round down.
 * 			  - 0 is not a valid value.
 *
 * @return Input value \a x rounded down to the power of two.
 */
#define rounddown_pow_of_two(x)						\
			({						\
				unsigned long ret;			\
									\
				if ((x) == 0UL) {			\
					BUG();				\
				} else {				\
					ret = 1UL <<			\
						(nvgpu_fls((x)) - 1UL);	\
				}					\
				ret;					\
			})

/**
 * @brief Check for power of 2.
 *
 * Checks if the input value \a x is a power of two or not.
 * Macro does not perform any validation of the parameter.
 *
 * @param x [in]     Number to check.
 *
 * @return True if the input \a x is a power of two, else returns false.
 */
#define is_power_of_2(x)						\
	(bool)({							\
		typeof(x) __x__ = (x);					\
		((__x__ != 0U) && ((__x__ & (__x__ - 1U)) == 0U));	\
	})
#endif /* NVGPU_POSIX_LOG2_H */
