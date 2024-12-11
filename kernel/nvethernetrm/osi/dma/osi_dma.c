/*
 * Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
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

#include "dma_local.h"
#include <local_common.h>
#include "hw_desc.h"
#include "../osi/common/common.h"
#ifdef OSI_DEBUG
#include "debug.h"
#endif /* OSI_DEBUG */
#include "hw_common.h"

/**
 * @brief g_dma - DMA local data array.
 */

/**
 * @brief g_ops - local DMA HW operations array.
 */

typedef nve32_t (*dma_intr_fn)(struct osi_dma_priv_data const *osi_dma,
			       nveu32_t intr_ctrl, nveu32_t intr_status,
			       nveu32_t dma_status, nveu32_t val);
static inline nve32_t enable_intr(struct osi_dma_priv_data const *osi_dma,
				  nveu32_t intr_ctrl, nveu32_t intr_status,
				  nveu32_t dma_status, nveu32_t val);
static inline nve32_t disable_intr(struct osi_dma_priv_data const *osi_dma,
				  nveu32_t intr_ctrl, nveu32_t intr_status,
				  nveu32_t dma_status, nveu32_t val);
static dma_intr_fn intr_fn[2] = { disable_intr, enable_intr };

static inline nveu32_t set_pos_val(nveu32_t val, nveu32_t pos_val)
{
	return (val | pos_val);
}

static inline nveu32_t clear_pos_val(nveu32_t val, nveu32_t pos_val)
{
	return (val & ~pos_val);
}

static inline nve32_t intr_en_dis_retry(nveu8_t *base, nveu32_t intr_ctrl,
					nveu32_t val, nveu32_t en_dis)
{
	typedef nveu32_t (*set_clear)(nveu32_t val, nveu32_t pos);
	const set_clear set_clr[2] = { clear_pos_val, set_pos_val };
	nveu32_t cntrl1, cntrl2, i;
	nve32_t ret = -1;

	for (i = 0U; i < 10U; i++) {
		cntrl1 = osi_readl(base + intr_ctrl);
		cntrl1 = set_clr[en_dis](cntrl1, val);
		osi_writel(cntrl1, base + intr_ctrl);

		cntrl2 = osi_readl(base + intr_ctrl);
		if (cntrl1 == cntrl2) {
			ret = 0;
			break;
		} else {
			continue;
		}
	}

	return ret;
}

static inline nve32_t enable_intr(struct osi_dma_priv_data const *osi_dma,
				  nveu32_t intr_ctrl, OSI_UNUSED nveu32_t intr_status,
				  OSI_UNUSED nveu32_t dma_status, nveu32_t val)
{
	return intr_en_dis_retry((nveu8_t *)osi_dma->base, intr_ctrl,
				 val, OSI_DMA_INTR_ENABLE);
}

static inline nve32_t disable_intr(struct osi_dma_priv_data const *osi_dma,
				  nveu32_t intr_ctrl, nveu32_t intr_status,
				  nveu32_t dma_status, nveu32_t val)
{
	nveu8_t *base = (nveu8_t *)osi_dma->base;
	const nveu32_t status_val[4] = {
		0,
		EQOS_DMA_CHX_STATUS_CLEAR_TX,
		EQOS_DMA_CHX_STATUS_CLEAR_RX,
		0,
	};
	nveu32_t status;

	status = osi_readl(base + intr_status);
	if ((status & val) == val) {
		osi_writel(status_val[val], base + dma_status);
		osi_writel(val, base + intr_status);
	}

	return intr_en_dis_retry((nveu8_t *)osi_dma->base, intr_ctrl,
				 val, OSI_DMA_INTR_DISABLE);
}

struct osi_dma_priv_data *osi_get_dma(void)
{
	static struct dma_local g_dma[MAX_DMA_INSTANCES];
	struct osi_dma_priv_data *osi_dma = OSI_NULL;
	nveu32_t i;

	for (i = 0U; i < MAX_DMA_INSTANCES; i++) {
		if (g_dma[i].init_done == OSI_ENABLE) {
			continue;
		}

		break;
	}

	if (i == MAX_DMA_INSTANCES) {
		goto fail;
	}

	g_dma[i].magic_num = (nveu64_t)&g_dma[i].osi_dma;

	osi_dma = &g_dma[i].osi_dma;
fail:
	return osi_dma;
}

/**
 * @brief Function to validate input arguments of API.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] l_dma: Local OSI DMA data structure.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t dma_validate_args(const struct osi_dma_priv_data *const osi_dma,
					const struct dma_local *const l_dma)
{
	nve32_t ret = 0;

	if ((osi_dma == OSI_NULL) || (osi_dma->base == OSI_NULL) ||
	    (l_dma->init_done == OSI_DISABLE)) {
		ret = -1;
	}

	return ret;
}

/**
 * @brief Function to validate input arguments of API.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] chan: DMA channel number.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t validate_dma_chan_num(struct osi_dma_priv_data *osi_dma,
					    nveu32_t chan)
{
	const struct dma_local *const l_dma = (struct dma_local *)(void *)osi_dma;
	nve32_t ret = 0;

	if (chan >= l_dma->num_max_chans) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
				"Invalid DMA channel number\n", chan);
		ret = -1;
	}

	return ret;
}

/**
 * @brief Function to validate array of DMA channels.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t validate_dma_chans(struct osi_dma_priv_data *osi_dma)
{
	const struct dma_local *const l_dma = (struct dma_local *)(void *)osi_dma;
	nveu32_t i = 0U;
	nve32_t ret = 0;

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		if (osi_dma->dma_chans[i] > l_dma->num_max_chans) {
			OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
				    "Invalid DMA channel number:\n",
				    osi_dma->dma_chans[i]);
			ret = -1;
		}
	}

	return ret;
}

#ifndef OSI_STRIPPED_LIB
/**
 * @brief Function to validate function pointers.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] ops_p: Pointer to OSI DMA channel operations.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t validate_func_ptrs(struct osi_dma_priv_data *osi_dma,
				  struct dma_chan_ops *ops_p)
{
	nveu32_t i = 0;
	void *temp_ops = (void *)ops_p;
#if __SIZEOF_POINTER__ == 8
	nveu64_t *l_ops = (nveu64_t *)temp_ops;
#elif __SIZEOF_POINTER__ == 4
	nveu32_t *l_ops = (nveu32_t *)temp_ops;
#else
	OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
		    "DMA: Undefined architecture\n", 0ULL);
	return -1;
#endif
	(void) osi_dma;

	for (i = 0; i < (sizeof(*ops_p) / (nveu64_t)__SIZEOF_POINTER__); i++) {
		if (*l_ops == 0U) {
			OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
				    "dma: fn ptr validation failed at\n",
				    (nveu64_t)i);
			return -1;
		}

		l_ops++;
	}

	return 0;
}
#endif

nve32_t osi_init_dma_ops(struct osi_dma_priv_data *osi_dma)
{
	const nveu32_t default_rz[] = { EQOS_DEFAULT_RING_SZ, MGBE_DEFAULT_RING_SZ };
	const nveu32_t max_rz[] = { EQOS_DEFAULT_RING_SZ, MGBE_MAX_RING_SZ };
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	static struct dma_chan_ops dma_gops[MAX_MAC_IP_TYPES];
#ifndef OSI_STRIPPED_LIB
	typedef void (*init_ops_arr)(struct dma_chan_ops *temp);
	const init_ops_arr i_ops[MAX_MAC_IP_TYPES] = {
		eqos_init_dma_chan_ops, mgbe_init_dma_chan_ops
	};
#endif
	nve32_t ret = 0;

	if (osi_dma == OSI_NULL) {
		ret = -1;
		goto fail;
	}

	if ((l_dma->magic_num != (nveu64_t)osi_dma) ||
	    (l_dma->init_done == OSI_ENABLE)) {
		ret = -1;
		goto fail;
	}

	if (osi_dma->is_ethernet_server != OSI_ENABLE) {
		if ((osi_dma->osd_ops.transmit_complete == OSI_NULL) ||
		    (osi_dma->osd_ops.receive_packet == OSI_NULL) ||
		    (osi_dma->osd_ops.ops_log == OSI_NULL) ||
#ifdef OSI_DEBUG
		    (osi_dma->osd_ops.printf == OSI_NULL) ||
#endif /* OSI_DEBUG */
		    (osi_dma->osd_ops.udelay == OSI_NULL)) {
			ret = -1;
			goto fail;
		}
	}

	if (osi_dma->mac > OSI_MAC_HW_MGBE) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "DMA: Invalid MAC HW type\n", 0ULL);
		ret = -1;
		goto fail;
	}

	if ((osi_dma->tx_ring_sz == 0U) ||
	    (is_power_of_two(osi_dma->tx_ring_sz) == 0U) ||
	    (osi_dma->tx_ring_sz < HW_MIN_RING_SZ) ||
	    (osi_dma->tx_ring_sz > default_rz[osi_dma->mac])) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "DMA: Invalid Tx ring size:\n",
			     osi_dma->tx_ring_sz);
		ret = -1;
		goto fail;
	}

	if ((osi_dma->rx_ring_sz == 0U) ||
	    (is_power_of_two(osi_dma->rx_ring_sz) == 0U) ||
	    (osi_dma->rx_ring_sz < HW_MIN_RING_SZ) ||
	    (osi_dma->rx_ring_sz > max_rz[osi_dma->mac])) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "DMA: Invalid Rx ring size:\n",
			     osi_dma->tx_ring_sz);
		ret = -1;
		goto fail;
	}
#ifndef OSI_STRIPPED_LIB
	i_ops[osi_dma->mac](&dma_gops[osi_dma->mac]);
#endif
	if (init_desc_ops(osi_dma) < 0) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "DMA desc ops init failed\n", 0ULL);
		ret = -1;
		goto fail;
	}

#ifndef OSI_STRIPPED_LIB
	if (validate_func_ptrs(osi_dma, &dma_gops[osi_dma->mac]) < 0) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "DMA ops validation failed\n", 0ULL);
		ret = -1;
		goto fail;
	}
#endif

	l_dma->ops_p = &dma_gops[osi_dma->mac];
	l_dma->init_done = OSI_ENABLE;

fail:
	return ret;
}

static inline void start_dma(const struct osi_dma_priv_data *const osi_dma, nveu32_t dma_chan)
{
	nveu32_t chan = dma_chan & 0xFU;
	const nveu32_t tx_dma_reg[2] = {
		EQOS_DMA_CHX_TX_CTRL(chan),
		MGBE_DMA_CHX_TX_CTRL(chan)
	};
	const nveu32_t rx_dma_reg[2] = {
		EQOS_DMA_CHX_RX_CTRL(chan),
		MGBE_DMA_CHX_RX_CTRL(chan)
	};
	nveu32_t val;

	/* Start Tx DMA */
	val = osi_readl((nveu8_t *)osi_dma->base + tx_dma_reg[osi_dma->mac]);
	val |= OSI_BIT(0);
	osi_writel(val, (nveu8_t *)osi_dma->base + tx_dma_reg[osi_dma->mac]);

	/* Start Rx DMA */
	val = osi_readl((nveu8_t *)osi_dma->base + rx_dma_reg[osi_dma->mac]);
	val |= OSI_BIT(0);
	val &= ~OSI_BIT(31);
	osi_writel(val, (nveu8_t *)osi_dma->base + rx_dma_reg[osi_dma->mac]);
}

static inline nveu32_t calculate_tx_pbl(nveu32_t tx_fifo_perq, nveu32_t mtu)
{
	nveu32_t subtraction_result = 0U;
	nveu32_t tx_pbl = 0U;

	/*
	 * Formula for TxPBL calculation is
	 * (TxPBL) < ((TXQSize - MTU)/(DATAWIDTH/8)) - 5
	 * if TxPBL exceeds the value of 256 then we need to make use of 256
	 * as the TxPBL else we should be using the value which we get after
	 * calculation by using above formula
	 */
	/* tx_pbl = ((((Total Q size / total enabled queues) - osi_dma->mtu) /
	 *             (MGBE_AXI_DATAWIDTH / 8U)) - 5U)
	 */
	if (tx_fifo_perq >= mtu) {
		subtraction_result = tx_fifo_perq - mtu;

		if (subtraction_result >= (5U * 16U))
			tx_pbl = (subtraction_result / 16U) - 5U;
	}

	return tx_pbl;
}

static nve32_t init_dma_channel(const struct osi_dma_priv_data *const osi_dma,
				nveu32_t dma_chan)
{
	nveu32_t chan = dma_chan & 0xFU;
	nveu32_t riwt = osi_dma->rx_riwt & 0xFFFU;
	const nveu32_t intr_en_reg[2] = {
		EQOS_DMA_CHX_INTR_ENA(chan),
		MGBE_DMA_CHX_INTR_ENA(chan)
	};
	const nveu32_t chx_ctrl_reg[2] = {
		EQOS_DMA_CHX_CTRL(chan),
		MGBE_DMA_CHX_CTRL(chan)
	};
	const nveu32_t tx_ctrl_reg[2] = {
		EQOS_DMA_CHX_TX_CTRL(chan),
		MGBE_DMA_CHX_TX_CTRL(chan)
	};
	const nveu32_t rx_ctrl_reg[2] = {
		EQOS_DMA_CHX_RX_CTRL(chan),
		MGBE_DMA_CHX_RX_CTRL(chan)
	};
	const nveu32_t rx_wdt_reg[2] = {
		EQOS_DMA_CHX_RX_WDT(chan),
		MGBE_DMA_CHX_RX_WDT(chan)
	};
	const nveu32_t rx_pbl[2] = {
		EQOS_DMA_CHX_RX_CTRL_RXPBL_RECOMMENDED,
		((MGBE_RXQ_SIZE / osi_dma->num_dma_chans) / 2U)
	};
	const nveu32_t rwt_val[2] = {
		(((riwt * (EQOS_AXI_CLK_FREQ / OSI_ONE_MEGA_HZ)) /
		  EQOS_DMA_CHX_RX_WDT_RWTU) & EQOS_DMA_CHX_RX_WDT_RWT_MASK),
		(((riwt * ((nveu32_t)MGBE_AXI_CLK_FREQ / OSI_ONE_MEGA_HZ)) /
		 MGBE_DMA_CHX_RX_WDT_RWTU) & MGBE_DMA_CHX_RX_WDT_RWT_MASK)
	};
	const nveu32_t rwtu_val[2] = {
		EQOS_DMA_CHX_RX_WDT_RWTU_512_CYCLE,
		MGBE_DMA_CHX_RX_WDT_RWTU_2048_CYCLE
	};
	const nveu32_t rwtu_mask[2] = {
		EQOS_DMA_CHX_RX_WDT_RWTU_MASK,
		MGBE_DMA_CHX_RX_WDT_RWTU_MASK
	};
	const nveu32_t owrq = (MGBE_DMA_CHX_RX_CNTRL2_OWRQ_MCHAN / osi_dma->num_dma_chans);
	const nveu32_t owrq_arr[OSI_MGBE_MAX_NUM_CHANS] = {
		MGBE_DMA_CHX_RX_CNTRL2_OWRQ_SCHAN, owrq, owrq, owrq,
		owrq, owrq, owrq, owrq, owrq, owrq
	};
	nveu32_t val, tx_fifo_perq, tx_pbl, result = 0U;
	nve32_t ret = 0;

	/* Enable Transmit/Receive interrupts */
	val = osi_readl((nveu8_t *)osi_dma->base + intr_en_reg[osi_dma->mac]);
	val |= (DMA_CHX_INTR_TIE | DMA_CHX_INTR_RIE);
	osi_writel(val, (nveu8_t *)osi_dma->base + intr_en_reg[osi_dma->mac]);

	/* Enable PBLx8 */
	val = osi_readl((nveu8_t *)osi_dma->base + chx_ctrl_reg[osi_dma->mac]);
	val |= DMA_CHX_CTRL_PBLX8;
	osi_writel(val, (nveu8_t *)osi_dma->base + chx_ctrl_reg[osi_dma->mac]);

	/* Program OSP, TSO enable and TXPBL */
	val = osi_readl((nveu8_t *)osi_dma->base + tx_ctrl_reg[osi_dma->mac]);
	val |= (DMA_CHX_TX_CTRL_OSP | DMA_CHX_TX_CTRL_TSE);

	/* Getting Per TX Q memory */
	if (osi_dma->mac == OSI_MAC_HW_EQOS) {
		val |= EQOS_DMA_CHX_TX_CTRL_TXPBL_RECOMMENDED;
	} else {
		/* Need to get per Q memeory assigned inside OSI core */
		tx_fifo_perq = osi_readl((nveu8_t *)osi_dma->base +
					 MGBE_MTL_CHX_TX_OP_MODE(chan));
		/* Mask the bits to get the per queue memory from the register */
		tx_fifo_perq = (tx_fifo_perq >> 16U) & 0x1FFU;
		/* Need to multiply by 256 to get the actual memory size */
		tx_fifo_perq = (tx_fifo_perq + 1U) * 256U;

		if (tx_fifo_perq >= 4176U)
			result = tx_fifo_perq - 4176U;

		if (osi_dma->mtu > result) {
			OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
				    "Invalid MTU, max allowed MTU should be less than:\n", result);
			ret = -1;
			goto err;
		}

		tx_pbl = calculate_tx_pbl(tx_fifo_perq, osi_dma->mtu);

		if (tx_pbl >= MGBE_DMA_CHX_MAX_PBL) {
			/* setting maximum value of 32 which is 32 * 8
			 * (because TxPBLx8 = 1) => 256 bytes
			 */
			val |= MGBE_DMA_CHX_MAX_PBL_VAL;
		} else {
			/* divide by 8 because TxPBLx8 = 1 */
			val |= ((tx_pbl / 8U) << MGBE_DMA_CHX_CTRL_PBL_SHIFT);
		}
	}

	osi_writel(val, (nveu8_t *)osi_dma->base + tx_ctrl_reg[osi_dma->mac]);

	val = osi_readl((nveu8_t *)osi_dma->base + rx_ctrl_reg[osi_dma->mac]);
	val &= ~DMA_CHX_RBSZ_MASK;
	/** Subtract 30 bytes again which were added for buffer address alignment
	 * HW don't need those extra 30 bytes. If data length received more than
	 * below programed value then it will result in two descriptors which
	 * eventually drop by OSI. Subtracting 30 bytes so that HW don't receive
	 * unwanted length data.
	 **/
	val |= ((osi_dma->rx_buf_len - 30U) << DMA_CHX_RBSZ_SHIFT);
	if (osi_dma->mac == OSI_MAC_HW_EQOS) {
		val |= rx_pbl[osi_dma->mac];
	} else {
		if (rx_pbl[osi_dma->mac] >= MGBE_DMA_CHX_MAX_PBL) {
			val |= MGBE_DMA_CHX_MAX_PBL_VAL;
		} else {
			val |= ((rx_pbl[osi_dma->mac] / 8U) << MGBE_DMA_CHX_CTRL_PBL_SHIFT);
		}
	}
	osi_writel(val, (nveu8_t *)osi_dma->base + rx_ctrl_reg[osi_dma->mac]);

	if ((osi_dma->use_riwt == OSI_ENABLE) &&
	    (osi_dma->rx_riwt < UINT_MAX)) {
		val = osi_readl((nveu8_t *)osi_dma->base + rx_wdt_reg[osi_dma->mac]);
		val &= ~DMA_CHX_RX_WDT_RWT_MASK;
		val |= rwt_val[osi_dma->mac];
		osi_writel(val, (nveu8_t *)osi_dma->base + rx_wdt_reg[osi_dma->mac]);

		val = osi_readl((nveu8_t *)osi_dma->base + rx_wdt_reg[osi_dma->mac]);
		val &= ~rwtu_mask[osi_dma->mac];
		val |= rwtu_val[osi_dma->mac];
		osi_writel(val, (nveu8_t *)osi_dma->base + rx_wdt_reg[osi_dma->mac]);
	}

	if (osi_dma->mac == OSI_MAC_HW_MGBE) {
		/* Update ORRQ in DMA_CH(#i)_Tx_Control2 register */
		val = osi_readl((nveu8_t *)osi_dma->base + MGBE_DMA_CHX_TX_CNTRL2(chan));
		val |= (((MGBE_DMA_CHX_TX_CNTRL2_ORRQ_RECOMMENDED / osi_dma->num_dma_chans)) <<
			MGBE_DMA_CHX_TX_CNTRL2_ORRQ_SHIFT);
		osi_writel(val, (nveu8_t *)osi_dma->base + MGBE_DMA_CHX_TX_CNTRL2(chan));

		/* Update OWRQ in DMA_CH(#i)_Rx_Control2 register */
		val = osi_readl((nveu8_t *)osi_dma->base + MGBE_DMA_CHX_RX_CNTRL2(chan));
		val |= (owrq_arr[osi_dma->num_dma_chans - 1U] << MGBE_DMA_CHX_RX_CNTRL2_OWRQ_SHIFT);
		osi_writel(val, (nveu8_t *)osi_dma->base + MGBE_DMA_CHX_RX_CNTRL2(chan));
	}

err:
	return ret;
}

nve32_t osi_hw_dma_init(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	nveu32_t i, chan;
	nve32_t ret = 0;

	if (dma_validate_args(osi_dma, l_dma) < 0) {
		ret = -1;
		goto fail;
	}

	l_dma->mac_ver = osi_readl((nveu8_t *)osi_dma->base + MAC_VERSION) &
				   MAC_VERSION_SNVER_MASK;
	if (validate_mac_ver_update_chans(l_dma->mac_ver,
					  &l_dma->num_max_chans,
					  &l_dma->l_mac_ver) == 0) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "Invalid MAC version\n", (nveu64_t)l_dma->mac_ver);
		ret = -1;
		goto fail;
	}

	if ((osi_dma->num_dma_chans == 0U) ||
	    (osi_dma->num_dma_chans > l_dma->num_max_chans)) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "Invalid number of DMA channels\n", 0ULL);
		ret = -1;
		goto fail;
	}

	if (validate_dma_chans(osi_dma) < 0) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "DMA channels validation failed\n", 0ULL);
		ret = -1;
		goto fail;
	}

	ret = dma_desc_init(osi_dma);
	if (ret != 0) {
		goto fail;
	}

	/* Enable channel interrupts at wrapper level and start DMA */
	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		chan = osi_dma->dma_chans[i];

		ret = init_dma_channel(osi_dma, chan);
		if (ret < 0) {
			goto fail;
		}

		ret = intr_fn[OSI_DMA_INTR_ENABLE](osi_dma,
				VIRT_INTR_CHX_CNTRL(chan),
				VIRT_INTR_CHX_STATUS(chan),
				((osi_dma->mac == OSI_MAC_HW_MGBE) ?
				MGBE_DMA_CHX_STATUS(chan) :
				EQOS_DMA_CHX_STATUS(chan)),
				OSI_BIT(OSI_DMA_CH_TX_INTR));
		if (ret < 0) {
			goto fail;
		}

		ret = intr_fn[OSI_DMA_INTR_ENABLE](osi_dma,
				VIRT_INTR_CHX_CNTRL(chan),
				VIRT_INTR_CHX_STATUS(chan),
				((osi_dma->mac == OSI_MAC_HW_MGBE) ?
				MGBE_DMA_CHX_STATUS(chan) :
				EQOS_DMA_CHX_STATUS(chan)),
				OSI_BIT(OSI_DMA_CH_RX_INTR));
		if (ret < 0) {
			goto fail;
		}

		start_dma(osi_dma, chan);
	}

	/**
	 * OSD will update this if PTP needs to be run in diffrent modes.
	 * Default configuration is PTP sync in two step sync with slave mode.
	 */
	if (osi_dma->ptp_flag == 0U) {
		osi_dma->ptp_flag = (OSI_PTP_SYNC_SLAVE | OSI_PTP_SYNC_TWOSTEP);
	}

fail:
	return ret;
}

static inline void stop_dma(const struct osi_dma_priv_data *const osi_dma,
			    nveu32_t dma_chan)
{
	nveu32_t chan = dma_chan & 0xFU;
	const nveu32_t dma_tx_reg[2] = {
		EQOS_DMA_CHX_TX_CTRL(chan),
		MGBE_DMA_CHX_TX_CTRL(chan)
	};
	const nveu32_t dma_rx_reg[2] = {
		EQOS_DMA_CHX_RX_CTRL(chan),
		MGBE_DMA_CHX_RX_CTRL(chan)
	};
	nveu32_t val;

	/* Stop Tx DMA */
	val = osi_readl((nveu8_t *)osi_dma->base + dma_tx_reg[osi_dma->mac]);
	val &= ~OSI_BIT(0);
	osi_writel(val, (nveu8_t *)osi_dma->base + dma_tx_reg[osi_dma->mac]);

	/* Stop Rx DMA */
	val = osi_readl((nveu8_t *)osi_dma->base + dma_rx_reg[osi_dma->mac]);
	val &= ~OSI_BIT(0);
	val |= OSI_BIT(31);
	osi_writel(val, (nveu8_t *)osi_dma->base + dma_rx_reg[osi_dma->mac]);
}

nve32_t osi_hw_dma_deinit(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	nve32_t ret = 0;
	nveu32_t i;

	if (dma_validate_args(osi_dma, l_dma) < 0) {
		ret = -1;
		goto fail;
	}

	if (osi_dma->num_dma_chans > l_dma->num_max_chans) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "Invalid number of DMA channels\n", 0ULL);
		ret = -1;
		goto fail;
	}

	if (validate_dma_chans(osi_dma) < 0) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "DMA channels validation failed\n", 0ULL);
		ret = -1;
		goto fail;
	}

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		stop_dma(osi_dma, osi_dma->dma_chans[i]);
	}

fail:
	return ret;
}

nveu32_t osi_get_global_dma_status(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	nveu32_t ret = 0U;

	if (dma_validate_args(osi_dma, l_dma) < 0) {
		goto fail;
	}

	ret = osi_readl((nveu8_t *)osi_dma->base + HW_GLOBAL_DMA_STATUS);
fail:
	return ret;
}

nve32_t osi_handle_dma_intr(struct osi_dma_priv_data *osi_dma,
			    nveu32_t chan,
			    nveu32_t tx_rx,
			    nveu32_t en_dis)
{
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	nve32_t ret = 0;

	if (dma_validate_args(osi_dma, l_dma) < 0) {
		ret = -1;
		goto fail;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		ret = -1;
		goto fail;
	}

	if ((tx_rx > OSI_DMA_CH_RX_INTR) ||
	    (en_dis > OSI_DMA_INTR_ENABLE)) {
		ret = -1;
		goto fail;
	}

	ret = intr_fn[en_dis](osi_dma, VIRT_INTR_CHX_CNTRL(chan),
		VIRT_INTR_CHX_STATUS(chan), ((osi_dma->mac == OSI_MAC_HW_MGBE) ?
		MGBE_DMA_CHX_STATUS(chan) : EQOS_DMA_CHX_STATUS(chan)),
		OSI_BIT(tx_rx));

fail:
	return ret;
}

nveu32_t osi_get_refill_rx_desc_cnt(const struct osi_dma_priv_data *const osi_dma,
				    nveu32_t chan)
{
	const struct osi_rx_ring *const rx_ring = osi_dma->rx_ring[chan];
	nveu32_t ret = 0U;

	if ((rx_ring == OSI_NULL) ||
	    (rx_ring->cur_rx_idx >= osi_dma->rx_ring_sz) ||
	    (rx_ring->refill_idx >= osi_dma->rx_ring_sz)) {
		goto fail;
	}

	ret = (rx_ring->cur_rx_idx - rx_ring->refill_idx) &
		(osi_dma->rx_ring_sz - 1U);
fail:
	return ret;
}

/**
 * @brief rx_dma_desc_dma_validate_args - DMA Rx descriptor init args Validate
 *
 * Algorithm: Validates DMA Rx descriptor init argments.
 *
 * @param[in] osi_dma: OSI DMA private data struture.
 * @param[in] rx_ring: HW ring corresponding to Rx DMA channel.
 * @param[in] chan: Rx DMA channel number
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t rx_dma_desc_dma_validate_args(
					    struct osi_dma_priv_data *osi_dma,
					    struct dma_local *l_dma,
					    const struct osi_rx_ring *const rx_ring,
					    nveu32_t chan)
{
	nve32_t ret = 0;

	if (dma_validate_args(osi_dma, l_dma) < 0) {
		ret = -1;
		goto fail;
	}

	if (!((rx_ring != OSI_NULL) && (rx_ring->rx_swcx != OSI_NULL) &&
	      (rx_ring->rx_desc != OSI_NULL))) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "dma: Invalid pointers\n", 0ULL);
		ret = -1;
		goto fail;
	}

	if (validate_dma_chan_num(osi_dma, chan) < 0) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "dma: Invalid channel\n", 0ULL);
		ret = -1;
		goto fail;
	}

fail:
	return ret;
}

/**
 * @brief rx_dma_handle_ioc - DMA Rx descriptor RWIT Handler
 *
 * Algorithm:
 * 1) Check RWIT enable and reset IOC bit
 * 2) Check rx_frames enable and update IOC bit
 *
 * @param[in] osi_dma: OSI DMA private data struture.
 * @param[in] rx_ring: HW ring corresponding to Rx DMA channel.
 * @param[in, out] rx_desc: Rx Rx descriptor.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 */
static inline void rx_dma_handle_ioc(const struct osi_dma_priv_data *const osi_dma,
				     const struct osi_rx_ring *const rx_ring,
				     struct osi_rx_desc *rx_desc)
{
	/* reset IOC bit if RWIT is enabled */
	if (osi_dma->use_riwt == OSI_ENABLE) {
		rx_desc->rdes3 &= ~RDES3_IOC;
		/* update IOC bit if rx_frames is enabled. Rx_frames
		 * can be enabled only along with RWIT.
		 */
		if (osi_dma->use_rx_frames == OSI_ENABLE) {
			if ((rx_ring->refill_idx %
			    osi_dma->rx_frames) == OSI_NONE) {
				rx_desc->rdes3 |= RDES3_IOC;
			}
		}
	}
}

nve32_t osi_rx_dma_desc_init(struct osi_dma_priv_data *osi_dma,
			     struct osi_rx_ring *rx_ring, nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	struct osi_rx_swcx *rx_swcx = OSI_NULL;
	struct osi_rx_desc *rx_desc = OSI_NULL;
	nveu64_t tailptr = 0;
	nve32_t ret = 0;

	if (rx_dma_desc_dma_validate_args(osi_dma, l_dma, rx_ring, chan) < 0) {
		/* Return on arguments validation failure */
		ret = -1;
		goto fail;
	}

	/* Refill buffers */
	while ((rx_ring->refill_idx != rx_ring->cur_rx_idx) &&
	       (rx_ring->refill_idx < osi_dma->rx_ring_sz)) {
		rx_swcx = rx_ring->rx_swcx + rx_ring->refill_idx;
		rx_desc = rx_ring->rx_desc + rx_ring->refill_idx;

		if ((rx_swcx->flags & OSI_RX_SWCX_BUF_VALID) !=
		    OSI_RX_SWCX_BUF_VALID) {
			break;
		}

		rx_swcx->flags = 0;

		/* Populate the newly allocated buffer address */
		rx_desc->rdes0 = L32(rx_swcx->buf_phy_addr);
		rx_desc->rdes1 = H32(rx_swcx->buf_phy_addr);

		rx_desc->rdes2 = 0;
		rx_desc->rdes3 = RDES3_IOC;

		if (osi_dma->mac == OSI_MAC_HW_EQOS) {
			rx_desc->rdes3 |= RDES3_B1V;
		}

		/* Reset IOC bit if RWIT is enabled */
		rx_dma_handle_ioc(osi_dma, rx_ring, rx_desc);
		rx_desc->rdes3 |= RDES3_OWN;

		INCR_RX_DESC_INDEX(rx_ring->refill_idx, osi_dma->rx_ring_sz);
	}

	/* Update the Rx tail ptr  whenever buffer is replenished to
	 * kick the Rx DMA to resume if it is in suspend. Always set
	 * Rx tailptr to 1 greater than last descriptor in the ring since HW
	 * knows to loop over to start of ring.
	 */
	tailptr = rx_ring->rx_desc_phy_addr +
		  (sizeof(struct osi_rx_desc) * (osi_dma->rx_ring_sz));

	if (osi_unlikely(tailptr < rx_ring->rx_desc_phy_addr)) {
		/* Will not hit this case, used for CERT-C compliance */
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "dma: Invalid tailptr\n", 0ULL);
		ret = -1;
		goto fail;
	}

	update_rx_tail_ptr(osi_dma, chan, tailptr);

fail:
	return ret;
}

nve32_t osi_set_rx_buf_len(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	nveu32_t rx_buf_len;
	nve32_t ret = 0;

	if (dma_validate_args(osi_dma, l_dma) < 0) {
		ret = -1;
		goto fail;
	}

	if (osi_dma->mtu > OSI_MAX_MTU_SIZE) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "Invalid MTU setting\n", 0ULL);
		ret = -1;
		goto fail;
	}

	/* Add Ethernet header + FCS */
	rx_buf_len = osi_dma->mtu + OSI_ETH_HLEN + NV_VLAN_HLEN;

	/* Add 30 bytes (15bytes extra at head portion for alignment and 15bytes
	 * extra to cover tail portion) again for the buffer address alignment
	 */
	rx_buf_len += 30U;

	/* Buffer alignment */
	osi_dma->rx_buf_len = ((rx_buf_len + (AXI_BUS_WIDTH - 1U)) &
			       ~(AXI_BUS_WIDTH - 1U));

fail:
	return ret;
}

nve32_t osi_dma_get_systime_from_mac(struct osi_dma_priv_data *const osi_dma,
				     nveu32_t *sec, nveu32_t *nsec)
{
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	nve32_t ret = 0;

	if (dma_validate_args(osi_dma, l_dma) < 0) {
		ret = -1;
	}

	common_get_systime_from_mac(osi_dma->base, osi_dma->mac, sec, nsec);

	return ret;
}

nveu32_t osi_is_mac_enabled(struct osi_dma_priv_data *const osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	nveu32_t ret = OSI_DISABLE;

	if (dma_validate_args(osi_dma, l_dma) < 0) {
		goto fail;
	}

	ret = common_is_mac_enabled(osi_dma->base, osi_dma->mac);
fail:
	return ret;
}

nve32_t osi_hw_transmit(struct osi_dma_priv_data *osi_dma, nveu32_t chan)
{
	struct dma_local *l_dma = (struct dma_local *)(void *)osi_dma;
	nve32_t ret = 0;

	if (osi_unlikely(dma_validate_args(osi_dma, l_dma) < 0)) {
		ret = -1;
		goto fail;
	}

	if (osi_unlikely(validate_dma_chan_num(osi_dma, chan) < 0)) {
		ret = -1;
		goto fail;
	}

	if (osi_unlikely(osi_dma->tx_ring[chan] == OSI_NULL)) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "DMA: Invalid Tx ring\n", 0ULL);
		ret = -1;
		goto fail;
	}

	ret = hw_transmit(osi_dma, osi_dma->tx_ring[chan], chan);
fail:
	return ret;
}

#ifdef OSI_DEBUG
nve32_t osi_dma_ioctl(struct osi_dma_priv_data *osi_dma)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	struct osi_dma_ioctl_data *data;

	if (osi_unlikely(dma_validate_args(osi_dma, l_dma) < 0)) {
		return -1;
	}

	data = &osi_dma->ioctl_data;

	switch (data->cmd) {
	case OSI_DMA_IOCTL_CMD_REG_DUMP:
		reg_dump(osi_dma);
		break;
	case OSI_DMA_IOCTL_CMD_STRUCTS_DUMP:
		structs_dump(osi_dma);
		break;
	case OSI_DMA_IOCTL_CMD_DEBUG_INTR_CONFIG:
		l_dma->ops_p->debug_intr_config(osi_dma);
		break;
	default:
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "DMA: Invalid IOCTL command", 0ULL);
		return -1;
	}

	return 0;
}
#endif /* OSI_DEBUG */

#ifndef OSI_STRIPPED_LIB

/**
 * @brief osi_slot_args_validate - Validate slot function arguments
 *
 * @note
 * Algorithm:
 *  - Check set argument and return error.
 *  - Validate osi_dma structure pointers.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] set: Flag to set with OSI_ENABLE and reset with OSI_DISABLE
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t osi_slot_args_validate(struct osi_dma_priv_data *osi_dma,
					     struct dma_local *l_dma,
					     nveu32_t set)
{
	if (dma_validate_args(osi_dma, l_dma) < 0) {
		return -1;
	}

	/* return on invalid set argument */
	if ((set != OSI_ENABLE) && (set != OSI_DISABLE)) {
		OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
			    "dma: Invalid set argument\n", set);
		return -1;
	}

	return 0;
}

nve32_t osi_config_slot_function(struct osi_dma_priv_data *osi_dma,
				 nveu32_t set)
{
	struct dma_local *l_dma = (struct dma_local *)osi_dma;
	nveu32_t i = 0U, chan = 0U, interval = 0U;
	struct osi_tx_ring *tx_ring = OSI_NULL;

	/* Validate arguments */
	if (osi_slot_args_validate(osi_dma, l_dma, set) < 0) {
		return -1;
	}

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		/* Get DMA channel and validate */
		chan = osi_dma->dma_chans[i];

		if ((chan == 0x0U) ||
		    (chan >= l_dma->num_max_chans)) {
			/* Ignore 0 and invalid channels */
			continue;
		}

		/* Check for slot enable */
		if (osi_dma->slot_enabled[chan] == OSI_ENABLE) {
			/* Get DMA slot interval and validate */
			interval = osi_dma->slot_interval[chan];
			if (interval > OSI_SLOT_INTVL_MAX) {
				OSI_DMA_ERR(osi_dma->osd,
					    OSI_LOG_ARG_INVALID,
					    "dma: Invalid interval arguments\n",
					    interval);
				return -1;
			}

			tx_ring = osi_dma->tx_ring[chan];
			if (tx_ring == OSI_NULL) {
				OSI_DMA_ERR(osi_dma->osd, OSI_LOG_ARG_INVALID,
					    "tx_ring is null\n", chan);
				return -1;
			}
			tx_ring->slot_check = set;
			l_dma->ops_p->config_slot(osi_dma, chan, set, interval);
		}
	}

	return 0;
}

nve32_t osi_txring_empty(struct osi_dma_priv_data *osi_dma, nveu32_t chan)
{
	struct osi_tx_ring *tx_ring = osi_dma->tx_ring[chan];

	return (tx_ring->clean_idx == tx_ring->cur_tx_idx) ? 1 : 0;
}
#endif /* !OSI_STRIPPED_LIB */
