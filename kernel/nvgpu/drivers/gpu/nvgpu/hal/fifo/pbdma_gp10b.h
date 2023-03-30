/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PBDMA_GP10B_H
#define NVGPU_PBDMA_GP10B_H

#include <nvgpu/types.h>

struct gk20a;

u32 gp10b_pbdma_get_signature(struct gk20a *g);

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
u32 gp10b_pbdma_channel_fatal_0_intr_descs(void);
u32 gp10b_pbdma_allowed_syncpoints_0_index_f(u32 syncpt);
u32 gp10b_pbdma_allowed_syncpoints_0_valid_f(void);
u32 gp10b_pbdma_allowed_syncpoints_0_index_v(u32 offset);
#endif

u32 gp10b_pbdma_get_fc_runlist_timeslice(void);
u32 gp10b_pbdma_get_config_auth_level_privileged(void);

#endif /* NVGPU_PBDMA_GP10B_H */
