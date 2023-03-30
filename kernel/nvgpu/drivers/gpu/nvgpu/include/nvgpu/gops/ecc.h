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
#ifndef NVGPU_GOPS_ECC_H
#define NVGPU_GOPS_ECC_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * ECC HAL interface.
 */
struct gk20a;

/**
 * ECC unit hal operations.
 *
 * This structure stores the ECC unit hal pointers.
 *
 * @see gops
 */
struct gops_ecc {
	/**
	 * @brief Initialize ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function initializes the list head for tracking the list
	 * of ecc error counts for all units (like GR/LTC/FB/PMU) and
	 * subunits of GR (like falcon/sm/gpccs/etc).
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*ecc_init_support)(struct gk20a *g);

	/**
	 * @brief Remove ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function frees all the memory allocated for keeping
	 * track of ecc error counts for each GR engine units.
	 */
	void (*ecc_remove_support)(struct gk20a *g);

	/**
	 * @brief Finish ECC support initialization.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function initializes the sysfs nodes for ECC counters and
	 * marks ECC as initialized.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*ecc_finalize_support)(struct gk20a *g);
};

#endif /* NVGPU_GOPS_ECC_H */
