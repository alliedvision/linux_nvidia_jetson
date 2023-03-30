/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_CLASS_H
#define NVGPU_GOPS_CLASS_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * common.class unit HAL interface
 *
 */

/**
 * common.class unit HAL operations
 *
 * @see gpu_ops
 */
struct gops_class {
	/**
	 * @brief Checks if given class number is valid as per our GPU
	 *        architechture. This API is used by common.gr unit to
	 *        validate the class associated with the channel.
	 *
	 * List of valid class numbers:
	 * 1. Compute class:
	 * 	- \ref #VOLTA_COMPUTE_A           -> 0xC3C0U
	 * 2. DMA copy class:
	 *	- \ref #VOLTA_DMA_COPY_A          -> 0xC3B5U
	 * 3. Channel Gpfifo class:
	 *	- \ref #VOLTA_CHANNEL_GPFIFO_A    -> 0xC36FU
	 * 4. Graphics class:
	 *	- \ref #VOLTA_A                   -> 0xC397U
	 *
	 * @param class_num [in]	Class number to be validated based on
	 *                              GPU architecture.
	 *				- No validation is performed on this
	 * 				  parameter
	 *
	 * @return true when \a class_num is one of the numbers in above list or
	 *	   false otherwise.
	 */
	bool (*is_valid)(u32 class_num);

	/**
	 * @brief Checks if given class number is valid compute class number
	 * 	  as per our GPU architechture. This API is used by common.gr
	 *        unit to set apart the compute class from other classes.
	 *        This is needed when the preemption mode is selected based
	 *        on the class type.
	 *
	 * List of valid compute class numbers:
	 * 	- \ref #VOLTA_COMPUTE_A           -> 0xC3C0U
	 *
	 * @param class_num [in]	Class number to be validated based on
	 * 				GPU architecture.
	 *				- No validation is performed on this
	 * 				  parameter
	 *
	 * @return true when \a class_num is one of the numbers in above list or
	 *	   false otherwise.
	 */
	bool (*is_valid_compute)(u32 class_num);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_GRAPHICS
	bool (*is_valid_gfx)(u32 class_num);
#endif
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

#endif /* NVGPU_GOPS_CLASS_H */
