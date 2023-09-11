/*
 * Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file pva-sys-dma.h
 *
 * @brief Types and constants related to PVA DMA setup and DMA
 * descriptors.
 */

#ifndef PVA_SYS_DMA_H
#define PVA_SYS_DMA_H

#include <pva-types.h>
#include <pva-bit.h>
#include <pva-packed.h>

/*** Version number of the current DMA info structure */
#define PVA_DMA_INFO_VERSION_ID (1U)

/** @brief DMA channels for a VPU app.
 *
 * The DMA channel structure contains the set-up of a PVA DMA channel
 * used by the VPU app.
 */
struct PVA_PACKED pva_dma_ch_config_s {
	/**< HW channel number. Zero if this config is unused. */
	uint32_t ch_number;
	/**< DMA CH_CNTL0 register. */
	uint32_t cntl0;
	/**< DMA CH_CNTL1 register. */
	uint32_t cntl1;
	/**< Boundary pad register. */
	uint32_t boundary_pad;
	/**< HWSEQ control register, Ignored on t19x. */
	uint32_t hwseqcntl;
	 /**< HWSEQ Frame Seq control register, Ignored on t19x and t23x. */
	uint32_t hwseqfscntl;
	uint32_t pad_dma_channel0[2];
};

/** Number of dma done masks in DMA info structure. */
#define PVA_SYS_DMA_NUM_TRIGGERS (9U)
/** Number of DMA channel configurations in DMA info structure. */
#define PVA_SYS_DMA_NUM_CHANNELS (15U)
/** Maximum number of DMA descriptors allowed. */
#define PVA_SYS_DMA_MAX_DESCRIPTORS (60U)

/** @brief DMA info for a VPU app.
 *
 * The DMA info contains the set-up of a PVA DMA engine for a VPU app.
 */
struct PVA_PACKED pva_dma_info_s {
	/**< size of this structure */
	uint16_t dma_info_size;
	/**< PVA_DMA_INFO_VERSION_ID */
	uint16_t dma_info_version;
	/**< Number of used channels */
	uint8_t num_channels;
	/**< Number of used descriptors*/
	uint8_t num_descriptors;
#ifdef SYSTEM_TESTS_ENABLED
	uint16_t r5_channel_mask; /**< channel is used by R5*/
#endif
	/**< Number of bytes used in hwseq */
	uint16_t num_hwseq;

	/*
	 *  * <First descriptor ID used.
	 * Valid range:  [1,PVA_SYS_DMA_MAX_DESCRIPTORS]
	 */
	uint8_t descriptor_id;
#ifndef SYSTEM_TESTS_ENABLED
	uint8_t pva_dma_info_pad_0[3]; /**< Padding for alignment. */
#else
	uint8_t special_access; /**< Padding for alignment. */
	uint32_t r5_descriptor_mask[2];
#endif

	/**@brief DMA done triggers used by the VPU app.
	 * Correspond to COMMON_DMA_OUTPUT_ENABLE registers.
	 */
	uint32_t dma_triggers[PVA_SYS_DMA_NUM_TRIGGERS];
	/** DMA channel config used by the VPU app. */
	struct pva_dma_ch_config_s dma_channels[PVA_SYS_DMA_NUM_CHANNELS];
	/** DMA common config used by the VPU app. */
	uint32_t dma_common_config;

	/** IOVA to an array of struct pva_dtd_s, aligned at 64 bytes */
	pva_iova dma_descriptor_base;
	/** IOVA to hwseq */
	pva_iova dma_hwseq_base;
	/** IOVA to MISR data (used by BIST/PFSD tests). */
	pva_iova dma_misr_base;
};

/**
 * @brief DMA descriptor.
 *
 * PVA DMA Descriptor in packed HW format.
 */
struct PVA_PACKED pva_dtd_s {
	uint8_t transfer_control0;
	uint8_t link_did;
	uint8_t src_adr1;
	uint8_t dst_adr1;
	uint32_t src_adr0;
	uint32_t dst_adr0;
	uint16_t tx;
	uint16_t ty;
	uint16_t slp_adv;
	uint16_t dlp_adv;
	/** SRC PT1 CNTL has st1_adv in low 24 bits
	 * and ns_adv in high 8 bits.
	 */
	uint32_t srcpt1_cntl;
	/** DST PT1 CNTL has dt1_adv in low 24 bits
	 * and nd1_adv in high 8 bits.
	 */
	uint32_t dstpt1_cntl;
	/** SRC PT2 CNTL has st2_adv in low 24 bits
	 * and ns2_adv in high 8 bits.
	 */
	uint32_t srcpt2_cntl;
	/** DST PT2 CNTL has dt2_adv in low 24 bits
	 * and nd2_adv in high 8 bits.
	 */
	uint32_t dstpt2_cntl;
	/** SRC PT3 CNTL has st3_adv in low 24 bits
	 * and ns3_adv in high 8 bits.
	 */
	uint32_t srcpt3_cntl;
	/** DST PT3 CNTL has dt3_adv in low 24 bits
	 * and nd3_adv in high 8 bits.
	 */
	uint32_t dstpt3_cntl;
	uint16_t sb_start;
	uint16_t db_start;
	uint16_t sb_size;
	uint16_t db_size;
	uint16_t trig_ch_events;
	uint16_t hw_sw_trig_events;
	uint8_t px;
	uint8_t py;
	uint8_t transfer_control1;
	uint8_t transfer_control2;
	uint8_t cb_ext;
	uint8_t rsvd;
	uint16_t frda;
};

/**
 *
 * @brief DMA MISR configuration information. This information is used by R5
 * to program MISR registers if a task requests MISR computation on its
 * output DMA channels.
 *
 */
struct PVA_PACKED pva_dma_misr_config_s {
	/* Reference value for CRC computed on
	 * write addresses, i.e., MISR 1
	 */
	uint32_t ref_addr;
	/* Seed value for address CRC */
	uint32_t seed_crc0;
	/* Reference value for CRC computed on
	 * first 256-bits of AXI write data
	 */
	uint32_t ref_data_1;
	/* Seed value for write data CRC */
	uint32_t seed_crc1;
	/* Reference value for CRC computed on
	 * second 256-bits of AXI write data
	 */
	uint32_t ref_data_2;
	/*
	 * MISR timeout value configured in DMA common register
	 * @ref PVA_DMA_COMMON_MISR_ENABLE. Timeout is caclutated as
	 * number of AXI clock cycles.
	 */
	uint32_t misr_timeout;
};

/**
 * @defgroup PVA_DMA_TC0_BITS PVA Transfer Control 0 Bitfields
 * @{
 */
#define PVA_DMA_TC0_DSTM_SHIFT (0U)
#define PVA_DMA_TC0_DSTM_MASK (7U)

#define PVA_DMA_TC0_SRC_TF_SHIFT (3U)
#define PVA_DMA_TC0_SRC_TF_MASK (1U)

#define PVA_DMA_TC0_DDTM_SHIFT (4U)
#define PVA_DMA_TC0_DDTM_MASK (7U)

#define PVA_DMA_TC0_DST_TF_SHIFT (7U)
#define PVA_DMA_TC0_DST_TF_MASK (1U)
/** @} */

/**
 * @defgroup PVA_DMA_TM DMA Transfer Modes
 *
 * @{
 */
#define PVA_DMA_TM_INVALID (0U)
#define PVA_DMA_TM_MC (1U)
#define PVA_DMA_TM_VMEM (2U)
#define PVA_DMA_TM_CVNAS (3U)
#define PVA_DMA_TM_L2RAM (3U)
#define PVA_DMA_TM_TCM (4U)
#define PVA_DMA_TM_MMIO (5U)
#define PVA_DMA_TM_RSVD (6U)
#define PVA_DMA_TM_VPU (7U)
/** @} */

/**
 * @defgroup PVA_DMA_TF DMA Transfer Format
 * @{
 */
#define PVA_DMA_TF_PITCH_LINEAR (0U)
#define PVA_DMA_TF_BLOCK_LINEAR (1U)
/** @} */

/**
 * @defgroup PVA_DMA_TC1_BITS PVA Transfer Control 1 Bitfields
 * @{
 */
#define PVA_DMA_TC1_BPP_SHIFT (0U)
#define PVA_DMA_TC1_BPP_MASK (3U)

#define PVA_DMA_TC1_PXDIR_SHIFT (2U)
#define PVA_DMA_TC1_PXDIR_MASK (1U)

#define PVA_DMA_TC1_PYDIR_SHIFT (3U)
#define PVA_DMA_TC1_PYDIR_MASK (1U)

#define PVA_DMA_TC1_BPE_SHIFT (3U)
#define PVA_DMA_TC1_BPE_MASK (1U)

#define PVA_DMA_TC1_TTS_SHIFT (3U)
#define PVA_DMA_TC1_TTS_MASK (1U)

#define PVA_DMA_TC1_ITC_SHIFT (3U)
#define PVA_DMA_TC1_ITC_MASK (1U)

/** @} */

/**@defgroup PVA_DMA_BPP PVA DMA Bits per Pixel
 * @{
 */
#define PVA_DMA_BPP_INT8 (0U)
#define PVA_DMA_BPP_INT16 (1U)
#define PVA_DMA_BPP_INT32 (2U)
/** @} */

/**@defgroup PVA_DMA_PXDIR PVA DMA Pad X direction
 * @{
 */
#define PVA_DMA_PXDIR_LEFT (0U)
#define PVA_DMA_PXDIR_RIGHT (1U)
/** @} */

/**@defgroup PVA_DMA_PYDIR PVA DMA Pad Y direction
 * @{
 */
#define PVA_DMA_PYDIR_TOP (0U)
#define PVA_DMA_PYDIR_BOT (1U)
/** @} */

/**@defgroup PVA_DMA_TTS PVA DMA TCM Transfer Size
 * @{
 */
#define PVA_DMA_TTS_4B (0U)
#define PVA_DMA_TTS_8B (1U)
/** @} */

/**@defgroup PVA_DMA_BPE PVA DMA Boundary Pixel Extension
 * @{
 */
#define PVA_DMA_BPE_DISABLE (0U)
#define PVA_DMA_BPE_ENABLE (1U)
/** @} */

/**@defgroup PVA_DMA_ITC VPU and Channel trigger
 * Intermediate Transfer Completion
 * @{
 */
#define PVA_DMA_ITC_DISABLE (0U)
#define PVA_DMA_ITC_ENABLE (1U)
/** @} */

/**
 * @defgroup PVA_DMA_TC2_BITS PVA DMA Transfer Control 2 Bitfields
 * @{
 */
#define PVA_DMA_TC2_PREFEN_SHIFT (0U)
#define PVA_DMA_TC2_PREFEN_MASK (1U)

#define PVA_DMA_TC2_DCBM_SHIFT (1U)
#define PVA_DMA_TC2_DCBM_MASK (1U)

#define PVA_DMA_TC2_SCBM_SHIFT (2U)
#define PVA_DMA_TC2_SCBM_MASK (1U)

#define PVA_DMA_TC2_SBADR_SHIFT (3U)
#define PVA_DMA_TC2_SBADR_MASK (31U)
/** @} */

/**@defgroup PVA_DMA_PREFETCH PVA DMA Prefetch
 * @{
 */
#define PVA_DMA_PREFETCH_DISABLE (0U)
#define PVA_DMA_PREFETCH_ENABLE (1U)

/**@defgroup PVA_DMA_CBM PVA DMA Circular Buffer Mode
 * @{
 */
#define PVA_DMA_CBM_DISABLE (0U)
#define PVA_DMA_CBM_ENABLE (1U)
/** @} */

#endif /* PVA_SYS_DMA_H */
