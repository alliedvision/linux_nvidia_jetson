/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2022, NVIDIA Corporation.  All rights reserved.
 */

#ifndef INCLUDE_CAMRTC_DIAG_MESSAGES_H
#define INCLUDE_CAMRTC_DIAG_MESSAGES_H

#include <camrtc-common.h>
#include <camrtc-capture.h>
#include <camrtc-diag.h>

#pragma GCC diagnostic error "-Wpadded"

/**
 * @defgroup MessageType Message types for RCE diagnostics channel
 * @{
 */
#define CAMRTC_DIAG_ISP5_SDL_SETUP_REQ		MK_U32(0x01)
#define CAMRTC_DIAG_ISP5_SDL_SETUP_RESP		MK_U32(0x02)
#define CAMRTC_DIAG_ISP5_SDL_RELEASE_REQ	MK_U32(0x03)
#define CAMRTC_DIAG_ISP5_SDL_RELEASE_RESP	MK_U32(0x04)
#define CAMRTC_DIAG_ISP5_SDL_STATUS_REQ		MK_U32(0x05)
#define CAMRTC_DIAG_ISP5_SDL_STATUS_RESP	MK_U32(0x06)
/**@}*/

/**
 * @defgroup ResultCodes Diagnostics channel Result codes
 * @{
 */
#define CAMRTC_DIAG_SUCCESS			MK_U32(0x00)
#define CAMRTC_DIAG_ERROR_INVAL			MK_U32(0x01)
#define CAMRTC_DIAG_ERROR_NOTSUP		MK_U32(0x02)
#define CAMRTC_DIAG_ERROR_BUSY			MK_U32(0x03)
#define CAMRTC_DIAG_ERROR_TIMEOUT		MK_U32(0x04)
#define CAMRTC_DIAG_ERROR_UNKNOWN		MK_U32(0xFF)
/**@}*/

/**
 * @brief camrtc_diag_isp5_sdl_setup_req - ISP5 SDL periodic diagnostics setup request.
 *
 * Submit the pinned addresses of the ISP5 SDL test vectors binary to RCE to
 * enable periodic diagnostics.
 */
struct camrtc_diag_isp5_sdl_setup_req {
	/** Binary base address (RCE STREAMID). */
	iova_t rce_iova;
	/** Binary base address (ISP STREAMID). */
	iova_t isp_iova;
	/** Total size of the test binary. */
	uint32_t size;
	/** Period [ms] b/w diagnostics tests submitted in batch, zero for no repeat. */
	uint32_t period;
} CAMRTC_DIAG_IVC_ALIGN;

/**
 * @brief camrtc_diag_isp5_sdl_setup_resp - ISP5 PFSD setup response.
 *
 * Setup status is returned in the result field.
 */
struct camrtc_diag_isp5_sdl_setup_resp {
	/** SDL setup status returned to caller. */
	uint32_t result;
	/** Reserved. */
	uint32_t pad32_[1];
} CAMRTC_DIAG_IVC_ALIGN;

/**
 * @brief camrtc_diag_isp5_sdl_release_resp - ISP5 PFSD release response.
 *
 * Response status is returned in the result field.
 */
struct camrtc_diag_isp5_sdl_release_resp {
	/** SDL release status returned to caller. */
	uint32_t result;
	/** Explicit padding. */
	uint32_t pad32_[1];
} CAMRTC_DIAG_IVC_ALIGN;

/**
 * @brief ISP5 PFSD status response.
 *
 * Response status is returned in the result field. The status of test
 * scheduler and results from test runner are returned, too.
 */
struct camrtc_diag_isp5_sdl_status_resp {
	/** Status returned to requester. */
	uint32_t result;
	/** Nonzero if PFSD tests are being scheduled. */
	uint32_t running;
	/** Number of tests that have been scheduled. */
	uint64_t scheduled;
	/** Number of tests that have been executed. */
	uint64_t executed;
	/** Number of tests that have been passed. */
	uint64_t passed;
	/** Number of CRC failures in tests. (Counter stops at UINT32_MAX.) */
	uint32_t crc_failed;
	/** Explicit padding */
	uint32_t pad32_[1];
} CAMRTC_DIAG_IVC_ALIGN;

/**
 * @brief camrtc_diag_msg - Message definition for camrtc diagnostics
 */
struct camrtc_diag_msg {
	/** @ref MessageType "Message type". */
	uint32_t msg_type;
	/** ID associated w/ request. */
	uint32_t transaction_id;
	union {
		/** SDL setup request. */
		struct camrtc_diag_isp5_sdl_setup_req isp5_sdl_setup_req;
		/** Response to SDL setup request. */
		struct camrtc_diag_isp5_sdl_setup_resp isp5_sdl_setup_resp;
		/** Response to SDL release request. */
		struct camrtc_diag_isp5_sdl_release_resp isp5_sdl_release_resp;
		/** Response to SDL status query. */
		struct camrtc_diag_isp5_sdl_status_resp isp5_sdl_status_resp;
	};
} CAMRTC_DIAG_IVC_ALIGN;

#pragma GCC diagnostic ignored "-Wpadded"

#endif /* INCLUDE_CAMRTC_DIAG_MESSAGES_H */
