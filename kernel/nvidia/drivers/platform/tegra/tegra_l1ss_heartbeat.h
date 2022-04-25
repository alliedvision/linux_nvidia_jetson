/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file Heartbeat_Common.h
 * @brief <b>Tegra Heartbeat Common Header File</b>
 *
 * Type definitions, Macros and functions shared across all
 * Heartbeat units.
 */

#ifndef HEARTBEAT_COMMON_H
#define HEARTBEAT_COMMON_H

/*==================[Heartbeat Field Macros]==================================*/

/* Bit masks and shift for Heartbeat parameters */

/* HeartbeatPkt Byte 8 :CmdRespFrame_t.data[0]
 *   Bit 7 6 5 4 3 2 1 0
 *	 | | | | | | | |
 *	 | | | | | | Failed Layer ID
 *	 | | | | | |
 *	 | | | | Tegra Boot Status
 *	 | | | |
 *	 | | Tegra FuSa State
 *	 | |
 *	 HSM Reset Status
 */

#define FAILED_LAYER_ID_MASK	0x3U
#define FAILED_LAYER_ID_BYTE	0U

#define BOOT_STATUS_MASK		0x0CU
#define BOOT_STATUS_SHIFT	   2U
#define BOOT_STATUS_BYTE		0U

#define TEGRA_FUSA_STATE_MASK   0x30U
#define TEGRA_FUSA_STATE_SHIFT  4U
#define TEGRA_FUSA_STATE_BYTE   0U

#define HSM_RESET_STATUS_MASK   0xC0U
#define HSM_RESET_STATUS_SHIFT  6U
#define HSM_RESET_STATUS_BYTE   0U

/* HeartbeatPkt Byte 9 :CmdRespFrame_t.data[1]
 *   Bit 7 6 5 4 3 2 1 0
 *	 | | | | | | | |
 *	 | | | | Tegra 3LSS Phase
 *	 | | | |
 *	 Diagnostic Service Status
 */

#define TEGRA_3LSS_PHASE_MASK		   0x0FU
#define TEGRA_3LSS_PHASE_SHIFT		  0U
#define TEGRA_3LSS_PHASE_BYTE		   1U

#define DIAG_SERVICE_STATUS_MASK		0xF0U
#define DIAG_SERVICE_STATUS_SHIFT	   4U
#define DIAG_SERVICE_STATUS_BYTE		1U

/* HeartbeatPkt Byte 10 :CmdRespFrame_t.data[2]
 * SW Safety Error Status
 */
#define SW_ERROR_STATUS_BYTE	2U

/* HeartbeatPkt Byte 11 :CmdRespFrame_t.data[3]
 * HW Safety Error Status
 */
#define HW_ERROR_STATUS_BYTE	3U

/* HeartbeatPkt Byte 12, Byte 13 :CmdRespFrame_t.data[4], CmdRespFrame_t.data[5]
 * Monotonic count value
 */
#define MONOTONIC_COUNT_LB	  4U
#define MONOTONIC_COUNT_HB	  5U

/* HeartbeatPkt Byte 14 - Byte 17 :
 * CmdRespFrame_t.data[6] - CmdRespFrame_t.data[9]
 * Mission Data
 */
#define MISSION_DATA_BYTE	   6U
#define MISSION_DATA_MASK	   0x7FFFFFFFU

#define MISSION_DATA_VALID_MASK		 0x80U
#define MISSION_DATA_VALID_BYTE		 9U
#define MISSION_DATA_VALID_SHIFT		7U

/* HeartbeatPkt Byte 18 - Byte 25 :
 * CmdRespFrame_t.data[10] - CmdRespFrame_t.data[17]
 * Time Stamp
 */
#define TIMESTAMP_BYTE  10U

/* HeartbeatPkt Byte 59 - Byte 60:
 * CmdRespFrame_t.data[51] - CmdRespFrame_t.data[52]
 * Common dt configurations
 */
#define DIAG_PERIOD_BYTE		51U
#define SRV_DEADLINE_BYTE	   52U

/* HeartbeatPkt Byte 61 - Byte 63 :
 * CmdRespFrame_t.data[53] - CmdRespFrame_t.data[55]
 * L1SS SW Version
 */
#define MAJOR_VER_BYTE		  53U
#define MINOR_VER_BYTE		  54U
#define PATCH_VER_BYTE		  55U

/* Macros for Tegrta Boot Status */
#define BOOT_IN_PROGRESS		0U
#define BOOT_COMPLETED		  1U

/*==================[L2SS Heartbeat Config Macros]============================*/

/*==================[L1SS Heartbeat Config Macros]============================*/

/* =================[Type Definitions]======================================= */

/*==================[common local function definitions]=======================*/

static inline u8 l_set_hb_field(u8 byte, u8 mask, u8 shift,
				u8 val)
{
	u8 ret;

	ret = (((byte) & (~(mask))) |
			(u8)(((val) << shift) & (u8)U8_MAX)) & (u8)U8_MAX;
	return ret;
}

static inline void l_update_failed_layer_id(cmdresp_frame_t *hb, u8 layer_id)
{
	u8 val;

	if ((layer_id == NVGUARD_LAYER_1) || (layer_id == NVGUARD_LAYER_2)) {
		val = (((1U) << (layer_id - 1U)) & FAILED_LAYER_ID_MASK);
		hb->data[FAILED_LAYER_ID_BYTE] |= val;
	} else {
		/* Invalid Layer Id */
	}
}

static inline void l_update_ext_failed_layer_id(cmdresp_frame_ex_t *hb,
						u8 layer_id)
{
	u8 val;

	if ((layer_id == NVGUARD_LAYER_1) || (layer_id == NVGUARD_LAYER_2)) {
		val = (((1U) << (layer_id - 1U)) & FAILED_LAYER_ID_MASK);
		hb->data[FAILED_LAYER_ID_BYTE] |= val;
	} else {
		/* Invalid Layer Id */
	}
}

/* MACROS for SDL */
/* Return type for client functions integrated with NvGuard,
 * including error handlers, diagnostic tests and notification callbacks
 */
typedef u8 sdl_returntype_t;

/* NvGuard client return values of type 'sdl_returntype_t' */
/* Return value on successful execution of a client function.
 * Denotes error recovery in case of an error handler.
 * Indicates absence of faults in case of a diagnostic test
 */
#define SDL_E_OK		((sdl_returntype_t)(0x0U))
/* Informs client library not to report service status to 3LSS.
 * Client is expected report the status using 'NvGuard_ReportServiceStatus' API.
 * This is also Default value for NvGuard Diagnostic services.
 * Can be used by clients to initialize function return value
 */
#define SDL_E_PENDING		((sdl_returntype_t)(0x3U))
/* Return value when client function execution fails.
 * Denotes error confirmation in case of an error handler.
 * Indicates presence of faults in case of a diagnostic test
 */
#define SDL_E_NOK		((sdl_returntype_t)(0x6U))
/* Client function return value on receiving invalid parameters
 */
#define SDL_E_PARAM		((sdl_returntype_t)(0x9U))
/* Client function return value when pre-conditions for execution are not met
 */
#define SDL_E_PRECON		((sdl_returntype_t)(0xAU))

/*  Tegra phase values of type 'NvGuard_TegraPhase_t'.
 *  Argument for 'Phase_Notification' callback in 3LSS notification config
 *  structure. 3LSS framework maintains and synchronizes execution phases
 *  to orchestrate safe startup and safe shutdown across different layers.
 *  A client will receive notification for all phase transitions if
 *  callback is configured using 'NvGuard_Register3LSSNotification' API
 */
/** Defines Tegra phase during 3LSS initialization.
 */
#define NVGUARD_TEGRA_PHASE_INIT                    (0U)
/** Defines Tegra phase when 3LSS initialization is completed.
 */
#define NVGUARD_TEGRA_PHASE_INITDONE                (2U)
/** Defines Tegra phase in which periodic tests are triggered.
 */
#define NVGUARD_TEGRA_PHASE_RUN                     (4U)
/** Defines Tegra phase when 3LSS de-registers all clients.
 */
#define NVGUARD_TEGRA_PHASE_PRESHUTDOWN             (6U)
/** Defines Tegra phase after Tegra shutdown request to system manager.
 */
#define NVGUARD_TEGRA_PHASE_SHUTDOWN                (8U)

#endif /* HEARTBEAT_COMMON_H */
