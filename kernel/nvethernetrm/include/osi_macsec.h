/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
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

#ifndef INCLUDED_OSI_MACSEC_H
#define INCLUDED_OSI_MACSEC_H

#include <osi_core.h>

#ifdef MACSEC_SUPPORT
//////////////////////////////////////////////////////////////////////////
	/* MACSEC OSI data structures */
//////////////////////////////////////////////////////////////////////////

/**
 * @addtogroup TX/RX BYP/SCI LUT helpers macros
 *
 * @brief Helper macros for LUT programming
 * @{
 */
#define OSI_AN0_VALID			OSI_BIT(0)
#define OSI_AN1_VALID			OSI_BIT(1)
#define OSI_AN2_VALID			OSI_BIT(2)
#define OSI_AN3_VALID			OSI_BIT(3)
#define OSI_MAX_NUM_SA			4U
#ifdef DEBUG_MACSEC
#define OSI_CURR_AN_MAX 		3
#endif /* DEBUG_MACSEC */
#define OSI_KEY_INDEX_MAX		31U
#define OSI_PN_MAX_DEFAULT		0xFFFFFFFFU
#define OSI_PN_THRESHOLD_DEFAULT	0xC0000000U
#define OSI_TCI_DEFAULT 		0x1
#define OSI_VLAN_IN_CLEAR_DEFAULT	0x0
#define OSI_SC_INDEX_MAX		15U
#define OSI_ETHTYPE_LEN 		2
#define OSI_LUT_BYTE_PATTERN_MAX	4U
/* LUT byte pattern offset range 0-63 */
#define OSI_LUT_BYTE_PATTERN_MAX_OFFSET 63U
/* VLAN PCP range 0-7 */
#define OSI_VLAN_PCP_MAX		7U
/* VLAN ID range 1-4095 */
#define OSI_VLAN_ID_MAX 		4095U
#define OSI_LUT_SEL_BYPASS		0U
#define OSI_LUT_SEL_SCI 		1U
#define OSI_LUT_SEL_SC_PARAM		2U
#define OSI_LUT_SEL_SC_STATE		3U
#define OSI_LUT_SEL_SA_STATE		4U
#define OSI_LUT_SEL_MAX 		4U

/* LUT input fields flags bit offsets */
#define OSI_LUT_FLAGS_DA_BYTE0_VALID	OSI_BIT(0)
#define OSI_LUT_FLAGS_DA_BYTE1_VALID	OSI_BIT(1)
#define OSI_LUT_FLAGS_DA_BYTE2_VALID	OSI_BIT(2)
#define OSI_LUT_FLAGS_DA_BYTE3_VALID	OSI_BIT(3)
#define OSI_LUT_FLAGS_DA_BYTE4_VALID	OSI_BIT(4)
#define OSI_LUT_FLAGS_DA_BYTE5_VALID	OSI_BIT(5)
#define OSI_LUT_FLAGS_DA_VALID		(OSI_BIT(0) | OSI_BIT(1) | OSI_BIT(2) |\
					 OSI_BIT(3) | OSI_BIT(4) | OSI_BIT(5))
#define OSI_LUT_FLAGS_SA_BYTE0_VALID	OSI_BIT(6)
#define OSI_LUT_FLAGS_SA_BYTE1_VALID	OSI_BIT(7)
#define OSI_LUT_FLAGS_SA_BYTE2_VALID	OSI_BIT(8)
#define OSI_LUT_FLAGS_SA_BYTE3_VALID	OSI_BIT(9)
#define OSI_LUT_FLAGS_SA_BYTE4_VALID	OSI_BIT(10)
#define OSI_LUT_FLAGS_SA_BYTE5_VALID	OSI_BIT(11)
#define OSI_LUT_FLAGS_SA_VALID		(OSI_BIT(6) | OSI_BIT(7) | OSI_BIT(8) |\
					 OSI_BIT(9) | OSI_BIT(10) | OSI_BIT(11))
#define OSI_LUT_FLAGS_ETHTYPE_VALID	OSI_BIT(12)
#define OSI_LUT_FLAGS_VLAN_PCP_VALID	OSI_BIT(13)
#define OSI_LUT_FLAGS_VLAN_ID_VALID	OSI_BIT(14)
#define OSI_LUT_FLAGS_VLAN_VALID	OSI_BIT(15)
#define OSI_LUT_FLAGS_BYTE0_PATTERN_VALID	OSI_BIT(16)
#define OSI_LUT_FLAGS_BYTE1_PATTERN_VALID	OSI_BIT(17)
#define OSI_LUT_FLAGS_BYTE2_PATTERN_VALID	OSI_BIT(18)
#define OSI_LUT_FLAGS_BYTE3_PATTERN_VALID	OSI_BIT(19)
#define OSI_LUT_FLAGS_PREEMPT		OSI_BIT(20)
#define OSI_LUT_FLAGS_PREEMPT_VALID	OSI_BIT(21)
#define OSI_LUT_FLAGS_CONTROLLED_PORT	OSI_BIT(22)
#define OSI_LUT_FLAGS_DVLAN_PKT		OSI_BIT(23)
#define OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL	OSI_BIT(24)
#define OSI_LUT_FLAGS_ENTRY_VALID	OSI_BIT(31)
/** @} */

/**
 * @addtogroup MACSEC-Generic table CONFIG register helpers macros
 *
 * @brief Helper macros for generic table CONFIG register programming
 * @{
 */
#define OSI_CTLR_SEL_TX		0U
#define OSI_CTLR_SEL_RX		1U
#define OSI_CTLR_SEL_MAX	1U
#define OSI_LUT_READ		0U
#define OSI_LUT_WRITE		1U
#define OSI_RW_MAX		1U
#define OSI_TABLE_INDEX_MAX	31U
#define OSI_BYP_LUT_MAX_INDEX	OSI_TABLE_INDEX_MAX
#define OSI_SC_LUT_MAX_INDEX	15U
#define OSI_SA_LUT_MAX_INDEX	OSI_TABLE_INDEX_MAX
/** @} */

#ifdef DEBUG_MACSEC
/**
 * @addtogroup Debug buffer table CONFIG register helpers macros
 *
 * @brief Helper macros for debug buffer table CONFIG register programming
 * @{
 */
/* Num of Tx debug buffers */
#define OSI_TX_DBG_BUF_IDX_MAX		12U
/* Num of Rx debug buffers */
#define OSI_RX_DBG_BUF_IDX_MAX		13U
/** flag - encoding various debug event bits */
#define OSI_TX_DBG_LKUP_MISS_EVT	OSI_BIT(0)
#define OSI_TX_DBG_AN_NOT_VALID_EVT	OSI_BIT(1)
#define OSI_TX_DBG_KEY_NOT_VALID_EVT	OSI_BIT(2)
#define OSI_TX_DBG_CRC_CORRUPT_EVT	OSI_BIT(3)
#define OSI_TX_DBG_ICV_CORRUPT_EVT	OSI_BIT(4)
#define OSI_TX_DBG_CAPTURE_EVT		OSI_BIT(5)
#define OSI_RX_DBG_LKUP_MISS_EVT	OSI_BIT(6)
#define OSI_RX_DBG_KEY_NOT_VALID_EVT	OSI_BIT(7)
#define OSI_RX_DBG_REPLAY_ERR_EVT	OSI_BIT(8)
#define OSI_RX_DBG_CRC_CORRUPT_EVT	OSI_BIT(9)
#define OSI_RX_DBG_ICV_ERROR_EVT	OSI_BIT(10)
#define OSI_RX_DBG_CAPTURE_EVT		OSI_BIT(11)
/** @} */
#endif /* DEBUG_MACSEC*/

/**
 * @addtogroup AES ciphers
 *
 * @brief Helper macro's for AES ciphers
 * @{
 */
#define OSI_MACSEC_CIPHER_AES128	0U
#define OSI_MACSEC_CIPHER_AES256	1U
/** @} */

/**
 * @addtogroup MACSEC related helper MACROs
 *
 * @brief MACSEC generic helper MACROs
 * @{
 */
#define OSI_MACSEC_TX_EN	OSI_BIT(0)
#define OSI_MACSEC_RX_EN	OSI_BIT(1)
/** @} */

/**
 * @brief Indicates different operations on MACSEC SA
 */
#ifdef MACSEC_KEY_PROGRAM
#define OSI_CREATE_SA           1U
#endif /* MACSEC_KEY_PROGRAM */
#define OSI_ENABLE_SA           2U

/**
 * @brief MACSEC SA State LUT entry outputs structure
 */
struct osi_sa_state_outputs {
	/** Indicates next PN to use */
	nveu32_t next_pn;
	/** Indicates lowest PN to use */
	nveu32_t lowest_pn;
};

/**
 * @brief MACSEC SC State LUT entry outputs structure
 */
struct osi_sc_state_outputs {
	/** Indicates current AN to use */
	nveu32_t curr_an;
};

/**
 * @brief MACSEC SC Param LUT entry outputs structure
 */
struct osi_sc_param_outputs {
	/** Indicates Key index start */
	nveu32_t key_index_start;
	/** PN max for given AN, after which HW will roll over to next AN */
	nveu32_t pn_max;
	/** PN threshold to trigger irq when threshold is reached */
	nveu32_t pn_threshold;
	/** Indidate PN window for engress packets */
	nveu32_t pn_window;
	/** SC identifier */
	nveu8_t sci[OSI_SCI_LEN];
	/** Indicates SECTAG 3 TCI bits V, ES, SC
	 * Default TCI value V=1, ES=0, SC = 1
	 */
	nveu8_t tci;
	/** Indicates 1 bit VLAN IN CLEAR config */
	nveu8_t vlan_in_clear;
};

/**
 * @brief MACSEC SCI LUT entry outputs structure
 */
struct osi_sci_lut_outputs {
	/** Indicates SC index to use */
	nveu32_t sc_index;
	/** SC identifier */
	nveu8_t sci[OSI_SCI_LEN];
	/** AN's valid */
	nveu32_t an_valid;
};

/**
 * @brief MACSEC LUT config data structure
 */
struct osi_macsec_table_config {
	/** Indicates controller select, Tx=0, Rx=1 */
	nveu16_t ctlr_sel;
	/** Read or write operation select, Read=0, Write=1 */
	nveu16_t rw;
	/** LUT entry index */
	nveu16_t index;
};

#if defined(MACSEC_KEY_PROGRAM) || defined(LINUX_OS)
/**
 * @brief MACSEC Key Table entry structure
 */
struct osi_kt_entry {
	/** Indicates SAK key - max 256bit */
	nveu8_t sak[OSI_KEY_LEN_256];
	/** Indicates Hash-key */
	nveu8_t h[OSI_KEY_LEN_128];
};
#endif /* MACSEC_KEY_PROGRAM */

/**
 * @brief MACSEC BYP/SCI LUT entry inputs structure
 */
struct osi_lut_inputs {
	/** MAC DA to compare */
	nveu8_t da[OSI_ETH_ALEN];
	/** MAC SA to compare */
	nveu8_t sa[OSI_ETH_ALEN];
	/** Ethertype to compare */
	nveu8_t ethtype[OSI_ETHTYPE_LEN];
	/** 4-Byte pattern to compare */
	nveu8_t byte_pattern[OSI_LUT_BYTE_PATTERN_MAX];
	/** Offset for 4-Byte pattern to compare */
	nveu32_t byte_pattern_offset[OSI_LUT_BYTE_PATTERN_MAX];
	/** VLAN PCP to compare */
	nveu32_t vlan_pcp;
	/** VLAN ID to compare */
	nveu32_t vlan_id;
};

/**
 * @brief MACSEC LUT config data structure
 */
struct osi_macsec_lut_config {
	/** Generic table config */
	struct osi_macsec_table_config table_config;
	/** Indicates LUT to select
	 * 0: Bypass LUT
	 * 1: SCI LUT
	 * 2: SC PARAM LUT
	 * 3: SC STATE LUT
	 * 4: SA STATE LUT
	 */
	nveu16_t lut_sel;
	/** flag - encoding various valid LUT bits for above fields */
	nveu32_t flags;
	/** LUT inputs to use */
	struct osi_lut_inputs lut_in;
	/** SCI LUT outputs */
	struct osi_sci_lut_outputs sci_lut_out;
	/** SC Param LUT outputs */
	struct osi_sc_param_outputs sc_param_out;
	/** SC State LUT outputs */
	struct osi_sc_state_outputs sc_state_out;
	/** SA State LUT outputs */
	struct osi_sa_state_outputs sa_state_out;
};

#if defined(MACSEC_KEY_PROGRAM) || defined(LINUX_OS)
/**
 * @brief MACSEC Key Table config data structure
 */
struct osi_macsec_kt_config {
	/** Generic table config */
	struct osi_macsec_table_config table_config;
	/** Key Table entry config */
	struct osi_kt_entry entry;
	/** Indicates key table entry valid or not, bit 31 */
	nveu32_t flags;
};
#endif /* MACSEC_KEY_PROGRAM */

/**
 * @brief MACSEC Debug buffer config data structure
 */
struct osi_macsec_dbg_buf_config {
	/** Indicates Controller select, Tx=0, Rx=1 */
	nveu16_t ctlr_sel;
	/** Read or write operation select, Read=0, Write=1 */
	nveu16_t rw;
	/** Indicates debug data buffer */
	nveu32_t dbg_buf[4];
	/** flag - encoding various debug event bits */
	nveu32_t flags;
	/** Indicates debug buffer index */
	nveu32_t index;
};

/**
 * @brief MACSEC core operations structure
 */
struct osi_macsec_core_ops {
	/** macsec init */
	nve32_t (*init)(struct osi_core_priv_data *const osi_core,
			nveu32_t mtu);
	/** macsec de-init */
	nve32_t (*deinit)(struct osi_core_priv_data *const osi_core);
	/** Macsec irq handler */
	void (*handle_irq)(struct osi_core_priv_data *const osi_core);
	/** macsec lut config */
	nve32_t (*lut_config)(struct osi_core_priv_data *const osi_core,
			  struct osi_macsec_lut_config *const lut_config);
#ifdef MACSEC_KEY_PROGRAM
	/** macsec kt config */
	nve32_t (*kt_config)(struct osi_core_priv_data *const osi_core,
			 struct osi_macsec_kt_config *const kt_config);
#endif /* MACSEC_KEY_PROGRAM */
	/** macsec cipher config */
	nve32_t (*cipher_config)(struct osi_core_priv_data *const osi_core,
			     nveu32_t cipher);
#ifdef DEBUG_MACSEC
	/** macsec loopback config */
	nve32_t (*loopback_config)(struct osi_core_priv_data *const osi_core,
			       nveu32_t enable);
#endif /* DEBUG_MACSEC */
	/** macsec enable */
	nve32_t (*macsec_en)(struct osi_core_priv_data *const osi_core,
			 nveu32_t enable);
	/** macsec config SA in HW LUT */
	nve32_t (*config)(struct osi_core_priv_data *const osi_core,
		      struct osi_macsec_sc_info *const sc,
		      nveu32_t enable, nveu16_t ctlr,
		      nveu16_t *kt_idx);
	/** macsec read mmc counters */
	void (*read_mmc)(struct osi_core_priv_data *const osi_core);
#ifdef DEBUG_MACSEC
	/** macsec debug buffer config */
	nve32_t (*dbg_buf_config)(struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config);
	/** macsec debug buffer config */
	nve32_t (*dbg_events_config)(struct osi_core_priv_data *const osi_core,
		 struct osi_macsec_dbg_buf_config *const dbg_buf_config);
#endif /* DEBUG_MACSEC */
	/** macsec get Key Index start for a given SCI */
	nve32_t (*get_sc_lut_key_index)(struct osi_core_priv_data *const osi_core,
		 nveu8_t *sci, nveu32_t *key_index, nveu16_t ctlr);
	/** macsec set MTU size */
	nve32_t (*update_mtu)(struct osi_core_priv_data *const osi_core,
			      nveu32_t mtu);
#ifdef DEBUG_MACSEC
	/** macsec interrupts configuration */
	void (*intr_config)(struct osi_core_priv_data *const osi_core, nveu32_t enable);
#endif /* DEBUG_MACSEC */
};

//////////////////////////////////////////////////////////////////////////
	/* MACSEC OSI interface API prototypes */
//////////////////////////////////////////////////////////////////////////

/**
 * @brief osi_init_macsec_ops - macsec initialize operations
 *
 * @note
 * Algorithm:
 *  - If virtualization is enabled initialize virt ops
 *  - Else
 *    - If macsec base is null return -1
 *    - initialize with macsec ops
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_init_macsec_ops(struct osi_core_priv_data *const osi_core);

/**
 * @brief osi_macsec_init - Initialize the macsec controller
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Configure MTU, controller configs, interrupts, clear all LUT's and
 *    set BYP LUT entries for MKPDU and BC packets
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] mtu: mtu to be programmed
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_init(struct osi_core_priv_data *const osi_core,
			nveu32_t mtu);

/**
 * @brief osi_macsec_deinit - De-Initialize the macsec controller
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Resets macsec global data structured and restores the mac confirguration
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_deinit(struct osi_core_priv_data *const osi_core);

/**
 * @brief osi_macsec_isr - macsec irq handler
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - handles macsec interrupts
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval none
 */
void osi_macsec_isr(struct osi_core_priv_data *const osi_core);

/**
 * @brief osi_macsec_config_lut - Read or write to macsec LUTs
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Reads or writes to MACSEC LUTs
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[out] lut_config: Pointer to the lut configuration
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_config_lut(struct osi_core_priv_data *const osi_core,
			  struct osi_macsec_lut_config *const lut_config);

#ifdef MACSEC_KEY_PROGRAM
/**
 * @brief osi_macsec_config_kt - API to read or update the keys
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Read or write the keys
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] kt_config: Keys that needs to be programmed
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_config_kt(struct osi_core_priv_data *const osi_core,
			 struct osi_macsec_kt_config *const kt_config);
#endif /* MACSEC_KEY_PROGRAM */

/**
 * @brief osi_macsec_cipher_config - API to update the cipher
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Updates cipher to use
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] cipher: Cipher suit to be used
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_cipher_config(struct osi_core_priv_data *const osi_core,
				 nveu32_t cipher);

#ifdef DEBUG_MACSEC
/**
 * @brief osi_macsec_loopback - API to enable/disable macsec loopback
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Enables/disables macsec loopback
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] enable: parameter to enable or disable
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */

nve32_t osi_macsec_loopback(struct osi_core_priv_data *const osi_core,
			nveu32_t enable);
#endif /* DEBUG_MACSEC */

/**
 * @brief osi_macsec_en - API to enable/disable macsec
 *
 * @note
 * Algorithm:
 *  - Return -1 if passed enable param is invalid
 *  - Return -1 if osi core or ops is null
 *  - Enables/disables macsec
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] enable: parameter to enable or disable
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_en(struct osi_core_priv_data *const osi_core,
		  nveu32_t enable);

/**
 * @brief osi_macsec_config - Updates SC or SA in the macsec
 *
 * @note
 * Algorithm:
 *  - Return -1 if passed params are invalid
 *  - Return -1 if osi core or ops is null
 *  - Update/add/delete SC/SA
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] sc: Pointer to the sc that needs to be added/deleted/updated
 * @param[in] enable: macsec enable/disable selection
 * @param[in] ctlr: Controller selected
 * @param[out] kt_idx: Pointer to the kt_index passed to OSD
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_config(struct osi_core_priv_data *const osi_core,
		      struct osi_macsec_sc_info *const sc,
		      nveu32_t enable, nveu16_t ctlr,
		      nveu16_t *kt_idx);

/**
 * @brief osi_macsec_read_mmc - Updates the mmc counters
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Updates the mcc counters in osi_core structure
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[out] osi_core: OSI core private data structure
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_read_mmc(struct osi_core_priv_data *const osi_core);

#ifdef DEBUG_MACSEC
/**
 * @brief osi_macsec_config_dbg_buf - Reads the debug buffer captured
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Reads the dbg buffers captured
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[out] dbg_buf_config: dbg buffer data captured
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_config_dbg_buf(
		struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config);

/**
 * @brief osi_macsec_dbg_events_config - Enables debug buffer events
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Enables specific events to capture debug buffers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] dbg_buf_config: dbg buffer data captured
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_dbg_events_config(
		struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config);
#endif /* DEBUG_MACSEC */
/**
 * @brief osi_macsec_get_sc_lut_key_index - API to get key index for a given SCI
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - gets the key index for the given sci
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] sci: Pointer to sci that needs to be found
 * @param[out] key_index: Pointer to key_index
 * @param[in] ctlr: macsec controller selected
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_get_sc_lut_key_index(
		struct osi_core_priv_data *const osi_core,
		nveu8_t *sci, nveu32_t *key_index, nveu16_t ctlr);

/**
 * @brief osi_macsec_update_mtu - Update the macsec mtu in run-time
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Updates the macsec mtu
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] mtu: mtu that needs to be programmed
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_update_mtu(struct osi_core_priv_data *const osi_core,
			      nveu32_t mtu);

#endif /* MACSEC_SUPPORT */
#endif /* INCLUDED_OSI_MACSEC_H */
