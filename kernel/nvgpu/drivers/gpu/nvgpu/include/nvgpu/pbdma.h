/*
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PBDMA_COMMON_H
#define NVGPU_PBDMA_COMMON_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Push-Buffer DMA
 *
 * PBDMA unit fetches pushbuffer data from memory, generates
 * commands, called "methods", from the fetched data, executes some of the
 * generated methods itself, and sends the remainder of the methods to engines.
 */
struct gk20a;

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#define PBDMA_PER_RUNLIST_SIZE		2U
#define NVGPU_INVALID_PBDMA_PRI_BASE	U32_MAX
#define NVGPU_INVALID_PBDMA_ID		U32_MAX

struct nvgpu_pbdma_info {
	/** The pri offset of the i'th PBDMA for runlist_pri_base */
	u32 pbdma_pri_base[PBDMA_PER_RUNLIST_SIZE];
	/** The ID of the i'th PBDMA that runs channels on this runlist */
	u32 pbdma_id[PBDMA_PER_RUNLIST_SIZE];
};
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Initialize PBDMA software context
 *
 * @param g [in]		The GPU driver struct for which to initialize
 *				PBDMA software context.
 *
 * Gets number of PBDMAs and builds a map of runlists that will be serviced
 * by those PBDMAs.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM in case there is not enough memory available.
 */
int nvgpu_pbdma_setup_sw(struct gk20a *g);

/**
 * @brief Clean up PBDMA software context
 *
 * @param g [in]		The GPU driver struct using PBDMA software
 *				context.
 *
 * Cleans up PBDMA software context and related resources.
 */
void nvgpu_pbdma_cleanup_sw(struct gk20a *g);

#endif /* NVGPU_PBDMA_COMMON_H */
