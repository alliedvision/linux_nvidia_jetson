/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/mm.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/firmware.h>
#include <nvgpu/bug.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/cmd.h>
#endif

#include <nvgpu/hw/gv11b/hw_pwr_gv11b.h>

#include "pmu_gv11b.h"

#define PWR_FALCON_MAILBOX1_DATA_INIT	(0U)
#define PMU_BAR0_HOST_READ_ERROR	(0U)
#define ALIGN_4KB     12

/* error handler */
void gv11b_clear_pmu_bar0_host_err_status(struct gk20a *g)
{
	u32 status;

	status = nvgpu_readl(g, pwr_pmu_bar0_host_error_r());
	nvgpu_writel(g, pwr_pmu_bar0_host_error_r(), status);
}

static u32 pmu_bar0_host_tout_etype(u32 val)
{
	return (val != PMU_BAR0_HOST_READ_ERROR) ?
			PMU_BAR0_HOST_WRITE_TOUT : PMU_BAR0_HOST_READ_TOUT;
}

static u32 pmu_bar0_fecs_tout_etype(u32 val)
{
	return (val != PMU_BAR0_HOST_READ_ERROR) ?
			PMU_BAR0_FECS_WRITE_TOUT : PMU_BAR0_FECS_READ_TOUT;
}

static u32 pmu_bar0_cmd_hwerr_etype(u32 val)
{
	return (val != PMU_BAR0_HOST_READ_ERROR) ?
			PMU_BAR0_CMD_WRITE_HWERR : PMU_BAR0_CMD_READ_HWERR;
}

static u32 pmu_bar0_fecserr_etype(u32 val)
{
	return (val != PMU_BAR0_HOST_READ_ERROR) ?
			PMU_BAR0_WRITE_FECSERR : PMU_BAR0_READ_FECSERR;
}

static u32 pmu_bar0_hosterr_etype(u32 val)
{
	return (val != PMU_BAR0_HOST_READ_ERROR) ?
			PMU_BAR0_WRITE_HOSTERR : PMU_BAR0_READ_HOSTERR;
}

int gv11b_pmu_bar0_error_status(struct gk20a *g, u32 *bar0_status,
	u32 *etype)
{
	u32 val = 0;
	u32 err_status = 0;
	u32 err_cmd = 0;

	val = nvgpu_readl(g, pwr_pmu_bar0_error_status_r());
	*bar0_status = val;
	if (val == 0U) {
		return 0;
	}

	err_cmd = val & pwr_pmu_bar0_error_status_err_cmd_m();

	if ((val & pwr_pmu_bar0_error_status_timeout_host_m()) != 0U) {
		*etype = pmu_bar0_host_tout_etype(err_cmd);
	} else if ((val & pwr_pmu_bar0_error_status_timeout_fecs_m()) != 0U) {
		*etype = pmu_bar0_fecs_tout_etype(err_cmd);
	} else if ((val & pwr_pmu_bar0_error_status_cmd_hwerr_m()) != 0U) {
		*etype = pmu_bar0_cmd_hwerr_etype(err_cmd);
	} else if ((val & pwr_pmu_bar0_error_status_fecserr_m()) != 0U) {
		*etype = pmu_bar0_fecserr_etype(err_cmd);
		err_status = nvgpu_readl(g, pwr_pmu_bar0_fecs_error_r());
		/*
		 * BAR0_FECS_ERROR would only record the first error code if
		 * multiple FECS error happen. Once BAR0_FECS_ERROR is cleared,
		 * BAR0_FECS_ERROR can record the error code from FECS again.
		 * Writing status regiter to clear the FECS Hardware state.
		 */
		nvgpu_writel(g, pwr_pmu_bar0_fecs_error_r(), err_status);
	} else if ((val & pwr_pmu_bar0_error_status_hosterr_m()) != 0U) {
		*etype = pmu_bar0_hosterr_etype(err_cmd);
		/*
		 * BAR0_HOST_ERROR would only record the first error code if
		 * multiple HOST error happen. Once BAR0_HOST_ERROR is cleared,
		 * BAR0_HOST_ERROR can record the error code from HOST again.
		 * Writing status regiter to clear the FECS Hardware state.
		 *
		 * Defining clear ops for host err as gk20a does not have
		 * status register for this.
		 */
		if (g->ops.pmu.pmu_clear_bar0_host_err_status != NULL) {
			g->ops.pmu.pmu_clear_bar0_host_err_status(g);
		}
	} else {
		nvgpu_err(g, "PMU bar0 status type is not found");
	}

	/* Writing Bar0 status regiter to clear the Hardware state */
	nvgpu_writel(g, pwr_pmu_bar0_error_status_r(), val);
	return (-EIO);
}

static int gv11b_pmu_correct_ecc(struct gk20a *g, u32 ecc_status, u32 ecc_addr)
{
	int ret = 0;

	if ((ecc_status &
		pwr_pmu_falcon_ecc_status_corrected_err_imem_m()) != 0U) {
		nvgpu_err(g, "falcon imem ecc error corrected. "
				"ecc_addr(0x%x)", ecc_addr);
	}
	if ((ecc_status &
		pwr_pmu_falcon_ecc_status_uncorrected_err_imem_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
				GPU_PMU_IMEM_ECC_UNCORRECTED);
		nvgpu_err(g, "falcon imem ecc error uncorrected. "
				"ecc_addr(0x%x)", ecc_addr);
		ret = -EFAULT;
	}
	if ((ecc_status &
		pwr_pmu_falcon_ecc_status_uncorrected_err_dmem_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
				GPU_PMU_DMEM_ECC_UNCORRECTED);
		nvgpu_err(g, "falcon dmem ecc error uncorrected. "
				"ecc_addr(0x%x)", ecc_addr);
		ret = -EFAULT;
	}

	return ret;
}

bool gv11b_pmu_validate_mem_integrity(struct gk20a *g)
{
	u32 ecc_status, ecc_addr;

	ecc_status = nvgpu_readl(g, pwr_pmu_falcon_ecc_status_r());
	ecc_addr = nvgpu_readl(g, pwr_pmu_falcon_ecc_address_r());

	return ((gv11b_pmu_correct_ecc(g, ecc_status, ecc_addr) == 0) ? true :
			false);
}

bool gv11b_pmu_is_debug_mode_en(struct gk20a *g)
{
	u32 ctl_stat =  nvgpu_readl(g, pwr_pmu_scpctl_stat_r());

	return pwr_pmu_scpctl_stat_debug_mode_v(ctl_stat) != 0U;
}

void gv11b_pmu_flcn_setup_boot_config(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	u32 inst_block_ptr;

	nvgpu_log_fn(g, " ");

	/* setup apertures */
	if (g->ops.pmu.setup_apertures != NULL) {
		g->ops.pmu.setup_apertures(g);
	}

	/* Clearing mailbox register used to reflect capabilities */
	nvgpu_writel(g, pwr_falcon_mailbox1_r(), PWR_FALCON_MAILBOX1_DATA_INIT);

	/* enable the context interface */
	nvgpu_writel(g, pwr_falcon_itfen_r(),
		nvgpu_readl(g, pwr_falcon_itfen_r()) |
		pwr_falcon_itfen_ctxen_enable_f());

	/*
	 * The instance block address to write is the lower 32-bits of the 4K-
	 * aligned physical instance block address.
	 */
	inst_block_ptr = nvgpu_inst_block_ptr(g, &mm->pmu.inst_block);

	nvgpu_writel(g, pwr_pmu_new_instblk_r(),
		pwr_pmu_new_instblk_ptr_f(inst_block_ptr) |
		pwr_pmu_new_instblk_valid_f(1U) |
		(nvgpu_is_enabled(g, NVGPU_USE_COHERENT_SYSMEM) ?
		pwr_pmu_new_instblk_target_sys_coh_f() :
		pwr_pmu_new_instblk_target_sys_ncoh_f()));
}

void gv11b_setup_apertures(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	struct nvgpu_mem *inst_block = &mm->pmu.inst_block;

	nvgpu_log_fn(g, " ");

	/* setup apertures - virtual */
	nvgpu_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_UCODE),
		pwr_fbif_transcfg_mem_type_physical_f() |
		nvgpu_aperture_mask(g, inst_block,
		pwr_fbif_transcfg_target_noncoherent_sysmem_f(),
		pwr_fbif_transcfg_target_coherent_sysmem_f(),
		pwr_fbif_transcfg_target_local_fb_f()));
	nvgpu_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_VIRT),
		pwr_fbif_transcfg_mem_type_virtual_f());
	/* setup apertures - physical */
	nvgpu_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_VID),
		pwr_fbif_transcfg_mem_type_physical_f() |
		nvgpu_aperture_mask(g, inst_block,
		pwr_fbif_transcfg_target_noncoherent_sysmem_f(),
		pwr_fbif_transcfg_target_coherent_sysmem_f(),
		pwr_fbif_transcfg_target_local_fb_f()));
	nvgpu_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_COH),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_coherent_sysmem_f());
	nvgpu_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_NCOH),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_noncoherent_sysmem_f());
}

bool gv11b_pmu_is_engine_in_reset(struct gk20a *g)
{
	u32 reg_reset;
	bool status = false;

	reg_reset = gk20a_readl(g, pwr_falcon_engine_r());
	if (reg_reset == pwr_falcon_engine_reset_true_f()) {
		status = true;
	}

	return status;
}

void gv11b_pmu_engine_reset(struct gk20a *g, bool do_reset)
{
	if (g->is_fusa_sku) {
		return;
	}

	/*
	* From GP10X onwards, we are using PPWR_FALCON_ENGINE for reset. And as
	* it may come into same behavior, reading NV_PPWR_FALCON_ENGINE again
	* after Reset.
	*/
	if (do_reset) {
		gk20a_writel(g, pwr_falcon_engine_r(),
			pwr_falcon_engine_reset_false_f());
		(void) gk20a_readl(g, pwr_falcon_engine_r());
	} else {
		gk20a_writel(g, pwr_falcon_engine_r(),
			pwr_falcon_engine_reset_true_f());
		(void) gk20a_readl(g, pwr_falcon_engine_r());
	}
}

u32 gv11b_pmu_falcon_base_addr(void)
{
	return pwr_falcon_irqsset_r();
}

bool gv11b_is_pmu_supported(struct gk20a *g)
{
	(void)g;

#ifdef CONFIG_NVGPU_LS_PMU
	return true;
#else
	/* set to false to disable LS PMU ucode support */
	return false;
#endif
}

int gv11b_pmu_ecc_init(struct gk20a *g)
{
	int err = 0;

	err = NVGPU_ECC_COUNTER_INIT_PMU(pmu_ecc_uncorrected_err_count);
	if (err != 0) {
		goto done;
	}

	err = NVGPU_ECC_COUNTER_INIT_PMU(pmu_ecc_corrected_err_count);
	if (err != 0) {
		goto done;
	}

done:
	if (err != 0) {
		nvgpu_err(g, "ecc counter allocate failed, err=%d", err);
		gv11b_pmu_ecc_free(g);
	}

	return err;
}

void gv11b_pmu_ecc_free(struct gk20a *g)
{
	NVGPU_ECC_COUNTER_FREE_PMU(pmu_ecc_corrected_err_count);
	NVGPU_ECC_COUNTER_FREE_PMU(pmu_ecc_uncorrected_err_count);
}

static void gv11b_pmu_handle_ecc_irq(struct gk20a *g)
{
	u32 intr1;
	u32 ecc_status, ecc_addr, corrected_cnt, uncorrected_cnt;
	u32 corrected_delta, uncorrected_delta;
	u32 corrected_overflow, uncorrected_overflow;

	intr1 = nvgpu_readl(g, pwr_pmu_ecc_intr_status_r());
	if ((intr1 &
		(pwr_pmu_ecc_intr_status_corrected_m() |
		 pwr_pmu_ecc_intr_status_uncorrected_m())) == 0U) {
		return;
	}

	ecc_status = nvgpu_readl(g,
		pwr_pmu_falcon_ecc_status_r());
	ecc_addr = nvgpu_readl(g,
		pwr_pmu_falcon_ecc_address_r());
	corrected_cnt = nvgpu_readl(g,
		pwr_pmu_falcon_ecc_corrected_err_count_r());
	uncorrected_cnt = nvgpu_readl(g,
		pwr_pmu_falcon_ecc_uncorrected_err_count_r());

	corrected_delta =
	  pwr_pmu_falcon_ecc_corrected_err_count_total_v(corrected_cnt);
	uncorrected_delta =
	  pwr_pmu_falcon_ecc_uncorrected_err_count_total_v(uncorrected_cnt);
	corrected_overflow = ecc_status &
	  pwr_pmu_falcon_ecc_status_corrected_err_total_counter_overflow_m();

	uncorrected_overflow = ecc_status &
	  pwr_pmu_falcon_ecc_status_uncorrected_err_total_counter_overflow_m();
	corrected_overflow = ecc_status &
	  pwr_pmu_falcon_ecc_status_corrected_err_total_counter_overflow_m();

	/* clear the interrupt */
	if (((intr1 & pwr_pmu_ecc_intr_status_corrected_m()) != 0U) ||
					(corrected_overflow != 0U)) {
		nvgpu_writel(g, pwr_pmu_falcon_ecc_corrected_err_count_r(), 0);
	}
	if (((intr1 & pwr_pmu_ecc_intr_status_uncorrected_m()) != 0U) ||
					(uncorrected_overflow != 0U)) {
		nvgpu_writel(g,
			pwr_pmu_falcon_ecc_uncorrected_err_count_r(), 0);
	}

	nvgpu_writel(g, pwr_pmu_falcon_ecc_status_r(),
		pwr_pmu_falcon_ecc_status_reset_task_f());

	/* update counters per slice */
	if (corrected_overflow != 0U) {
		corrected_delta +=
			BIT32(pwr_pmu_falcon_ecc_corrected_err_count_total_s());
	}
	if (uncorrected_overflow != 0U) {
		uncorrected_delta +=
		  BIT32(pwr_pmu_falcon_ecc_uncorrected_err_count_total_s());
	}

	g->ecc.pmu.pmu_ecc_corrected_err_count[0].counter =
		nvgpu_safe_add_u32(
			g->ecc.pmu.pmu_ecc_corrected_err_count[0].counter,
			corrected_delta);
	g->ecc.pmu.pmu_ecc_uncorrected_err_count[0].counter =
		nvgpu_safe_add_u32(
			g->ecc.pmu.pmu_ecc_uncorrected_err_count[0].counter,
			uncorrected_delta);

	nvgpu_log(g, gpu_dbg_intr,
		"pmu ecc interrupt intr1: 0x%x", intr1);

	(void)gv11b_pmu_correct_ecc(g, ecc_status, ecc_addr);

	if ((corrected_overflow != 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_info(g, "ecc counter overflow!");
	}

	nvgpu_log(g, gpu_dbg_intr,
		"ecc error row address: 0x%x",
		pwr_pmu_falcon_ecc_address_row_address_v(ecc_addr));

	nvgpu_log(g, gpu_dbg_intr,
		"ecc error count corrected: %d, uncorrected %d",
		g->ecc.pmu.pmu_ecc_corrected_err_count[0].counter,
		g->ecc.pmu.pmu_ecc_uncorrected_err_count[0].counter);
}

void gv11b_pmu_handle_ext_irq(struct gk20a *g, u32 intr0)
{
	/*
	 * handle the ECC interrupt
	 */
	if ((intr0 & pwr_falcon_irqstat_ext_ecc_parity_true_f()) != 0U) {
		gv11b_pmu_handle_ecc_irq(g);
	}
}

void gv11b_pmu_enable_irq(struct nvgpu_pmu *pmu, bool enable)
{
	struct gk20a *g = pmu->g;
	u32 intr_mask = 0x0;
	u32 intr_dest = 0x0;

	nvgpu_log_fn(g, " ");

	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_PMU, NVGPU_CIC_INTR_DISABLE);

	nvgpu_falcon_set_irq(pmu->flcn, false, intr_mask, intr_dest);

	if (enable) {
		intr_dest = g->ops.pmu.get_irqdest(g);
#ifdef CONFIG_NVGPU_LS_PMU
		/* 0=disable, 1=enable */
		intr_mask = pwr_falcon_irqmset_gptmr_f(1)  |
			pwr_falcon_irqmset_wdtmr_f(1)  |
			pwr_falcon_irqmset_mthd_f(0)   |
			pwr_falcon_irqmset_ctxsw_f(0)  |
			pwr_falcon_irqmset_halt_f(1)   |
			pwr_falcon_irqmset_exterr_f(1) |
			pwr_falcon_irqmset_swgen0_f(1) |
			pwr_falcon_irqmset_swgen1_f(1) |
			pwr_falcon_irqmset_ext_ecc_parity_f(1);
#else
		intr_mask = pwr_falcon_irqmset_ext_ecc_parity_f(1);
#endif
		nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_PMU,
						 NVGPU_CIC_INTR_ENABLE);

		nvgpu_falcon_set_irq(pmu->flcn, true, intr_mask, intr_dest);
	}

	nvgpu_log_fn(g, "done");
}

u32 gv11b_pmu_get_irqdest(struct gk20a *g)
{
	u32 intr_dest;

	(void)g;

#ifdef CONFIG_NVGPU_LS_PMU
	/* dest 0=falcon, 1=host; level 0=irq0, 1=irq1 */
	intr_dest = pwr_falcon_irqdest_host_gptmr_f(0)      |
		pwr_falcon_irqdest_host_wdtmr_f(1)          |
		pwr_falcon_irqdest_host_mthd_f(0)           |
		pwr_falcon_irqdest_host_ctxsw_f(0)          |
		pwr_falcon_irqdest_host_halt_f(1)           |
		pwr_falcon_irqdest_host_exterr_f(0)         |
		pwr_falcon_irqdest_host_swgen0_f(1)         |
		pwr_falcon_irqdest_host_swgen1_f(0)         |
		pwr_falcon_irqdest_host_ext_ecc_parity_f(1) |
		pwr_falcon_irqdest_target_gptmr_f(1)        |
		pwr_falcon_irqdest_target_wdtmr_f(0)        |
		pwr_falcon_irqdest_target_mthd_f(0)         |
		pwr_falcon_irqdest_target_ctxsw_f(0)        |
		pwr_falcon_irqdest_target_halt_f(0)         |
		pwr_falcon_irqdest_target_exterr_f(0)       |
		pwr_falcon_irqdest_target_swgen0_f(0)       |
		pwr_falcon_irqdest_target_swgen1_f(0)       |
		pwr_falcon_irqdest_target_ext_ecc_parity_f(0);
#else
	intr_dest = pwr_falcon_irqdest_host_ext_ecc_parity_f(1) |
		pwr_falcon_irqdest_target_ext_ecc_parity_f(0);
#endif

	return intr_dest;
}
