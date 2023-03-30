/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_GR_FECS_TRACE_H
#define NVGPU_GR_FECS_TRACE_H

#ifdef CONFIG_NVGPU_FECS_TRACE

#include <nvgpu/types.h>
#include <nvgpu/list.h>
#include <nvgpu/lock.h>
#include <nvgpu/periodic_timer.h>

/*
 * If HW circular buffer is getting too many "buffer full" conditions,
 * increasing this constant should help (it drives Linux' internal buffer size).
 */
#define GK20A_FECS_TRACE_NUM_RECORDS		(1 << 10)
#define GK20A_FECS_TRACE_FRAME_PERIOD_NS	(1000000000ULL/60ULL)
#define GK20A_FECS_TRACE_PTIMER_SHIFT		5

#define NVGPU_GPU_CTXSW_TAG_SOF                     0x00U
#define NVGPU_GPU_CTXSW_TAG_CTXSW_REQ_BY_HOST       0x01U
#define NVGPU_GPU_CTXSW_TAG_FE_ACK                  0x02U
#define NVGPU_GPU_CTXSW_TAG_FE_ACK_WFI              0x0aU
#define NVGPU_GPU_CTXSW_TAG_FE_ACK_GFXP             0x0bU
#define NVGPU_GPU_CTXSW_TAG_FE_ACK_CTAP             0x0cU
#define NVGPU_GPU_CTXSW_TAG_FE_ACK_CILP             0x0dU
#define NVGPU_GPU_CTXSW_TAG_SAVE_END                0x03U
#define NVGPU_GPU_CTXSW_TAG_RESTORE_START           0x04U
#define NVGPU_GPU_CTXSW_TAG_CONTEXT_START           0x05U
#define NVGPU_GPU_CTXSW_TAG_ENGINE_RESET            0xfeU
#define NVGPU_GPU_CTXSW_TAG_INVALID_TIMESTAMP       0xffU
#define NVGPU_GPU_CTXSW_TAG_LAST                    \
	NVGPU_GPU_CTXSW_TAG_INVALID_TIMESTAMP

#define NVGPU_GPU_CTXSW_FILTER_ISSET(n, p) \
	((p)->tag_bits[(n) / 64] &   (1U << ((n) & 63)))

#define NVGPU_GPU_CTXSW_FILTER_SIZE (NVGPU_GPU_CTXSW_TAG_LAST + 1)
#define NVGPU_FECS_TRACE_FEATURE_CONTROL_BIT 31

struct gk20a;
struct nvgpu_mem;
struct nvgpu_gr_subctx;
struct nvgpu_gr_ctx;
struct nvgpu_tsg;
struct vm_area_struct;

struct nvgpu_gr_fecs_trace {
	struct nvgpu_list_node context_list;
	struct nvgpu_mutex list_lock;

	struct nvgpu_mutex poll_lock;
	struct nvgpu_periodic_timer poll_timer;

	struct nvgpu_mutex enable_lock;
	u32 enable_count;
};

struct nvgpu_fecs_trace_record {
	u32 magic_lo;
	u32 magic_hi;
	u32 context_id;
	u32 context_ptr;
	u32 new_context_id;
	u32 new_context_ptr;
	u64 ts[];
};

/* must be consistent with nvgpu_ctxsw_ring_header */
struct nvgpu_ctxsw_ring_header_internal {
	u32 magic;
	u32 version;
	u32 num_ents;
	u32 ent_size;
	u32 drop_count;	/* excluding filtered out events */
	u32 write_seqno;
	u32 write_idx;
	u32 read_idx;
};

/*
 * The binary format of 'struct nvgpu_gpu_ctxsw_trace_entry' introduced here
 * should match that of 'struct nvgpu_ctxsw_trace_entry' defined in uapi
 * header, since this struct is intended to be a mirror copy of the uapi
 * struct.
 */
struct nvgpu_gpu_ctxsw_trace_entry {
	u8 tag;
	u8 vmid;
	u16 seqno;            /* sequence number to detect drops */
	u32 context_id;       /* context_id as allocated by FECS */
	u64 pid;              /* 64-bit is max bits of different OS pid */
	u64 timestamp;        /* 64-bit time */
};

struct nvgpu_gpu_ctxsw_trace_filter {
	u64 tag_bits[(NVGPU_GPU_CTXSW_FILTER_SIZE + 63) / 64];
};

struct nvgpu_fecs_trace_context_entry {
	u32 context_ptr;

	pid_t pid;
	u32 vmid;

	struct nvgpu_list_node entry;
};

static inline struct nvgpu_fecs_trace_context_entry *
nvgpu_fecs_trace_context_entry_from_entry(struct nvgpu_list_node *node)
{
	return (struct nvgpu_fecs_trace_context_entry *)
		((uintptr_t)node -
		offsetof(struct nvgpu_fecs_trace_context_entry, entry));
};

int nvgpu_gr_fecs_trace_init(struct gk20a *g);
int nvgpu_gr_fecs_trace_deinit(struct gk20a *g);

int nvgpu_gr_fecs_trace_num_ts(struct gk20a *g);
struct nvgpu_fecs_trace_record *nvgpu_gr_fecs_trace_get_record(struct gk20a *g,
	int idx);
bool nvgpu_gr_fecs_trace_is_valid_record(struct gk20a *g,
	struct nvgpu_fecs_trace_record *r);

int nvgpu_gr_fecs_trace_add_context(struct gk20a *g, u32 context_ptr,
	pid_t pid, u32 vmid, struct nvgpu_list_node *list);
void nvgpu_gr_fecs_trace_remove_context(struct gk20a *g, u32 context_ptr,
	struct nvgpu_list_node *list);
void nvgpu_gr_fecs_trace_remove_contexts(struct gk20a *g,
	struct nvgpu_list_node *list);
void nvgpu_gr_fecs_trace_find_pid(struct gk20a *g, u32 context_ptr,
	struct nvgpu_list_node *list, pid_t *pid, u32 *vmid);

size_t nvgpu_gr_fecs_trace_buffer_size(struct gk20a *g);
int nvgpu_gr_fecs_trace_max_entries(struct gk20a *g,
		struct nvgpu_gpu_ctxsw_trace_filter *filter);

int nvgpu_gr_fecs_trace_enable(struct gk20a *g);
int nvgpu_gr_fecs_trace_disable(struct gk20a *g);
bool nvgpu_gr_fecs_trace_is_enabled(struct gk20a *g);
void nvgpu_gr_fecs_trace_reset_buffer(struct gk20a *g);

int nvgpu_gr_fecs_trace_ring_read(struct gk20a *g, int index,
	u32 *vm_update_mask);
int nvgpu_gr_fecs_trace_poll(struct gk20a *g);
int nvgpu_gr_fecs_trace_reset(struct gk20a *g);

int nvgpu_gr_fecs_trace_bind_channel(struct gk20a *g,
	struct nvgpu_mem *inst_block, struct nvgpu_gr_subctx *subctx,
	struct nvgpu_gr_ctx *gr_ctx, pid_t pid, u32 vmid);
int nvgpu_gr_fecs_trace_unbind_channel(struct gk20a *g,
	struct nvgpu_mem *inst_block);

/*
 * Below functions are defined in OS-specific code.
 * Declare them here in common header since they are called from common code
 */
int nvgpu_gr_fecs_trace_ring_alloc(struct gk20a *g, void **buf, size_t *size);
int nvgpu_gr_fecs_trace_ring_free(struct gk20a *g);
void nvgpu_gr_fecs_trace_get_mmap_buffer_info(struct gk20a *g,
				void **mmapaddr, size_t *mmapsize);
void nvgpu_gr_fecs_trace_add_tsg_reset(struct gk20a *g, struct nvgpu_tsg *tsg);
u8 nvgpu_gpu_ctxsw_tags_to_common_tags(u8 tags);
int nvgpu_gr_fecs_trace_write_entry(struct gk20a *g,
			    struct nvgpu_gpu_ctxsw_trace_entry *entry);
void nvgpu_gr_fecs_trace_wake_up(struct gk20a *g, int vmid);

#endif /* CONFIG_NVGPU_FECS_TRACE */
#endif /* NVGPU_GR_FECS_TRACE_H */
