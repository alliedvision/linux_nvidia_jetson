/*
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
#include <nvgpu/falcon.h>
#include <nvgpu/log.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/riscv.h>
#include <nvgpu/hw/ga10b/hw_falcon_ga10b.h>
#include <nvgpu/hw/ga10b/hw_priscv_ga10b.h>

#include "hal/falcon/falcon_gk20a.h"

#include "falcon_ga10b.h"

u32 ga10b_falcon_dmemc_blk_mask(void)
{
	return falcon_falcon_dmemc_blk_m();
}

u32 ga10b_falcon_imemc_blk_field(u32 blk)
{
	return falcon_falcon_imemc_blk_f(blk);
}

bool ga10b_falcon_is_cpu_halted(struct nvgpu_falcon *flcn)
{
	if (flcn->is_falcon2_enabled) {
		return (priscv_priscv_cpuctl_halted_v(
			nvgpu_riscv_readl(flcn, priscv_priscv_cpuctl_r())) != 0U);
	} else {
		return ((nvgpu_falcon_readl(flcn, falcon_falcon_cpuctl_r()) &
			falcon_falcon_cpuctl_halt_intr_m()) != 0U);
	}
}

void ga10b_falcon_set_bcr(struct nvgpu_falcon *flcn)
{
	nvgpu_riscv_writel(flcn, priscv_priscv_bcr_ctrl_r(), 0x11);
}

void ga10b_falcon_bootstrap(struct nvgpu_falcon *flcn, u32 boot_vector)
{
	/* Need to check this through fuse/SW policy*/
	if (flcn->is_falcon2_enabled) {
		nvgpu_log_info(flcn->g, "boot riscv core");
		nvgpu_riscv_writel(flcn, priscv_priscv_cpuctl_r(),
			priscv_priscv_cpuctl_startcpu_true_f());
	} else {
		nvgpu_log_info(flcn->g, "falcon boot vec 0x%x", boot_vector);

		nvgpu_falcon_writel(flcn, falcon_falcon_dmactl_r(),
			falcon_falcon_dmactl_require_ctx_f(0));

		nvgpu_falcon_writel(flcn, falcon_falcon_bootvec_r(),
			falcon_falcon_bootvec_vec_f(boot_vector));

		nvgpu_falcon_writel(flcn, falcon_falcon_cpuctl_r(),
			falcon_falcon_cpuctl_startcpu_f(1));
	}
}

void ga10b_falcon_dump_brom_stats(struct nvgpu_falcon *flcn)
{
	u32 reg = 0;

	reg = nvgpu_falcon_readl(flcn, falcon_falcon_hwcfg2_r());
	nvgpu_falcon_dbg(flcn->g, "HWCFG2: 0x%08x", reg);

	if (falcon_falcon_hwcfg2_riscv_br_priv_lockdown_v(reg) ==
		falcon_falcon_hwcfg2_riscv_br_priv_lockdown_lock_v()) {
		nvgpu_falcon_dbg(flcn->g, "PRIV LOCKDOWN enabled");
	} else {
		nvgpu_falcon_dbg(flcn->g, "PRIV LOCKDOWN disabled");

		reg = nvgpu_riscv_readl(flcn, priscv_priscv_bcr_ctrl_r());
		nvgpu_falcon_dbg(flcn->g, "Bootrom Configuration: 0x%08x", reg);
	}

	reg = nvgpu_riscv_readl(flcn, priscv_priscv_br_retcode_r());
	nvgpu_falcon_dbg(flcn->g, "RISCV BROM RETCODE: 0x%08x", reg);
}

u32 ga10b_falcon_get_brom_retcode(struct nvgpu_falcon *flcn)
{
	return nvgpu_riscv_readl(flcn, priscv_priscv_br_retcode_r());
}

bool ga10b_falcon_is_priv_lockdown(struct nvgpu_falcon *flcn)
{
	u32 reg = nvgpu_falcon_readl(flcn, falcon_falcon_hwcfg2_r());

	if (falcon_falcon_hwcfg2_riscv_br_priv_lockdown_v(reg) ==
		falcon_falcon_hwcfg2_riscv_br_priv_lockdown_lock_v()) {
		return true;
	}

	return false;
}

bool ga10b_falcon_check_brom_passed(u32 retcode)
{
	return (priscv_priscv_br_retcode_result_v(retcode) ==
			priscv_priscv_br_retcode_result_pass_f());
}

bool ga10b_falcon_check_brom_failed(u32 retcode)
{
	return (priscv_priscv_br_retcode_result_v(retcode) ==
			priscv_priscv_br_retcode_result_fail_f());
}


void ga10b_falcon_brom_config(struct nvgpu_falcon *flcn, u64 fmc_code_addr,
		u64 fmc_data_addr, u64 manifest_addr)
{
	nvgpu_riscv_writel(flcn, priscv_priscv_bcr_dmaaddr_fmccode_lo_r(),
			u64_lo32(fmc_code_addr));
	nvgpu_riscv_writel(flcn, priscv_priscv_bcr_dmaaddr_fmccode_hi_r(),
			u64_hi32(fmc_code_addr));

	nvgpu_riscv_writel(flcn, priscv_priscv_bcr_dmaaddr_fmcdata_lo_r(),
			u64_lo32(fmc_data_addr));
	nvgpu_riscv_writel(flcn, priscv_priscv_bcr_dmaaddr_fmcdata_hi_r(),
			u64_hi32(fmc_data_addr));

	nvgpu_riscv_writel(flcn, priscv_priscv_bcr_dmaaddr_pkcparam_lo_r(),
			u64_lo32(manifest_addr));
	nvgpu_riscv_writel(flcn, priscv_priscv_bcr_dmaaddr_pkcparam_hi_r(),
			u64_hi32(manifest_addr));

	nvgpu_riscv_writel(flcn, priscv_priscv_bcr_dmacfg_r(),
			priscv_priscv_bcr_dmacfg_target_noncoherent_system_f() |
			priscv_priscv_bcr_dmacfg_lock_locked_f());

	nvgpu_riscv_writel(flcn, priscv_priscv_bcr_ctrl_r(), 0x111);
}

#ifdef CONFIG_NVGPU_FALCON_DEBUG
static void ga10b_riscv_dump_stats(struct nvgpu_falcon *flcn)
{
	struct gk20a *g = NULL;

	g = flcn->g;

	nvgpu_err(g, "<<< FALCON id-%d RISCV DEBUG INFORMATION - START >>>",
			flcn->flcn_id);

	nvgpu_err(g, " RISCV REGISTERS DUMP");
	nvgpu_err(g, "riscv_riscv_mailbox0_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_mailbox0_r()));
	nvgpu_err(g, "riscv_riscv_mailbox1_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_mailbox1_r()));
	nvgpu_err(g, "priscv_priscv_cpuctl_r : 0x%x",
		nvgpu_riscv_readl(flcn, priscv_priscv_cpuctl_r()));
	nvgpu_err(g, "priscv_riscv_irqmask_r : 0x%x",
		nvgpu_riscv_readl(flcn, priscv_riscv_irqmask_r()));
	nvgpu_err(g, "priscv_riscv_irqdest_r : 0x%x",
		nvgpu_riscv_readl(flcn, priscv_riscv_irqdest_r()));
}

void ga10b_falcon_dump_stats(struct nvgpu_falcon *flcn)
{
	if (flcn->is_falcon2_enabled) {
		ga10b_riscv_dump_stats(flcn);
	} else {
		gk20a_falcon_dump_stats(flcn);
	}
}
#endif /* CONFIG_NVGPU_FALCON_DEBUG */

bool ga10b_is_falcon_scrubbing_done(struct nvgpu_falcon *flcn)
{
	u32 hwcfg = 0U;

	hwcfg = nvgpu_falcon_readl(flcn, falcon_falcon_hwcfg2_r());

	if (falcon_falcon_hwcfg2_mem_scrubbing_v(hwcfg) ==
		falcon_falcon_hwcfg2_mem_scrubbing_pending_v()) {
		return false;
	} else {
		return true;
	}
}

bool ga10b_is_falcon_idle(struct nvgpu_falcon *flcn)
{
	u32 reg = 0U;

	if (flcn->is_falcon2_enabled == false) {
		return gk20a_is_falcon_idle(flcn);
	} else {
		reg = nvgpu_falcon_readl(flcn, falcon_falcon_hwcfg2_r());
		nvgpu_pmu_dbg(flcn->g, "HWCFG2: 0x%08x", reg);

		if (falcon_falcon_hwcfg2_riscv_br_priv_lockdown_v(reg) ==
			falcon_falcon_hwcfg2_riscv_br_priv_lockdown_lock_v()) {
			nvgpu_pmu_dbg(flcn->g, "PRIV LOCKDOWN enabled");
			return true;
		}
	}
	return true;
}
