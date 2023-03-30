/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PBDMA_GM20B_H
#define NVGPU_PBDMA_GM20B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_debug_context;
struct nvgpu_channel_dump_info;
struct nvgpu_gpfifo_entry;
struct nvgpu_pbdma_status_info;
struct nvgpu_device;

bool gm20b_pbdma_handle_intr_0(struct gk20a *g, u32 pbdma_id,
			u32 pbdma_intr_0, u32 *error_notifier);
void gm20b_pbdma_handle_intr(struct gk20a *g, u32 pbdma_id, bool recover);

u32 gm20b_pbdma_read_data(struct gk20a *g, u32 pbdma_id);
void gm20b_pbdma_reset_header(struct gk20a *g, u32 pbdma_id);
void gm20b_pbdma_reset_method(struct gk20a *g, u32 pbdma_id,
			u32 pbdma_method_index);
u32 gm20b_pbdma_acquire_val(u64 timeout);

void gm20b_pbdma_format_gpfifo_entry(struct gk20a *g,
		struct nvgpu_gpfifo_entry *gpfifo_entry,
		u64 pb_gpu_va, u32 method_size);

u32 gm20b_pbdma_device_fatal_0_intr_descs(void);
u32 gm20b_pbdma_restartable_0_intr_descs(void);

void gm20b_pbdma_clear_all_intr(struct gk20a *g, u32 pbdma_id);
void gm20b_pbdma_disable_and_clear_all_intr(struct gk20a *g);

u32 gm20b_pbdma_get_gp_base(u64 gpfifo_base);
u32 gm20b_pbdma_get_gp_base_hi(u64 gpfifo_base, u32 gpfifo_entry);

u32 gm20b_pbdma_get_fc_subdevice(void);
u32 gm20b_pbdma_get_fc_target(const struct nvgpu_device *dev);
u32 gm20b_pbdma_get_ctrl_hce_priv_mode_yes(void);
u32 gm20b_pbdma_get_userd_aperture_mask(struct gk20a *g, struct nvgpu_mem *mem);
u32 gm20b_pbdma_get_userd_addr(u32 addr_lo);
u32 gm20b_pbdma_get_userd_hi_addr(u32 addr_hi);
bool gm20b_pbdma_find_for_runlist(struct gk20a *g,
				  u32 runlist_id, u32 *pbdma_mask);

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void gm20b_pbdma_intr_enable(struct gk20a *g, bool enable);

bool gm20b_pbdma_handle_intr_1(struct gk20a *g, u32 pbdma_id,
			u32 pbdma_intr_1, u32 *error_notifier);
u32 gm20b_pbdma_get_signature(struct gk20a *g);
u32 gm20b_pbdma_channel_fatal_0_intr_descs(void);

void gm20b_pbdma_syncpoint_debug_dump(struct gk20a *g,
		     struct nvgpu_debug_context *o,
		     struct nvgpu_channel_dump_info *info);
void gm20b_pbdma_setup_hw(struct gk20a *g);
u32 gm20b_pbdma_get_fc_formats(void);
u32 gm20b_pbdma_get_fc_pb_header(void);
void gm20b_pbdma_dump_status(struct gk20a *g, struct nvgpu_debug_context *o);
#endif

#endif /* NVGPU_PBDMA_GM20B_H */
