/*
 * Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
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

#include "../osi/common/common.h"
#include "core_common.h"
#include "mgbe_core.h"
#include "eqos_core.h"
#include "xpcs.h"
#include "macsec.h"

static inline nve32_t poll_check(struct osi_core_priv_data *const osi_core, nveu8_t *addr,
				 nveu32_t bit_check, nveu32_t *value)
{
	nveu32_t retry = RETRY_COUNT;
	nve32_t cond = COND_NOT_MET;
	nveu32_t count;
	nve32_t ret = 0;

	/* Poll Until Poll Condition */
	count = 0;
	while (cond == COND_NOT_MET) {
		if (count > retry) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "poll_check: timeout\n", 0ULL);
			ret = -1;
			goto fail;
		}

		count++;

		*value = osi_readla(osi_core, addr);
		if ((*value & bit_check) == OSI_NONE) {
			cond = COND_MET;
		} else {
			osi_core->osd_ops.udelay(OSI_DELAY_1000US);
		}
	}
fail:
	return ret;
}


nve32_t hw_poll_for_swr(struct osi_core_priv_data *const osi_core)
{
	nveu32_t dma_mode_val = 0U;
	const nveu32_t dma_mode[2] = { EQOS_DMA_BMR, MGBE_DMA_MODE };
	void *addr = osi_core->base;

	return poll_check(osi_core, ((nveu8_t *)addr + dma_mode[osi_core->mac]),
			  DMA_MODE_SWR, &dma_mode_val);
}

void hw_start_mac(struct osi_core_priv_data *const osi_core)
{
	void *addr = osi_core->base;
	nveu32_t value;
	const nveu32_t mac_mcr_te_reg[2] = { EQOS_MAC_MCR, MGBE_MAC_TMCR };
	const nveu32_t mac_mcr_re_reg[2] = { EQOS_MAC_MCR, MGBE_MAC_RMCR };
	const nveu32_t set_bit_te[2] = { EQOS_MCR_TE, MGBE_MAC_TMCR_TE };
	const nveu32_t set_bit_re[2] = { EQOS_MCR_RE, MGBE_MAC_RMCR_RE };

	value = osi_readla(osi_core, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));
	value |= set_bit_te[osi_core->mac];
	osi_writela(osi_core, value, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));

	value = osi_readla(osi_core, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
	value |= set_bit_re[osi_core->mac];
	osi_writela(osi_core, value, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
}

void hw_stop_mac(struct osi_core_priv_data *const osi_core)
{
	void *addr = osi_core->base;
	nveu32_t value;
	const nveu32_t mac_mcr_te_reg[2] = { EQOS_MAC_MCR, MGBE_MAC_TMCR };
	const nveu32_t mac_mcr_re_reg[2] = { EQOS_MAC_MCR, MGBE_MAC_RMCR };
	const nveu32_t clear_bit_te[2] = { EQOS_MCR_TE, MGBE_MAC_TMCR_TE };
	const nveu32_t clear_bit_re[2] = { EQOS_MCR_RE, MGBE_MAC_RMCR_RE };

	value = osi_readla(osi_core, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));
	value &= ~clear_bit_te[osi_core->mac];
	osi_writela(osi_core, value, ((nveu8_t *)addr + mac_mcr_te_reg[osi_core->mac]));

	value = osi_readla(osi_core, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
	value &= ~clear_bit_re[osi_core->mac];
	osi_writela(osi_core, value, ((nveu8_t *)addr + mac_mcr_re_reg[osi_core->mac]));
}

nve32_t hw_set_mode(struct osi_core_priv_data *const osi_core, const nve32_t mode)
{
	void *base = osi_core->base;
	nveu32_t mcr_val;
	nve32_t ret = 0;
	const nveu32_t bit_set[2] = { EQOS_MCR_DO, EQOS_MCR_DM };
	const nveu32_t clear_bit[2] = { EQOS_MCR_DM, EQOS_MCR_DO };

	/* don't allow only if loopback mode is other than 0 or 1 */
	if ((mode != OSI_FULL_DUPLEX) && (mode != OSI_HALF_DUPLEX)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				"Invalid duplex mode\n", 0ULL);
		ret = -1;
		goto fail;
	}

	if (osi_core->mac == OSI_MAC_HW_EQOS) {
		mcr_val = osi_readla(osi_core, (nveu8_t *)base + EQOS_MAC_MCR);
		mcr_val |= bit_set[mode];
		mcr_val &= ~clear_bit[mode];
		osi_writela(osi_core, mcr_val, ((nveu8_t *)base + EQOS_MAC_MCR));
	}
fail:
	return ret;
}

nve32_t hw_set_speed(struct osi_core_priv_data *const osi_core, const nve32_t speed)
{
	nveu32_t  value;
	nve32_t  ret = 0;
	void *base = osi_core->base;
	const nveu32_t mac_mcr[2] = { EQOS_MAC_MCR, MGBE_MAC_TMCR };

	if (((osi_core->mac == OSI_MAC_HW_EQOS) && (speed > OSI_SPEED_1000)) ||
	    ((osi_core->mac == OSI_MAC_HW_MGBE) && ((speed < OSI_SPEED_2500) ||
	    (speed > OSI_SPEED_10000)))) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "unsupported speed\n", (nveul64_t)speed);
		ret = -1;
		goto fail;
	}

	value = osi_readla(osi_core, ((nveu8_t *)base + mac_mcr[osi_core->mac]));
	switch (speed) {
	case OSI_SPEED_10:
		value |= EQOS_MCR_PS;
		value &= ~EQOS_MCR_FES;
		break;
	case OSI_SPEED_100:
		value |= EQOS_MCR_PS;
		value |= EQOS_MCR_FES;
		break;
	case OSI_SPEED_1000:
		value &= ~EQOS_MCR_PS;
		value &= ~EQOS_MCR_FES;
		break;
	case OSI_SPEED_2500:
		value |= MGBE_MAC_TMCR_SS_2_5G;
		break;
	case OSI_SPEED_5000:
		value |= MGBE_MAC_TMCR_SS_5G;
		break;
	case OSI_SPEED_10000:
		value &= ~MGBE_MAC_TMCR_SS_10G;
		break;
	default:
		if (osi_core->mac == OSI_MAC_HW_EQOS) {
			value &= ~EQOS_MCR_PS;
			value &= ~EQOS_MCR_FES;
		} else if (osi_core->mac == OSI_MAC_HW_MGBE) {
			value &= ~MGBE_MAC_TMCR_SS_10G;
		} else {
			/* Do Nothing */
		}
		break;
	}
	osi_writela(osi_core, value, ((nveu8_t *)osi_core->base + mac_mcr[osi_core->mac]));

	if (osi_core->mac == OSI_MAC_HW_MGBE) {
		ret = xpcs_init(osi_core);
		if (ret < 0) {
			goto fail;
		}

		ret = xpcs_start(osi_core);
		if (ret < 0) {
			goto fail;
		}
	}
fail:
	return ret;
}


nve32_t hw_flush_mtl_tx_queue(struct osi_core_priv_data *const osi_core,
			      const nveu32_t q_inx)
{
	void *addr = osi_core->base;
	nveu32_t tx_op_mode_val = 0U;
	nveu32_t que_idx = (q_inx & 0xFU);
	nveu32_t value;
	const nveu32_t tx_op_mode[2] = { EQOS_MTL_CHX_TX_OP_MODE(que_idx),
					 MGBE_MTL_CHX_TX_OP_MODE(que_idx)};

	/* Read Tx Q Operating Mode Register and flush TxQ */
	value = osi_readla(osi_core, ((nveu8_t *)addr + tx_op_mode[osi_core->mac]));
	value |= MTL_QTOMR_FTQ;
	osi_writela(osi_core, value, ((nveu8_t *)addr + tx_op_mode[osi_core->mac]));

	/* Poll Until FTQ bit resets for Successful Tx Q flush */
	return poll_check(osi_core, ((nveu8_t *)addr + tx_op_mode[osi_core->mac]),
			  MTL_QTOMR_FTQ, &tx_op_mode_val);
}

nve32_t hw_config_fw_err_pkts(struct osi_core_priv_data *osi_core,
			      const nveu32_t q_inx, const nveu32_t enable_fw_err_pkts)
{
	nveu32_t val;
	nve32_t ret = 0;
	nveu32_t que_idx = (q_inx & 0xFU);
	const nveu32_t rx_op_mode[2] = { EQOS_MTL_CHX_RX_OP_MODE(que_idx),
					 MGBE_MTL_CHX_RX_OP_MODE(que_idx)};
#ifndef OSI_STRIPPED_LIB
	const nveu32_t max_q[2] = { OSI_EQOS_MAX_NUM_QUEUES,
				    OSI_MGBE_MAX_NUM_QUEUES};
	/* Check for valid enable_fw_err_pkts and que_idx values */
	if (((enable_fw_err_pkts != OSI_ENABLE) &&
	    (enable_fw_err_pkts != OSI_DISABLE)) ||
	    (que_idx >= max_q[osi_core->mac])) {
		ret = -1;
		goto fail;
	}

	/* Read MTL RXQ Operation_Mode Register */
	val = osi_readla(osi_core, ((nveu8_t *)osi_core->base +
			 rx_op_mode[osi_core->mac]));

	/* enable_fw_err_pkts, 1 is for enable and 0 is for disable */
	if (enable_fw_err_pkts == OSI_ENABLE) {
		/* When enable_fw_err_pkts bit is set, all packets except
		 * the runt error packets are forwarded to the application
		 * or DMA.
		 */
		val |= MTL_RXQ_OP_MODE_FEP;
	} else {
		/* When this bit is reset, the Rx queue drops packets with error
		 * status (CRC error, GMII_ER, watchdog timeout, or overflow)
		 */
		val &= ~MTL_RXQ_OP_MODE_FEP;
	}

	/* Write to FEP bit of MTL RXQ Operation Mode Register to enable or
	 * disable the forwarding of error packets to DMA or application.
	 */
	osi_writela(osi_core, val, ((nveu8_t *)osi_core->base +
		    rx_op_mode[osi_core->mac]));
fail:
	return ret;
#else
	/* using void to skip the misra error of unused variable */
	(void)enable_fw_err_pkts;
	/* Read MTL RXQ Operation_Mode Register */
	val = osi_readla(osi_core, ((nveu8_t *)osi_core->base +
			 rx_op_mode[osi_core->mac]));
	val |= MTL_RXQ_OP_MODE_FEP;
	/* Write to FEP bit of MTL RXQ Operation Mode Register to enable or
	 * disable the forwarding of error packets to DMA or application.
	 */
	osi_writela(osi_core, val, ((nveu8_t *)osi_core->base +
		    rx_op_mode[osi_core->mac]));

	return ret;
#endif /* !OSI_STRIPPED_LIB */
}

nve32_t hw_config_rxcsum_offload(struct osi_core_priv_data *const osi_core,
				nveu32_t enabled)
{
	void *addr = osi_core->base;
	nveu32_t value;
	nve32_t ret = 0;
	const nveu32_t rxcsum_mode[2] = { EQOS_MAC_MCR, MGBE_MAC_RMCR};
	const nveu32_t ipc_value[2] = { EQOS_MCR_IPC, MGBE_MAC_RMCR_IPC};

	if ((enabled != OSI_ENABLE) && (enabled != OSI_DISABLE)) {
		ret = -1;
		goto fail;
	}

	value = osi_readla(osi_core, ((nveu8_t *)addr + rxcsum_mode[osi_core->mac]));
	if (enabled == OSI_ENABLE) {
		value |= ipc_value[osi_core->mac];
	} else {
		value &= ~ipc_value[osi_core->mac];
	}

	osi_writela(osi_core, value, ((nveu8_t *)addr + rxcsum_mode[osi_core->mac]));
fail:
	return ret;
}

nve32_t hw_set_systime_to_mac(struct osi_core_priv_data *const osi_core,
			      const nveu32_t sec, const nveu32_t nsec)
{
	void *addr = osi_core->base;
	nveu32_t mac_tcr = 0U;
	nve32_t ret = 0;
	const nveu32_t mac_tscr[2] = { EQOS_MAC_TCR, MGBE_MAC_TCR};
	const nveu32_t mac_stsur[2] = { EQOS_MAC_STSUR, MGBE_MAC_STSUR};
	const nveu32_t mac_stnsur[2] = { EQOS_MAC_STNSUR, MGBE_MAC_STNSUR};

	ret = poll_check(osi_core, ((nveu8_t *)addr + mac_tscr[osi_core->mac]),
			 MAC_TCR_TSINIT, &mac_tcr);
	if (ret == -1) {
		goto fail;
	}

	/* write seconds value to MAC_System_Time_Seconds_Update register */
	osi_writela(osi_core, sec, ((nveu8_t *)addr + mac_stsur[osi_core->mac]));

	/* write nano seconds value to MAC_System_Time_Nanoseconds_Update
	 * register
	 */
	osi_writela(osi_core, nsec, ((nveu8_t *)addr + mac_stnsur[osi_core->mac]));

	/* issue command to update the configured secs and nsecs values */
	mac_tcr |= MAC_TCR_TSINIT;
	osi_writela(osi_core, mac_tcr, ((nveu8_t *)addr + mac_tscr[osi_core->mac]));

	ret = poll_check(osi_core, ((nveu8_t *)addr + mac_tscr[osi_core->mac]),
			 MAC_TCR_TSINIT, &mac_tcr);
fail:
	return ret;
}

nve32_t hw_config_addend(struct osi_core_priv_data *const osi_core,
			 const nveu32_t addend)
{
	void *addr = osi_core->base;
	nveu32_t mac_tcr = 0U;
	nve32_t ret = 0;
	const nveu32_t mac_tscr[2] = { EQOS_MAC_TCR, MGBE_MAC_TCR};
	const nveu32_t mac_tar[2] = { EQOS_MAC_TAR, MGBE_MAC_TAR};

	ret = poll_check(osi_core, ((nveu8_t *)addr + mac_tscr[osi_core->mac]),
			 MAC_TCR_TSADDREG, &mac_tcr);
	if (ret == -1) {
		goto fail;
	}

	/* write addend value to MAC_Timestamp_Addend register */
	osi_writela(osi_core, addend, ((nveu8_t *)addr + mac_tar[osi_core->mac]));

	/* issue command to update the configured addend value */
	mac_tcr |= MAC_TCR_TSADDREG;
	osi_writela(osi_core, mac_tcr, ((nveu8_t *)addr + mac_tscr[osi_core->mac]));

	ret = poll_check(osi_core, ((nveu8_t *)addr + mac_tscr[osi_core->mac]),
			 MAC_TCR_TSADDREG, &mac_tcr);
fail:
	return ret;
}

#ifndef OSI_STRIPPED_LIB
void hw_config_tscr(struct osi_core_priv_data *const osi_core, const nveu32_t ptp_filter)
#else
void hw_config_tscr(struct osi_core_priv_data *const osi_core, OSI_UNUSED const nveu32_t ptp_filter)
#endif /* !OSI_STRIPPED_LIB */
{
	void *addr = osi_core->base;
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	nveu32_t mac_tcr = 0U;
#ifndef OSI_STRIPPED_LIB
	nveu32_t i = 0U, temp = 0U;
#endif /* !OSI_STRIPPED_LIB */
	nveu32_t value = 0x0U;
	const nveu32_t mac_tscr[2] = { EQOS_MAC_TCR, MGBE_MAC_TCR};
	const nveu32_t mac_pps[2] = { EQOS_MAC_PPS_CTL, MGBE_MAC_PPS_CTL};

#ifndef OSI_STRIPPED_LIB
	if (ptp_filter != OSI_DISABLE) {
		mac_tcr = (OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT | OSI_MAC_TCR_TSCTRLSSR);
		for (i = 0U; i < 32U; i++) {
			temp = ptp_filter & OSI_BIT(i);

			switch (temp) {
			case OSI_MAC_TCR_SNAPTYPSEL_1:
				mac_tcr |= OSI_MAC_TCR_SNAPTYPSEL_1;
				break;
			case OSI_MAC_TCR_SNAPTYPSEL_2:
				mac_tcr |= OSI_MAC_TCR_SNAPTYPSEL_2;
				break;
			case OSI_MAC_TCR_SNAPTYPSEL_3:
				mac_tcr |= OSI_MAC_TCR_SNAPTYPSEL_3;
				break;
			case OSI_MAC_TCR_TSIPV4ENA:
				mac_tcr |= OSI_MAC_TCR_TSIPV4ENA;
				break;
			case OSI_MAC_TCR_TSIPV6ENA:
				mac_tcr |= OSI_MAC_TCR_TSIPV6ENA;
				break;
			case OSI_MAC_TCR_TSEVENTENA:
				mac_tcr |= OSI_MAC_TCR_TSEVENTENA;
				break;
			case OSI_MAC_TCR_TSMASTERENA:
				mac_tcr |= OSI_MAC_TCR_TSMASTERENA;
				break;
			case OSI_MAC_TCR_TSVER2ENA:
				mac_tcr |= OSI_MAC_TCR_TSVER2ENA;
				break;
			case OSI_MAC_TCR_TSIPENA:
				mac_tcr |= OSI_MAC_TCR_TSIPENA;
				break;
			case OSI_MAC_TCR_AV8021ASMEN:
				mac_tcr |= OSI_MAC_TCR_AV8021ASMEN;
				break;
			case OSI_MAC_TCR_TSENALL:
				mac_tcr |= OSI_MAC_TCR_TSENALL;
				break;
			case OSI_MAC_TCR_CSC:
				mac_tcr |= OSI_MAC_TCR_CSC;
				break;
			default:
				break;
			}
		}
	} else {
		/* Disabling the MAC time stamping */
		mac_tcr = OSI_DISABLE;
	}
#else
	mac_tcr = (OSI_MAC_TCR_TSENA | OSI_MAC_TCR_TSCFUPDT | OSI_MAC_TCR_TSCTRLSSR
		   | OSI_MAC_TCR_TSVER2ENA | OSI_MAC_TCR_TSIPENA | OSI_MAC_TCR_TSIPV6ENA |
		   OSI_MAC_TCR_TSIPV4ENA | OSI_MAC_TCR_SNAPTYPSEL_1);
#endif /* !OSI_STRIPPED_LIB */

	osi_writela(osi_core, mac_tcr, ((nveu8_t *)addr + mac_tscr[osi_core->mac]));

	value = osi_readla(osi_core, (nveu8_t *)addr + mac_pps[osi_core->mac]);
	value &= ~MAC_PPS_CTL_PPSCTRL0;
	if (l_core->pps_freq == OSI_ENABLE) {
		value |= OSI_ENABLE;
	}
	osi_writela(osi_core, value, ((nveu8_t *)addr + mac_pps[osi_core->mac]));
}

void hw_config_ssir(struct osi_core_priv_data *const osi_core)
{
	nveu32_t val = 0U;
	void *addr = osi_core->base;
	const struct core_local *l_core = (struct core_local *)(void *)osi_core;
	const nveu32_t mac_ssir[2] = { EQOS_MAC_SSIR, MGBE_MAC_SSIR};
	const nveu32_t ptp_ssinc[3] = {OSI_PTP_SSINC_4, OSI_PTP_SSINC_6, OSI_PTP_SSINC_4};

	/* by default Fine method is enabled */
	/* Fix the SSINC value based on Exact MAC used */
	val = ptp_ssinc[l_core->l_mac_ver];

	val |= val << MAC_SSIR_SSINC_SHIFT;
	/* update Sub-second Increment Value */
	osi_writela(osi_core, val, ((nveu8_t *)addr + mac_ssir[osi_core->mac]));
}

nve32_t hw_ptp_tsc_capture(struct osi_core_priv_data *const osi_core,
			   struct osi_core_ptp_tsc_data *data)
{
#ifndef OSI_STRIPPED_LIB
	const struct core_local *l_core = (struct core_local *)osi_core;
#endif /* !OSI_STRIPPED_LIB */
	void *addr = osi_core->base;
	nveu32_t tsc_ptp = 0U;
	nve32_t ret = 0;

#ifndef OSI_STRIPPED_LIB
	/* This code is NA for Orin use case */
	if (l_core->l_mac_ver < MAC_CORE_VER_TYPE_EQOS_5_30) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "ptp_tsc: older IP\n", 0ULL);
		ret = -1;
		goto done;
	}
#endif /* !OSI_STRIPPED_LIB */

	osi_writela(osi_core, OSI_ENABLE, (nveu8_t *)osi_core->base + WRAP_SYNC_TSC_PTP_CAPTURE);

	ret = poll_check(osi_core, ((nveu8_t *)addr + WRAP_SYNC_TSC_PTP_CAPTURE),
			 OSI_ENABLE, &tsc_ptp);
	if (ret == -1) {
		goto done;
	}

	data->tsc_low_bits = osi_readla(osi_core, (nveu8_t *)osi_core->base +
					WRAP_TSC_CAPTURE_LOW);
	data->tsc_high_bits =  osi_readla(osi_core, (nveu8_t *)osi_core->base +
					  WRAP_TSC_CAPTURE_HIGH);
	data->ptp_low_bits =  osi_readla(osi_core, (nveu8_t *)osi_core->base +
					 WRAP_PTP_CAPTURE_LOW);
	data->ptp_high_bits =  osi_readla(osi_core, (nveu8_t *)osi_core->base +
					  WRAP_PTP_CAPTURE_HIGH);
done:
	return ret;
}

#ifndef OSI_STRIPPED_LIB
static inline void config_l2_da_perfect_inverse_match(
					struct osi_core_priv_data *osi_core,
					nveu32_t perfect_inverse_match)
{
	nveu32_t value = 0U;

	value = osi_readla(osi_core, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));
	value &= ~MAC_PFR_DAIF;
	if (perfect_inverse_match == OSI_INV_MATCH) {
		/* Set DA Inverse Filtering */
		value |= MAC_PFR_DAIF;
	}
	osi_writela(osi_core, value, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));
}
#endif /* !OSI_STRIPPED_LIB */

nve32_t hw_config_mac_pkt_filter_reg(struct osi_core_priv_data *const osi_core,
				     const struct osi_filter *filter)
{
	nveu32_t value = 0U;
	nve32_t ret = 0;

	value = osi_readla(osi_core, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));

	/*Retain all other values */
	value &= (MAC_PFR_DAIF | MAC_PFR_DBF  | MAC_PFR_SAIF |
		  MAC_PFR_SAF  | MAC_PFR_PCF  | MAC_PFR_VTFE |
		  MAC_PFR_IPFE | MAC_PFR_DNTU | MAC_PFR_RA);

	if ((filter->oper_mode & OSI_OPER_EN_PERFECT) != OSI_DISABLE) {
		value |= MAC_PFR_HPF;
	}

#ifndef OSI_STRIPPED_LIB
	if ((filter->oper_mode & OSI_OPER_DIS_PERFECT) != OSI_DISABLE) {
		value &= ~MAC_PFR_HPF;
	}

	if ((filter->oper_mode & OSI_OPER_EN_PROMISC) != OSI_DISABLE) {
		value |= MAC_PFR_PR;
	}

	if ((filter->oper_mode & OSI_OPER_DIS_PROMISC) != OSI_DISABLE) {
		value &= ~MAC_PFR_PR;
	}

	if ((filter->oper_mode & OSI_OPER_EN_ALLMULTI) != OSI_DISABLE) {
		value |= MAC_PFR_PM;
	}

	if ((filter->oper_mode & OSI_OPER_DIS_ALLMULTI) != OSI_DISABLE) {
		value &= ~MAC_PFR_PM;
	}
#endif /* !OSI_STRIPPED_LIB */

	osi_writela(osi_core, value,
		    ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));

#ifndef OSI_STRIPPED_LIB
	if ((filter->oper_mode & OSI_OPER_EN_L2_DA_INV) != OSI_DISABLE) {
		config_l2_da_perfect_inverse_match(osi_core, OSI_INV_MATCH);
	}

	if ((filter->oper_mode & OSI_OPER_DIS_L2_DA_INV) != OSI_DISABLE) {
		config_l2_da_perfect_inverse_match(osi_core, OSI_PFT_MATCH);
	}
#else
	value = osi_readla(osi_core, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));
	value &= ~MAC_PFR_DAIF;
	osi_writela(osi_core, value, ((nveu8_t *)osi_core->base + MAC_PKT_FILTER_REG));

#endif /* !OSI_STRIPPED_LIB */

	return ret;
}

nve32_t hw_config_l3_l4_filter_enable(struct osi_core_priv_data *const osi_core,
				      const nveu32_t filter_enb_dis)
{
	nveu32_t value = 0U;
	void *base = osi_core->base;
	nve32_t ret = 0;

	/* validate filter_enb_dis argument */
	if ((filter_enb_dis != OSI_ENABLE) && (filter_enb_dis != OSI_DISABLE)) {
		OSI_CORE_ERR(OSI_NULL, OSI_LOG_ARG_INVALID,
			     "Invalid filter_enb_dis value\n",
			     filter_enb_dis);
		ret = -1;
		goto fail;
	}

	value = osi_readla(osi_core, ((nveu8_t *)base + MAC_PKT_FILTER_REG));
	value &= ~(MAC_PFR_IPFE);
	value |= ((filter_enb_dis << MAC_PFR_IPFE_SHIFT) & MAC_PFR_IPFE);
	osi_writela(osi_core, value, ((nveu8_t *)base + MAC_PKT_FILTER_REG));
fail:
	return ret;
}

/**
 * @brief hw_est_read - indirect read the GCL to Software own list
 * (SWOL)
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] addr_val: Address offset for indirect write.
 * @param[in] data: Data to be written at offset.
 * @param[in] gcla: Gate Control List Address, 0 for ETS register.
 *	      1 for GCL memory.
 * @param[in] bunk: Memory bunk from which vlaues will be read. Possible
 *            value 0 or 1.
 * @param[in] mac: MAC index
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t hw_est_read(struct osi_core_priv_data *osi_core,
				  nveu32_t addr_val, nveu32_t *data,
				  nveu32_t gcla, nveu32_t bunk,
				  nveu32_t mac)
{
	nve32_t retry = 1000;
	nveu32_t val = 0U;
	nve32_t ret;
	const nveu32_t MTL_EST_GCL_CONTROL[MAX_MAC_IP_TYPES] = {
			EQOS_MTL_EST_GCL_CONTROL, MGBE_MTL_EST_GCL_CONTROL};
	const nveu32_t MTL_EST_DATA[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_DATA, MGBE_MTL_EST_DATA};

	*data = 0U;
	val &= ~MTL_EST_ADDR_MASK;
	val |= (gcla == 1U) ? 0x0U : MTL_EST_GCRR;
	val |= MTL_EST_SRWO | MTL_EST_R1W0 | MTL_EST_DBGM | bunk | addr_val;
	osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
		    MTL_EST_GCL_CONTROL[mac]);

	while (--retry > 0) {
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MTL_EST_GCL_CONTROL[mac]);
		if ((val & MTL_EST_SRWO) == MTL_EST_SRWO) {
			continue;
		}
		osi_core->osd_ops.udelay(OSI_DELAY_1US);

		break;
	}

	if (((val & MTL_EST_ERR0) == MTL_EST_ERR0) ||
	    (retry <= 0)) {
		ret = -1;
		goto err;
	}

	*data = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			   MTL_EST_DATA[mac]);
	ret = 0;
err:
	return ret;
}

/**
 * @brief eqos_gcl_validate - validate GCL from user
 *
 * Algorithm: validate GCL size and width of time interval value
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: Configuration input argument.
 * @param[in] btr: Base time register value.
 * @param[in] mac: MAC index
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t gcl_validate(struct osi_core_priv_data *const osi_core,
			    struct osi_est_config *const est,
			    const nveu32_t *btr, nveu32_t mac)
{
	const struct core_local *l_core = (struct core_local *)(void *)osi_core;
	const nveu32_t PTP_CYCLE_8[MAX_MAC_IP_TYPES] = {EQOS_8PTP_CYCLE,
						  MGBE_8PTP_CYCLE};
	const nveu32_t MTL_EST_CONTROL[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL,
						MGBE_MTL_EST_CONTROL};
	const nveu32_t MTL_EST_STATUS[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_STATUS,
						MGBE_MTL_EST_STATUS};
	const nveu32_t MTL_EST_BTR_LOW[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_BTR_LOW,
						MGBE_MTL_EST_BTR_LOW};
	const nveu32_t MTL_EST_BTR_HIGH[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_BTR_HIGH,
						MGBE_MTL_EST_BTR_HIGH};
	const nveu32_t MTL_EST_CTR_LOW[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CTR_LOW,
						MGBE_MTL_EST_CTR_LOW};
	const nveu32_t MTL_EST_CTR_HIGH[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CTR_HIGH,
						MGBE_MTL_EST_CTR_HIGH};
	nveu32_t i;
	nveu64_t sum_ti = 0U;
	nveu64_t sum_tin = 0U;
	nveu64_t ctr = 0U;
	nveu64_t btr_new = 0U;
	nveu32_t btr_l, btr_h, ctr_l, ctr_h;
	nveu32_t bunk = 0U;
	nveu32_t est_status;
	nveu64_t old_btr, old_ctr;
	nve32_t ret = 0;
	nveu32_t val = 0U;
	nveu64_t rem = 0U;
	const struct est_read hw_read_arr[4] = {
				    {&btr_l, MTL_EST_BTR_LOW[mac]},
				    {&btr_h, MTL_EST_BTR_HIGH[mac]},
				    {&ctr_l, MTL_EST_CTR_LOW[mac]},
				    {&ctr_h, MTL_EST_CTR_HIGH[mac]}};

	if (est->en_dis > OSI_ENABLE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "input argument en_dis value\n",
			     (nveul64_t)est->en_dis);
		ret = -1;
		goto done;
	}

	if (est->llr > l_core->gcl_dep) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "input argument more than GCL depth\n",
			     (nveul64_t)est->llr);
		ret = -1;
		goto done;
	}

	/* 24 bit configure time in GCL + 7) */
	if (est->ter > 0x7FFFFFFFU) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "invalid TER value\n",
			     (nveul64_t)est->ter);
		ret = -1;
		goto done;
	}

	/* nenosec register value can't be more than 10^9 nsec */
	if ((est->ctr[0] > OSI_NSEC_PER_SEC) ||
	    (est->btr[0] > OSI_NSEC_PER_SEC) ||
	    (est->ctr[1] > 0xFFU)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "input argument CTR/BTR nsec is invalid\n",
			     0ULL);
		ret = -1;
		goto done;
	}

	/* if btr + offset is more than limit */
	if ((est->btr[0] > (OSI_NSEC_PER_SEC - est->btr_offset[0])) ||
	    (est->btr[1] > (UINT_MAX - est->btr_offset[1]))) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "input argument BTR offset is invalid\n",
			     0ULL);
		ret = -1;
		goto done;
	}

	ctr = ((nveu64_t)est->ctr[1] * OSI_NSEC_PER_SEC)  + est->ctr[0];
	btr_new = (((nveu64_t)btr[1] + est->btr_offset[1]) * OSI_NSEC_PER_SEC) +
		   (btr[0] + est->btr_offset[0]);
	for (i = 0U; i < est->llr; i++) {
		if (est->gcl[i] <= l_core->gcl_width_val) {
			sum_ti += ((nveu64_t)est->gcl[i] & l_core->ti_mask);
			if ((sum_ti > ctr) &&
			    ((ctr - sum_tin) >= PTP_CYCLE_8[mac])) {
				continue;
			} else if (((ctr - sum_ti) != 0U) &&
				   ((ctr - sum_ti) < PTP_CYCLE_8[mac])) {
				OSI_CORE_ERR(osi_core->osd,
					     OSI_LOG_ARG_INVALID,
					     "CTR issue due to trancate\n",
					     (nveul64_t)i);
				ret = -1;
				goto done;
			} else {
				//do nothing
			}
			sum_tin = sum_ti;
			continue;
		}

		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "validation of GCL entry failed\n",
			     (nveul64_t)i);
		ret = -1;
		goto done;
	}

	/* Check for BTR in case of new ETS while current GCL enabled */

	val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			 MTL_EST_CONTROL[mac]);
	if ((val & MTL_EST_CONTROL_EEST) != MTL_EST_CONTROL_EEST) {
		ret = 0;
		goto done;
	}

	/* Read EST_STATUS for bunk */
	est_status = osi_readla(osi_core,
				(nveu8_t *)osi_core->base +
				MTL_EST_STATUS[mac]);
	if ((est_status & MTL_EST_STATUS_SWOL) == 0U) {
		bunk = MTL_EST_DBGB;
	}

	/* Read last BTR and CTR */
	for (i = 0U; i < (sizeof(hw_read_arr) / sizeof(hw_read_arr[0])); i++) {
		ret = hw_est_read(osi_core, hw_read_arr[i].addr,
				  hw_read_arr[i].var, OSI_DISABLE,
				  bunk, mac);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "Reading failed for index\n",
				     (nveul64_t)i);
			goto done;
		}
	}

	old_btr = btr_l + ((nveu64_t)btr_h * OSI_NSEC_PER_SEC);
	old_ctr = ctr_l + ((nveu64_t)ctr_h * OSI_NSEC_PER_SEC);
	if (old_btr > btr_new) {
		rem = (old_btr - btr_new) % old_ctr;
		if ((rem != OSI_NONE) && (rem < PTP_CYCLE_8[mac])) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "invalid BTR", (nveul64_t)rem);
			ret = -1;
			goto done;
		}
	} else if (btr_new > old_btr) {
		rem = (btr_new - old_btr) % old_ctr;
		if ((rem != OSI_NONE) && (rem < PTP_CYCLE_8[mac])) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "invalid BTR", (nveul64_t)rem);
			ret = -1;
			goto done;
		}
	} else {
		// Nothing to do
	}

done:
	return ret;
}

/**
 * @brief hw_est_write - indirect write the GCL to Software own list
 * (SWOL)
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] addr_val: Address offset for indirect write.
 * @param[in] data: Data to be written at offset.
 * @param[in] gcla: Gate Control List Address, 0 for ETS register.
 *	      1 for GCL memory.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t hw_est_write(struct osi_core_priv_data *osi_core,
			    nveu32_t addr_val, nveu32_t data,
			    nveu32_t gcla)
{
	nve32_t retry = 1000;
	nveu32_t val = 0x0;
	nve32_t ret = 0;
	const nveu32_t MTL_EST_DATA[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_DATA,
						MGBE_MTL_EST_DATA};
	const nveu32_t MTL_EST_GCL_CONTROL[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_GCL_CONTROL,
						MGBE_MTL_EST_GCL_CONTROL};

	osi_writela(osi_core, data, (nveu8_t *)osi_core->base +
		   MTL_EST_DATA[osi_core->mac]);

	val &= ~MTL_EST_ADDR_MASK;
	val |= (gcla == 1U) ? 0x0U : MTL_EST_GCRR;
	val |= MTL_EST_SRWO;
	val |= addr_val;
	osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
		    MTL_EST_GCL_CONTROL[osi_core->mac]);

	while (--retry > 0) {
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MTL_EST_GCL_CONTROL[osi_core->mac]);
		if ((val & MTL_EST_SRWO) == MTL_EST_SRWO) {
			osi_core->osd_ops.udelay(OSI_DELAY_1US);
			continue;
		}

		break;
	}

	if (((val & MTL_EST_ERR0) == MTL_EST_ERR0) ||
	    (retry <= 0)) {
		ret = -1;
	}

	return ret;
}

/**
 * @brief hw_config_est - Read Setting for GCL from input and update
 * registers.
 *
 * Algorithm:
 * 1) Write  TER, LLR and EST control register
 * 2) Update GCL to sw own GCL (MTL_EST_Status bit SWOL will tell which is
 *    owned by SW) and store which GCL is in use currently in sw.
 * 3) TODO set DBGB and DBGM for debugging
 * 4) EST_data and GCRR to 1, update entry sno in ADDR and put data at
 *    est_gcl_data enable GCL MTL_EST_SSWL and wait for self clear or use
 *    SWLC in MTL_EST_Status. Please note new GCL will be pushed for each entry.
 * 5) Configure btr. Update btr based on current time (current time
 *    should be updated based on PTP by this time)
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: EST configuration input argument.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t hw_config_est(struct osi_core_priv_data *const osi_core,
		      struct osi_est_config *const est)
{
	nveu32_t btr[2] = {0};
	nveu32_t val = 0x0;
	void *base = osi_core->base;
	nveu32_t i;
	nve32_t ret = 0;
	nveu32_t addr = 0x0;
	const nveu32_t MTL_EST_CONTROL[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL,
						MGBE_MTL_EST_CONTROL};
	const nveu32_t MTL_EST_BTR_LOW[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_BTR_LOW,
						MGBE_MTL_EST_BTR_LOW};
	const nveu32_t MTL_EST_BTR_HIGH[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_BTR_HIGH,
						MGBE_MTL_EST_BTR_HIGH};
	const nveu32_t MTL_EST_CTR_LOW[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CTR_LOW,
						MGBE_MTL_EST_CTR_LOW};
	const nveu32_t MTL_EST_CTR_HIGH[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CTR_HIGH,
						MGBE_MTL_EST_CTR_HIGH};
	const nveu32_t MTL_EST_TER[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_TER,
						MGBE_MTL_EST_TER};
	const nveu32_t MTL_EST_LLR[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_LLR,
						MGBE_MTL_EST_LLR};

	if ((osi_core->hw_feature != OSI_NULL) &&
	    (osi_core->hw_feature->est_sel == OSI_DISABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "EST not supported in HW\n", 0ULL);
		ret = -1;
		goto done;
	}

	if (est->en_dis == OSI_DISABLE) {
		val = osi_readla(osi_core, (nveu8_t *)base +
				 MTL_EST_CONTROL[osi_core->mac]);
		val &= ~MTL_EST_EEST;
		osi_writela(osi_core, val, (nveu8_t *)base +
			    MTL_EST_CONTROL[osi_core->mac]);

		ret = 0;
	} else {
		btr[0] = est->btr[0];
		btr[1] = est->btr[1];
		if ((btr[0] == 0U) && (btr[1] == 0U)) {
			common_get_systime_from_mac(osi_core->base,
						    osi_core->mac,
						    &btr[1], &btr[0]);
		}

		if (gcl_validate(osi_core, est, btr, osi_core->mac) < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "GCL validation failed\n", 0LL);
			ret = -1;
			goto done;
		}

		ret = hw_est_write(osi_core, MTL_EST_CTR_LOW[osi_core->mac], est->ctr[0], 0);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "GCL CTR[0] failed\n", 0LL);
			goto done;
		}
		/* check for est->ctr[i]  not more than FF, TODO as per hw config
		 * parameter we can have max 0x3 as this value in sec */
		est->ctr[1] &= MTL_EST_CTR_HIGH_MAX;
		ret = hw_est_write(osi_core, MTL_EST_CTR_HIGH[osi_core->mac], est->ctr[1], 0);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "GCL CTR[1] failed\n", 0LL);
			goto done;
		}

		ret = hw_est_write(osi_core, MTL_EST_TER[osi_core->mac], est->ter, 0);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "GCL TER failed\n", 0LL);
			goto done;
		}

		ret = hw_est_write(osi_core, MTL_EST_LLR[osi_core->mac], est->llr, 0);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "GCL LLR failed\n", 0LL);
			goto done;
		}

		/* Write GCL table */
		for (i = 0U; i < est->llr; i++) {
			addr = i;
			addr = addr << MTL_EST_ADDR_SHIFT;
			addr &= MTL_EST_ADDR_MASK;
			ret = hw_est_write(osi_core, addr, est->gcl[i], 1);
			if (ret < 0) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
					     "GCL enties write failed\n",
					     (nveul64_t)i);
				goto done;
			}
		}

		/* Write parameters */
		ret = hw_est_write(osi_core, MTL_EST_BTR_LOW[osi_core->mac],
				btr[0] + est->btr_offset[0], OSI_DISABLE);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "GCL BTR[0] failed\n",
				     (btr[0] + est->btr_offset[0]));
			goto done;
		}

		ret = hw_est_write(osi_core, MTL_EST_BTR_HIGH[osi_core->mac],
				btr[1] + est->btr_offset[1], OSI_DISABLE);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "GCL BTR[1] failed\n",
				     (btr[1] + est->btr_offset[1]));
			goto done;
		}

		val = osi_readla(osi_core, (nveu8_t *)base +
				 MTL_EST_CONTROL[osi_core->mac]);
		/* Store table */
		val |= MTL_EST_SSWL;
		val |= MTL_EST_EEST;
		val |= MTL_EST_QHLBF;
		osi_writela(osi_core, val, (nveu8_t *)base +
			    MTL_EST_CONTROL[osi_core->mac]);
	}
done:
	return ret;
}

/**
 * @brief hw_config_fpe - Read Setting for preemption and express for TC
 * and update registers.
 *
 * Algorithm:
 * 1) Check for TC enable and TC has masked for setting to preemptable.
 * 2) update FPE control status register
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] fpe: FPE configuration input argument.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t hw_config_fpe(struct osi_core_priv_data *const osi_core,
		      struct osi_fpe_config *const fpe)
{
	nveu32_t i = 0U;
	nveu32_t val = 0U;
	nveu32_t temp = 0U, temp1 = 0U;
	nveu32_t temp_shift = 0U;
	nve32_t ret = 0;
	const nveu32_t MTL_FPE_CTS[MAX_MAC_IP_TYPES] = {EQOS_MTL_FPE_CTS,
						MGBE_MTL_FPE_CTS};
	const nveu32_t MAC_FPE_CTS[MAX_MAC_IP_TYPES] = {EQOS_MAC_FPE_CTS,
						MGBE_MAC_FPE_CTS};
	const nveu32_t max_number_queue[MAX_MAC_IP_TYPES] = {OSI_EQOS_MAX_NUM_QUEUES,
						OSI_MGBE_MAX_NUM_QUEUES};
	const nveu32_t MAC_RQC1R[MAX_MAC_IP_TYPES] = {EQOS_MAC_RQC1R,
						MGBE_MAC_RQC1R};
	const nveu32_t MAC_RQC1R_RQ[MAX_MAC_IP_TYPES] = {EQOS_MAC_RQC1R_FPRQ,
						MGBE_MAC_RQC1R_RQ};
	const nveu32_t MAC_RQC1R_RQ_SHIFT[MAX_MAC_IP_TYPES] = {EQOS_MAC_RQC1R_FPRQ_SHIFT,
						MGBE_MAC_RQC1R_RQ_SHIFT};
	const nveu32_t MTL_FPE_ADV[MAX_MAC_IP_TYPES] = {EQOS_MTL_FPE_ADV,
						MGBE_MTL_FPE_ADV};

	if ((osi_core->hw_feature != OSI_NULL) &&
	    (osi_core->hw_feature->fpe_sel == OSI_DISABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "FPE not supported in HW\n", 0ULL);
		ret = -1;
		goto error;
	}

	/* Only 8 TC */
	if (fpe->tx_queue_preemption_enable > 0xFFU) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "FPE input tx_queue_preemption_enable is invalid\n",
			     (nveul64_t)fpe->tx_queue_preemption_enable);
		ret = -1;
		goto error;
	}

	if (osi_core->mac == OSI_MAC_HW_MGBE) {
#ifdef MACSEC_SUPPORT
		osi_lock_irq_enabled(&osi_core->macsec_fpe_lock);
		/* MACSEC and FPE cannot coexist on MGBE refer bug 3484034 */
		if (osi_core->is_macsec_enabled == OSI_ENABLE) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "FPE and MACSEC cannot co-exist\n", 0ULL);
			ret = -1;
			goto done;
		}
#endif /*  MACSEC_SUPPORT */
	}

	osi_core->fpe_ready = OSI_DISABLE;

	if (((fpe->tx_queue_preemption_enable << MTL_FPE_CTS_PEC_SHIFT) &
	     MTL_FPE_CTS_PEC) == OSI_DISABLE) {
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MTL_FPE_CTS[osi_core->mac]);
		val &= ~MTL_FPE_CTS_PEC;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MTL_FPE_CTS[osi_core->mac]);

		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MAC_FPE_CTS[osi_core->mac]);
		val &= ~MAC_FPE_CTS_EFPE;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MAC_FPE_CTS[osi_core->mac]);

		if (osi_core->mac == OSI_MAC_HW_MGBE) {
#ifdef MACSEC_SUPPORT
			osi_core->is_fpe_enabled = OSI_DISABLE;
#endif /*  MACSEC_SUPPORT */
		}
		ret = 0;
	} else {
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MTL_FPE_CTS[osi_core->mac]);
		val &= ~MTL_FPE_CTS_PEC;
		for (i = 0U; i < OSI_MAX_TC_NUM; i++) {
			/* max 8 bit for this structure fot TC/TXQ. Set the TC for express or
			 * preemption. Default is express for a TC. DWCXG_NUM_TC = 8 */
			temp = OSI_BIT(i);
			if ((fpe->tx_queue_preemption_enable & temp) == temp) {
				temp_shift = i;
				temp_shift += MTL_FPE_CTS_PEC_SHIFT;
				/* set queue for preemtable */
				if (temp_shift < MTL_FPE_CTS_PEC_MAX_SHIFT) {
					temp1 = OSI_ENABLE;
					temp1 = temp1 << temp_shift;
					val |= temp1;
				} else {
					/* Do nothing */
				}
			}
		}
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MTL_FPE_CTS[osi_core->mac]);

		if ((fpe->rq == 0x0U) || (fpe->rq >= max_number_queue[osi_core->mac])) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "FPE init failed due to wrong RQ\n", fpe->rq);
			ret = -1;
			goto done;
		}

		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MAC_RQC1R[osi_core->mac]);
		val &= ~MAC_RQC1R_RQ[osi_core->mac];
		temp = fpe->rq;
		temp = temp << MAC_RQC1R_RQ_SHIFT[osi_core->mac];
		temp = (temp & MAC_RQC1R_RQ[osi_core->mac]);
		val |= temp;
		osi_core->residual_queue = fpe->rq;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MAC_RQC1R[osi_core->mac]);

		if (osi_core->mac == OSI_MAC_HW_MGBE) {
			val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
					 MGBE_MAC_RQC4R);
			val &= ~MGBE_MAC_RQC4R_PMCBCQ;
			temp = fpe->rq;
			temp = temp << MGBE_MAC_RQC4R_PMCBCQ_SHIFT;
			temp = (temp & MGBE_MAC_RQC4R_PMCBCQ);
			val |= temp;
			osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
				    MGBE_MAC_RQC4R);
		}
		/* initiate SVER for SMD-V and SMD-R */
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MTL_FPE_CTS[osi_core->mac]);
		val |= MAC_FPE_CTS_SVER;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MAC_FPE_CTS[osi_core->mac]);

		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MTL_FPE_ADV[osi_core->mac]);
		val &= ~MTL_FPE_ADV_HADV_MASK;
		//(minimum_fragment_size +IPG/EIPG + Preamble) *.8 ~98ns for10G
		val |= MTL_FPE_ADV_HADV_VAL;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MTL_FPE_ADV[osi_core->mac]);

		if (osi_core->mac == OSI_MAC_HW_MGBE) {
#ifdef MACSEC_SUPPORT
			osi_core->is_fpe_enabled = OSI_ENABLE;
#endif /*  MACSEC_SUPPORT */
		}
	}
done:

	if (osi_core->mac == OSI_MAC_HW_MGBE) {
#ifdef MACSEC_SUPPORT
		osi_unlock_irq_enabled(&osi_core->macsec_fpe_lock);
#endif /*  MACSEC_SUPPORT */
	}

error:
	return ret;
}

/**
 * @brief enable_mtl_interrupts - Enable MTL interrupts
 *
 * Algorithm: enable MTL interrupts for EST
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void enable_mtl_interrupts(struct osi_core_priv_data *osi_core)
{
	nveu32_t  mtl_est_ir = OSI_DISABLE;
	const nveu32_t MTL_EST_ITRE[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_ITRE,
							 MGBE_MTL_EST_ITRE};

	mtl_est_ir = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				MTL_EST_ITRE[osi_core->mac]);
	/* enable only MTL interrupt realted to
	 * Constant Gate Control Error
	 * Head-Of-Line Blocking due to Scheduling
	 * Head-Of-Line Blocking due to Frame Size
	 * BTR Error
	 * Switch to S/W owned list Complete
	 */
	mtl_est_ir |= (MTL_EST_ITRE_CGCE | MTL_EST_ITRE_IEHS |
		       MTL_EST_ITRE_IEHF | MTL_EST_ITRE_IEBE |
		       MTL_EST_ITRE_IECC);
	osi_writela(osi_core, mtl_est_ir, (nveu8_t *)osi_core->base +
		    MTL_EST_ITRE[osi_core->mac]);
}

/**
 * @brief enable_fpe_interrupts - Enable MTL interrupts
 *
 * Algorithm: enable FPE interrupts
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void enable_fpe_interrupts(struct osi_core_priv_data *osi_core)
{
	nveu32_t  value = OSI_DISABLE;
	const nveu32_t MAC_IER[MAX_MAC_IP_TYPES] = {EQOS_MAC_IMR,
						    MGBE_MAC_IER};
	const nveu32_t IMR_FPEIE[MAX_MAC_IP_TYPES] = {EQOS_IMR_FPEIE,
						      MGBE_IMR_FPEIE};

	/* Read MAC IER Register and enable Frame Preemption Interrupt
	 * Enable */
	value = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			   MAC_IER[osi_core->mac]);
	value |= IMR_FPEIE[osi_core->mac];
	osi_writela(osi_core, value, (nveu8_t *)osi_core->base +
		    MAC_IER[osi_core->mac]);
}

/**
 * @brief save_gcl_params - save GCL configs in local core structure
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
static inline void save_gcl_params(struct osi_core_priv_data *osi_core)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	const nveu32_t gcl_widhth[4] = {0, OSI_MAX_24BITS, OSI_MAX_28BITS,
					OSI_MAX_32BITS};
	const nveu32_t gcl_ti_mask[4] = {0, OSI_MASK_16BITS, OSI_MASK_20BITS,
					 OSI_MASK_24BITS};
	const nveu32_t gcl_depthth[6] = {0, OSI_GCL_SIZE_64, OSI_GCL_SIZE_128,
					 OSI_GCL_SIZE_256, OSI_GCL_SIZE_512,
					 OSI_GCL_SIZE_1024};

	if ((osi_core->hw_feature->gcl_width == 0U) ||
	    (osi_core->hw_feature->gcl_width > 3U)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Wrong HW feature GCL width\n",
			     (nveul64_t)osi_core->hw_feature->gcl_width);
	} else {
		l_core->gcl_width_val =
				    gcl_widhth[osi_core->hw_feature->gcl_width];
		l_core->ti_mask = gcl_ti_mask[osi_core->hw_feature->gcl_width];
	}

	if ((osi_core->hw_feature->gcl_depth == 0U) ||
	    (osi_core->hw_feature->gcl_depth > 5U)) {
		/* Do Nothing */
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Wrong HW feature GCL depth\n",
			   (nveul64_t)osi_core->hw_feature->gcl_depth);
	} else {
		l_core->gcl_dep = gcl_depthth[osi_core->hw_feature->gcl_depth];
	}
}

/**
 * @brief hw_tsn_init - initialize TSN feature
 *
 * Algorithm:
 * 1) If hardware support EST,
 *   a) Set default EST configuration
 *   b) Set enable interrupts
 * 2) If hardware supports FPE
 *   a) Set default FPE configuration
 *   b) enable interrupts
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est_sel: EST HW support present or not
 * @param[in] fpe_sel: FPE HW support present or not
 *
 * @note MAC should be init and started. see osi_start_mac()
 */
void hw_tsn_init(struct osi_core_priv_data *osi_core,
		 nveu32_t est_sel, nveu32_t fpe_sel)
{
	nveu32_t val = 0x0;
	nveu32_t temp = 0U;
	const nveu32_t MTL_EST_CONTROL[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL,
							MGBE_MTL_EST_CONTROL};
	const nveu32_t MTL_EST_CONTROL_PTOV[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL_PTOV,
							MGBE_MTL_EST_CONTROL_PTOV};
	const nveu32_t MTL_EST_PTOV_RECOMMEND[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_PTOV_RECOMMEND,
							MGBE_MTL_EST_PTOV_RECOMMEND};
	const nveu32_t MTL_EST_CONTROL_PTOV_SHIFT[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL_PTOV_SHIFT,
							MGBE_MTL_EST_CONTROL_PTOV_SHIFT};
	const nveu32_t MTL_EST_CONTROL_CTOV[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL_CTOV,
							MGBE_MTL_EST_CONTROL_CTOV};
	const nveu32_t MTL_EST_CTOV_RECOMMEND[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CTOV_RECOMMEND,
							MGBE_MTL_EST_CTOV_RECOMMEND};
	const nveu32_t MTL_EST_CONTROL_CTOV_SHIFT[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL_CTOV_SHIFT,
							MGBE_MTL_EST_CONTROL_CTOV_SHIFT};
	const nveu32_t MTL_EST_CONTROL_LCSE[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL_LCSE,
							MGBE_MTL_EST_CONTROL_LCSE};
	const nveu32_t MTL_EST_CONTROL_LCSE_VAL[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL_LCSE_VAL,
							MGBE_MTL_EST_CONTROL_LCSE_VAL};
	const nveu32_t MTL_EST_CONTROL_DDBF[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL_DDBF,
							MGBE_MTL_EST_CONTROL_DDBF};
	const nveu32_t MTL_EST_OVERHEAD[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_OVERHEAD,
							MGBE_MTL_EST_OVERHEAD};
	const nveu32_t MTL_EST_OVERHEAD_OVHD[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_OVERHEAD_OVHD,
							MGBE_MTL_EST_OVERHEAD_OVHD};
	const nveu32_t MTL_EST_OVERHEAD_RECOMMEND[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_OVERHEAD_RECOMMEND,
							MGBE_MTL_EST_OVERHEAD_RECOMMEND};
	const nveu32_t MAC_RQC1R[MAX_MAC_IP_TYPES] = {EQOS_MAC_RQC1R,
							MGBE_MAC_RQC1R};
	const nveu32_t MAC_RQC1R_RQ[MAX_MAC_IP_TYPES] = {EQOS_MAC_RQC1R_FPRQ,
							MGBE_MAC_RQC1R_RQ};
	const nveu32_t MAC_RQC1R_RQ_SHIFT[MAX_MAC_IP_TYPES] = {EQOS_MAC_RQC1R_FPRQ_SHIFT,
							MGBE_MAC_RQC1R_RQ_SHIFT};

	if (est_sel == OSI_ENABLE) {
		save_gcl_params(osi_core);
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MTL_EST_CONTROL[osi_core->mac]);

		/*
		 * PTOV PTP clock period * 6
		 * dual-port RAM based asynchronous FIFO controllers or
		 * Single-port RAM based synchronous FIFO controllers
		 * CTOV 96 x Tx clock period
		 * :
		 * :
		 * set other default value
		 */
		val &= ~MTL_EST_CONTROL_PTOV[osi_core->mac];
		temp = MTL_EST_PTOV_RECOMMEND[osi_core->mac];
		temp = temp << MTL_EST_CONTROL_PTOV_SHIFT[osi_core->mac];
		val |= temp;

		val &= ~MTL_EST_CONTROL_CTOV[osi_core->mac];
		temp = MTL_EST_CTOV_RECOMMEND[osi_core->mac];
		temp = temp << MTL_EST_CONTROL_CTOV_SHIFT[osi_core->mac];
		val |= temp;

		/*Loop Count to report Scheduling Error*/
		val &= ~MTL_EST_CONTROL_LCSE[osi_core->mac];
		val |= MTL_EST_CONTROL_LCSE_VAL[osi_core->mac];

		if (osi_core->mac == OSI_MAC_HW_EQOS) {
			val &= ~EQOS_MTL_EST_CONTROL_DFBS;
		}
		val &= ~MTL_EST_CONTROL_DDBF[osi_core->mac];
		val |= MTL_EST_CONTROL_DDBF[osi_core->mac];
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MTL_EST_CONTROL[osi_core->mac]);

		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				MTL_EST_OVERHEAD[osi_core->mac]);
		val &= ~MTL_EST_OVERHEAD_OVHD[osi_core->mac];
		/* As per hardware programming info */
		val |= MTL_EST_OVERHEAD_RECOMMEND[osi_core->mac];
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MTL_EST_OVERHEAD[osi_core->mac]);

		enable_mtl_interrupts(osi_core);
	}

	if (fpe_sel == OSI_ENABLE) {
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MAC_RQC1R[osi_core->mac]);
		val &= ~MAC_RQC1R_RQ[osi_core->mac];
		temp = osi_core->residual_queue;
		temp = temp << MAC_RQC1R_RQ_SHIFT[osi_core->mac];
		temp = (temp & MAC_RQC1R_RQ[osi_core->mac]);
		val |= temp;
		osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
			    MAC_RQC1R[osi_core->mac]);

		if (osi_core->mac == OSI_MAC_HW_MGBE) {
			val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
					 MGBE_MAC_RQC4R);
			val &= ~MGBE_MAC_RQC4R_PMCBCQ;
			temp = osi_core->residual_queue;
			temp = temp << MGBE_MAC_RQC4R_PMCBCQ_SHIFT;
			temp = (temp & MGBE_MAC_RQC4R_PMCBCQ);
			val |= temp;
			osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
				    MGBE_MAC_RQC4R);
		}

		enable_fpe_interrupts(osi_core);
	}

	/* CBS setting for TC or TXQ for default configuration
	   user application should use IOCTL to set CBS as per requirement
	 */
}

#ifdef HSI_SUPPORT
/**
 * @brief hsi_common_error_inject
 *
 * Algorithm:
 * - For macsec HSI: trigger interrupt using MACSEC_*_INTERRUPT_SET_0 register
 * - For mmc counter based: trigger interrupt by incrementing count by threshold value
 * - For rest: Directly set the error detected as there is no other mean to induce error
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] error_code: Ethernet HSI error code
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t hsi_common_error_inject(struct osi_core_priv_data *osi_core,
				nveu32_t error_code)
{
	nve32_t ret = 0;

	switch (error_code) {
	case OSI_INBOUND_BUS_CRC_ERR:
		osi_core->hsi.inject_crc_err_count =
			osi_update_stats_counter(osi_core->hsi.inject_crc_err_count,
						 osi_core->hsi.err_count_threshold);
		break;
	case OSI_RECEIVE_CHECKSUM_ERR:
		osi_core->hsi.inject_udp_err_count =
			osi_update_stats_counter(osi_core->hsi.inject_udp_err_count,
						 osi_core->hsi.err_count_threshold);
		break;
	case OSI_MACSEC_RX_CRC_ERR:
		osi_writela(osi_core, MACSEC_RX_MAC_CRC_ERROR,
			    (nveu8_t *)osi_core->macsec_base +
			    MACSEC_RX_ISR_SET);
		break;
	case OSI_MACSEC_TX_CRC_ERR:
		osi_writela(osi_core, MACSEC_TX_MAC_CRC_ERROR,
			    (nveu8_t *)osi_core->macsec_base +
			    MACSEC_TX_ISR_SET);
		break;
	case OSI_MACSEC_RX_ICV_ERR:
		osi_writela(osi_core, MACSEC_RX_ICV_ERROR,
			    (nveu8_t *)osi_core->macsec_base +
			    MACSEC_RX_ISR_SET);
		break;
	case OSI_MACSEC_REG_VIOL_ERR:
		osi_writela(osi_core, MACSEC_SECURE_REG_VIOL,
			    (nveu8_t *)osi_core->macsec_base +
			    MACSEC_COMMON_ISR_SET);
		break;
	case OSI_TX_FRAME_ERR:
		osi_core->hsi.report_count_err[TX_FRAME_ERR_IDX] = OSI_ENABLE;
		osi_core->hsi.err_code[TX_FRAME_ERR_IDX] = OSI_TX_FRAME_ERR;
		osi_core->hsi.report_err = OSI_ENABLE;
		break;
	case OSI_PCS_AUTONEG_ERR:
		osi_core->hsi.err_code[AUTONEG_ERR_IDX] = OSI_PCS_AUTONEG_ERR;
		osi_core->hsi.report_err = OSI_ENABLE;
		osi_core->hsi.report_count_err[AUTONEG_ERR_IDX] = OSI_ENABLE;
		break;
	case OSI_XPCS_WRITE_FAIL_ERR:
		osi_core->hsi.err_code[XPCS_WRITE_FAIL_IDX] = OSI_XPCS_WRITE_FAIL_ERR;
		osi_core->hsi.report_err = OSI_ENABLE;
		osi_core->hsi.report_count_err[XPCS_WRITE_FAIL_IDX] = OSI_ENABLE;
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Invalid error code\n", (nveu32_t)error_code);
		ret = -1;
		break;
	}

	return ret;
}
#endif

/**
 * @brief prepare_l3l4_ctr_reg - Prepare control register for L3L4 filters.
 *
 * @note
 * Algorithm:
 * - This sequence is used to prepare L3L4 control register for SA and DA Port Number matching.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (#osi_l3_l4_filter)
 * @param[out] ctr_reg: Pointer to L3L4 CTR register value
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *
 * @retval L3L4 CTR register value
 */
static void prepare_l3l4_ctr_reg(const struct osi_core_priv_data *const osi_core,
				 const struct osi_l3_l4_filter *const l3_l4,
				 nveu32_t *ctr_reg)
{
#ifndef OSI_STRIPPED_LIB
	nveu32_t dma_routing_enable = l3_l4->dma_routing_enable;
	nveu32_t dst_addr_match = l3_l4->data.dst.addr_match;
#else
	nveu32_t dma_routing_enable = OSI_TRUE;
	nveu32_t dst_addr_match = OSI_TRUE;
#endif /* !OSI_STRIPPED_LIB */
	const nveu32_t dma_chan_en_shift[2] = {
		EQOS_MAC_L3L4_CTR_DMCHEN_SHIFT,
		MGBE_MAC_L3L4_CTR_DMCHEN_SHIFT
	};
	nveu32_t value = 0U;

	/* set routing dma channel */
	value |= dma_routing_enable << (dma_chan_en_shift[osi_core->mac] & 0x1FU);
	value |= l3_l4->dma_chan << MAC_L3L4_CTR_DMCHN_SHIFT;

	/* Enable L3 filters for IPv4 DESTINATION addr matching */
	value |= dst_addr_match << MAC_L3L4_CTR_L3DAM_SHIFT;

#ifndef OSI_STRIPPED_LIB
	/* Enable L3 filters for IPv4 DESTINATION addr INV matching */
	value |= l3_l4->data.dst.addr_match_inv << MAC_L3L4_CTR_L3DAIM_SHIFT;

	/* Enable L3 filters for IPv4 SOURCE addr matching */
	value |= (l3_l4->data.src.addr_match << MAC_L3L4_CTR_L3SAM_SHIFT) |
		 (l3_l4->data.src.addr_match_inv << MAC_L3L4_CTR_L3SAIM_SHIFT);

	/* Enable L4 filters for DESTINATION port No matching */
	value |= (l3_l4->data.dst.port_match << MAC_L3L4_CTR_L4DPM_SHIFT) |
		 (l3_l4->data.dst.port_match_inv << MAC_L3L4_CTR_L4DPIM_SHIFT);

	/* Enable L4 filters for SOURCE Port No matching */
	value |= (l3_l4->data.src.port_match << MAC_L3L4_CTR_L4SPM_SHIFT) |
		 (l3_l4->data.src.port_match_inv << MAC_L3L4_CTR_L4SPIM_SHIFT);

	/* set udp / tcp port matching bit (for l4) */
	value |= l3_l4->data.is_udp << MAC_L3L4_CTR_L4PEN_SHIFT;

	/* set ipv4 / ipv6 protocol matching bit (for l3) */
	value |= l3_l4->data.is_ipv6 << MAC_L3L4_CTR_L3PEN_SHIFT;
#endif /* !OSI_STRIPPED_LIB */

	*ctr_reg = value;
}

/**
 * @brief prepare_l3_addr_registers - prepare register data for IPv4/IPv6 address filtering
 *
 * @note
 * Algorithm:
 *  - Update IPv4/IPv6 source/destination address for L3 layer filtering.
 *  - For IPv4, both source/destination address can be configured but
 *    for IPv6, only one of the source/destination address can be configured.
 *
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (#osi_l3_l4_filter)
 * @param[out] l3_addr1_reg: Pointer to L3 ADDR1 register value
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 */
static void prepare_l3_addr_registers(const struct osi_l3_l4_filter *const l3_l4,
#ifndef OSI_STRIPPED_LIB
				      nveu32_t *l3_addr0_reg,
				      nveu32_t *l3_addr2_reg,
				      nveu32_t *l3_addr3_reg,
#endif /* !OSI_STRIPPED_LIB */
				      nveu32_t *l3_addr1_reg)
{
#ifndef OSI_STRIPPED_LIB
	if (l3_l4->data.is_ipv6 == OSI_TRUE) {
		const nveu16_t *addr;
		/* For IPv6, either source address or destination
		 * address only one of them can be enabled
		 */
		if (l3_l4->data.src.addr_match == OSI_TRUE) {
			/* select src address only */
			addr = l3_l4->data.src.ip6_addr;
		} else {
			/* select dst address only */
			addr = l3_l4->data.dst.ip6_addr;
		}
		/* update Bits[31:0] of 128-bit IP addr */
		*l3_addr0_reg = addr[7] | ((nveu32_t)addr[6] << 16);

		/* update Bits[63:32] of 128-bit IP addr */
		*l3_addr1_reg = addr[5] | ((nveu32_t)addr[4] << 16);

		/* update Bits[95:64] of 128-bit IP addr */
		*l3_addr2_reg = addr[3] | ((nveu32_t)addr[2] << 16);

		/* update Bits[127:96] of 128-bit IP addr */
		*l3_addr3_reg = addr[1] | ((nveu32_t)addr[0] << 16);
	} else {
#endif /* !OSI_STRIPPED_LIB */
		const nveu8_t *addr;
		nveu32_t value;

#ifndef OSI_STRIPPED_LIB
		/* set source address */
		addr = l3_l4->data.src.ip4_addr;
		value = addr[3];
		value |= (nveu32_t)addr[2] << 8;
		value |= (nveu32_t)addr[1] << 16;
		value |= (nveu32_t)addr[0] << 24;
		*l3_addr0_reg = value;
#endif /* !OSI_STRIPPED_LIB */

		/* set destination address */
		addr = l3_l4->data.dst.ip4_addr;
		value = addr[3];
		value |= (nveu32_t)addr[2] << 8;
		value |= (nveu32_t)addr[1] << 16;
		value |= (nveu32_t)addr[0] << 24;
		*l3_addr1_reg = value;
#ifndef OSI_STRIPPED_LIB
	}
#endif /* !OSI_STRIPPED_LIB */
}

#ifndef OSI_STRIPPED_LIB
/**
 * @brief prepare_l4_port_register - program source and destination port number
 *
 * @note
 * Algorithm:
 *  - Program l4 address register with source and destination port numbers.
 *
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (#osi_l3_l4_filter)
 * @param[out] l4_addr_reg: Pointer to L3 ADDR0 register value
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 3) DCS bits should be enabled in RXQ to DMA mapping register
 */
static void prepare_l4_port_register(const struct osi_l3_l4_filter *const l3_l4,
				     nveu32_t *l4_addr_reg)
{
	nveu32_t value = 0U;

	/* set source port */
	value |= ((nveu32_t)l3_l4->data.src.port_no
		   & MGBE_MAC_L4_ADDR_SP_MASK);

	/* set destination port */
	value |= (((nveu32_t)l3_l4->data.dst.port_no <<
		    MGBE_MAC_L4_ADDR_DP_SHIFT) & MGBE_MAC_L4_ADDR_DP_MASK);

	*l4_addr_reg = value;
}
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief prepare_l3l4_registers - function to prepare l3l4 registers
 *
 * @note
 * Algorithm:
 *  - If filter to be enabled,
 *        - Prepare l3 ip address registers using prepare_l3_addr_registers().
 *        - Prepare l4 port register using prepare_l4_port_register().
 *        - Prepare l3l4 control register using prepare_l3l4_ctr_reg().
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] l3_l4: Pointer to l3 l4 filter structure (#osi_l3_l4_filter)
 * @param[out] l3_addr1_reg: Pointer to L3 ADDR1 register value
 * @param[out] ctr_reg: Pointer to L3L4 CTR register value
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *	 2) osi_core->osd should be populated
 *	 3) DCS bits should be enabled in RXQ to DMA mapping register
 */
void prepare_l3l4_registers(const struct osi_core_priv_data *const osi_core,
			    const struct osi_l3_l4_filter *const l3_l4,
#ifndef OSI_STRIPPED_LIB
			    nveu32_t *l3_addr0_reg,
			    nveu32_t *l3_addr2_reg,
			    nveu32_t *l3_addr3_reg,
			    nveu32_t *l4_addr_reg,
#endif /* !OSI_STRIPPED_LIB */
			    nveu32_t *l3_addr1_reg,
			    nveu32_t *ctr_reg)
{
	/* prepare regiser data if filter to be enabled */
	if (l3_l4->filter_enb_dis == OSI_TRUE) {
		/* prepare l3 filter ip address register data */
		prepare_l3_addr_registers(l3_l4,
#ifndef OSI_STRIPPED_LIB
				   l3_addr0_reg,
				   l3_addr2_reg,
				   l3_addr3_reg,
#endif /* !OSI_STRIPPED_LIB */
				   l3_addr1_reg);

#ifndef OSI_STRIPPED_LIB
		/* prepare l4 filter port register data */
		prepare_l4_port_register(l3_l4, l4_addr_reg);
#endif /* !OSI_STRIPPED_LIB */

		/* prepare control register data */
		prepare_l3l4_ctr_reg(osi_core, l3_l4, ctr_reg);
	}
}

/**
 * @brief hw_validate_avb_input- validate input arguments
 *
 * Algorithm:
 *	1) Check if idle slope is valid
 *	2) Check if send slope is valid
 *	3) Check if hi credit is valid
 *	4) Check if low credit is valid
 *
 * @param[in] osi_core: osi core priv data structure
 * @param[in] avb: structure having configuration for avb algorithm
 *
 * @note 1) MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t hw_validate_avb_input(struct osi_core_priv_data *const osi_core,
			      const struct osi_core_avb_algorithm *const avb)
{
	nve32_t ret = 0;
	nveu32_t ETS_QW_ISCQW_MASK[MAX_MAC_IP_TYPES] = {EQOS_MTL_TXQ_ETS_QW_ISCQW_MASK,
							MGBE_MTL_TCQ_ETS_QW_ISCQW_MASK};
	nveu32_t ETS_SSCR_SSC_MASK[MAX_MAC_IP_TYPES] = {EQOS_MTL_TXQ_ETS_SSCR_SSC_MASK,
							MGBE_MTL_TCQ_ETS_SSCR_SSC_MASK};
	nveu32_t ETS_HC_BOUND = 0x8000000U;
	nveu32_t ETS_LC_BOUND = 0xF8000000U;
	nveu32_t mac = osi_core->mac;

	if (avb->idle_slope > ETS_QW_ISCQW_MASK[mac]) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid idle_slope\n",
			     (nveul64_t)avb->idle_slope);
		ret = -1;
		goto fail;
	}
	if (avb->send_slope > ETS_SSCR_SSC_MASK[mac]) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid send_slope\n",
			     (nveul64_t)avb->send_slope);
		ret = -1;
		goto fail;
	}
	if (avb->hi_credit > ETS_HC_BOUND) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid hi credit\n",
			     (nveul64_t)avb->hi_credit);
		ret = -1;
		goto fail;
	}
	if ((avb->low_credit < ETS_LC_BOUND) &&
	    (avb->low_credit != 0U)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "Invalid low credit\n",
			     (nveul64_t)avb->low_credit);
		ret = -1;
		goto fail;
	}
fail:
	return ret;
}
