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

#ifndef NVGPU_GR_FS_STATE_H
#define NVGPU_GR_FS_STATE_H

/**
 * @file
 *
 * common.gr.fs_state unit interface
 */
struct gk20a;
struct nvgpu_gr_config;

/**
 * @brief Initialize GR engine h/w state post-floorsweeping.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param config [in]		Pointer to GR config struct.
 *
 * This function initializes GR engine h/w state after considering
 * floorsweeping.
 *
 * It is possible that certain TPC (and hence SMs) in GPC are
 * floorswept and hence not available for any processing. In this case
 * common.gr unit is responsible to enumerate only available TPCs
 * and configure GR engine h/w registers with available GPC/TPC/SM count
 * and mapping.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if memory allocation fails for any internal data
 *         structure.
 */
int nvgpu_gr_fs_state_init(struct gk20a *g, struct nvgpu_gr_config *config);
/** @cond DOXYGEN_SHOULD_SKIP_THIS */
int nvgpu_gr_init_sm_id_early_config(struct gk20a *g, struct nvgpu_gr_config *config);
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif /* NVGPU_GR_FS_STATE_H */
