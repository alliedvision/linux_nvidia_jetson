/*
 * GA10B FUSE
 *
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FUSE_GA10B_H
#define NVGPU_FUSE_GA10B_H

#define GA10B_FUSE_READ_DEVICE_IDENTIFIER_RETRIES 100000U

struct gk20a;
struct nvgpu_fuse_feature_override_ecc;

int ga10b_fuse_read_gcplex_config_fuse(struct gk20a *g, u32 *val);
bool ga10b_fuse_is_opt_ecc_enable(struct gk20a *g);
bool ga10b_fuse_is_opt_feature_override_disable(struct gk20a *g);
u32 ga10b_fuse_status_opt_gpc(struct gk20a *g);
u32 ga10b_fuse_status_opt_fbio(struct gk20a *g);
u32 ga10b_fuse_status_opt_fbp(struct gk20a *g);
u32 ga10b_fuse_status_opt_l2_fbp(struct gk20a *g, u32 fbp);
u32 ga10b_fuse_status_opt_tpc_gpc(struct gk20a *g, u32 gpc);
void ga10b_fuse_ctrl_opt_tpc_gpc(struct gk20a *g, u32 gpc, u32 val);
u32 ga10b_fuse_opt_priv_sec_en(struct gk20a *g);
u32 ga10b_fuse_opt_sm_ttu_en(struct gk20a *g);
void ga10b_fuse_write_feature_override_ecc(struct gk20a *g, u32 val);
void ga10b_fuse_write_feature_override_ecc_1(struct gk20a *g, u32 val);
void ga10b_fuse_read_feature_override_ecc(struct gk20a *g,
		struct nvgpu_fuse_feature_override_ecc *ecc_feature);
int ga10b_fuse_read_per_device_identifier(struct gk20a *g, u64 *pdi);
u32 ga10b_fuse_opt_sec_debug_en(struct gk20a *g);
u32 ga10b_fuse_opt_secure_source_isolation_en(struct gk20a *g);
int ga10b_fuse_check_priv_security(struct gk20a *g);
int ga10b_fetch_falcon_fuse_settings(struct gk20a *g, u32 falcon_id,
		unsigned long *fuse_settings);
u32 ga10b_fuse_status_opt_pes_gpc(struct gk20a *g, u32 gpc);
u32 ga10b_fuse_status_opt_rop_gpc(struct gk20a *g, u32 gpc);

#endif /* NVGPU_FUSE_GA10B_H */
