/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PMU_SEQ_H
#define NVGPU_PMU_SEQ_H

#include <nvgpu/flcnif_cmn.h>
#include <nvgpu/lock.h>

struct nvgpu_engine_fb_queue;
struct nvgpu_mem;
struct pmu_msg;
struct gk20a;
struct nvgpu_pmu;;

#define PMU_MAX_NUM_SEQUENCES		(256U)
#define PMU_SEQ_BIT_SHIFT		(5U)
#define PMU_SEQ_TBL_SIZE	\
		(PMU_MAX_NUM_SEQUENCES >> PMU_SEQ_BIT_SHIFT)

typedef void (*pmu_callback)(struct gk20a *g, struct pmu_msg *msg, void *param,
		u32 status);

enum pmu_seq_state {
	PMU_SEQ_STATE_FREE = 0,
	PMU_SEQ_STATE_PENDING,
	PMU_SEQ_STATE_USED
};

struct pmu_sequence {
	u8 id;
	enum pmu_seq_state state;
	union {
		struct pmu_allocation_v1 in_v1;
		struct pmu_allocation_v2 in_v2;
		struct pmu_allocation_v3 in_v3;
	};
	struct nvgpu_mem *in_mem;
	union {
		struct pmu_allocation_v1 out_v1;
		struct pmu_allocation_v2 out_v2;
		struct pmu_allocation_v3 out_v3;
	};
	struct nvgpu_mem *out_mem;
	u8 *out_payload;
	pmu_callback callback;
	void *cb_params;

	/* fb queue that is associated with this seq */
	struct nvgpu_engine_fb_queue *cmd_queue;
	/* fbq element that is associated with this seq */
	u8 *fbq_work_buffer;
	u32 fbq_element_index;
	/* flags if queue element has an in payload */
	bool in_payload_fb_queue;
	/* flags if queue element has an out payload */
	bool out_payload_fb_queue;
	/* Heap location this cmd will use in the nvgpu managed heap */
	u16 fbq_heap_offset;
	/*
	 * Track the amount of the "work buffer" (queue_buffer) that
	 * has been used so far, as the outbound frame is assembled
	 * (first FB Queue hdr, then CMD, then payloads).
	 */
	u16 buffer_size_used;
	/* offset to out data in the queue element */
	u16 fbq_out_offset_in_queue_element;
};

struct pmu_sequences {
	struct pmu_sequence *seq;
	struct nvgpu_mutex pmu_seq_lock;
	unsigned long pmu_seq_tbl[PMU_SEQ_TBL_SIZE];
};

void nvgpu_pmu_sequences_sw_setup(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_sequences *sequences);
int nvgpu_pmu_sequences_init(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_sequences **sequences_p);
void nvgpu_pmu_sequences_deinit(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_sequences *sequences);

void nvgpu_pmu_seq_payload_free(struct gk20a *g, struct pmu_sequence *seq);
int nvgpu_pmu_seq_acquire(struct gk20a *g,
			  struct pmu_sequences *sequences,
			  struct pmu_sequence **pseq,
			  pmu_callback callback, void *cb_params);
void nvgpu_pmu_seq_release(struct gk20a *g,
			   struct pmu_sequences *sequences,
			   struct pmu_sequence *seq);
u16 nvgpu_pmu_seq_get_fbq_out_offset(struct pmu_sequence *seq);
void nvgpu_pmu_seq_set_fbq_out_offset(struct pmu_sequence *seq, u16 size);
u16 nvgpu_pmu_seq_get_buffer_size(struct pmu_sequence *seq);
void nvgpu_pmu_seq_set_buffer_size(struct pmu_sequence *seq, u16 size);
struct nvgpu_engine_fb_queue *nvgpu_pmu_seq_get_cmd_queue(
					struct pmu_sequence *seq);
void nvgpu_pmu_seq_set_cmd_queue(struct pmu_sequence *seq,
				 struct nvgpu_engine_fb_queue *fb_queue);
u16 nvgpu_pmu_seq_get_fbq_heap_offset(struct pmu_sequence *seq);
void nvgpu_pmu_seq_set_fbq_heap_offset(struct pmu_sequence *seq, u16 size);
u8 *nvgpu_pmu_seq_get_out_payload(struct pmu_sequence *seq);
void nvgpu_pmu_seq_set_out_payload(struct pmu_sequence *seq, u8 *payload);
void nvgpu_pmu_seq_set_in_payload_fb_queue(struct pmu_sequence *seq,
					   bool state);
bool nvgpu_pmu_seq_get_out_payload_fb_queue(struct pmu_sequence *seq);
void nvgpu_pmu_seq_set_out_payload_fb_queue(struct pmu_sequence *seq,
					    bool state);
u32 nvgpu_pmu_seq_get_fbq_element_index(struct pmu_sequence *seq);
void nvgpu_pmu_seq_set_fbq_element_index(struct pmu_sequence *seq, u32 index);
u8 nvgpu_pmu_seq_get_id(struct pmu_sequence *seq);
enum pmu_seq_state nvgpu_pmu_seq_get_state(struct pmu_sequence *seq);
void nvgpu_pmu_seq_set_state(struct pmu_sequence *seq,
			     enum pmu_seq_state state);
struct pmu_sequence *nvgpu_pmu_sequences_get_seq(struct pmu_sequences *seqs,
						 u8 id);
void nvgpu_pmu_seq_callback(struct gk20a *g, struct pmu_sequence *seq,
			    struct pmu_msg *msg, int err);

#endif /* NVGPU_PMU_SEQ_H */
