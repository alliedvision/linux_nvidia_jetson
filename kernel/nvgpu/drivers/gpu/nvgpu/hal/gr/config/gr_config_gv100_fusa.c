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
#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/config.h>

#include "gr_config_gv100.h"

static int gr_gv100_scg_calculate_perf(struct nvgpu_gr_config *gr_config,
		u32 scale_factor, u32 scg_num_pes, u32 *num_tpc_gpc,
		u32 max_tpc_gpc, u32 min_scg_gpc_pix_perf,
		u32 average_tpcs, u32 *perf)
{
	int err = 0;
	u32 scg_world_perf;
	u32 tpc_balance;
	u32 diff;
	u32 gpc_id;
	u32 pix_scale_perf, world_scale_perf, tpc_scale_perf;
	u32 pix_world_scale_sum;
	u32 pix_scale = 1024U*1024U;	/* Pix perf in [29:20] */
	u32 world_scale = 1024U;	/* World performance in [19:10] */
	u32 tpc_scale = 1U;		/* TPC balancing in [9:0] */
	u32 norm_tpc_deviation;		/* deviation/max_tpc_per_gpc */
	u32 deviation = 0U;		/* absolute diff between TPC# and
					 * average_tpcs, averaged across GPCs
					 */

	scg_world_perf = nvgpu_safe_mult_u32(scale_factor, scg_num_pes) /
		nvgpu_gr_config_get_ppc_count(gr_config);

	for (gpc_id = 0U;
	     gpc_id < nvgpu_gr_config_get_gpc_count(gr_config);
	     gpc_id++) {
		u32 temp = nvgpu_safe_mult_u32(scale_factor,
						num_tpc_gpc[gpc_id]);
		if (average_tpcs > temp) {
			diff = nvgpu_safe_sub_u32(average_tpcs, temp);
		} else {
			diff = nvgpu_safe_sub_u32(temp, average_tpcs);
		}
		deviation = nvgpu_safe_add_u32(deviation, diff);
	}

	deviation /= nvgpu_gr_config_get_gpc_count(gr_config);

	nvgpu_assert(max_tpc_gpc != 0U);
	norm_tpc_deviation = deviation / max_tpc_gpc;

	tpc_balance = nvgpu_safe_sub_u32(scale_factor, norm_tpc_deviation);

	if ((tpc_balance > scale_factor)          ||
	    (scg_world_perf > scale_factor)       ||
	    (min_scg_gpc_pix_perf > scale_factor) ||
	    (norm_tpc_deviation > scale_factor)) {
		err = -EINVAL;
		goto calc_perf_end;
	}

	pix_scale_perf = nvgpu_safe_mult_u32(pix_scale, min_scg_gpc_pix_perf);
	world_scale_perf = nvgpu_safe_mult_u32(world_scale, scg_world_perf);
	tpc_scale_perf = nvgpu_safe_mult_u32(tpc_scale, tpc_balance);
	pix_world_scale_sum = nvgpu_safe_add_u32(pix_scale_perf, world_scale_perf);
	*perf = nvgpu_safe_add_u32(pix_world_scale_sum, tpc_scale_perf);

calc_perf_end:
	return err;
}

static int gr_gv100_calc_valid_pes(struct nvgpu_gr_config *gr_config,
		u32 gpc_id, u32 *gpc_tpc_mask, u32 disable_gpc_id,
		u32 disable_tpc_id, bool *is_tpc_removed_pes,
		u32 *scg_num_pes)
{
	int err = 0;
	u32 pes_id;
	u32 num_tpc_mask;

	/* Calculate # of surviving PES */
	for (pes_id = 0;
	     pes_id < nvgpu_gr_config_get_gpc_ppc_count(gr_config, gpc_id);
	     pes_id++) {
		/* Count the number of TPC on the set */
		num_tpc_mask = nvgpu_gr_config_get_pes_tpc_mask(
					gr_config, gpc_id, pes_id) &
				gpc_tpc_mask[gpc_id];

		if ((gpc_id == disable_gpc_id) &&
		    ((num_tpc_mask & BIT32(disable_tpc_id)) != 0U)) {

			if (*is_tpc_removed_pes) {
				err = -EINVAL;
				goto calc_pes_err;
			}
			num_tpc_mask &= ~(BIT32(disable_tpc_id));
			*is_tpc_removed_pes = true;
		}
		if (hweight32(num_tpc_mask) != 0UL) {
			*scg_num_pes = nvgpu_safe_add_u32(*scg_num_pes, 1U);
		}
	}

calc_pes_err:
	return err;
}

static int gr_gv100_remove_logical_tpc(struct nvgpu_gr_config *gr_config,
		u32 gpc_id, u32 *gpc_tpc_mask, u32 disable_gpc_id,
		u32 disable_tpc_id, bool *is_tpc_removed_gpc,
		u32 *num_tpc_gpc)
{
	int err = 0;
	u32 num_tpc_mask = gpc_tpc_mask[gpc_id];

	(void)gr_config;

	if ((gpc_id == disable_gpc_id) &&
	    ((num_tpc_mask & BIT32(disable_tpc_id)) != 0U)) {
		/* Safety check if a TPC is removed twice */
		if (*is_tpc_removed_gpc) {
			err = -EINVAL;
			goto remove_tpc_err;
		}
		/* Remove logical TPC from set */
		num_tpc_mask &= ~(BIT32(disable_tpc_id));
		*is_tpc_removed_gpc = true;
	}

	/* track balancing of tpcs across gpcs */
	num_tpc_gpc[gpc_id] = hweight32(num_tpc_mask);

remove_tpc_err:
	return err;
}

static u32 gr_gv100_find_max_gpc(u32 *num_tpc_gpc, u32 gpc_id, u32 max_tpc_gpc)
{
	return (num_tpc_gpc[gpc_id] > max_tpc_gpc) ?
				num_tpc_gpc[gpc_id] : max_tpc_gpc;
}

static int gr_gr100_find_perf_reduction_rate_gpc(struct gk20a *g,
			struct nvgpu_gr_config *gr_config,
			u32 *gpc_tpc_mask, u32 disable_gpc_id,
			u32 disable_tpc_id, u32 *min_scg_gpc_pix_perf,
			u32 *max_tpc_gpc, u32 *scg_num_pes, u32 *average_tpcs,
			u32 *num_tpc_gpc)
{
	int err = 0;
	u32 scale_factor = 512U; /* Use fx23.9 */
	u32 gpc_id;
	u32 scg_gpc_pix_perf = 0U;
	bool is_tpc_removed_gpc = false;
	bool is_tpc_removed_pes = false;
	u32 tpc_cnt = 0U;

	(void)g;

	for (gpc_id = 0;
	     gpc_id < nvgpu_gr_config_get_gpc_count(gr_config);
	     gpc_id++) {

		err = gr_gv100_remove_logical_tpc(gr_config, gpc_id,
				gpc_tpc_mask, disable_gpc_id, disable_tpc_id,
				&is_tpc_removed_gpc, num_tpc_gpc);
		if (err != 0) {
			goto perf_rate_calc_fail;
		}

		/* track balancing of tpcs across gpcs */
		*average_tpcs = nvgpu_safe_add_u32(*average_tpcs,
					num_tpc_gpc[gpc_id]);

		/* save the maximum numer of gpcs */
		*max_tpc_gpc = gr_gv100_find_max_gpc(num_tpc_gpc,
					gpc_id, *max_tpc_gpc);

		/*
		 * Calculate ratio between TPC count and post-FS and post-SCG
		 *
		 * ratio represents relative throughput of the GPC
		 */
		tpc_cnt = nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc_id);
		nvgpu_assert(tpc_cnt != 0U);

		scg_gpc_pix_perf = nvgpu_safe_mult_u32(scale_factor,
						num_tpc_gpc[gpc_id]) / tpc_cnt;

		if (*min_scg_gpc_pix_perf > scg_gpc_pix_perf) {
			*min_scg_gpc_pix_perf = scg_gpc_pix_perf;
		}

		/* Calculate # of surviving PES */
		err = gr_gv100_calc_valid_pes(gr_config, gpc_id, gpc_tpc_mask,
				disable_gpc_id, disable_tpc_id,
				&is_tpc_removed_pes, scg_num_pes);
		if (err != 0) {
			goto perf_rate_calc_fail;
		}
	}

	if (!is_tpc_removed_gpc || !is_tpc_removed_pes) {
		err = -EINVAL;
	}

perf_rate_calc_fail:
	return err;
}
/*
 * Estimate performance if the given logical TPC in the given logical GPC were
 * removed.
 */
static int gr_gv100_scg_estimate_perf(struct gk20a *g,
					struct nvgpu_gr_config *gr_config,
					u32 *gpc_tpc_mask,
					u32 disable_gpc_id, u32 disable_tpc_id,
					u32 *perf)
{
	int err = 0;
	u32 scale_factor = 512U; /* Use fx23.9 */
	u32 scg_num_pes = 0U;
	u32 min_scg_gpc_pix_perf = scale_factor; /* Init perf as maximum */
	u32 average_tpcs = 0U;		/* Average of # of TPCs per GPC */
	u32 max_tpc_gpc = 0U;
	u32 tpc_cnt = nvgpu_safe_mult_u32((u32)sizeof(u32),
				nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS));
	u32 *num_tpc_gpc = nvgpu_kzalloc(g, tpc_cnt);

	if (num_tpc_gpc == NULL) {
		return -ENOMEM;
	}

	/* Calculate pix-perf-reduction-rate per GPC and find bottleneck TPC */
	err = gr_gr100_find_perf_reduction_rate_gpc(g, gr_config, gpc_tpc_mask,
			disable_gpc_id, disable_tpc_id, &min_scg_gpc_pix_perf,
			&max_tpc_gpc, &scg_num_pes, &average_tpcs, num_tpc_gpc);
	if (err != 0) {
		goto free_resources;
	}

	if (max_tpc_gpc == 0U) {
		*perf = 0;
		goto free_resources;
	}

	/* Now calculate perf */
	average_tpcs = nvgpu_safe_mult_u32(scale_factor, average_tpcs) /
			nvgpu_gr_config_get_gpc_count(gr_config);

	err = gr_gv100_scg_calculate_perf(gr_config, scale_factor,
			scg_num_pes, num_tpc_gpc, max_tpc_gpc,
			min_scg_gpc_pix_perf, average_tpcs, perf);
	if (err != 0) {
		nvgpu_err(g, "scg perf calculation failed");
	}
free_resources:
	nvgpu_kfree(g, num_tpc_gpc);
	return err;
}

static int gr_gv100_scg_estimate_perf_for_all_gpc_tpc(struct gk20a *g,
			struct nvgpu_gr_config *gr_config, u32 *gpc_tpc_mask,
			u32 *gpc_table, u32 *tpc_table)
{
	unsigned long gpc_tpc_mask_tmp;
	unsigned long tpc_tmp;
	u32 perf, maxperf;
	int err = 0;
	u32 gtpc, gpc, tpc;

	for (gtpc = 0; gtpc < nvgpu_gr_config_get_tpc_count(gr_config); gtpc++) {
		maxperf = 0U;
		for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(gr_config); gpc++) {
			gpc_tpc_mask_tmp = (unsigned long)gpc_tpc_mask[gpc];

			for_each_set_bit(tpc_tmp, &gpc_tpc_mask_tmp,
					nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc)) {
				perf = 0U;
				tpc = (u32)tpc_tmp;

				err = gr_gv100_scg_estimate_perf(g, gr_config,
						gpc_tpc_mask, gpc, tpc, &perf);

				if (err != 0) {
					nvgpu_err(g,
						"Error while estimating perf");
					goto exit_perf_err;
				}

				if (perf >= maxperf) {
					maxperf = perf;
					gpc_table[gtpc] = gpc;
					tpc_table[gtpc] = tpc;
				}
			}
		}
		gpc_tpc_mask[gpc_table[gtpc]] &= ~(BIT32(tpc_table[gtpc]));
	}

exit_perf_err:
	return err;
}

#ifdef CONFIG_NVGPU_SM_DIVERSITY
static void gv100_gr_config_set_redex_sminfo(struct gk20a *g,
		struct nvgpu_gr_config *gr_config, u32 num_sm,
		u32 sm_per_tpc, u32 *gpc_table, u32 *tpc_table)
{
	u32 sm;
	u32 tpc = nvgpu_gr_config_get_tpc_count(gr_config);
	u32 sm_id = 0;
	u32 glboal_index = 0;

	for (sm_id = 0; sm_id < num_sm; sm_id += sm_per_tpc) {
		tpc = nvgpu_safe_sub_u32(tpc, 1U);
		for (sm = 0; sm < sm_per_tpc; sm++) {
			u32 index = nvgpu_safe_add_u32(sm_id, sm);
			struct nvgpu_sm_info *sm_info =
				nvgpu_gr_config_get_redex_sm_info(
					gr_config, index);
			nvgpu_gr_config_set_sm_info_gpc_index(sm_info,
							gpc_table[tpc]);
			nvgpu_gr_config_set_sm_info_tpc_index(sm_info,
							tpc_table[tpc]);
			nvgpu_gr_config_set_sm_info_sm_index(sm_info, sm);
			nvgpu_gr_config_set_sm_info_global_tpc_index(
				sm_info, glboal_index);

			nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr,
				"gpc : %d tpc %d sm_index %d global_index: %d",
				nvgpu_gr_config_get_sm_info_gpc_index(sm_info),
				nvgpu_gr_config_get_sm_info_tpc_index(sm_info),
				nvgpu_gr_config_get_sm_info_sm_index(sm_info),
				nvgpu_gr_config_get_sm_info_global_tpc_index(
					sm_info));

		}
		glboal_index = nvgpu_safe_add_u32(glboal_index, 1U);
	}
}
#endif

static void gv100_gr_config_set_sminfo(struct gk20a *g,
		struct nvgpu_gr_config *gr_config, u32 num_sm,
		u32 sm_per_tpc, u32 *gpc_table, u32 *tpc_table)
{
	u32 sm;
	u32 tpc = 0;
	u32 sm_id = 0;

	for (sm_id = 0; sm_id < num_sm; sm_id += sm_per_tpc) {
		for (sm = 0; sm < sm_per_tpc; sm++) {
			u32 index = nvgpu_safe_add_u32(sm_id, sm);
			struct nvgpu_sm_info *sm_info =
				nvgpu_gr_config_get_sm_info(gr_config, index);
			nvgpu_assert(sm_info != NULL);

			nvgpu_gr_config_set_sm_info_gpc_index(sm_info,
							gpc_table[tpc]);
			nvgpu_gr_config_set_sm_info_tpc_index(sm_info,
							tpc_table[tpc]);
			nvgpu_gr_config_set_sm_info_sm_index(sm_info, sm);
			nvgpu_gr_config_set_sm_info_global_tpc_index(sm_info, tpc);

			nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr,
				"gpc : %d tpc %d sm_index %d global_index: %d",
				nvgpu_gr_config_get_sm_info_gpc_index(sm_info),
				nvgpu_gr_config_get_sm_info_tpc_index(sm_info),
				nvgpu_gr_config_get_sm_info_sm_index(sm_info),
				nvgpu_gr_config_get_sm_info_global_tpc_index(sm_info));

		}
		tpc = nvgpu_safe_add_u32(tpc, 1U);
	}

#ifdef CONFIG_NVGPU_SM_DIVERSITY
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_SM_DIVERSITY)) {
		gv100_gr_config_set_redex_sminfo(g, gr_config, num_sm,
			sm_per_tpc, gpc_table, tpc_table);
	}
#endif
}

int gv100_gr_config_init_sm_id_table(struct gk20a *g,
		struct nvgpu_gr_config *gr_config)
{
	u32 gpc, pes;
	u32 sm_per_tpc = nvgpu_gr_config_get_sm_count_per_tpc(gr_config);
	u32 tpc_cnt = nvgpu_gr_config_get_tpc_count(gr_config);
	u32 num_sm = nvgpu_safe_mult_u32(sm_per_tpc, tpc_cnt);
	int err = 0;
	u32 *gpc_tpc_mask;
	u32 *tpc_table, *gpc_table;
	u32 tbl_size = 0U;
	u32 temp = 0U;

	tbl_size = nvgpu_safe_mult_u32(tpc_cnt, (u32)sizeof(u32));
	gpc_table = nvgpu_kzalloc(g, tbl_size);
	tpc_table = nvgpu_kzalloc(g, tbl_size);
	temp = nvgpu_safe_mult_u32((u32)sizeof(u32),
				nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS));
	gpc_tpc_mask = nvgpu_kzalloc(g, temp);

	if ((gpc_table == NULL) ||
	    (tpc_table == NULL) ||
	    (gpc_tpc_mask == NULL)) {
		nvgpu_err(g, "Error allocating memory for sm tables");
		err = -ENOMEM;
		goto exit_build_table;
	}

	for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(gr_config); gpc++) {
		for (pes = 0;
		     pes < nvgpu_gr_config_get_gpc_ppc_count(gr_config, gpc);
		     pes++) {
			gpc_tpc_mask[gpc] |= nvgpu_gr_config_get_pes_tpc_mask(
						gr_config, gpc, pes);
		}
	}

	err = gr_gv100_scg_estimate_perf_for_all_gpc_tpc(g, gr_config,
				gpc_tpc_mask, gpc_table, tpc_table);
	if (err != 0) {
		goto exit_build_table;
	}

	nvgpu_gr_config_set_no_of_sm(gr_config, num_sm);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "total number of sm = %d", num_sm);

	gv100_gr_config_set_sminfo(g, gr_config, num_sm, sm_per_tpc,
					gpc_table, tpc_table);

exit_build_table:
	nvgpu_kfree(g, gpc_table);
	nvgpu_kfree(g, tpc_table);
	nvgpu_kfree(g, gpc_tpc_mask);
	return err;
}
