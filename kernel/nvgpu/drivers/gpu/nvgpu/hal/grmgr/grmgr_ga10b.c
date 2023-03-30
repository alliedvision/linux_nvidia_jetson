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

#include <nvgpu/types.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/grmgr.h>

#include "grmgr_ga10b.h"
#include <nvgpu/hw/ga10b/hw_smcarb_ga10b.h>

#ifdef CONFIG_NVGPU_MIG
#include <nvgpu/enabled.h>
#include <nvgpu/string.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>

#define GA10B_GRMGR_PSMCARB_ALLOWED_UGPU(gpu_instance_id, gpcgrp_id) \
	(((gpu_instance_id) == 0U))

/* Static mig config list for 2 syspipes(0x3U) + 2 GPCs + 2 Aysnc LCEs + 2:0 gpc group config */
static const struct nvgpu_mig_gpu_instance_config ga10b_gpu_instance_config = {
	.usable_gr_syspipe_count	= 2U,
	.usable_gr_syspipe_mask		= 0x3U,
	.num_config_supported		= 2U,
	.gpcgrp_gpc_count		= { 2U, 0U },
	.gpc_count			= 2U,
	.gpu_instance_config = {
		{.config_name = "2 GPU instances each with 1 GPC",
		 .num_gpu_instances = 2U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 0U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 0U,
			 .gr_syspipe_id			= 1U,
			 .num_gpc			= 1U}}},
		{.config_name = "1 GPU instance with 2 GPCs",
		 .num_gpu_instances = 1U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 0U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 2U}}}}
};

const struct nvgpu_mig_gpu_instance_config *ga10b_grmgr_get_mig_config_ptr(
		struct gk20a *g) {
	static struct nvgpu_mig_gpu_instance_config ga10b_gpu_instance_default_config;
	struct nvgpu_gpu_instance_config *gpu_instance_config =
		&ga10b_gpu_instance_default_config.gpu_instance_config[0];

	if ((g->mig.usable_gr_syspipe_count ==
				ga10b_gpu_instance_config.usable_gr_syspipe_count) &&
			(g->mig.usable_gr_syspipe_mask ==
				ga10b_gpu_instance_config.usable_gr_syspipe_mask) &&
			(g->mig.gpc_count ==
				ga10b_gpu_instance_config.gpc_count) &&
			(g->mig.gpcgrp_gpc_count[0] ==
				ga10b_gpu_instance_config.gpcgrp_gpc_count[0]) &&
			(g->mig.gpcgrp_gpc_count[1] ==
				ga10b_gpu_instance_config.gpcgrp_gpc_count[1])) {
		nvgpu_log(g, gpu_dbg_mig,
			"Static mig config list for 2 syspipes + 2 GPCs + 2 Aysnc LCEs "
				"+ 2:0 gpc group config ");
		return &ga10b_gpu_instance_config;
	}

	/* Fall back to default config */
	ga10b_gpu_instance_default_config.usable_gr_syspipe_count =
		g->mig.usable_gr_syspipe_count;
	ga10b_gpu_instance_default_config.usable_gr_syspipe_mask =
		g->mig.usable_gr_syspipe_mask;
	ga10b_gpu_instance_default_config.num_config_supported = 1U;
	ga10b_gpu_instance_default_config.gpcgrp_gpc_count[0] =
		g->mig.gpcgrp_gpc_count[0];
	ga10b_gpu_instance_default_config.gpcgrp_gpc_count[1] =
		g->mig.gpcgrp_gpc_count[1];
	ga10b_gpu_instance_default_config.gpc_count = g->mig.gpc_count;
	snprintf(gpu_instance_config->config_name,
		NVGPU_MIG_MAX_CONFIG_NAME_SIZE,
		"1 GPU instance with %u GPCs", g->mig.gpc_count);
	gpu_instance_config->num_gpu_instances = 1U;
	gpu_instance_config->gpu_instance_static_config[0].gpu_instance_id = 0U;
	gpu_instance_config->gpu_instance_static_config[0].gr_syspipe_id = 0U;
	gpu_instance_config->gpu_instance_static_config[0].num_gpc =
		g->mig.gpc_count;

	nvgpu_err(g,
		"mig gpu instance config is not found for usable_gr_syspipe_count[%u %u] "
			"usable_gr_syspipe_mask[%x %x] gpc[%u %u] "
			"fall back to 1 GPU instance with %u GPCs",
		g->mig.usable_gr_syspipe_count,
		ga10b_gpu_instance_config.usable_gr_syspipe_count,
		g->mig.usable_gr_syspipe_mask,
		ga10b_gpu_instance_config.usable_gr_syspipe_mask,
		g->mig.gpc_count,
		ga10b_gpu_instance_config.gpc_count,
		g->mig.gpc_count);
	return ((const struct nvgpu_mig_gpu_instance_config *)
		&ga10b_gpu_instance_default_config);
}

void ga10b_grmgr_get_gpcgrp_count(struct gk20a *g)
{
	u32 gpcgrp_gpc_count[2] = {0U, 0U};
	u32 logical_gpc_id;
	struct nvgpu_gpc *gpcs = g->mig.gpu_instance[0].gr_syspipe.gpcs;
	u32 reg_val = nvgpu_readl(g, smcarb_ugpu_gpc_count_r());
	g->mig.gpcgrp_gpc_count[0] = smcarb_ugpu_gpc_count_ugpu0_v(reg_val);
	g->mig.gpcgrp_gpc_count[1] = smcarb_ugpu_gpc_count_ugpu1_v(reg_val);

	for (logical_gpc_id = 0U; logical_gpc_id < g->mig.gpc_count;
			logical_gpc_id++) {
		if (gpcs[logical_gpc_id].gpcgrp_id == 0U) {
			++gpcgrp_gpc_count[0];
		} else if (gpcs[logical_gpc_id].gpcgrp_id == 1U) {
			++gpcgrp_gpc_count[1];
		} else {
			nvgpu_err(g, "invalid gpcgrp_id[%d]",
				gpcs[logical_gpc_id].gpcgrp_id);
			nvgpu_assert(gpcs[logical_gpc_id].gpcgrp_id <= 1U);
		}
	}

	if ((gpcgrp_gpc_count[0] != g->mig.gpcgrp_gpc_count[0]) ||
			(gpcgrp_gpc_count[1] != g->mig.gpcgrp_gpc_count[1])) {
		nvgpu_log(g, gpu_dbg_mig,
			"expected gpcgrp0_gpc_count[%u] actual gpcgrp0_gpc_count[%u] "
				"expected gpcgrp1_gpc_count[%u] actual gpcgrp1_gpc_count[%u] "
				"g->mig.gpc_count[%u]",
			g->mig.gpcgrp_gpc_count[0], gpcgrp_gpc_count[0],
			g->mig.gpcgrp_gpc_count[1], gpcgrp_gpc_count[1],
			g->mig.gpc_count);
	}

	g->mig.gpcgrp_gpc_count[0] = gpcgrp_gpc_count[0];
	g->mig.gpcgrp_gpc_count[1] = gpcgrp_gpc_count[1];
}

static bool ga10b_grmgr_is_syspipe_lce(struct gk20a *g,
				      const struct nvgpu_device *gr_dev,
				      const struct nvgpu_device *lce_dev)
{
	u32 gr_fb_thread_id;
	u32 lce_fb_thread_id;

	gr_fb_thread_id = g->ops.runlist.get_esched_fb_thread_id(g,
		gr_dev->rl_pri_base);

	lce_fb_thread_id = g->ops.runlist.get_esched_fb_thread_id(g,
		lce_dev->rl_pri_base);

	nvgpu_log(g, gpu_dbg_mig,
		"gr_engine_id[%u] lce_engine_id[%u] "
			"gr_fb_thread_id[%u] lce_fb_thread_id[%u] ",
		gr_dev->engine_id, lce_dev->engine_id,
		gr_fb_thread_id, lce_fb_thread_id);

	return (gr_fb_thread_id == lce_fb_thread_id);
}

static u32 ga10b_grmgr_get_local_gr_syspipe_index(struct gk20a *g,
		u32 gr_syspipe_id)
{
	u32 usable_gr_syspipe_mask = g->mig.usable_gr_syspipe_mask;
	u32 local_gr_syspipe_index = 0U;
	u32 gr_syspipe_mask = (usable_gr_syspipe_mask &
		nvgpu_safe_sub_u32(BIT32(gr_syspipe_id), 1U));

	while (gr_syspipe_mask != 0U) {
		u32 bit_position = nvgpu_safe_sub_u32(
			(u32)nvgpu_ffs(gr_syspipe_mask), 1UL);
		++local_gr_syspipe_index;
		gr_syspipe_mask ^= BIT32(bit_position);
	}

	nvgpu_log(g, gpu_dbg_mig,
		"usable_gr_syspipe_mask[%x] gr_syspipe_id[%u] "
			"local_gr_syspipe_index[%u] ",
		usable_gr_syspipe_mask, gr_syspipe_id,
		local_gr_syspipe_index);

	return local_gr_syspipe_index;
}

static u32 ga10b_grmgr_get_gr_syspipe_id_from_local_gr_syspipe_index(
		struct gk20a *g,
		u32 local_gr_syspipe_index)
{
	u32 usable_gr_syspipe_mask = g->mig.usable_gr_syspipe_mask;
	u32 temp_gr_syspipe_index = 0U;
	u32 gr_syspipe_id = 0U;
	u32 max_allowed_syspipe_index = nvgpu_safe_add_u32(
		local_gr_syspipe_index, 1U);

	nvgpu_assert(max_allowed_syspipe_index <= g->mig.usable_gr_syspipe_count);

	while (temp_gr_syspipe_index < max_allowed_syspipe_index) {
		gr_syspipe_id = nvgpu_safe_sub_u32(
			(u32)nvgpu_ffs(usable_gr_syspipe_mask), 1UL);
		++temp_gr_syspipe_index;
		usable_gr_syspipe_mask ^= BIT32(gr_syspipe_id);
	}

	nvgpu_log(g, gpu_dbg_mig,
		"usable_gr_syspipe_mask[%x] local_gr_syspipe_index[%u] "
			"num_gr[%u] gr_syspipe_id[%u]",
		g->mig.usable_gr_syspipe_mask, local_gr_syspipe_index,
		g->mig.usable_gr_syspipe_count, gr_syspipe_id);

	return gr_syspipe_id;
}

static u32 ga10b_grmgr_get_num_gr_syspipe_enabled(struct gk20a *g,
		u32 start_gr_syspipe_id,
		u32 num_gpc)
{
	u32 usable_gr_syspipe_mask = g->mig.usable_gr_syspipe_mask;
	u32 expected_gr_syspipe_mask = ((nvgpu_safe_sub_u32(BIT32(num_gpc), 1U)) <<
		start_gr_syspipe_id);
	u32 gr_syspipe_enabled_mask = (usable_gr_syspipe_mask &
		expected_gr_syspipe_mask);
	u32 gr_syspipe_enabled_count = 0U;

	while (gr_syspipe_enabled_mask != 0U) {
		u32 bit_pos = nvgpu_safe_sub_u32(
					(u32)nvgpu_ffs(gr_syspipe_enabled_mask), 1UL);
		gr_syspipe_enabled_mask ^= BIT32(bit_pos);
		++gr_syspipe_enabled_count;
	}

	nvgpu_log(g, gpu_dbg_mig,
		"start_gr_syspipe_id[%u] num_gpc[%u] "
			"usable_gr_syspipe_mask[%x] expected_gr_syspipe_mask[%x] "
			"gr_syspipe_enabled_count[%u] ",
		start_gr_syspipe_id, num_gpc, usable_gr_syspipe_mask,
		expected_gr_syspipe_mask, gr_syspipe_enabled_count);

	return gr_syspipe_enabled_count;
}

static int ga10b_grmgr_get_gpu_instance(struct gk20a *g,
		u32 config_id,
		struct nvgpu_gpu_instance gpu_instance[],
		struct nvgpu_gpc gpcs[],
		u32 *num_gpu_instances)
{
	int err = 0;
	u32 num_gr;
	const struct nvgpu_device *lces[NVGPU_MIG_MAX_ENGINES] = { };
	u32 num_lce;
	struct nvgpu_gr_syspipe *gr_syspipe;
	u32 index;
	u32 physical_ce_id;
	u32 logical_gpc_id;
	bool is_memory_partition_supported;
	u32 temp_num_gpu_instances;
	u32 lce_mask;
	u32 gpc_mask;
	u32 temp_lce_mask;
	u32 temp_gpc_mask;
	u32 temp_lce_cnt;
	u32 temp_gpc_cnt;
	u32 local_gr_syspipe_index;
	u32 gr_syspipe_enabled_count = 0U;
	u32 veid_count_per_gpc;
	u32 veid_start_offset = 0U;
	u32 num_gpc = g->mig.gpc_count;
	u32 gpu_instance_gpcgrp_id[NVGPU_MIG_MAX_GPU_INSTANCES];
	const struct nvgpu_gpu_instance_static_config *gpu_instance_static_config;
	u32 *gr_instance_id_per_swizzid;
	const struct nvgpu_mig_gpu_instance_config *mig_gpu_instance_config =
		g->ops.grmgr.get_mig_config_ptr(g);
	u32 allowed_swizzid_size = g->ops.grmgr.get_allowed_swizzid_size(g);
	u32 max_subctx_count = g->ops.gr.init.get_max_subctx_count();
	u32 max_fbps_count = g->mig.max_fbps_count;

	if ((mig_gpu_instance_config == NULL) || (num_gpc > NVGPU_MIG_MAX_GPCS)) {
		nvgpu_err(g,"mig_gpu_instance_config NULL "
			"or (num_gpc > NVGPU_MIG_MAX_GPCS)[%u %u] ",
			num_gpc, NVGPU_MIG_MAX_GPCS);
		return -EINVAL;
	}

	temp_num_gpu_instances = mig_gpu_instance_config->gpu_instance_config[config_id].num_gpu_instances;

	if ((config_id >= mig_gpu_instance_config->num_config_supported) ||
		(gpu_instance == NULL) || (num_gpu_instances == NULL) ||
		(temp_num_gpu_instances > g->ops.grmgr.get_max_sys_pipes(g))) {
		nvgpu_err(g,
			"[Invalid param] conf_id[%u %u] num_gpu_inst[%u %u] ",
			config_id, mig_gpu_instance_config->num_config_supported,
			temp_num_gpu_instances, g->ops.grmgr.get_max_sys_pipes(g));
		return -EINVAL;
	}

	gpu_instance_static_config =
		mig_gpu_instance_config->gpu_instance_config[config_id].gpu_instance_static_config;
	nvgpu_log(g, gpu_dbg_mig,
		"temp_num_gpu_instances[%u] config_name[%s] ",
		temp_num_gpu_instances,
		mig_gpu_instance_config->gpu_instance_config[config_id].config_name);

	/* TODO : Enable SMC memory partition support. */
	is_memory_partition_supported = false; /*(allowed_swizzid_size > 1U); */

	num_lce = nvgpu_device_get_async_copies(g, lces, NVGPU_MIG_MAX_ENGINES);
	nvgpu_assert(num_lce > 0U);

	num_gr = g->mig.usable_gr_syspipe_count;
	if (num_gr < temp_num_gpu_instances) {
		nvgpu_err(g, "(num_gr < temp_num_gpu_instances)[%u %u]",
			num_gr, temp_num_gpu_instances);
		return -EINVAL;
	}

	lce_mask = nvgpu_safe_cast_u64_to_u32(nvgpu_safe_sub_u64(BIT64(num_lce), 1ULL));
	gpc_mask = nvgpu_safe_cast_u64_to_u32(nvgpu_safe_sub_u64(BIT64(num_gpc), 1ULL));

	gr_instance_id_per_swizzid = (u32 *)nvgpu_kzalloc(g,
		nvgpu_safe_mult_u32(sizeof(u32), allowed_swizzid_size));
	if (gr_instance_id_per_swizzid == NULL) {
		nvgpu_err(g, "(gr_instance_id_per_swizzid- kzalloc failed");
		return -ENOMEM;
	}

	nvgpu_log(g, gpu_dbg_mig, "num_gr[%u] num_lce[%u] ", num_gr, num_lce);

	nvgpu_assert(max_subctx_count > 0U);

	veid_count_per_gpc = (max_subctx_count / num_gpc);

	nvgpu_log(g, gpu_dbg_mig, "veid_count_per_gpc[%u] num_gpc[%u] ",
		veid_count_per_gpc, num_gpc);

	for (index = 0U; index < temp_num_gpu_instances; index++) {
		u32 gr_syspipe_id =
			gpu_instance_static_config[index].gr_syspipe_id;

		local_gr_syspipe_index =
			ga10b_grmgr_get_local_gr_syspipe_index(g, gr_syspipe_id);
		if (local_gr_syspipe_index >= num_gr) {
			nvgpu_err(g,
				"GR index config mismatch, "
				"num_gr[%d] actual_gr_index[%u] ",
				num_gr, local_gr_syspipe_index);
			err = -EINVAL;
			goto exit;
		}

		if ((g->mig.usable_gr_syspipe_instance_id[local_gr_syspipe_index] !=
				gr_syspipe_id)) {
			nvgpu_err(g,
				"GR SYSPIPE ID mismatch expected[%u] actual[%u] "
				"or (gr_engine_info == NULL) ",
				gr_syspipe_id,
				g->mig.usable_gr_syspipe_instance_id[local_gr_syspipe_index]);
			err = -EINVAL;
			goto exit;
		}

		gr_syspipe = &gpu_instance[index].gr_syspipe;

		if (g->ops.grmgr.get_gpc_instance_gpcgrp_id(g,
				gpu_instance_static_config[index].gpu_instance_id,
				gpu_instance_static_config[index].gr_syspipe_id,
				&gpu_instance_gpcgrp_id[index]) != 0) {
			nvgpu_err(g,
				"g->ops.grmgr.get_gpc_instance_gpcgrp_id -failed");
			err = -EINVAL;
			goto exit;
		}

		temp_gpc_cnt = 0U;
		temp_gpc_mask = gpc_mask;
		gr_syspipe->num_gpc = 0;
		while (temp_gpc_mask && (temp_gpc_cnt <
				(gpu_instance_static_config[index].num_gpc))) {

			logical_gpc_id = nvgpu_safe_sub_u32(
					(u32)nvgpu_ffs(temp_gpc_mask), 1UL);

			if ((gpcs[logical_gpc_id].gpcgrp_id ==
					gpu_instance_gpcgrp_id[index]) ||
					(temp_num_gpu_instances == 1U)) {
				gr_syspipe->gpcs[temp_gpc_cnt].logical_id =
					gpcs[logical_gpc_id].logical_id;
				gr_syspipe->gpcs[temp_gpc_cnt].physical_id =
					gpcs[logical_gpc_id].physical_id;
				gr_syspipe->gpcs[temp_gpc_cnt].gpcgrp_id =
					gpcs[logical_gpc_id].gpcgrp_id;

				gpc_mask ^= BIT32(logical_gpc_id);

				nvgpu_log(g, gpu_dbg_mig,
					"gpu_instance_id[%u] "
						"gr_instance_id[%u] gr_syspipe_id[%u] "
						"gpc_local_id[%u] gpc_logical_id[%u] "
						"gpc_physical_id[%u]  gpc_grpid[%u] "
						"free_gpc_mask[%x] gr_syspipe_id[%u]",
					gpu_instance_static_config[index].gpu_instance_id,
					index,
					gpu_instance_static_config[index].gr_syspipe_id,
					temp_gpc_cnt,
					gr_syspipe->gpcs[temp_gpc_cnt].logical_id,
					gr_syspipe->gpcs[temp_gpc_cnt].physical_id,
					gr_syspipe->gpcs[temp_gpc_cnt].gpcgrp_id,
					gpc_mask,
					gpu_instance_static_config[index].gr_syspipe_id);

				++temp_gpc_cnt;
				++gr_syspipe->num_gpc;
			}
			temp_gpc_mask ^= BIT32(logical_gpc_id);
		}

		if (gr_syspipe->num_gpc !=
				gpu_instance_static_config[index].num_gpc) {
			nvgpu_err(g,
				"GPC config mismatch, [%d] gpu_instance_id[%u] "
					"gr_syspipe_id[%u] available[%u] expected[%u] ",
				index,
				gpu_instance_static_config[index].gpu_instance_id,
				gpu_instance_static_config[index].gr_syspipe_id,
				gr_syspipe->num_gpc,
				gpu_instance_static_config[index].num_gpc);
			err = -EINVAL;
			goto exit;
		}

		gpu_instance[index].gpu_instance_id =
			gpu_instance_static_config[index].gpu_instance_id;
		gr_syspipe->gr_instance_id = gr_instance_id_per_swizzid[
			gpu_instance[index].gpu_instance_id]++;
		gr_syspipe->gr_syspipe_id =
			gpu_instance_static_config[index].gr_syspipe_id;
		gr_syspipe->gpc_mask = nvgpu_safe_sub_u32(
			BIT32(gr_syspipe->num_gpc), 1U);
		gr_syspipe->gr_dev = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS,
						      gr_syspipe->gr_syspipe_id);
		nvgpu_assert(gr_syspipe->gr_dev != NULL);

		gr_syspipe->max_veid_count_per_tsg = nvgpu_safe_mult_u32(
			veid_count_per_gpc, gr_syspipe->num_gpc);

		/* Add extra VEIDs in 1st gpu instance */
		if (index == 0U) {
			gr_syspipe->max_veid_count_per_tsg =
				nvgpu_safe_add_u32(gr_syspipe->max_veid_count_per_tsg,
					(max_subctx_count % num_gpc));
		}

		gr_syspipe->veid_start_offset = veid_start_offset;
		veid_start_offset = nvgpu_safe_add_u32(
			veid_start_offset,
			gr_syspipe->max_veid_count_per_tsg);

		gpu_instance[index].is_memory_partition_supported =
			is_memory_partition_supported;
		gpu_instance[index].gpu_instance_type = NVGPU_MIG_TYPE_MIG;

		if (g->mig.is_nongr_engine_sharable ||
				(temp_num_gpu_instances == 1U)) {
			gpu_instance[index].num_lce = num_lce;
			nvgpu_memcpy((u8 *)gpu_instance[index].lce_devs,
				     (u8 *)lces,
				     nvgpu_safe_mult_u32(sizeof(*lces), num_lce));
		} else {
			temp_lce_cnt = 0U;
			temp_lce_mask = lce_mask;
			gr_syspipe_enabled_count =
				ga10b_grmgr_get_num_gr_syspipe_enabled(g,
					gr_syspipe->gr_syspipe_id, gr_syspipe->num_gpc);
			while (temp_lce_mask &&
					(temp_lce_cnt < gr_syspipe_enabled_count)) {
				u32 gr_syspipe_id =
					ga10b_grmgr_get_gr_syspipe_id_from_local_gr_syspipe_index(g,
						nvgpu_safe_add_u32(local_gr_syspipe_index,
							temp_lce_cnt));
				physical_ce_id = nvgpu_safe_sub_u32(
					(u32)nvgpu_ffs(temp_lce_mask), 1UL);
				if (ga10b_grmgr_is_syspipe_lce(g,
						nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS,
							gr_syspipe_id),
						lces[physical_ce_id])) {
					gpu_instance[index].lce_devs[temp_lce_cnt] =
						lces[physical_ce_id];
					++temp_lce_cnt;
					lce_mask ^= BIT32(physical_ce_id);
					nvgpu_log(g, gpu_dbg_mig,
						"[%d] gpu_instance_id[%u] "
							"gr_instance_id[%u] gr_syspipe_id[%u] "
							"gr_syspipe_id[%u] "
							"gr_engine_id [%u] lce_engine_id[%u] "
							"gr_syspipe_enabled_count[%u] ",
						index,
						gpu_instance[index].gpu_instance_id,
						gr_syspipe->gr_instance_id,
						gr_syspipe->gr_syspipe_id,
						gr_syspipe_id,
						nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS,
								 gr_syspipe_id)->engine_id,
						lces[physical_ce_id]->engine_id,
						gr_syspipe_enabled_count);
				}
				temp_lce_mask ^= BIT32(physical_ce_id);
			}
			gpu_instance[index].num_lce = temp_lce_cnt;

			if (index == nvgpu_safe_sub_u32(temp_num_gpu_instances, 1U)) {
				u32 gpu_instance_id = 0U;
				while ((lce_mask != 0U) &&
						(temp_lce_cnt < NVGPU_MIG_MAX_ENGINES) &&
						(gpu_instance_id < temp_num_gpu_instances)) {
					struct nvgpu_gr_syspipe *local_gr_syspipe =
						&gpu_instance[gpu_instance_id].gr_syspipe;
					physical_ce_id = nvgpu_safe_sub_u32(
						(u32)nvgpu_ffs(lce_mask), 1UL);
					temp_lce_cnt = gpu_instance[gpu_instance_id].num_lce;
					gpu_instance[gpu_instance_id].lce_devs[temp_lce_cnt] =
						lces[physical_ce_id];
					lce_mask ^= BIT32(physical_ce_id);
					++temp_lce_cnt;
					gpu_instance[gpu_instance_id].num_lce = temp_lce_cnt;
					nvgpu_log(g, gpu_dbg_mig,
						"Added Extra LCEs to %u GPU instance "
							"gpu_instance_id[%u] "
							"gr_instance_id[%u] gr_syspipe_id[%u] "
							"gr_engine_id [%u] lce_engine_id[%u] "
							"temp_lce_cnt[%u] ",
						gpu_instance_id,
						gpu_instance[gpu_instance_id].gpu_instance_id,
						local_gr_syspipe->gr_instance_id,
						local_gr_syspipe->gr_syspipe_id,
						local_gr_syspipe->gr_dev->engine_id,
						lces[physical_ce_id]->engine_id,
						temp_lce_cnt);
					++gpu_instance_id;
					gpu_instance_id %= temp_num_gpu_instances;
				}
			}
		}

		gpu_instance[index].fbp_l2_en_mask =
			nvgpu_kzalloc(g,
				nvgpu_safe_mult_u64(max_fbps_count, sizeof(u32)));
		if (gpu_instance[index].fbp_l2_en_mask == NULL) {
			nvgpu_err(g,
				"gpu_instance[%d].fbp_l2_en_mask aloc failed",
				index);
			err = -ENOMEM;
			goto exit;
		}

		if (gpu_instance[index].is_memory_partition_supported == false) {
			u32 tmp_fbp_index = 0;

			gpu_instance[index].num_fbp = g->mig.gpu_instance[0].num_fbp;
			gpu_instance[index].fbp_en_mask = g->mig.gpu_instance[0].fbp_en_mask;
			nvgpu_memcpy((u8 *)gpu_instance[index].fbp_l2_en_mask,
				(u8 *)g->mig.gpu_instance[0].fbp_l2_en_mask,
					nvgpu_safe_mult_u64(max_fbps_count, sizeof(u32)));

			while (tmp_fbp_index < gpu_instance[index].num_fbp) {
				gpu_instance[index].fbp_mappings[tmp_fbp_index] = tmp_fbp_index;
				tmp_fbp_index = nvgpu_safe_add_u32(tmp_fbp_index, 1U);
			}
		} else {
			/* SMC Memory partition is not yet supported */
			nvgpu_assert(
				gpu_instance[index].is_memory_partition_supported == false);
		}

		nvgpu_log(g, gpu_dbg_mig,
			"[%d] gpu_instance_id[%u] gr_instance_id[%u] "
				"gr_syspipe_id[%u] num_gpc[%u] gr_engine_id[%u] "
				"max_veid_count_per_tsg[%u] veid_start_offset[%u] "
				"veid_end_offset[%u] "
				"is_memory_partition_support[%d] num_lce[%u] "
				"gr_syspipe_enabled_count[%u] "
				"max_fbps_count[%u] num_fbp[%u] fbp_en_mask [0x%x] ",
			index, gpu_instance[index].gpu_instance_id,
			gr_syspipe->gr_instance_id,
			gr_syspipe->gr_syspipe_id,
			gr_syspipe->num_gpc,
			gr_syspipe->gr_dev->engine_id,
			gr_syspipe->max_veid_count_per_tsg,
			gr_syspipe->veid_start_offset,
			nvgpu_safe_sub_u32(
				nvgpu_safe_add_u32(gr_syspipe->veid_start_offset,
					gr_syspipe->max_veid_count_per_tsg), 1U),
			gpu_instance[index].is_memory_partition_supported,
			gpu_instance[index].num_lce,
			gr_syspipe_enabled_count,
			max_fbps_count,
			gpu_instance[index].num_fbp,
			gpu_instance[index].fbp_en_mask);
	}

	*num_gpu_instances = temp_num_gpu_instances;

exit:
	nvgpu_kfree(g, gr_instance_id_per_swizzid);
	return err;
}

static void ga10b_grmgr_set_smc_state(struct gk20a *g, bool enable)
{
	u32 smc_state = 0U;

	smc_state = nvgpu_readl(g, smcarb_sys_pipe_info_r());

	if (smcarb_sys_pipe_info_mode_v(smc_state) != enable) {
		smc_state &= ~smcarb_sys_pipe_info_mode_m();
		if (enable) {
			smc_state |= smcarb_sys_pipe_info_mode_f(
				smcarb_sys_pipe_info_mode_smc_v());
		} else {
			smc_state |= smcarb_sys_pipe_info_mode_f(
				smcarb_sys_pipe_info_mode_legacy_v());
		}
		nvgpu_writel(g, smcarb_sys_pipe_info_r(), smc_state);
		nvgpu_log(g, gpu_dbg_mig, "MIG boot reg_val[%x] enable[%d]",
			smc_state, enable);
	}
}

static int ga10b_grmgr_config_gpc_smc_map(struct gk20a *g, bool enable)
{
	u32 physical_gpc_id;
	u32 local_gpc_id;
	u32 logical_gpc_id = 0U;
	u32 gpu_instance_id;
	u32 gr_sys_pipe_id;
	u32 ugpu_id;
	u32 reg_val;
	struct nvgpu_gr_syspipe *gr_syspipe;

	for (gpu_instance_id = 0; gpu_instance_id < g->mig.num_gpu_instances;
			++gpu_instance_id) {

		if (!nvgpu_grmgr_is_mig_type_gpu_instance(
				&g->mig.gpu_instance[gpu_instance_id])) {
			nvgpu_log(g, gpu_dbg_mig, "skip physical instance[%u]",
				gpu_instance_id);
			/* Skip physical device gpu instance when MIG is enabled */
			continue;
		}

		gr_syspipe = &g->mig.gpu_instance[gpu_instance_id].gr_syspipe;
		gr_sys_pipe_id = gr_syspipe->gr_syspipe_id;
		local_gpc_id = 0;

		while (local_gpc_id < gr_syspipe->num_gpc) {
			ugpu_id = gr_syspipe->gpcs[local_gpc_id].gpcgrp_id;
			physical_gpc_id = gr_syspipe->gpcs[local_gpc_id].physical_id;
			logical_gpc_id = gr_syspipe->gpcs[local_gpc_id].logical_id;

			reg_val = nvgpu_readl(g,
				smcarb_smc_partition_gpc_map_r(logical_gpc_id));

			if (enable == false) {
				reg_val = set_field(reg_val,
					smcarb_smc_partition_gpc_map_valid_m(),
					smcarb_smc_partition_gpc_map_valid_f(
						smcarb_smc_partition_gpc_map_valid_false_v()));
			}
			else if (enable && (physical_gpc_id ==
					smcarb_smc_partition_gpc_map_physical_gpc_id_v(reg_val)) &&
					(ugpu_id == smcarb_smc_partition_gpc_map_ugpu_id_v(reg_val))) {
				reg_val = set_field(reg_val,
					smcarb_smc_partition_gpc_map_sys_pipe_local_gpc_id_m(),
					smcarb_smc_partition_gpc_map_sys_pipe_local_gpc_id_f(
						local_gpc_id));
				reg_val = set_field(reg_val,
					smcarb_smc_partition_gpc_map_sys_pipe_id_m(),
					smcarb_smc_partition_gpc_map_sys_pipe_id_f(
						gr_sys_pipe_id));
				reg_val = set_field(reg_val,
					smcarb_smc_partition_gpc_map_valid_m(),
					smcarb_smc_partition_gpc_map_valid_f(
						smcarb_smc_partition_gpc_map_valid_true_v()));
			} else {
				nvgpu_err(g, "wrong mig config found [%u %u %u %u %u]",
					logical_gpc_id,
					physical_gpc_id,
					smcarb_smc_partition_gpc_map_physical_gpc_id_v(reg_val),
					ugpu_id,
					smcarb_smc_partition_gpc_map_ugpu_id_v(reg_val));
				return -EINVAL;
			}

			nvgpu_writel(g, smcarb_smc_partition_gpc_map_r(logical_gpc_id), reg_val);
			nvgpu_log(g, gpu_dbg_mig,
				"[%d] gpu_instance_id[%u] gr_instance_id[%u] "
					"gr_syspipe_id[%u] logical_gpc_id[%u] physical_gpc_id[%u] "
					" local_gpc_id[%u] gpcgrp_id[%u] reg_val[%x] enable[%d] ",
				gpu_instance_id, g->mig.gpu_instance[gpu_instance_id].gpu_instance_id,
				gr_syspipe->gr_instance_id,
				gr_sys_pipe_id, logical_gpc_id, physical_gpc_id,
				local_gpc_id, ugpu_id, reg_val, enable);
			++local_gpc_id;
		}
	}

	if (g->ops.priv_ring.config_gpc_rs_map(g, enable) != 0) {
		nvgpu_err(g, "g->ops.priv_ring.config_gpc_rs_map-failed");
		return -EINVAL;
	}

	if (g->ops.fb.set_smc_eng_config(g, enable) != 0) {
		nvgpu_err(g, "g->ops.fb.set_smc_eng_config-failed");
		return -EINVAL;
	}

	return 0;
}

int ga10b_grmgr_init_gr_manager(struct gk20a *g)
{
	struct nvgpu_gr_syspipe *gr_syspipe;
	int err;
	u32 index;
	u32 num_gpu_instances;
	const struct nvgpu_device *gr_dev = NULL;
	u32 max_veid_count_per_tsg = g->ops.gr.init.get_max_subctx_count();

	/* Init physical device gpu instance */
	err = nvgpu_init_gr_manager(g);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_init_gr_manager-failed[%d]", err);
		return err;
	}

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG) ||
			(g->mig.gpc_count < 2U)) {
		/*
		 * Fall back to 1 GPU instance.
		 * It can be Physical/legacy or MIG mode based NVGPU_SUPPORT_MIG.
		 */
		nvgpu_log(g, gpu_dbg_mig,
			"Fall back to 1 GPU instance - mode[%s]",
			(nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG) ?
				"MIG_Physical" : "Physical"));
		return 0;
	}

	g->mig.is_nongr_engine_sharable = false;
	if (max_veid_count_per_tsg < 64U) {
		nvgpu_err(g,
			"re-generate mig gpu instance config based on floorsweep config veid[%u]",
			max_veid_count_per_tsg);
		return -EINVAL;
	}

	err = ga10b_grmgr_get_gpu_instance(g, g->mig.current_gpu_instance_config_id,
			&g->mig.gpu_instance[1],
			g->mig.gpu_instance[0].gr_syspipe.gpcs,
			&num_gpu_instances);
	if (err != 0) {
		nvgpu_err(g, "ga10b_grmgr_get_gpu_instance-failed[%d]", err);
		return err;
	}

	g->mig.num_gpu_instances = nvgpu_safe_add_u32(num_gpu_instances, 1U);

	g->mig.max_gr_sys_pipes_supported = g->ops.grmgr.get_max_sys_pipes(g);

	g->mig.gr_syspipe_en_mask = 0;
	g->mig.num_gr_sys_pipes_enabled = 0U;

	for (index = 0U; index  < g->mig.num_gpu_instances; index++) {
		if (!nvgpu_grmgr_is_mig_type_gpu_instance(
				&g->mig.gpu_instance[index])) {
			/* Skip physical device gpu instance when MIG is enabled */
			nvgpu_log(g, gpu_dbg_mig, "skip physical instance[%u]", index);
			continue;
		}
		gr_syspipe = &g->mig.gpu_instance[index].gr_syspipe;
		g->mig.gr_syspipe_en_mask |= BIT32(gr_syspipe->gr_syspipe_id);

		gr_dev = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS,
			gr_syspipe->gr_syspipe_id);

		nvgpu_assert(gr_dev != NULL);

		/*
		 * HW recommended to put GR engine into reset before programming
		 * config_gpc_rs_map (ga10b_grmgr_config_gpc_smc_map()).
		 */
		err = g->ops.mc.enable_dev(g, gr_dev, false);
		if (err != 0) {
			nvgpu_err(g, "GR engine reset failed gr_syspipe_id[%u %u]",
				gr_syspipe->gr_syspipe_id, gr_dev->inst_id);
			return err;
		}

		++g->mig.num_gr_sys_pipes_enabled;
	}

	g->mig.current_gr_syspipe_id = NVGPU_MIG_INVALID_GR_SYSPIPE_ID;
	nvgpu_mutex_init(&g->mig.gr_syspipe_lock);

	err = ga10b_grmgr_config_gpc_smc_map(g, true);
	if (err != 0) {
		nvgpu_err(g, "ga10b_grmgr_config_gpc_smc_map-failed[%d]", err);
		return err;
	}

	err = g->ops.fb.config_veid_smc_map(g, true);
	if (err != 0) {
		return err;
	}

	err = g->ops.fb.set_remote_swizid(g, true);
	if (err != 0) {
		nvgpu_err(g, "g->ops.fb.set_remote_swizid-failed[%d]", err);
		return err;
	}

	ga10b_grmgr_set_smc_state(g, true);

	nvgpu_log(g, gpu_dbg_mig,
		"MIG boot success num_gpu_instances[%u] "
			"num_gr_sys_pipes_enabled[%u] gr_syspipe_en_mask[%x]",
		g->mig.num_gpu_instances,
		g->mig.num_gr_sys_pipes_enabled,
		g->mig.gr_syspipe_en_mask);

	return err;
}

u32 ga10b_grmgr_get_max_sys_pipes(struct gk20a *g)
{
	(void)g;
	return smcarb_max_partitionable_sys_pipes_v();
}

u32 ga10b_grmgr_get_allowed_swizzid_size(struct gk20a *g)
{
	(void)g;
	return smcarb_allowed_swizzid__size1_v();
}

int ga10b_grmgr_get_gpc_instance_gpcgrp_id(struct gk20a *g,
		u32 gpu_instance_id, u32 gr_syspipe_id, u32 *gpcgrp_id)
{

	if ((gpu_instance_id >= smcarb_allowed_swizzid__size1_v()) ||
		(gr_syspipe_id >= g->ops.grmgr.get_max_sys_pipes(g)) ||
		(gpcgrp_id == NULL)) {
		nvgpu_err(g,
			"[Invalid_param] gr_syspipe_id[%u %u] gpu_instance_id[%u %u] "
				"or gpcgrp_id == NULL ",
				gr_syspipe_id, g->ops.grmgr.get_max_sys_pipes(g),
				gpu_instance_id, smcarb_allowed_swizzid__size1_v());
		return -EINVAL;
	}

	*gpcgrp_id = 0U;
	nvgpu_log(g, gpu_dbg_mig,
			"Found [%u] gpcgrp id for gpu_instance_id[%u] "
				"gr_syspipe_id[%u] ",
			*gpcgrp_id,
			gpu_instance_id,
			gr_syspipe_id);
	return 0;
}

int ga10b_grmgr_remove_gr_manager(struct gk20a *g)
{
	int err;
	u32 index;

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		/* Fall back to non MIG gr manager remove ops - noop */
		return 0;
	}

	ga10b_grmgr_set_smc_state(g, false);
	err = ga10b_grmgr_config_gpc_smc_map(g, false);
	err |= g->ops.fb.config_veid_smc_map(g, false);
	err |= g->ops.fb.set_remote_swizid(g, false);

	/* Free only MIG instance fbp_l2_en_mask */
	for (index = 1U; index  < g->mig.num_gpu_instances; index++) {
		if (g->mig.gpu_instance[index].fbp_l2_en_mask !=
				NULL) {
			nvgpu_kfree(g,
				g->mig.gpu_instance[index].fbp_l2_en_mask);
			g->mig.gpu_instance[index].fbp_l2_en_mask = NULL;
			g->mig.gpu_instance[index].num_fbp = 0U;
			g->mig.gpu_instance[index].fbp_en_mask = 0U;
		}
	}
	nvgpu_mutex_destroy(&g->mig.gr_syspipe_lock);

	(void)memset(&g->mig, 0, sizeof(struct nvgpu_mig));

	nvgpu_log(g, gpu_dbg_mig, "success");

	return err;
}

int ga10b_grmgr_get_mig_gpu_instance_config(struct gk20a *g,
		const char **config_name,
		u32 *num_config_supported) {

	u32 config_id;
	const struct nvgpu_mig_gpu_instance_config *mig_gpu_instance_config =
		g->ops.grmgr.get_mig_config_ptr(g);

	if (num_config_supported == NULL) {
		return -EINVAL;
	}

	*num_config_supported = mig_gpu_instance_config->num_config_supported;

	if (config_name != NULL) {
		for (config_id = 0U; config_id < *num_config_supported;
			config_id++) {
				config_name[config_id] =
					mig_gpu_instance_config->gpu_instance_config[config_id].config_name;
		}
	}
	return 0;
}

#endif

void ga10b_grmgr_load_smc_arb_timestamp_prod(struct gk20a *g)
{
	u32 reg_val;

	/* set prod value for smc arb timestamp ctrl disable tick */
	reg_val = nvgpu_readl(g, smcarb_timestamp_ctrl_r());
	reg_val = set_field(reg_val,
			smcarb_timestamp_ctrl_disable_tick_m(),
                        smcarb_timestamp_ctrl_disable_tick__prod_f());
	nvgpu_writel(g, smcarb_timestamp_ctrl_r(), reg_val);

}

int ga10b_grmgr_discover_gpc_ids(struct gk20a *g,
		u32 num_gpc, struct nvgpu_gpc *gpcs)
{
	u32 logical_gpc_id;
	u32 reg_val;

	if (gpcs == NULL) {
		nvgpu_err(g, "no valid gpcs ptr");
		return -EINVAL;
	}

	for (logical_gpc_id = 0U; logical_gpc_id < num_gpc; logical_gpc_id++) {
		reg_val = nvgpu_readl(g,
			smcarb_smc_partition_gpc_map_r(logical_gpc_id));
		gpcs[logical_gpc_id].logical_id = logical_gpc_id;
		gpcs[logical_gpc_id].physical_id =
			smcarb_smc_partition_gpc_map_physical_gpc_id_v(reg_val);
		gpcs[logical_gpc_id].gpcgrp_id =
			smcarb_smc_partition_gpc_map_ugpu_id_v(reg_val);
		nvgpu_log(g, gpu_dbg_mig,
			"index[%u] gpc_logical_id[%u] "
				"gpc_physical_id[%u]  gpc_grpid[%u] ",
			logical_gpc_id,
			gpcs[logical_gpc_id].logical_id,
			gpcs[logical_gpc_id].physical_id,
			gpcs[logical_gpc_id].gpcgrp_id);
	}
	return 0;
}
