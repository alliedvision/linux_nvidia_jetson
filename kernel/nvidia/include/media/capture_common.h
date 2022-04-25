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

#include <media/mc_common.h>

/* Progress status */
#define PROGRESS_STATUS_BUSY		(U32_C(0x1))
#define PROGRESS_STATUS_DONE		(U32_C(0x2))

#define BUFFER_READ	(U32_C(0x01))
#define BUFFER_WRITE	(U32_C(0x02))
#define BUFFER_ADD	(U32_C(0x04))
#define BUFFER_RDWR	(BUFFER_READ|BUFFER_WRITE)
/** @brief  max pin count per request. Used to preallocate unpin list */
#define MAX_PIN_BUFFER_PER_REQUEST 	(U32_C(24))


/**
 * @struct capture_buffer_table
 * A structure holding buffer mapping relationship for a device
 */
struct capture_buffer_table;

struct capture_buffer_table *create_buffer_table(struct device *dev);

void destroy_buffer_table(struct capture_buffer_table *tab);

int capture_buffer_request(
	struct capture_buffer_table *tab, uint32_t memfd, uint32_t flag);

static inline int
capture_buffer_add(struct capture_buffer_table *t, uint32_t fd)
{
	return capture_buffer_request(t, fd, BUFFER_ADD | BUFFER_RDWR);
}

struct capture_mapping;

void put_mapping(
	struct capture_buffer_table *t, struct capture_mapping *pin);

/* buffer details including dma_buf and iova etc. */
struct capture_common_buf {
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t iova;
	void *va; /**< virtual address for kernel access */
};

/* unpin details for a capture channel, per request */
struct capture_common_unpins {
	uint32_t num_unpins;
	struct capture_mapping *data[MAX_PIN_BUFFER_PER_REQUEST]; /**< Surface buffers to unpin */
};

struct capture_common_status_notifier {
	struct dma_buf *buf;
	void *va;
	uint32_t offset;
};

int capture_common_setup_progress_status_notifier(
		struct capture_common_status_notifier *status_notifier,
		uint32_t mem,
		uint32_t buffer_size,
		uint32_t mem_offset);

int capture_common_set_progress_status(
		struct capture_common_status_notifier *progress_status_notifier,
		uint32_t buffer_slot,
		uint32_t buffer_depth, uint8_t new_val);

int capture_common_release_progress_status_notifier(
		struct capture_common_status_notifier *progress_status_notifier);

int capture_common_pin_memory(struct device *dev,
		uint32_t mem, struct capture_common_buf *unpin_data);

void capture_common_unpin_memory(struct capture_common_buf *unpin_data);

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

