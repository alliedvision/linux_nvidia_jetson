/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file camrtc-channels.h
 *
 * @brief RCE channel setup tags & structures.
 */

#ifndef INCLUDE_CAMRTC_CHANNELS_H
#define INCLUDE_CAMRTC_CHANNELS_H

#include "camrtc-common.h"

/**
 * @defgroups RceTags RCE tags
 *
 * All the enums and the fields inside the structs described in this header
 * file supports only uintX_t types, where X can be 8,16,32,64.
 * @{
 */
#define CAMRTC_TAG64(s0, s1, s2, s3, s4, s5, s6, s7) ( \
	((uint64_t)(s0) << 0U) | ((uint64_t)(s1) << 8U) | \
	((uint64_t)(s2) << 16U) | ((uint64_t)(s3) << 24U) | \
	((uint64_t)(s4) << 32U) | ((uint64_t)(s5) << 40U) | \
	((uint64_t)(s6) << 48U) | ((uint64_t)(s7) << 56U))

#define CAMRTC_TAG_IVC_SETUP	CAMRTC_TAG64('I', 'V', 'C', '-', 'S', 'E', 'T', 'U')
#define CAMRTC_TAG_NV_TRACE	CAMRTC_TAG64('N', 'V', ' ', 'T', 'R', 'A', 'C', 'E')
#define CAMRTC_TAG_NV_CAM_TRACE	CAMRTC_TAG64('N', 'V', ' ', 'C', 'A', 'M', 'T', 'R')
#define CAMRTC_TAG_NV_COVERAGE	CAMRTC_TAG64('N', 'V', ' ', 'C', 'O', 'V', 'E', 'R')
/** }@ */

/**
 * @brief RCE Tag, length, and value (TLV)
 */
struct camrtc_tlv {
	/** Command tag. See @ref RceTags "RCE Tags" */
	uint64_t tag;
	/** Length of the tag specific data */
	uint64_t len;
};

/**
 * @brief Setup TLV for IVC
 *
 * Multiple setup structures can follow each other.
 */
struct camrtc_tlv_ivc_setup {
	/** Command tag. See @ref RceTags "RCE Tags" */
	uint64_t tag;
	/** Length of the tag specific data */
	uint64_t len;
	/** Base address of write header. RX from CCPLEX point of view */
	uint64_t rx_iova;
	/** Size of IVC write frame */
	uint32_t rx_frame_size;
	/** Number of IVC write frames */
	uint32_t rx_nframes;
	/** Base address of read header. TX from CCPLEX point of view */
	uint64_t tx_iova;
	/** Size of IVC read frame */
	uint32_t tx_frame_size;
	/** Number of IVC read frames */
	uint32_t tx_nframes;
	/** IVC channel group*/
	uint32_t channel_group;
	/** IVC version */
	uint32_t ivc_version;
	/** IVC service name */
	char ivc_service[32];
};

/**
 * @defgroup CamRTCChannelErrors Channel setup error codes
 * @{
 */
#define	RTCPU_CH_SUCCESS		MK_U32(0)
#define	RTCPU_CH_ERR_NO_SERVICE		MK_U32(128)
#define	RTCPU_CH_ERR_ALREADY		MK_U32(129)
#define	RTCPU_CH_ERR_UNKNOWN_TAG	MK_U32(130)
#define	RTCPU_CH_ERR_INVALID_IOVA	MK_U32(131)
#define	RTCPU_CH_ERR_INVALID_PARAM	MK_U32(132)
/* @} */

/**
 * @brief Code coverage memory header
 */
struct camrtc_coverage_memory_header {
	/** Code coverage tag. Should be CAMRTC_TAG_NV_COVERAGE */
	uint64_t signature;
	/** Size of camrtc_coverage_memory_header */
	uint64_t length;
	/** Header revision */
	uint32_t revision;
	/** Size of the coverage memory buffer */
	uint32_t coverage_buffer_size;
	/** Coverage data inside the memory buffer in bytes */
	uint32_t coverage_total_bytes;
	/** Reserved */
	uint32_t reserved;
};

#endif /* INCLUDE_CAMRTC_CHANNELS_H */
