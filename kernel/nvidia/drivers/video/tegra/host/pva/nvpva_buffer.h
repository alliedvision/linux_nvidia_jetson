/*
 * NVPVA Buffer Management Header
 *
 * Copyright (c) 2016-2022, NVIDIA Corporation.  All rights reserved.
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

#ifndef __NVPVA_NVPVA_BUFFER_H__
#define __NVPVA_NVPVA_BUFFER_H__

#include <linux/dma-buf.h>
#include "pva_bit_helpers.h"

enum nvpva_buffers_heap {
	NVPVA_BUFFERS_HEAP_DRAM = 0,
	NVPVA_BUFFERS_HEAP_CVNAS
};

/**
 * @brief		Information needed for buffers
 *
 * pdev			Pointer to NVHOST device
 * rb_root		RB tree root for of all the buffers used by a file pointer
 * list			List for traversing through all the buffers
 * mutex		Mutex for the buffer tree and the buffer list
 * kref			Reference count for the bufferlist
 * ids			unique ID assigned to a pinned buffer
 */
#define NVPVA_ID_SEGMENT_SIZE		32
#define NVPVA_MAX_NUM_UNIQUE_IDS	(NVPVA_ID_SEGMENT_SIZE * 1024)
#define NVPVA_NUM_ID_SEGMENTS						\
			(NVPVA_MAX_NUM_UNIQUE_IDS/NVPVA_ID_SEGMENT_SIZE)
struct nvpva_buffers {
	struct platform_device *pdev;
	struct platform_device *pdev_cntxt;
	struct list_head list_head;
	struct rb_root rb_root;
	struct rb_root rb_root_id;
	struct mutex mutex;
	struct kref kref;
	uint32_t ids[NVPVA_NUM_ID_SEGMENTS];
	uint32_t num_assigned_ids;
};

/**
 * @brief			Initialize the nvpva_buffer per open request
 *
 * This function allocates	nvpva_buffers struct and init the bufferlist
 * and mutex.
 *
 * @param nvpva_buffers		Pointer to nvpva_buffers struct
 * @return			nvpva_buffers pointer on success
 *				or negative on error
 *
 */
struct nvpva_buffers
*nvpva_buffer_init(struct platform_device *pdev,
		   struct platform_device *pdev_cntxt);

/**
 * @brief			Pin the memhandle using dma_buf functions
 *
 * This function maps the buffer memhandle list passed from user side
 * to device iova.
 *
 * @param nvpva_buffers		Pointer to nvpva_buffers struct
 * @param dmabufs		Pointer to dmabuffer list
 * @param offset		pointer to offsets of regions to be pinned
 * @param size			pointer to sizes of regions to be pinned
 * @param count			Number of memhandles in the list
 * @return			0 on success or negative on error
 *
 */
int nvpva_buffer_pin(struct nvpva_buffers *nvpva_buffers,
		     struct dma_buf **dmabufs,
		     u64 *offset,
		     u64 *size,
		     u32 count,
		     u32 *id,
		     u32 *eerr);
/**
 * @brief			UnPins the mapped address space.
 *
 * @param nvpva_buffers		Pointer to nvpva_buffer struct
 * @param dmabufs		Pointer to dmabuffer list
 * @param count			Pointer to offset list
 * @param offset		Pointer to size list
 * @param count			Number of memhandles in the list
 * @return			None
 *
 */
void
nvpva_buffer_unpin(struct nvpva_buffers *nvpva_buffers,
		   struct dma_buf **dmabufs,
		   u64 *offset,
		   u64 *size,
		   u32 count);
/**
 * @brief			UnPins the mapped address space.
 *
 * @param nvpva_buffers		Pointer to nvpva_buffer struct
 * @param ids			Pointer to id list
 * @param count			Number of memhandles in the list
 * @param id			pointer to variable where assigned
 *				ID is returned
 * @return			None
 *
 */
void nvpva_buffer_unpin_id(struct nvpva_buffers *nvpva_buffers,
				u32 *ids,
				u32 count);

/**
 * @brief			Pin the mapped buffer for a task submit
 *
 * This function increased the reference count for a mapped buffer during
 * task submission.
 *
 * @param nvpva_buffers		Pointer to nvpva_buffer struct
 * @param dmabufs		Pointer to dmabuffer list
 * @param count			Number of memhandles in the list
 * @param paddr			Pointer to IOVA list
 * @param psize			Pointer to size of buffer to return
 * @param heap			Pointer to a list of heaps. This is
 *				filled by the routine.
 *
 * @return			0 on success or negative on error
 *
 */
int nvpva_buffer_submit_pin(struct nvpva_buffers *nvpva_buffers,
			     struct dma_buf **dmabufs, u32 count,
			     dma_addr_t *paddr, size_t *psize,
			     enum nvpva_buffers_heap *heap);
/**
 * @brief			Pin the mapped buffer for a task submit
 *
 * This function increased the reference count for a mapped buffer during
 * task submission.
 *
 * @param nvpva_buffers		Pointer to nvpva_buffer struct
 * @param ids			Pointer to id list
 * @param count			Number of memhandles in the list
 * @param paddr			Pointer to IOVA list
 * @param psize			Pointer to size of buffer to return
 * @param heap			Pointer to a list of heaps. This is
 *				filled by the routine.
 *
 * @return			0 on success or negative on error
 *
 */
int nvpva_buffer_submit_pin_id(struct nvpva_buffers *nvpva_buffers,
			       u32 *ids,
			       u32 count,
			       struct dma_buf **dmabuf,
			       dma_addr_t *paddr,
			       size_t *psize,
			       enum nvpva_buffers_heap *heap,
			       bool is_cntxt);

/**
 * @brief		UnPins the mapped address space on task completion.
 *
 * This function decrease the reference count for a mapped buffer when the
 * task get completed or aborted.
 *
 * @param nvpva_buffers		Pointer to nvpva_buffer struct
 * @param dmabufs		Pointer to dmabuffer list
 * @param count			Number of memhandles in the list
 * @return			None
 *
 */
void nvpva_buffer_submit_unpin(struct nvpva_buffers *nvpva_buffers,
					struct dma_buf **dmabufs, u32 count);

/**
 * @brief		UnPins the mapped address space on task completion.
 *
 * This function decrease the reference count for a mapped buffer when the
 * task get completed or aborted.
 *
 * @param nvpva_buffers		Pointer to nvpva_buffer struct
 * @param ids			Pointer to dmabuffer list
 * @param count			Number of memhandles in the list
 * @return			None
 *
 */
void nvpva_buffer_submit_unpin_id(struct nvpva_buffers *nvpva_buffers,
					u32 *ids, u32 count);

/**
 * @brief			Drop a user reference to buffer structure
 *
 * @param nvpva_buffers		Pointer to nvpva_buffer struct
 * @return			None
 *
 */
void nvpva_buffer_release(struct nvpva_buffers *nvpva_buffers);

/**
 * @brief		Returns dma buf and dma addr for a given handle
 *
 * @param nvpva_buffers		Pointer to nvpva_buffer struct
 * @param dmabuf		dma buf pointer to search for
 * @param addr			dma_addr_t pointer to return
 * @return			0 on success or negative on error
 *
 */
int nvpva_get_iova_addr(struct nvpva_buffers *nvpva_buffers,
			struct dma_buf *dmabuf, dma_addr_t *addr);

#endif /*__NVPVA_NVPVA_BUFFER_H__ */
