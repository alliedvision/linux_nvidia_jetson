/*
 * GA10B GR MANAGER
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

#ifndef NVGPU_GRMGR_GA10B_H
#define NVGPU_GRMGR_GA10B_H

struct gk20a;

#ifdef CONFIG_NVGPU_MIG
const struct nvgpu_mig_gpu_instance_config *ga10b_grmgr_get_mig_config_ptr(
		struct gk20a *g);
u32 ga10b_grmgr_get_allowed_swizzid_size(struct gk20a *g);
int ga10b_grmgr_init_gr_manager(struct gk20a *g);
u32 ga10b_grmgr_get_max_sys_pipes(struct gk20a *g);
int ga10b_grmgr_get_gpc_instance_gpcgrp_id(struct gk20a *g,
		u32 gpu_instance_id, u32 gr_syspipe_id, u32 *gpcgrp_id);
int ga10b_grmgr_remove_gr_manager(struct gk20a *g);
int ga10b_grmgr_get_mig_gpu_instance_config(struct gk20a *g,
		const char **config_name,
		u32 *num_config_supported);
void ga10b_grmgr_get_gpcgrp_count(struct gk20a *g);
#endif

void ga10b_grmgr_load_smc_arb_timestamp_prod(struct gk20a *g);
int ga10b_grmgr_discover_gpc_ids(struct gk20a *g,
		u32 num_gpc, struct nvgpu_gpc *gpcs);

#endif /* NVGPU_GRMGR_GA10B_H */
