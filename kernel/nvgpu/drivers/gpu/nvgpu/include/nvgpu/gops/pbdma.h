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
#ifndef NVGPU_GOPS_PBDMA_H
#define NVGPU_GOPS_PBDMA_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_gpfifo_entry;
struct nvgpu_pbdma_status_info;
struct nvgpu_device;

/** @cond DOXYGEN_SHOULD_SKIP_THIS */

/** NON FUSA */
struct nvgpu_debug_context;
struct nvgpu_channel_dump_info;

struct gops_pbdma_status {
	void (*read_pbdma_status_info)(struct gk20a *g,
		u32 pbdma_id, struct nvgpu_pbdma_status_info *status);
};

struct gops_pbdma {
	int (*setup_sw)(struct gk20a *g);
	void (*cleanup_sw)(struct gk20a *g);
	void (*setup_hw)(struct gk20a *g);
	void (*intr_enable)(struct gk20a *g, bool enable);
	bool (*handle_intr_0)(struct gk20a *g,
			u32 pbdma_id, u32 pbdma_intr_0,
			u32 *error_notifier);
	bool (*handle_intr_1)(struct gk20a *g,
			u32 pbdma_id, u32 pbdma_intr_1,
			u32 *error_notifier);
	void (*handle_intr)(struct gk20a *g, u32 pbdma_id, bool recover);
	u32 (*set_clear_intr_offsets) (struct gk20a *g,
			u32 set_clear_size);
	u32 (*get_signature)(struct gk20a *g);
	u32 (*acquire_val)(u64 timeout);
	u32 (*read_data)(struct gk20a *g, u32 pbdma_id);
	void (*reset_header)(struct gk20a *g, u32 pbdma_id);
	u32 (*device_fatal_0_intr_descs)(void);
	u32 (*channel_fatal_0_intr_descs)(void);
	u32 (*restartable_0_intr_descs)(void);
	void (*format_gpfifo_entry)(struct gk20a *g,
			struct nvgpu_gpfifo_entry *gpfifo_entry,
			u64 pb_gpu_va, u32 method_size);
	u32 (*get_gp_base)(u64 gpfifo_base);
	u32 (*get_gp_base_hi)(u64 gpfifo_base, u32 gpfifo_entry);
	u32 (*get_fc_formats)(void);
	u32 (*get_fc_pb_header)(void);
	u32 (*get_fc_subdevice)(void);
	u32 (*get_fc_target)(const struct nvgpu_device *dev);
	u32 (*get_ctrl_hce_priv_mode_yes)(void);
	u32 (*get_userd_aperture_mask)(struct gk20a *g,
			struct nvgpu_mem *mem);
	u32 (*get_userd_addr)(u32 addr_lo);
	u32 (*get_userd_hi_addr)(u32 addr_hi);
	u32 (*get_fc_runlist_timeslice)(void);
	u32 (*get_config_auth_level_privileged)(void);
	u32 (*set_channel_info_veid)(u32 subctx_id);
	u32 (*config_userd_writeback_enable)(u32 v);
	u32 (*allowed_syncpoints_0_index_f)(u32 syncpt);
	u32 (*allowed_syncpoints_0_valid_f)(void);
	u32 (*allowed_syncpoints_0_index_v)(u32 offset);
	u32 (*set_channel_info_chid)(u32 chid);
	u32 (*set_intr_notify)(u32 eng_intr_vector);
	u32 (*get_mmu_fault_id)(struct gk20a *g, u32 pbdma_id);
	u32 (*get_num_of_pbdmas)(void);

	/** NON FUSA */
	void (*syncpt_debug_dump)(struct gk20a *g,
			struct nvgpu_debug_context *o,
			struct nvgpu_channel_dump_info *info);
	void (*dump_status)(struct gk20a *g,
			struct nvgpu_debug_context *o);
#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
	void (*pbdma_force_ce_split)(struct gk20a *g);
#endif
};

/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif
