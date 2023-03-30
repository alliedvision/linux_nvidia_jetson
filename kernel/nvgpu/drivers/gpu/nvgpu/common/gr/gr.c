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
#include <nvgpu/errata.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/io.h>
#include <nvgpu/bug.h>
#include <nvgpu/errno.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_intr.h>
#ifdef CONFIG_NVGPU_GRAPHICS
#include <nvgpu/gr/zbc.h>
#include <nvgpu/gr/zcull.h>
#endif
#include <nvgpu/netlist.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/hwpm_map.h>
#include <nvgpu/gr/obj_ctx.h>
#include <nvgpu/gr/fs_state.h>
#include <nvgpu/gr/fecs_trace.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/mc.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/device.h>
#include <nvgpu/engines.h>
#include <nvgpu/grmgr.h>

#include "gr_priv.h"

static int gr_alloc_global_ctx_buffers(struct gk20a *g, struct nvgpu_gr *gr)
{
	int err;
	u32 size;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	/*
	 * MIG supports only compute class.
	 * Allocate BUNDLE_CB, PAGEPOOL, ATTRIBUTE_CB and RTV_CB
	 * if 2D/3D/I2M classes(graphics) are supported.
	 */
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		size = g->ops.gr.init.get_global_ctx_cb_buffer_size(g);
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr,
			"cb_buffer_size : %d", size);

		nvgpu_gr_global_ctx_set_size(gr->global_ctx_buffer,
			NVGPU_GR_GLOBAL_CTX_CIRCULAR, size);
#ifdef CONFIG_NVGPU_VPR
		nvgpu_gr_global_ctx_set_size(gr->global_ctx_buffer,
			NVGPU_GR_GLOBAL_CTX_CIRCULAR_VPR, size);
#endif

		size = g->ops.gr.init.get_global_ctx_pagepool_buffer_size(g);
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr,
			"pagepool_buffer_size : %d", size);

		nvgpu_gr_global_ctx_set_size(gr->global_ctx_buffer,
			NVGPU_GR_GLOBAL_CTX_PAGEPOOL, size);
#ifdef CONFIG_NVGPU_VPR
		nvgpu_gr_global_ctx_set_size(gr->global_ctx_buffer,
			NVGPU_GR_GLOBAL_CTX_PAGEPOOL_VPR, size);
#endif
		size = g->ops.gr.init.get_global_attr_cb_size(g,
				nvgpu_gr_config_get_tpc_count(gr->config),
				nvgpu_gr_config_get_max_tpc_count(gr->config));
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr,
			"attr_buffer_size : %u", size);

		nvgpu_gr_global_ctx_set_size(gr->global_ctx_buffer,
			NVGPU_GR_GLOBAL_CTX_ATTRIBUTE, size);
#ifdef CONFIG_NVGPU_VPR
		nvgpu_gr_global_ctx_set_size(gr->global_ctx_buffer,
			NVGPU_GR_GLOBAL_CTX_ATTRIBUTE_VPR, size);
#endif

#ifdef CONFIG_NVGPU_GRAPHICS
		if (g->ops.gr.init.get_rtv_cb_size != NULL) {
			size = g->ops.gr.init.get_rtv_cb_size(g);
			nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr,
				"rtv_circular_buffer_size : %u", size);

			nvgpu_gr_global_ctx_set_size(gr->global_ctx_buffer,
				NVGPU_GR_GLOBAL_CTX_RTV_CIRCULAR_BUFFER, size);
		}
#endif
	}

	size = NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP_SIZE;
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "priv_access_map_size : %d", size);

	nvgpu_gr_global_ctx_set_size(gr->global_ctx_buffer,
		NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP, size);

#ifdef CONFIG_NVGPU_FECS_TRACE
	size = (u32)nvgpu_gr_fecs_trace_buffer_size(g);
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "fecs_trace_buffer_size : %d", size);

	nvgpu_gr_global_ctx_set_size(gr->global_ctx_buffer,
		NVGPU_GR_GLOBAL_CTX_FECS_TRACE_BUFFER, size);
#endif

	err = nvgpu_gr_global_ctx_buffer_alloc(g, gr->global_ctx_buffer);
	if (err != 0) {
		return err;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	return 0;
}

u32 nvgpu_gr_get_no_of_sm(struct gk20a *g)
{
	return nvgpu_gr_config_get_no_of_sm(g->gr->config);
}

u32 nvgpu_gr_gpc_offset(struct gk20a *g, u32 gpc)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 gpc_offset = nvgpu_safe_mult_u32(gpc_stride , gpc);

	nvgpu_assert(gpc < nvgpu_gr_config_get_gpc_count(nvgpu_gr_get_config_ptr(g)));

	return gpc_offset;
}

u32 nvgpu_gr_tpc_offset(struct gk20a *g, u32 tpc)
{
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
					GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 tpc_offset = nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc);

	nvgpu_assert(tpc < nvgpu_gr_config_get_max_tpc_per_gpc_count(nvgpu_gr_get_config_ptr(g)));

	return tpc_offset;
}

u32 nvgpu_gr_sm_offset(struct gk20a *g, u32 sm)
{
	u32 sm_pri_stride = nvgpu_get_litter_value(g, GPU_LIT_SM_PRI_STRIDE);
	u32 sm_offset = nvgpu_safe_mult_u32(sm_pri_stride, sm);

	nvgpu_assert(sm < nvgpu_gr_config_get_sm_count_per_tpc(nvgpu_gr_get_config_ptr(g)));

	return sm_offset;
}

u32 nvgpu_gr_rop_offset(struct gk20a *g, u32 rop)
{
	u32 rop_pri_stride = nvgpu_get_litter_value(g, GPU_LIT_ROP_STRIDE);
	u32 rop_offset = nvgpu_safe_mult_u32(rop_pri_stride, rop);

	return rop_offset;
}

static void disable_gr_interrupts(struct gk20a *g)
{
	/** Disable gr intr */
	g->ops.gr.intr.enable_interrupts(g, false);

	/** Disable all exceptions */
	g->ops.gr.intr.enable_exceptions(g, g->gr->config, false);

	/** Disable interrupts at MC level */
	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_GR,
					NVGPU_CIC_INTR_DISABLE);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	nvgpu_cic_mon_intr_nonstall_unit_config(g, NVGPU_CIC_INTR_UNIT_GR,
					   NVGPU_CIC_INTR_DISABLE);
#endif
}

int nvgpu_gr_suspend(struct gk20a *g)
{
	int ret = 0;

	nvgpu_log_fn(g, " ");

	ret = g->ops.gr.init.wait_empty(g);
	if (ret != 0) {
		return ret;
	}

	/* Disable fifo access */
	g->ops.gr.init.fifo_access(g, false);

	disable_gr_interrupts(g);

	g->ops.gr.intr.flush_channel_tlb(g);

	/* Clear GR Falcon state */
	nvgpu_gr_falcon_suspend(g, nvgpu_gr_get_falcon_ptr(g));

	g->gr->initialized = false;

	nvgpu_log_fn(g, "done");
	return ret;
}

static int gr_init_setup_hw(struct gk20a *g, struct nvgpu_gr *gr)
{
	int err;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	if (g->ops.gr.init.eng_config != NULL) {
		g->ops.gr.init.eng_config(g);
	}

	g->ops.gr.init.gpc_mmu(g);

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		g->ops.gr.init.pes_vsc_stream(g);
	}

	if (g->ops.priv_ring.set_ppriv_timeout_settings != NULL) {
		g->ops.priv_ring.set_ppriv_timeout_settings(g);
	}

	/** Enable fecs error interrupts */
	g->ops.gr.falcon.fecs_host_int_enable(g);
	g->ops.gr.intr.enable_hww_exceptions(g);
	/** Enable TPC exceptions per GPC */
	g->ops.gr.intr.enable_gpc_exceptions(g, gr->config);
	/** Reset and enable exceptions */
	g->ops.gr.intr.enable_exceptions(g, gr->config, true);

	/*
	 * SM HWWs are enabled during golden context creation, which happens
	 * at the time of first context creation i.e. first GPU job submission.
	 * Hence, injection of SM HWWs should only be attempted afterwards.
	 */

	/* enable ECC for L1/SM */
	if (g->ops.gr.init.ecc_scrub_reg != NULL) {
		err = g->ops.gr.init.ecc_scrub_reg(g, gr->config);
		if (err != 0) {
			goto out;
		}
	}

#ifdef CONFIG_NVGPU_GRAPHICS
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		err = nvgpu_gr_zcull_init_hw(g, gr->zcull, gr->config);
		if (err != 0) {
			goto out;
		}

		nvgpu_gr_zbc_load_table(g, gr->zbc);

#ifdef CONFIG_NVGPU_GFXP
		if (g->ops.gr.init.preemption_state != NULL) {
			err = g->ops.gr.init.preemption_state(g);
			if (err != 0) {
				goto out;
			}
		}
#endif /* CONFIG_NVGPU_GFXP */
	}
#endif /* CONFIG_NVGPU_GRAPHICS */

	/*
	 * Disable both surface and LG coalesce.
	 */
	if (g->ops.gr.init.su_coalesce != NULL) {
		g->ops.gr.init.su_coalesce(g, 0);
	}
	if (g->ops.gr.init.lg_coalesce != NULL) {
		g->ops.gr.init.lg_coalesce(g, 0);
	}

	/* floorsweep anything left */
	err = nvgpu_gr_fs_state_init(g, gr->config);
	if (err != 0) {
		goto out;
	}

	if ((nvgpu_is_errata_present(g, NVGPU_ERRATA_2557724)) &&
		(g->ops.gr.init.set_sm_l1tag_surface_collector != NULL)) {
			g->ops.gr.init.set_sm_l1tag_surface_collector(g);
	}

	err = g->ops.gr.init.wait_idle(g);
out:
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	return err;
}

static void gr_remove_support(struct gk20a *g)
{
	struct nvgpu_gr *gr = NULL;
	u32 i;

	nvgpu_log_fn(g, " ");

	nvgpu_netlist_deinit_ctx_vars(g);

	for (i = 0U; i < g->num_gr_instances; i++) {
		gr = &g->gr[i];

		nvgpu_gr_global_ctx_buffer_free(g, gr->global_ctx_buffer);
		nvgpu_gr_global_ctx_desc_free(g, gr->global_ctx_buffer);
		gr->global_ctx_buffer = NULL;

		nvgpu_gr_ctx_desc_free(g, gr->gr_ctx_desc);
		gr->gr_ctx_desc = NULL;

	#ifdef CONFIG_NVGPU_DEBUGGER
		nvgpu_gr_hwpm_map_deinit(g, gr->hwpm_map);
		gr->hwpm_map = NULL;
	#endif

		nvgpu_gr_obj_ctx_deinit(g, gr->golden_image);
		gr->golden_image = NULL;
	}

	nvgpu_gr_free(g);
}

static int gr_init_access_map(struct gk20a *g, struct nvgpu_gr *gr)
{
	struct nvgpu_mem *mem;
	u32 nr_pages =
		DIV_ROUND_UP(NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP_SIZE,
			     NVGPU_CPU_PAGE_SIZE);
	u32 nr_pages_size = nvgpu_safe_mult_u32(NVGPU_CPU_PAGE_SIZE, nr_pages);
#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
	u32 *whitelist = NULL;
	u32 w, num_entries = 0U;
#endif

	nvgpu_log(g, gpu_dbg_gr, " ");

	mem = nvgpu_gr_global_ctx_buffer_get_mem(gr->global_ctx_buffer,
			NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP);
	if (mem == NULL) {
		return -EINVAL;
	}

	nvgpu_memset(g, mem, 0, 0, nr_pages_size);

#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
	g->ops.gr.init.get_access_map(g, &whitelist, &num_entries);

	for (w = 0U; w < num_entries; w++) {
		u32 map_bit, map_byte, map_shift, x;
		map_bit = whitelist[w] >> 2;
		map_byte = map_bit >> 3;
		map_shift = map_bit & 0x7U; /* i.e. 0-7 */
		nvgpu_log_info(g, "access map addr:0x%x byte:0x%x bit:%d",
			       whitelist[w], map_byte, map_shift);
		x = nvgpu_mem_rd32(g, mem, (u64)map_byte / (u64)sizeof(u32));
		x |= BIT32(
			   (map_byte % (u32)sizeof(u32) * BITS_PER_BYTE_U32)
			  + map_shift);
		nvgpu_mem_wr32(g, mem, (u64)map_byte / (u64)sizeof(u32), x);
	}
#endif

	return 0;
}

static int gr_init_config(struct gk20a *g, struct nvgpu_gr *gr)
{
	gr->config = nvgpu_gr_config_init(g);
	if (gr->config == NULL) {
		return -ENOMEM;
	}

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "bundle_cb_default_size: %d",
		g->ops.gr.init.get_bundle_cb_default_size(g));
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "min_gpm_fifo_depth: %d",
		g->ops.gr.init.get_min_gpm_fifo_depth(g));
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "bundle_cb_token_limit: %d",
		g->ops.gr.init.get_bundle_cb_token_limit(g));
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "attrib_cb_default_size: %d",
		g->ops.gr.init.get_attrib_cb_default_size(g));
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "attrib_cb_size: %d",
		g->ops.gr.init.get_attrib_cb_size(g,
			nvgpu_gr_config_get_tpc_count(gr->config)));
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "alpha_cb_default_size: %d",
		g->ops.gr.init.get_alpha_cb_default_size(g));
	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "alpha_cb_size: %d",
		g->ops.gr.init.get_alpha_cb_size(g,
			nvgpu_gr_config_get_tpc_count(gr->config)));

	return 0;
}

static int nvgpu_gr_init_ctx_state(struct gk20a *g, struct nvgpu_gr *gr)
{
	int err = 0;

	/* Initialize ctx state during boot and recovery */
	err = nvgpu_gr_falcon_init_ctx_state(g, gr->falcon);
	if (err != 0) {
		nvgpu_err(g, "gr ctx_state init failed");
	}

	return err;
}

static int gr_init_ctx_bufs(struct gk20a *g, struct nvgpu_gr *gr)
{
	int err = 0;

	gr->gr_ctx_desc = nvgpu_gr_ctx_desc_alloc(g);
	if (gr->gr_ctx_desc == NULL) {
		err = -ENOMEM;
		goto clean_up;
	}

#ifdef CONFIG_NVGPU_GFXP
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		nvgpu_gr_ctx_set_size(gr->gr_ctx_desc,
			NVGPU_GR_CTX_PREEMPT_CTXSW,
			nvgpu_gr_falcon_get_preempt_image_size(gr->falcon));
	}
#endif

	gr->global_ctx_buffer = nvgpu_gr_global_ctx_desc_alloc(g);
	if (gr->global_ctx_buffer == NULL) {
		err = -ENOMEM;
		goto clean_up;
	}

	err = gr_alloc_global_ctx_buffers(g, gr);
	if (err != 0) {
		goto clean_up;
	}

	err = gr_init_access_map(g, gr);
	if (err != 0) {
		goto clean_up;
	}

	return 0;

clean_up:
	return err;
}

static int gr_init_ecc_init(struct gk20a *g)
{
	int err = 0;

	nvgpu_log(g, gpu_dbg_gr, " ");

	if ((g->ops.gr.ecc.gpc_tpc_ecc_init != NULL) && !g->ecc.initialized) {
		err = g->ops.gr.ecc.gpc_tpc_ecc_init(g);
		if (err != 0) {
			nvgpu_err(g, "failed to init gr gpc/tpc ecc");
			return err;
		}
	}

	nvgpu_log(g, gpu_dbg_gr, "done");
	return err;
}

static int gr_init_setup_sw(struct gk20a *g, struct nvgpu_gr *gr)
{
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	if (gr->sw_ready) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "skip init");
		return 0;
	}

	err = nvgpu_gr_obj_ctx_init(g, &gr->golden_image,
			nvgpu_gr_falcon_get_golden_image_size(gr->falcon));
	if (err != 0) {
		goto clean_up;
	}

#ifdef CONFIG_NVGPU_DEBUGGER
	err = nvgpu_gr_hwpm_map_init(g, &gr->hwpm_map,
			nvgpu_gr_falcon_get_pm_ctxsw_image_size(gr->falcon));
	if (err != 0) {
		nvgpu_err(g, "hwpm_map init failed");
		goto clean_up;
	}
#endif

	err = gr_init_ctx_bufs(g, gr);
	if (err != 0) {
		goto clean_up;
	}

#ifdef CONFIG_NVGPU_GRAPHICS
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		err = nvgpu_gr_config_init_map_tiles(g, gr->config);
		if (err != 0) {
			goto clean_up;
		}

		err = nvgpu_gr_zcull_init(g, &gr->zcull,
				nvgpu_gr_falcon_get_zcull_image_size(gr->falcon),
				gr->config);
		if (err != 0) {
			goto clean_up;
		}

		err = nvgpu_gr_zbc_init(g, &gr->zbc);
		if (err != 0) {
			goto clean_up;
		}
	} else {
		gr->zbc = NULL;
		gr->zcull = NULL;
	}
#endif /* CONFIG_NVGPU_GRAPHICS */

	gr->remove_support = gr_remove_support;
	gr->sw_ready = true;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	return 0;

clean_up:
	nvgpu_err(g, "fail");
	gr_remove_support(g);
	return err;
}

static int gr_init_prepare_hw_impl(struct gk20a *g)
{
	struct netlist_av_list *sw_non_ctx_load =
		nvgpu_netlist_get_sw_non_ctx_load_av_list(g);
	u32 i;
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "Prepare GR%u HW",
		nvgpu_gr_get_cur_instance_id(g));

	/** Enable interrupts */
	g->ops.gr.intr.enable_interrupts(g, true);

	/* enable fifo access */
	g->ops.gr.init.fifo_access(g, true);

	/* load non_ctx init */
	nvgpu_log_info(g, "begin: netlist: sw_non_ctx_load: register writes");
	for (i = 0; i < sw_non_ctx_load->count; i++) {
		nvgpu_writel(g, sw_non_ctx_load->l[i].addr,
			sw_non_ctx_load->l[i].value);
	}

	nvgpu_gr_init_reset_enable_hw_non_ctx_local(g);
	nvgpu_gr_init_reset_enable_hw_non_ctx_global(g);
	nvgpu_log_info(g, "end: netlist: sw_non_ctx_load: register writes");

	err = g->ops.gr.falcon.wait_mem_scrubbing(g);
	if (err != 0) {
		goto out;
	}

	err = g->ops.gr.init.wait_idle(g);
	if (err != 0) {
		goto out;
	}

out:
	if (err != 0) {
		nvgpu_err(g, "fail");
	} else {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	}

	return err;
}

static int gr_init_prepare_hw(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	/** Enable interrupts at MC level */
	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_GR, NVGPU_CIC_INTR_ENABLE);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	nvgpu_cic_mon_intr_nonstall_unit_config(g, NVGPU_CIC_INTR_UNIT_GR, NVGPU_CIC_INTR_ENABLE);
#endif

	return nvgpu_gr_exec_with_ret_for_each_instance(g,
			gr_init_prepare_hw_impl(g));
}

static int gr_reset_engine(struct gk20a *g)
{
	u32 cur_gr_instance_id = nvgpu_gr_get_cur_instance_id(g);
	int err;
	const struct nvgpu_device *dev =
		nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS,
			nvgpu_gr_get_syspipe_id(g, g->mig.cur_gr_instance));

	nvgpu_assert(dev != NULL);

	nvgpu_log(g, gpu_dbg_gr, "Reset GR%u", cur_gr_instance_id);

	/* Reset GR engine: Disable then enable GR engine */
	err = g->ops.mc.enable_dev(g, dev, false);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_info, "Device reset_id:%u disable failed",
			dev->reset_id);
		return err;
	}

	if (g->ops.gr.init.reset_gpcs != NULL) {
		err = g->ops.gr.init.reset_gpcs(g);
		if (err != 0) {
			nvgpu_err(g, "Reset gpcs failed");
			return err;
		}
	}

	err = g->ops.mc.enable_dev(g, dev, true);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_info, "Device reset_id:%u enable failed",
			dev->reset_id);
		return err;
	}

	/*
	 * TODO: Do not reset PERFMON in gr_reset_engine() as PERFMON is a
	 * global engine which is shared by all contexts/syspipes.
	 * Individual PERF counters can be reset during gr syspipe reset.
	 */
	err = nvgpu_mc_reset_units(g,
		NVGPU_UNIT_PERFMON | NVGPU_UNIT_BLG);
	if (err != 0) {
		nvgpu_log_info(g, "PERMON | BLG unit reset failed");
		return err;
	}

	nvgpu_log(g, gpu_dbg_gr, "done");
	return 0;
}

static int gr_reset_hw_and_load_prod(struct gk20a *g)
{
	int err;

	err = nvgpu_gr_exec_with_ret_for_each_instance(g, gr_reset_engine(g));
	if (err != 0) {
		return err;
	}

	nvgpu_gr_exec_for_all_instances(g, nvgpu_cg_init_gr_load_gating_prod(g));

	/* Disable elcg until it gets enabled later in the init*/
	nvgpu_cg_elcg_disable_no_wait(g);

	return 0;
}

int nvgpu_gr_enable_hw(struct gk20a *g)
{
	int err;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	err = gr_reset_hw_and_load_prod(g);
	if (err != 0) {
		return err;
	}

	err = gr_init_prepare_hw(g);
	if (err != 0) {
		return err;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");

	return 0;
}

#ifdef CONFIG_NVGPU_ENGINE_RESET
static int nvgpu_gr_enable_hw_for_instance(struct gk20a *g)
{
	int err;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "Enable GR%u HW",
		nvgpu_gr_get_cur_instance_id(g));

	err = gr_reset_engine(g);
	if (err != 0) {
		nvgpu_err(g, "Gr Reset failed");
		return err;
	}

	nvgpu_cg_init_gr_load_gating_prod(g);

	/* Disable elcg until it gets enabled later in the init*/
	nvgpu_cg_elcg_disable_no_wait(g);

	/** Enable interrupts at MC level */
	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_GR, NVGPU_CIC_INTR_ENABLE);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	nvgpu_cic_mon_intr_nonstall_unit_config(g, NVGPU_CIC_INTR_UNIT_GR, NVGPU_CIC_INTR_ENABLE);
#endif

	err = gr_init_prepare_hw_impl(g);
	if (err != 0) {
		nvgpu_err(g, "gr_init_prepare_hw_impl failed");
		return err;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");

	return 0;
}

int nvgpu_gr_reset(struct gk20a *g)
{
	int err;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	struct nvgpu_mutex *fecs_mutex =
		nvgpu_gr_falcon_get_fecs_mutex(gr->falcon);

	g->gr->initialized = false;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr | gpu_dbg_rec, "Resetting GR%u HW",
		nvgpu_gr_get_cur_instance_id(g));

	nvgpu_mutex_acquire(fecs_mutex);

	err = nvgpu_gr_enable_hw_for_instance(g);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_gr_enable_hw_for_instance failed");
		nvgpu_mutex_release(fecs_mutex);
		return err;
	}

	err = gr_init_setup_hw(g, gr);
	if (err != 0) {
		nvgpu_err(g, "gr_init_setup_hw failed");
		nvgpu_mutex_release(fecs_mutex);
		return err;
	}

	err = nvgpu_gr_falcon_init_ctxsw(g, gr->falcon);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_gr_falcon_init_ctxsw failed");
		nvgpu_mutex_release(fecs_mutex);
		return err;
	}

	nvgpu_mutex_release(fecs_mutex);

	/*
	 * This appears query for sw states but fecs actually inits
	 * ramchain, etc so this is hw init. Hence should be executed
	 * for every GR engine HW initialization.
	 */
	err = nvgpu_gr_init_ctx_state(g, gr);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_gr_init_ctx_state failed");
		return err;
	}

#ifdef CONFIG_NVGPU_POWER_PG
	if (g->can_elpg) {
		err = nvgpu_gr_falcon_bind_fecs_elpg(g);
		if (err != 0) {
			nvgpu_err(g, "nvgpu_gr_falcon_bind_fecs_elpg failed");
			return err;
		}
	}
#endif

	nvgpu_cg_init_gr_load_gating_prod(g);

	nvgpu_cg_elcg_enable_no_wait(g);

	/* GR is inialized, signal possible waiters */
	g->gr->initialized = true;
	nvgpu_cond_signal(&gr->init_wq);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	return err;
}
#endif

static int gr_init_sm_id_config_early(struct gk20a *g, struct nvgpu_gr *gr)
{
	int err;

	if (g->ops.gr.init.sm_id_config_early != NULL) {
		err = g->ops.gr.init.sm_id_config_early(g, gr->config);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}

static int gr_init_ctxsw_falcon_support(struct gk20a *g, struct nvgpu_gr *gr)
{
	int err;

	err = nvgpu_gr_falcon_init_ctxsw(g, gr->falcon);
	if (err != 0) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_FECS,
				GPU_FECS_CTXSW_INIT_ERROR);
		nvgpu_err (g, "FECS context switch init error");
		return err;
	}

	/*
	 * This appears query for sw states but fecs actually inits
	 * ramchain, etc so this is hw init. Hence should be executed
	 * for every GR engine HW initialization.
	 */
	err = nvgpu_gr_init_ctx_state(g, gr);
	if (err != 0) {
		return err;
	}

	return 0;
}

static int gr_init_support_impl(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "Init support for GR%u", gr->instance_id);

	gr->initialized = false;

	/* This is prerequisite for calling sm_id_config_early hal. */
	if (!gr->sw_ready) {
		err = gr_init_config(g, gr);
		if (err != 0) {
			return err;
		}
	}

	/*
	 * Move sm id programming before loading ctxsw and gpccs firmwares. This
	 * is the actual sequence expected by ctxsw ucode.
	 */
	err = gr_init_sm_id_config_early(g, gr);
	if (err != 0) {
		return err;
	}

	err = gr_init_ctxsw_falcon_support(g, gr);
	if (err != 0) {
		return err;
	}

#ifdef CONFIG_NVGPU_POWER_PG
	if (g->can_elpg) {
		err = nvgpu_gr_falcon_bind_fecs_elpg(g);
		if (err != 0) {
			return err;
		}
	}
#endif

	err = gr_init_setup_sw(g, gr);
	if (err != 0) {
		return err;
	}

	err = gr_init_setup_hw(g, gr);
	if (err != 0) {
		return err;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");

	return 0;
}

static void gr_init_support_finalize(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "Finalize support for GR%u",
		gr->instance_id);

	gr->initialized = true;
	nvgpu_cond_signal(&gr->init_wq);
}

int nvgpu_gr_init_support(struct gk20a *g)
{
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	err = nvgpu_gr_exec_with_ret_for_each_instance(g, gr_init_support_impl(g));
	if (err != 0) {
		return err;
	}

	err = gr_init_ecc_init(g);
	if (err != 0) {
		return err;
	}

	nvgpu_cg_elcg_enable_no_wait(g);

	/* GR is inialized, signal possible waiters */
	nvgpu_gr_exec_for_each_instance(g, gr_init_support_finalize(g));

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");

	return 0;
}

int nvgpu_gr_alloc(struct gk20a *g)
{
	struct nvgpu_gr *gr = NULL;
	int err;
	u32 i;

	nvgpu_log(g, gpu_dbg_gr, " ");

	/* if gr exists return */
	if (g->gr != NULL) {
		return 0;
	}

	g->num_gr_instances = nvgpu_grmgr_get_num_gr_instances(g);
	if (g->num_gr_instances == 0U) {
		nvgpu_err(g, "No GR engine enumerated");
		return -EINVAL;
	}

	/* Allocate memory for gr struct */
	g->gr = nvgpu_kzalloc(g, sizeof(*gr) * g->num_gr_instances);
	if (g->gr == NULL) {
		return -ENOMEM;
	}

	g->mig.cur_gr_instance = 0U; /* default */

	for (i = 0U; i < g->num_gr_instances; i++) {
		gr = &g->gr[i];
		gr->instance_id = i;

		gr->syspipe_id = nvgpu_grmgr_get_gr_syspipe_id(g, i);
		if (gr->syspipe_id == U32_MAX) {
			nvgpu_err(g, "failed to get syspipe id");
			err = -EINVAL;
			goto fail;
		}

		nvgpu_log(g, gpu_dbg_gr, "GR instance %u attached to GR syspipe %u",
				i, gr->syspipe_id);

		gr->falcon = nvgpu_gr_falcon_init_support(g);
		if (gr->falcon == NULL) {
			nvgpu_err(g, "failed to init gr falcon");
			err = -ENOMEM;
			goto fail;
		}

		gr->intr = nvgpu_gr_intr_init_support(g);
		if (gr->intr == NULL) {
			nvgpu_err(g, "failed to init gr intr support");
			err = -ENOMEM;
			goto fail;
		}

		gr->g = g;
		nvgpu_cond_init(&gr->init_wq);
#ifdef CONFIG_NVGPU_NON_FUSA
		nvgpu_gr_override_ecc_val(gr, g->fecs_feature_override_ecc_val);
#endif
#if defined(CONFIG_NVGPU_RECOVERY) || defined(CONFIG_NVGPU_DEBUGGER)
		nvgpu_mutex_init(&gr->ctxsw_disable_mutex);
		gr->ctxsw_disable_count = 0;
#endif
	}

	/*
	 * Initialize FECS ECC counters here before acr_construct_execute as the
	 * FECS ECC errors during FECS load need to be handled and reported
	 * using the ECC counters.
	 */
	if ((g->ops.gr.ecc.fecs_ecc_init != NULL) && !g->ecc.initialized) {
		err = g->ops.gr.ecc.fecs_ecc_init(g);
		if (err != 0) {
			nvgpu_err(g, "failed to init gr fecs ecc");
			goto fail;
		}
	}

	nvgpu_log(g, gpu_dbg_gr, "Initialized %u GR engine instances",
		g->num_gr_instances);

	return 0;

fail:
	nvgpu_gr_free(g);
	return err;
}

void nvgpu_gr_free(struct gk20a *g)
{
	struct nvgpu_gr *gr = NULL;
	u32 i;

	if (g->gr == NULL) {
		return;
	}

	for (i = 0U; i < g->num_gr_instances; i++) {
		gr = &g->gr[i];

		nvgpu_gr_config_deinit(g, gr->config);
		gr->config = NULL;

		nvgpu_gr_falcon_remove_support(g, gr->falcon);
		gr->falcon = NULL;

		nvgpu_gr_intr_remove_support(g, gr->intr);
		gr->intr = NULL;

#ifdef CONFIG_NVGPU_GRAPHICS
		nvgpu_gr_zbc_deinit(g, gr->zbc);
		nvgpu_gr_zcull_deinit(g, gr->zcull);
		gr->zbc = NULL;
		gr->zcull = NULL;
#endif /* CONFIG_NVGPU_GRAPHICS */
	}

	nvgpu_kfree(g, g->gr);
	g->gr = NULL;
}

u32 nvgpu_gr_get_syspipe_id(struct gk20a *g, u32 gr_instance_id)
{
	return g->gr[gr_instance_id].syspipe_id;
}

#if defined(CONFIG_NVGPU_RECOVERY) || defined(CONFIG_NVGPU_DEBUGGER)
/**
 * Stop processing (stall) context switches at FECS:-
 * If fecs is sent stop_ctxsw method, elpg entry/exit cannot happen
 * and may timeout. It could manifest as different error signatures
 * depending on when stop_ctxsw fecs method gets sent with respect
 * to pmu elpg sequence. It could come as pmu halt or abort or
 * maybe ext error too.
 */
int nvgpu_gr_disable_ctxsw(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	nvgpu_mutex_acquire(&gr->ctxsw_disable_mutex);

	/* check for gr->ctxsw_disable_count overflow */
	if (INT_MAX == gr->ctxsw_disable_count) {
		nvgpu_err(g, "ctxsw_disable_count overflow");
		err = -ERANGE;
		goto out;
	}

	gr->ctxsw_disable_count++;
	if (gr->ctxsw_disable_count == 1) {
#ifdef CONFIG_NVGPU_POWER_PG
		err = nvgpu_pg_elpg_disable(g);
		if (err != 0) {
			nvgpu_err(g,
				"failed to disable elpg for stop_ctxsw");
			/* stop ctxsw command is not sent */
			gr->ctxsw_disable_count--;
		} else
#endif
		{
			err = g->ops.gr.falcon.ctrl_ctxsw(g,
				NVGPU_GR_FALCON_METHOD_CTXSW_STOP, 0U, NULL);
			if (err != 0) {
				nvgpu_err(g, "failed to stop fecs ctxsw");
				/* stop ctxsw failed */
				gr->ctxsw_disable_count--;
			}
		}
	} else {
		nvgpu_log_info(g, "ctxsw disabled, ctxsw_disable_count: %d",
			gr->ctxsw_disable_count);
	}
out:
	nvgpu_mutex_release(&gr->ctxsw_disable_mutex);

	return err;
}

/* Start processing (continue) context switches at FECS */
int nvgpu_gr_enable_ctxsw(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	nvgpu_mutex_acquire(&gr->ctxsw_disable_mutex);
	if (gr->ctxsw_disable_count == 0) {
		goto ctxsw_already_enabled;
	}
	gr->ctxsw_disable_count--;
	nvgpu_assert(gr->ctxsw_disable_count >= 0);
	if (gr->ctxsw_disable_count == 0) {
		err = g->ops.gr.falcon.ctrl_ctxsw(g,
				NVGPU_GR_FALCON_METHOD_CTXSW_START, 0U, NULL);
		if (err != 0) {
			nvgpu_err(g, "failed to start fecs ctxsw");
		}
#ifdef CONFIG_NVGPU_POWER_PG
		else {
			if (nvgpu_pg_elpg_enable(g) != 0) {
				nvgpu_err(g,
					"failed to enable elpg for start_ctxsw");
			}
		}
#endif
	} else {
		nvgpu_log_info(g, "ctxsw_disable_count: %d is not 0 yet",
			gr->ctxsw_disable_count);
	}
ctxsw_already_enabled:
	nvgpu_mutex_release(&gr->ctxsw_disable_mutex);

	return err;
}
#endif

void nvgpu_gr_remove_support(struct gk20a *g)
{
	if (g->gr != NULL && g->gr->remove_support != NULL) {
		g->gr->remove_support(g);
	}
}

void nvgpu_gr_sw_ready(struct gk20a *g, bool enable)
{
	if (g->gr != NULL) {
		g->gr->sw_ready = enable;
	}
}

#ifdef CONFIG_NVGPU_NON_FUSA
/* Wait until GR is initialized */
void nvgpu_gr_wait_initialized(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	NVGPU_COND_WAIT(&gr->init_wq, gr->initialized ||
			(nvgpu_is_enabled(g, NVGPU_KERNEL_IS_DYING) ||
			nvgpu_is_enabled(g, NVGPU_DRIVER_IS_DYING)), 0U);
}
#endif

bool nvgpu_gr_is_tpc_addr(struct gk20a *g, u32 addr)
{
	u32 tpc_in_gpc_base =
		nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride =
		nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 num_tpc_per_gpc =
		nvgpu_get_litter_value(g, GPU_LIT_NUM_TPC_PER_GPC);
	u32 tpc_in_gpc_shared_base =
		nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_SHARED_BASE);
	bool is_tpc_addr_shared = ((addr >= tpc_in_gpc_shared_base) &&
			(addr < (tpc_in_gpc_shared_base + tpc_in_gpc_stride)));

	return (((addr >= tpc_in_gpc_base) &&
		(addr < (tpc_in_gpc_base +
			(num_tpc_per_gpc * tpc_in_gpc_stride)))) ||
		is_tpc_addr_shared);
}

u32 nvgpu_gr_get_tpc_num(struct gk20a *g, u32 addr)
{
	u32 i, start;
	u32 num_tpcs =
		nvgpu_get_litter_value(g, GPU_LIT_NUM_TPC_PER_GPC);
	u32 tpc_in_gpc_base =
		nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride =
		nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	for (i = 0; i < num_tpcs; i++) {
		start = tpc_in_gpc_base + (i * tpc_in_gpc_stride);
		if ((addr >= start) &&
		    (addr < (start + tpc_in_gpc_stride))) {
			return i;
		}
	}
	return 0;
}

void nvgpu_gr_init_reset_enable_hw_non_ctx_local(struct gk20a *g)
{
	u32 i = 0U;
	struct netlist_av_list *sw_non_ctx_local_compute_load =
		nvgpu_netlist_get_sw_non_ctx_local_compute_load_av_list(g);
#ifdef CONFIG_NVGPU_GRAPHICS
	struct netlist_av_list *sw_non_ctx_local_gfx_load =
		nvgpu_netlist_get_sw_non_ctx_local_gfx_load_av_list(g);
#endif

	for (i = 0U; i < sw_non_ctx_local_compute_load->count; i++) {
		nvgpu_writel(g, sw_non_ctx_local_compute_load->l[i].addr,
			sw_non_ctx_local_compute_load->l[i].value);
	}

#ifdef CONFIG_NVGPU_GRAPHICS
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		for (i = 0U; i < sw_non_ctx_local_gfx_load->count; i++) {
			nvgpu_writel(g, sw_non_ctx_local_gfx_load->l[i].addr,
				sw_non_ctx_local_gfx_load->l[i].value);
		}
	}
#endif

	return;
}

void nvgpu_gr_init_reset_enable_hw_non_ctx_global(struct gk20a *g)
{
	u32 i = 0U;
	struct netlist_av_list *sw_non_ctx_global_compute_load =
		nvgpu_netlist_get_sw_non_ctx_global_compute_load_av_list(g);
#ifdef CONFIG_NVGPU_GRAPHICS
	struct netlist_av_list *sw_non_ctx_global_gfx_load =
		nvgpu_netlist_get_sw_non_ctx_global_gfx_load_av_list(g);
#endif

	for (i = 0U; i < sw_non_ctx_global_compute_load->count; i++) {
		nvgpu_writel(g, sw_non_ctx_global_compute_load->l[i].addr,
			sw_non_ctx_global_compute_load->l[i].value);
	}

#ifdef CONFIG_NVGPU_GRAPHICS
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		for (i = 0U; i < sw_non_ctx_global_gfx_load->count; i++) {
			nvgpu_writel(g, sw_non_ctx_global_gfx_load->l[i].addr,
				sw_non_ctx_global_gfx_load->l[i].value);
		}
	}
#endif

	return;
}
