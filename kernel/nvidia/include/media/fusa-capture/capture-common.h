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
 * @file include/media/fusa-capture/capture-common.h
 *
 * @brief VI/ISP channel common operations header for the T186/T194 Camera RTCPU
 * platform.
 */

#ifndef __FUSA_CAPTURE_COMMON_H__
#define __FUSA_CAPTURE_COMMON_H__

#include <media/mc_common.h>

struct capture_buffer_table;
struct capture_mapping;

/**
 * @defgroup CAPTURE_PROGRESS_NOTIFIER_STATES
 *
 * Progress state of a capture request.
 *
 * @note PROGRESS_STATUS_DONE only means that the capture request has been
 *	 completed, the descriptor status must still be read in order to
 *	 determine whether or not it was successful or in error.
 *
 * @{
 */

/** Capture request is enqueued or in progress */
#define PROGRESS_STATUS_BUSY		(U32_C(0x1))
/** Capture request is complete and the data can be consumed */
#define PROGRESS_STATUS_DONE		(U32_C(0x2))

/** @} */

/**
 * @defgroup CAPTURE_BUFFER_OPS
 *
 * Capture surface buffer operations and DMA directions.
 *
 * @{
 */

/** @brief DMA @em to device data direction. */
#define BUFFER_READ	(U32_C(0x01))

/** @brief DMA @em from device data direction. */
#define BUFFER_WRITE	(U32_C(0x02))

/** @brief Add buffer to the channel's management table. */
#define BUFFER_ADD	(U32_C(0x04))

/** @brief DMA bidirectional data direction. */
#define BUFFER_RDWR	(BUFFER_READ | BUFFER_WRITE)

/** @} */

/** @brief  max pin count per request. Used to preallocate unpin list */
#define MAX_PIN_BUFFER_PER_REQUEST 	(U32_C(24))



/**
 * @brief Initialize the capture surface management table for SLAB allocations.
 *
 * @param[in]	dev	Originating device (VI or ISP)
 *
 * @returns	pointer to table on success, NULL on error
 */
struct capture_buffer_table *create_buffer_table(
	struct device *dev);

/**
 * @brief Release all capture buffers and free the management table
 *
 * @param[in,out]	tab	Surface buffer management table
 */
void destroy_buffer_table(
	struct capture_buffer_table *tab);

/**
 * @brief Perform a buffer management operation on a capture surface buffer.
 *
 * @param[in,out]	tab	Surface buffer management table
 * @param[in]		memfd	FD or NvRm handle to buffer
 * @param[in]		flag	Surface BUFFER_* op bitmask
 *
 * @returns		0 (success), neg. errno (failure)
 */
int capture_buffer_request(
	struct capture_buffer_table *tab,
	uint32_t memfd,
	uint32_t flag);

/**
 * @brief Add a capture surface buffer to the buffer management table.
 *
 * @param[in,out]	t	Surface buffer management table
 * @param[in]		fd	FD or NvRm handle to buffer
 *
 * @returns		0 (success), neg. errno (failure)
 */
int capture_buffer_add(
	struct capture_buffer_table *t,
	uint32_t fd);

/**
 * @brief Decrement refcount for buffer mapping, and release it if it reaches
 * zero, unless it is a preserved mapping.
 *
 * @param[in,out]	t	Surface buffer management table
 * @param[in,out]	pin	Surface buffer to unpin
 */
void put_mapping(
	struct capture_buffer_table *t,
	struct capture_mapping *pin);

/**
 * @brief Capture surface buffer context.
 */
struct capture_common_buf {
	struct dma_buf *buf; /**< dma_buf context */
	struct dma_buf_attachment *attach; /**< dma_buf attachment context */
	struct sg_table *sgt; /**< scatterlist table */
	dma_addr_t iova; /**< dma address */
	void *va; /**< virtual address for kernel access */
};

/**
 * @brief List of buffers to unpin for a capture request.
 */
struct capture_common_unpins {
	uint32_t num_unpins; /**< No. of entries in data[] */
	struct capture_mapping *data[MAX_PIN_BUFFER_PER_REQUEST]; /**< Surface buffers to unpin */
};

/**
 * @brief Progress status notifier handle.
 */
struct capture_common_status_notifier {
	struct dma_buf *buf; /**< dma_buf handle */
	void *va; /**< buffer virtual mapping to kernel address space */
	uint32_t offset; /**< status notifier offset [byte] */
};

/**
 * @brief Setup the progress status notifier handle
 *
 * @param[in]	status_notifer	Progress status notifier handle
 * @param[in]	mem		FD or NvRm handle to buffer
 * @param[in]	buffer_size	Buffer size [byte]
 * @param[in]	mem_offset	Status notifier offset [byte]
 *
 * @returns	0 (success), neg. errno (failure)
 */
int capture_common_setup_progress_status_notifier(
	struct capture_common_status_notifier *status_notifier,
	uint32_t mem,
	uint32_t buffer_size,
	uint32_t mem_offset);

/**
 * @brief Release the progress status notifier handle.
 *
 * @param[in,out]	progress_status_notifier	Progress status notifier
 *							handle to release
 *
 * @returns		0
 */
int capture_common_release_progress_status_notifier(
	struct capture_common_status_notifier *progress_status_notifier);

/**
 * @brief Update the progress status for a capture request.
 *
 * @param[in]	progress_status_notifier	Progress status notifier handle
 * @param[in]	buffer_slot			Capture descriptor index
 * @param[in]	buffer_depth			Capture descriptor queue size
 * @param[in]	new_val				Progress status to set
 *
 * @returns	0 (success), neg. errno (failure)
 */
int capture_common_set_progress_status(
	struct capture_common_status_notifier *progress_status_notifier,
	uint32_t buffer_slot,
	uint32_t buffer_depth,
	uint8_t new_val);

/**
 * @brief Pins buffer memory, returns dma_buf handles for unpinning.
 *
 * @param[in]	dev		target device (rtcpu)
 * @param[in]	mem		FD or NvRm handle to buffer
 * @param[out]	unpin_data	struct w/ dma_buf handles for unpinning
 *
 * @returns	0 (success), neg. errno (failure)
 */
int capture_common_pin_memory(
	struct device *dev,
	uint32_t mem,
	struct capture_common_buf *unpin_data);

/**
 * @brief Unpins buffer memory, releasing dma_buf resources.
 *
 * @param[in,out]	unpin_data	data handle to be unpinned
 */
void capture_common_unpin_memory(
	struct capture_common_buf *unpin_data);

/**
 * @brief Pins (maps) the physical address for provided capture surface address
 * and updates the iova pointer.
 *
 * @param[in,out] 	buf_ctx			Surface buffer management table
 * @param[in] 		mem_handle		Memory handle (descriptor). Can be NULL,
 *						in this case function will do nothing and
 *						and return 0. This is to simplify handling of
 *						capture descriptors data fields, NULL indicates
 *						unused memory surface.
 * @param[in] 		mem_offset		Offset inside memory buffer
 * @param[out] 		meminfo_base_address 	Surface iova address, including offset
 * @param[out] 		meminfo_size 		Size of iova range, excluding offset
 * @param[in,out]	unpins			Unpin data used to unref/unmap buffer
 * 						after capture
 */
int capture_common_pin_and_get_iova(struct capture_buffer_table *buf_ctx,
		uint32_t mem_handle, uint64_t mem_offset,
		uint64_t *meminfo_base_address, uint64_t *meminfo_size,
		struct capture_common_unpins *unpins);

#endif /* __FUSA_CAPTURE_COMMON_H__*/
