/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
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


#ifndef INCLUDED_DMA_LOCAL_H
#define INCLUDED_DMA_LOCAL_H

#include "../osi/common/common.h"
#include <osi_dma.h>
#include "eqos_dma.h"
#include "mgbe_dma.h"

/**
 * @brief Maximum number of OSI DMA instances.
 */
#ifndef MAX_DMA_INSTANCES
#define MAX_DMA_INSTANCES	10U
#endif

/**
 * @brief Default DMA Tx/Rx ring sizes for EQOS/MGBE.
 */
#define EQOS_DEFAULT_RING_SZ	1024U
#define MGBE_DEFAULT_RING_SZ	4096U
#define MGBE_MAX_RING_SZ	16384U
#define HW_MIN_RING_SZ		4U

/**
 * @brief MAC DMA Channel operations
 */
struct dma_chan_ops {
#ifndef OSI_STRIPPED_LIB
	/** Called to configure the DMA channel slot function */
	void (*config_slot)(struct osi_dma_priv_data *osi_dma,
			    nveu32_t chan,
			    nveu32_t set,
			    nveu32_t interval);
#endif /* !OSI_STRIPPED_LIB */
#ifdef OSI_DEBUG
	/** Called to enable/disable debug interrupt */
	void (*debug_intr_config)(struct osi_dma_priv_data *osi_dma);
#endif
};

/**
 * @brief DMA descriptor operations
 */
struct desc_ops {
	/** Called to get receive checksum */
	void (*get_rx_csum)(const struct osi_rx_desc *const rx_desc,
			    struct osi_rx_pkt_cx *rx_pkt_cx);
#ifndef OSI_STRIPPED_LIB
	/** Called to get rx error stats */
	void (*update_rx_err_stats)(struct osi_rx_desc *rx_desc,
				    struct osi_pkt_err_stats *stats);
	/** Called to get rx VLAN from descriptor */
	void (*get_rx_vlan)(struct osi_rx_desc *rx_desc,
			    struct osi_rx_pkt_cx *rx_pkt_cx);
	/** Called to get rx HASH from descriptor */
	void (*get_rx_hash)(struct osi_rx_desc *rx_desc,
			    struct osi_rx_pkt_cx *rx_pkt_cx);
#endif /* !OSI_STRIPPED_LIB */
	/** Called to get RX hw timestamp */
	nve32_t (*get_rx_hwstamp)(const struct osi_dma_priv_data *const osi_dma,
				  const struct osi_rx_desc *const rx_desc,
				  const struct osi_rx_desc *const context_desc,
				  struct osi_rx_pkt_cx *rx_pkt_cx);
};

/**
 * @brief OSI DMA private data.
 */
struct dma_local {
	/** OSI DMA data variable */
	struct osi_dma_priv_data osi_dma;
	/** DMA channel operations */
	struct dma_chan_ops *ops_p;
	/**
	 * PacketID for PTP TS.
	 * MSB 4-bits of channel number and LSB 6-bits of local
	 * index(PKT_ID_CNT).
	 */
	nveu32_t pkt_id;
	/** Flag to represent OSI DMA software init done */
	nveu32_t init_done;
	/** Holds the MAC version of MAC controller */
	nveu32_t mac_ver;
	/** Magic number to validate osi_dma pointer */
	nveu64_t magic_num;
	/** Maximum number of DMA channels */
	nveu32_t num_max_chans;
	/** Exact MAC used across SOCs 0:Legacy EQOS, 1:Orin EQOS, 2:Orin MGBE */
	nveu32_t l_mac_ver;
};

#ifndef OSI_STRIPPED_LIB
/**
 * @brief eqos_init_dma_chan_ops - Initialize eqos DMA operations.
 *
 * @param[in] ops: DMA channel operations pointer.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void eqos_init_dma_chan_ops(struct dma_chan_ops *ops);

/**
 * @brief mgbe_init_dma_chan_ops - Initialize MGBE DMA operations.
 *
 * @param[in] ops: DMA channel operations pointer.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void mgbe_init_dma_chan_ops(struct dma_chan_ops *ops);
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief eqos_get_desc_ops - EQOS init DMA descriptor operations
 */
void eqos_init_desc_ops(struct desc_ops *p_dops);

/**
 * @brief mgbe_get_desc_ops - MGBE init DMA descriptor operations
 */
void mgbe_init_desc_ops(struct desc_ops *p_dops);

nve32_t init_desc_ops(const struct osi_dma_priv_data *const osi_dma);

/**
 * @brief osi_hw_transmit - Initialize Tx DMA descriptors for a channel
 *
 * @note
 * Algorithm:
 *  - Initialize Transmit descriptors with DMA mappable buffers,
 *    set OWN bit, Tx ring length and set starting address of Tx DMA channel
 *    Tx ring base address in Tx DMA registers.
 *
 * @param[in, out] osi_dma: OSI DMA private data.
 * @param[in] tx_ring: DMA Tx ring.
 * @param[in] dma_chan: DMA Tx channel number. Max OSI_EQOS_MAX_NUM_CHANS.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
nve32_t hw_transmit(struct osi_dma_priv_data *osi_dma,
		    struct osi_tx_ring *tx_ring,
		    nveu32_t dma_chan);

/* Function prototype needed for misra */

/**
 * @brief dma_desc_init - Initialize DMA Tx/Rx descriptors
 *
 * @note
 * Algorithm:
 *  - Transmit and Receive descriptors will be initialized with
 *    required values so that MAC DMA can understand and act accordingly.
 *
 * @param[in, out] osi_dma: OSI DMA private data structure.
 * @param[in] ops: DMA channel operations.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t dma_desc_init(struct osi_dma_priv_data *osi_dma);

static inline nveu32_t is_power_of_two(nveu32_t num)
{
	nveu32_t ret = OSI_DISABLE;

	if ((num > 0U) && ((num & (num - 1U)) == 0U)) {
		ret = OSI_ENABLE;
	}

	return ret;
}

#define BOOLEAN_FALSE	(0U != 0U)
#define L32(data)       ((nveu32_t)((data) & 0xFFFFFFFFU))
#define H32(data)       ((nveu32_t)(((data) & 0xFFFFFFFF00000000UL) >> 32UL))

static inline void update_rx_tail_ptr(const struct osi_dma_priv_data *const osi_dma,
				      nveu32_t dma_chan,
				      nveu64_t tailptr)
{
	nveu32_t chan = dma_chan & 0xFU;
	const nveu32_t tail_ptr_reg[2] = {
		EQOS_DMA_CHX_RDTP(chan),
		MGBE_DMA_CHX_RDTLP(chan)
	};

	osi_writel(L32(tailptr), (nveu8_t *)osi_dma->base + tail_ptr_reg[osi_dma->mac]);
}

/** @} */

#endif /* INCLUDED_DMA_LOCAL_H */
