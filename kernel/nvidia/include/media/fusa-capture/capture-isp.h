/*
 * Copyright (c) 2017-2019 NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

/**
 * @file include/media/fusa-capture/capture-isp.h
 *
 * @brief ISP channel operations header for the T186/T194 Camera RTCPU platform.
 */

#ifndef __FUSA_CAPTURE_ISP_H__
#define __FUSA_CAPTURE_ISP_H__

#if defined(__KERNEL__)
#include <linux/compiler.h>
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#include <linux/ioctl.h>

#define __ISP_CAPTURE_ALIGN __aligned(8)

struct tegra_isp_channel;

/**
 * @brief ISP descriptor relocs config.
 */
struct capture_isp_reloc {
	uint32_t num_relocs; /**< No. of buffers to pin/reloc */
	uint32_t __pad;
	uint64_t reloc_relatives;
		/**< Offsets buffer addresses to patch in descriptor */
} __ISP_CAPTURE_ALIGN;

/**
 * @brief ISP channel setup config (IOCTL payload).
 *
 * These fields are used to set up the ISP channel and capture contexts, and
 * will be copied verbatim in the IVC capture_channel_isp_config struct to
 * allocate ISP resources in the RCE subsystem.
 */
struct isp_capture_setup {
	uint32_t channel_flags;
		/**<
		 * Bitmask for channel flags, see @ref CAPTURE_ISP_CHANNEL_FLAGS
		 */
	uint32_t __pad_flags;

	/* ISP process capture descriptor queue (ring buffer) */
	uint32_t queue_depth;
		/**< No. of process capture descriptors in queue */
	uint32_t request_size;
		/**< Size of a single process capture descriptor [byte] */
	uint32_t mem; /**< Process capture descriptors queue NvRm handle */

	/* ISP process program descriptor queue (ring buffer) */
	uint32_t isp_program_queue_depth;
		/**< No. of process program descriptors in queue */
	uint32_t isp_program_request_size;
		/**< Size of a single process program descriptor [byte] */
	uint32_t isp_program_mem;
		/**< Process program descriptors queue NvRm handle */
	uint32_t error_mask_correctable;
		/**<
		 * Bitmask for correctable channel errors. See
		 * @ref CAPTURE_ISP_CHANNEL_ERRORS
		 */
	uint32_t error_mask_uncorrectable;
		/**<
		 * Bitmask for uncorrectable channel errors. See
		 * @ref CAPTURE_ISP_CHANNEL_ERRORS
		 */
} __ISP_CAPTURE_ALIGN;

/**
 * @brief ISP capture info (resp. to query).
 */
struct isp_capture_info {
	struct isp_capture_syncpts {
		uint32_t progress_syncpt; /**< Progress syncpoint id */
		uint32_t progress_syncpt_val; /**< Progress syncpoint value */
		uint32_t stats_progress_syncpt;
			/**< Stats progress syncpoint id */
		uint32_t stats_progress_syncpt_val;
			/**< Stats progress syncpoint value */
	} syncpts;
	uint32_t channel_id; /**< RCE-assigned ISP FW channel id */
} __ISP_CAPTURE_ALIGN;

/**
 * @brief ISP process capture request (IOCTL payload).
 */
struct isp_capture_req {
	uint32_t buffer_index; /**< Process descriptor index */
	uint32_t __pad;
	struct capture_isp_reloc isp_relocs;
		/**<
		 * Surface buffers pin/reloc config. See @ref capture_isp_reloc
		 */
	struct capture_isp_reloc inputfences_relocs;
		/**<
		 * Inputfences to pin/reloc. config. See @ref capture_isp_reloc
		 */
	uint32_t gos_relative; /* GoS offset [byte] */
	uint32_t sp_relative; /* Syncpt offset [byte] */
	struct capture_isp_reloc prefences_relocs;
		/**<
		 * Prefences to pin/reloc. config. See @ref capture_isp_reloc
		 */
} __ISP_CAPTURE_ALIGN;

/**
 * @brief ISP process program request (IOCTL payload).
 */
struct isp_program_req {
	uint32_t buffer_index; /**< Program descriptor index. */
	uint32_t __pad;
	struct capture_isp_reloc isp_program_relocs;
		/**<
		 * Push buffers to pin/reloc. config. See
		 * @ref capture_isp_reloc
		 */
} __ISP_CAPTURE_ALIGN;

/**
 * @brief ISP joint capture+program request (IOCTL payload).
 */
struct isp_capture_req_ex {
	struct isp_capture_req capture_req; /**< ISP capture process request */
	struct isp_program_req program_req; /**< ISP program process request */
	uint32_t __pad[4];
} __ISP_CAPTURE_ALIGN;

/**
 * @brief ISP capture progress status setup config (IOCTL payload).
 */
struct isp_capture_progress_status_req {
	uint32_t mem; /**< NvRm handle to buffer region start */
	uint32_t mem_offset; /**< Status notifier offset [byte] */
	uint32_t process_buffer_depth;
		/**< Process capture descriptor queue size [num] */
	uint32_t program_buffer_depth;
		/**< Process program descriptor queue size [num] */
	uint32_t __pad[4];
} __ISP_CAPTURE_ALIGN;

/**
 * @brief Add ISP capture buffer to management table (IOCTL payload).
 */
struct isp_buffer_req {
	uint32_t mem; /**< NvRm handle to buffer */
	uint32_t flag; /**< Buffer @ref CAPTURE_BUFFER_OPS bitmask */
} __ISP_CAPTURE_ALIGN;

/**
 * @brief Initialize an ISP channel capture context (at channel open).
 *
 * The ISP channel context is already partially-initialized by the calling
 * function, the channel capture context is allocated and linked here.
 *
 * @param[in,out]	chan		Allocated ISP channel context,
 *					partially-initialized

 * @returns		0 (success), neg. errno (failure)
 */
int isp_capture_init(
	struct tegra_isp_channel *chan);

/**
 * @brief De-initialize an ISP capture channel, closing open ISP streams, and
 * freeing the buffer management table and channel capture context.
 *
 * The ISP channel context is not freed in this function, only the capture
 * context is.
 *
 * The ISP channel should have been RESET and RELEASE'd when this function is
 * called, but they may still be active due to programming error or client UMD
 * crash. In such cases, they will be called automatically by the @em Abort
 * functionality.
 *
 * @param[in,out]	chan		VI channel context
 */
void isp_capture_shutdown(
	struct tegra_isp_channel *chan);

/**
 * @brief Open an ISP channel in RCE, sending channel configuration to request a
 * SW channel allocation. Syncpts are allocated by the KMD in this subroutine.
 *
 * @param[in,out]	chan	ISP channel context
 * @param[in]		setup	ISP channel setup config
 *
 * @returns		0 (success), neg. errno (failure)
 */
int isp_capture_setup(
	struct tegra_isp_channel *chan,
	struct isp_capture_setup *setup);

/**
 * @brief Reset an opened ISP channel, all pending process requests to RCE are
 * discarded.
 *
 * The channel's progress syncpoint is advanced to the threshold of the latest
 * capture/program request to unblock any waiting observers.
 *
 * A reset barrier may be enqueued in the capture IVC channel to flush stale
 * capture/program descriptors, in case of abnormal channel termination.
 *
 * @param[in]	chan		VI channel context
 * @param[in]	reset_flags	Bitmask for ISP channel reset options
 *				(CAPTURE_CHANNEL_RESET_FLAG_*)

 * @returns	0 (success), neg. errno (failure)
 */
int isp_capture_reset(
	struct tegra_isp_channel *chan,
	uint32_t reset_flags);

/**
 * @brief Release an opened ISP channel; the RCE channel allocation, syncpoints
 * and IVC channel callbacks are released.
 *
 * @param[in]	chan		ISP channel context
 * @param[in]	reset_flags	Bitmask for ISP channel reset options
 *				(CAPTURE_CHANNEL_RESET_FLAG_*)
 *
 * @returns	0 (success), neg. errno (failure)
 */
int isp_capture_release(
	struct tegra_isp_channel *chan,
	uint32_t reset_flags);

/**
 * @brief Query an ISP channel's syncpoint ids and values, and retrieve the
 * RCE-assigned ISP FW channel id.
 *
 * @param[in]	chan	ISP channel context
 * @param[out]	info	ISP channel info response
 *
 * @returns	0 (success), neg. errno (failure)
 */
int isp_capture_get_info(
	struct tegra_isp_channel *chan,
	struct isp_capture_info *info);

/**
 * @brief Send a capture (aka. process) request for a frame via the capture IVC
 * channel to RCE.
 *
 * This is a non-blocking call.
 *
 * @param[in]	chan	ISP channel context
 * @param[in]	req	ISP process capture request
 *
 * @returns	0 (success), neg. errno (failure)
 */
int isp_capture_request(
	struct tegra_isp_channel *chan,
	struct isp_capture_req *req);

/**
 * @brief Wait on receipt of the capture status of the head of the capture
 * request FIFO queue to RCE. The RCE ISP driver sends a CAPTURE_ISP_STATUS_IND
 * notification at frame completion.
 *
 * This is a blocking call, with the possibility of timeout.
 *
 * @todo The capture progress status notifier is expected to replace this
 * functionality in the future, deprecating it.
 *
 * @param[in]	chan		ISP channel context
 * @param[in]	timeout_ms	Time to wait for status completion [ms], set to
 *				0 for indefinite
 *
 * @returns	0 (success), neg. errno (failure)
 */
int isp_capture_status(
	struct tegra_isp_channel *chan,
	int32_t timeout_ms);

/**
 * @brief Send a program request containing an ISP pushbuffer configuration via
 * the capture IVC channel to RCE.
 *
 * This is a non-blocking call.
 *
 * @param[in]	chan	ISP channel context
 * @param[in]	req	ISP program request
 *
 * @returns	0 (success), neg. errno (failure)
 */
int isp_capture_program_request(
	struct tegra_isp_channel *chan,
	struct isp_program_req *req);

/**
 * @brief Wait on receipt of the program status of the head of the program
 * request FIFO queue to RCE. The RCE ISP driver sends a
 * CAPTURE_ISP_PROGRAM_STATUS_IND notification at completion.
 *
 * This is a blocking call, with no possibility of timeout; as programs may be
 * reused for multiple frames.
 *
 * @todo The capture progress status notifier is expected to replace this
 * functionality in the future, deprecating it.
 *
 * @param[in]	chan		ISP channel context
 *
 * @returns	0 (success), neg. errno (failure)
 */
int isp_capture_program_status(
	struct tegra_isp_channel *chan);

/**
 * @brief Send an extended capture (aka. process) request for a frame,
 * containing the ISP pushbuffer program to execute via the capture IVC channel
 * to RCE.
 *
 * The extended call is equivalent to separately sending a capture and a program
 * request for every frame; it is an optimization to reduce the number of
 * system context switches from IOCTL and IVC calls.
 *
 * This is a non-blocking call.
 *
 * @param[in]	chan	ISP channel context
 * @param[in]	req	ISP extended process request
 *
 * @returns	0 (success), neg. errno (failure)
 */
int isp_capture_request_ex(
	struct tegra_isp_channel *chan,
	struct isp_capture_req_ex *req);

/**
 * @brief Setup ISP channel capture status progress notifier
 *
 * @param[in]	chan	ISP channel context
 * @param[in]	req	ISP capture progress status setup config
 *
 * @returns	0 (success), neg. errno (failure)
 */
int isp_capture_set_progress_status_notifier(
	struct tegra_isp_channel *chan,
	struct isp_capture_progress_status_req *req);

/**
 * @brief Perform a buffer management operation on an ISP capture buffer.
 *
 * @param[in]	chan	ISP channel context
 * @param[in]	req	ISP capture buffer request
 *
 * @returns		0 (success), neg. errno (failure)
 */
int isp_capture_buffer_request(
	struct tegra_isp_channel *chan,
	struct isp_buffer_req *req);

#endif /* __FUSA_CAPTURE_ISP_H__ */
