/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/grmgr.h>

#include "gr_config_priv.h"

static void gr_config_init_pes_tpc(struct gk20a *g,
				struct nvgpu_gr_config *config,
				u32 gpc_index)
{
	u32 pes_index;
	u32 pes_tpc_mask;
	u32 pes_tpc_count;

	for (pes_index = 0; pes_index < config->pe_count_per_gpc;
			    pes_index++) {
		pes_tpc_mask = g->ops.gr.config.get_pes_tpc_mask(g,
					config, gpc_index, pes_index);
		pes_tpc_count = hweight32(pes_tpc_mask);

		/* detect PES presence by seeing if there are
		 * TPCs connected to it.
		 */
		if (pes_tpc_count != 0U) {
			config->gpc_ppc_count[gpc_index] = nvgpu_safe_add_u32(
				config->gpc_ppc_count[gpc_index], 1U);
		}

		config->pes_tpc_count[pes_index][gpc_index] = pes_tpc_count;
		config->pes_tpc_mask[pes_index][gpc_index] = pes_tpc_mask;
	}
}

static void gr_config_init_gpc_skip_mask(struct nvgpu_gr_config *config,
					u32 gpc_index)
{
	u32 pes_heavy_index;
	u32 gpc_new_skip_mask = 0U;
	u32 pes_tpc_cnt = 0U, pes_tpc_mask = 0U;

	if (config->pe_count_per_gpc <= 1U) {
		goto skip_mask_end;
	}

	pes_tpc_cnt = nvgpu_safe_add_u32(
		config->pes_tpc_count[0][gpc_index],
		config->pes_tpc_count[1][gpc_index]);

	pes_heavy_index =
		(config->pes_tpc_count[0][gpc_index] >
			config->pes_tpc_count[1][gpc_index]) ? 0U : 1U;

	if ((pes_tpc_cnt == 5U) || ((pes_tpc_cnt == 4U) &&
		   (config->pes_tpc_count[0][gpc_index] !=
		    config->pes_tpc_count[1][gpc_index]))) {
		pes_tpc_mask = nvgpu_safe_sub_u32(
			config->pes_tpc_mask[pes_heavy_index][gpc_index], 1U);
		gpc_new_skip_mask =
			config->pes_tpc_mask[pes_heavy_index][gpc_index] ^
			   (config->pes_tpc_mask[pes_heavy_index][gpc_index] &
			   pes_tpc_mask);
	}

skip_mask_end:
	config->gpc_skip_mask[gpc_index] = gpc_new_skip_mask;
}

static int gr_config_init_gpc_rop_config(struct gk20a *g,
		struct nvgpu_gr_config *config)
{
	u32 rop_cnt = 0U;
	u32 max_rop_per_gpc_size;
	int err = 0;
	u32 gpc_index = 0U, rop_index = 0U, i = 0U;
	u32 gpc_phys_id;
	u32 cur_gr_instance = nvgpu_gr_get_cur_instance_id(g);

	/*
	 * Allocate memory to store the per GPC ROP mask. The ROP masks will
	 * be indexed using logical gpc id, so allocate memory based on the
	 * number of non-FSed GPCs, which is config->gpc_count.
	 */
	config->gpc_rop_mask = nvgpu_kzalloc(g,
			nvgpu_safe_mult_u64((size_t)config->gpc_count,
				sizeof(u32)));
	if (config->gpc_rop_mask == NULL) {
		nvgpu_err(g, "alloc gpc_rop_mask failed");
		err = -ENOMEM;
		goto rop_mask_alloc_fail;
	}
	/*
	 * This structure holds the logical id for a ROP chiplet within a
	 * GPC. The GPC is indexed using logical id and the ROP is indexed using
	 * physical id.
	 */
	config->gpc_rop_logical_id_map = nvgpu_kzalloc(g,
			nvgpu_safe_mult_u64((size_t)config->gpc_count,
				sizeof(u32 *)));
	if (config->gpc_rop_logical_id_map == NULL) {
		nvgpu_err(g, "alloc gpc_rop_logical_id_map failed");
		err = -ENOMEM;
		goto rop_logical_id_map_alloc_fail;
	}

	config->max_rop_per_gpc_count = g->ops.top.get_max_rop_per_gpc(g);
	max_rop_per_gpc_size = nvgpu_safe_mult_u32(
			(size_t)config->max_rop_per_gpc_count, sizeof(u32));

	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		config->gpc_rop_logical_id_map[gpc_index] =
			nvgpu_kzalloc(g, max_rop_per_gpc_size);
		if (config->gpc_rop_logical_id_map[gpc_index] == NULL) {
			nvgpu_err(g, "alloc rop_logical_id_map(%u) failed",
					gpc_index);
			err = -ENOMEM;
			goto rop_logical_id_map_alloc_fail;
		}
		/*
		 * Fuse registers must be queried with physical gpc-id and not
		 * the logical ones. For tu104 and before chips logical gpc-id
		 * is same as physical gpc-id for non-floorswept config but for
		 * chips after tu104 it may not be true.
		 */
		gpc_phys_id = nvgpu_grmgr_get_gr_gpc_phys_id(g,
				cur_gr_instance, (u32)gpc_index);
		config->gpc_rop_mask[gpc_index] =
			g->ops.gr.config.get_gpc_rop_mask(g, config,
					gpc_phys_id);
		rop_cnt = 0U;
		for (rop_index = 0; rop_index < config->max_rop_per_gpc_count;
				rop_index++) {
			/*
			 * The gpc_rop_logical_id_map will be intiailized to
			 * UINT_MAX and this is considered to be an invalid
			 * entry, the actual logical_id will be updated based
			 * on the floorsweeping status of the chiplet. So, a
			 * FSed chiplet will have the logical_id_map set to
			 * UINT_MAX.
			 */
			config->gpc_rop_logical_id_map[gpc_index][rop_index] =
				UINT_MAX;
			if (config->gpc_rop_mask[gpc_index] & BIT(rop_index)) {
				config->gpc_rop_logical_id_map[gpc_index][rop_index] =
					rop_cnt++;
			}
		}
	}

	return err;

rop_logical_id_map_alloc_fail:
	for (i = 0; i < gpc_index; i++) {
		nvgpu_kfree(g, config->gpc_rop_logical_id_map[i]);
	}
	nvgpu_kfree(g, config->gpc_rop_logical_id_map);
	nvgpu_kfree(g, config->gpc_rop_mask);
rop_mask_alloc_fail:
	return err;
}

static void gr_config_free_gpc_rop_config(struct gk20a *g,
		struct nvgpu_gr_config *config)
{
	u32 i;

	for (i = 0; i < config->gpc_count; i++) {
		nvgpu_kfree(g, config->gpc_rop_logical_id_map[i]);
	}
	nvgpu_kfree(g, config->gpc_rop_logical_id_map);
	nvgpu_kfree(g, config->gpc_rop_mask);
}

const u32 *gr_config_get_gpc_rop_logical_id_map(struct nvgpu_gr_config *config,
		u32 gpc)
{
	nvgpu_assert(gpc < config->gpc_count);
	return config->gpc_rop_logical_id_map[gpc];
}

static int gr_config_init_gpc_pes_config(struct gk20a *g,
		struct nvgpu_gr_config *config)
{
	u32 pes_cnt = 0U;
	u32 max_pes_per_gpc_size;
	int err = 0;
	u32 gpc_index = 0U, pes_index = 0U, i = 0U;
	u32 gpc_phys_id;
	u32 cur_gr_instance = nvgpu_gr_get_cur_instance_id(g);

	/*
	 * Allocate memory to store the per GPC PES mask. The PES masks will
	 * be indexed using logical gpc id, so allocate memory based on the
	 * number of non-FSed GPCs, which is config->gpc_count.
	 */
	config->gpc_pes_mask = nvgpu_kzalloc(g,
			nvgpu_safe_mult_u64((size_t)config->gpc_count,
				sizeof(u32)));
	if (config->gpc_pes_mask == NULL) {
		nvgpu_err(g, "alloc gpc_pes_mask failed");
		err = -ENOMEM;
		goto pes_mask_alloc_fail;
	}
	/*
	 * This structure holds the logical id for a PES chiplet within a
	 * GPC. The GPC is indexed using logical id and the PES is indexed using
	 * physical id.
	 */
	config->gpc_pes_logical_id_map = nvgpu_kzalloc(g,
			nvgpu_safe_mult_u64((size_t)config->gpc_count,
				sizeof(u32 *)));
	if (config->gpc_pes_logical_id_map == NULL) {
		nvgpu_err(g, "alloc gpc_pes_logical_id_map failed");
		err = -ENOMEM;
		goto pes_logical_id_map_alloc_fail;
	}

	config->max_pes_per_gpc_count = g->ops.top.get_max_pes_per_gpc(g);
	max_pes_per_gpc_size = nvgpu_safe_mult_u32(
			(size_t)config->max_pes_per_gpc_count, sizeof(u32));

	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		config->gpc_pes_logical_id_map[gpc_index] =
			nvgpu_kzalloc(g, max_pes_per_gpc_size);
		if (config->gpc_pes_logical_id_map[gpc_index] == NULL) {
			nvgpu_err(g, "alloc pes_logical_id_map(%u) failed",
					gpc_index);
			err = -ENOMEM;
			goto pes_logical_id_map_alloc_fail;
		}

		/*
		 * Fuse registers must be queried with physical gpc-id and not
		 * the logical ones. For tu104 and before chips logical gpc-id
		 * is same as physical gpc-id for non-floorswept config but for
		 * chips after tu104 it may not be true.
		 */
		gpc_phys_id = nvgpu_grmgr_get_gr_gpc_phys_id(g,
				cur_gr_instance, (u32)gpc_index);
		config->gpc_pes_mask[gpc_index] =
			g->ops.gr.config.get_gpc_pes_mask(g, config,
					gpc_phys_id);
		pes_cnt = 0U;
		for (pes_index = 0; pes_index < config->max_pes_per_gpc_count;
				pes_index++) {
			/*
			 * The gpc_pes_logical_id_map will be intiailized to
			 * UINT_MAX and this is considered to be an invalid
			 * entry, the actual logical_id will be updated based
			 * on the floorsweeping status of the chiplet. So, a
			 * FSed chiplet will have the logical_id_map set to
			 * UINT_MAX.
			 */
			config->gpc_pes_logical_id_map[gpc_index][pes_index] =
				UINT_MAX;
			if (config->gpc_pes_mask[gpc_index] & BIT(pes_index)) {
				config->gpc_pes_logical_id_map[gpc_index][pes_index] =
					pes_cnt++;
			}
		}
	}

	return err;

pes_logical_id_map_alloc_fail:
	for (i = 0; i < gpc_index; i++) {
		nvgpu_kfree(g, config->gpc_pes_logical_id_map[i]);
	}
	nvgpu_kfree(g, config->gpc_pes_logical_id_map);
	nvgpu_kfree(g, config->gpc_pes_mask);
pes_mask_alloc_fail:
	return err;
}

static void gr_config_free_gpc_pes_config(struct gk20a *g,
		struct nvgpu_gr_config *config)
{
	u32 i;

	for (i = 0; i < config->gpc_count; i++) {
		nvgpu_kfree(g, config->gpc_pes_logical_id_map[i]);
	}
	nvgpu_kfree(g, config->gpc_pes_logical_id_map);
	nvgpu_kfree(g, config->gpc_pes_mask);
}

const u32 *gr_config_get_gpc_pes_logical_id_map(struct nvgpu_gr_config *config,
		u32 gpc)
{
	nvgpu_assert(gpc < config->gpc_count);
	return config->gpc_pes_logical_id_map[gpc];
}

static void gr_config_log_info(struct gk20a *g,
					struct nvgpu_gr_config *config)
{
	u32 gpc_index, pes_index;

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "max_gpc_count: %d", config->max_gpc_count);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "gpc_count: %d", config->gpc_count);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "gpc_mask: 0x%x", config->gpc_mask);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "max_tpc_per_gpc_count: %d", config->max_tpc_per_gpc_count);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "max_tpc_count: %d", config->max_tpc_count);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "tpc_count: %d", config->tpc_count);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "sm_count_per_tpc: %d", config->sm_count_per_tpc);
#ifdef CONFIG_NVGPU_GRAPHICS
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "max_zcull_per_gpc_count: %d", config->max_zcull_per_gpc_count);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "zcb_count: %d", config->zcb_count);
#endif
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "pe_count_per_gpc: %d", config->pe_count_per_gpc);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "ppc_count: %d", config->ppc_count);

	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "gpc_tpc_count[%d] : %d",
			   gpc_index, config->gpc_tpc_count[gpc_index]);
	}
	for (gpc_index = 0; gpc_index < config->max_gpc_count; gpc_index++) {
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "gpc_tpc_mask[%d] : 0x%x",
			   gpc_index, config->gpc_tpc_mask[gpc_index]);
	}
#ifdef CONFIG_NVGPU_GRAPHICS
	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "gpc_zcb_count[%d] : %d",
			   gpc_index, config->gpc_zcb_count != NULL ?
				      config->gpc_zcb_count[gpc_index] : 0U);
	}
#endif
	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "gpc_ppc_count[%d] : %d",
			   gpc_index, config->gpc_ppc_count[gpc_index]);
	}
	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "gpc_skip_mask[%d] : 0x%x",
			   gpc_index, config->gpc_skip_mask[gpc_index]);
	}
	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		for (pes_index = 0;
		     pes_index < config->pe_count_per_gpc;
		     pes_index++) {
			nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "pes_tpc_count[%d][%d] : %d",
				   pes_index, gpc_index,
				   config->pes_tpc_count[pes_index][gpc_index]);
		}
	}
	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		for (pes_index = 0;
		     pes_index < config->pe_count_per_gpc;
		     pes_index++) {
			nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "pes_tpc_mask[%d][%d] : 0x%x",
				   pes_index, gpc_index,
				   config->pes_tpc_mask[pes_index][gpc_index]);
		}
	}
}

static void gr_config_set_gpc_mask(struct gk20a *g,
					struct nvgpu_gr_config *config)
{
#ifdef CONFIG_NVGPU_DGPU
	if (g->ops.gr.config.get_gpc_mask != NULL) {
		config->gpc_mask = g->ops.gr.config.get_gpc_mask(g);
	} else
#else
	(void)g;
#endif
	{
		config->gpc_mask = nvgpu_safe_sub_u32(BIT32(config->gpc_count),
								1U);
	}
}

static bool gr_config_alloc_valid(struct nvgpu_gr_config *config)
{
	if ((config->gpc_tpc_count == NULL) || (config->gpc_tpc_mask == NULL) ||
	    (config->gpc_ppc_count == NULL) ||
	    (config->gpc_skip_mask == NULL)) {
		return false;
	}

#ifdef CONFIG_NVGPU_GRAPHICS
	if (!nvgpu_is_enabled(config->g, NVGPU_SUPPORT_MIG) &&
			(config->gpc_zcb_count == NULL)) {
		return false;
	}
#endif

	return true;
}

static void gr_config_free_mem(struct gk20a *g,
				struct nvgpu_gr_config *config)
{
	u32 pes_index;

	for (pes_index = 0U; pes_index < config->pe_count_per_gpc; pes_index++) {
		nvgpu_kfree(g, config->pes_tpc_count[pes_index]);
		nvgpu_kfree(g, config->pes_tpc_mask[pes_index]);
	}

	nvgpu_kfree(g, config->gpc_skip_mask);
	nvgpu_kfree(g, config->gpc_ppc_count);
#ifdef CONFIG_NVGPU_GRAPHICS
	nvgpu_kfree(g, config->gpc_zcb_count);
#endif
	nvgpu_kfree(g, config->gpc_tpc_mask);
	nvgpu_kfree(g, config->gpc_tpc_count);
	nvgpu_kfree(g, config->gpc_tpc_mask_physical);
}

static bool gr_config_alloc_struct_mem(struct gk20a *g,
				struct nvgpu_gr_config *config)
{
	u32 pes_index;
	u32 total_tpc_cnt;
	size_t sm_info_size;
	size_t gpc_size, sm_size, max_gpc_cnt;
	size_t pd_tbl_size;

	total_tpc_cnt = nvgpu_safe_mult_u32(config->gpc_count,
				config->max_tpc_per_gpc_count);
	sm_size = nvgpu_safe_mult_u64((size_t)config->sm_count_per_tpc,
				sizeof(struct nvgpu_sm_info));
	/* allocate for max tpc per gpc */
	sm_info_size = nvgpu_safe_mult_u64((size_t)total_tpc_cnt, sm_size);

	config->sm_to_cluster = nvgpu_kzalloc(g, sm_info_size);
	if (config->sm_to_cluster == NULL) {
		nvgpu_err(g, "sm_to_cluster == NULL");
		goto alloc_err;
	}

#ifdef CONFIG_NVGPU_SM_DIVERSITY
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_SM_DIVERSITY)) {
		config->sm_to_cluster_redex_config =
			nvgpu_kzalloc(g, sm_info_size);
		if (config->sm_to_cluster_redex_config == NULL) {
			nvgpu_err(g, "sm_to_cluster_redex_config == NULL");
			goto clean_alloc_mem;
		}
	}
#endif
	config->no_of_sm = 0;

	gpc_size = nvgpu_safe_mult_u64((size_t)config->gpc_count, sizeof(u32));
	max_gpc_cnt = nvgpu_safe_mult_u64((size_t)config->max_gpc_count, sizeof(u32));
	config->gpc_tpc_count = nvgpu_kzalloc(g, gpc_size);
	config->gpc_tpc_mask = nvgpu_kzalloc(g, max_gpc_cnt);
	config->gpc_tpc_mask_physical = nvgpu_kzalloc(g, max_gpc_cnt);
#ifdef CONFIG_NVGPU_GRAPHICS
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		config->max_zcull_per_gpc_count = nvgpu_get_litter_value(g,
			GPU_LIT_NUM_ZCULL_BANKS);

		config->gpc_zcb_count = nvgpu_kzalloc(g, gpc_size);
	}
#endif
	config->gpc_ppc_count = nvgpu_kzalloc(g, gpc_size);

	pd_tbl_size = nvgpu_safe_mult_u64(
			(size_t)g->ops.gr.config.get_pd_dist_skip_table_size(),
			sizeof(u32));
	pd_tbl_size = nvgpu_safe_mult_u64(pd_tbl_size, 4UL);
	config->gpc_skip_mask = nvgpu_kzalloc(g, pd_tbl_size);

	if (gr_config_alloc_valid(config) == false) {
		goto clean_alloc_mem;
	}

	for (pes_index = 0U; pes_index < config->pe_count_per_gpc; pes_index++) {
		config->pes_tpc_count[pes_index] = nvgpu_kzalloc(g, gpc_size);
		config->pes_tpc_mask[pes_index] = nvgpu_kzalloc(g, gpc_size);
		if ((config->pes_tpc_count[pes_index] == NULL) ||
		    (config->pes_tpc_mask[pes_index] == NULL)) {
			goto clean_alloc_mem;
		}
	}

	return true;

clean_alloc_mem:
	nvgpu_kfree(g, config->sm_to_cluster);
	config->sm_to_cluster = NULL;
#ifdef CONFIG_NVGPU_SM_DIVERSITY
	if (config->sm_to_cluster_redex_config != NULL) {
		nvgpu_kfree(g, config->sm_to_cluster_redex_config);
		config->sm_to_cluster_redex_config = NULL;
	}
#endif
	gr_config_free_mem(g, config);

alloc_err:
	return false;
}

static int gr_config_init_mig_gpcs(struct nvgpu_gr_config *config)
{
	struct gk20a *g = config->g;
	u32 cur_gr_instance = nvgpu_gr_get_cur_instance_id(g);

	config->max_gpc_count = nvgpu_grmgr_get_max_gpc_count(g);
	config->gpc_count = nvgpu_grmgr_get_gr_num_gpcs(g, cur_gr_instance);
	if (config->gpc_count == 0U) {
		nvgpu_err(g, "gpc_count==0!");
		return -EINVAL;
	}

	config->gpc_mask = nvgpu_grmgr_get_gr_logical_gpc_mask(
		g, cur_gr_instance);

	return 0;
}

static int gr_config_init_gpcs(struct nvgpu_gr_config *config)
{
	struct gk20a *g = config->g;

	config->max_gpc_count = g->ops.top.get_max_gpc_count(g);
	config->gpc_count = g->ops.priv_ring.get_gpc_count(g);
	if (config->gpc_count == 0U) {
		nvgpu_err(g, "gpc_count==0!");
		return -EINVAL;
	}

	gr_config_set_gpc_mask(g, config);

	return 0;
}

struct nvgpu_gr_config *nvgpu_gr_config_init(struct gk20a *g)
{
	struct nvgpu_gr_config *config;
	u32 cur_gr_instance = nvgpu_gr_get_cur_instance_id(g);
	u32 gpc_index;
	u32 gpc_phys_id;
	int err;

	config = nvgpu_kzalloc(g, sizeof(*config));
	if (config == NULL) {
		return NULL;
	}

	config->g = g;

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		err = gr_config_init_mig_gpcs(config);
		if (err < 0) {
			nvgpu_err(g, "MIG GPC config init failed");
			nvgpu_kfree(g, config);
			return NULL;
		}
	} else {
		err = gr_config_init_gpcs(config);
		if (err < 0) {
			nvgpu_err(g, "GPC config init failed");
			nvgpu_kfree(g, config);
			return NULL;
		}
	}

	/* Required to read gpc_tpc_mask below */
	config->max_tpc_per_gpc_count = g->ops.top.get_max_tpc_per_gpc_count(g);

	config->max_tpc_count = nvgpu_safe_mult_u32(config->max_gpc_count,
				config->max_tpc_per_gpc_count);

	config->pe_count_per_gpc = nvgpu_get_litter_value(g,
		GPU_LIT_NUM_PES_PER_GPC);
	if (config->pe_count_per_gpc > GK20A_GR_MAX_PES_PER_GPC) {
		nvgpu_err(g, "too many pes per gpc");
		goto clean_up_init;
	}

	config->sm_count_per_tpc =
		nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);
	if (config->sm_count_per_tpc == 0U) {
		nvgpu_err(g, "sm_count_per_tpc==0!");
		goto clean_up_init;
	}

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PES_FS)) {
		err = gr_config_init_gpc_pes_config(g, config);
		if (err < 0) {
			goto clean_up_init;
		}
	}

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_ROP_IN_GPC)) {
		err = gr_config_init_gpc_rop_config(g, config);
		if (err < 0) {
			goto clean_up_gpc_pes_config;
		}
	}

	if (gr_config_alloc_struct_mem(g, config) == false) {
		goto clean_up_gpc_rop_config;
	}

	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		/*
		 * Fuse registers must be queried with physical gpc-id and not
		 * the logical ones. For tu104 and before chips logical gpc-id
		 * is same as physical gpc-id for non-floorswept config but for
		 * chips after tu104 it may not be true.
		 */
		gpc_phys_id = nvgpu_grmgr_get_gr_gpc_phys_id(g,
				cur_gr_instance, gpc_index);

		config->gpc_tpc_mask[gpc_index] =
		     g->ops.gr.config.get_gpc_tpc_mask(g, config, gpc_phys_id);
		config->gpc_tpc_mask_physical[gpc_phys_id] =
		     g->ops.gr.config.get_gpc_tpc_mask(g, config, gpc_phys_id);
	}

	config->ppc_count = 0;
	config->tpc_count = 0;
#ifdef CONFIG_NVGPU_GRAPHICS
	config->zcb_count = 0;
#endif
	for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
		config->gpc_tpc_count[gpc_index] =
			g->ops.gr.config.get_tpc_count_in_gpc(g, config,
				gpc_index);
		config->tpc_count = nvgpu_safe_add_u32(config->tpc_count,
					config->gpc_tpc_count[gpc_index]);

#ifdef CONFIG_NVGPU_GRAPHICS
		if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
			config->gpc_zcb_count[gpc_index] =
				g->ops.gr.config.get_zcull_count_in_gpc(g, config,
					gpc_index);
			config->zcb_count = nvgpu_safe_add_u32(config->zcb_count,
						config->gpc_zcb_count[gpc_index]);
		}
#endif

		gr_config_init_pes_tpc(g, config, gpc_index);

		config->ppc_count = nvgpu_safe_add_u32(config->ppc_count,
					config->gpc_ppc_count[gpc_index]);

		gr_config_init_gpc_skip_mask(config, gpc_index);
	}

	gr_config_log_info(g, config);
	return config;

clean_up_gpc_rop_config:
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_ROP_IN_GPC)) {
		gr_config_free_gpc_rop_config(g, config);
	}
clean_up_gpc_pes_config:
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PES_FS)) {
		gr_config_free_gpc_pes_config(g, config);
	}
clean_up_init:
	nvgpu_kfree(g, config);
	return NULL;
}

#ifdef CONFIG_NVGPU_GRAPHICS
static u32 prime_set[18] = {
	2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61 };

/*
 * Return map tiles count for given index
 * Return 0 if index is out-of-bounds
 */
u32 nvgpu_gr_config_get_map_tile_count(struct nvgpu_gr_config *config, u32 index)
{
	if (index >= config->map_tile_count) {
		return 0;
	}

	return config->map_tiles[index];
}

u8 *nvgpu_gr_config_get_map_tiles(struct nvgpu_gr_config *config)
{
	return config->map_tiles;
}

u32 nvgpu_gr_config_get_map_row_offset(struct nvgpu_gr_config *config)
{
	return config->map_row_offset;
}

int nvgpu_gr_config_init_map_tiles(struct gk20a *g,
	struct nvgpu_gr_config *config)
{
	s32 comm_denom;
	s32 mul_factor;
	s32 *init_frac = NULL;
	s32 *init_err = NULL;
	s32 *run_err = NULL;
	u32 *sorted_num_tpcs = NULL;
	u32 *sorted_to_unsorted_gpc_map = NULL;
	u32 gpc_index;
	u32 gpc_mark = 0;
	u32 num_tpc;
	u32 max_tpc_count = 0;
	u32 swap;
	u32 tile_count;
	u32 index;
	bool delete_map = false;
	bool gpc_sorted;
	int ret = 0;
	u32 num_gpcs = nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS);
	u32 num_tpc_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_TPC_PER_GPC);
	u32 map_tile_count = num_gpcs * num_tpc_per_gpc;

	nvgpu_log(g, gpu_dbg_gr, " ");

	init_frac = nvgpu_kzalloc(g, num_gpcs * sizeof(s32));
	init_err = nvgpu_kzalloc(g, num_gpcs * sizeof(s32));
	run_err = nvgpu_kzalloc(g, num_gpcs * sizeof(s32));
	sorted_num_tpcs =
		nvgpu_kzalloc(g, (size_t)num_gpcs *
				 (size_t)num_tpc_per_gpc *
				 sizeof(s32));
	sorted_to_unsorted_gpc_map =
		nvgpu_kzalloc(g, (size_t)num_gpcs * sizeof(s32));

	if (!((init_frac != NULL) &&
	      (init_err != NULL) &&
	      (run_err != NULL) &&
	      (sorted_num_tpcs != NULL) &&
	      (sorted_to_unsorted_gpc_map != NULL))) {
		ret = -ENOMEM;
		goto clean_up;
	}

	config->map_row_offset = 0xFFFFFFFFU;

	if (config->tpc_count == 3U) {
		config->map_row_offset = 2;
	} else if (config->tpc_count < 3U) {
		config->map_row_offset = 1;
	} else {
		config->map_row_offset = 3;

		for (index = 1U; index < 18U; index++) {
			u32 prime = prime_set[index];
			if ((config->tpc_count % prime) != 0U) {
				config->map_row_offset = prime;
				break;
			}
		}
	}

	switch (config->tpc_count) {
	case 15:
		config->map_row_offset = 6;
		break;
	case 14:
		config->map_row_offset = 5;
		break;
	case 13:
		config->map_row_offset = 2;
		break;
	case 11:
		config->map_row_offset = 7;
		break;
	case 10:
		config->map_row_offset = 6;
		break;
	case 7:
	case 5:
		config->map_row_offset = 1;
		break;
	default:
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "unsupported tpc count = %u",
				config->tpc_count);
		break;
	}

	if (config->map_tiles != NULL) {
		if (config->map_tile_count != config->tpc_count) {
			delete_map = true;
		}

		for (tile_count = 0; tile_count < config->map_tile_count; tile_count++) {
			if (nvgpu_gr_config_get_map_tile_count(config, tile_count)
					>= config->tpc_count) {
				delete_map = true;
			}
		}

		if (delete_map) {
			nvgpu_kfree(g, config->map_tiles);
			config->map_tiles = NULL;
			config->map_tile_count = 0;
		}
	}

	if (config->map_tiles == NULL) {
		config->map_tiles = nvgpu_kzalloc(g, map_tile_count * sizeof(u8));
		if (config->map_tiles == NULL) {
			ret = -ENOMEM;
			goto clean_up;
		}
		config->map_tile_count = map_tile_count;

		for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
			sorted_num_tpcs[gpc_index] = config->gpc_tpc_count[gpc_index];
			sorted_to_unsorted_gpc_map[gpc_index] = gpc_index;
		}

		gpc_sorted = false;
		while (!gpc_sorted) {
			gpc_sorted = true;
			for (gpc_index = 0U; gpc_index < config->gpc_count - 1U; gpc_index++) {
				if (sorted_num_tpcs[gpc_index + 1U] > sorted_num_tpcs[gpc_index]) {
					gpc_sorted = false;
					swap = sorted_num_tpcs[gpc_index];
					sorted_num_tpcs[gpc_index] = sorted_num_tpcs[gpc_index + 1U];
					sorted_num_tpcs[gpc_index + 1U] = swap;
					swap = sorted_to_unsorted_gpc_map[gpc_index];
					sorted_to_unsorted_gpc_map[gpc_index] =
						sorted_to_unsorted_gpc_map[gpc_index + 1U];
					sorted_to_unsorted_gpc_map[gpc_index + 1U] = swap;
				}
			}
		}

		for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
			if (config->gpc_tpc_count[gpc_index] > max_tpc_count) {
				max_tpc_count = config->gpc_tpc_count[gpc_index];
			}
		}

		mul_factor = S32(config->gpc_count) * S32(max_tpc_count);
		if ((U32(mul_factor) & 0x1U) != 0U) {
			mul_factor = 2;
		} else {
			mul_factor = 1;
		}

		comm_denom = S32(config->gpc_count) * S32(max_tpc_count) * mul_factor;

		for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
			num_tpc = sorted_num_tpcs[gpc_index];

			init_frac[gpc_index] = S32(num_tpc) * S32(config->gpc_count) * mul_factor;

			if (num_tpc != 0U) {
				init_err[gpc_index] = S32(gpc_index) * S32(max_tpc_count) * mul_factor - comm_denom/2;
			} else {
				init_err[gpc_index] = 0;
			}

			run_err[gpc_index] = init_frac[gpc_index] + init_err[gpc_index];
		}

		while (gpc_mark < config->tpc_count) {
			for (gpc_index = 0; gpc_index < config->gpc_count; gpc_index++) {
				if ((run_err[gpc_index] * 2) >= comm_denom) {
					config->map_tiles[gpc_mark++] = (u8)sorted_to_unsorted_gpc_map[gpc_index];
					run_err[gpc_index] += init_frac[gpc_index] - comm_denom;
				} else {
					run_err[gpc_index] += init_frac[gpc_index];
				}
			}
		}
	}

clean_up:
	nvgpu_kfree(g, init_frac);
	nvgpu_kfree(g, init_err);
	nvgpu_kfree(g, run_err);
	nvgpu_kfree(g, sorted_num_tpcs);
	nvgpu_kfree(g, sorted_to_unsorted_gpc_map);

	if (ret != 0) {
		nvgpu_err(g, "fail");
	} else {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	}

	return ret;
}

u32 nvgpu_gr_config_get_max_zcull_per_gpc_count(struct nvgpu_gr_config *config)
{
	return config->max_zcull_per_gpc_count;
}

u32 nvgpu_gr_config_get_zcb_count(struct nvgpu_gr_config *config)
{
	return config->zcb_count;
}

u32 nvgpu_gr_config_get_gpc_zcb_count(struct nvgpu_gr_config *config,
	u32 gpc_index)
{
	return config->gpc_zcb_count[gpc_index];
}
#endif

void nvgpu_gr_config_deinit(struct gk20a *g, struct nvgpu_gr_config *config)
{
	if (config == NULL) {
		return;
	}

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PES_FS)) {
		gr_config_free_gpc_pes_config(g, config);
	}
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_ROP_IN_GPC)) {
		gr_config_free_gpc_rop_config(g, config);
	}
	gr_config_free_mem(g, config);
#ifdef CONFIG_NVGPU_GRAPHICS
	nvgpu_kfree(g, config->map_tiles);
#endif
	nvgpu_kfree(g, config->sm_to_cluster);
	config->sm_to_cluster = NULL;
#ifdef CONFIG_NVGPU_SM_DIVERSITY
	if (config->sm_to_cluster_redex_config != NULL) {
		nvgpu_kfree(g, config->sm_to_cluster_redex_config);
		config->sm_to_cluster_redex_config = NULL;
	}
#endif

	nvgpu_kfree(g, config);
}

u32 nvgpu_gr_config_get_max_gpc_count(struct nvgpu_gr_config *config)
{
	return config->max_gpc_count;
}

u32 nvgpu_gr_config_get_max_tpc_per_gpc_count(struct nvgpu_gr_config *config)
{
	return config->max_tpc_per_gpc_count;
}

u32 nvgpu_gr_config_get_max_pes_per_gpc_count(struct nvgpu_gr_config *config)
{
	return config->max_pes_per_gpc_count;
}

u32 nvgpu_gr_config_get_max_rop_per_gpc_count(struct nvgpu_gr_config *config)
{
	return config->max_rop_per_gpc_count;
}

u32 nvgpu_gr_config_get_max_tpc_count(struct nvgpu_gr_config *config)
{
	return config->max_tpc_count;
}

u32 nvgpu_gr_config_get_gpc_count(struct nvgpu_gr_config *config)
{
	return config->gpc_count;
}

u32 nvgpu_gr_config_get_tpc_count(struct nvgpu_gr_config *config)
{
	return config->tpc_count;
}

u32 nvgpu_gr_config_get_ppc_count(struct nvgpu_gr_config *config)
{
	return config->ppc_count;
}

u32 nvgpu_gr_config_get_pe_count_per_gpc(struct nvgpu_gr_config *config)
{
	return config->pe_count_per_gpc;
}

u32 nvgpu_gr_config_get_sm_count_per_tpc(struct nvgpu_gr_config *config)
{
	return config->sm_count_per_tpc;
}

u32 nvgpu_gr_config_get_gpc_ppc_count(struct nvgpu_gr_config *config,
	u32 gpc_index)
{
	nvgpu_assert(gpc_index < nvgpu_gr_config_get_gpc_count(config));
	return config->gpc_ppc_count[gpc_index];
}

u32 *nvgpu_gr_config_get_base_count_gpc_tpc(struct nvgpu_gr_config *config)
{
	return config->gpc_tpc_count;
}

u32 nvgpu_gr_config_get_gpc_tpc_count(struct nvgpu_gr_config *config,
	u32 gpc_index)
{
	if (gpc_index >= config->gpc_count) {
		return 0;
	}
	return config->gpc_tpc_count[gpc_index];
}

u32 nvgpu_gr_config_get_pes_tpc_count(struct nvgpu_gr_config *config,
	u32 gpc_index, u32 pes_index)
{
	nvgpu_assert(gpc_index < nvgpu_gr_config_get_gpc_count(config));
	nvgpu_assert(pes_index < nvgpu_gr_config_get_pe_count_per_gpc(config));
	return config->pes_tpc_count[pes_index][gpc_index];
}

u32 *nvgpu_gr_config_get_base_mask_gpc_tpc(struct nvgpu_gr_config *config)
{
	return config->gpc_tpc_mask;
}

u32 *nvgpu_gr_config_get_gpc_tpc_mask_physical_base(struct nvgpu_gr_config *config)
{
	return config->gpc_tpc_mask_physical;
}

u32 nvgpu_gr_config_get_gpc_tpc_mask(struct nvgpu_gr_config *config,
	u32 gpc_index)
{
	nvgpu_assert(gpc_index < nvgpu_gr_config_get_max_gpc_count(config));
	return config->gpc_tpc_mask[gpc_index];
}

u32 nvgpu_gr_config_get_gpc_tpc_mask_physical(struct nvgpu_gr_config *config,
	u32 gpc_index)
{
	nvgpu_assert(gpc_index < nvgpu_gr_config_get_max_gpc_count(config));
	return config->gpc_tpc_mask_physical[gpc_index];
}

void nvgpu_gr_config_set_gpc_tpc_mask(struct nvgpu_gr_config *config,
	u32 gpc_index, u32 val)
{
	nvgpu_assert(gpc_index < nvgpu_gr_config_get_gpc_count(config));
	config->gpc_tpc_mask[gpc_index] = val;
}

u32 nvgpu_gr_config_get_gpc_skip_mask(struct nvgpu_gr_config *config,
	u32 gpc_index)
{
	if (gpc_index >= config->gpc_count) {
		return 0;
	}
	return config->gpc_skip_mask[gpc_index];
}

u32 nvgpu_gr_config_get_pes_tpc_mask(struct nvgpu_gr_config *config,
	u32 gpc_index, u32 pes_index)
{
	nvgpu_assert(gpc_index < nvgpu_gr_config_get_gpc_count(config));
	nvgpu_assert(pes_index < nvgpu_gr_config_get_pe_count_per_gpc(config));
	return config->pes_tpc_mask[pes_index][gpc_index];
}

u32 nvgpu_gr_config_get_gpc_mask(struct nvgpu_gr_config *config)
{
	return config->gpc_mask;
}

u32 nvgpu_gr_config_get_no_of_sm(struct nvgpu_gr_config *config)
{
	return config->no_of_sm;
}

void nvgpu_gr_config_set_no_of_sm(struct nvgpu_gr_config *config, u32 no_of_sm)
{
	config->no_of_sm = no_of_sm;
}

struct nvgpu_sm_info *nvgpu_gr_config_get_sm_info(struct nvgpu_gr_config *config,
	u32 sm_id)
{
	if (sm_id < config->no_of_sm) {
		return &config->sm_to_cluster[sm_id];
	}
	return NULL;
}

#ifdef CONFIG_NVGPU_SM_DIVERSITY
struct nvgpu_sm_info *nvgpu_gr_config_get_redex_sm_info(
	struct nvgpu_gr_config *config, u32 sm_id)
{
	return &config->sm_to_cluster_redex_config[sm_id];
}
#endif

u32 nvgpu_gr_config_get_sm_info_gpc_index(struct nvgpu_sm_info *sm_info)
{
	return sm_info->gpc_index;
}

void nvgpu_gr_config_set_sm_info_gpc_index(struct nvgpu_sm_info *sm_info,
	u32 gpc_index)
{
	sm_info->gpc_index = gpc_index;
}

u32 nvgpu_gr_config_get_sm_info_tpc_index(struct nvgpu_sm_info *sm_info)
{
	return sm_info->tpc_index;
}

void nvgpu_gr_config_set_sm_info_tpc_index(struct nvgpu_sm_info *sm_info,
	u32 tpc_index)
{
	sm_info->tpc_index = tpc_index;
}

u32 nvgpu_gr_config_get_sm_info_global_tpc_index(struct nvgpu_sm_info *sm_info)
{
	return sm_info->global_tpc_index;
}

void nvgpu_gr_config_set_sm_info_global_tpc_index(struct nvgpu_sm_info *sm_info,
	u32 global_tpc_index)
{
	sm_info->global_tpc_index = global_tpc_index;
}

u32 nvgpu_gr_config_get_sm_info_sm_index(struct nvgpu_sm_info *sm_info)
{
	return sm_info->sm_index;
}

void nvgpu_gr_config_set_sm_info_sm_index(struct nvgpu_sm_info *sm_info,
	u32 sm_index)
{
	sm_info->sm_index = sm_index;
}
