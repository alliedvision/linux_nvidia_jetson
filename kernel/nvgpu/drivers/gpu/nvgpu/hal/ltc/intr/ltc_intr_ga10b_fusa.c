/*
 * GA10B LTC INTR
 *
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

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/utils.h>

#include "ltc_intr_ga10b.h"

#include <nvgpu/hw/ga10b/hw_ltc_ga10b.h>

static void ga10b_ltc_intr1_configure(struct gk20a *g)
{
	u32 reg;

	/* Enable ltc interrupts indicating illegal activity */
	reg = nvgpu_readl(g, ltc_ltcs_ltss_intr_r());

	/*
	 * IDLE_ERROR_CBC - flag if cbc gets a request while slcg clock is
	 * disabled
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr_en_idle_error_cbc_m(),
		ltc_ltcs_ltss_intr_en_idle_error_cbc_enabled_f());

	/*
	 * IDLE_ERROR_TSTG - flag if tstg gets a request while slcg clock is
	 * disabled
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr_en_idle_error_tstg_m(),
		ltc_ltcs_ltss_intr_en_idle_error_tstg_enabled_f());

	/*
	 * IDLE_ERROR_DSTG - flag if dstg gets a request while slcg clock is
	 * disabled
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr_en_idle_error_dstg_m(),
		ltc_ltcs_ltss_intr_en_idle_error_dstg_enabled_f());

	/*
	 * EVICTED_CB - indicates that a CB was demoted.
	 * Normally this should not happen because the CBs should be flushed
	 * during context switch and/or invalidated when no longer used.
	 *
	 * Note: this occurs more frequently than expected, so is being left
	 * disabled as on previous chips and consistent with HW POR value.
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr_en_evicted_cb_m(),
		ltc_ltcs_ltss_intr_en_evicted_cb_disabled_f());

	/*
	 * ILLEGAL_ATOMIC - unsupported atomic op and/or size received.
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr_en_illegal_atomic_m(),
		ltc_ltcs_ltss_intr_en_illegal_atomic_enabled_f());

	/*
	 * BLKACTIVITY_ERR - internal error in power sensing block activity
	 * monitor
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr_en_blkactivity_err_m(),
		ltc_ltcs_ltss_intr_en_blkactivity_err_enabled_f());

	/*
	 * ILLEGAL_COMPSTAT_ACCESS - indicates that some memory access
	 * read/wrote into the memory space reserved for the compression bit
	 * carveout (Bug 942161)
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr_en_illegal_compstat_access_m(),
		ltc_ltcs_ltss_intr_en_illegal_compstat_access_enabled_f());

	nvgpu_writel(g, ltc_ltcs_ltss_intr_r(), reg);

	/* Read back register for write synchronization */
	reg = nvgpu_readl(g, ltc_ltcs_ltss_intr_r());

#ifdef CONFIG_NVGPU_NON_FUSA
	/* illegal_compstat interrupts can be also controlled through
	 * debug_fs, so enable/disable based on g->ltc_intr_en_illegal_compstat
	 * settings
	 */
	if (g->ops.ltc.intr.en_illegal_compstat != NULL) {
		g->ops.ltc.intr.en_illegal_compstat(g,
					g->ltc_intr_en_illegal_compstat);
	}
#endif
}

/* LTC interrupts included in intr2 are not used for ga10b */
static void ga10b_ltc_intr2_configure(struct gk20a *g)
{
	u32 reg;

	reg = nvgpu_readl(g, ltc_ltcs_ltss_intr2_r());

	/*
	 * TRDONE_INVALID_TDTAG - The tdtag for a transdone does not match any
	 * valid L2 tag (subid/tdtag) for outstanding requests/transdones.
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_trdone_invalid_tdtag_m(),
		ltc_ltcs_ltss_intr2_en_trdone_invalid_tdtag_enabled_f());

	/*
	 * UNEXPECTED_TRDONE - Transdone is unexpected response type. The tdtag
	 * received matches a L2 tag (subid) of a non-cache coherent request
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_unexpected_trdone_m(),
		ltc_ltcs_ltss_intr2_en_unexpected_trdone_enabled_f());

	/*
	 * RWC_UPG_UNEXPECTED_TRDONE_DATA - Transdone.D is an unexpected
	 * response type for a read.rwc/upgrade. The tdtag received matches a
	 * tag (tdtag) of an oustanding transdone for a cache coherent
	 * read.rwc/upgrade
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr2_en_rwc_upg_unexpected_trdone_data_m(),
		ltc_ltcs_ltss_intr2_en_rwc_upg_unexpected_trdone_data_enabled_f());

	/*
	 * RWC_UPG_UNEXPECTED_TRDONE_CANCEL - Transdone.cancel is an unexpected
	 * response type for read.rwc/upgrade. The tdtag matches a tag (tdtag)
	 * of a cache coherent read.rwc/upgrade
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr2_en_rwc_upg_unexpected_trdone_cancel_m(),
		ltc_ltcs_ltss_intr2_en_rwc_upg_unexpected_trdone_cancel_enabled_f());

	/*
	 * PRBRS_INVALID_SUBID - The subid of a probe response does not match
	 * any tag (subid/tdtag) from L2 that has an outstanding requests'
	 * response
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_prbrs_invalid_subid_m(),
		ltc_ltcs_ltss_intr2_en_prbrs_invalid_subid_enabled_f());

	/*
	 * UNEXPECTED_PRBRS - Probe Reponse is an unexpected response type. The
	 * subid received matches a tag (subid/tdtag) of a non-probe
	 * request/transdone from L2
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_unexpected_prbrs_m(),
		ltc_ltcs_ltss_intr2_en_unexpected_prbrs_enabled_f());

	/*
	 * PRBIN_UNEXPECTED_PRBRS - PrbRsp.I.ND is expected given that Prb.I.ND
	 * is outstanding for a given address (128B granularity)
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_prbin_unexpected_prbrs_m(),
		ltc_ltcs_ltss_intr2_en_prbin_unexpected_prbrs_enabled_f());

	/*
	 * PRBIMO_UNEXPECTED_PRBRS - PrbRsp.I is expected given that there is an
	 * outstanding probe.I.MO for a given address
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_prbimo_unexpected_prbrs_m(),
		ltc_ltcs_ltss_intr2_en_prbimo_unexpected_prbrs_enabled_f());

	/*
	 * PRBX_MISSING_DATA - For PrbRsp.X should receive data beats (32B) for
	 * all the probed sectors to a given address
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_prbx_missing_data_m(),
		ltc_ltcs_ltss_intr2_en_prbx_missing_data_enabled_f());

	/*
	 * PRBX_UNEXPECTED_DATA - For PrbRsp.X should not receive data beats
	 * (32B) for non probed sectors to a given address
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_prbx_unexpected_data_m(),
		ltc_ltcs_ltss_intr2_en_prbx_unexpected_data_enabled_f());

	/*
	 * PRBRS_UNEXPECTED_PA7 - pa7 in probe response (based on mask) for a
	 * given subid should correspond to pa7 of the probe
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_prbrs_unexpected_pa7_m(),
		ltc_ltcs_ltss_intr2_en_prbrs_unexpected_pa7_enabled_f());

	/*
	 * TRDONE_UNEXPECTED_PA7 - pa7 in downgrade trasndone (based on mask)
	 * for a given subid should correspond to pa7 of the downgrade
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr2_en_trdone_unexpected_pa7_m(),
		ltc_ltcs_ltss_intr2_en_trdone_unexpected_pa7_enabled_f());

	/*
	 * SYSFILL_BYPASS_INVALID_SUBID - The subid for a sysmem fill response
	 * does not match any valid L2 tag (subid/tdtag) for outstanding
	 * requests/transdones.
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr2_en_sysfill_bypass_invalid_subid_m(),
		ltc_ltcs_ltss_intr2_en_sysfill_bypass_invalid_subid_enabled_f());

	/*
	 * UNEXPECTED_SYSFILL_BYPASS - Sysmem fill/data response is an
	 * unexpected response type. The subid matches an L2 tag (subid) of a
	 * non-read/non-atomic L2 reflected request
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr2_en_unexpected_sysfill_bypass_m(),
		ltc_ltcs_ltss_intr2_en_unexpected_sysfill_bypass_enabled_f());

	/*
	 * CHECKEDIN_UNEXPECTED_PRBRS - PrbRsp.I.D or PrbRsp.E or PrbRsp.M
	 * should not be received to a checked-in line (128B), i.e. after a
	 * dgd.transdone.I in a probe/downgrade race
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr2_en_checkedin_unexpected_prbrs_m(),
		ltc_ltcs_ltss_intr2_en_checkedin_unexpected_prbrs_enabled_f());

	/*
	 * CHECKEDIN_UNEXPECTED_TRDONE - Downgrade transdone (128B granularity)
	 * should be cancelled if the transdone is received when the line is
	 * checked-in, i.e. either a dgd to checked-in line or dgd losing in a
	 * probe/downgrade race
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr2_en_checkedin_unexpected_trdone_m(),
		ltc_ltcs_ltss_intr2_en_checkedin_unexpected_trdone_enabled_f());

	nvgpu_writel(g, ltc_ltcs_ltss_intr2_r(), reg);

	/* Read back register for write synchronization */
	reg = nvgpu_readl(g, ltc_ltcs_ltss_intr2_r());
}

void ga10b_ltc_intr3_configure_extra(struct gk20a *g, u32 *reg)
{
	(void)g;

	/*
	 * DTM_KIND_INVALID - If the kind of a comp stat req read or packed
	 * read is invalid or pitch, the inter3_dtm_kind_invalid interrupt will
	 * assert if enabled.
	 */
	*reg = set_field(*reg, ltc_ltcs_ltss_intr3_en_dtm_kind_invalid_m(),
		ltc_ltcs_ltss_intr3_en_dtm_kind_invalid_enabled_f());

	/*
	 * DTM_COMPTAG_INVALID - If the comptag_vld of a comp stat req read or
	 * packed read is invalid, the inter3_dtm_comptag_invalid interrupt will
	 * assert if enabled.
	 */
	*reg = set_field(*reg, ltc_ltcs_ltss_intr3_en_dtm_comptag_invalid_m(),
		ltc_ltcs_ltss_intr3_en_dtm_comptag_invalid_enabled_f());

	/*
	 * CDCMP_IP_ERROR is the OR of these outputs from the decomprIP:
	 * - err_2x_reduced_partial_fetch_mask  -- error condition in IP with
	 *   64B input interface where it receives a reduced 2x request with
	 *   partial fetch_mask
	 * - err_exp_eop_no_match_act_eop  -- error condition where actual EOP
	 *   doesnt match the expected EOP
	 * - err_illegal_catom_encoded_comp_info -- error indication that the
	 *   3bit encoded comp_info is illegal
	 * - err_sdc_has_more_sv_bits_set --  error indicating malformed SDC
	 *   packet
	 * - err_unsupported_comp_stat -- error condition indicating unsupported
	 *   comp_stat condition
	 * - err_vdc_packing -- error in vdc packing. In ga100 we merged this
	 *   with decompr_64B_fsm_state
	 */
	*reg = set_field(*reg, ltc_ltcs_ltss_intr3_en_cdcmp_ip_error_m(),
		ltc_ltcs_ltss_intr3_en_cdcmp_ip_error_enabled_f());
}

static void ga10b_ltc_intr3_configure(struct gk20a *g)
{
	u32 reg;

	reg = nvgpu_readl(g, ltc_ltcs_ltss_intr3_r());

	/*
	 * CHECKEDOUT_RWC_UPG_UNEXPECTED_NVPORT - RWC/Upgrade to the same 256B
	 * L2 slice address should have the same nvlink port#.
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr3_en_checkedout_rwc_upg_unexpected_nvport_m(),
		ltc_ltcs_ltss_intr3_en_checkedout_rwc_upg_unexpected_nvport_enabled_f());

	/*
	 * CHECKEDOUT_TRDONE_UNEXPECTED_NVPORT - TrDone to the same 256B L2
	 * slice address should have the same nvlink port#.
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr3_en_checkedout_trdone_unexpected_nvport_m(),
		ltc_ltcs_ltss_intr3_en_checkedout_trdone_unexpected_nvport_enabled_f());

	/*
	 * CHECKEDOUT_PRBRS_UNEXPECTED_NVPORT - PrbRs to the same 256B L2 slice
	 * address should have the same nvlink port#.
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr3_en_checkedout_prbrs_unexpected_nvport_m(),
		ltc_ltcs_ltss_intr3_en_checkedout_prbrs_unexpected_nvport_enabled_f());

	/*
	 * CHECKEDOUT_NINB_NCNP_REQ - NCNP request on NISO-NB received to a 128B
	 * line that is checked out
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr3_en_checkedout_ninb_ncnp_req_m(),
		ltc_ltcs_ltss_intr3_en_checkedout_ninb_ncnp_req_enabled_f());

	/*
	 * CHECKEDOUT_CREQ_NCNP_REQ - NCNP request on CREQ received to a 128B
	 * line that is checked out
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr3_en_checkedout_creq_ncnp_req_m(),
		ltc_ltcs_ltss_intr3_en_checkedout_creq_ncnp_req_enabled_f());

	/*
	 * RMWRS_INVALID_SUBID - The subid of a RMW response does not match any
	 * outstanding request's tag (subid) from L2.
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr3_en_rmwrs_invalid_subid_m(),
		ltc_ltcs_ltss_intr3_en_rmwrs_invalid_subid_enabled_f());

	/*
	 * UNEXPECTED_RMWRS - RMW Reponse is an unexpected response type. The
	 * subid received matches a tag (subid/tdtag) of a non-RMW request.
	 */
	reg = set_field(reg, ltc_ltcs_ltss_intr3_en_unexpected_rmwrs_m(),
		ltc_ltcs_ltss_intr3_en_unexpected_rmwrs_enabled_f());

	/*
	 * ILLEGAL_ACCESS_KIND_TYPE1 - A read request kind does not match CBC
	 * stored kind type1
	 * 1.  Access kind is not color and stored kind is color with
	 * compression status non-zero
	 * 2.  Access kind is color and stored kind is not color and the request
	 * is from CROP
	 * 3.  Access kind is z and stored kind is different z
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr3_en_illegal_access_kind_type1_m(),
		ltc_ltcs_ltss_intr3_en_illegal_access_kind_type1_enabled_f());

	/*
	 * ILLEGAL_ACCESS_KIND_TYPE2 - A read request kind does not match the
	 * CBC stored kind type2
	 * 1.  Access kind is not color and stored kind is color with
	 * compression status zero
	 * 2.  Access kind is color and stored kind is not color and the request
	 * is from XBAR
	 */
	reg = set_field(reg,
		ltc_ltcs_ltss_intr3_en_illegal_access_kind_type2_m(),
		ltc_ltcs_ltss_intr3_en_illegal_access_kind_type2_enabled_f());

	if (g->ops.ltc.intr.ltc_intr3_configure_extra != NULL) {
		g->ops.ltc.intr.ltc_intr3_configure_extra(g, &reg);
	}

	nvgpu_writel(g, ltc_ltcs_ltss_intr3_r(), reg);

	/* Read back register for write synchronization */
	reg = nvgpu_readl(g, ltc_ltcs_ltss_intr3_r());
}

void ga10b_ltc_intr_configure(struct gk20a *g)
{
	ga10b_ltc_intr1_configure(g);
	ga10b_ltc_intr2_configure(g);
	ga10b_ltc_intr3_configure(g);
}

static void ga10b_ltc_intr_handle_rstg_ecc_interrupts(struct gk20a *g,
			u32 ltc, u32 slice, u32 ecc_status, u32 ecc_addr,
			u32 uncorrected_delta)
{
	bool is_rstg_ecc_addr = (ltc_ltc0_lts0_l2_cache_ecc_address_subunit_v(ecc_addr) ==
				 ltc_ltc0_lts0_l2_cache_ecc_address_subunit_rstg_v());

	if ((ecc_status & ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_rstg_m()) != 0U) {
		nvgpu_err(g, "rstg ecc error uncorrected");

		if (!is_rstg_ecc_addr) {
			nvgpu_log(g, gpu_dbg_intr, "ECC address doesn't belong to RSTG");
			return;
		}

		g->ecc.ltc.rstg_ecc_parity_count[ltc][slice].counter =
				nvgpu_wrapping_add_u32(
				g->ecc.ltc.rstg_ecc_parity_count[ltc][slice].counter,
					uncorrected_delta);
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_LTC,
				GPU_LTC_CACHE_RSTG_CBC_ECC_UNCORRECTED);
	}

	if ((ecc_status & ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_rstg_m()) != 0U) {
		nvgpu_err(g, "rstg ecc error corrected");
		/* This error is not expected to occur in ga10x and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
}

static void ga10b_ltc_intr_handle_tstg_ecc_interrupts(struct gk20a *g,
			u32 ltc, u32 slice, u32 ecc_status, u32 ecc_addr,
			u32 uncorrected_delta)
{
	bool is_tstg_ecc_addr = (ltc_ltc0_lts0_l2_cache_ecc_address_subunit_v(ecc_addr) ==
				 ltc_ltc0_lts0_l2_cache_ecc_address_subunit_tstg_v());

	if ((ecc_status & ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_tstg_m()) != 0U) {
		nvgpu_err(g, "tstg ecc error uncorrected");

		if (!is_tstg_ecc_addr) {
			nvgpu_log(g, gpu_dbg_intr, "ECC address doesn't belong to TSTG");
			return;
		}

		g->ecc.ltc.tstg_ecc_parity_count[ltc][slice].counter =
				nvgpu_wrapping_add_u32(
				g->ecc.ltc.tstg_ecc_parity_count[ltc][slice].counter,
					uncorrected_delta);
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_LTC,
				GPU_LTC_CACHE_TSTG_ECC_UNCORRECTED);
	}

	if ((ecc_status & ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_tstg_m()) != 0U) {
		nvgpu_err(g, "tstg ecc error corrected");
		/* This error is not expected to occur in ga10b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
}

static bool ga10b_ltc_intr_is_dstg_data_bank(u32 ecc_addr)
{
	u32 ecc_ram = ltc_ltc0_lts0_l2_cache_ecc_address_ram_v(ecc_addr);
	bool is_dstg_data_bank = false;

	if ((ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_bank0_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_bank1_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_bank2_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_bank3_v())) {
		is_dstg_data_bank = true;
	}

	return is_dstg_data_bank;
}

static bool ga10b_ltc_intr_is_dstg_be_ram(u32 ecc_addr)
{
	u32 ecc_ram = ltc_ltc0_lts0_l2_cache_ecc_address_ram_v(ecc_addr);
	bool is_dstg_be_ram = false;

	if ((ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram0_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram1_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram2_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram3_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram4_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram5_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram6_v()) ||
	    (ecc_ram == ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram7_v())) {
		is_dstg_be_ram = true;
	}

	return is_dstg_be_ram;
}

static void ga10b_ltc_intr_handle_dstg_ecc_interrupts(struct gk20a *g,
			u32 ltc, u32 slice, u32 ecc_status, u32 ecc_addr,
			u32 corrected_delta, u32 uncorrected_delta)
{
	bool is_dstg_ecc_addr = (ltc_ltc0_lts0_l2_cache_ecc_address_subunit_v(ecc_addr) ==
				 ltc_ltc0_lts0_l2_cache_ecc_address_subunit_dstg_v());

	if ((ecc_status & ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_dstg_m()) != 0U) {
		nvgpu_err(g, "dstg ecc error (SEC) corrected");

		if (!is_dstg_ecc_addr) {
			nvgpu_log(g, gpu_dbg_intr, "ECC address doesn't belong to DSTG");
			return;
		}

		g->ecc.ltc.ecc_sec_count[ltc][slice].counter =
			nvgpu_wrapping_add_u32(
				g->ecc.ltc.ecc_sec_count[ltc][slice].counter,
				corrected_delta);

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_LTC,
				GPU_LTC_CACHE_DSTG_ECC_CORRECTED);

		/*
		 * Using a SEC code will allow correction of an SBE (Single Bit
		 * Error). But the current HW doesn't have the ability to clear
		 * out the SBE from the RAMs for a read access. So before the
		 * SBE turns into a DBE (Double Bit Error), a SW flush is
		 * preferred.
		 */
		if (g->ops.mm.cache.l2_flush(g, true) != 0) {
			nvgpu_err(g, "l2_flush failed");
			BUG();
		}
	}

	if ((ecc_status & ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_dstg_m()) != 0U) {
		nvgpu_err(g, "dstg ecc error uncorrected");

		if (!is_dstg_ecc_addr) {
			nvgpu_log(g, gpu_dbg_intr, "ECC address doesn't belong to DSTG");
			return;
		}

		if (ga10b_ltc_intr_is_dstg_data_bank(ecc_addr)) {
			nvgpu_err(g, "Double bit error detected in GPU L2!");

			g->ecc.ltc.ecc_ded_count[ltc][slice].counter =
				nvgpu_wrapping_add_u32(
					g->ecc.ltc.ecc_ded_count[ltc][slice].counter,
					uncorrected_delta);

			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_LTC,
					GPU_LTC_CACHE_DSTG_ECC_UNCORRECTED);
		} else if (ga10b_ltc_intr_is_dstg_be_ram(ecc_addr)) {
			nvgpu_log(g, gpu_dbg_intr, "dstg be ecc error uncorrected");

			g->ecc.ltc.dstg_be_ecc_parity_count[ltc][slice].counter =
				nvgpu_wrapping_add_u32(
					g->ecc.ltc.dstg_be_ecc_parity_count[ltc][slice].counter,
					uncorrected_delta);

		} else {
			nvgpu_err(g, "unsupported uncorrected dstg ecc error");
			BUG();
		}
	}
}

static void ga10b_ltc_intr_init_counters(struct gk20a *g,
			u32 uncorrected_delta, u32 uncorrected_overflow,
			u32 corrected_delta, u32 corrected_overflow,
			u32 offset)
{
	if ((uncorrected_delta > 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_writel(g,
			nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_r(),
			offset), 0);
	}

	if ((corrected_delta > 0U) || (corrected_overflow != 0U)) {
		nvgpu_writel(g,
			nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_r(),
			offset), 0);
	}
}

static void ga10b_ltc_intr3_ecc_interrupts(struct gk20a *g, u32 ltc, u32 slice,
				u32 offset, u32 ltc_intr3)
{
	u32 ecc_status, ecc_addr, corrected_cnt, uncorrected_cnt;
	u32 corrected_delta, uncorrected_delta;
	u32 corrected_overflow, uncorrected_overflow;

	/*
	 * Detect and handle ECC PARITY errors and SEC-DED errors.
	 * SEC errors are reported as DSTG corrected errors and
	 * DED errors are reported as DSTG uncorrected errors.
	 * Below are the supported errors:
	 *
	 *   1. UNCORRECTED_ERR_RSTG - signals a parity error in RSTG RAMS, for now only CBC RAMS
	 *   2. UNCORRECTED_ERR_TSTG - signals a parity error in TSTG RAMS
	 *   3. UNCORRECTED_ERR_DSTG - signals a parity error in DSTG RAMS, non-data RAMS
	 *                             and DED in data RAMS.
	 *   4. CORRECTED_ERR_DSTG - signals an ecc corrected error in DSTG data RAMS (SEC)
	 */
	if ((ltc_intr3 &
		(ltc_ltcs_ltss_intr3_ecc_uncorrected_m() |
		 ltc_ltcs_ltss_intr3_ecc_corrected_m())) != 0U) {

		ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
				ltc_ltc0_lts0_l2_cache_ecc_status_r(), offset));
		ecc_addr = nvgpu_readl(g, nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_address_r(), offset));

		uncorrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_r(),
			offset));

		uncorrected_delta =
			ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_total_v(uncorrected_cnt);

		uncorrected_overflow = ecc_status &
			ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_total_counter_overflow_m();

		corrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_r(),
			offset));

		corrected_delta =
			ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_total_v(corrected_cnt);

		corrected_overflow = ecc_status &
			ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_total_counter_overflow_m();

		ga10b_ltc_intr_init_counters(g,
			uncorrected_delta, uncorrected_overflow,
			corrected_delta, corrected_overflow, offset);

		nvgpu_writel(g, nvgpu_safe_add_u32(
				ltc_ltc0_lts0_l2_cache_ecc_status_r(), offset),
			ltc_ltc0_lts0_l2_cache_ecc_status_reset_task_f());

		/* update counters per slice */
		if (uncorrected_overflow != 0U) {
			nvgpu_info(g, "uncorrected ecc counter overflow!");
			uncorrected_delta += BIT32(
				ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_total_s());
		}

		if (corrected_overflow != 0U) {
			nvgpu_info(g, "corrected ecc counter overflow!");
			corrected_delta += BIT32(
				ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_total_s());
		}

		nvgpu_log(g, gpu_dbg_intr,
			  "ecc status 0x%08x error address: 0x%08x subunit: %u corrected_delta: 0x%08x uncorrected_delta: 0x%08x",
			  ecc_status, ecc_addr,
			  ltc_ltc0_lts0_l2_cache_ecc_address_subunit_v(ecc_addr),
			  corrected_delta, uncorrected_delta);

		ga10b_ltc_intr_handle_rstg_ecc_interrupts(g, ltc, slice,
						ecc_status, ecc_addr,
						uncorrected_delta);

		ga10b_ltc_intr_handle_tstg_ecc_interrupts(g, ltc, slice,
						ecc_status, ecc_addr,
						uncorrected_delta);

		ga10b_ltc_intr_handle_dstg_ecc_interrupts(g, ltc, slice,
						ecc_status, ecc_addr,
						corrected_delta, uncorrected_delta);
	}
}

void ga10b_ltc_intr_handle_lts_intr3_extra(struct gk20a *g, u32 ltc, u32 slice, u32 *reg_value)
{
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);

	u32 offset = nvgpu_safe_add_u32(nvgpu_safe_mult_u32(ltc_stride, ltc),
					nvgpu_safe_mult_u32(lts_stride, slice));
	u32 ltc_intr3 = nvgpu_readl(g, nvgpu_safe_add_u32(
					ltc_ltc0_lts0_intr3_r(), offset));

	if (ltc_intr3 == 0U) {
		return;
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_dtm_kind_invalid_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d dtm_kind_invalid interrupt",
			ltc, slice);
		*reg_value = set_field(*reg_value,
			ltc_ltcs_ltss_intr3_dtm_kind_invalid_m(),
			ltc_ltcs_ltss_intr3_dtm_kind_invalid_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_dtm_comptag_invalid_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d dtm_comptag_invalid interrupt",
			ltc, slice);
		*reg_value = set_field(*reg_value,
			ltc_ltcs_ltss_intr3_dtm_comptag_invalid_m(),
			ltc_ltcs_ltss_intr3_dtm_comptag_invalid_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_cdcmp_ip_error_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d cdcmp_ip_error interrupt",
			ltc, slice);
		*reg_value = set_field(*reg_value,
			ltc_ltcs_ltss_intr3_cdcmp_ip_error_m(),
			ltc_ltcs_ltss_intr3_cdcmp_ip_error_reset_f());
	}
}

void ga10b_ltc_intr3_interrupts(struct gk20a *g, u32 ltc, u32 slice,
				u32 ltc_intr3)
{
	u32 reg_value = ltc_intr3;

	if (ltc_intr3 &
		ltc_ltcs_ltss_intr3_checkedout_rwc_upg_unexpected_nvport_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d checkedout_rwc_upg_unexpected_nvport interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr3_checkedout_rwc_upg_unexpected_nvport_m(),
			ltc_ltcs_ltss_intr3_checkedout_rwc_upg_unexpected_nvport_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_checkedout_trdone_unexpected_nvport_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d checkedout_trdone_unexpected_nvport interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr3_checkedout_trdone_unexpected_nvport_m(),
			ltc_ltcs_ltss_intr3_checkedout_trdone_unexpected_nvport_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_checkedout_prbrs_unexpected_nvport_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d checkedout_prbrs_unexpected_nvport interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr3_checkedout_prbrs_unexpected_nvport_m(),
			ltc_ltcs_ltss_intr3_checkedout_prbrs_unexpected_nvport_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_checkedout_ninb_ncnp_req_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d checkedout_ninb_ncnp_req interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr3_checkedout_ninb_ncnp_req_m(),
			ltc_ltcs_ltss_intr3_checkedout_ninb_ncnp_req_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_checkedout_creq_ncnp_req_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d checkedout_creq_ncnp_req interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr3_checkedout_creq_ncnp_req_m(),
			ltc_ltcs_ltss_intr3_checkedout_creq_ncnp_req_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_rmwrs_invalid_subid_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d rmwrs_invalid_subid interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr3_rmwrs_invalid_subid_m(),
			ltc_ltcs_ltss_intr3_rmwrs_invalid_subid_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_unexpected_rmwrs_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d unexpected_rmwrs interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr3_unexpected_rmwrs_m(),
			ltc_ltcs_ltss_intr3_unexpected_rmwrs_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_illegal_access_kind_type1_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d illegal_access_kind_type1 interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr3_illegal_access_kind_type1_m(),
			ltc_ltcs_ltss_intr3_illegal_access_kind_type1_reset_f());
	}

	if (ltc_intr3 & ltc_ltcs_ltss_intr3_illegal_access_kind_type2_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d illegal_access_kind_type2 interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr3_illegal_access_kind_type2_m(),
			ltc_ltcs_ltss_intr3_illegal_access_kind_type2_reset_f());
	}

	if (g->ops.ltc.intr.isr_extra != NULL) {
		g->ops.ltc.intr.isr_extra(g, ltc, slice, &reg_value);
	}
}

void ga10b_ltc_intr_handle_lts_intr3(struct gk20a *g, u32 ltc, u32 slice)
{
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);

	u32 offset = nvgpu_safe_add_u32(nvgpu_safe_mult_u32(ltc_stride, ltc),
					nvgpu_safe_mult_u32(lts_stride, slice));
	u32 ltc_intr3 = nvgpu_readl(g, nvgpu_safe_add_u32(
					ltc_ltc0_lts0_intr3_r(), offset));
	u32 reg_value = ltc_intr3;

	if (ltc_intr3 == 0U) {
		return;
	}

	ga10b_ltc_intr3_ecc_interrupts(g, ltc, slice, offset, ltc_intr3);
	ga10b_ltc_intr3_interrupts(g, ltc, slice, ltc_intr3);

	/* Reset interrupts */
	nvgpu_writel(g, nvgpu_safe_add_u32( ltc_ltc0_lts0_intr3_r(), offset),
			reg_value);
}

void ga10b_ltc_intr_handle_lts_intr2(struct gk20a *g, u32 ltc, u32 slice)
{
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);

	u32 offset = nvgpu_safe_add_u32(nvgpu_safe_mult_u32(ltc_stride, ltc),
					nvgpu_safe_mult_u32(lts_stride, slice));
	u32 ltc_intr2 = nvgpu_readl(g, nvgpu_safe_add_u32(
					ltc_ltc0_lts0_intr2_r(), offset));
	u32 reg_value = ltc_intr2;

	if (ltc_intr2 == 0U) {
		return;
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_trdone_invalid_tdtag_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d trdone_invalid_tdtag interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_trdone_invalid_tdtag_m(),
			ltc_ltcs_ltss_intr2_trdone_invalid_tdtag_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_unexpected_trdone_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d unexpected_trdone interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_unexpected_trdone_m(),
			ltc_ltcs_ltss_intr2_unexpected_trdone_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_data_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d rwc_upg_unexpected_trdone_data interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_data_m(),
			ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_data_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_cancel_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d rwc_upg_unexpected_trdone_cancel interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_cancel_m(),
			ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_cancel_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_prbrs_invalid_subid_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d prbrs_invalid_subid interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_prbrs_invalid_subid_m(),
			ltc_ltcs_ltss_intr2_prbrs_invalid_subid_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_unexpected_prbrs_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d unexpected_prbrs interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_unexpected_prbrs_m(),
			ltc_ltcs_ltss_intr2_unexpected_prbrs_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_prbin_unexpected_prbrs_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d prbin_unexpected_prbrs interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_prbin_unexpected_prbrs_m(),
			ltc_ltcs_ltss_intr2_prbin_unexpected_prbrs_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_prbimo_unexpected_prbrs_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d prbimo_unexpected_prbrs interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_prbimo_unexpected_prbrs_m(),
			ltc_ltcs_ltss_intr2_prbimo_unexpected_prbrs_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_prbx_missing_data_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d prbx_missing_data interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_prbx_missing_data_m(),
			ltc_ltcs_ltss_intr2_prbx_missing_data_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_prbx_unexpected_data_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d prbx_unexpected_data interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_prbx_unexpected_data_m(),
			ltc_ltcs_ltss_intr2_prbx_unexpected_data_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_prbrs_unexpected_pa7_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d prbrs_unexpected_pa7 interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_prbrs_unexpected_pa7_m(),
			ltc_ltcs_ltss_intr2_prbrs_unexpected_pa7_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_trdone_unexpected_pa7_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d trdone_unexpected_pa7 interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_trdone_unexpected_pa7_m(),
			ltc_ltcs_ltss_intr2_trdone_unexpected_pa7_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_sysfill_bypass_invalid_subid_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d sysfill_bypass_invalid_subid interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_sysfill_bypass_invalid_subid_m(),
			ltc_ltcs_ltss_intr2_sysfill_bypass_invalid_subid_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_unexpected_sysfill_bypass_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d unexpected_sysfill_bypass interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_unexpected_sysfill_bypass_m(),
			ltc_ltcs_ltss_intr2_unexpected_sysfill_bypass_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_checkedin_unexpected_prbrs_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d checkedin_unexpected_prbrs interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_checkedin_unexpected_prbrs_m(),
			ltc_ltcs_ltss_intr2_checkedin_unexpected_prbrs_reset_f());
	}

	if (ltc_intr2 & ltc_ltcs_ltss_intr2_checkedin_unexpected_trdone_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d checkedin_unexpected_trdone interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr2_checkedin_unexpected_trdone_m(),
			ltc_ltcs_ltss_intr2_checkedin_unexpected_trdone_reset_f());
	}

	/* Reset interrupts */
	nvgpu_writel(g, nvgpu_safe_add_u32( ltc_ltc0_lts0_intr2_r(), offset),
			reg_value);
}

void ga10b_ltc_intr_handle_lts_intr(struct gk20a *g, u32 ltc, u32 slice)
{
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);

	u32 offset = nvgpu_safe_add_u32(nvgpu_safe_mult_u32(ltc_stride, ltc),
					nvgpu_safe_mult_u32(lts_stride, slice));
	u32 ltc_intr = nvgpu_readl(g, nvgpu_safe_add_u32(
					ltc_ltc0_lts0_intr_r(), offset));
	u32 reg_value = ltc_intr;

	if (ltc_intr == 0U) {
		return;
	}

	if (ltc_intr & ltc_ltcs_ltss_intr_idle_error_cbc_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d idle_error_cbc interrupt", ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr_idle_error_cbc_m(),
			ltc_ltcs_ltss_intr_idle_error_cbc_reset_f());
	}

	if (ltc_intr & ltc_ltcs_ltss_intr_idle_error_tstg_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d idle_error_tstg interrupt", ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr_idle_error_tstg_m(),
			ltc_ltcs_ltss_intr_idle_error_tstg_reset_f());
	}

	if (ltc_intr & ltc_ltcs_ltss_intr_idle_error_dstg_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d idle_error_dstg interrupt", ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr_idle_error_dstg_m(),
			ltc_ltcs_ltss_intr_idle_error_dstg_reset_f());
	}

	if (ltc_intr & ltc_ltcs_ltss_intr_evicted_cb_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d evicted_cb interrupt", ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr_evicted_cb_m(),
			ltc_ltcs_ltss_intr_evicted_cb_reset_f());
	}

	if ((ltc_intr & ltc_ltcs_ltss_intr_en_illegal_compstat_enabled_f()) &&
		(ltc_intr & ltc_ltcs_ltss_intr_illegal_compstat_m())) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d illegal_compstat interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr_illegal_compstat_m(),
			ltc_ltcs_ltss_intr_illegal_compstat_reset_f());
	}

	if (ltc_intr & ltc_ltcs_ltss_intr_illegal_atomic_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d illegal_atomic interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr_illegal_atomic_m(),
			ltc_ltcs_ltss_intr_illegal_atomic_reset_f());
	}

	if (ltc_intr & ltc_ltcs_ltss_intr_blkactivity_err_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d blkactivity_err interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr_blkactivity_err_m(),
			ltc_ltcs_ltss_intr_blkactivity_err_reset_f());
	}

	if (ltc_intr & ltc_ltcs_ltss_intr_illegal_compstat_access_m()) {
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d illegal_compstat_access interrupt",
			ltc, slice);
		reg_value = set_field(reg_value,
			ltc_ltcs_ltss_intr_illegal_compstat_access_m(),
			ltc_ltcs_ltss_intr_illegal_compstat_access_reset_f());
	}

	/* Reset interrupts */
	nvgpu_writel(g, nvgpu_safe_add_u32( ltc_ltc0_lts0_intr_r(), offset),
			reg_value);
}

void ga10b_ltc_intr_isr(struct gk20a *g, u32 ltc)
{
	u32 slice;

	for (slice = 0U; slice < g->ltc->slices_per_ltc; slice++) {
		ga10b_ltc_intr_handle_lts_intr(g, ltc, slice);
		ga10b_ltc_intr_handle_lts_intr2(g, ltc, slice);
		ga10b_ltc_intr_handle_lts_intr3(g, ltc, slice);
	}
}
