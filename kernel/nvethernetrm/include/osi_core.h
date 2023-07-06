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

#ifndef INCLUDED_OSI_CORE_H
#define INCLUDED_OSI_CORE_H

#include "nvethernetrm_export.h"
#include "nvethernetrm_l3l4.h"
#include <osi_common.h>
#include "mmc.h"

struct ivc_msg_common;
/**
 * @addtogroup typedef related info
 *
 * @brief typedefs that indicate size and signness
 * @{
 */
/* Following added to avoid misraC 4.6
 * Here we are defining intermediate type
 */
/** intermediate type for long long */
typedef long long		my_lint_64;

/** typedef equivalent to long long */
typedef my_lint_64		nvel64_t;
/** @} */

#ifndef OSI_STRIPPED_LIB
#define OSI_OPER_EN_L2_DA_INV	OSI_BIT(4)
#define OSI_OPER_DIS_L2_DA_INV	OSI_BIT(5)
#define OSI_PTP_SNAP_TRANSPORT	1U
#define OSI_VLAN_ACTION_DEL	0x0U
#define OSI_VLAN_ACTION_ADD	OSI_BIT(31)
#define OSI_RXQ_ROUTE_PTP	0U
#define EQOS_MAX_HTR_REGS		8U

/**
 * @addtogroup RSS related information
 *
 * @brief RSS hash key and table size.
 * @{
 */
#define OSI_RSS_HASH_KEY_SIZE	40U
#define OSI_RSS_MAX_TABLE_SIZE	128U
/** @} */

#define OSI_CMD_RESET_MMC		12U
#define OSI_CMD_MDC_CONFIG		1U
#define OSI_CMD_MAC_LB			14U
#define OSI_CMD_FLOW_CTRL		15U
#define OSI_CMD_CONFIG_TXSTATUS		27U
#define OSI_CMD_CONFIG_RX_CRC_CHECK	25U
#define OSI_CMD_CONFIG_EEE		32U
#define OSI_CMD_ARP_OFFLOAD		30U
#define OSI_CMD_UPDATE_VLAN_ID		26U
#define OSI_CMD_VLAN_FILTER		31U
#define OSI_CMD_CONFIG_PTP_OFFLOAD	34U
#define OSI_CMD_PTP_RXQ_ROUTE		35U
#define OSI_CMD_CONFIG_RSS		37U
#define OSI_CMD_CONFIG_FW_ERR		29U
#define OSI_CMD_SET_MODE		16U
#define OSI_CMD_POLL_FOR_MAC_RST	4U
#define OSI_CMD_GET_MAC_VER		10U

/**
 * @addtogroup PTP-offload PTP offload defines
 * @{
 */
#define OSI_PTP_MAX_PORTID	0xFFFFU
#define OSI_PTP_MAX_DOMAIN	0xFFU
#define OSI_PTP_SNAP_ORDINARY	0U
#define OSI_PTP_SNAP_P2P	3U
/** @} */

#define OSI_MAC_TCR_TSMASTERENA		OSI_BIT(15)
#define OSI_MAC_TCR_TSEVENTENA		OSI_BIT(14)
#define OSI_MAC_TCR_TSENALL		OSI_BIT(8)
#define OSI_MAC_TCR_SNAPTYPSEL_3	(OSI_BIT(16) | OSI_BIT(17))
#define OSI_MAC_TCR_SNAPTYPSEL_2	OSI_BIT(17)
#define OSI_MAC_TCR_CSC			OSI_BIT(19)
#define OSI_MAC_TCR_AV8021ASMEN		OSI_BIT(28)

#define OSI_FLOW_CTRL_RX		OSI_BIT(1)

#define OSI_INSTANCE_ID_MBGE0	0
#define OSI_INSTANCE_ID_MGBE1	1
#define OSI_INSTANCE_ID_MGBE2	2
#define OSI_INSTANCE_ID_MGBE3	3
#define OSI_INSTANCE_ID_EQOS	4

#endif /* !OSI_STRIPPED_LIB */


#ifdef MACSEC_SUPPORT
/**
 * @addtogroup MACSEC related helper MACROs
 *
 * @brief MACSEC generic helper MACROs
 * @{
 */
#define OSI_MAX_NUM_SC                  8U
#define OSI_SCI_LEN			8U
#define OSI_KEY_LEN_128			16U
#define OSI_KEY_LEN_256			32U
#define OSI_NUM_CTLR			2U
/** @} */
#endif /* MACSEC_SUPPORT */

/**
 * @addtogroup PTP PTP related information
 *
 * @brief PTP MAC-to-MAC sync role
 */
#define OSI_PTP_M2M_INACTIVE	0U
#define OSI_PTP_M2M_PRIMARY	1U
#define OSI_PTP_M2M_SECONDARY	2U
/** @} */


/**
 * @addtogroup EQOS_PTP PTP Helper MACROS
 *
 * @brief EQOS PTP MAC Time stamp control reg bit fields
 * @{
 */
#define OSI_MAC_TCR_TSENA		OSI_BIT(0)
#define OSI_MAC_TCR_TSCFUPDT		OSI_BIT(1)
#define OSI_MAC_TCR_TSCTRLSSR		OSI_BIT(9)
#define OSI_MAC_TCR_TSVER2ENA		OSI_BIT(10)
#define OSI_MAC_TCR_TSIPENA		OSI_BIT(11)
#define OSI_MAC_TCR_TSIPV6ENA		OSI_BIT(12)
#define OSI_MAC_TCR_TSIPV4ENA		OSI_BIT(13)
#define OSI_MAC_TCR_SNAPTYPSEL_1	OSI_BIT(16)
#define OSI_MAC_TCR_TXTSSMIS		OSI_BIT(31)
/** @} */

/**
 * @addtogroup Helper MACROS
 *
 * @brief EQOS generic helper MACROS.
 * @{
 */
#define EQOS_DMA_CHX_IER(x)		((0x0080U * (x)) + 0x1134U)
#define EQOS_MAX_MAC_ADDRESS_FILTER	128U
#define EQOS_MAX_MAC_5_3_ADDRESS_FILTER	32U
#define EQOS_MAX_L3_L4_FILTER		8U
#define OSI_MGBE_MAX_MAC_ADDRESS_FILTER	32U
#define OSI_DA_MATCH			0U
#ifndef OSI_STRIPPED_LIB
#define OSI_INV_MATCH			1U
#endif /* !OSI_STRIPPED_LIB */
#define OSI_AMASK_DISABLE		0U
#define OSI_CHAN_ANY			0xFFU
#define OSI_DFLT_MTU_SIZE		1500U
#define OSI_MTU_SIZE_9000		9000U
/* Reg ETHER_QOS_AUTO_CAL_CONFIG_0[AUTO_CAL_PD/PU_OFFSET] max value */
#define OSI_PAD_CAL_CONFIG_PD_PU_OFFSET_MAX	0x1FU

#ifndef OSI_STRIPPED_LIB
/* HW supports 8 Hash table regs, but eqos_validate_core_regs only checks 4 */
#define OSI_EQOS_MAX_HASH_REGS		4U
#endif /* !OSI_STRIPPED_LIB */

#define OSI_FLOW_CTRL_TX		OSI_BIT(0)

#define OSI_FULL_DUPLEX			1
#define OSI_HALF_DUPLEX			0

/* L2 filter operations supported by OSI layer. These operation modes shall be
 * set by OSD driver as input to update registers accordingly.
 */
#define OSI_OPER_EN_PROMISC		OSI_BIT(0)
#define OSI_OPER_DIS_PROMISC		OSI_BIT(1)
#define OSI_OPER_EN_ALLMULTI		OSI_BIT(2)
#define OSI_OPER_DIS_ALLMULTI		OSI_BIT(3)
#define OSI_OPER_EN_PERFECT		OSI_BIT(6)
#define OSI_OPER_DIS_PERFECT		OSI_BIT(7)
#define OSI_OPER_ADDR_UPDATE		OSI_BIT(8)
#define OSI_OPER_ADDR_DEL		OSI_BIT(9)

#define OSI_PFT_MATCH		0U
#define OSI_SA_MATCH		1U

#define OSI_SPEED_10		10
#define OSI_SPEED_100		100
#define OSI_SPEED_1000		1000
#define OSI_SPEED_2500		2500
#define OSI_SPEED_5000		5000
#define OSI_SPEED_10000		10000

#define TEN_POWER_9		0x3B9ACA00U
#define TWO_POWER_32		0x100000000ULL
#define TWO_POWER_31		0x80000000U
/* MDIO clause 45 bit */
#define OSI_MII_ADDR_C45		OSI_BIT(30)
/** @} */

/**
 * @brief Ethernet PHY Interface Modes
 */
#define OSI_XFI_MODE_10G	0U
#define OSI_XFI_MODE_5G	1U
#define OSI_USXGMII_MODE_10G	2U
#define OSI_USXGMII_MODE_5G	3U

/**
 * @addtogroup IOCTL OPS MACROS
 *
 * @brief IOCTL OPS for runtime commands
 * @{
 */
#define OSI_CMD_L3L4_FILTER		3U
#define OSI_CMD_COMMON_ISR		7U
#define OSI_CMD_PAD_CALIBRATION		8U
#define OSI_CMD_READ_MMC		9U
#define OSI_CMD_SET_SPEED		17U
#define OSI_CMD_L2_FILTER		18U
#define OSI_CMD_RXCSUM_OFFLOAD		19U
#define OSI_CMD_ADJ_FREQ		20U
#define OSI_CMD_ADJ_TIME		21U
#define OSI_CMD_CONFIG_PTP		22U
#define OSI_CMD_GET_AVB			23U
#define OSI_CMD_SET_AVB			24U
#define OSI_CMD_GET_HW_FEAT		28U
#define OSI_CMD_SET_SYSTOHW_TIME	33U
#define OSI_CMD_CONFIG_FRP		36U
#define OSI_CMD_CONFIG_EST		38U
#define OSI_CMD_CONFIG_FPE		39U
#define OSI_CMD_READ_REG		40U
#define OSI_CMD_WRITE_REG		41U
#define OSI_CMD_GET_TX_TS			42U
#define OSI_CMD_FREE_TS			43U
#ifdef OSI_DEBUG
#define OSI_CMD_REG_DUMP		44U
#define OSI_CMD_STRUCTS_DUMP		45U
#endif /* OSI_DEBUG */
#define OSI_CMD_CAP_TSC_PTP		46U
#define OSI_CMD_MAC_MTU			47U
#define OSI_CMD_CONF_M2M_TS		48U
#ifdef MACSEC_SUPPORT
#define OSI_CMD_READ_MACSEC_REG		49U
#define OSI_CMD_WRITE_MACSEC_REG	50U
#endif /* MACSEC_SUPPORT */
#ifdef HSI_SUPPORT
#define OSI_CMD_HSI_CONFIGURE		51U
#endif
#ifdef OSI_DEBUG
#define OSI_CMD_DEBUG_INTR_CONFIG	52U
#endif
#define OSI_CMD_SUSPEND			53U
#define OSI_CMD_RESUME			54U
#ifdef HSI_SUPPORT
#define OSI_CMD_HSI_INJECT_ERR		55U
#endif
#define OSI_CMD_READ_STATS		56U
/** @} */

#ifdef LOG_OSI
/**
 * @brief OSI error macro definition,
 * @param[in] priv: OSD private data OR NULL
 * @param[in] type: error type
 * @param[in] err:  error string
 * @param[in] loga: error additional information
 */
#define OSI_CORE_ERR(priv, type, err, loga)			\
{								\
	osi_core->osd_ops.ops_log(priv, __func__, __LINE__,	\
				  OSI_LOG_ERR, type, err, loga);\
}

/**
 * @brief OSI info macro definition
 * @param[in] priv: OSD private data OR NULL
 * @param[in] type: error type
 * @param[in] err:  error string
 * @param[in] loga: error additional information
 */
#define OSI_CORE_INFO(priv, type, err, loga)				\
{									\
	osi_core->osd_ops.ops_log(priv, __func__, __LINE__,		\
				  OSI_LOG_INFO, type, err, loga);	\
}
#else
#define OSI_CORE_ERR(priv, type, err, loga)
#define OSI_CORE_INFO(priv, type, err, loga)
#endif

#define VLAN_NUM_VID		4096U
#define OSI_DELAY_1000US	1000U
#define OSI_DELAY_1US		1U

/**
 * @addtogroup PTP PTP related information
 *
 * @brief PTP SSINC values
 * @{
 */
#define OSI_PTP_SSINC_4		4U
#define OSI_PTP_SSINC_6		6U
/** @} */

/**
 * @addtogroup Flexible Receive Parser related information
 *
 * @brief Flexible Receive Parser commands, table size and other defines
 * @{
 */
#ifndef OSI_STRIPPED_LIB
#define OSI_FRP_CMD_MAX			3U
#define OSI_FRP_MATCH_MAX		10U
#endif /* !OSI_STRIPPED_LIB */
#define OSI_FRP_MAX_ENTRY		256U
#define OSI_FRP_OFFSET_MAX		64U
/* FRP Command types */
#define OSI_FRP_CMD_ADD			0U
#define OSI_FRP_CMD_UPDATE		1U
#define OSI_FRP_CMD_DEL			2U
/* FRP Filter mode defines */
#define OSI_FRP_MODE_ROUTE		0U
#define OSI_FRP_MODE_DROP		1U
#define OSI_FRP_MODE_BYPASS		2U
#define OSI_FRP_MODE_LINK		3U
#define OSI_FRP_MODE_IM_ROUTE		4U
#define OSI_FRP_MODE_IM_DROP		5U
#define OSI_FRP_MODE_IM_BYPASS		6U
#define OSI_FRP_MODE_IM_LINK		7U
#define OSI_FRP_MODE_MAX		8U
/* Match data defines */
#define OSI_FRP_MATCH_NORMAL		0U
#define OSI_FRP_MATCH_L2_DA		1U
#define OSI_FRP_MATCH_L2_SA		2U
#define OSI_FRP_MATCH_L3_SIP		3U
#define OSI_FRP_MATCH_L3_DIP		4U
#define OSI_FRP_MATCH_L4_S_UPORT	5U
#define OSI_FRP_MATCH_L4_D_UPORT	6U
#define OSI_FRP_MATCH_L4_S_TPORT	7U
#define OSI_FRP_MATCH_L4_D_TPORT	8U
#define OSI_FRP_MATCH_VLAN		9U
/** @} */

#define XPCS_WRITE_FAIL_CODE	-9

#ifdef HSI_SUPPORT
/**
 * @addtogroup osi_hsi_err_code_idx
 *
 * @brief data index for osi_hsi_err_code array
 * @{
 */
#define UE_IDX			0U
#define CE_IDX			1U
#define RX_CRC_ERR_IDX		2U
#define TX_FRAME_ERR_IDX	3U
#define RX_CSUM_ERR_IDX		4U
#define AUTONEG_ERR_IDX		5U
#define XPCS_WRITE_FAIL_IDX	6U
#define MACSEC_RX_CRC_ERR_IDX	0U
#define MACSEC_TX_CRC_ERR_IDX	1U
#define MACSEC_RX_ICV_ERR_IDX	2U
#define MACSEC_REG_VIOL_ERR_IDX 3U
/** @} */

/**
 * @addtogroup HSI_TIME_THRESHOLD
 *
 * @brief HSI time threshold to report error in ms
 * @{
 */
#define OSI_HSI_ERR_TIME_THRESHOLD_DEFAULT	3000U
#define OSI_HSI_ERR_TIME_THRESHOLD_MIN		1000U
#define OSI_HSI_ERR_TIME_THRESHOLD_MAX		60000U
/** @} */

/**
 * @brief HSI error count threshold to report error
 */
#define OSI_HSI_ERR_COUNT_THRESHOLD		1000U

/**
 * @brief Maximum number of different mac error code
 * HSI_SW_ERR_CODE + Two (Corrected and Uncorrected error code)
 */
#define OSI_HSI_MAX_MAC_ERROR_CODE		7U

/**
 * @brief Maximum number of different macsec error code
 */
#define HSI_MAX_MACSEC_ERROR_CODE	4U

/**
 * @addtogroup HSI_SW_ERR_CODE
 *
 * @brief software defined error code
 * @{
 */
#define OSI_INBOUND_BUS_CRC_ERR		0x1001U
#define OSI_TX_FRAME_ERR		0x1002U
#define OSI_RECEIVE_CHECKSUM_ERR	0x1003U
#define OSI_PCS_AUTONEG_ERR		0x1004U
#define OSI_MACSEC_RX_CRC_ERR		0x1005U
#define OSI_MACSEC_TX_CRC_ERR		0x1006U
#define OSI_MACSEC_RX_ICV_ERR		0x1007U
#define OSI_MACSEC_REG_VIOL_ERR		0x1008U
#define OSI_XPCS_WRITE_FAIL_ERR		0x1009U

#define OSI_HSI_MGBE0_UE_CODE		0x2A00U
#define OSI_HSI_MGBE1_UE_CODE		0x2A01U
#define OSI_HSI_MGBE2_UE_CODE		0x2A02U
#define OSI_HSI_MGBE3_UE_CODE		0x2A03U
#define OSI_HSI_EQOS0_UE_CODE		0x28ADU

#define OSI_HSI_MGBE0_CE_CODE		0x2E08U
#define OSI_HSI_MGBE1_CE_CODE		0x2E09U
#define OSI_HSI_MGBE2_CE_CODE		0x2E0AU
#define OSI_HSI_MGBE3_CE_CODE		0x2E0BU
#define OSI_HSI_EQOS0_CE_CODE		0x2DE6U

#define OSI_HSI_MGBE0_REPORTER_ID	0x8019U
#define OSI_HSI_MGBE1_REPORTER_ID	0x801AU
#define OSI_HSI_MGBE2_REPORTER_ID	0x801BU
#define OSI_HSI_MGBE3_REPORTER_ID	0x801CU
#define OSI_HSI_EQOS0_REPORTER_ID	0x8009U
/** @} */
#endif

struct osi_core_priv_data;

/**
 * @brief OSI core structure for filters
 */
struct osi_filter {
	/** indicates operation needs to perform. refer to OSI_OPER_* */
	nveu32_t oper_mode;
	/** Indicates the index of the filter to be modified.
	 * Filter index must be between 0 - 127 */
	nveu32_t index;
	/** Ethernet MAC address to be added */
	nveu8_t mac_address[OSI_ETH_ALEN];
	/** Indicates dma channel routing enable(1) disable (0) */
	nveu32_t dma_routing;
	/**  indicates dma channel number to program */
	nveu32_t dma_chan;
	/** filter will not consider byte in comparison
	 *	Bit 5: MAC_Address${i}_High[15:8]
	 *	Bit 4: MAC_Address${i}_High[7:0]
	 *	Bit 3: MAC_Address${i}_Low[31:24]
	 *	..
	 *	Bit 0: MAC_Address${i}_Low[7:0] */
	nveu32_t addr_mask;
	/** src_dest: SA(1) or DA(0) */
	nveu32_t src_dest;
	/**  indicates one hot encoded DMA receive channels to program */
	nveu32_t dma_chansel;
};

#ifndef OSI_STRIPPED_LIB
/**
 * @brief OSI core structure for RXQ route
 */
struct osi_rxq_route {
#define OSI_RXQ_ROUTE_PTP	0U
	/** Indicates RX routing type OSI_RXQ_ROUTE_* */
	nveu32_t route_type;
	/** RXQ routing enable(1) disable (0) */
	nveu32_t enable;
	/** RX queue index */
	nveu32_t idx;
};
#endif

/**
 * @brief struct osi_hw_features - MAC HW supported features.
 */
struct osi_hw_features {
	/** It is set to 1 when 10/100 Mbps is selected as the Mode of
	 * Operation */
	nveu32_t mii_sel;
	/** It is set to 1 when the RGMII Interface option is selected */
	nveu32_t rgmii_sel;
	/** It is set to 1 when the RMII Interface option is selected */
	nveu32_t rmii_sel;
	/** It sets to 1 when 1000 Mbps is selected as the Mode of Operation */
	nveu32_t gmii_sel;
	/** It sets to 1 when the half-duplex mode is selected */
	nveu32_t hd_sel;
	/** It sets to 1 when the TBI, SGMII, or RTBI PHY interface
	 * option is selected */
	nveu32_t pcs_sel;
	/** It sets to 1 when the Enable VLAN Hash Table Based Filtering
	 * option is selected */
	nveu32_t vlan_hash_en;
	/** It sets to 1 when the Enable Station Management (MDIO Interface)
	 * option is selected */
	nveu32_t sma_sel;
	/** It sets to 1 when the Enable Remote Wake-Up Packet Detection
	 * option is selected */
	nveu32_t rwk_sel;
	/** It sets to 1 when the Enable Magic Packet Detection option is
	 * selected */
	nveu32_t mgk_sel;
	/** It sets to 1 when the Enable MAC Management Counters (MMC) option
	 * is selected */
	nveu32_t mmc_sel;
	/** It sets to 1 when the Enable IPv4 ARP Offload option is selected */
	nveu32_t arp_offld_en;
	/** It sets to 1 when the Enable IEEE 1588 Timestamp Support option
	 * is selected */
	nveu32_t ts_sel;
	/** It sets to 1 when the Enable Energy Efficient Ethernet (EEE) option
	 * is selected */
	nveu32_t eee_sel;
	/** It sets to 1 when the Enable Transmit TCP/IP Checksum Insertion
	 * option is selected */
	nveu32_t tx_coe_sel;
	/** It sets to 1 when the Enable Receive TCP/IP Checksum Check option
	 * is selected */
	nveu32_t rx_coe_sel;
	/** It sets to 1 when the Enable Additional 1-31 MAC Address Registers
	 * option is selected */
	nveu32_t mac_addr_sel;
	/** It sets to 1 when the Enable Additional 32-63 MAC Address Registers
	 * option is selected */
	nveu32_t mac_addr32_sel;
	/** It sets to 1 when the Enable Additional 64-127 MAC Address Registers
	 * option is selected */
	nveu32_t mac_addr64_sel;
	/** It sets to 1 when the Enable IEEE 1588 Timestamp Support option
	 * is selected */
	nveu32_t tsstssel;
	/** It sets to 1 when the Enable SA and VLAN Insertion on Tx option
	 * is selected */
	nveu32_t sa_vlan_ins;
	/** Active PHY Selected
	 * When you have multiple PHY interfaces in your configuration,
	 * this field indicates the sampled value of phy_intf_sel_i during
	 * reset de-assertion:
	 * 000: GMII or MII
	 * 001: RGMII
	 * 010: SGMII
	 * 011: TBI
	 * 100: RMII
	 * 101: RTBI
	 * 110: SMII
	 * 111: RevMII
	 * All Others: Reserved */
	nveu32_t act_phy_sel;
	/** MTL Receive FIFO Size
	 * This field contains the configured value of MTL Rx FIFO in bytes
	 * expressed as Log to base 2 minus 7, that is, Log2(RXFIFO_SIZE) -7:
	 * 00000: 128 bytes
	 * 00001: 256 bytes
	 * 00010: 512 bytes
	 * 00011: 1,024 bytes
	 * 00100: 2,048 bytes
	 * 00101: 4,096 bytes
	 * 00110: 8,192 bytes
	 * 00111: 16,384 bytes
	 * 01000: 32,767 bytes
	 * 01000: 32 KB
	 * 01001: 64 KB
	 * 01010: 128 KB
	 * 01011: 256 KB
	 * 01100-11111: Reserved */
	nveu32_t rx_fifo_size;
	/** MTL Transmit FIFO Size.
	 * This field contains the configured value of MTL Tx FIFO in
	 * bytes expressed as Log to base 2 minus 7, that is,
	 * Log2(TXFIFO_SIZE) -7:
	 * 00000: 128 bytes
	 * 00001: 256 bytes
	 * 00010: 512 bytes
	 * 00011: 1,024 bytes
	 * 00100: 2,048 bytes
	 * 00101: 4,096 bytes
	 * 00110: 8,192 bytes
	 * 00111: 16,384 bytes
	 * 01000: 32 KB
	 * 01001: 64 KB
	 * 01010: 128 KB
	 * 01011-11111: Reserved */
	nveu32_t tx_fifo_size;
	/** It set to 1 when Advance timestamping High Word selected */
	nveu32_t adv_ts_hword;
	/** Address Width.
	 * This field indicates the configured address width:
	 * 00: 32
	 * 01: 40
	 * 10: 48
	 * 11: Reserved */
	nveu32_t addr_64;
	/** It sets to 1 when DCB Feature Enable */
	nveu32_t dcb_en;
	/** It sets to 1 when Split Header Feature Enable */
	nveu32_t sph_en;
	/** It sets to 1 when TCP Segmentation Offload Enable */
	nveu32_t tso_en;
	/** It sets to 1 when DMA debug registers are enabled */
	nveu32_t dma_debug_gen;
	/** It sets to 1 if AV Feature Enabled */
	nveu32_t av_sel;
	/** It sets to 1 if Receive side AV Feature Enabled */
	nveu32_t rav_sel;
	/** This field indicates the size of the hash table:
	 * 00: No hash table
	 * 01: 64
	 * 10: 128
	 * 11: 256 */
	nveu32_t hash_tbl_sz;
	/** This field indicates the total number of L3 or L4 filters:
	 * 0000: No L3 or L4 Filter
	 * 0001: 1 L3 or L4 Filter
	 * 0010: 2 L3 or L4 Filters
	 * ..
	 * 1000: 8 L3 or L4 */
	nveu32_t l3l4_filter_num;
	/** It holds number of MTL Receive Queues */
	nveu32_t rx_q_cnt;
	/** It holds number of MTL Transmit Queues */
	nveu32_t tx_q_cnt;
	/** It holds number of DMA Receive channels */
	nveu32_t rx_ch_cnt;
	/** This field indicates the number of DMA Transmit channels:
	 * 0000: 1 DMA Tx Channel
	 * 0001: 2 DMA Tx Channels
	 * ..
	 * 0111: 8 DMA Tx */
	nveu32_t tx_ch_cnt;
	/** This field indicates the number of PPS outputs:
	 * 000: No PPS output
	 * 001: 1 PPS output
	 * 010: 2 PPS outputs
	 * 011: 3 PPS outputs
	 * 100: 4 PPS outputs
	 * 101-111: Reserved */
	nveu32_t pps_out_num;
	/** Number of Auxiliary Snapshot Inputs
	 * This field indicates the number of auxiliary snapshot inputs:
	 * 000: No auxiliary input
	 * 001: 1 auxiliary input
	 * 010: 2 auxiliary inputs
	 * 011: 3 auxiliary inputs
	 * 100: 4 auxiliary inputs
	 * 101-111: Reserved */
	nveu32_t aux_snap_num;
	/** VxLAN/NVGRE Support */
	nveu32_t vxn;
	/** Enhanced DMA.
	 * This bit is set to 1 when the "Enhanced DMA" option is
	 * selected. */
	nveu32_t edma;
	/** Different Descriptor Cache
	 * When set to 1, then EDMA mode Separate Memory is
	 * selected for the Descriptor Cache.*/
	nveu32_t ediffc;
	/** PFC Enable
	 * This bit is set to 1 when the Enable PFC Feature is selected */
	nveu32_t pfc_en;
	/** One-Step Timestamping Enable */
	nveu32_t ost_en;
	/** PTO Offload Enable */
	nveu32_t pto_en;
	/** Receive Side Scaling Enable */
	nveu32_t rss_en;
	/** Number of Traffic Classes */
	nveu32_t num_tc;
	/** Number of Extended VLAN Tag Filters Enabled */
	nveu32_t num_vlan_filters;
	/** Supported Flexible Receive Parser.
	 * This bit is set to 1 when the Enable Flexible Programmable
	 * Receive Parser option is selected */
	nveu32_t frp_sel;
	/** Queue/Channel based VLAN tag insertion on Tx Enable
	 * This bit is set to 1 when the Enable Queue/Channel based
	 * VLAN tag insertion on Tx Feature is selected. */
	nveu32_t cbti_sel;
	/** Supported Parallel Instruction Processor Engines (PIPEs)
	 * This field indicates the maximum number of Instruction
	 * Processors supported by flexible receive parser. */
	nveu32_t num_frp_pipes;
	/** One Step for PTP over UDP/IP Feature Enable
	 * This bit is set to 1 when the Enable One step timestamp for
	 * PTP over UDP/IP feature is selected */
	nveu32_t ost_over_udp;
	/** Supported Flexible Receive Parser Parsable Bytes
	 * This field indicates the supported Max Number of bytes of the
	 * packet data to be Parsed by Flexible Receive Parser */
	nveu32_t max_frp_bytes;
	/** Supported Flexible Receive Parser Instructions
	 * This field indicates the Max Number of Parser Instructions
	 * supported by Flexible Receive Parser */
	nveu32_t max_frp_entries;
	/** Double VLAN Processing Enabled
	 * This bit is set to 1 when the Enable Double VLAN Processing
	 * feature is selected */
	nveu32_t double_vlan_en;
	/** Automotive Safety Package
	 * Following are the encoding for the different Safety features
	 * Values:
	 * 0x0 (NONE): No Safety features selected
	 * 0x1 (ECC_ONLY): Only "ECC protection for external
	 * memory" feature is selected
	 * 0x2 (AS_NPPE): All the Automotive Safety features are
	 * selected without the "Parity Port Enable for external interface"
	 * feature
	 * 0x3 (AS_PPE): All the Automotive Safety features are
	 * selected with the "Parity Port Enable for external interface"
	 * feature */
	nveu32_t auto_safety_pkg;
	/** Tx Timestamp FIFO Depth
	 * This value indicates the depth of the Tx Timestamp FIFO
	 * 3'b000: Reserved
	 * 3'b001: 1
	 * 3'b010: 2
	 * 3'b011: 4
	 * 3'b100: 8
	 * 3'b101: 16
	 * 3'b110: Reserved
	 * 3'b111: Reserved */
	nveu32_t tts_fifo_depth;
	/** Enhancements to Scheduling Traffic Enable
	 * This bit is set to 1 when the Enable Enhancements to
	 * Scheduling Traffic feature is selected.
	 * Values:
	 * 0x0 (INACTIVE): Enable Enhancements to Scheduling
	 * Traffic feature is not selected
	 * 0x1 (ACTIVE): Enable Enhancements to Scheduling
	 * Traffic feature is selected */
	nveu32_t est_sel;
	/** Depth of the Gate Control List
	 * This field indicates the depth of Gate Control list expressed as
	 * Log2(DWCXG_GCL_DEP)-5
	 * Values:
	 * 0x0 (NODEPTH): No Depth configured
	 * 0x1 (DEPTH64): 64
	 * 0x2 (DEPTH128): 128
	 * 0x3 (DEPTH256): 256
	 * 0x4 (DEPTH512): 512
	 * 0x5 (DEPTH1024): 1024
	 * 0x6 (RSVD): Reserved */
	nveu32_t gcl_depth;
	/** Width of the Time Interval field in the Gate Control List
	 * This field indicates the width of the Configured Time Interval
	 * Field
	 * Values:
	 * 0x0 (NOWIDTH): Width not configured
	 * 0x1 (WIDTH16): 16
	 * 0x2 (WIDTH20): 20
	 * 0x3 (WIDTH24): 24 */
	nveu32_t gcl_width;
	/** Frame Preemption Enable
	 * This bit is set to 1 when the Enable Frame preemption feature
	 * is selected.
	 * Values:
	 * 0x0 (INACTIVE): Frame Preemption Enable feature is not
	 * selected
	 * 0x1 (ACTIVE): Frame Preemption Enable feature is
	 * selected */
	nveu32_t fpe_sel;
	/** Time Based Scheduling Enable
	 * This bit is set to 1 when the Time Based Scheduling feature is
	 * selected.
	 * Values:
	 * 0x0 (INACTIVE): Time Based Scheduling Enable feature is
	 * not selected
	 * 0x1 (ACTIVE): Time Based Scheduling Enable feature is
	 * selected */
	nveu32_t tbs_sel;
	/** The number of DMA channels enabled for TBS (starting from
	 * the highest Tx Channel in descending order)
	 * This field provides the number of DMA channels enabled for
	 * TBS (starting from the highest Tx Channel in descending
	 * order):
	 * 0000: 1 DMA Tx Channel enabled for TBS
	 * 0001: 2 DMA Tx Channels enabled for TBS
	 * 0010: 3 DMA Tx Channels enabled for TBS
	 * ...
	 * 1111: 16 DMA Tx Channels enabled for TBS */
	nveu32_t num_tbs_ch;
};

#ifndef OSI_STRIPPED_LIB
/**
 * @brief Vlan filter Function dependent parameter
 */
struct osi_vlan_filter {
	/** vlan filter enable(1) or disable(0) */
	nveu32_t filter_enb_dis;
	/** perfect(0) or hash(1) */
	nveu32_t perfect_hash;
	/** perfect(0) or inverse(1) */
	nveu32_t perfect_inverse_match;
};

/**
 * @brief L2 filter function dependent parameter
 */
struct osi_l2_da_filter {
	/** perfect(0) or hash(1) */
	nveu32_t perfect_hash;
	/** perfect(0) or inverse(1) */
	nveu32_t perfect_inverse_match;
};

/**
 * @brief struct ptp_offload_param - Parameter to support PTP offload.
 */
struct osi_pto_config {
	/** enable(0) / disable(1) */
	nveu32_t en_dis;
	/** Flag for Master mode.
	 * OSI_ENABLE for master OSI_DISABLE for slave */
	nveu32_t master;
	/** Flag to Select PTP packets for Taking Snapshots */
	nveu32_t snap_type;
	/** ptp domain */
	nveu32_t domain_num;
	/**  The PTP Offload function qualifies received PTP
	 *  packet with unicast Destination  address
	 *  0 - only multicast, 1 - unicast and multicast */
	nveu32_t mc_uc;
	/** Port identification */
	nveu32_t portid;
};

/**
 * @brief osi_core_rss - Struture used to store RSS Hash key and table
 * information.
 */
struct osi_core_rss {
	/** Flag to represent to enable RSS or not */
	nveu32_t enable;
	/** Array for storing RSS Hash key */
	nveu8_t key[OSI_RSS_HASH_KEY_SIZE];
	/** Array for storing RSS Hash table */
	nveu32_t table[OSI_RSS_MAX_TABLE_SIZE];
};

/**
 * @brief Max num of MAC core registers to backup. It should be max of or >=
 * (EQOS_MAX_BAK_IDX=380, coreX,...etc) backup registers.
 */
#define CORE_MAX_BAK_IDX	700U

/**
 * @brief core_backup - Struct used to store backup of core HW registers.
 */
struct core_backup {
	/** Array of reg MMIO addresses (base of MAC + offset of reg) */
	void *reg_addr[CORE_MAX_BAK_IDX];
	/** Array of value stored in each corresponding register */
	nveu32_t reg_val[CORE_MAX_BAK_IDX];
};

#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief PTP configuration structure
 */
struct osi_ptp_config {
	/** PTP filter parameters bit fields.
	 *
	 * Enable Timestamp, Fine Timestamp, 1 nanosecond accuracy
	 * are enabled by default.
	 *
	 * Need to set below bit fields accordingly as per the requirements.
	 *
	 * Enable Timestamp for All Packets			OSI_BIT(8)
	 *
	 * Enable PTP Packet Processing for Version 2 Format	OSI_BIT(10)
	 *
	 * Enable Processing of PTP over Ethernet Packets	OSI_BIT(11)
	 *
	 * Enable Processing of PTP Packets Sent over IPv6-UDP	OSI_BIT(12)
	 *
	 * Enable Processing of PTP Packets Sent over IPv4-UDP	OSI_BIT(13)
	 *
	 * Enable Timestamp Snapshot for Event Messages		OSI_BIT(14)
	 *
	 * Enable Snapshot for Messages Relevant to Master	OSI_BIT(15)
	 *
	 * Select PTP packets for Taking Snapshots		OSI_BIT(16)
	 *
	 * Select PTP packets for Taking Snapshots		OSI_BIT(17)
	 *
	 * Select PTP packets for Taking Snapshots (OSI_BIT(16) + OSI_BIT(17))
	 *
	 * AV 802.1AS Mode Enable				OSI_BIT(28)
	 *
	 * if ptp_filter is set to Zero then Time stamping is disabled */
	nveu32_t ptp_filter;
	/** seconds to be updated to MAC */
	nveu32_t sec;
	/** nano seconds to be updated to MAC */
	nveu32_t nsec;
	/** PTP reference clock read from DT */
	nveu32_t ptp_ref_clk_rate;
	/** Use one nsec accuracy (need to set 1) */
	nveu32_t one_nsec_accuracy;
	/** PTP system clock which is 62500000Hz */
	nveu32_t ptp_clock;
	/** PTP Packets RX Queue.*/
	nveu32_t ptp_rx_queue;
};

/**
 * @brief osi_core_ptp_tsc_data - Struture used to store TSC and PTP time
 * information.
 */
struct osi_core_ptp_tsc_data {
	/** high bits of MAC */
	nveu32_t ptp_high_bits;
	/** low bits of MAC */
	nveu32_t ptp_low_bits;
	/** high bits of TSC */
	nveu32_t tsc_high_bits;
	/** low bits of TSC */
	nveu32_t tsc_low_bits;
};

/**
 * @brief OSI VM IRQ data
 */
struct osi_vm_irq_data {
	/** Number of VM channels per VM IRQ */
	nveu32_t num_vm_chans;
	/** VM/OS number to be used */
	nveu32_t vm_num;
	/** Array of VM channel list */
	nveu32_t vm_chans[OSI_MGBE_MAX_NUM_CHANS];
};

/**
 *@brief OSD Core callbacks
 */
struct osd_core_ops {
	/** padctrl rx pin disable/enable callback */
	nve32_t (*padctrl_mii_rx_pins)(void *priv, nveu32_t enable);
	/** logging callback */
	void (*ops_log)(void *priv, const nve8_t *func, nveu32_t line,
			nveu32_t level, nveu32_t type, const nve8_t *err,
			nveul64_t loga);
	/** udelay callback */
	void (*udelay)(nveu64_t usec);
	/** usleep range callback */
	void (*usleep_range)(nveu64_t umin, nveu64_t umax);
	/** msleep callback */
	void (*msleep)(nveu32_t msec);
	/** ivcsend callback*/
	nve32_t (*ivc_send)(void *priv, struct ivc_msg_common *ivc,
			    nveu32_t len);
#ifdef MACSEC_SUPPORT
	/** Program macsec key table through Trust Zone callback */
	nve32_t (*macsec_tz_kt_config)(void *priv, nveu8_t cmd,
				   void *const kt_config,
				   void *const genl_info);
#endif /* MACSEC_SUPPORT */
#ifdef OSI_DEBUG
	/**.printf function callback */
	void (*printf)(struct osi_core_priv_data *osi_core,
		       nveu32_t type,
		       const char *fmt, ...);
#endif
	/** Lane bringup restart callback */
	void (*restart_lane_bringup)(void *priv, nveu32_t en_disable);
};

#ifdef MACSEC_SUPPORT
/**
 * @brief MACSEC secure channel basic information
 */
struct osi_macsec_sc_info {
	/** Secure channel identifier */
	nveu8_t sci[OSI_SCI_LEN];
	/** Secure association key */
	nveu8_t sak[OSI_KEY_LEN_256];
#ifdef MACSEC_KEY_PROGRAM
	/** Secure association key */
	nveu8_t hkey[OSI_KEY_LEN_128];
#endif /* MACSEC_KEY_PROGRAM */
	/** current AN */
	nveu8_t curr_an;
	/** Next PN to use for the current AN */
	nveu32_t next_pn;
	/** Lowest PN to use for the current AN */
	nveu32_t lowest_pn;
	/** bitmap of valid AN */
	nveu32_t an_valid;
	/** PN window */
	nveu32_t pn_window;
	/** SC LUT index */
	nveu32_t sc_idx_start;
	/** flags - encoding various states of SA */
	nveu32_t flags;
};

/**
 * @brief MACSEC HW controller LUT's global status
 */
struct osi_macsec_lut_status {
	/** List of max SC's supported */
	struct osi_macsec_sc_info sc_info[OSI_MAX_NUM_SC];
	/** next available BYP LUT index */
	nveu16_t next_byp_idx;
	/** number of active SCs */
	nveu32_t num_of_sc_used;
};

/**
 * @brief MACsec interrupt stats structure.
 */
struct osi_macsec_irq_stats {
	/** Tx debug buffer capture done */
	nveu64_t tx_dbg_capture_done;
	/** Tx MTU check failed */
	nveu64_t tx_mtu_check_fail;
	/** Tx MAC CRC err */
	nveu64_t tx_mac_crc_error;
	/** Tx SC AN not valid */
	nveu64_t tx_sc_an_not_valid;
	/** Tx AES GCM buffer overflow */
	nveu64_t tx_aes_gcm_buf_ovf;
	/** Tx LUT lookup miss */
	nveu64_t tx_lkup_miss;
	/** Tx uninitialized key slot */
	nveu64_t tx_uninit_key_slot;
	/** Tx PN threshold reached */
	nveu64_t tx_pn_threshold;
	/** Tx PN exhausted */
	nveu64_t tx_pn_exhausted;
	/** Tx debug buffer capture done */
	nveu64_t rx_dbg_capture_done;
	/** Rx ICV error threshold */
	nveu64_t rx_icv_err_threshold;
	/** Rx replay error */
	nveu64_t rx_replay_error;
	/** Rx MTU check failed */
	nveu64_t rx_mtu_check_fail;
	/** Rx MAC CRC err */
	nveu64_t rx_mac_crc_error;
	/** Rx AES GCM buffer overflow */
	nveu64_t rx_aes_gcm_buf_ovf;
	/** Rx LUT lookup miss */
	nveu64_t rx_lkup_miss;
	/** Rx uninitialized key slot */
	nveu64_t rx_uninit_key_slot;
	/** Rx PN exhausted */
	nveu64_t rx_pn_exhausted;
	/** Secure reg violation */
	nveu64_t secure_reg_viol;
};
#endif /* MACSEC_SUPPORT */

/**
 * @brief FRP Instruction configuration structure
 */
struct osi_core_frp_data {
	/** Entry Match Data */
	nveu32_t match_data;
	/** Entry Match Enable mask */
	nveu32_t match_en;
	/** Entry Accept frame flag */
	nveu8_t accept_frame;
	/** Entry Reject Frame flag */
	nveu8_t reject_frame;
	/** Entry Inverse match flag */
	nveu8_t inverse_match;
	/** Entry Next Instruction Control match flag */
	nveu8_t next_ins_ctrl;
	/** Entry Frame offset in the packet data */
	nveu8_t frame_offset;
	/** Entry OK Index - Next Instruction */
	nveu8_t ok_index;
	/** Entry DMA Channel selection (1-bit for each channel) */
	nveu32_t dma_chsel;
};

/**
 * @brief FRP Instruction table entry configuration structure
 */
struct osi_core_frp_entry {
	/** FRP ID */
	nve32_t frp_id;
	/** FRP Entry data structure */
	struct osi_core_frp_data data;
};

/**
 * @brief Core time stamp data strcuture
 */
struct osi_core_tx_ts {
	/** Pointer to next item in the link */
	struct osi_core_tx_ts *next;
	/** Pointer to prev item in the link */
	struct osi_core_tx_ts *prev;
	/** Packet ID for corresponding timestamp */
	nveu32_t pkt_id;
	/** Time in seconds */
	nveu32_t sec;
	/** Time in nano seconds */
	nveu32_t nsec;
	/** Variable which says if pkt_id is in use or not */
	nveu32_t in_use;
};

/**
 * @brief OSI Core data structure for runtime commands.
 */
struct osi_ioctl {
	/* runtime command */
	nveu32_t cmd;
	/* u32 general argument 1 */
	nveu32_t arg1_u32;
	/* u32 general argument 2 */
	nveu32_t arg2_u32;
	/* u32 general argument 3 */
	nveu32_t arg3_u32;
	/* u32 general argument 4 */
	nveu32_t arg4_u32;
	/* u64 general argument 5 */
	nveul64_t arg5_u64;
	/* s32 general argument 6 */
	nve32_t arg6_32;
	/* u8 string pointer general argument 7 for string */
	nveu8_t *arg7_u8_p;
	/* s64 general argument 8 */
	nvel64_t arg8_64;
	/* L2 filter structure */
	struct osi_filter l2_filter;
	/*l3_l4 filter structure */
	struct osi_l3_l4_filter l3l4_filter;
	/* HW feature structure */
	struct osi_hw_features hw_feat;
	/** AVB structure */
	struct osi_core_avb_algorithm avb;
#ifndef OSI_STRIPPED_LIB
	/** VLAN filter structure */
	struct osi_vlan_filter vlan_filter;
	/** PTP offload config structure*/
	struct osi_pto_config pto_config;
	/** RXQ route structure */
	struct osi_rxq_route rxq_route;
#endif /* !OSI_STRIPPED_LIB */
	/** FRP structure */
	struct osi_core_frp_cmd frp_cmd;
	/** EST structure */
	struct osi_est_config est;
	/** FRP structure */
	struct osi_fpe_config fpe;
	/** PTP configuration settings */
	struct osi_ptp_config ptp_config;
	/** TX Timestamp structure */
	struct osi_core_tx_ts tx_ts;
	/** PTP TSC data */
	struct osi_core_ptp_tsc_data ptp_tsc;
};

/**
 * @brief core_padctrl - Struct used to eqos padctrl details.
 */
struct core_padctrl {
	/** Memory mapped base address of eqos padctrl registers */
	void *padctrl_base;
	/** EQOS_RD0_0 register offset */
	nveu32_t offset_rd0;
	/** EQOS_RD1_0 register offset */
	nveu32_t offset_rd1;
	/** EQOS_RD2_0 register offset */
	nveu32_t offset_rd2;
	/** EQOS_RD3_0 register offset */
	nveu32_t offset_rd3;
	/** RX_CTL_0 register offset */
	nveu32_t offset_rx_ctl;
	/** is pad calibration in progress */
	nveu32_t is_pad_cal_in_progress;
	/** This flag set/reset using priv ioctl and DT entry */
	nveu32_t pad_calibration_enable;
	/** Reg ETHER_QOS_AUTO_CAL_CONFIG_0[AUTO_CAL_PD_OFFSET] value */
	nveu32_t pad_auto_cal_pd_offset;
	/** Reg ETHER_QOS_AUTO_CAL_CONFIG_0[AUTO_CAL_PU_OFFSET] value */
	nveu32_t pad_auto_cal_pu_offset;
};

#ifdef HSI_SUPPORT
/**
 * @brief The OSI Core HSI private data structure.
 */
struct osi_hsi_data {
	/** Indicates if HSI feature is enabled */
	nveu32_t enabled;
	/** time threshold to report error */
	nveu32_t err_time_threshold;
	/** error count threshold to report error  */
	nveu32_t err_count_threshold;
	/** HSI reporter ID */
	nveu16_t reporter_id;
	/** HSI error codes */
	nveu32_t err_code[OSI_HSI_MAX_MAC_ERROR_CODE];
	/** HSI MAC report count threshold based error */
	nveu32_t report_count_err[OSI_HSI_MAX_MAC_ERROR_CODE];
	/** Indicates if error reporting to FSI is pending */
	nveu32_t report_err;
	/** HSI MACSEC error codes */
	nveu32_t macsec_err_code[HSI_MAX_MACSEC_ERROR_CODE];
	/** HSI MACSEC report error based on count threshold */
	nveu32_t macsec_report_count_err[HSI_MAX_MACSEC_ERROR_CODE];
	/** Indicates if error report to FSI is pending for MACSEC*/
	nveu32_t macsec_report_err;
	/** RX CRC error report count */
	nveu64_t rx_crc_err_count;
	/** RX Checksum error report count */
	nveu64_t rx_checksum_err_count;
	/** MACSEC RX CRC error report count */
	nveu64_t macsec_rx_crc_err_count;
	/** MACSEC TX CRC error report count */
	nveu64_t macsec_tx_crc_err_count;
	/** MACSEC RX ICV error report count */
	nveu64_t macsec_rx_icv_err_count;
	/** HW correctable error count */
	nveu64_t ce_count;
	/** HW correctable error count hit threshold limit */
	nveu64_t ce_count_threshold;
	/** tx frame error count */
	nveu64_t tx_frame_err_count;
	/** tx frame error count threshold hit */
	nveu64_t tx_frame_err_threshold;
	/** Rx UDP error injection count */
	nveu64_t inject_udp_err_count;
	/** Rx CRC error injection count */
	nveu64_t inject_crc_err_count;
};
#endif

/**
 * @brief The OSI Core (MAC & MTL) private data structure.
 */
struct osi_core_priv_data {
	/** Memory mapped base address of MAC IP */
	void *base;
	/** Memory mapped base address of DMA window of MAC IP */
	void *dma_base;
	/** Memory mapped base address of XPCS IP */
	void *xpcs_base;
	/** Memory mapped base address of MACsec IP */
	void *macsec_base;
#ifdef MACSEC_SUPPORT
	/** Memory mapped base address of MACsec TZ page */
	void *tz_base;
	/** Address of MACsec HW operations structure */
	struct osi_macsec_core_ops *macsec_ops;
	/** Instance of macsec interrupt stats structure */
	struct osi_macsec_irq_stats macsec_irq_stats;
	/** Instance of macsec HW controller Tx/Rx LUT status */
	struct osi_macsec_lut_status macsec_lut_status[OSI_NUM_CTLR];
	/** macsec mmc counters */
	struct osi_macsec_mmc_counters macsec_mmc;
	/** MACSEC enabled state */
	nveu32_t is_macsec_enabled;
	/** macsec_fpe_lock used to exclusively configure either macsec
	 * or fpe config due to bug 3484034 */
	nveu32_t macsec_fpe_lock;
	/** FPE HW configuration initited to enable/disable
	 * 1- FPE HW configuration initiated to enable
	 * 0- FPE HW configuration initiated to disable */
	nveu32_t is_fpe_enabled;
#endif /* MACSEC_SUPPORT */
	/** Pointer to OSD private data structure */
	void *osd;
	/** OSD callback ops structure */
	struct osd_core_ops osd_ops;
	/** Number of MTL queues enabled in MAC */
	nveu32_t num_mtl_queues;
	/** Array of MTL queues */
	nveu32_t mtl_queues[OSI_MGBE_MAX_NUM_CHANS];
	/** List of MTL Rx queue mode that need to be enabled */
	nveu32_t rxq_ctrl[OSI_MGBE_MAX_NUM_CHANS];
	/** Rx MTl Queue mapping based on User Priority field */
	nveu32_t rxq_prio[OSI_MGBE_MAX_NUM_CHANS];
	/** MAC HW type EQOS based on DT compatible */
	nveu32_t mac;
	/** MAC version */
	nveu32_t mac_ver;
	/** HW supported feature list */
	struct osi_hw_features *hw_feat;
	/** MTU size */
	nveu32_t mtu;
	/** Ethernet MAC address */
	nveu8_t mac_addr[OSI_ETH_ALEN];
	/** Current flow control settings */
	nveu32_t flow_ctrl;
	/** PTP configuration settings */
	struct osi_ptp_config ptp_config;
	/** Default addend value */
	nveu32_t default_addend;
	/** mmc counter structure */
	struct osi_mmc_counters mmc;
	/** DMA channel selection enable (1) */
	nveu32_t dcs_en;
	/** TQ:TC mapping */
	nveu32_t tc[OSI_MGBE_MAX_NUM_CHANS];
#ifndef OSI_STRIPPED_LIB
	/** Memory mapped base address of HV window */
	void *hv_base;
	/** csr clock is to program LPI 1 us tick timer register.
	 * Value stored in MHz
	 */
	nveu32_t csr_clk_speed;
	nveu64_t vf_bitmap;
	/** Array to maintain VLAN filters */
	nveu16_t vid[VLAN_NUM_VID];
	/** Count of number of VLAN filters in vid array */
	nveu16_t vlan_filter_cnt;
	/** RSS core structure */
	struct osi_core_rss rss;
	/** DT entry to enable(1) or disable(0) pause frame support */
	nveu32_t pause_frames;
#endif
	/** Residual queue valid with FPE support */
	nveu32_t residual_queue;
	/** FRP Instruction Table */
	struct osi_core_frp_entry frp_table[OSI_FRP_MAX_ENTRY];
	/** Number of valid Entries in the FRP Instruction Table */
	nveu32_t frp_cnt;
	/* Switch to Software Owned List Complete.
	 *  1 - Successful and User configured GCL in placed
	 */
	nveu32_t est_ready;
	/* FPE enabled, verify and respose done with peer device
	 * 1- Successful and can be used between P2P device
	 */
	nveu32_t fpe_ready;
	/** MAC stats counters */
	struct osi_stats stats;
	/** eqos pad control structure */
	struct core_padctrl padctrl;
	/** MDC clock rate */
	nveu32_t mdc_cr;
	/** VLAN tag stripping enable(1) or disable(0) */
	nveu32_t strip_vlan_tag;
	/** L3L4 filter bit bask, set index corresponding bit for
	 * filter if filter enabled */
	nveu32_t l3l4_filter_bitmask;
	/** Flag which decides virtualization is enabled(1) or disabled(0) */
	nveu32_t use_virtualization;
	/** HW supported feature list */
	struct osi_hw_features *hw_feature;
	/** MC packets Multiple DMA channel selection flags */
	nveu32_t mc_dmasel;
	/** UPHY GBE mode (1 for 10G, 0 for 5G) */
	nveu32_t uphy_gbe_mode;
	/** Array of VM IRQ's */
	struct osi_vm_irq_data irq_data[OSI_MAX_VM_IRQS];
	/** number of VM IRQ's */
	nveu32_t num_vm_irqs;
	/** PHY interface mode (0/1 for XFI 10/5G, 2/3 for USXGMII 10/5) */
	nveu32_t phy_iface_mode;
	/** MGBE MAC instance ID's */
	nveu32_t instance_id;
	/** Ethernet controller MAC to MAC Time sync role
	 * 1 - Primary interface, 2 - secondary interface, 0 - inactive interface
	 */
	nveu32_t m2m_role;
	/** control pps output signal
	 */
	nveu32_t pps_frq;
#ifdef HSI_SUPPORT
	struct osi_hsi_data hsi;
#endif
};

/**
 * @brief osi_hw_core_init - EQOS MAC, MTL and common DMA initialization.
 *
 * @note
 * Algorithm:
 *  - Invokes EQOS MAC, MTL and common DMA register init code.
 *
 * @param[in, out] osi_core: OSI core private data structure.
 *
 * @pre
 * - MAC should be out of reset. See osi_poll_for_mac_reset_complete()
 *   for details.
 * - osi_core->base needs to be filled based on ioremap.
 * - osi_core->num_mtl_queues needs to be filled.
 * - osi_core->mtl_queues[qinx] need to be filled.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETRM_006
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_hw_core_init(struct osi_core_priv_data *const osi_core);

/**
 * @brief osi_hw_core_deinit - EQOS MAC deinitialization.
 *
 * @note
 * Algorithm:
 *  - Stops MAC transmission and reception.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MAC has to be out of reset.
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETRM_007
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: No
 *  - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_hw_core_deinit(struct osi_core_priv_data *const osi_core);

/**
 * @brief osi_write_phy_reg - Write to a PHY register through MAC over MDIO bus.
 *
 * @note
 * Algorithm:
 * - Before proceeding for reading for PHY register check whether any MII
 *   operation going on MDIO bus by polling MAC_GMII_BUSY bit.
 * - Program data into MAC MDIO data register.
 * - Populate required parameters like phy address, phy register etc,,
 *   in MAC MDIO Address register. write and GMII busy bits needs to be set
 *   in this operation.
 * - Write into MAC MDIO address register poll for GMII busy for MDIO
 *   operation to complete.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be write to PHY.
 * @param[in] phydata: Data to write to a PHY register.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETRM_002
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_write_phy_reg(struct osi_core_priv_data *const osi_core,
			  const nveu32_t phyaddr, const nveu32_t phyreg,
			  const nveu16_t phydata);

/**
 * @brief osi_read_phy_reg - Read from a PHY register through MAC over MDIO bus.
 *
 * @note
 * Algorithm:
 *  - Before proceeding for reading for PHY register check whether any MII
 *    operation going on MDIO bus by polling MAC_GMII_BUSY bit.
 *  - Populate required parameters like phy address, phy register etc,,
 *    in program it in MAC MDIO Address register. Read and GMII busy bits
 *    needs to be set in this operation.
 *  - Write into MAC MDIO address register poll for GMII busy for MDIO
 *    operation to complete. After this data will be available at MAC MDIO
 *    data register.
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be read from PHY.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETRM_003
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval data from PHY register on success
 * @retval -1 on failure
 */
nve32_t osi_read_phy_reg(struct osi_core_priv_data *const osi_core,
			 const nveu32_t phyaddr,
			 const nveu32_t phyreg);

/**
 * @brief initializing the core operations
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval data from PHY register on success
 * @retval -1 on failure
 *
 * @note
 * Traceability Details:
 * - SWUD_ID: ETHERNET_NVETHERNETRM_001
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 */
nve32_t osi_init_core_ops(struct osi_core_priv_data *const osi_core);

/**
 * @brief osi_handle_ioctl - API to handle runtime command
 *
 * @note
 * Algorithm:
 *  - Handle runtime commands to OSI
 *  - OSI_CMD_MDC_CONFIG
 *	Derive MDC clock based on provided AXI_CBB clk
 *	arg1_u32 - CSR (AXI CBB) clock rate.
 *  - OSI_CMD_POLL_FOR_MAC_RST
 *	Poll Software reset bit in MAC HW
 *  - OSI_CMD_COMMON_ISR
 *	Common ISR handler
 *  - OSI_CMD_PAD_CALIBRATION
 *	PAD calibration
 *  - OSI_CMD_READ_MMC
 *	invoke function to read actual registers and update
 *     structure variable mmc
 *  - OSI_CMD_GET_MAC_VER
 *	Reading MAC version
 *	arg1_u32 - holds mac version
 *  - OSI_CMD_RESET_MMC
 *	invoke function to reset MMC counter and data
 *        structure
 *  - OSI_CMD_MAC_LB
 *	Configure MAC loopback
 *  - OSI_CMD_FLOW_CTRL
 *	Configure flow control settings
 *	arg1_u32 - Enable or disable flow control settings
 *  - OSI_CMD_SET_MODE
 *	Set Full/Half Duplex mode.
 *	arg1_u32 - mode
 *  - OSI_CMD_SET_SPEED
 *	Set Operating speed
 *	arg1_u32 - Operating speed
 *  - OSI_CMD_L2_FILTER
 *	configure L2 mac filter
 *	l2_filter_struct - OSI filter structure
 *  - OSI_CMD_RXCSUM_OFFLOAD
 *	Configure RX checksum offload in MAC
 *	arg1_u32 - enable(1)/disable(0)
 *  - OSI_CMD_ADJ_FREQ
 *	Adjust frequency
 *	arg6_u32 - Parts per Billion
 *  - OSI_CMD_ADJ_TIME
 *	Adjust MAC time with system time
 *	arg1_u32 - Delta time in nano seconds
 *  - OSI_CMD_CONFIG_PTP
 *	Configure PTP
 *	arg1_u32 - Enable(1) or disable(0) Time Stamping
 *  - OSI_CMD_GET_AVB
 *	Get CBS algo and parameters
 *	avb_struct -  osi core avb data structure
 *  - OSI_CMD_SET_AVB
 *	Set CBS algo and parameters
 *	avb_struct -  osi core avb data structure
 *  - OSI_CMD_CONFIG_RX_CRC_CHECK
 *	Configure CRC Checking for Received Packets
 *	arg1_u32 - Enable or disable checking of CRC field in
 *	received pkts
 *  - OSI_CMD_UPDATE_VLAN_ID
 *	invoke osi call to update VLAN ID
 *	arg1_u32 - VLAN ID
 *  - OSI_CMD_CONFIG_TXSTATUS
 *	Configure Tx packet status reporting
 *	Enable(1) or disable(0) tx packet status reporting
 *  - OSI_CMD_GET_HW_FEAT
 *	Reading MAC HW features
 *	hw_feat_struct - holds the supported features of the hardware
 *  - OSI_CMD_CONFIG_FW_ERR
 *	Configure forwarding of error packets
 *	arg1_u32 - queue index, Max OSI_EQOS_MAX_NUM_QUEUES
 *	arg2_u32 - FWD error enable(1)/disable(0)
 *  - OSI_CMD_ARP_OFFLOAD
 *	Configure ARP offload in MAC
 *	arg1_u32 - Enable/disable flag
 *	arg7_u8_p - Char array representation of IP address
 *  - OSI_CMD_VLAN_FILTER
 *	OSI call for configuring VLAN filter
 *	vlan_filter - vlan filter structure
 *  - OSI_CMD_CONFIG_EEE
 *	Configure EEE LPI in MAC
 *	arg1_u32 - Enable (1)/disable (0) tx lpi
 *	arg2_u32 - Tx LPI entry timer in usecs upto
 *		   OSI_MAX_TX_LPI_TIMER (in steps of 8usec)
 *  - OSI_CMD_L3L4_FILTER
 *	invoke OSI call to add L3/L4
 *	l3l4_filter - l3_l4 filter structure
 *	arg1_u32 - L3 filter (ipv4(0) or ipv6(1))
 *            or L4 filter (tcp(0) or udp(1)
 *	arg2_u32 - filter based dma routing enable(1)
 *	arg3_u32 - dma channel for routing based on filter.
 *		   Max OSI_EQOS_MAX_NUM_CHANS.
 *	arg4_u32 - API call for L3 filter(0) or L4 filter(1)
 *  - OSI_CMD_SET_SYSTOHW_TIME
 *	set system to MAC hardware
 *	arg1_u32 - sec
 *	arg1_u32 - nsec
 *  - OSI_CMD_CONFIG_PTP_OFFLOAD
 *	enable/disable PTP offload feature
 *	pto_config - ptp offload structure
 *  - OSI_CMD_PTP_RXQ_ROUTE
 *	rxq routing to secific queue
 *	rxq_route - rxq routing information in structure
 *  - OSI_CMD_CONFIG_FRP
 *	Issue FRP command to HW
 *	frp_cmd - FRP command parameter
 *  - OSI_CMD_CONFIG_RSS
 *	Configure RSS
 *  - OSI_CMD_CONFIG_EST
 *	Configure EST registers and GCL to hw
 *	est - EST configuration structure
 *  - OSI_CMD_CONFIG_FPE
 *	Configuration FPE register and preemptable queue
 *	fpe - FPE configuration structure
 *
 *  - OSI_CMD_GET_TX_TS
 *	Command to get TX timestamp for PTP packet
 *	ts - OSI core timestamp structure
 *
 *  - OSI_CMD_FREE_TS
 *	Command to free old timestamp for PTP packet
 *	chan - DMA channel number +1. 0 will be used for onestep
 *
 *  - OSI_CMD_CAP_TSC_PTP
 *      Capture TSC and PTP time stamp
 *      ptp_tsc_data - output structure with time
 *
 *  - OSI_CMD_CONF_M2M_TS
 *	Enable/Disable MAC to MAC time sync for Secondary interface
 *	enable_disable - 1 - enable, 0- disable
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] data: void pointer pointing to osi_ioctl
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * Traceability Details:
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: No
 *  - Run time: Yes
 *  - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t osi_handle_ioctl(struct osi_core_priv_data *osi_core,
			 struct osi_ioctl *data);

/**
 * @brief osi_get_core - Get pointer to osi_core data structure.
 *
 * @note
 * Algorithm:
 *  - Returns OSI core data structure.
 *
 * @pre OSD layer should use this as first API to get osi_core pointer and
 * use the same in remaning API invocation.
 *
 * @note
 * Traceability Details:
 *
 * @usage
 * - Allowed context for the API call
 *  - Interrupt handler: No
 *  - Signal handler: No
 *  - Thread safe: No
 *  - Async/Sync: Sync
 *  - Required Privileges: None
 * - API Group:
 *  - Initialization: Yes
 *  - Run time: No
 *  - De-initialization: No
 *
 * @retval valid and unique osi_core pointer on success
 * @retval NULL on failure.
 */
struct osi_core_priv_data *osi_get_core(void);
#endif /* INCLUDED_OSI_CORE_H */
