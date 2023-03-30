/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2022, NVIDIA Corporation.  All rights reserved.
 */

/**
 * @file camrtc-capture-messages.h
 *
 * @brief Capture control and Capture IVC messages
 */

#ifndef INCLUDE_CAMRTC_CAPTURE_MESSAGES_H
#define INCLUDE_CAMRTC_CAPTURE_MESSAGES_H

#include "camrtc-capture.h"

#pragma GCC diagnostic error "-Wpadded"

/**
 * @brief Standard message header for all capture and capture-control IVC messages.
 *
 * Control Requests not associated with a specific channel
 * will use an opaque transaction ID rather than channel_id.
 * The transaction ID in the response message is copied from
 * the request message.
 */
struct CAPTURE_MSG_HEADER {
	/** Message identifier. */
	uint32_t msg_id;
	/** @anon_union */
	union {
		/** Channel number. @anon_union_member */
		uint32_t channel_id;
		/** Transaction id. @anon_union_member */
		uint32_t transaction;
	};
} CAPTURE_IVC_ALIGN;

/**
 * @defgroup ViCapCtrlMsgType Message types for capture-control IVC channel messages.
 * @{
 */
#define CAPTURE_CONTROL_RESERVED_10		MK_U32(0x10)
#define CAPTURE_CHANNEL_SETUP_REQ		MK_U32(0x1E)
#define CAPTURE_CHANNEL_SETUP_RESP		MK_U32(0x11)
#define CAPTURE_CHANNEL_RESET_REQ		MK_U32(0x12)
#define CAPTURE_CHANNEL_RESET_RESP		MK_U32(0x13)
#define CAPTURE_CHANNEL_RELEASE_REQ		MK_U32(0x14)
#define CAPTURE_CHANNEL_RELEASE_RESP		MK_U32(0x15)
#define CAPTURE_COMPAND_CONFIG_REQ		MK_U32(0x16)
#define CAPTURE_COMPAND_CONFIG_RESP		MK_U32(0x17)
#define CAPTURE_PDAF_CONFIG_REQ			MK_U32(0x18)
#define CAPTURE_PDAF_CONFIG_RESP		MK_U32(0x19)
#define CAPTURE_SYNCGEN_ENABLE_REQ		MK_U32(0x1A)
#define CAPTURE_SYNCGEN_ENABLE_RESP		MK_U32(0x1B)
#define CAPTURE_SYNCGEN_DISABLE_REQ		MK_U32(0x1C)
#define CAPTURE_SYNCGEN_DISABLE_RESP		MK_U32(0x1D)
/** @} */

/**
 * @defgroup IspCapCtrlMsgType Message types for ISP capture-control IVC channel messages.
 * @{
 */
#define CAPTURE_CHANNEL_ISP_SETUP_REQ		MK_U32(0x20)
#define CAPTURE_CHANNEL_ISP_SETUP_RESP		MK_U32(0x21)
#define CAPTURE_CHANNEL_ISP_RESET_REQ		MK_U32(0x22)
#define CAPTURE_CHANNEL_ISP_RESET_RESP		MK_U32(0x23)
#define CAPTURE_CHANNEL_ISP_RELEASE_REQ		MK_U32(0x24)
#define CAPTURE_CHANNEL_ISP_RELEASE_RESP	MK_U32(0x25)
/** @} */

/**
 * @defgroup ViCapMsgType Message types for capture channel IVC messages.
 * @{
 */
#define	CAPTURE_REQUEST_REQ			MK_U32(0x01)
#define	CAPTURE_STATUS_IND			MK_U32(0x02)
#define	CAPTURE_RESET_BARRIER_IND		MK_U32(0x03)
/** @} */

/**
 * @defgroup IspCapMsgType Message types for ISP capture channel IVC messages.
 * @{
 */
#define CAPTURE_ISP_REQUEST_REQ			MK_U32(0x04)
#define CAPTURE_ISP_STATUS_IND			MK_U32(0x05)
#define CAPTURE_ISP_PROGRAM_REQUEST_REQ		MK_U32(0x06)
#define CAPTURE_ISP_PROGRAM_STATUS_IND		MK_U32(0x07)
#define CAPTURE_ISP_RESET_BARRIER_IND		MK_U32(0x08)
#define CAPTURE_ISP_EX_STATUS_IND		MK_U32(0x09)
/** @} */

/**
 * @brief Invalid message type. This can be used to respond to an invalid request.
 */
#define CAPTURE_MSG_ID_INVALID			MK_U32(0xFFFFFFFF)

/**
 * @brief Invalid channel id. Used when channel is not specified.
 */
#define CAPTURE_CHANNEL_ID_INVALID		MK_U32(0xFFFFFFFF)

typedef uint32_t capture_result;

/**
 * @defgroup CapErrorCodes Unsigned 32-bit return values for the capture-control IVC messages.
 * @{
 */
#define CAPTURE_OK				MK_U32(0)
#define CAPTURE_ERROR_INVALID_PARAMETER		MK_U32(1)
#define CAPTURE_ERROR_NO_MEMORY			MK_U32(2)
#define CAPTURE_ERROR_BUSY			MK_U32(3)
#define CAPTURE_ERROR_NOT_SUPPORTED		MK_U32(4)
#define CAPTURE_ERROR_NOT_INITIALIZED		MK_U32(5)
#define CAPTURE_ERROR_OVERFLOW			MK_U32(6)
#define CAPTURE_ERROR_NO_RESOURCES		MK_U32(7)
#define CAPTURE_ERROR_TIMEOUT			MK_U32(8)
#define CAPTURE_ERROR_INVALID_STATE		MK_U32(9)
/** @} */

/**
 * @brief VI capture channel setup request message.
 *
 * Setup the VI Falcon channel context and initialize the
 * RCE capture channel context. The GOS tables are also configured.
 * The client shall use the transaction id field
 * in the standard message header to associate request and response.
 */
struct CAPTURE_CHANNEL_SETUP_REQ_MSG {
	/** Capture channel configuration. */
	struct capture_channel_config	channel_config;
} CAPTURE_IVC_ALIGN;

/**
 * @brief VI Capture channel setup response message.
 *
 * The transaction id field in the standard message header
 * will be copied from the associated request.
 *
 * The setup response message returns a channel_id, which
 * identifies this set of resources and is used to refer to the
 * allocated capture channel in subsequent messages.
 */
struct CAPTURE_CHANNEL_SETUP_RESP_MSG {
	/** Capture result return value. See @ref CapErrorCodes "Return values" */
	capture_result result;
	/** Capture channel identifier for the new channel. */
	uint32_t channel_id;
	/** Bitmask of allocated VI channel(s). LSB is VI channel 0. */
	uint64_t vi_channel_mask;
} CAPTURE_IVC_ALIGN;

/**
 * @defgroup CapResetFlags VI Capture channel reset flags
 * @{
 */
/** Reset the channel without waiting for FE first. */
#define CAPTURE_CHANNEL_RESET_FLAG_IMMEDIATE	MK_U32(0x01)
/** @} */

/**
 * @brief Reset a VI capture channel.
 *
 * Halt the associated VI channel. Flush the request queue for the
 * channel and increment syncpoints in the request queue to their target
 * values.
 */
struct CAPTURE_CHANNEL_RESET_REQ_MSG {
	/** See @ref CapResetFlags "Reset flags" */
	uint32_t reset_flags;
	/** Reserved */
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief VI capture channel reset response message.
 *
 * The response is sent after the RCE side channel cleanup is
 * complete. If the reset barrier is not received within the timeout
 * interval a CAPTURE_ERROR_TIMEOUT error is reported as the return value.
 * If the reset succeeds then the return value is CAPTURE_OK.
 */
struct CAPTURE_CHANNEL_RESET_RESP_MSG {
	/** Reset status return value. See @ref CapErrorCodes "Return values" */
	capture_result result;
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Release a VI capture channel and all the associated resources.
 *
 * Halt the associated VI channel and release the channel context.
 */
struct CAPTURE_CHANNEL_RELEASE_REQ_MSG {
	/** Reset flags. Currently not used in release request. */
	uint32_t reset_flags;
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Capture channel release response message.
 *
 * The release is acknowledged after the channel cleanup is complete
 * and all resources have been freed on RCE.
 */
struct CAPTURE_CHANNEL_RELEASE_RESP_MSG {
	/** Release status return value. See @ref CapErrorCodes "Return values" */
	capture_result result;
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Configure the piece-wise linear function used by the VI companding module.
 *
 * The companding table is shared by all capture channels and must be
 * configured before enabling companding for a specific capture. Each channel
 * can explicitly enable processing by the companding unit i.e the channels can
 * opt-out of the global companding config. See @ref CapErrorCodes "Capture request return codes"
 * for more details on the return values.
 */
struct CAPTURE_COMPAND_CONFIG_REQ_MSG {
	/** VI companding configuration */
	struct vi_compand_config compand_config;
} CAPTURE_IVC_ALIGN;

/**
 * @brief VI Companding unit configuration response message.
 *
 * Informs the client the status of VI companding unit configuration request.
 * A return value of CAPTURE_OK in the result field indicates the request
 * message succeeded. Any other value indicates an error.
 */
struct CAPTURE_COMPAND_CONFIG_RESP_MSG {
	/** Companding config setup result. See @ref CapErrorCodes "Return values". */
	capture_result result;
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Configure the Phase Detection Auto Focus (PDAF) pattern.
 */
struct CAPTURE_PDAF_CONFIG_REQ_MSG {
	/** PDAF configuration data */
	struct vi_pdaf_config pdaf_config;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Configure PDAF unit response message
 *
 * Returns the status PDAF unit configuration request.
 * A return value of CAPTURE_OK in the result field indicates the request
 * message succeeded. Any other value indicates an error. See
 * @ref CapErrorCodes "Capture request return codes" for more details on
 * the return values.
 */
struct CAPTURE_PDAF_CONFIG_RESP_MSG {
	/** PDAF config setup result. See @ref CapErrorCodes "Return values". */
	capture_result result;
	/** Reserved */
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;

/*
 * @brief Enable SLVS-EC synchronization
 *
 * Enable the generation of XVS and XHS synchronization signals for a
 * SLVS-EC sensor.
 */
struct CAPTURE_SYNCGEN_ENABLE_REQ_MSG {
	/** Syncgen unit */
	uint32_t unit;
	/** Reserved */
	uint32_t pad__;
	/** VI SYNCGEN unit configuration */
	struct vi_syncgen_config syncgen_config;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Enable SLVS-EC synchronization response message.
 *
 * Returns the status of enable SLVS-EC synchronization request.
 * A return value of CAPTURE_OK in the result field indicates the request
 * message succeeded. Any other value indicates an error. See
 * @ref CapErrorCodes "Capture request return codes" for more details on
 * the return values.
 */
struct CAPTURE_SYNCGEN_ENABLE_RESP_MSG {
	/** Syncgen unit */
	uint32_t unit;
	/** Syncgen enable request result. See @ref CapErrorCodes "Return values". */
	capture_result result;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Disable SLVS-EC synchronization
 *
 * Disable the generation of XVS and XHS synchronization signals for a
 * SLVS-EC sensor.
 */
struct CAPTURE_SYNCGEN_DISABLE_REQ_MSG {
	/** Syncgen unit */
	uint32_t unit;
	/** See SyncgenDisableFlags "Syncgen disable flags" */
	uint32_t syncgen_disable_flags;

/**
 * @defgroup SyncgenDisableFlags Syncgen disable flags
 * @{
 */
/** Disable SYNCGEN without waiting for frame end */
#define CAPTURE_SYNCGEN_DISABLE_FLAG_IMMEDIATE	MK_U32(0x01)
/** @} */

} CAPTURE_IVC_ALIGN;

/**
 * @brief Disable SLVS-EC synchronization response message.
 *
 * Returns the status of the SLVS-EC synchronization request message.
 * A return value of CAPTURE_OK in the result field indicates the request
 * message succeeded. Any other value indicates an error. See
 * @ref CapErrorCodes "Capture request return codes" for more details on
 * the return values.
 */
struct CAPTURE_SYNCGEN_DISABLE_RESP_MSG {
	/** Syncgen unit */
	uint32_t unit;
	/** Syncgen disable request result .See @ref CapErrorCodes "Return values". */
	capture_result result;
} CAPTURE_IVC_ALIGN;


/**
 * @brief Open an NVCSI stream request message
 * @deprecated
 */
struct CAPTURE_PHY_STREAM_OPEN_REQ_MSG {
	/** See NvCsiStream "NVCSI stream id" */
	uint32_t stream_id;
	/** See NvCsiPort "NvCSI Port" */
	uint32_t csi_port;
	/** See @ref NvPhyType "NvCSI Physical stream type" */
	uint32_t phy_type;
	/** Reserved */
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief NVCSI stream open response message
 * @deprecated
 */
struct CAPTURE_PHY_STREAM_OPEN_RESP_MSG {
	/** Stream open request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	/** Reserved */
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief NVCSI stream close request message
 * @deprecated
 */
struct CAPTURE_PHY_STREAM_CLOSE_REQ_MSG {
	/** NVCSI stream Id */
	uint32_t stream_id;
	/** NVCSI port */
	uint32_t csi_port;
	/** See @ref NvPhyType "NvCSI Physical stream type" */
	uint32_t phy_type;
	/** Reserved */
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief NVCSI stream close response message
 * @deprecated
 */
struct CAPTURE_PHY_STREAM_CLOSE_RESP_MSG {
	/** Stream close request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	/** Reserved */
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Physical stream dump registers request message. (Debug only)
 */
struct CAPTURE_PHY_STREAM_DUMPREGS_REQ_MSG {
	/** NVCSI stream Id */
	uint32_t stream_id;
	/** NVCSI port */
	uint32_t csi_port;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Physical stream dump registers response message. (Debug only)
 */
struct CAPTURE_PHY_STREAM_DUMPREGS_RESP_MSG {
	/** Stream dump registers request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Set NVCSI stream configuration request message.
 */
struct CAPTURE_CSI_STREAM_SET_CONFIG_REQ_MSG {
	/** NVCSI stream Id */
	uint32_t stream_id;
	/** NVCSI port */
	uint32_t csi_port;
	/** @ref See NvCsiConfigFlags "NVCSI Configuration Flags" */
	uint32_t config_flags;
	/** Reserved */
	uint32_t pad32__;
	/** NVCSI super control and interface logic (SCIL aka brick) configuration */
	struct nvcsi_brick_config brick_config;
	/** NVCSI control and interface logic (CIL) partition configuration */
	struct nvcsi_cil_config cil_config;
	/** User-defined error configuration */
	struct nvcsi_error_config error_config;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Set NVCSI stream configuration response message.
 */
struct CAPTURE_CSI_STREAM_SET_CONFIG_RESP_MSG {
	/** NVCSI stream config request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	/** Reserved */
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Set NVCSI stream parameter request message.
 */
struct CAPTURE_CSI_STREAM_SET_PARAM_REQ_MSG {
	/** NVCSI stream Id */
	uint32_t stream_id;
	/** NVCSI stream virtual channel id */
	uint32_t virtual_channel_id;
	/** The parameter to set. See @ref NvCsiParamType "NVCSI Parameter Type" */
	uint32_t param_type;
	/** Reserved */
	uint32_t pad32__;
	/** @anon_union */
	union {
		/** Set DPCM config for an NVCSI stream @anon_union_member */
		struct nvcsi_dpcm_config dpcm_config;
		/** NVCSI watchdog timer config @anon_union_member */
		struct nvcsi_watchdog_config watchdog_config;
	};
} CAPTURE_IVC_ALIGN;

/**
 * @brief Set NVCSI stream parameter response message.
 */
struct CAPTURE_CSI_STREAM_SET_PARAM_RESP_MSG {
	/** NVCSI set stream parameter request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	/** Reserved */
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief NVCSI test pattern generator (TPG) stream config request message.
 */
struct CAPTURE_CSI_STREAM_TPG_SET_CONFIG_REQ_MSG {
	/** TPG configuration */
	union nvcsi_tpg_config tpg_config;
} CAPTURE_IVC_ALIGN;

/**
 * @brief NVCSI TPG stream config response message.
 */
struct CAPTURE_CSI_STREAM_TPG_SET_CONFIG_RESP_MSG {
	/** Set TPG config request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	/** Reserved */
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Start NVCSI TPG streaming request message.
 */
struct CAPTURE_CSI_STREAM_TPG_START_REQ_MSG {
	/** NVCSI stream Id */
	uint32_t stream_id;
	/** NVCSI stream virtual channel id */
	uint32_t virtual_channel_id;
	/** TPG rate configuration */
	struct nvcsi_tpg_rate_config tpg_rate_config;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Start NVCSI TPG streaming response message.
 */
struct CAPTURE_CSI_STREAM_TPG_START_RESP_MSG {
	/** TPG start request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	/** Reserved */
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;


/**
 * @brief Start NVCSI TPG streaming at specified frame rate request message.
 *
 * This message is similar to CAPTURE_CSI_STREAM_TPG_START_REQ_MSG. Here the frame rate
 * and clock is specified using which the TPG rate config will be calculated.
 */
struct CAPTURE_CSI_STREAM_TPG_START_RATE_REQ_MSG {
	/** NVCSI stream Id */
	uint32_t stream_id;
	/** NVCSI stream virtual channel id */
	uint32_t virtual_channel_id;
	/** TPG frame rate in Hz */
	uint32_t frame_rate;
	/** Reserved */
	uint32_t __pad32;
} CAPTURE_IVC_ALIGN;

/**
 * @brief NVCSI TPG stream start at a specified frame rate response message.
 */
struct CAPTURE_CSI_STREAM_TPG_START_RATE_RESP_MSG {
	/** TPG start rate request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	/** Reserved */
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @defgroup gain ratio settings that can be set to frame generated by NVCSI TPG.
 * @{
 */
#define CAPTURE_CSI_STREAM_TPG_GAIN_RATIO_EIGHT_TO_ONE		MK_U8(0) /* 8:1 gain */
#define CAPTURE_CSI_STREAM_TPG_GAIN_RATIO_FOUR_TO_ONE		MK_U8(1) /* 4:1 gain */
#define CAPTURE_CSI_STREAM_TPG_GAIN_RATIO_TWO_TO_ONE		MK_U8(2) /* 2:1 gain */
#define CAPTURE_CSI_STREAM_TPG_GAIN_RATIO_NONE			MK_U8(3) /* 1:1 gain */
#define CAPTURE_CSI_STREAM_TPG_GAIN_RATIO_HALF			MK_U8(4) /* 0.5:1 gain */
#define CAPTURE_CSI_STREAM_TPG_GAIN_RATIO_ONE_FOURTH		MK_U8(5) /* 0.25:1 gain */
#define CAPTURE_CSI_STREAM_TPG_GAIN_RATIO_ONE_EIGHTH		MK_U8(6) /* 0.125:1 gain */

/**
 * @brief Apply gain ratio on specified VC of the desired CSI stream.
 *
 * This message is request to apply gain on specified vc, and it be
 * applied on next frame.
 */
struct CAPTURE_CSI_STREAM_TPG_APPLY_GAIN_REQ_MSG {
	/** NVCSI stream Id */
	uint32_t stream_id;
	/** NVCSI stream virtual channel id */
	uint32_t virtual_channel_id;
	/** Gain ratio */
	uint32_t gain_ratio;
	/** Reserved */
	uint32_t __pad32;
} CAPTURE_IVC_ALIGN;

/**
 * @brief NVCSI TPG stream start at a specified frame rate response message.
 */
struct CAPTURE_CSI_STREAM_TPG_APPLY_GAIN_RESP_MSG {
	/** TPG apply gain request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	/** Reserved */
	uint32_t __pad32;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Stop NVCSI TPG streaming request message.
 */
struct CAPTURE_CSI_STREAM_TPG_STOP_REQ_MSG {
	/** NVCSI stream Id */
	uint32_t stream_id;
	/** NVCSI stream virtual channel id */
	uint32_t virtual_channel_id;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Stop NVCSI TPG streaming response message.
 */
struct CAPTURE_CSI_STREAM_TPG_STOP_RESP_MSG {
	/** Stop TPG steaming request status. See @ref CapErrorCodes "Return values". */
	uint32_t result;
	uint32_t pad32__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Max number of events
 */
#define VI_NUM_INJECT_EVENTS 10U

/**
 * @brief Event injection configuration.
 *
 * A capture request must be sent before this message
 */
struct CAPTURE_CHANNEL_EI_REQ_MSG {
	/** Event data used for event injection */
	struct event_inject_msg events[VI_NUM_INJECT_EVENTS];
	/** Number of error events */
	uint8_t num_events;
	/** Reserved */
	uint8_t pad__[7];
} CAPTURE_IVC_ALIGN;

/**
 * @brief Acknowledge Event Injection request
 */
struct CAPTURE_CHANNEL_EI_RESP_MSG {
	/** Stop TPG steaming request status. See @ref CapErrorCodes "Return values". */
	capture_result result;
	/** Reserved */
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Event injection channel reset request.
 */
struct CAPTURE_CHANNEL_EI_RESET_REQ_MSG {
	/** Reserved */
	uint8_t pad__[8];
} CAPTURE_IVC_ALIGN;

/**
 * @brief Acknowledge Event injection channel reset request.
 */
struct CAPTURE_CHANNEL_EI_RESET_RESP_MSG {
	/** Event injection channel reset request result. See @ref CapErrorCodes "Return values". */
	capture_result result;
	/** Reserved */
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;

/**
 * @defgroup PhyStreamMsgType Message types for NvPhy
 * @{
 */
#define CAPTURE_PHY_STREAM_OPEN_REQ		MK_U32(0x36)
#define CAPTURE_PHY_STREAM_OPEN_RESP		MK_U32(0x37)
#define CAPTURE_PHY_STREAM_CLOSE_REQ		MK_U32(0x38)
#define CAPTURE_PHY_STREAM_CLOSE_RESP		MK_U32(0x39)
#define CAPTURE_PHY_STREAM_DUMPREGS_REQ		MK_U32(0x3C)
#define CAPTURE_PHY_STREAM_DUMPREGS_RESP	MK_U32(0x3D)
/** @} */

/**
 * @defgroup NvCsiMsgType Message types for NVCSI
 * @{
 */
#define CAPTURE_CSI_STREAM_SET_CONFIG_REQ	MK_U32(0x40)
#define CAPTURE_CSI_STREAM_SET_CONFIG_RESP	MK_U32(0x41)
#define CAPTURE_CSI_STREAM_SET_PARAM_REQ	MK_U32(0x42)
#define CAPTURE_CSI_STREAM_SET_PARAM_RESP	MK_U32(0x43)
#define CAPTURE_CSI_STREAM_TPG_SET_CONFIG_REQ	MK_U32(0x44)
#define CAPTURE_CSI_STREAM_TPG_SET_CONFIG_RESP	MK_U32(0x45)
#define CAPTURE_CSI_STREAM_TPG_START_REQ	MK_U32(0x46)
#define CAPTURE_CSI_STREAM_TPG_START_RESP	MK_U32(0x47)
#define CAPTURE_CSI_STREAM_TPG_STOP_REQ		MK_U32(0x48)
#define CAPTURE_CSI_STREAM_TPG_STOP_RESP	MK_U32(0x49)
#define CAPTURE_CSI_STREAM_TPG_START_RATE_REQ	MK_U32(0x4A)
#define CAPTURE_CSI_STREAM_TPG_START_RATE_RESP	MK_U32(0x4B)
#define CAPTURE_CSI_STREAM_TPG_APPLY_GAIN_REQ	MK_U32(0x4C)
#define CAPTURE_CSI_STREAM_TPG_APPLY_GAIN_RESP	MK_U32(0x4D)
/** @} */

/**
 * @addtogroup NvCsiMsgType Message types for NVCSI
 * @{
 */
#define CAPTURE_CHANNEL_EI_REQ			MK_U32(0x50)
#define CAPTURE_CHANNEL_EI_RESP			MK_U32(0x51)
#define CAPTURE_CHANNEL_EI_RESET_REQ		MK_U32(0x52)
#define CAPTURE_CHANNEL_EI_RESET_RESP		MK_U32(0x53)
/** @} */

/**
 * @addtogroup ViCapCtrlMsgType
 * @{
 */
#define CAPTURE_HSM_CHANSEL_ERROR_MASK_REQ	MK_U32(0x54)
#define CAPTURE_HSM_CHANSEL_ERROR_MASK_RESP	MK_U32(0x55)
/** @} */

/**
 * @addtogroup ViCapCtrlMsgs
 * @{
 */
/**
 * @brief Set CHANSEL error mask from HSM reporting message
 */
struct CAPTURE_HSM_CHANSEL_ERROR_MASK_REQ_MSG {
	/** VI EC/HSM global CHANSEL error mask configuration */
	struct vi_hsm_chansel_error_mask_config hsm_chansel_error_config;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Acknowledge CHANEL error mask request
 */
struct CAPTURE_HSM_CHANSEL_ERROR_MASK_RESP_MSG {
	/** HSM CHANSEL error mask request result. See @ref CapErrorCodes "Return values". */
	capture_result result;
	/** Reserved */
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;
/** @} */

/**
 * @brief Set up RCE side resources for ISP capture pipe-line.
 *
 * The client shall use the transaction id field in the
 * standard message header to associate request and response.
 */
struct CAPTURE_CHANNEL_ISP_SETUP_REQ_MSG {
	/** ISP process channel configuration. */
	struct capture_channel_isp_config	channel_config;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Acknowledge isp capture channel setup request.
 *
 * The transaction id field in the standard message header
 * will be copied from the associated request.
 *
 * The setup response message returns a channel_id, which
 * identifies this set of resources and is used to refer to the
 * allocated capture channel in subsequent messages.
 */
struct CAPTURE_CHANNEL_ISP_SETUP_RESP_MSG {
	/** ISP process channel setup request status. See @ref CapErrorCodes "Return values". */
	capture_result result;
	/** ISP process channel identifier for the new channel. */
	uint32_t channel_id;
} CAPTURE_IVC_ALIGN;

typedef struct CAPTURE_CHANNEL_RESET_REQ_MSG
			CAPTURE_CHANNEL_ISP_RESET_REQ_MSG;
typedef struct CAPTURE_CHANNEL_RESET_RESP_MSG
			CAPTURE_CHANNEL_ISP_RESET_RESP_MSG;
typedef struct CAPTURE_CHANNEL_RELEASE_REQ_MSG
			CAPTURE_CHANNEL_ISP_RELEASE_REQ_MSG;
typedef struct CAPTURE_CHANNEL_RELEASE_RESP_MSG
			CAPTURE_CHANNEL_ISP_RELEASE_RESP_MSG;

/**
 * @brief Message frame for capture-control IVC channel.
 */
struct CAPTURE_CONTROL_MSG {
	struct CAPTURE_MSG_HEADER header;
	/** @anon_union */
	union {
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_SETUP_REQ_MSG channel_setup_req;
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_SETUP_RESP_MSG channel_setup_resp;
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_RESET_REQ_MSG channel_reset_req;
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_RESET_RESP_MSG channel_reset_resp;
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_RELEASE_REQ_MSG channel_release_req;
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_RELEASE_RESP_MSG channel_release_resp;
		/** @anon_union_member */
		struct CAPTURE_COMPAND_CONFIG_REQ_MSG compand_config_req;
		/** @anon_union_member */
		struct CAPTURE_COMPAND_CONFIG_RESP_MSG compand_config_resp;
		/** @anon_union_member */
		struct CAPTURE_PDAF_CONFIG_REQ_MSG pdaf_config_req;
		/** @anon_union_member */
		struct CAPTURE_PDAF_CONFIG_RESP_MSG pdaf_config_resp;
		/** @anon_union_member */
		struct CAPTURE_SYNCGEN_ENABLE_REQ_MSG syncgen_enable_req;
		/** @anon_union_member */
		struct CAPTURE_SYNCGEN_ENABLE_RESP_MSG syncgen_enable_resp;
		/** @anon_union_member */
		struct CAPTURE_SYNCGEN_DISABLE_REQ_MSG syncgen_disable_req;
		/** @anon_union_member */
		struct CAPTURE_SYNCGEN_DISABLE_RESP_MSG syncgen_disable_resp;

		/** @anon_union_member */
		struct CAPTURE_PHY_STREAM_OPEN_REQ_MSG phy_stream_open_req;
		/** @anon_union_member */
		struct CAPTURE_PHY_STREAM_OPEN_RESP_MSG phy_stream_open_resp;
		/** @anon_union_member */
		struct CAPTURE_PHY_STREAM_CLOSE_REQ_MSG phy_stream_close_req;
		/** @anon_union_member */
		struct CAPTURE_PHY_STREAM_CLOSE_RESP_MSG phy_stream_close_resp;
		/** @anon_union_member */
		struct CAPTURE_PHY_STREAM_DUMPREGS_REQ_MSG
			phy_stream_dumpregs_req;
		/** @anon_union_member */
		struct CAPTURE_PHY_STREAM_DUMPREGS_RESP_MSG
			phy_stream_dumpregs_resp;

		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_SET_CONFIG_REQ_MSG
			csi_stream_set_config_req;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_SET_CONFIG_RESP_MSG
			csi_stream_set_config_resp;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_SET_PARAM_REQ_MSG
			csi_stream_set_param_req;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_SET_PARAM_RESP_MSG
			csi_stream_set_param_resp;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_SET_CONFIG_REQ_MSG
			csi_stream_tpg_set_config_req;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_SET_CONFIG_RESP_MSG
			csi_stream_tpg_set_config_resp;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_START_REQ_MSG
			csi_stream_tpg_start_req;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_START_RESP_MSG
			csi_stream_tpg_start_resp;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_STOP_REQ_MSG
			csi_stream_tpg_stop_req;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_STOP_RESP_MSG
			csi_stream_tpg_stop_resp;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_START_RATE_REQ_MSG
			csi_stream_tpg_start_rate_req;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_START_RATE_RESP_MSG
			csi_stream_tpg_start_rate_resp;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_APPLY_GAIN_REQ_MSG
			csi_stream_tpg_apply_gain_req;
		/** @anon_union_member */
		struct CAPTURE_CSI_STREAM_TPG_APPLY_GAIN_RESP_MSG
			csi_stream_tpg_apply_gain_resp;

		/** @anon_union_member */
		struct CAPTURE_CHANNEL_EI_REQ_MSG ei_req;
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_EI_RESP_MSG ei_resp;
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_EI_RESET_REQ_MSG ei_reset_req;
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_EI_RESET_RESP_MSG ei_reset_resp;

		/** @anon_union_member */
		struct CAPTURE_CHANNEL_ISP_SETUP_REQ_MSG channel_isp_setup_req;
		/** @anon_union_member */
		struct CAPTURE_CHANNEL_ISP_SETUP_RESP_MSG channel_isp_setup_resp;
		/** @anon_union_member */
		CAPTURE_CHANNEL_ISP_RESET_REQ_MSG channel_isp_reset_req;
		/** @anon_union_member */
		CAPTURE_CHANNEL_ISP_RESET_RESP_MSG channel_isp_reset_resp;
		/** @anon_union_member */
		CAPTURE_CHANNEL_ISP_RELEASE_REQ_MSG channel_isp_release_req;
		/** @anon_union_member */
		CAPTURE_CHANNEL_ISP_RELEASE_RESP_MSG channel_isp_release_resp;

		/** @anon_union_member */
		struct CAPTURE_HSM_CHANSEL_ERROR_MASK_REQ_MSG hsm_chansel_mask_req;
		/** @anon_union_member */
		struct CAPTURE_HSM_CHANSEL_ERROR_MASK_RESP_MSG hsm_chansel_mask_resp;
	};
} CAPTURE_IVC_ALIGN;

/**
 * @brief Enqueue a new capture request on a capture channel.
 *
 * The request contains channel identifier and the capture sequence
 * number, which are required to schedule the capture request. The
 * actual capture programming is stored in the capture descriptor,
 * stored in a DRAM ring buffer set up with CAPTURE_CHANNEL_SETUP_REQ.
 *
 * The capture request descriptor with buffer_index=N can be located
 * within the ring buffer as follows:
 *
 * struct capture_descriptor *desc = requests + buffer_index * request_size;
 *
 * The capture request message is asynchronous. Capture completion is
 * indicated by incrementing the progress syncpoint a pre-calculated
 * number of times = 1 + <number of sub-frames>. The first increment
 * occurs at start-of-frame and the last increment occurs at
 * end-of-frame. The progress-syncpoint is used to synchronize with
 * down-stream engines. This model assumes that the capture client
 * knows the number of subframes used in the capture and has
 * programmed the VI accordingly.
 *
 * If the flag CAPTURE_FLAG_STATUS_REPORT_ENABLE is set in the capture
 * descriptor, RCE will store the capture status into status field
 * of the descriptor. RCE will also send a CAPTURE_STATUS_IND
 * message to indicate that capture has completed. The capture status
 * record contains information about the capture, such as CSI frame
 * number, start-of-frame and end-of-frame timestamps, as well as
 * error status.
 *
 * If the flag CAPTURE_FLAG_ERROR_REPORT_ENABLE is set, RCE will send a
 * CAPTURE_STATUS_IND upon an error, even if
 * CAPTURE_FLAG_STATUS_REPORT_ENABLE is not set.
 */
struct CAPTURE_REQUEST_REQ_MSG {
	/** Buffer index identifying capture descriptor. */
	uint32_t buffer_index;
	/** Reserved */
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Capture status indication.
 *
 * The message is sent after the capture status record has been
 * written into the capture request descriptor.
 */
struct CAPTURE_STATUS_IND_MSG {
	/** Buffer index identifying capture descriptor. */
	uint32_t buffer_index;
	/** Reserved */
	uint32_t pad__;
} CAPTURE_IVC_ALIGN;


/**
 * @brief Send new isp_capture request on a capture channel.
 *
 * The request contains channel identifier and the capture sequence
 * number (ring-buffer index), which are required to schedule the
 * isp capture request.
 * The actual capture programming is stored in isp_capture_descriptor,
 * stored in DRAM ring buffer, which includes the sequence, ISP
 * surfaces' details, surface related configs, ISP PB2 iova, input prefences,
 * and isp_capture status written by RCE.
 *
 * NvCapture UMD allocates the pool of isp_capture descriptors in setup call,
 * where each isp_capture_desc is followed by corresponding PB2 memory
 * (ATOM aligned).
 * RCE would generate the PB2 using surface details found in isp_capture
 * descriptor.
 * The ring-buffer (pool) would look like below:
 *
 * [isp_capture_desc][PB2][isp_capture_desc][PB2][isp_capture_desc]...
 *
 * The isp_capture_descriptor with buffer_index=N can be located within
 * the ring buffer as follows:
 *
 * isp_capture_descriptor *desc = requests + buffer_index * request_size;
 *
 * Note, here request_size = sizeof (isp_capture_descriptor) + sizeof (PB2).
 *
 * UMD fills isp_capture_desc and submits the request to KMD which pins the
 * surfaces and PB and then does the in-place replacement with iovas' within
 * isp_capture_descriptor.
 * KMD then sends the isp_capture request to RCE over capture ivc channel.
 *
 * The isp capture request message is asynchronous. Capture completion is
 * indicated by incrementing the progress syncpoint a pre-calculated
 * number of times = <number of sub-frames>. The progress-syncpoint is
 * used to synchronize with down-stream engines. This model assumes that
 * the capture client knows the number of subframes used in the capture and has
 * programmed the ISP accordingly.
 * All stats completion are indicated by incrementing stats progress syncpoint
 * a number of times = <num-stats-enabled>.
 *
 * If the flag CAPTURE_FLAG_ISP_STATUS_REPORT_ENABLE is set in the isp
 * capture descriptor, RCE will store the capture status into status field
 * of the descriptor. RCE will also send a CAPTURE_ISP_STATUS_IND
 * message to indicate that capture has completed.
 *
 * If the flag CAPTURE_FLAG_ISP_ERROR_REPORT_ENABLE is set, RCE will send a
 * CAPTURE_ISP_STATUS_IND upon an error, even if
 * CAPTURE_FLAG_ISP_STATUS_REPORT_ENABLE is not set.
 *
 * Typedef-ed CAPTURE_REQUEST_REQ_MSG.
 *
 * The buffer_index field is isp_capture_descriptor index in ring buffer.
 */
typedef struct CAPTURE_REQUEST_REQ_MSG CAPTURE_ISP_REQUEST_REQ_MSG;

/**
 * @brief ISP Capture status indication.
 *
 * The message is sent after the capture status record has been
 * written into the capture request descriptor.
 *
 * The buffer_index	in this case is identifying the ISP capture descriptor.
 */
typedef struct CAPTURE_STATUS_IND_MSG CAPTURE_ISP_STATUS_IND_MSG;

/**
 * @brief Extended ISP capture status indication.
 *
 * The message is sent after the capture status record has been
 * written into the capture request descriptor.
 */
struct CAPTURE_ISP_EX_STATUS_IND_MSG {
	/** Buffer index identifying ISP process descriptor. */
	uint32_t process_buffer_index;
	/** Buffer index identifying ISP program descriptor. */
	uint32_t program_buffer_index;
} CAPTURE_IVC_ALIGN;

/**
 * @brief Send new isp_program request on a capture ivc channel.
 *
 * The request contains channel identifier and the program sequence
 * number (ring-buffer index).
 * The actual programming details is stored in isp_program
 * descriptor, which includes the offset to isp_program
 * buffer (which has PB1 containing ISP HW settings), sequence,
 * settings-id, activation-flags, isp_program buffer size, iova's
 * of ISP PB1 and isp_program status written by RCE.
 *
 * NvCapture UMD allocates the pool of isp_program descriptors in setup call,
 * where each isp_pgram_descriptor is followed by corresponding isp_program
 * buffer (ATOM aligned).
 * The ring-buffer (pool) would look like below:
 *
 * [isp_prog_desc][isp_program][isp_prog_desc][isp_program][isp_prog_desc]...
 *
 * The isp_program_descriptor with buffer_index=N can be located within
 * the ring buffer as follows:
 *
 * isp_program_descriptor *desc = programs + buffer_index * program_size;
 *
 * Note, program_size = sizeof (isp_program_descriptor) + sizeof (isp_program).
 *
 * NvISP fills these and submits the isp_program request to KMD which pins the
 * PB and then does the in-place replacement with iova within
 * isp_program_descriptor.
 * KMD then sends the isp_program request to RCE over capture ivc channel.
 *
 * The sequence is the frame_id which tells RCE, that the given isp_program
 * must be used from that frame_id onwards until UMD provides new one.
 * So RCE will use the sequence field to select the correct isp_program from
 * the isp_program descriptors' ring buffer for given frame request and will
 * keep on using it for further frames until the new isp_program (desc) is
 * provided to be used.
 * RCE populates both matched isp_program (reads from isp program desc) and
 * isp capture descriptor and forms single task descriptor for given frame
 * request and feeds it to falcon, which further programs it to ISP.
 *
 * settings_id is unique id for isp_program, NvCapture and RCE will use
 * the ring buffer array index as settings_id.
 * It can also be used to select the correct isp_program for the given
 * frame, in that case, UMD writes this unique settings_id to sensor's
 * scratch register, and sensor will send back it as part of embedded data,
 * when the given settings/gains are applied on that particular frame
 * coming from sensor.
 *
 * RCE reads this settings_id back from embedded data and uses it to select
 * the corresponding isp_program from the isp_program desc ring buffer.
 * The activation_flags tells the RCE which id (sequence or settings_id) to
 * use to select correct isp_program for the given frame.
 *
 * As same isp_program can be used for multiple frames, it can not be freed
 * when the frame capture is done. RCE will send a separate status
 * indication CAPTURE_ISP_PROGRAM_STATUS_IND message to CCPEX to notify
 * that the given isp_program is no longer in use and can be freed or reused.
 * settings_id (ring-buffer index) field is used to uniquely identify the
 * correct isp_program.
 * RCE also writes the isp_program status in isp program descriptor.
 *
 * Typedef-ed CAPTURE_REQUEST_REQ_MSG.
 *
 * The buffer_index field is the isp_program descriptor index in ring buffer.
 */
typedef struct CAPTURE_REQUEST_REQ_MSG CAPTURE_ISP_PROGRAM_REQUEST_REQ_MSG;

/**
 * @brief ISP program status indication.
 *
 * The message is sent to notify CCPLEX about the isp_program which is expired
 * so UMD client can free or reuse it.
 *
 * Typedef-ed CAPTURE_STATUS_IND_MSG.
 *
 * The buffer_index field in this case is identifying ISP program descriptor.
 */
typedef struct CAPTURE_STATUS_IND_MSG CAPTURE_ISP_PROGRAM_STATUS_IND_MSG;

/**
 * @brief Message frame for capture IVC channel.
 */
struct CAPTURE_MSG {
	struct CAPTURE_MSG_HEADER header;
	/** @anon_union */
	union {
		/** @anon_union_member */
		struct CAPTURE_REQUEST_REQ_MSG capture_request_req;
		/** @anon_union_member */
		struct CAPTURE_STATUS_IND_MSG capture_status_ind;

		/** @anon_union_member */
		CAPTURE_ISP_REQUEST_REQ_MSG capture_isp_request_req;
		/** @anon_union_member */
		CAPTURE_ISP_STATUS_IND_MSG capture_isp_status_ind;
		/** @anon_union_member */
		struct CAPTURE_ISP_EX_STATUS_IND_MSG capture_isp_ex_status_ind;

		/** @anon_union_member */
		CAPTURE_ISP_PROGRAM_REQUEST_REQ_MSG
				capture_isp_program_request_req;
		/** @anon_union_member */
		CAPTURE_ISP_PROGRAM_STATUS_IND_MSG
				capture_isp_program_status_ind;
	};
} CAPTURE_IVC_ALIGN;

#pragma GCC diagnostic ignored "-Wpadded"

#endif /* INCLUDE_CAMRTC_CAPTURE_MESSAGES_H */
