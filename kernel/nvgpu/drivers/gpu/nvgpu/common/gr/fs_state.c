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

#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>

#include <nvgpu/gr/config.h>
#include <nvgpu/gr/fs_state.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/grmgr.h>

static int gr_load_sm_id_config(struct gk20a *g, struct nvgpu_gr_config *config)
{
	int err;
	u32 *tpc_sm_id;
	u32 sm_id_size = g->ops.gr.init.get_sm_id_size();

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	tpc_sm_id = nvgpu_kcalloc(g, sm_id_size, sizeof(u32));
	if (tpc_sm_id == NULL) {
		return -ENOMEM;
	}

	err = g->ops.gr.init.sm_id_config(g, tpc_sm_id, config, NULL, false);

	nvgpu_kfree(g, tpc_sm_id);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	return err;
}

int nvgpu_gr_fs_state_init(struct gk20a *g, struct nvgpu_gr_config *config)
{
	u32 tpc_index, gpc_index;
	u32 sm_id = 0;
#ifdef CONFIG_NVGPU_NON_FUSA
	u32 fuse_tpc_mask;
	u32 max_tpc_cnt;
	u32 cur_gr_instance = nvgpu_gr_get_cur_instance_id(g);
	u32 gpc_phys_id;
#endif
	u32 gpc_cnt, tpc_cnt;
	u32 num_sm;
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	g->ops.gr.init.fs_state(g);

	err = g->ops.gr.config.init_sm_id_table(g, config);
	if (err != 0) {
		return err;
	}

	num_sm = nvgpu_gr_config_get_no_of_sm(config);
	nvgpu_assert(num_sm > 0U);

	for (sm_id = 0; sm_id < num_sm; sm_id++) {
		struct nvgpu_sm_info *sm_info =
			nvgpu_gr_config_get_sm_info(config, sm_id);
		nvgpu_assert(sm_info != NULL);
		tpc_index = nvgpu_gr_config_get_sm_info_tpc_index(sm_info);
		gpc_index = nvgpu_gr_config_get_sm_info_gpc_index(sm_info);

		g->ops.gr.init.sm_id_numbering(g, gpc_index, tpc_index, sm_id,
					       config, NULL, false);
	}

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		g->ops.gr.init.pd_tpc_per_gpc(g, config);
	}

#ifdef CONFIG_NVGPU_GRAPHICS
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		/* gr__setup_pd_mapping */
		g->ops.gr.init.rop_mapping(g, config);
		g->ops.gr.init.pd_skip_table_gpc(g, config);
	}
#endif

	gpc_cnt = nvgpu_gr_config_get_gpc_count(config);
	tpc_cnt = nvgpu_gr_config_get_tpc_count(config);

#ifdef CONFIG_NVGPU_NON_FUSA
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		/*
		 * Fuse registers must be queried with physical gpc-id and not
		 * the logical ones. For tu104 and before chips logical gpc-id
		 * is same as physical gpc-id for non-floorswept config but for
		 * chips after tu104 it may not be true.
		 */
		gpc_phys_id = nvgpu_grmgr_get_gr_gpc_phys_id(g,
				cur_gr_instance, 0U);
		fuse_tpc_mask = g->ops.gr.config.get_gpc_tpc_mask(g, config, gpc_phys_id);
		max_tpc_cnt = nvgpu_gr_config_get_max_tpc_count(config);

		if ((g->tpc_fs_mask_user != 0U) &&
			(fuse_tpc_mask ==
				nvgpu_safe_sub_u32(BIT32(max_tpc_cnt), U32(1)))) {
			u32 val = g->tpc_fs_mask_user;
			val &= nvgpu_safe_sub_u32(BIT32(max_tpc_cnt), U32(1));
			tpc_cnt = (u32)hweight32(val);
		}
	}
#endif

	g->ops.gr.init.cwd_gpcs_tpcs_num(g, gpc_cnt, tpc_cnt);

	if (g->ops.gr.init.gr_load_tpc_mask != NULL) {
		g->ops.gr.init.gr_load_tpc_mask(g, config);
	}

	err = gr_load_sm_id_config(g, config);
	if (err != 0) {
		nvgpu_err(g, "load_smid_config failed err=%d", err);
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	return err;
}

int nvgpu_gr_init_sm_id_early_config(struct gk20a *g, struct nvgpu_gr_config *config)
{
	u32 tpc_index, gpc_index;
	u32 sm_id = 0;
	u32 num_sm;
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	err = g->ops.gr.config.init_sm_id_table(g, config);
	if (err != 0) {
		return err;
	}

	num_sm = nvgpu_gr_config_get_no_of_sm(config);
	nvgpu_assert(num_sm > 0U);

	for (sm_id = 0; sm_id < num_sm; sm_id++) {
		struct nvgpu_sm_info *sm_info =
			nvgpu_gr_config_get_sm_info(config, sm_id);
		tpc_index = nvgpu_gr_config_get_sm_info_tpc_index(sm_info);
		gpc_index = nvgpu_gr_config_get_sm_info_gpc_index(sm_info);

		g->ops.gr.init.sm_id_numbering(g, gpc_index, tpc_index, sm_id,
					       config, NULL, false);
	}

	return err;
}
