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
#ifndef NVGPU_GOPS_USERMODE_H
#define NVGPU_GOPS_USERMODE_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Usermode HAL interface.
 */
struct gk20a;
struct nvgpu_channel;

/**
 * @file
 *
 * Usermode HAL operations.
 */
struct gops_usermode {

	/**
	 * @brief Base address for usermode drivers.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * Usermode is a mappable range of registers for use by usermode
	 * drivers.
	 *
	 * @return 64-bit base address for usermode accessible registers.
	 */
	u64 (*base)(struct gk20a *g);

	/**
	 * @brief Doorbell token.
	 *
	 * @param ch [in]	Channel Pointer.
	 *
	 * The function builds a doorbell token for channel \a ch.
	 *
	 * This token is used to notify H/W that new work is available for
	 * a given channel. This allows "usermode submit", where application
	 * handles GP and PB entries by itself, then writes token to submit
	 * work, without intervention of nvgpu rm.
	 *
	 * @return 32-bit token to ring doorbell for channel \a ch.
	 */
	u32 (*doorbell_token)(struct nvgpu_channel *ch);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */

	void (*setup_hw)(struct gk20a *g);
	void (*ring_doorbell)(struct nvgpu_channel *ch);
	u64 (*bus_base)(struct gk20a *g);

/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};


#endif
