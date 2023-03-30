/*
 * GR MANAGER
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

#ifndef NVGPU_GRMGR_H
#define NVGPU_GRMGR_H

#include <nvgpu/types.h>
#include <nvgpu/mig.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gk20a.h>

#define EMULATE_MODE_MASK          0X000000FFU

enum emulate_mode_config {
	EMULATE_MODE_DISABLE = 0U,
	EMULATE_MODE_1_GPC = 1U,
	EMULATE_MODE_2_GPC = 2U,
	EMULATE_MODE_MAX_CONFIG = 3U
};

struct gk20a;

int nvgpu_init_gr_manager(struct gk20a *g);

int nvgpu_grmgr_config_gr_remap_window(struct gk20a *g,
		u32 gr_syspipe_id, bool enable);
u32 nvgpu_grmgr_get_num_gr_instances(struct gk20a *g);
u32 nvgpu_grmgr_get_gr_syspipe_id(struct gk20a *g, u32 gr_instance_id);
u32 nvgpu_grmgr_get_gr_num_gpcs(struct gk20a *g, u32 gr_instance_id);
u32 nvgpu_grmgr_get_gr_num_fbps(struct gk20a *g, u32 gr_instance_id);
u32 nvgpu_grmgr_get_gr_gpc_phys_id(struct gk20a *g, u32 gr_instance_id,
		u32 gpc_local_id);
u32 nvgpu_grmgr_get_gr_gpc_logical_id(struct gk20a *g, u32 gr_instance_id,
		u32 gpc_local_id);
u32 nvgpu_grmgr_get_gr_instance_id(struct gk20a *g, u32 gpu_instance_id);
bool nvgpu_grmgr_is_valid_runlist_id(struct gk20a *g,
		u32 gpu_instance_id, u32 runlist_id);
u32 nvgpu_grmgr_get_gpu_instance_runlist_id(struct gk20a *g,
		u32 gpu_instance_id);
u32 nvgpu_grmgr_get_gr_instance_id_for_syspipe(struct gk20a *g,
		u32 gr_syspipe_id);
u32 nvgpu_grmgr_get_gpu_instance_max_veid_count(struct gk20a *g,
		u32 gpu_instance_id);
u32 nvgpu_grmgr_get_gr_max_veid_count(struct gk20a *g, u32 gr_instance_id);
u32 nvgpu_grmgr_get_gr_logical_gpc_mask(struct gk20a *g, u32 gr_instance_id);
u32 nvgpu_grmgr_get_gr_physical_gpc_mask(struct gk20a *g, u32 gr_instance_id);
u32 nvgpu_grmgr_get_num_fbps(struct gk20a *g, u32 gpu_instance_id);
u32 nvgpu_grmgr_get_fbp_en_mask(struct gk20a *g, u32 gpu_instance_id);
u32 nvgpu_grmgr_get_fbp_logical_id(struct gk20a *g, u32 gr_instance_id,
	u32 fbp_local_id);
u32 *nvgpu_grmgr_get_fbp_l2_en_mask(struct gk20a *g, u32 gpu_instance_id);
bool nvgpu_grmgr_get_memory_partition_support_status(struct gk20a *g,
	u32 gr_instance_id);

static inline bool nvgpu_grmgr_is_mig_type_gpu_instance(
		struct nvgpu_gpu_instance *gpu_instance)
{
	return (gpu_instance->gpu_instance_type == NVGPU_MIG_TYPE_MIG);
}

static inline bool nvgpu_grmgr_is_multi_gr_enabled(struct gk20a *g)
{
	return ((nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) &&
		(g->mig.num_gpu_instances > 1U));
}

static inline u32 nvgpu_grmgr_get_max_gpc_count(struct gk20a *g)
{
	return g->mig.max_gpc_count;
}

static inline u32 nvgpu_grmgr_get_max_fbps_count(struct gk20a *g)
{
	return g->mig.max_fbps_count;
}

#endif /* NVGPU_GRMGR_H */
