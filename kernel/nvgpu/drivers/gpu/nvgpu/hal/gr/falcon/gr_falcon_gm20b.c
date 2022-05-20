/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/io.h>
#include <nvgpu/debug.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/soc.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/gr_utils.h>

#include "gr_falcon_gm20b.h"
#include "common/gr/gr_falcon_priv.h"
#include <nvgpu/gr/gr_utils.h>

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

#define GR_FECS_POLL_INTERVAL	5U /* usec */

#define FECS_ARB_CMD_TIMEOUT_MAX_US 40U
#define FECS_ARB_CMD_TIMEOUT_DEFAULT_US 2U
#define CTXSW_MEM_SCRUBBING_TIMEOUT_MAX_US 1000U
#define CTXSW_MEM_SCRUBBING_TIMEOUT_DEFAULT_US 10U

#define CTXSW_WDT_DEFAULT_VALUE 0x7FFFFFFFU
#define CTXSW_INTR0 BIT32(0)
#define CTXSW_INTR1 BIT32(1)

#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
void gm20b_gr_falcon_gpccs_dmemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc)
{
	nvgpu_writel(g, gr_gpccs_dmemc_r(port),
			gr_gpccs_dmemc_offs_f(offs) |
			gr_gpccs_dmemc_blk_f(blk) |
			gr_gpccs_dmemc_aincw_f(ainc));
}

void gm20b_gr_falcon_load_gpccs_dmem(struct gk20a *g,
			const u32 *ucode_u32_data, u32 ucode_u32_size)
{
	u32 i;
	u32 checksum = 0;

	/* enable access for gpccs dmem */
	g->ops.gr.falcon.gpccs_dmemc_write(g, 0U, 0U, 0U, 1U);

	for (i = 0; i < ucode_u32_size; i++) {
		nvgpu_writel(g, gr_gpccs_dmemd_r(0), ucode_u32_data[i]);
		checksum = nvgpu_gr_checksum_u32(checksum, ucode_u32_data[i]);
	}
	nvgpu_log_info(g, "gpccs dmem checksum: 0x%x", checksum);
}

void gm20b_gr_falcon_load_fecs_dmem(struct gk20a *g,
			const u32 *ucode_u32_data, u32 ucode_u32_size)
{
	u32 i;
	u32 checksum = 0;

	/* set access for fecs dmem */
	g->ops.gr.falcon.fecs_dmemc_write(g, 0U, 0U, 0U, 0U, 1U);

	for (i = 0; i < ucode_u32_size; i++) {
		nvgpu_writel(g, gr_fecs_dmemd_r(0), ucode_u32_data[i]);
		checksum = nvgpu_gr_checksum_u32(checksum, ucode_u32_data[i]);
	}
	nvgpu_log_info(g, "fecs dmem checksum: 0x%x", checksum);
}

void gm20b_gr_falcon_gpccs_imemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc)
{
	nvgpu_writel(g, gr_gpccs_imemc_r(port),
			gr_gpccs_imemc_offs_f(offs) |
			gr_gpccs_imemc_blk_f(blk) |
			gr_gpccs_imemc_aincw_f(ainc));
}

void gm20b_gr_falcon_load_gpccs_imem(struct gk20a *g,
			const u32 *ucode_u32_data, u32 ucode_u32_size)
{
	u32 cfg, gpccs_imem_size;
	u32 tag, i, pad_start, pad_end;
	u32 checksum = 0;

	/* enable access for gpccs imem */
	g->ops.gr.falcon.gpccs_imemc_write(g, 0U, 0U, 0U, 1U);

	cfg = nvgpu_readl(g, gr_gpc0_cfg_r());
	gpccs_imem_size = gr_gpc0_cfg_imem_sz_v(cfg);

	/* Setup the tags for the instruction memory. */
	tag = 0;
	nvgpu_writel(g, gr_gpccs_imemt_r(0), gr_gpccs_imemt_tag_f(tag));

	for (i = 0; i < ucode_u32_size; i++) {
		if ((i != 0U) && ((i % (256U/sizeof(u32))) == 0U)) {
			tag = nvgpu_safe_add_u32(tag, 1U);
			nvgpu_writel(g, gr_gpccs_imemt_r(0),
					gr_gpccs_imemt_tag_f(tag));
		}
		nvgpu_writel(g, gr_gpccs_imemd_r(0), ucode_u32_data[i]);
		checksum = nvgpu_gr_checksum_u32(checksum, ucode_u32_data[i]);
	}

	pad_start = nvgpu_safe_mult_u32(i, 4U);
	pad_end = nvgpu_safe_add_u32(pad_start, nvgpu_safe_add_u32(
			nvgpu_safe_sub_u32(256U, (pad_start % 256U)), 256U));
	for (i = pad_start;
		(i < nvgpu_safe_mult_u32(gpccs_imem_size, 256U)) &&
						(i < pad_end); i += 4U) {
		if ((i != 0U) && ((i % 256U) == 0U)) {
			tag = nvgpu_safe_add_u32(tag, 1U);
			nvgpu_writel(g, gr_gpccs_imemt_r(0),
					gr_gpccs_imemt_tag_f(tag));
		}
		nvgpu_writel(g, gr_gpccs_imemd_r(0), 0);
	}

	nvgpu_log_info(g, "gpccs imem checksum: 0x%x", checksum);
}

void gm20b_gr_falcon_fecs_imemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc)
{
	nvgpu_writel(g, gr_fecs_imemc_r(port),
			gr_fecs_imemc_offs_f(offs) |
			gr_fecs_imemc_blk_f(blk) |
			gr_fecs_imemc_aincw_f(ainc));
}

void gm20b_gr_falcon_load_fecs_imem(struct gk20a *g,
			const u32 *ucode_u32_data, u32 ucode_u32_size)
{
	u32 cfg, fecs_imem_size;
	u32 tag, i, pad_start, pad_end;
	u32 checksum = 0;

	/* set access for fecs imem */
	g->ops.gr.falcon.fecs_imemc_write(g, 0U, 0U, 0U, 1U);

	cfg = nvgpu_readl(g, gr_fecs_cfg_r());
	fecs_imem_size = gr_fecs_cfg_imem_sz_v(cfg);

	/* Setup the tags for the instruction memory. */
	tag = 0;
	nvgpu_writel(g, gr_fecs_imemt_r(0), gr_fecs_imemt_tag_f(tag));

	for (i = 0; i < ucode_u32_size; i++) {
		if ((i != 0U) && ((i % (256U/sizeof(u32))) == 0U)) {
			tag = nvgpu_safe_add_u32(tag, 1U);
			nvgpu_writel(g, gr_fecs_imemt_r(0),
				      gr_fecs_imemt_tag_f(tag));
		}
		nvgpu_writel(g, gr_fecs_imemd_r(0), ucode_u32_data[i]);
		checksum = nvgpu_gr_checksum_u32(checksum, ucode_u32_data[i]);
	}

	pad_start = nvgpu_safe_mult_u32(i, 4U);
	pad_end = nvgpu_safe_add_u32(pad_start, nvgpu_safe_add_u32(
			nvgpu_safe_sub_u32(256U, (pad_start % 256U)), 256U));
	for (i = pad_start;
	     (i < nvgpu_safe_mult_u32(fecs_imem_size, 256U)) && i < pad_end;
	     i += 4U) {
		if ((i != 0U) && ((i % 256U) == 0U)) {
			tag = nvgpu_safe_add_u32(tag, 1U);
			nvgpu_writel(g, gr_fecs_imemt_r(0),
				      gr_fecs_imemt_tag_f(tag));
		}
		nvgpu_writel(g, gr_fecs_imemd_r(0), 0);
	}
	nvgpu_log_info(g, "fecs imem checksum: 0x%x", checksum);
}

void gm20b_gr_falcon_start_ucode(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	g->ops.gr.falcon.fecs_ctxsw_clear_mailbox(g, 0U, (~U32(0U)));

	nvgpu_writel(g, gr_gpccs_dmactl_r(), gr_gpccs_dmactl_require_ctx_f(0U));
	nvgpu_writel(g, gr_fecs_dmactl_r(), gr_fecs_dmactl_require_ctx_f(0U));

	nvgpu_writel(g, gr_gpccs_cpuctl_r(), gr_gpccs_cpuctl_startcpu_f(1U));
	nvgpu_writel(g, gr_fecs_cpuctl_r(), gr_fecs_cpuctl_startcpu_f(1U));

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
}
#endif

#ifdef CONFIG_NVGPU_SIM
void gm20b_gr_falcon_configure_fmodel(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_writel(g, gr_fecs_ctxsw_mailbox_r(7),
		gr_fecs_ctxsw_mailbox_value_f(0xc0de7777U));
	nvgpu_writel(g, gr_gpccs_ctxsw_mailbox_r(7),
		gr_gpccs_ctxsw_mailbox_value_f(0xc0de7777U));

}
#endif

/* The following is a less brittle way to call gr_gk20a_submit_fecs_method(...)
 * We should replace most, if not all, fecs method calls to this instead.
 */

/* Sideband mailbox writes are done a bit differently */
#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
void gm20b_gr_falcon_fecs_host_int_enable(struct gk20a *g)
{
	nvgpu_writel(g, gr_fecs_host_int_enable_r(),
		     gr_fecs_host_int_enable_ctxsw_intr1_enable_f() |
		     gr_fecs_host_int_enable_fault_during_ctxsw_enable_f() |
		     gr_fecs_host_int_enable_umimp_firmware_method_enable_f() |
		     gr_fecs_host_int_enable_umimp_illegal_method_enable_f() |
		     gr_fecs_host_int_enable_watchdog_enable_f());
}
#endif
