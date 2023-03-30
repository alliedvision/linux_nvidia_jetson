/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifdef CONFIG_NVGPU_NVLINK

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include "intr_and_err_handling_tu104.h"

#include <nvgpu/hw/tu104/hw_nvlipt_tu104.h>
#include <nvgpu/hw/tu104/hw_ioctrl_tu104.h>
#include <nvgpu/hw/tu104/hw_nvl_tu104.h>
#include <nvgpu/hw/tu104/hw_ioctrlmif_tu104.h>
#include <nvgpu/hw/tu104/hw_nvtlc_tu104.h>

#define IPT_ERR_UC_ACTIVE_BITS	(nvlipt_err_uc_status_link0_dlprotocol_f(1) | \
				 nvlipt_err_uc_status_link0_datapoisoned_f(1) | \
				 nvlipt_err_uc_status_link0_flowcontrol_f(1) | \
				 nvlipt_err_uc_status_link0_responsetimeout_f(1) | \
				 nvlipt_err_uc_status_link0_targeterror_f(1) | \
				 nvlipt_err_uc_status_link0_unexpectedresponse_f(1) | \
				 nvlipt_err_uc_status_link0_receiveroverflow_f(1) | \
				 nvlipt_err_uc_status_link0_malformedpacket_f(1) | \
				 nvlipt_err_uc_status_link0_stompedpacketreceived_f(1) | \
				 nvlipt_err_uc_status_link0_unsupportedrequest_f(1) | \
				 nvlipt_err_uc_status_link0_ucinternal_f(1))

/*
 * Initialize logging and containment policy for TLC Parity errors
 */
static void tu104_nvlink_init_tlc_link_err(struct gk20a *g, u32 link_id)
{
	u32 reg;

	/*TX error */

	/* Containment (Do not enable for TX Data RAM parity errors).
	 * That bit should be left 0, so that the error can be signaled
	 * to the far device by poisoning. As long as containment is
	 * turned off, the poison enable is set by default.
	 */
	reg = (nvtlc_tx_err_contain_en_0_txhdrcreditovferr__prod_f() | \
		nvtlc_tx_err_contain_en_0_txdatacreditovferr__prod_f() | \
		nvtlc_tx_err_contain_en_0_txdlcreditovferr__prod_f() | \
		nvtlc_tx_err_contain_en_0_txdlcreditparityerr__prod_f() | \
		nvtlc_tx_err_contain_en_0_txramhdrparityerr__prod_f() | \
		nvtlc_tx_err_contain_en_0_txunsupvcovferr__prod_f() | \
		nvtlc_tx_err_contain_en_0_txstompdet__prod_f() | \
		nvtlc_tx_err_contain_en_0_txpoisondet_f(1U) | \
		nvtlc_tx_err_contain_en_0_targeterr_f(1U) | \
		nvtlc_tx_err_contain_en_0_unsupportedrequesterr_f(1U));
	TLC_REG_WR32(g, link_id, nvtlc_tx_err_contain_en_0_r(), reg);

	/* Logging */
	reg = (nvtlc_tx_err_log_en_0_txhdrcreditovferr__prod_f() | \
		nvtlc_tx_err_log_en_0_txdatacreditovferr__prod_f() | \
		nvtlc_tx_err_log_en_0_txdlcreditovferr__prod_f() | \
		nvtlc_tx_err_log_en_0_txdlcreditparityerr__prod_f() | \
		nvtlc_tx_err_log_en_0_txramhdrparityerr__prod_f() | \
		nvtlc_tx_err_log_en_0_txramdataparityerr__prod_f() | \
		nvtlc_tx_err_log_en_0_txunsupvcovferr__prod_f() | \
		nvtlc_tx_err_log_en_0_txstompdet__prod_f() | \
		nvtlc_tx_err_log_en_0_txpoisondet__prod_f() | \
		nvtlc_tx_err_log_en_0_targeterr__prod_f() | \
		nvtlc_tx_err_log_en_0_unsupportedrequesterr__prod_f());
	TLC_REG_WR32(g, link_id, nvtlc_tx_err_log_en_0_r(), reg);

	/* RX Error */
	/* Containment */
	reg = (nvtlc_rx_err_contain_en_0_rxdlhdrparityerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_rxdldataparityerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_rxdlctrlparityerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_rxramdataparityerr_f(1U) | \
		nvtlc_rx_err_contain_en_0_rxramhdrparityerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_rxinvalidaeerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_rxinvalidbeerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_rxinvalidaddralignerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_rxpktlenerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_datlengtatomicreqmaxerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_datlengtrmwreqmaxerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_datlenltatrrspminerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_invalidcacheattrpoerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_invalidcrerr__prod_f() | \
		nvtlc_rx_err_contain_en_0_rxrespstatustargeterr__prod_f() | \
		nvtlc_rx_err_contain_en_0_rxrespstatusunsupportedrequesterr__prod_f());
	TLC_REG_WR32(g, link_id, nvtlc_rx_err_contain_en_0_r(), reg);

	reg = (nvtlc_rx_err_contain_en_1_rxhdrovferr__prod_f() | \
		nvtlc_rx_err_contain_en_1_rxdataovferr__prod_f() | \
		nvtlc_rx_err_contain_en_1_stompdeterr__prod_f() | \
		nvtlc_rx_err_contain_en_1_rxpoisonerr__prod_f() | \
		nvtlc_rx_err_contain_en_1_rxunsupvcovferr__prod_f() | \
		nvtlc_rx_err_contain_en_1_rxunsupnvlinkcreditrelerr__prod_f() | \
		nvtlc_rx_err_contain_en_1_rxunsupncisoccreditrelerr__prod_f());
	TLC_REG_WR32(g, link_id, nvtlc_rx_err_contain_en_1_r(), reg);

	/* Logging */
	reg = (nvtlc_rx_err_log_en_0_rxdlhdrparityerr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxdldataparityerr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxdlctrlparityerr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxramdataparityerr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxramhdrparityerr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxinvalidaeerr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxinvalidbeerr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxinvalidaddralignerr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxpktlenerr__prod_f() | \
		nvtlc_rx_err_log_en_0_datlengtatomicreqmaxerr__prod_f() | \
		nvtlc_rx_err_log_en_0_datlengtrmwreqmaxerr__prod_f() | \
		nvtlc_rx_err_log_en_0_datlenltatrrspminerr__prod_f() | \
		nvtlc_rx_err_log_en_0_invalidcacheattrpoerr__prod_f() | \
		nvtlc_rx_err_log_en_0_invalidcrerr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxrespstatustargeterr__prod_f() | \
		nvtlc_rx_err_log_en_0_rxrespstatusunsupportedrequesterr__prod_f());
	TLC_REG_WR32(g, link_id, nvtlc_rx_err_log_en_0_r(), reg);

	reg = (nvtlc_rx_err_log_en_1_rxhdrovferr__prod_f() | \
		nvtlc_rx_err_log_en_1_rxdataovferr__prod_f() | \
		nvtlc_rx_err_log_en_1_stompdeterr__prod_f() | \
		nvtlc_rx_err_log_en_1_rxpoisonerr__prod_f() | \
		nvtlc_rx_err_log_en_1_rxunsupvcovferr__prod_f() | \
		nvtlc_rx_err_log_en_1_rxunsupnvlinkcreditrelerr__prod_f() | \
		nvtlc_rx_err_log_en_1_rxunsupncisoccreditrelerr__prod_f());
	TLC_REG_WR32(g, link_id, nvtlc_rx_err_log_en_1_r(), reg);
}

/*
 * Enable TLC per link interrupts
 */
static void tu104_nvlink_enable_tlc_link_err(struct gk20a *g, u32 link_id,
								bool enable)
{
	u32 reg_rx0 = 0, reg_rx1 = 0, reg_tx = 0;

	if (enable) {
		reg_tx = (nvtlc_tx_err_report_en_0_txhdrcreditovferr__prod_f() | \
			nvtlc_tx_err_report_en_0_txdatacreditovferr__prod_f() | \
			nvtlc_tx_err_report_en_0_txdlcreditovferr__prod_f() | \
			nvtlc_tx_err_report_en_0_txdlcreditparityerr__prod_f() | \
			nvtlc_tx_err_report_en_0_txramhdrparityerr__prod_f() | \
			nvtlc_tx_err_report_en_0_txramdataparityerr__prod_f() | \
			nvtlc_tx_err_report_en_0_txunsupvcovferr__prod_f() | \
			nvtlc_tx_err_report_en_0_txstompdet__prod_f() | \
			nvtlc_tx_err_report_en_0_txpoisondet__prod_f() | \
			nvtlc_tx_err_report_en_0_targeterr__prod_f() | \
			nvtlc_tx_err_report_en_0_unsupportedrequesterr__prod_f());

		reg_rx0 = (nvtlc_rx_err_report_en_0_rxdlhdrparityerr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxdldataparityerr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxdlctrlparityerr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxramdataparityerr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxramhdrparityerr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxinvalidaeerr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxinvalidbeerr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxinvalidaddralignerr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxpktlenerr__prod_f() | \
			nvtlc_rx_err_report_en_0_datlengtatomicreqmaxerr__prod_f() | \
			nvtlc_rx_err_report_en_0_datlengtrmwreqmaxerr__prod_f() | \
			nvtlc_rx_err_report_en_0_datlenltatrrspminerr__prod_f() | \
			nvtlc_rx_err_report_en_0_invalidcacheattrpoerr__prod_f() | \
			nvtlc_rx_err_report_en_0_invalidcrerr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxrespstatustargeterr__prod_f() | \
			nvtlc_rx_err_report_en_0_rxrespstatusunsupportedrequesterr__prod_f());

		reg_rx1 = (nvtlc_rx_err_report_en_1_rxhdrovferr__prod_f() | \
			nvtlc_rx_err_report_en_1_rxdataovferr__prod_f() | \
			nvtlc_rx_err_report_en_1_stompdeterr__prod_f() | \
			nvtlc_rx_err_report_en_1_rxpoisonerr__prod_f() | \
			nvtlc_rx_err_report_en_1_rxunsupvcovferr__prod_f() | \
			nvtlc_rx_err_report_en_1_rxunsupnvlinkcreditrelerr__prod_f() | \
			nvtlc_rx_err_report_en_1_rxunsupncisoccreditrelerr__prod_f());

	}

	TLC_REG_WR32(g, link_id, nvtlc_rx_err_report_en_0_r(), reg_rx0);
	TLC_REG_WR32(g, link_id, nvtlc_rx_err_report_en_1_r(), reg_rx1);
	TLC_REG_WR32(g, link_id, nvtlc_tx_err_report_en_0_r(), reg_tx);
}

/*
 * Interrupt routine handler for TLC
 */
static void tu104_nvlink_tlc_isr(struct gk20a *g, u32 link_id)
{
	u32 rx_status_0;
	u32 rx_status_1;
	u32 tx_status_0;

	rx_status_0 = TLC_REG_RD32(g, link_id, nvtlc_rx_err_status_0_r());
	rx_status_1 = TLC_REG_RD32(g, link_id, nvtlc_rx_err_status_1_r());
	tx_status_0 = TLC_REG_RD32(g, link_id, nvtlc_tx_err_status_0_r());

	nvgpu_log(g, gpu_dbg_nvlink, "Nvlink TLC ISR: RX0=0x%x, RX1=0x%x, TX0=0x%x",
					rx_status_0, rx_status_1, tx_status_0);

	if (rx_status_0 != 0U) {
		/* All TLC RX 0 errors are fatal. Notify and disable */
		nvgpu_err(g, "Fatal TLC RX 0 interrupt on link %d mask: %x",
			link_id, rx_status_0);
		TLC_REG_WR32(g, link_id, nvtlc_rx_err_first_0_r(),
				rx_status_0);
		TLC_REG_WR32(g, link_id, nvtlc_rx_err_status_0_r(),
				rx_status_0);
	}
	if (rx_status_1 != 0U) {
		/* All TLC RX 1 errors are fatal. Notify and disable */
		nvgpu_err(g, "Fatal TLC RX 1 interrupt on link %d mask: %x",
			link_id, rx_status_1);
		TLC_REG_WR32(g, link_id, nvtlc_rx_err_first_1_r(),
				rx_status_1);
		TLC_REG_WR32(g, link_id, nvtlc_rx_err_status_1_r(),
				rx_status_1);
	}
	if (tx_status_0 != 0U) {
		/* All TLC TX 0 errors are fatal. Notify and disable */
		nvgpu_err(g, "Fatal TLC TX 0 interrupt on link %d mask: %x",
			link_id, tx_status_0);
		TLC_REG_WR32(g, link_id, nvtlc_tx_err_first_0_r(),
				tx_status_0);
		TLC_REG_WR32(g, link_id, nvtlc_tx_err_status_0_r(),
				tx_status_0);
	}
}

/*
 * Enable link specific DLPL interrupts
 */
static void tu104_nvlink_enable_dlpl_link_intr(struct gk20a *g, u32 link_id, bool enable)
{
	u32 reg = 0U;

	/* Always disable nonstall tree */
	DLPL_REG_WR32(g, link_id, nvl_intr_nonstall_en_r(), 0U);

	if (!enable) {
		DLPL_REG_WR32(g, link_id, nvl_intr_stall_en_r(), 0U);
		return;
	}

	/* Clear interrupt register to get rid of stale state (W1C) */
	DLPL_REG_WR32(g, link_id, nvl_intr_r(), 0xffffffffU);
	DLPL_REG_WR32(g, link_id, nvl_intr_sw2_r(), 0xffffffffU);

	reg = nvl_intr_stall_en_ltssm_protocol_enable_f()            |
		nvl_intr_stall_en_ltssm_fault_enable_f()             |
		nvl_intr_stall_en_tx_recovery_long_enable_f()        |
		nvl_intr_stall_en_tx_fault_ram_enable_f()            |
		nvl_intr_stall_en_tx_fault_interface_enable_f()      |
		nvl_intr_stall_en_rx_fault_sublink_change_enable_f() |
		nvl_intr_stall_en_rx_fault_dl_protocol_enable_f()    |
		nvl_intr_stall_en_rx_short_error_rate_enable_f();

	DLPL_REG_WR32(g, link_id, nvl_intr_stall_en_r(), reg);

	/* Configure error threshold */
	reg = DLPL_REG_RD32(g, link_id, nvl_sl1_error_count_ctrl_r());
	reg = set_field(reg, nvl_sl1_error_count_ctrl_short_rate_m(),
			nvl_sl1_error_count_ctrl_short_rate_enable_f());
	reg = set_field(reg, nvl_sl1_error_count_ctrl_rate_count_mode_m(),
			nvl_sl1_error_count_ctrl_rate_count_mode_flit_f());
	DLPL_REG_WR32(g, link_id, nvl_sl1_error_count_ctrl_r(), reg);

	reg = DLPL_REG_RD32(g, link_id, nvl_sl1_error_rate_ctrl_r());
	reg = set_field(reg, nvl_sl1_error_rate_ctrl_short_threshold_man_m(),
			nvl_sl1_error_rate_ctrl_short_threshold_man_f(12U));
	reg = set_field(reg, nvl_sl1_error_rate_ctrl_short_threshold_exp_m(),
			nvl_sl1_error_rate_ctrl_short_threshold_exp_f(1U));
	reg = set_field(reg, nvl_sl1_error_rate_ctrl_short_timescale_man_m(),
			nvl_sl1_error_rate_ctrl_short_timescale_man_f(5U));
	reg = set_field(reg, nvl_sl1_error_rate_ctrl_short_timescale_exp_m(),
			nvl_sl1_error_rate_ctrl_short_timescale_exp_f(2U));
	DLPL_REG_WR32(g, link_id, nvl_sl1_error_rate_ctrl_r(), reg);
}

/*
 * DLPL per-link isr
 */
static void tu104_nvlink_dlpl_isr(struct gk20a *g, u32 link_id)
{
	u32 intr = 0;

	intr = DLPL_REG_RD32(g, link_id, nvl_intr_r()) &
		DLPL_REG_RD32(g, link_id, nvl_intr_stall_en_r());

	nvgpu_log(g, gpu_dbg_nvlink, "Nvlink DLPL ISR triggered with intr: 0x%x", intr);

	if (intr == 0U) {
		return;
	}

	/* Clear interrupts */
	DLPL_REG_WR32(g, link_id, nvl_intr_r(), intr);
	DLPL_REG_WR32(g, link_id, nvl_intr_sw2_r(), intr);
}

/*
 * Initialize logging and containment policy for MIF Parity errors
 */
static void tu104_nvlink_init_mif_link_err(struct gk20a *g, u32 link_id)
{
	u32 reg;

	/*RX error */

	/* Containment (Enabled only for Header errors)
	 * In the Rx direction, the HSHUB does not handle either poison or
	 * containing (stomping) in mid packet (see bug 1939387),
	 * so there is no containment applied.
	 */
	reg = 0U;
	reg = set_field(reg,
		ioctrlmif_rx_err_contain_en_0_rxramhdrparityerr_m(),
		ioctrlmif_rx_err_contain_en_0_rxramhdrparityerr__prod_f());
	MIF_REG_WR32(g, link_id, ioctrlmif_rx_err_contain_en_0_r(), reg);

	/* Logging (do not ignore) */
	reg = 0U;
	reg = set_field(reg,
		ioctrlmif_rx_err_log_en_0_rxramdataparityerr_m(),
		ioctrlmif_rx_err_log_en_0_rxramdataparityerr_f(1U));
	reg = set_field(reg,
		ioctrlmif_rx_err_log_en_0_rxramhdrparityerr_m(),
		ioctrlmif_rx_err_log_en_0_rxramhdrparityerr_f(1U));
	MIF_REG_WR32(g, link_id, ioctrlmif_rx_err_log_en_0_r(), reg);

	/* Tx Error */

	/* Containment (Enabled only for Header errors)
	 * In the Tx direction, data parity errors will be poisoned,
	 * making it the far receiver’s responsibility to handle containment,
	 * and removing the requirement to contain at the transmitter.
	 */
	reg = 0U;
	reg = set_field(reg,
		ioctrlmif_tx_err_contain_en_0_txramhdrparityerr_m(),
		ioctrlmif_tx_err_contain_en_0_txramhdrparityerr__prod_f());
	MIF_REG_WR32(g, link_id, ioctrlmif_tx_err_contain_en_0_r(), reg);

	reg = 0U;
	reg = set_field(reg,
		ioctrlmif_tx_err_misc_0_txramdataparitypois_m(),
		ioctrlmif_tx_err_misc_0_txramdataparitypois_f(1U));
	MIF_REG_WR32(g, link_id, ioctrlmif_tx_err_misc_0_r(), reg);

	/* Logging (do not ignore) */
	reg = 0U;
	reg = set_field(reg, ioctrlmif_tx_err_log_en_0_txramdataparityerr_m(),
		ioctrlmif_tx_err_log_en_0_txramdataparityerr_f(1U));
	reg = set_field(reg, ioctrlmif_tx_err_log_en_0_txramhdrparityerr_m(),
		ioctrlmif_tx_err_log_en_0_txramhdrparityerr_f(1U));
	MIF_REG_WR32(g, link_id, ioctrlmif_tx_err_log_en_0_r(), reg);

	/* Credit release */
	MIF_REG_WR32(g, link_id, ioctrlmif_rx_ctrl_buffer_ready_r(), 0x1U);
	MIF_REG_WR32(g, link_id, ioctrlmif_tx_ctrl_buffer_ready_r(), 0x1U);
}

/*
 * Enable reporting(interrupt generation) per-link MIF interrupts
 */
static void tu104_nvlink_enable_mif_link_err(struct gk20a *g, u32 link_id,
								bool enable)
{
	u32 reg0 = 0U;
	u32 reg1 = 0U;

	if (enable) {
		reg0 = set_field(reg0,
			ioctrlmif_rx_err_report_en_0_rxramdataparityerr_m(),
			ioctrlmif_rx_err_report_en_0_rxramdataparityerr_f(1U));
		reg0 = set_field(reg0,
			ioctrlmif_rx_err_report_en_0_rxramhdrparityerr_m(),
			ioctrlmif_rx_err_report_en_0_rxramhdrparityerr_f(1U));
		reg1 = set_field(reg1,
			ioctrlmif_tx_err_report_en_0_txramdataparityerr_m(),
			ioctrlmif_tx_err_report_en_0_txramdataparityerr_f(1U));
		reg1 = set_field(reg1,
			ioctrlmif_tx_err_report_en_0_txramhdrparityerr_m(),
			ioctrlmif_tx_err_report_en_0_txramhdrparityerr_f(1U));
	}

	MIF_REG_WR32(g, link_id, ioctrlmif_rx_err_report_en_0_r(), reg0);
	MIF_REG_WR32(g, link_id, ioctrlmif_tx_err_report_en_0_r(), reg1);
}

/*
 * Handle per-link MIF interrupts
 */
static void tu104_nvlink_mif_isr(struct gk20a *g, u32 link_id)
{
	u32 intr, fatal_mask = 0;

	/* RX Errors */
	intr = MIF_REG_RD32(g, link_id, ioctrlmif_rx_err_status_0_r());
	nvgpu_log(g, gpu_dbg_nvlink, "Nvlink MIF RX ISR triggered with intr: 0x%x", intr);

	if (intr != 0U) {
		if ((intr & ioctrlmif_rx_err_status_0_rxramdataparityerr_m()) !=
									0U) {
			nvgpu_err(g, "Fatal MIF RX interrupt hit on link %d: RAM_DATA_PARITY",
				link_id);
			fatal_mask |= ioctrlmif_rx_err_status_0_rxramdataparityerr_f(1);
		}
		if ((intr & ioctrlmif_rx_err_status_0_rxramhdrparityerr_m()) !=
									0U) {
			nvgpu_err(g, "Fatal MIF RX interrupt hit on link %d: RAM_HDR_PARITY",
				link_id);
			fatal_mask |= ioctrlmif_rx_err_status_0_rxramhdrparityerr_f(1);
		}

		if (fatal_mask != 0U) {
			MIF_REG_WR32(g, link_id, ioctrlmif_rx_err_first_0_r(),
				fatal_mask);
			MIF_REG_WR32(g, link_id, ioctrlmif_rx_err_status_0_r(),
				fatal_mask);
		}
	}

	/* TX Errors */
	fatal_mask = 0;
	intr = MIF_REG_RD32(g, link_id, ioctrlmif_tx_err_status_0_r());
	nvgpu_log(g, gpu_dbg_nvlink, "Nvlink MIF TX ISR triggered with intr: 0x%x", intr);
	if (intr != 0U) {
		if ((intr & ioctrlmif_tx_err_status_0_txramdataparityerr_m()) !=
									0U) {
			nvgpu_err(g, "Fatal MIF TX interrupt hit on link %d: RAM_DATA_PARITY",
				link_id);
			fatal_mask |= ioctrlmif_tx_err_status_0_txramdataparityerr_f(1);
		}
		if ((intr & ioctrlmif_tx_err_status_0_txramhdrparityerr_m()) !=
									0U) {
			nvgpu_err(g, "Fatal MIF TX interrupt hit on link %d: RAM_HDR_PARITY",
				link_id);
			fatal_mask |= ioctrlmif_tx_err_status_0_txramhdrparityerr_f(1);
		}

		if (fatal_mask != 0U) {
			MIF_REG_WR32(g, link_id, ioctrlmif_tx_err_first_0_r(),
				fatal_mask);
			MIF_REG_WR32(g, link_id, ioctrlmif_tx_err_status_0_r(),
				fatal_mask);
		}
	}
}

/*
 * Initialize NVLIPT level link Aerr settings
 */
static void tu104_nvlink_init_nvlipt_link_err(struct gk20a *g, u32 link_id)
{
	/*
	 * AErr settings (nvlipt level)
	 */

	/* UC first and status reg (W1C) need to be cleared */
	IPT_REG_WR32(g, nvlipt_err_uc_first_link0_r(), IPT_ERR_UC_ACTIVE_BITS);
	IPT_REG_WR32(g, nvlipt_err_uc_status_link0_r(), IPT_ERR_UC_ACTIVE_BITS);

	/* AErr Severity */
	IPT_REG_WR32(g, nvlipt_err_uc_severity_link0_r(),
							IPT_ERR_UC_ACTIVE_BITS);
}

/*
 * Enable NVLIPT link errors and interrupts
 */
static void tu104_nvlink_enable_nvlipt_link_err_intr(struct gk20a *g,
							u32 link_id,
							bool enable)
{
	u32 val = 0U;

	if (enable) {
		val = 1U;
	}

	/* Enable fatal link errors. There are no non-fatal or correctable
	 * link errors. All errors are marked fatal.
	 */
	IPT_REG_WR32(g, nvlipt_err_control_link0_r(),
				nvlipt_err_control_link0_fatalenable_f(val));

	/* Enable stalling link interrupts. No non-stalling interrupts as per
	 * HSI.
	 */
	IPT_REG_WR32(g, nvlipt_intr_control_link0_r(),
				nvlipt_intr_control_link0_stallenable_f(val));
}

/*
 * Per-link NVLIPT ISR handler
 */
static void tu104_nvlink_nvlipt_isr(struct gk20a *g, u32 link_id)
{
	nvgpu_log(g, gpu_dbg_nvlink, "Nvlink NVLIPT ISR");
	/*
	 * Interrupt handling happens in leaf handlers. Assume all interrupts
	 * were handled and clear roll ups/
	 */
	IPT_REG_WR32(g, nvlipt_err_uc_first_link0_r(), IPT_ERR_UC_ACTIVE_BITS);
	IPT_REG_WR32(g, nvlipt_err_uc_status_link0_r(), IPT_ERR_UC_ACTIVE_BITS);

	return;
}

/*
 * Enable interrupts at top(IOCTRL) level
 */
static void tu104_nvlink_enable_ioctrl_link_intr(struct gk20a *g, u32 link_id,
								bool enable)
{
	u32 val = 0U;

	if (enable) {
		val = 1U;
	}

	/* Init IOCTRL */
	IOCTRL_REG_WR32(g, ioctrl_link_intr_0_mask_r(link_id),
				(ioctrl_link_intr_0_mask_fatal_f(val) |
					ioctrl_link_intr_0_mask_intra_f(val)));
}

void tu104_nvlink_init_link_err_intr(struct gk20a *g, u32 link_id)
{
	tu104_nvlink_init_tlc_link_err(g, link_id);
	tu104_nvlink_init_mif_link_err(g, link_id);
	tu104_nvlink_init_nvlipt_link_err(g, link_id);
}
/*
 *  * Enable link specific errors and interrupts (top-level)
 *   */
void tu104_nvlink_enable_link_err_intr(struct gk20a *g, u32 link_id, bool enable)
{
	tu104_nvlink_enable_ioctrl_link_intr(g, link_id, enable);
	g->ops.nvlink.minion.enable_link_intr(g, link_id, enable);
	tu104_nvlink_enable_dlpl_link_intr(g, link_id, enable);
	tu104_nvlink_enable_tlc_link_err(g, link_id, enable);
	tu104_nvlink_enable_mif_link_err(g, link_id, enable);
	tu104_nvlink_enable_nvlipt_link_err_intr(g, link_id, enable);
}

/*
 *  * Top level interrupt handler
 *   */
void tu104_nvlink_isr(struct gk20a *g)
{
	unsigned long links;
	u32 link_id;
	unsigned long bit;

	links = ioctrl_top_intr_0_status_link_v(
			IOCTRL_REG_RD32(g, ioctrl_top_intr_0_status_r()));
	nvgpu_log(g, gpu_dbg_nvlink, "Top-level nvlink ISR triggered on link:%lu", links);

	links &= g->nvlink.enabled_links;
	/* As per ARCH minion must be serviced first */
	g->ops.nvlink.minion.isr(g);

	for_each_set_bit(bit, &links, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		tu104_nvlink_dlpl_isr(g, link_id);
		tu104_nvlink_tlc_isr(g, link_id);
		tu104_nvlink_mif_isr(g, link_id);
		/* NVLIPT is top-level. Do it last */
		tu104_nvlink_nvlipt_isr(g, link_id);
	}
	return;
}

#endif /* CONFIG_NVGPU_NVLINK */
