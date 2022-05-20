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

#ifndef NVGPU_FBP_H
#define NVGPU_FBP_H

/**
 * @file
 *
 * Declares the Public APIs exposed by common.fbp unit.
 */
#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_fbp;

/**
 * @brief Read and initialize FBP configuration information in struct nvgpu_fbp.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This API reads various FBP related configuration information like:
 * 1. Maximum number of FBPs from PTOP_SCAL_NUM_FBPS
 * 2. Active FBP mask from the fuse(accessed from GPU MMIO register space)
 *
 * All the above configuration information is stored in a struct nvgpu_fbp
 * and exposed to other units through APIs.
 *
 * @return 0 for success and <0 value for failure.
 * @retval -ENOMEM in case there is insufficient memory to allocate struct
 *         nvgpu_fbp
 */
int nvgpu_fbp_init_support(struct gk20a *g);

/**
 * @brief Removes all the FBP related stored configuration information.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This API frees up all the memory used to store FBP configuration information
 * and sets the pointer to FBP structure in \a g to NULL.
 */
void nvgpu_fbp_remove_support(struct gk20a *g);


/**
 * @brief Get the maximum number of FBPs as stored in \a fbp.
 *
 * @param fbp [in]	Pointer to FBP configuration structure.
 *
 * This API helps retrieve the FBP data namely maximum number of FBPs from the
 * information stored in \a fbp.
 * During initialization, \a fbp is populated with all the configuration
 * information like max_fbps_count and APIs like this are used to get data from
 * FBP's private structure (struct nvgpu_fbp).
 *
 * @return Maximum number of FBPs as stored in \a fbp.
 */
u32 nvgpu_fbp_get_max_fbps_count(struct nvgpu_fbp *fbp);

/**
 * @brief Gets the active FBP mask as stored in \a fbp.
 *
 * @param fbp [in]	Pointer to FBP configuration structure.
 *
 * This API helps retrieve the FBP data namely active FBP mask from the
 * information stored in \a fbp.
 * During initialization, \a fbp is populated with all the configuration
 * information like fbp_en_mask and APIs like this are used to get data from
 * FBP's private structure (struct nvgpu_fbp).
 *
 * @return Mask corresponding to active FBPs as stored in \a fbp.
 */
u32 nvgpu_fbp_get_fbp_en_mask(struct nvgpu_fbp *fbp);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */

#ifdef CONFIG_NVGPU_NON_FUSA
u32 nvgpu_fbp_get_num_fbps(struct nvgpu_fbp *fbp);
u32 *nvgpu_fbp_get_l2_en_mask(struct nvgpu_fbp *fbp);
#endif

/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif /* NVGPU_FBP_H */
