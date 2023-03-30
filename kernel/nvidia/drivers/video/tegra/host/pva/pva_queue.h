/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION. All rights reserved.
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

#ifndef PVA_QUEUE_H
#define PVA_QUEUE_H

#include <uapi/linux/nvpva_ioctl.h>
#include "nvpva_queue.h"
#include "nvpva_buffer.h"
#include "pva-sys-params.h"
#include "pva-interface.h"
#include "pva-task.h"

#define task_err(task, fmt, ...) \
	dev_err(&task->pva->pdev->dev, fmt, ##__VA_ARGS__)

struct dma_buf;

extern struct nvpva_queue_ops pva_queue_ops;

struct pva_pinned_memory {
	int id;
	dma_addr_t dma_addr;
	size_t size;
	struct dma_buf *dmabuf;
	enum nvpva_buffers_heap heap;
};

struct pva_cb {
	dma_addr_t head_addr;
	uint32_t *head_va;
	dma_addr_t tail_addr;
	uint32_t *tail_va;
	dma_addr_t err_addr;
	uint32_t *err_va;
	dma_addr_t buffer_addr;
	uint8_t *buffer_va;
	uint32_t tail;
	uint32_t size;
};

/**
 * @brief	Describe a task for PVA
 *
 * This is an internal representation of the task structure. All
 * pointers refer to kernel memory.
 *
 * pva				Pointer to struct pva
 * buffers			Pointer to struct nvpva_buffers
 * queue			Pointer to struct nvpva_queue
 * node				Used to build queue task list
 * kref				Used to manage allocation and freeing
 * dma_addr			task dma_addr_t
 * va				task virtual address
 * pool_index			task pool index
 * postfence_va			postfence virtual address
 * num_prefences		Number of pre-fences in this task
 * num_postfences		Number of post-fences in this task
 * num_input_surfaces		Number of input surfaces
 * num_output_surfaces		Number of output surfaces
 * num_input_task_status	Number of input task status structures
 * num_output_task_status	Number of output task status structures
 * operation			task operation
 * timeout			Latest Unix time when the task must complete or
 *				0 if disabled.
 * prefences			Pre-fence structures
 * postfences			Post-fence structures
 * input_surfaces		Input surfaces structures
 * input_scalars		Information for input scalars
 * output_surfaces		Output surfaces
 * output_scalars		Information for output scalars
 * input_task_status		Input status structure
 * output_task_status		Output status structure
 *
 */
struct pva_submit_task {
	struct pva *pva;
	struct nvpva_queue *queue;
	struct nvpva_client_context *client;

	struct list_head node;
	struct kref ref;

	dma_addr_t dma_addr;
	void *va;
	int pool_index;

	bool pinned_app;
	u32 exe_id;

	u32 l2_alloc_size; /* Not applicable for Xavier */
	struct pva_cb *stdout;
	u32 symbol_payload_size;

	u32 flags;
	u8 num_prefences;
	u8 num_user_fence_actions;
	u8 num_input_task_status;
	u8 num_output_task_status;
	u8 num_dma_descriptors;
	u8 num_dma_channels;
	u8 num_symbols;
	u8 special_access;

	u64 timeout;
	u64 desc_hwseq_frm;
	u32 syncpt_thresh;
	u32 fence_num;

	u32 sem_thresh;
	u32 sem_num;

	/* Data provided by userspace "as is" */
	struct nvpva_submit_fence prefences[NVPVA_TASK_MAX_PREFENCES];
	struct nvpva_fence_action
		user_fence_actions[NVPVA_MAX_FENCE_TYPES *
				   NVPVA_TASK_MAX_FENCEACTIONS];
	struct nvpva_mem input_task_status[NVPVA_TASK_MAX_INPUT_STATUS];
	struct nvpva_mem output_task_status[NVPVA_TASK_MAX_OUTPUT_STATUS];
	struct nvpva_dma_descriptor
		dma_descriptors[NVPVA_TASK_MAX_DMA_DESCRIPTORS];
	struct nvpva_dma_channel dma_channels
		[NVPVA_TASK_MAX_DMA_CHANNELS_T23X]; /* max of T19x & T23x */
	struct nvpva_dma_misr dma_misr_config;
	struct nvpva_hwseq_config hwseq_config;
	struct nvpva_symbol_param symbols[NVPVA_TASK_MAX_SYMBOLS];
	u8 symbol_payload[NVPVA_TASK_MAX_PAYLOAD_SIZE];

	struct pva_pinned_memory pinned_memory[256];
	u32 num_pinned;
	u8 num_pva_fence_actions[NVPVA_MAX_FENCE_TYPES];
	struct nvpva_fence_action
		pva_fence_actions[NVPVA_MAX_FENCE_TYPES]
				 [NVPVA_TASK_MAX_FENCEACTIONS];
	/** Store Suface base address */
	u64 src_surf_base_addr;
	u64 dst_surf_base_addr;
	bool is_system_app;
};

struct pva_submit_tasks {
	struct pva_submit_task *tasks[NVPVA_SUBMIT_MAX_TASKS];
	u32 task_thresh[NVPVA_SUBMIT_MAX_TASKS];
	u16 num_tasks;
	u64 execution_timeout_us;
};

#define ACTION_LIST_FENCE_SIZE 21U
#define ACTION_LIST_STATUS_OPERATION_SIZE 11U
#define ACTION_LIST_TERMINATION_SIZE 1U
#define ACTION_LIST_STATS_SIZE 9U
#define PVA_TSC_TICKS_TO_US_FACTOR (0.032f)

/*
 * The worst-case input action buffer size:
 * - Prefences trigger a word memory operation (size 13 bytes)
 * - Input status reads trigger a half-word memory operation (size 11 bytes)
 * - The action list is terminated by a null action (1 byte)
 */
#define INPUT_ACTION_BUFFER_SIZE                                               \
	ALIGN(((NVPVA_TASK_MAX_PREFENCES * ACTION_LIST_FENCE_SIZE) +           \
	       ((NVPVA_TASK_MAX_FENCEACTIONS * 2U) * ACTION_LIST_FENCE_SIZE) + \
	       NVPVA_TASK_MAX_INPUT_STATUS *                                   \
		       ACTION_LIST_STATUS_OPERATION_SIZE +                     \
	       ACTION_LIST_TERMINATION_SIZE),                                  \
	      256)

/**
 * Ensure that sufficient preactions per task are supported by FW/KMD interface.
 * Maximum possible number of preactions can be determined by adding below
 * limits:
 * - Maximum number of prefences allowed per task
 * - Maximum number of SOT_R and SOT_V fences allowed per task
 * - Maximum number of input status buffers allowed per task
 */
#if ((PVA_MAX_PREACTION_LISTS) < \
		( \
			(NVPVA_TASK_MAX_PREFENCES) + \
			(NVPVA_TASK_MAX_FENCEACTIONS * 2U) + \
			(NVPVA_TASK_MAX_INPUT_STATUS) \
		) \
	)
#error "Insufficient preactions supported by FW/KMD interface"
#endif

/**
 * Ensure that sufficient postactions per task are supported by FW/KMD interface.
 * Maximum possible number of postactions can be determined by adding below
 * limits:
 * - Maximum number of EOT_V, EOT_R and EOT fences allowed per task
 * - Maximum number of output status buffers allowed per task
 * - Maximum one postaction for statistics
 */
#if ((PVA_MAX_POSTACTION_LISTS) < \
		( \
			(NVPVA_TASK_MAX_FENCEACTIONS * 3U) + \
			(NVPVA_TASK_MAX_OUTPUT_STATUS) + \
			(1U) \
		) \
	)
#error "Insufficient postactions supported by FW/KMD interface"
#endif

struct PVA_PACKED pva_task_action_ptr_s {
	/* IOVA Pointer to update Sync Point Value */
	pva_iova			p;
	/* Value to be written to Sync Point */
	uint32_t			v;
	/* Pointer to write timestamp */
	pva_iova			t;
};

struct PVA_PACKED pva_task_action_status_s {
	/* IOVA to pva_gen_task_status_t struct */
	pva_iova		p;
	uint16_t		status;
};

struct PVA_PACKED pva_task_action_statistics_s {
	/* IOVA to pva_task_statistics_t struct */
	pva_iova			p;
};
struct PVA_PACKED pva_task_action_s {
	uint8_t		action;
	union {
		struct pva_task_action_ptr_s		ptr;
		struct pva_task_action_status_s		status;
		struct pva_task_action_statistics_s	statistics;
	} args;
};

struct pva_hw_task {
	struct pva_td_s task;
	struct pva_task_action_s preactions[PVA_MAX_PREACTION_LISTS];
	struct pva_task_action_s postactions[PVA_MAX_POSTACTION_LISTS];
	struct pva_dma_info_s dma_info;
	struct pva_dma_misr_config_s dma_misr_config;
	struct pva_dtd_s dma_desc[NVPVA_TASK_MAX_DMA_DESCRIPTORS];
	struct pva_vpu_parameter_info_s param_info;
	struct pva_vpu_parameters_s param_list[NVPVA_TASK_MAX_SYMBOLS];
	u8 sym_payload[NVPVA_TASK_MAX_PAYLOAD_SIZE];
	struct pva_task_statistics_s statistics;
	struct pva_circular_buffer_info_s stdout_cb_info;
};

void pva_task_remove(struct pva_submit_task *task);
void pva_task_free(struct kref *ref);

void pva_task_update(struct work_struct *work);

struct pva_pinned_memory *pva_task_pin_mem(struct pva_submit_task *task,
					   u32 id,
					   bool is_cntxt);

void pva_dmabuf_vunmap(struct dma_buf *dmabuf, void *addr);
void *pva_dmabuf_vmap(struct dma_buf *dmabuf);

#endif
