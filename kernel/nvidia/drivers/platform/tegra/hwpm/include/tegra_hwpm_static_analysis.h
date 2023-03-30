/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef TEGRA_HWPM_STATIC_ANALYSIS_H
#define TEGRA_HWPM_STATIC_ANALYSIS_H

#include <linux/types.h>
#include <linux/bug.h>

/**
 * @brief Add two u32 values and check for overflow.
 *
 * @param ui_a [in]	Addend value for adding.
 * @param ui_b [in]	Addend value for adding.
 *
 * Adds the two u32 (unsigned 32 bit) values unless the result will overflow a
 * u32. Overflow will happen if the difference between UNIT_MAX (the max number
 * representable with 32 bits) and one addend is less than the other addend.
 * Call #BUG() in such a case else return the sum.
 *
 * @return If no overflow, sum of the two integers (\a ui_a + \a ui_b).
 */
static inline u32 tegra_hwpm_safe_add_u32(u32 ui_a, u32 ui_b)
{
	if ((UINT_MAX - ui_a) < ui_b) {
		WARN_ON(true);
		return 0U;
	} else {
		return ui_a + ui_b;
	}
}

/**
 * @brief Add two u64 values and check for overflow.
 *
 * @param ul_a [in]	Addend value for adding.
 * @param ul_b [in]	Addend value for adding.
 *
 * Adds the two u64 (unsigned 64 bit) values unless the result will overflow a
 * u64. Overflow will happen if the difference between ULONG_MAX (the max number
 * representable with 64 bits) and one addend is less than the other addend.
 * Call #BUG() in such a case else return the sum.
 *
 * @return If no overflow, sum of the two integers (\a ul_a + \a ul_b).
 */
static inline u64 tegra_hwpm_safe_add_u64(u64 ul_a, u64 ul_b)
{
	if ((ULONG_MAX - ul_a) < ul_b) {
		WARN_ON(true);
		return 0U;
	} else {
		return ul_a + ul_b;
	}
}

/**
 * @brief Subtract two u32 values and check for underflow.
 *
 * @param ui_a [in]	Value of minuend.
 * @param ui_b [in]	Value of subtrahend.
 *
 * Subtracts \a ui_b from \a ui_a unless the result will underflow a u32
 * (unsigned 32 bit). If \a ui_a is less than \a ui_b then result would
 * underflow so call #BUG().
 *
 * @return If no overflow, difference of the two integers (\a ui_a - \a ui_b).
 */
static inline u32 tegra_hwpm_safe_sub_u32(u32 ui_a, u32 ui_b)
{
	if (ui_a < ui_b) {
		WARN_ON(true);
		return 0U;
	} else {
		return ui_a - ui_b;
	}
}

/**
 * @brief Subtract two u64 values and check for underflow.
 *
 * @param ul_a [in]	Value of minuend.
 * @param ul_b [in]	Value of subtrahend.
 *
 * Subtracts \a ul_b from \a ul_a unless the result will underflow a u64
 * (unsigned 64 bit). If \a ul_a is less than \a ul_b then result would
 * underflow so call #BUG().
 *
 * @return If no overflow, difference of the two integers (\a ul_a - \a ul_b).
 */
static inline u64 tegra_hwpm_safe_sub_u64(u64 ul_a, u64 ul_b)
{
	if (ul_a < ul_b) {
		WARN_ON(true);
		return 0U;
	} else {
		return ul_a - ul_b;
	}
}

/**
 * @brief Multiply two u32 values and check for overflow.
 *
 * @param ui_a [in]	Value of multiplicand.
 * @param ui_b [in]	Value of multiplier.
 *
 * Multiplies \a ui_a and \a ui_b unless the result will overflow a u32. If the
 * result would overflow, call #BUG(). To determine whether the multiplication
 * will cause an overflow, following steps are performed.
 * - If \a ui_a or \a ui_b is 0 return 0 as multiplication result.
 * - Else if \a ui_a is greater than UINT_MAX division by \a ui_b,
 *   multiplication will overflow so call #BUG().
 * - Else return result of multiplication.
 *
 * @return If no overflow, product of the two integers (\a ui_a * \a ui_b).
 */
static inline u32 tegra_hwpm_safe_mult_u32(u32 ui_a, u32 ui_b)
{
	if ((ui_a == 0U) || (ui_b == 0U)) {
		return 0U;
	} else if (ui_a > (UINT_MAX / ui_b)) {
		WARN_ON(true);
		return 0U;
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
 * Multiplies \a ul_a and \a ul_b unless the result will overflow a u32. If the
 * result would overflow, call #BUG(). To determine whether the multiplication
 * will cause an overflow, following steps are performed.
 * - If \a ul_a or \a ul_b is 0 return 0 as multiplication result.
 * - Else if \a ul_a is greater than ULONG_MAX division by \a ul_b,
 *   multiplication will overflow so call #BUG().
 * - Else return result of multiplication.
 *
 * @return If no overflow, product of the two integers (\a ul_a * \a ul_b).
 */
static inline u64 tegra_hwpm_safe_mult_u64(u64 ul_a, u64 ul_b)
{
	if ((ul_a == 0UL) || (ul_b == 0UL)) {
		return 0UL;
	} else if (ul_a > (ULONG_MAX / ul_b)) {
		WARN_ON(true);
		return 0U;
	} else {
		return ul_a * ul_b;
	}
}

/**
 * @brief Cast u64 to u32 and check for overflow.
 *
 * @param ul_a [in]	Value to cast.
 *
 * Casts \a ul_a to a u32 unless the result will overflow a u32. If \a ul_a is
 * greater than UINT_MAX which indicates overflow, call #BUG().
 *
 * @return If no overflow, u32 representation of the value in \a ul_a.
 */
static inline u32 tegra_hwpm_safe_cast_u64_to_u32(u64 ul_a)
{
	if (ul_a > UINT_MAX) {
		WARN_ON(true);
		return 0U;
	} else {
		return (u32)ul_a;
	}
}

/**
 * @brief Cast s32 to u64 and check for underflow.
 *
 * @param si_a [in]	Value to cast.
 *
 * Casts \a si_a to a u64 unless the result will underflow a u64. If \a si_a is
 * less than 0 which indicates underflow, call #BUG().
 *
 * @return If no underflow, u64 representation of the value in \a si_a.
 */
static inline u64 tegra_hwpm_safe_cast_s32_to_u64(s32 si_a)
{
	if (si_a < 0) {
		WARN_ON(true);
		return 0U;
	} else {
		return (u64)si_a;
	}
}

/**
 * @brief Cast u64 to s32 and check for overflow.
 *
 * @param ul_a [in]	Value to cast.
 *
 * Casts \a ul_a to a s32 unless the result will overflow a s32. If \a ul_a is
 * greater than INT_MAX (typecasted to u64) which indicates overflow, call
 * #BUG().
 *
 * @return If no overflow, s32 representation of the value in \a ul_a.
 */
static inline s32 tegra_hwpm_safe_cast_u64_to_s32(u64 ul_a)
{
	if (ul_a > tegra_hwpm_safe_cast_s32_to_u64(INT_MAX)) {
		WARN_ON(true);
		return 0;
	} else {
		return (s32)ul_a;
	}
}

/**
 * @brief Cast u32 to u8 and check for overflow.
 *
 * @param ui_a [in]	Value to cast.
 *
 * Casts \a ui_a to a u8 unless the result will overflow a u8. If \a ui_a is
 * greater than UCHAR_MAX (typecasted to u32) which indicates overflow, call
 * #BUG().
 *
 * @return If no overflow, u8 representation of the value in \a ui_a.
 */
static inline u8 tegra_hwpm_safe_cast_u32_to_u8(u32 ui_a)
{
	if (ui_a > U8_MAX) {
		WARN_ON(true);
		return 0U;
	} else {
		return (u8)ui_a;
	}
}

#endif /* TEGRA_HWPM_STATIC_ANALYSIS_H */
