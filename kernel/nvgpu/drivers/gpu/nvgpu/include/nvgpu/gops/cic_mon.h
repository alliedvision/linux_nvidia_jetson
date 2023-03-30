/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_CIC_MON_H
#define NVGPU_GOPS_CIC_MON_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Central Interrupt Controller unit HAL interface
 *
 */
struct gk20a;
struct nvgpu_cic_mon;

/**
 * CIC-MON unit HAL operations
 *
 * @see gpu_ops
 */
struct gops_cic_mon {
	/**
	 * @brief Chip specific CIC unit initialization.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param cic [in] 		Pointer to CIC private struct.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*init)(struct gk20a *g, struct nvgpu_cic_mon *cic_mon);

	/**
	 * @brief Report error to safety services.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param err_id [in]		Error ID.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*report_err)(struct gk20a *g, u32 err_id);
};

#endif/*NVGPU_GOPS_CIC_MON_H*/
