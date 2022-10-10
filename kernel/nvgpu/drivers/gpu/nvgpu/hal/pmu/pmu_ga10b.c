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
#include <nvgpu/pmu.h>
#include <nvgpu/falcon.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/firmware.h>
#include <nvgpu/mm.h>
#include <nvgpu/io.h>
#include <nvgpu/soc.h>
#include <nvgpu/cic_mon.h>

#include "pmu_gk20a.h"
#include "pmu_gv11b.h"
#include "pmu_ga10b.h"

#include <nvgpu/hw/ga10b/hw_pwr_ga10b.h>

bool ga10b_is_pmu_supported(struct gk20a *g)
{
	(void)g;
#ifdef CONFIG_NVGPU_LS_PMU
	if (nvgpu_platform_is_silicon(g)) {
		return true;
	} else {
		/* Pre-Si platforms */
		if (nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
			/* Security is enabled - PMU is supported. */
			return true;
		} else {
			/* NS PMU is not supported */
			return false;
		}
	}
#else
	/* set to false to disable LS PMU ucode support */
	return false;
#endif
}

u32 ga10b_pmu_falcon2_base_addr(void)
{
	return pwr_falcon2_pwr_base_r();
}

u32 ga10b_pmu_get_irqmask(struct gk20a *g)
{
	u32 mask = 0U;

	if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
		nvgpu_pmu_dbg(g, "RISCV core INTR");
		mask = nvgpu_readl(g, pwr_riscv_irqmask_r());
		mask &= nvgpu_readl(g, pwr_riscv_irqdest_r());
	} else {
		nvgpu_pmu_dbg(g, "Falcon core INTR");
		mask = nvgpu_readl(g, pwr_falcon_irqmask_r());
		mask &= nvgpu_readl(g, pwr_falcon_irqdest_r());
	}

	return mask;
}

#ifdef CONFIG_NVGPU_LS_PMU
static int ga10b_pmu_ns_falcon_bootstrap(struct gk20a *g, struct nvgpu_pmu *pmu,
			u32 args_offset)
{
	struct mm_gk20a *mm = &g->mm;
	struct nvgpu_firmware *fw = NULL;
	struct pmu_ucode_desc_v1 *desc = NULL;
	u32 addr_code_lo, addr_data_lo, addr_load_lo;
	u32 addr_code_hi, addr_data_hi;
	u32  blocks, i;
	u32 inst_block_ptr;
	int err;

	nvgpu_log_fn(g, " ");

	fw = nvgpu_pmu_fw_desc_desc(g, pmu);
	desc = (struct pmu_ucode_desc_v1 *)(void *)fw->data;

	nvgpu_writel(g, pwr_falcon_itfen_r(),
		nvgpu_readl(g, pwr_falcon_itfen_r()) |
		pwr_falcon_itfen_ctxen_enable_f());

	inst_block_ptr = nvgpu_inst_block_ptr(g, &mm->pmu.inst_block);
	nvgpu_writel(g, pwr_pmu_new_instblk_r(),
			pwr_pmu_new_instblk_ptr_f(inst_block_ptr) |
			pwr_pmu_new_instblk_valid_f(1) |
				(nvgpu_is_enabled(g, NVGPU_USE_COHERENT_SYSMEM) ?
			pwr_pmu_new_instblk_target_sys_coh_f() :
			pwr_pmu_new_instblk_target_sys_ncoh_f()));

	nvgpu_writel(g, pwr_falcon_dmemc_r(0),
			pwr_falcon_dmemc_offs_f(0) |
			pwr_falcon_dmemc_blk_f(0)  |
			pwr_falcon_dmemc_aincw_f(1));

	addr_code_lo = u64_lo32(right_shift_8bits((pmu->fw->ucode.gpu_va +
				desc->app_start_offset +
				desc->app_resident_code_offset)));

	addr_code_hi = u64_hi32(right_shift_8bits((pmu->fw->ucode.gpu_va +
				desc->app_start_offset +
				desc->app_resident_code_offset)));
	addr_data_lo = u64_lo32(right_shift_8bits((pmu->fw->ucode.gpu_va +
				desc->app_start_offset +
				desc->app_resident_data_offset)));
	addr_data_hi = u64_hi32(right_shift_8bits((pmu->fw->ucode.gpu_va +
				desc->app_start_offset +
				desc->app_resident_data_offset)));
	addr_load_lo = u64_lo32(right_shift_8bits((pmu->fw->ucode.gpu_va +
				desc->bootloader_start_offset)));

	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), GK20A_PMU_DMAIDX_UCODE);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), left_shift_8bits(addr_code_lo));
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), addr_code_hi);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_code_offset);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_code_size);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_0);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), desc->app_imem_entry);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), left_shift_8bits(addr_data_lo));
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), addr_data_hi);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_data_size);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), DMEM_DATA_1);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), args_offset);

	g->ops.pmu.write_dmatrfbase(g,
		addr_load_lo -
		(right_shift_8bits(desc->bootloader_imem_offset)));

	blocks = right_shift_8bits(((desc->bootloader_size + U8_MAX) & ~(u32)U8_MAX));

	for (i = DMA_OFFSET_START; i < blocks; i++) {
		nvgpu_writel(g, pwr_falcon_dmatrfmoffs_r(),
			desc->bootloader_imem_offset + left_shift_8bits(i));
		nvgpu_writel(g, pwr_falcon_dmatrffboffs_r(),
			desc->bootloader_imem_offset + left_shift_8bits(i));
		nvgpu_writel(g, pwr_falcon_dmatrfcmd_r(),
			pwr_falcon_dmatrfcmd_imem_f(1)  |
			pwr_falcon_dmatrfcmd_write_f(0) |
			pwr_falcon_dmatrfcmd_size_f(6)  |
			pwr_falcon_dmatrfcmd_ctxdma_f(GK20A_PMU_DMAIDX_UCODE));
	}

	err = nvgpu_falcon_bootstrap(pmu->flcn, desc->bootloader_entry_point);

	nvgpu_writel(g, pwr_falcon_os_r(), desc->app_version);

	return err;
}

u32 ga10b_pmu_get_inst_block_config(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	u32 inst_block_ptr = 0;

	inst_block_ptr = nvgpu_inst_block_ptr(g, &mm->pmu.inst_block);
	return (pwr_pmu_new_instblk_ptr_f(inst_block_ptr) |
			pwr_pmu_new_instblk_valid_f(1) |
			(nvgpu_is_enabled(g, NVGPU_USE_COHERENT_SYSMEM) ?
				pwr_pmu_new_instblk_target_sys_coh_f() :
				pwr_pmu_new_instblk_target_sys_ncoh_f()));
}

static int ga10b_pmu_ns_nvriscv_bootstrap(struct gk20a *g,  struct nvgpu_pmu *pmu,
		u32 args_offset)
{
	struct falcon_next_core_ucode_desc *desc;
	struct pmu_rtos_fw *rtos_fw = g->pmu->fw;
	u64 fmc_code_addr = 0;
	u64 fmc_data_addr = 0;
	u64 manifest_addr = 0;

	(void)args_offset;

	desc = (struct falcon_next_core_ucode_desc *)(void *)
			rtos_fw->fw_desc->data;

	fmc_code_addr = right_shift_8bits((nvgpu_mem_get_addr(g,
			&pmu->fw->ucode) + desc->monitor_code_offset));

	fmc_data_addr = right_shift_8bits((nvgpu_mem_get_addr(g,
			&pmu->fw->ucode) + desc->monitor_data_offset));

	manifest_addr = right_shift_8bits((nvgpu_mem_get_addr(g,
			&pmu->fw->ucode) + desc->manifest_offset));

	g->ops.falcon.brom_config(pmu->flcn, fmc_code_addr, fmc_data_addr,
			manifest_addr);

	g->ops.falcon.bootstrap(pmu->flcn, 0U);

	return 0;
}

int ga10b_pmu_ns_bootstrap(struct gk20a *g, struct nvgpu_pmu *pmu,
		u32 args_offset)
{
	int err = 0;

	if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
		err = ga10b_pmu_ns_nvriscv_bootstrap(g, pmu, args_offset);
	} else {
		err = ga10b_pmu_ns_falcon_bootstrap(g, pmu, args_offset);
	}

	return err;
}
#endif /* CONFIG_NVGPU_LS_PMU */

void ga10b_pmu_dump_elpg_stats(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = pmu->g;

	nvgpu_pmu_dbg(g, "pwr_pmu_idle_mask_supp_r(3): 0x%08x",
		nvgpu_readl(g, pwr_pmu_idle_mask_supp_r(3)));
	nvgpu_pmu_dbg(g, "pwr_pmu_idle_mask_1_supp_r(3): 0x%08x",
		nvgpu_readl(g, pwr_pmu_idle_mask_1_supp_r(3)));
	nvgpu_pmu_dbg(g, "pwr_pmu_idle_ctrl_supp_r(3): 0x%08x",
		nvgpu_readl(g, pwr_pmu_idle_ctrl_supp_r(3)));
	nvgpu_pmu_dbg(g, "pwr_pmu_pg_idle_cnt_r(0): 0x%08x",
		nvgpu_readl(g, pwr_pmu_pg_idle_cnt_r(0)));
	nvgpu_pmu_dbg(g, "pwr_pmu_pg_intren_r(0): 0x%08x",
		nvgpu_readl(g, pwr_pmu_pg_intren_r(0)));

	nvgpu_pmu_dbg(g, "pwr_pmu_idle_count_r(3): 0x%08x",
		nvgpu_readl(g, pwr_pmu_idle_count_r(3)));
	nvgpu_pmu_dbg(g, "pwr_pmu_idle_count_r(4): 0x%08x",
		nvgpu_readl(g, pwr_pmu_idle_count_r(4)));
	nvgpu_pmu_dbg(g, "pwr_pmu_idle_count_r(7): 0x%08x",
		nvgpu_readl(g, pwr_pmu_idle_count_r(7)));
}

void ga10b_pmu_init_perfmon_counter(struct gk20a *g)
{
	u32 data;

	/* use counter #3 for GR && CE2 busy cycles */
	nvgpu_writel(g, pwr_pmu_idle_mask_r(IDLE_COUNTER_3),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	/* disable idle filtering for counters 3 and 6 */
	data = nvgpu_readl(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_3));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
		pwr_pmu_idle_ctrl_filter_m(),
		pwr_pmu_idle_ctrl_value_busy_f() |
		pwr_pmu_idle_ctrl_filter_disabled_f());
	nvgpu_writel(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_3), data);

	/* use counter #6 for total cycles */
	data = nvgpu_readl(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_6));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
		pwr_pmu_idle_ctrl_filter_m(),
		pwr_pmu_idle_ctrl_value_always_f() |
		pwr_pmu_idle_ctrl_filter_disabled_f());
	nvgpu_writel(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_6), data);

	/*
	 * We don't want to disturb counters #3 and #6, which are used by
	 * perfmon, so we add wiring also to counters #1 and #2 for
	 * exposing raw counter readings.
	 */
	nvgpu_writel(g, pwr_pmu_idle_mask_r(IDLE_COUNTER_1),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	data = nvgpu_readl(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_1));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
		pwr_pmu_idle_ctrl_filter_m(),
		pwr_pmu_idle_ctrl_value_busy_f() |
		pwr_pmu_idle_ctrl_filter_disabled_f());
	nvgpu_writel(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_1), data);

	data = nvgpu_readl(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_2));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
		pwr_pmu_idle_ctrl_filter_m(),
		pwr_pmu_idle_ctrl_value_always_f() |
		pwr_pmu_idle_ctrl_filter_disabled_f());
	nvgpu_writel(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_2), data);

	/*
	* use counters 4 and 0 for perfmon to log busy cycles and total
	* cycles counter #0 overflow sets pmu idle intr status bit
	*/
	nvgpu_writel(g, pwr_pmu_idle_intr_r(),
		pwr_pmu_idle_intr_en_f(0));

	nvgpu_writel(g, pwr_pmu_idle_threshold_r(IDLE_COUNTER_0),
		pwr_pmu_idle_threshold_value_f(PMU_IDLE_THRESHOLD_V));

	data = nvgpu_readl(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_0));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
		pwr_pmu_idle_ctrl_filter_m(),
		pwr_pmu_idle_ctrl_value_always_f() |
		pwr_pmu_idle_ctrl_filter_disabled_f());
	nvgpu_writel(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_0), data);

	nvgpu_writel(g, pwr_pmu_idle_mask_r(IDLE_COUNTER_4),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	data = nvgpu_readl(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_4));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
		pwr_pmu_idle_ctrl_filter_m(),
		pwr_pmu_idle_ctrl_value_busy_f() |
		pwr_pmu_idle_ctrl_filter_disabled_f());
	nvgpu_writel(g, pwr_pmu_idle_ctrl_r(IDLE_COUNTER_4), data);

	nvgpu_writel(g, pwr_pmu_idle_count_r(IDLE_COUNTER_0),
		pwr_pmu_idle_count_reset_f(1));
	nvgpu_writel(g, pwr_pmu_idle_count_r(IDLE_COUNTER_4),
		pwr_pmu_idle_count_reset_f(1));
	nvgpu_writel(g, pwr_pmu_idle_intr_status_r(),
		pwr_pmu_idle_intr_status_intr_f(1));
}

u32 ga10b_pmu_read_idle_counter(struct gk20a *g, u32 counter_id)
{
	return pwr_pmu_idle_count_value_v(
		nvgpu_readl(g, pwr_pmu_idle_count_r(counter_id)));
}

void ga10b_pmu_reset_idle_counter(struct gk20a *g, u32 counter_id)
{
	nvgpu_writel(g, pwr_pmu_idle_count_r(counter_id),
		pwr_pmu_idle_count_reset_f(1));
}

bool ga10b_pmu_is_debug_mode_en(struct gk20a *g)
{
	u32 ctl_stat =  nvgpu_readl(g, pwr_falcon_hwcfg2_r());

	if (pwr_falcon_hwcfg2_dbgmode_v(ctl_stat) ==
		pwr_falcon_hwcfg2_dbgmode_enable_v()) {
		nvgpu_pmu_dbg(g, "DEBUG MODE");
		return true;
	} else {
		nvgpu_pmu_dbg(g, "PROD MODE");
		return false;
	}
}

void ga10b_pmu_handle_swgen1_irq(struct gk20a *g, u32 intr)
{
#ifdef CONFIG_NVGPU_FALCON_DEBUG
	struct nvgpu_pmu *pmu = g->pmu;
	int err = 0;

	if ((intr & pwr_falcon_irqstat_swgen1_true_f()) != 0U) {
		err = nvgpu_falcon_dbg_buf_display(pmu->flcn);
		if (err != 0) {
			nvgpu_err(g, "nvgpu_falcon_dbg_buf_display failed err=%d",
				err);
		}
	}
#endif
	(void)g;
	(void)intr;
}

/*
 * GA10B PMU IRQ registers are not accessible when NVRISCV PRIV
 * lockdown is engaged, so need to skip accessing IRQ registers.
 */
#ifdef CONFIG_NVGPU_LS_PMU
bool ga10b_pmu_is_interrupted(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = pmu->g;

	if (!g->ops.falcon.is_priv_lockdown(pmu->flcn)) {
		return gk20a_pmu_is_interrupted(pmu);
	}

	return false;
}
#endif

/*
 *
 * Interrupts required for LS-PMU are configured by LS-PMU ucode as part of
 * LS-PMU init code, so just enable/disable PMU interrupt from MC.
 *
 */
void ga10b_pmu_enable_irq(struct nvgpu_pmu *pmu, bool enable)
{
	struct gk20a *g = pmu->g;

	nvgpu_log_fn(g, " ");

	nvgpu_cic_mon_intr_stall_unit_config(g,
				NVGPU_CIC_INTR_UNIT_PMU,
				enable);
}

static int ga10b_pmu_handle_ecc(struct gk20a *g)
{
	int ret = 0;
	u32 ecc_status = 0;

	ecc_status = nvgpu_readl(g, pwr_pmu_falcon_ecc_status_r());

	if ((ecc_status &
		pwr_pmu_falcon_ecc_status_uncorrected_err_imem_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
					GPU_PMU_IMEM_ECC_UNCORRECTED);
		nvgpu_err(g, "imem ecc error uncorrected ");
		ret = -EFAULT;
	}

	if ((ecc_status &
		pwr_pmu_falcon_ecc_status_uncorrected_err_dmem_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
					GPU_PMU_DMEM_ECC_UNCORRECTED);
		nvgpu_err(g, "dmem ecc error uncorrected");
		ret = -EFAULT;
	}

	if ((ecc_status &
		pwr_pmu_falcon_ecc_status_uncorrected_err_dcls_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
					GPU_PMU_DCLS_UNCORRECTED);
		nvgpu_err(g, "dcls ecc error uncorrected");
		ret = -EFAULT;
	}

	if ((ecc_status &
		pwr_pmu_falcon_ecc_status_uncorrected_err_reg_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
					GPU_PMU_REG_ECC_UNCORRECTED);
		nvgpu_err(g, "reg ecc error uncorrected");
		ret = -EFAULT;
	}

	if ((ecc_status &
		pwr_pmu_falcon_ecc_status_uncorrected_err_mpu_ram_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
					GPU_PMU_MPU_ECC_UNCORRECTED);
		nvgpu_err(g, "mpu ecc error uncorrected");
		ret = -EFAULT;
	}

	if (ret != 0) {
		nvgpu_err(g, "ecc_addr(0x%x)",
			nvgpu_readl(g, pwr_pmu_falcon_ecc_address_r()));
	}

	return ret;
}

void ga10b_pmu_handle_ext_irq(struct gk20a *g, u32 intr0)
{
	/* handle the ECC interrupt */
	if ((intr0 & pwr_falcon_irqstat_ext_ecc_parity_true_f()) != 0U) {
		ga10b_pmu_handle_ecc(g);
	}

	/* handle the MEMERR interrupt */
	if ((intr0 & pwr_falcon_irqstat_memerr_true_f()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
				GPU_PMU_ACCESS_TIMEOUT_UNCORRECTED);
		nvgpu_err(g, "memerr/access timeout error uncorrected");
	}

	/* handle the IOPMP interrupt */
	if ((intr0 & pwr_falcon_irqstat_iopmp_true_f()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
				GPU_PMU_ILLEGAL_ACCESS_UNCORRECTED);
		nvgpu_err(g, "iopmp/illegal access error uncorrected");
	}

	/* handle the WDT interrupt */
	if ((intr0 & pwr_falcon_irqstat_wdt_true_f()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
				GPU_PMU_WDT_UNCORRECTED);
		nvgpu_err(g, "wdt error uncorrected");
	}
}
