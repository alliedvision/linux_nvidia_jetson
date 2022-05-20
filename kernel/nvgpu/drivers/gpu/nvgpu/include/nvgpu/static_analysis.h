/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_STATIC_ANALYSIS_H
#define NVGPU_STATIC_ANALYSIS_H

/**
 * @file
 *
 * Macros/functions/etc for static analysis of nvgpu code.
 */

#include <nvgpu/types.h>

#include <nvgpu/cov_whitelist.h>
/*
 * bug.h needs the whitelist macros, so wait to include it until after those
 * are defined.
 */
#include <nvgpu/bug.h>

/**
 * @addtogroup unit-common-nvgpu
 * @{
 */

static inline u32 nvgpu_safe_cast_s32_to_u32(s32 si_a);
static inline u64 nvgpu_safe_cast_s32_to_u64(s32 si_a);
static inline u64 nvgpu_safe_cast_s64_to_u64(s64 l_a);
static inline s64 nvgpu_safe_cast_u64_to_s64(u64 ul_a);

/**
 * @brief Add two u32 values and check for overflow.
 *
 * @param ui_a [in]	Addend value for adding.
 * @param ui_b [in]	Addend value for adding.
 *
 * Adds the two u32 values unless the result will overflow a u32. If the result
 * would overflow a u32, calls BUG().
 *
 * @return If no overflow, sum of the two integers (\a ui_a + \a ui_b).
 */
static inline u32 nvgpu_safe_add_u32(u32 ui_a, u32 ui_b)
{
	if ((UINT_MAX - ui_a) < ui_b) {
		BUG();
	} else {
		return ui_a + ui_b;
	}
}

/**
 * @brief Add two s32 values and check for overflow.
 *
 * @param si_a [in]	Addend value for adding.
 * @param si_b [in]	Addend value for adding.
 *
 * Adds the two s32 values unless the result will overflow a s32. If the result
 * would overflow a s32, calls BUG().
 *
 * @return If no overflow, sum of the two integers (\a si_a + \a si_b).
 */
static inline s32 nvgpu_safe_add_s32(s32 si_a, s32 si_b)
{
	if (((si_b > 0) && (si_a > (INT_MAX - si_b))) ||
		((si_b < 0) && (si_a < (INT_MIN - si_b)))) {
		BUG();
	} else {
		return si_a + si_b;
	}
}

/**
 * @brief Add two u64 values and check for overflow.
 *
 * @param ul_a [in]	Addend value for adding.
 * @param ul_b [in]	Addend value for adding.
 *
 * Adds the two u64 values unless the result will overflow a u64. If the result
 * would overflow a u64, calls BUG().
 *
 * @return If no overflow, sum of the two integers (\a ul_a + \a ul_b).
 */
static inline u64 nvgpu_safe_add_u64(u64 ul_a, u64 ul_b)
{
NVGPU_COV_WHITELIST(false_positive, NVGPU_CERT(INT30_C), "Bug 2643092")
	if ((ULONG_MAX - ul_a) < ul_b) {
		BUG();
	} else {
		return ul_a + ul_b;
	}
}

/**
 * @brief Add two s64 values and check for overflow.
 *
 * @param sl_a [in]	Addend value for adding.
 * @param sl_b [in]	Addend value for adding.
 *
 * Adds the two s64 values unless the result will overflow a s64. If the result
 * would overflow a s64, calls BUG().
 *
 * @return If no overflow, sum of the two integers (\a sl_a + \a sl_b).
 */
static inline s64 nvgpu_safe_add_s64(s64 sl_a, s64 sl_b)
{
	if (((sl_b > 0) && (sl_a > (LONG_MAX - sl_b))) ||
		((sl_b < 0) && (sl_a < (LONG_MIN - sl_b)))) {
		BUG();
	} else {
		return sl_a + sl_b;
	}
}

/**
 * @brief Add two u32 values with wraparound arithmetic
 *
 * @param ui_a [in]	Addend value for adding.
 * @param ui_b [in]	Addend value for adding.
 *
 * Adds the two u32 values together. If the result would overflow an u32, wrap
 * in modulo arithmetic as defined in the C standard (value mod (U32_MAX + 1)).
 *
 * @return wrapping add of (\a ui_a + \a ui_b).
 */
static inline u32 nvgpu_wrapping_add_u32(u32 ui_a, u32 ui_b)
{
	/* INT30-C */
	u64 ul_a = (u64)ui_a;
	u64 ul_b = (u64)ui_b;
	u64 sum = (ul_a + ul_b) & 0xffffffffULL;

	/* satisfy Coverity's CERT INT31-C checker */
	nvgpu_assert(sum <= U32_MAX);

	return (u32)sum;
}

/**
 * @brief Subtract two u32 values with wraparound arithmetic
 *
 * @param ui_a [in]	Value of minuend.
 * @param ui_b [in]	Value of subtrahend.
 *
 * Subtracts \a ui_b from \a ui_a. If the result would underflow an u32, wrap
 * in modulo arithmetic as defined in the C standard (value mod (U32_MAX + 1)).
 *
 * @return wrapping sub of (\a ui_a - \a ui_b).
 */
static inline u32 nvgpu_wrapping_sub_u32(u32 ui_a, u32 ui_b)
{
	/* INT30-C */
	u64 ul_a = (u64)ui_a;
	u64 ul_b = (u64)ui_b;
	u64 diff = (ul_a + 0xffffffff00000000ULL - ul_b) & 0xffffffffULL;
	/*
	 * Note how the borrow bit will be taken from the least significant bit
	 * of the upper 32 bits; there is no way for an underflow to occur in
	 * 64 bits, and an underflow in the lower 32 bits will be handled as
	 * expected. For example, 0 - 1 becomes U32_MAX:
	 *   0xffff ffff 0000 0000
	 * - 0x                  1
	 *   0xffff fffe ffff ffff
	 *               ^^^^ ^^^^
	 *
	 * and 3 - (U32_MAX - 1) becomes 5:
	 *   0xffff ffff 0000 0003
	 * - 0x          ffff fffe
	 *   0xffff fffe 0000 0005
	 *               ^^^^ ^^^^
	 *
	 * If there is no underflow, this behaves as usual. 42 - 40:
	 *   0xffff ffff 0000 002a
	 * - 0x          0000 0028
	 *   0xffff ffff 0000 0002
	 *               ^^^^ ^^^^
	 */

	/* satisfy Coverity's CERT INT31-C checker */
	nvgpu_assert(diff <= U32_MAX);

	return (u32)diff;
}

/**
 * @brief Subtract two u8 values and check for underflow.
 *
 * @param uc_a [in]	Value of minuend.
 * @param uc_b [in]	Value of subtrahend.
 *
 * Subtracts \a uc_b from \a uc_a unless the result will underflow a u8. If the
 * result would underflow, calls BUG().
 *
 * @return If no overflow, difference of the two integers (\a uc_a - \a uc_b).
 */
static inline u8 nvgpu_safe_sub_u8(u8 uc_a, u8 uc_b)
{
	if (uc_a < uc_b) {
		BUG();
	} else {
		return uc_a - uc_b;
	}
}

/**
 * @brief Subtract two u32 values and check for underflow.
 *
 * @param ui_a [in]	Value of minuend.
 * @param ui_b [in]	Value of subtrahend.
 *
 * Subtracts \a ui_b from \a ui_a unless the result will underflow a u32. If the
 * result would underflow, calls BUG().
 *
 * @return If no overflow, difference of the two integers (\a ui_a - \a ui_b).
 */
static inline u32 nvgpu_safe_sub_u32(u32 ui_a, u32 ui_b)
{
	if (ui_a < ui_b) {
		BUG();
	} else {
		return ui_a - ui_b;
	}
}

/**
 * @brief Subtract two s32 values and check for underflow.
 *
 * @param si_a [in]	Value of minuend.
 * @param si_b [in]	Value of subtrahend.
 *
 * Subtracts \a si_b from \a si_a unless the result will underflow a s32. If the
 * result would underflow, calls BUG().
 *
 * @return If no overflow, difference of the two integers (\a si_a - \a si_b).
 */
static inline s32 nvgpu_safe_sub_s32(s32 si_a, s32 si_b)
{
	if (((si_b > 0) && (si_a < (INT_MIN + si_b))) ||
		((si_b < 0) && (si_a > (INT_MAX + si_b)))) {
		BUG();
	} else {
		return si_a - si_b;
	}
}

/**
 * @brief Subtract two u64 values and check for underflow.
 *
 * @param ul_a [in]	Value of minuend.
 * @param ul_b [in]	Value of subtrahend.
 *
 * Subtracts \a ul_b from \a ul_a unless the result will underflow a u64. If the
 * result would underflow, calls BUG().
 *
 * @return If no overflow, difference of the two integers (\a ul_a - \a ul_b).
 */
static inline u64 nvgpu_safe_sub_u64(u64 ul_a, u64 ul_b)
{
	if (ul_a < ul_b) {
		BUG();
	} else {
		return ul_a - ul_b;
	}
}

/**
 * @brief Subtract two s64 values and check for underflow.
 *
 * @param si_a [in]	Value of minuend.
 * @param si_b [in]	Value of subtrahend.
 *
 * Subtracts \a si_b from \a si_a unless the result will underflow a s64. If the
 * result would underflow, calls BUG().
 *
 * @return If no overflow, difference of the two integers (\a si_a - \a si_b).
 */
static inline s64 nvgpu_safe_sub_s64(s64 si_a, s64 si_b)
{
	if (((si_b > 0) && (si_a < (LONG_MIN + si_b))) ||
		((si_b < 0) && (si_a > (LONG_MAX + si_b)))) {
		BUG();
	} else {
		return si_a - si_b;
	}
}

/**
 * @brief Multiply two u32 values and check for overflow.
 *
 * @param ui_a [in]	Value of multiplicand.
 * @param ui_b [in]	Value of multiplier.
 *
 * Multiplies \a ui_a and \a ui_b unless the result will overflow a u32. If the
 * result would underflow, calls BUG().
 *
 * @return If no overflow, product of the two integers (\a ui_a * \a ui_b).
 */
static inline u32 nvgpu_safe_mult_u32(u32 ui_a, u32 ui_b)
{
	if ((ui_a == 0U) || (ui_b == 0U)) {
		return 0U;
	} else if (ui_a > (UINT_MAX / ui_b)) {
		BUG();
	} else {
		return ui_a * ui_b;
	}
}

/**
 * @brief Multiply two u64 values and check for overflow.
 *
 * @param ul_a [in]	Value of multiplicand.
 * @param ul_b [in]	Value of multiplier.
 *
 * Multiplies \a ul_a and \a ul_b unless the result will overflow a u64. If the
 * result would underflow, calls BUG().
 *
 * @return If no overflow, product of the two integers (\a ul_a * \a ul_b).
 */
static inline u64 nvgpu_safe_mult_u64(u64 ul_a, u64 ul_b)
{
	if ((ul_a == 0UL) || (ul_b == 0UL)) {
		return 0UL;
	} else if (ul_a > (ULONG_MAX / ul_b)) {
		BUG();
	} else {
		return ul_a * ul_b;
	}
}

/**
 * @brief Multiply two s64 values and check for overflow/underflow.
 *
 * @param sl_a [in]	Value of multiplicand.
 * @param sl_b [in]	Value of multiplier.
 *
 * Multiplies \a sl_a and \a sl_b unless the result will overflow or underflow a
 * s64. If the result would overflow or underflow, calls BUG().
 *
 * @return If no overflow/underflow, product of the two integers (\a sl_a *
 * \a sl_b).
 */
static inline s64 nvgpu_safe_mult_s64(s64 sl_a, s64 sl_b)
{
	if (sl_a > 0) {
		if (sl_b > 0) {
			if (sl_a > (LONG_MAX / sl_b)) {
				BUG();
			}
		} else {
			if (sl_b < (LONG_MIN / sl_a)) {
				BUG();
			}
		}
	} else {
		if (sl_b > 0) {
			if (sl_a < (LONG_MIN / sl_b)) {
				BUG();
			}
		} else {
			if ((sl_a != 0) && (sl_b < (LONG_MAX / sl_a))) {
				BUG();
			}
		}
	}

	return sl_a * sl_b;
}

/**
 * @brief Cast u64 to u16 and check for overflow.
 *
 * @param ul_a [in]	Value to cast.
 *
 * Casts \a ul_a to a u16 unless the result will overflow a u16. If the result
 * would overflow, calls BUG().
 *
 * @return If no overflow, u16 representation of the value in \a ul_a.
 */
static inline u16 nvgpu_safe_cast_u64_to_u16(u64 ul_a)
{
	if (ul_a > USHRT_MAX) {
		BUG();
	} else {
		return (u16)ul_a;
	}
}

/**
 * @brief Cast u64 to u32 and check for overflow.
 *
 * @param ul_a [in]	Value to cast.
 *
 * Casts \a ul_a to a u32 unless the result will overflow a u32. If the result
 * would overflow, calls BUG().
 *
 * @return If no overflow, u32 representation of the value in \a ul_a.
 */
static inline u32 nvgpu_safe_cast_u64_to_u32(u64 ul_a)
{
	if (ul_a > UINT_MAX) {
		BUG();
	} else {
		return (u32)ul_a;
	}
}

/**
 * @brief Cast u64 to u8 and check for overflow.
 *
 * @param ul_a [in]	Value to cast.
 *
 * Casts \a ul_a to a u8 unless the result will overflow a u8. If the result
 * would overflow, calls BUG().
 *
 * @return If no overflow, u8 representation of the value in \a ul_a.
 */
static inline u8 nvgpu_safe_cast_u64_to_u8(u64 ul_a)
{
	if (ul_a > nvgpu_safe_cast_s32_to_u64(UCHAR_MAX)) {
		BUG();
	} else {
		return (u8)ul_a;
	}
}

/**
 * @brief Cast s64 to u32 and check for overflow or underflow.
 *
 * @param l_a [in]	Value to cast.
 *
 * Casts \a l_a to a u32 unless the result will overflow or underflow a u32.
 * If the result would overflow/underflow, calls BUG().
 *
 * @return If no overflow/underflow, u32 representation of the value in \a l_a.
 */
static inline u32 nvgpu_safe_cast_s64_to_u32(s64 l_a)
{
	if ((l_a < 0) || (l_a > nvgpu_safe_cast_u64_to_s64(U64(UINT_MAX)))) {
		BUG();
	} else {
		return (u32)l_a;
	}
}

/**
 * @brief Cast s64 to u64 and check for underflow.
 *
 * @param l_a [in]	Value to cast.
 *
 * Casts \a l_a to a u64 unless the result will underflow a u64. If the result
 * would underflow, calls BUG().
 *
 * @return If no underflow, u64 representation of the value in \a l_a.
 */
static inline u64 nvgpu_safe_cast_s64_to_u64(s64 l_a)
{
	if (l_a < 0) {
		BUG();
	} else {
		return (u64)l_a;
	}
}

/**
 * @brief Cast bool to u32.
 *
 * @param bl_a [in]	Value to cast.
 *
 * @return If \a bl_a is true, return 1; otherwise, return 0.
 */
static inline u32 nvgpu_safe_cast_bool_to_u32(bool bl_a)
{
	return (bl_a == true) ? 1U : 0U;
}

/**
 * @brief Cast s8 to u8 and check for underflow.
 *
 * @param sc_a [in]	Value to cast.
 *
 * Casts \a sc_a to a u8 unless the result will underflow a u8. If the result
 * would underflow, calls BUG().
 *
 * @return If no underflow, u8 representation of the value in \a sc_a.
 */
static inline u8 nvgpu_safe_cast_s8_to_u8(s8 sc_a)
{
NVGPU_COV_WHITELIST(false_positive, NVGPU_CERT(STR34_C), "Bug 2673832")
	if (sc_a < 0) {
		BUG();
	} else {
		return (u8)sc_a;
	}
}

/**
 * @brief Cast s32 to u32 and check for underflow.
 *
 * @param si_a [in]	Value to cast.
 *
 * Casts \a si_a to a u32 unless the result will underflow a u32. If the result
 * would underflow, calls BUG().
 *
 * @return If no underflow, u32 representation of the value in \a si_a.
 */
static inline u32 nvgpu_safe_cast_s32_to_u32(s32 si_a)
{
	if (si_a < 0) {
		BUG();
	} else {
		return (u32)si_a;
	}
}

/**
 * @brief Cast s32 to u64 and check for underflow.
 *
 * @param si_a [in]	Value to cast.
 *
 * Casts \a si_a to a u64 unless the result will underflow a u64. If the result
 * would underflow, calls BUG().
 *
 * @return If no underflow, u64 representation of the value in \a si_a.
 */
static inline u64 nvgpu_safe_cast_s32_to_u64(s32 si_a)
{
	if (si_a < 0) {
		BUG();
	} else {
		return (u64)si_a;
	}
}

/**
 * @brief Cast u32 to u16 and check for overflow.
 *
 * @param ui_a [in]	Value to cast.
 *
 * Casts \a ui_a to a u16 unless the result will overflow a u16. If the result
 * would overflow, calls BUG().
 *
 * @return If no overflow, u16 representation of the value in \a ui_a.
 */
static inline u16 nvgpu_safe_cast_u32_to_u16(u32 ui_a)
{
	if (ui_a > USHRT_MAX) {
		BUG();
	} else {
		return (u16)ui_a;
	}
}

/**
 * @brief Cast u32 to u8 and check for overflow.
 *
 * @param ui_a [in]	Value to cast.
 *
 * Casts \a ui_a to a u8 unless the result will overflow a u8. If the result
 * would overflow, calls BUG().
 *
 * @return If no overflow, u8 representation of the value in \a ui_a.
 */
static inline u8 nvgpu_safe_cast_u32_to_u8(u32 ui_a)
{
	if (ui_a > nvgpu_safe_cast_s32_to_u32(UCHAR_MAX)) {
		BUG();
	} else {
		return (u8)ui_a;
	}
}

/**
 * @brief Cast u32 to s8 and check for overflow.
 *
 * @param ui_a [in]	Value to cast.
 *
 * Casts \a ui_a to a s8 unless the result will overflow a s8. If the result
 * would overflow, calls BUG().
 *
 * @return If no overflow, s8 representation of the value in \a ui_a.
 */
static inline s8 nvgpu_safe_cast_u32_to_s8(u32 ui_a)
{
	if (ui_a > nvgpu_safe_cast_s32_to_u32(SCHAR_MAX)) {
		BUG();
	} else {
		return (s8)ui_a;
	}
}

/**
 * @brief Cast u32 to s32 and check for overflow.
 *
 * @param ui_a [in]	Value to cast.
 *
 * Casts \a ui_a to a s32 unless the result will overflow a s32. If the result
 * would overflow, calls BUG().
 *
 * @return If no overflow, s32 representation of the value in \a ui_a.
 */
static inline s32 nvgpu_safe_cast_u32_to_s32(u32 ui_a)
{
	if (ui_a > nvgpu_safe_cast_s32_to_u32(INT_MAX)) {
		BUG();
	} else {
		return (s32)ui_a;
	}
}

/**
 * @brief Cast u64 to s32 and check for overflow.
 *
 * @param ul_a [in]	Value to cast.
 *
 * Casts \a ul_a to a s32 unless the result will overflow a s32. If the result
 * would overflow, calls BUG().
 *
 * @return If no overflow, s32 representation of the value in \a ul_a.
 */
static inline s32 nvgpu_safe_cast_u64_to_s32(u64 ul_a)
{
	if (ul_a > nvgpu_safe_cast_s32_to_u64(INT_MAX)) {
		BUG();
	} else {
		return (s32)ul_a;
	}
}

/**
 * @brief Cast u64 to s64 and check for overflow.
 *
 * @param ul_a [in]	Value to cast.
 *
 * Casts \a ul_a to a s64 unless the result will overflow a s64. If the result
 * would overflow, calls BUG().
 *
 * @return If no overflow, s64 representation of the value in \a ul_a.
 */
static inline s64 nvgpu_safe_cast_u64_to_s64(u64 ul_a)
{
NVGPU_COV_WHITELIST(false_positive, NVGPU_MISRA(Rule, 14_3), "Bug 2615925")
	if (ul_a > nvgpu_safe_cast_s64_to_u64(LONG_MAX)) {
		BUG();
	} else {
		return (s64)ul_a;
	}
}

/**
 * @brief Cast s64 to s32 and check for overflow or underflow.
 *
 * @param sl_a [in]	Value to cast.
 *
 * Casts \a sl_a to a s32 unless the result will overflow or underflow a s32. If
 * the result would overflow/underflow, calls BUG().
 *
 * @return If no overflow/underflow, s32 representation of the value in \a sl_a.
 */
static inline s32 nvgpu_safe_cast_s64_to_s32(s64 sl_a)
{
	if ((sl_a > INT_MAX) || (sl_a < INT_MIN)) {
		BUG();
	} else {
		return (s32)sl_a;
	}
}

/*
 * Skip unaligned access check in NvGPU when it's built for unit testing with
 * __NVGPU_UNIT_TEST__. As NvGPU UT build is not used on any production system,
 * there is no need to consider security aspects related NvGPU UT build.
 */
#if !defined(__NVGPU_UNIT_TEST__)

/*
 * NvGPU driver is built for ARM targets with unaligned-access enabled. Confirm
 * enabling of unaligned-access with a check for __ARM_FEATURE_UNALIGNED.
 *
 * This confirmation helps argue NvGPU security in context of CERT-C EXP36-C and
 * INT36-C violations which flag misalignment.
 *
 * If and when NvGPU is built using a different compiler/architecture (non-ARM),
 * a similar check for that architecture is required. If an unaligned access is
 * not possible, then the CERT-C violations due to unaligned access need to be
 * fixed.
 */
#if !defined(__ARM_FEATURE_UNALIGNED)
#error "__ARM_FEATURE_UNALIGNED not defined. Check static_analysis.h"
#endif

#endif

/**
 * @brief Return precision in bits of a number.
 *
 * @param v [in]	Value to determine precision for.
 *
 * @return s32 representation of the precision in bits of the value passed in.
 */
#define NVGPU_PRECISION(v) _Generic(v, \
		unsigned int : __builtin_popcount, \
		unsigned long : __builtin_popcountl, \
		unsigned long long : __builtin_popcountll, \
		default : __builtin_popcount)(v)

/**
 * @brief Safety checks before executing driver.
 *
 * Validate precision of unsigned types. These validations are used to justify
 * that no security issues exist in NvGPU driver due to CERT-C INT34-C and
 * INT35-C violations.
 *
 * This function shall be called early in the driver probe to ensure that code
 * violating CERT-C INT34-C and INT35-C rules is not run before these checks.
 */
static inline void nvgpu_safety_checks(void)
{
	/*
	 * For CERT-C INT35-C rule
	 * Check compatibility between size (in bytes) and precision
	 * (in bits) of unsigned int. BUG() if two are not same.
	 */
	if ((sizeof(unsigned int) * 8U) !=
		nvgpu_safe_cast_s32_to_u64(NVGPU_PRECISION(UINT_MAX))) {
		BUG();
	}

	/*
	 * For CERT-C INT34-C rule
	 * Check precision of unsigned types. Shift operands have been
	 * checked to be less than these values.
	 */
	if ((NVGPU_PRECISION(UCHAR_MAX) != 8) ||
		(NVGPU_PRECISION(USHRT_MAX) != 16) ||
		(NVGPU_PRECISION(UINT_MAX) != 32) ||
		(NVGPU_PRECISION(ULONG_MAX) != 64) ||
		(NVGPU_PRECISION(ULLONG_MAX) != 64)) {
		BUG();
	}

#if defined(__QNX__)
	/*
	 * For CERT-C EXP37-C rule
	 * Check sizes of same types considered for EXP37-C deviation record.
	 * If the sizes of data types are same, a compiler results in using same
	 * size and same precision base data type like int, long or long long,
	 * etc for redefined data types.
	 */
	if ((sizeof(s64) != sizeof(long) ||
		sizeof(s64) != sizeof(int64_t) ||
		sizeof(u64) != sizeof(uint64_t) ||
		sizeof(u64) != sizeof(_Uint64t) ||
		sizeof(u64) != sizeof(uintptr_t) ||
		sizeof(u64) != sizeof(unsigned long) ||
		sizeof(size_t) != sizeof(u64) ||
		sizeof(size_t) != sizeof(unsigned long long) ||
		sizeof(unsigned long long) != sizeof(unsigned long))) {
		BUG();
	}
#endif
}

/**
 * @}
 */

#endif /* NVGPU_STATIC_ANALYSIS_H */
