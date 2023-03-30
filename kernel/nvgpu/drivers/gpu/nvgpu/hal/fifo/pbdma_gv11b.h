/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PBDMA_GV11B_H
#define NVGPU_PBDMA_GV11B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_device;

void gv11b_pbdma_setup_hw(struct gk20a *g);
void gv11b_pbdma_intr_enable(struct gk20a *g, bool enable);
bool gv11b_pbdma_handle_intr_0(struct gk20a *g, u32 pbdma_id, u32 pbdma_intr_0,
			u32 *error_notifier);
bool gv11b_pbdma_handle_intr_1(struct gk20a *g, u32 pbdma_id, u32 pbdma_intr_1,
			u32 *error_notifier);
u32 gv11b_pbdma_channel_fatal_0_intr_descs(void);
u32 gv11b_pbdma_get_fc_pb_header(void);
u32 gv11b_pbdma_get_fc_target(const struct nvgpu_device *dev);
u32 gv11b_pbdma_set_channel_info_veid(u32 subctx_id);
u32 gv11b_pbdma_config_userd_writeback_enable(u32 v);

#endif /* NVGPU_PBDMA_GV11B_H */
