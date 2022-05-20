/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/ltc.h>
#include <nvgpu/io.h>
#include <nvgpu/timers.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/trace.h>
#include <nvgpu/regops.h>

#include "hal/gr/gr/gr_gk20a.h"
#include "ltc_tu104.h"
#include "ltc_gv11b.h"

#include <nvgpu/hw/tu104/hw_ltc_tu104.h>

void ltc_tu104_init_fs_state(struct gk20a *g)
{
	u32 reg;
	u32 line_size = 512U;

	gv11b_ltc_init_fs_state(g);

	reg = nvgpu_readl(g, ltc_ltcs_ltss_cbc_param2_r());
	g->ltc->slices_per_ltc =
		ltc_ltcs_ltss_cbc_param2_slices_per_ltc_v(reg);
	g->ltc->cacheline_size =
		line_size << ltc_ltcs_ltss_cbc_param2_cache_line_size_v(reg);

	/* disable PLC compression */
	reg = nvgpu_readl(g, ltc_ltcs_ltss_tstg_set_mgmt_1_r());
	reg = set_field(reg,
		ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_plc_m(),
		ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_plc_disabled_f());
	reg = set_field(reg,
		ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_rmw_m(),
		ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_rmw_disabled_f());
	nvgpu_writel(g, ltc_ltcs_ltss_tstg_set_mgmt_1_r(), reg);
}

#ifdef CONFIG_NVGPU_DEBUGGER
u32 tu104_ltc_pri_is_lts_tstg_addr(struct gk20a *g, u32 addr)
{
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);
	u32 ltc_addr_mask = nvgpu_safe_sub_u32(ltc_stride, 1);
	u32 lts_addr_mask = nvgpu_safe_sub_u32(lts_stride, 1);
	u32 ltc_addr = addr & ltc_addr_mask;
	u32 lts_addr = ltc_addr & lts_addr_mask;

	return (lts_addr >= LTS_TSTG_BASE && lts_addr <= LTS_TSTG_EXTENT) ?
			    true : false;
}

int tu104_set_l2_sector_promotion(struct gk20a *g, struct nvgpu_tsg *tsg,
		u32 policy)
{
	int err = 0;
	struct nvgpu_dbg_reg_op cfg_ops[2] = {
		{
			.op = REGOP(READ_32),
			.type = REGOP(TYPE_GR_CTX),
			.offset = ltc_ltcs_ltss_tstg_cfg2_r()
		},
		{
			.op = REGOP(READ_32),
			.type = REGOP(TYPE_GR_CTX),
			.offset = ltc_ltcs_ltss_tstg_cfg3_r()
		},
	};
	u32 flags = NVGPU_REG_OP_FLAG_MODE_ALL_OR_NONE;
	u32 num_ops = 2U;
	u32 cfg2_vidmem = 0U, cfg3_sysmem = 0U;

	/*
	 * Read current value for ltc_ltcs_ltss_tstg_cfg(2,3)_r
	 */
	err = gr_gk20a_exec_ctx_ops(tsg, cfg_ops, num_ops, 0, num_ops, &flags);
	if (err != 0) {
		nvgpu_err(g, "failed to read ltcs_ltss_tstg_cfg(2,3)_r");
		goto fail;
	}
	cfg2_vidmem = cfg_ops[0].value_lo;
	cfg3_sysmem = cfg_ops[1].value_lo;

#define APPLY_SECTOR_PROMOTION_POLICY(cfg, unit, policy)				\
	do {										\
		switch (policy) {							\
		case NVGPU_L2_SECTOR_PROMOTE_FLAG_NONE:					\
			cfg = set_field(cfg,						\
				ltc_ltcs_ltss_tstg_##cfg##_##unit##_promote_m(),	\
				ltc_ltcs_ltss_tstg_##cfg##_##unit##_promote_f(		\
				ltc_ltcs_ltss_tstg_##cfg##_##unit##_promote_none_v()	\
				));							\
			break;								\
		case NVGPU_L2_SECTOR_PROMOTE_FLAG_64B:					\
			cfg = set_field(cfg,						\
				ltc_ltcs_ltss_tstg_##cfg##_##unit##_promote_m(),	\
				ltc_ltcs_ltss_tstg_##cfg##_##unit##_promote_f(		\
				ltc_ltcs_ltss_tstg_##cfg##_##unit##_promote_64b_v()	\
				));							\
			break;								\
		case NVGPU_L2_SECTOR_PROMOTE_FLAG_128B:					\
			cfg = set_field(cfg,						\
				ltc_ltcs_ltss_tstg_##cfg##_##unit##_promote_m(),	\
				ltc_ltcs_ltss_tstg_##cfg##_##unit##_promote_f(		\
				ltc_ltcs_ltss_tstg_##cfg##_##unit##_promote_128b_v()	\
				));							\
			break;								\
		}									\
	} while (0)

	/*
	 * Update T1_PROMOTE and L1_PROMOTE fields of cfg2_vidmem and
	 * cfg3_sysmem.
	 */
	APPLY_SECTOR_PROMOTION_POLICY(cfg2_vidmem, t1, policy);
	APPLY_SECTOR_PROMOTION_POLICY(cfg2_vidmem, l1, policy);
	APPLY_SECTOR_PROMOTION_POLICY(cfg3_sysmem, t1, policy);
	APPLY_SECTOR_PROMOTION_POLICY(cfg3_sysmem, l1, policy);

#undef APPLY_SECTOR_PROMOTION_POLICY

	cfg_ops[0].op = REGOP(WRITE_32);
	cfg_ops[0].value_lo = cfg2_vidmem;
	cfg_ops[1].op = REGOP(WRITE_32);
	cfg_ops[1].value_lo = cfg3_sysmem;
	err = gr_gk20a_exec_ctx_ops(tsg, cfg_ops, num_ops, num_ops, 0, &flags);
	if (err != 0) {
		nvgpu_err(g, "failed to update ltcs_ltss_tstg_cfg(2,3)_r");
		goto fail;
	}

	/* Readback and verify the write */
	cfg_ops[0].op = REGOP(READ_32);
	cfg_ops[0].value_lo = 0U;
	cfg_ops[1].op = REGOP(READ_32);
	cfg_ops[1].value_lo = 0U;
	err = gr_gk20a_exec_ctx_ops(tsg, cfg_ops, num_ops, 0, num_ops, &flags);
	if (err != 0) {
		nvgpu_err(g, "failed to read ltcs_ltss_tstg_cfg(2,3)_r");
		goto fail;
	}
	if (cfg2_vidmem != cfg_ops[0].value_lo || cfg3_sysmem != cfg_ops[1].value_lo) {
		nvgpu_err(g, "mismatch: cfg2: wrote(0x%x) read(0x%x)",
				cfg_ops[0].value_lo, cfg2_vidmem);
		nvgpu_err(g, "          cfg3: wrote(0x%x) read(0x%x)",
				cfg_ops[1].value_lo, cfg3_sysmem);
		err = -EINVAL;
	}

fail:
	return err;
}
#endif
