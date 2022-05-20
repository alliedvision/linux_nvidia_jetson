/*
 * GA100 GR MANAGER
 *
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GRMGR_GA100_H
#define NVGPU_GRMGR_GA100_H

struct gk20a;

const struct nvgpu_mig_gpu_instance_config *ga100_grmgr_get_mig_config_ptr(
		struct gk20a *g);
u32 ga100_grmgr_get_max_sys_pipes(struct gk20a *g);
u32 ga100_grmgr_get_allowed_swizzid_size(struct gk20a *g);
int ga100_grmgr_get_gpc_instance_gpcgrp_id(struct gk20a *g,
		u32 gpu_instance_id, u32 gr_syspipe_id, u32 *gpcgrp_id);

#endif /* NVGPU_GRMGR_GA100_H */
