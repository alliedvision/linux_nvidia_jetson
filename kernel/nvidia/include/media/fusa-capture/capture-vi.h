/*
 * Copyright (c) 2017-2021 NVIDIA Corporation.  All rights reserved.
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
 * @file include/media/fusa-capture/capture-vi.h
 *
 * @brief VI channel operations header for the T186/T194 Camera RTCPU platform.
 */

#ifndef __FUSA_CAPTURE_VI_H__
#define __FUSA_CAPTURE_VI_H__

#if defined(__KERNEL__)
#include <linux/compiler.h>
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#include <media/fusa-capture/capture-common.h>
#include <media/fusa-capture/capture-vi-channel.h>
#include "soc/tegra/camrtc-capture.h"
#include "soc/tegra/camrtc-capture-messages.h"

#define __VI_CAPTURE_ALIGN __aligned(8)

struct tegra_vi_channel;
struct capture_buffer_table;

/**
 * @brief VI channel capture context.
 */
struct vi_capture {
	uint16_t channel_id; /**< RCE-assigned VI FW channel id */
	struct device *rtcpu_dev; /**< rtcpu device */
	struct tegra_vi_channel *vi_channel; /**< VI channel context */
	struct capture_buffer_table *buf_ctx;
		/**< Surface buffer management table */
	struct capture_common_buf requests; /**< Capture descriptors queue */
	struct capture_descriptor_memoryinfo *requests_memoryinfo;
		/**< memory info ringbuffer handle*/
	uint64_t requests_memoryinfo_iova;
		/**< memory info ringbuffer rtcpu iova */
	size_t request_buf_size;
		/**< Size of capture descriptor queue [byte] */
	uint32_t queue_depth; /**< No. of capture descriptors in queue */
	uint32_t request_size; /**< Size of single capture descriptor [byte] */
	bool is_mem_pinned; /**< Whether capture request memory is pinned */

	struct capture_common_status_notifier progress_status_notifier;
		/**< Capture progress status notifier context */
	uint32_t progress_status_buffer_depth;
		/**< No. of capture descriptors */
	bool is_progress_status_notifier_set;
		/**< Whether progress_status_notifer has been initialized */

	uint32_t stream_id; /**< NVCSI PixelParser index [0-5] */
	uint32_t csi_port; /**< NVCSI ports A-H [0-7] */
	uint32_t virtual_channel_id; /**< CSI virtual channel id [0-15] */

	uint32_t num_gos_tables; /**< No. of cv devices in gos_tables */
	const dma_addr_t *gos_tables; /**< IOVA addresses of all GoS devices */

	struct syncpoint_info progress_sp; /**< Syncpoint for frame progress */
	struct syncpoint_info embdata_sp;
		/**< Syncpoint for embedded metadata */
	struct syncpoint_info linetimer_sp;
		/**< Syncpoint for frame line timer */

	struct completion control_resp;
		/**< Completion for capture-control IVC response */
	struct completion capture_resp;
		/**<
		 * Completion for capture requests (frame), if progress status
		 * notifier is not in use
		 */
	struct mutex control_msg_lock;
		/**< Lock for capture-control IVC control_resp_msg */
	struct CAPTURE_CONTROL_MSG control_resp_msg;
		/**< capture-control IVC resp msg written to by callback */

	struct mutex reset_lock;
		/**< Channel lock for reset/abort support (via RCE) */
	struct mutex unpins_list_lock; /**< Lock for unpins_list */
	struct capture_common_unpins *unpins_list;
		/**< List of capture request buffer unpins */

	uint64_t vi_channel_mask;
		/**< Bitmask of RCE-assigned VI FW channel(s). */
	uint64_t vi2_channel_mask;
		/**< Bitmask of RCE-assigned VI FW channel(s) for 2nd VI. */
};

/**
 * @brief VI channel setup config (IOCTL payload).
 *
 * These fields are used to set up the VI channel and capture contexts, and will
 * be copied verbatim in the IVC capture_channel_config struct to allocate VI
 * resources in the RCE subsystem.
 */
struct vi_capture_setup {
	uint32_t channel_flags;
		/**<
		 * Bitmask for channel flags, see @ref CAPTURE_CHANNEL_FLAGS
		 */
	uint32_t error_mask_correctable;
		/**<
		 * Bitmask for correctable channel errors. See
		 * @ref CAPTURE_CHANNEL_ERRORS
		 */
	uint64_t vi_channel_mask;
		/**< Bitmask of VI channels to consider for allocation by RCE */
	uint64_t vi2_channel_mask;
		/**< Bitmask of 2nd VI channels */
	uint32_t queue_depth; /**< No. of capture descriptors in queue. */
	uint32_t request_size;
		/**< Size of a single capture descriptor [byte] */
	union {
		uint32_t mem; /**< Capture descriptors queue NvRm handle */
		uint64_t iova;
			/**<
			 * Capture descriptors queue base address (written back
			 * after pinning by KMD)
			 */
	};
	uint8_t slvsec_stream_main;
		/**< SLVS-EC main stream (hardcode to 0x00) */
	uint8_t slvsec_stream_sub;
		/**< SLVS-EC sub stream (hardcode to 0xFF - disabled) */
	uint16_t __pad_slvsec1;

	uint32_t csi_stream_id; /**< NVCSI PixelParser index [0-5] */
	uint32_t virtual_channel_id; /**< Virtual Channel index [0-15] */
	uint32_t csi_port; /**< NVCSI Port [0-7], not valid for TPG */
	uint32_t __pad_csi; /**< Reserved */

	uint32_t error_mask_uncorrectable;
		/**<
		 * Bitmask for correctable channel errors. See
		 * @ref CAPTURE_CHANNEL_ERRORS
		 */
	uint64_t stop_on_error_notify_bits;
		/**<
		 * Bitmask for NOTIFY errors that force channel stop upon
		 * receipt
		 */
	uint64_t reserved[2];
} __VI_CAPTURE_ALIGN;

/**
 * @brief VI capture info (resp. to query).
 */
struct vi_capture_info {
	struct vi_capture_syncpts {
		uint32_t progress_syncpt; /**< Progress syncpoint id */
		uint32_t progress_syncpt_val; /**< Progress syncpoint value. */
		uint32_t emb_data_syncpt; /**< Embedded metadata syncpoint id */
		uint32_t emb_data_syncpt_val;
			/**< Embedded metadata syncpt value. */
		uint32_t line_timer_syncpt; /**< Line timer syncpoint id */
		uint32_t line_timer_syncpt_val;
			/**< Line timer syncpoint value */
	} syncpts;
	uint32_t hw_channel_id; /**< RCE-assigned VI FW channel id */
	uint32_t __pad;
	uint64_t vi_channel_mask;
		/**< Bitmask of RCE-assigned VI FW channel(s) */
	uint64_t vi2_channel_mask;
		/**< Bitmask of RCE-assigned VI FW channel(s) for 2nd VI */
} __VI_CAPTURE_ALIGN;

/**
 * @brief Container for CAPTURE_CONTROL_MSG req./resp. from FuSa UMD (IOCTL
 * payload).
 *
 * The response and request pointers may be to the same memory allocation; in
 * which case the request message will be overwritten by the response.
 */
struct vi_capture_control_msg {
	uint64_t ptr; /**< Pointer to capture-control message req. */
	uint32_t size; /**< Size of req./resp. msg [byte] */
	uint32_t __pad;
	uint64_t response; /**< Pointer to capture-control message resp. */
} __VI_CAPTURE_ALIGN;

/**
 * @brief VI capture request (IOCTL payload).
 */
struct vi_capture_req {
	uint32_t buffer_index; /**< Capture descriptor index. */
	uint32_t num_relocs; /**< No. of surface buffers to pin/reloc. */
	uint64_t reloc_relatives;
		/**<
		 * Offsets to surface buffer addresses to patch in capture
		 * descriptor [byte].
		 */
} __VI_CAPTURE_ALIGN;

/**
 * @brief VI capture progress status setup config (IOCTL payload)
 */
struct vi_capture_progress_status_req {
	uint32_t mem; /**< NvRm handle to buffer region start. */
	uint32_t mem_offset; /**< Status notifier offset [byte]. */
	uint32_t buffer_depth; /**< Capture descriptor queue size [num]. */
	uint32_t __pad[3];
} __VI_CAPTURE_ALIGN;

/**
 * @brief Add VI capture surface buffer to management table (IOCTL payload)
 */
struct vi_buffer_req {
	uint32_t mem; /**< NvRm handle to buffer. */
	uint32_t flag; /**< Buffer @ref CAPTURE_BUFFER_OPS bitmask. */
} __VI_CAPTURE_ALIGN;

/**
 * @brief The compand configuration describes a piece-wise linear tranformation
 * function used by the VI companding module.
 */
#define VI_CAPTURE_NUM_COMPAND_KNEEPTS 10

/**
 * @brief VI compand setup config (IOCTL payload).
 */
struct vi_capture_compand {
	uint32_t base[VI_CAPTURE_NUM_COMPAND_KNEEPTS];
		/**< kneept base param. */
	uint32_t scale[VI_CAPTURE_NUM_COMPAND_KNEEPTS];
		/**< kneept scale param. */
	uint32_t offset[VI_CAPTURE_NUM_COMPAND_KNEEPTS];
		/**< kneept offset param. */
} __VI_CAPTURE_ALIGN;

/**
 * @brief Initialize a VI channel capture context (at channel open).
 *
 * The VI channel context is already partially-initialized by the calling
 * function, the channel capture context is allocated and linked here.
 *
 * @param[in,out]	chan		Allocated VI channel context,
 *					partially-initialized
 * @param[in]		is_mem_pinned	Whether capture request memory is pinned
 *
 * @returns		0 (success), neg. errno (failure)
 */
int vi_capture_init(
	struct tegra_vi_channel *chan,
	bool is_mem_pinned);

/**
 * @brief De-initialize a VI capture channel, closing open VI/NVCSI streams, and
 * freeing the buffer management table and channel capture context.
 *
 * The VI channel context is not freed in this function, only the capture
 * context is.
 *
 * @param[in,out]	chan		VI channel context
 */
void vi_capture_shutdown(
	struct tegra_vi_channel *chan);

/**
 * @brief Select the NvHost VI client instance platform driver to be
 * associated with the channel.
 * Only used in the case where VI standalone driver is used
 * to enumerate the VI channel character drivers
 *
 * @param[in/out]	chan	VI channel context
 * @param[in]	setup	VI channel setup config
 *
 */
void vi_get_nvhost_device(
	struct tegra_vi_channel *chan,
	struct vi_capture_setup *setup);

/**
 * @brief The function returns the corresponding NvHost VI client device
 * pointer associated with the NVCSI stream Id. A NULL value is returned
 * if invalid input parameters are passed.
 *
 * @param[in]	pdev	VI capture platform device pointer
 * @param[in]	csi_stream_id	NVCSI stream Id
 *
 * @returns		reference to VI device (success), null (failure)
 */
struct device *vi_csi_stream_to_nvhost_device(
	struct platform_device *pdev,
	uint32_t csi_stream_id);

/**
 * @brief Open a VI channel in RCE, sending channel configuration to request a
 * HW channel allocation. Syncpoints are allocated by the KMD in this
 * subroutine.
 *
 * @param[in,out]	chan	VI channel context
 * @param[in]		setup	VI channel setup config
 *
 * @returns		0 (success), neg. errno (failure)
 */
int vi_capture_setup(
	struct tegra_vi_channel *chan,
	struct vi_capture_setup *setup);

/**
 * @brief Get the pointer to tegra_vi_channel struct associated with the
 * stream id and virtual id passed as function input params.
 *
 * If no valid tegra_vi_channel pointer is found associated with the given
 * stream id/ VC id combo then NULL is returned.
 *
 * @param[in]	stream_id	CSI stream ID
 * @param[in]	virtual_channel_id	CSI virtual channel ID
 *
 * @returns		pointer to tegra_vi_channel(success), NULL(failure)
 */
struct tegra_vi_channel *get_tegra_vi_channel(
	unsigned int stream_id,
	unsigned int virtual_channel_id);
/**
 * @brief Reset an opened VI channel, all pending capture requests to RCE are
 * discarded.
 *
 * The channel's progress syncpoint is advanced to the threshold of the latest
 * capture request to unblock any waiting observers.
 *
 * A reset barrier may be enqueued in the capture IVC channel to flush stale
 * capture descriptors, in case of abnormal channel termination.
 *
 * @param[in]	chan		VI channel context
 * @param[in]	reset_flags	Bitmask for VI channel reset options
 *				(CAPTURE_CHANNEL_RESET_FLAG_*)
 *
 * @returns	0 (success), neg. errno (failure)
 */
int vi_capture_reset(
	struct tegra_vi_channel *chan,
	uint32_t reset_flags);

/**
 * @brief Release an opened VI channel; the RCE channel allocation, syncpts and
 * IVC channel callbacks are released.
 *
 * @param[in]	chan		VI channel context
 * @param[in]	reset_flags	Bitmask for VI channel reset options
 *				(CAPTURE_CHANNEL_RESET_FLAG_*)
 *
 *  @returns	0 (success), neg. errno (failure)
 */
int vi_capture_release(
	struct tegra_vi_channel *chan,
	uint32_t reset_flags);

/**
 * @brief Release the TPG and/or NVCSI stream on a VI channel, if they are
 * active.
 *
 * This function normally does not execute except in the event of abnormal UMD
 * termination, as it is the client's responsibility to open and close NVCSI and
 * TPG sources.
 *
 * @param[in]	chan	VI channel context
 *
 * @returns	0 (success), neg. errno (failure)
 */
int csi_stream_release(
	struct tegra_vi_channel *chan);

/**
 * @brief Send a capture-control IVC message to RCE and wait for a response.
 *
 * This is a blocking call, with the possibility of timeout.
 *
 * @param[in]		chan	VI channel context
 * @param[in,out]	msg	capture-control IVC container w/ req./resp. pair
 *
 * @returns		0 (success), neg. errno (failure)
 */
int vi_capture_control_message(
	struct tegra_vi_channel *chan,
	struct vi_capture_control_msg *msg);

/**
 * @brief Send a capture-control IVC message which is received from
 * userspace to RCE and wait for a response.
 *
 * This is a blocking call, with the possibility of timeout.
 *
 * @param[in]		chan	VI channel context
 * @param[in,out]	msg	capture-control IVC container w/ req./resp. pair
 *
 * @returns		0 (success), neg. errno (failure)
 */
int vi_capture_control_message_from_user(
	struct tegra_vi_channel *chan,
	struct vi_capture_control_msg *msg);

/**
 * @brief Query a VI channel's syncpt ids and values, and retrieve the
 * RCE-assigned VI FW channel id and mask.
 *
 * @param[in]	chan	VI channel context
 * @param[out]	info	VI channel info response
 * @returns	0 (success), neg. errno (failure)
 */
int vi_capture_get_info(
	struct tegra_vi_channel *chan,
	struct vi_capture_info *info);

/**
 * @brief Send a capture request for a frame via the capture IVC channel to RCE.
 *
 * This is a non-blocking call.
 *
 * @param[in]	chan	VI channel context
 * @param[in]	req	VI capture request
 *
 * @returns	0 (success), neg. errno (failure)
 */
int vi_capture_request(
	struct tegra_vi_channel *chan,
	struct vi_capture_req *req);

/**
 * @brief Wait on receipt of the capture status of the head of the capture
 *	  request FIFO queue to RCE. The RCE VI driver sends a
 *	  CAPTURE_STATUS_IND notification at frame completion.
 *
 * This is a blocking call, with the possibility of timeout.
 *
 * @param[in]	chan		VI channel context
 * @param[in]	timeout_ms	Time to wait for status completion [ms], set to
 *				0 for indefinite
 *
 * @returns	0 (success), neg. errno (failure)
 */
int vi_capture_status(
	struct tegra_vi_channel *chan,
	int32_t timeout_ms);

/**
 * @brief Setup VI compand in RCE.
 *
 * @param[in]	chan	VI channel context
 * @param[in]	compand	VI compand setup config
 *
 * @returns	0 (success), neg. errno (failure)
 */
int vi_capture_set_compand(
	struct tegra_vi_channel *chan,
	struct vi_capture_compand *compand);

/**
 * @brief Setup VI channel capture status progress notifier.
 *
 * @param[in]	chan	VI channel context
 * @param[in]	req	VI capture progress status setup config
 *
 * @returns	0 (success), neg. errno (failure)
 */
int vi_capture_set_progress_status_notifier(
	struct tegra_vi_channel *chan,
	struct vi_capture_progress_status_req *req);

int vi_stop_waiting(struct tegra_vi_channel *chan);

#endif /* __FUSA_CAPTURE_VI_H__ */
