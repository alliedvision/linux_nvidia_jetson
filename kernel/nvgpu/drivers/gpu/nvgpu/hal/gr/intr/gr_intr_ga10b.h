/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_INTR_GA10B_H
#define NVGPU_GR_INTR_GA10B_H

struct gk20a;
struct nvgpu_gr_config;
struct nvgpu_gr_tpc_exception;
struct nvgpu_gr_sm_ecc_status;
enum nvgpu_gr_sm_ecc_error_types;
struct nvgpu_gr_intr_info;

/* Copy required definitions from clc797.h and clc7c0.h class files */
#define NVC797_SET_SHADER_EXCEPTIONS		0x1528U
#define NVC797_SET_GO_IDLE_TIMEOUT 		0x022cU
#define NVC797_SET_CIRCULAR_BUFFER_SIZE 	0x1280U
#define NVC797_SET_ALPHA_CIRCULAR_BUFFER_SIZE 	0x02dcU
#define NVC797_SET_CB_BASE			0x1014U
#define NVC797_SET_BES_CROP_DEBUG4		0x10b0U
#define NVC797_SET_TEX_IN_DBG			0x10bcU
#define NVC797_SET_SKEDCHECK			0x10c0U

#define NVC7C0_SET_SHADER_EXCEPTIONS		0x1528U
#define NVC7C0_SET_CB_BASE			0x0220U
#define NVC7C0_SET_BES_CROP_DEBUG4		0x022cU
#define NVC7C0_SET_TEX_IN_DBG			0x0238U
#define NVC7C0_SET_SKEDCHECK			0x023cU

int ga10b_gr_intr_handle_sw_method(struct gk20a *g, u32 addr,
			u32 class_num, u32 offset, u32 data);
void ga10b_gr_intr_enable_interrupts(struct gk20a *g, bool enable);
void ga10b_gr_intr_enable_gpc_exceptions(struct gk20a *g,
					 struct nvgpu_gr_config *gr_config);
u32 ga10b_gr_intr_get_tpc_exception(struct gk20a *g, u32 offset,
				   struct nvgpu_gr_tpc_exception *pending_tpc);
void ga10b_gr_intr_set_hww_esr_report_mask(struct gk20a *g);
void ga10b_gr_intr_enable_exceptions(struct gk20a *g,
			struct nvgpu_gr_config *gr_config, bool enable);
bool ga10b_gr_intr_handle_exceptions(struct gk20a *g, bool *is_gpc_exception);
void ga10b_gr_intr_handle_gpc_gpcmmu_exception(struct gk20a *g, u32 gpc,
		u32 gpc_exception, u32 *corrected_err, u32 *uncorrected_err);
void ga10b_gr_intr_handle_tpc_sm_ecc_exception(struct gk20a *g, u32 gpc,
					       u32 tpc);
bool ga10b_gr_intr_sm_ecc_status_errors(struct gk20a *g,
	u32 ecc_status_reg, enum nvgpu_gr_sm_ecc_error_types err_type,
	struct nvgpu_gr_sm_ecc_status *ecc_status);
int ga10b_gr_intr_retrigger(struct gk20a *g);
void ga10b_gr_intr_enable_gpc_crop_hww(struct gk20a *g);
void ga10b_gr_intr_enable_gpc_zrop_hww(struct gk20a *g);
void ga10b_gr_intr_handle_gpc_crop_hww(struct gk20a *g, u32 gpc, u32 exception);
void ga10b_gr_intr_handle_gpc_zrop_hww(struct gk20a *g, u32 gpc, u32 exception);
void ga10b_gr_intr_handle_gpc_rrh_hww(struct gk20a *g, u32 gpc, u32 exception);
u32 ga10b_gr_intr_read_pending_interrupts(struct gk20a *g,
					struct nvgpu_gr_intr_info *intr_info);
u32 ga10b_gr_intr_enable_mask(struct gk20a *g);

#endif /* NVGPU_GR_INTR_GA10B_H */
