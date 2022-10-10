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
#include <nvgpu/soc.h>
#include <nvgpu/io.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/netlist.h>
#include <nvgpu/gr/ctx.h>

#include "gr_init_gm20b.h"
#include "gr_init_tu104.h"

#include <nvgpu/hw/tu104/hw_gr_tu104.h>

u32 tu104_gr_init_get_bundle_cb_default_size(struct gk20a *g)
{
	(void)g;
	return gr_scc_bundle_cb_size_div_256b__prod_v();
}

u32 tu104_gr_init_get_min_gpm_fifo_depth(struct gk20a *g)
{
	(void)g;
	return gr_pd_ab_dist_cfg2_state_limit_min_gpm_fifo_depths_v();
}

u32 tu104_gr_init_get_bundle_cb_token_limit(struct gk20a *g)
{
	(void)g;
	return gr_pd_ab_dist_cfg2_token_limit_init_v();
}

u32 tu104_gr_init_get_attrib_cb_default_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v();
}

u32 tu104_gr_init_get_alpha_cb_default_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_alpha_cb_size_v_default_v();
}

static bool tu104_gr_init_is_allowed_sw_bundle64(struct gk20a *g,
		u32 bundle_addr, u32 bundle_hi_value,
		u32 bundle_lo_value, int *context)
{
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		nvgpu_log(g, gpu_dbg_mig,
			"Allowed bundle64 addr[%x] hi_value[%x] lo_value[%x] ",
			bundle_addr, bundle_hi_value, bundle_lo_value);
		return true;
	}
	/*
	 * Capture whether the current bundle is compute or not.
	 * Store in context.
	 */
	if (gr_pipe_bundle_address_value_v(bundle_addr) ==
			GR_PIPE_MODE_BUNDLE) {
		*context = ((bundle_hi_value == 0U) &&
			(bundle_lo_value == GR_PIPE_MODE_MAJOR_COMPUTE));
		nvgpu_log(g, gpu_dbg_mig, "(MIG) Bundle64 start "
			"addr[%x] hi_value[%x] lo_value[%x] "
				"is_compute_start[%d] ",
			bundle_addr, bundle_hi_value, bundle_lo_value,
			(*context != 0));
		return *context != 0;
	}

	/* And now use context, only compute bundles allowed in MIG. */
	if (*context == 0) {
		nvgpu_log(g, gpu_dbg_mig, "(MIG) Skipped bundle "
			"addr[%x] hi_value[%x] lo_value[%x] ",
			bundle_addr, bundle_hi_value, bundle_lo_value);
		return false;
	}

	nvgpu_log(g, gpu_dbg_mig, "(MIG) Compute bundle "
		"addr[%x] hi_value[%x] lo_value[%x] ",
		bundle_addr, bundle_hi_value, bundle_lo_value);

	return true;
}

int tu104_gr_init_load_sw_bundle64(struct gk20a *g,
		struct netlist_av64_list *sw_bundle64_init)
{
	u32 i;
	u32 last_bundle_data_lo = 0;
	u32 last_bundle_data_hi = 0;
	int err = 0;
	int context = 0;

	for (i = 0U; i < sw_bundle64_init->count; i++) {
		if (!tu104_gr_init_is_allowed_sw_bundle64(g,
				sw_bundle64_init->l[i].addr,
				sw_bundle64_init->l[i].value_hi,
				sw_bundle64_init->l[i].value_lo,
				&context)) {
			continue;
		}

		if (i == 0U ||
		   (last_bundle_data_lo != sw_bundle64_init->l[i].value_lo) ||
		   (last_bundle_data_hi != sw_bundle64_init->l[i].value_hi)) {
			nvgpu_writel(g, gr_pipe_bundle_data_r(),
				sw_bundle64_init->l[i].value_lo);
			nvgpu_writel(g, gr_pipe_bundle_data_hi_r(),
				sw_bundle64_init->l[i].value_hi);

			last_bundle_data_lo = sw_bundle64_init->l[i].value_lo;
			last_bundle_data_hi = sw_bundle64_init->l[i].value_hi;
		}

		nvgpu_writel(g, gr_pipe_bundle_address_r(),
			sw_bundle64_init->l[i].addr);

		if (gr_pipe_bundle_address_value_v(sw_bundle64_init->l[i].addr)
				== GR_GO_IDLE_BUNDLE) {
			err = g->ops.gr.init.wait_idle(g);
		} else {
			if (nvgpu_platform_is_silicon(g)) {
				err = g->ops.gr.init.wait_fe_idle(g);
			}
		}
		if (err != 0) {
			break;
		}
	}

	return err;
}

#ifdef CONFIG_NVGPU_GRAPHICS
u32 tu104_gr_init_get_rtv_cb_size(struct gk20a *g)
{
	(void)g;
	return nvgpu_safe_mult_u32(
		nvgpu_safe_add_u32(
			gr_scc_rm_rtv_cb_size_div_256b_default_f(),
			gr_scc_rm_rtv_cb_size_div_256b_db_adder_f()),
		gr_scc_bundle_cb_size_div_256b_byte_granularity_v());
}

static void tu104_gr_init_patch_rtv_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	u32 addr, u32 size, u32 gfxpAddSize, bool patch)
{
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_scc_rm_rtv_cb_base_r(),
		gr_scc_rm_rtv_cb_base_addr_39_8_f(addr), patch);
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_scc_rm_rtv_cb_size_r(),
		gr_scc_rm_rtv_cb_size_div_256b_f(size), patch);
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_gcc_rm_rtv_cb_base_r(),
		gr_gpcs_gcc_rm_rtv_cb_base_addr_39_8_f(addr), patch);
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_scc_rm_gfxp_reserve_r(),
		gr_scc_rm_gfxp_reserve_rtv_cb_size_div_256b_f(gfxpAddSize),
		patch);
}

void tu104_gr_init_commit_rtv_cb(struct gk20a *g, u64 addr,
	struct nvgpu_gr_ctx *gr_ctx, bool patch)
{
	u32 size = nvgpu_safe_add_u32(
			gr_scc_rm_rtv_cb_size_div_256b_default_f(),
			gr_scc_rm_rtv_cb_size_div_256b_db_adder_f());

	addr = addr >> gr_scc_rm_rtv_cb_base_addr_39_8_align_bits_f();

	nvgpu_assert(u64_hi32(addr) == 0U);
	tu104_gr_init_patch_rtv_cb(g, gr_ctx, (u32)addr, size, 0, patch);
}
#endif /* CONFIG_NVGPU_GRAPHICS */

#ifdef CONFIG_NVGPU_GFXP
void tu104_gr_init_commit_gfxp_rtv_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, bool patch)
{
	u64 addr;
	u32 rtv_cb_size;
	u32 gfxp_addr_size;
	struct nvgpu_mem *buf_mem;

	nvgpu_log_fn(g, " ");

	rtv_cb_size = nvgpu_safe_add_u32(
		nvgpu_safe_add_u32(
			gr_scc_rm_rtv_cb_size_div_256b_default_f(),
			gr_scc_rm_rtv_cb_size_div_256b_db_adder_f()),
		gr_scc_rm_rtv_cb_size_div_256b_gfxp_adder_f());
	gfxp_addr_size = gr_scc_rm_rtv_cb_size_div_256b_gfxp_adder_f();

	/* GFXP RTV circular buffer */
	buf_mem = nvgpu_gr_ctx_get_gfxp_rtvcb_ctxsw_buffer(gr_ctx);
	addr = buf_mem->gpu_va >>
			gr_scc_rm_rtv_cb_base_addr_39_8_align_bits_f();

	nvgpu_assert(u64_hi32(addr) == 0U);
	tu104_gr_init_patch_rtv_cb(g, gr_ctx, (u32)addr,
		rtv_cb_size, gfxp_addr_size, patch);
}

u32 tu104_gr_init_get_attrib_cb_gfxp_default_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v();
}

u32 tu104_gr_init_get_attrib_cb_gfxp_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v();
}

u32 tu104_gr_init_get_ctx_spill_size(struct gk20a *g)
{
	(void)g;
	return  nvgpu_safe_mult_u32(
		gr_gpc0_swdx_rm_spill_buffer_size_256b_default_v(),
		gr_gpc0_swdx_rm_spill_buffer_size_256b_byte_granularity_v());
}

u32 tu104_gr_init_get_ctx_betacb_size(struct gk20a *g)
{
	return nvgpu_safe_add_u32(
		g->ops.gr.init.get_attrib_cb_default_size(g),
		nvgpu_safe_sub_u32(
			gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v(),
			gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v()));
}

u32 tu104_gr_init_get_gfxp_rtv_cb_size(struct gk20a *g)
{
	(void)g;
	return nvgpu_safe_mult_u32(
		nvgpu_safe_add_u32(
			nvgpu_safe_add_u32(
				gr_scc_rm_rtv_cb_size_div_256b_default_f(),
				gr_scc_rm_rtv_cb_size_div_256b_db_adder_f()),
			gr_scc_rm_rtv_cb_size_div_256b_gfxp_adder_f()),
		gr_scc_rm_rtv_cb_size_div_256b_byte_granularity_v());
}
#endif /* CONFIG_NVGPU_GFXP */
