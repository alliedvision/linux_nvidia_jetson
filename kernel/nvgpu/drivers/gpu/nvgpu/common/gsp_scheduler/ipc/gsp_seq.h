/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GSP_SEQ_H
#define NVGPU_GSP_SEQ_H

#include <nvgpu/types.h>
#include <nvgpu/lock.h>

struct gk20a;
struct nv_flcn_msg_gsp;

#define GSP_MAX_NUM_SEQUENCES	256U
#define GSP_SEQ_BIT_SHIFT	5U
#define GSP_SEQ_TBL_SIZE	(GSP_MAX_NUM_SEQUENCES >> GSP_SEQ_BIT_SHIFT)

enum gsp_seq_state {
	GSP_SEQ_STATE_FREE = 0U,
	GSP_SEQ_STATE_PENDING,
	GSP_SEQ_STATE_USED
};

typedef void (*gsp_callback)(struct gk20a *g, struct nv_flcn_msg_gsp *msg,
	void *param, u32 status);

struct gsp_sequence {
	u8 id;
	enum gsp_seq_state state;
	u8 *out_payload;
	gsp_callback callback;
	void *cb_params;
};

struct gsp_sequences {
	struct gsp_sequence *seq;
	unsigned long gsp_seq_tbl[GSP_SEQ_TBL_SIZE];
	struct nvgpu_mutex gsp_seq_lock;
};

int nvgpu_gsp_sequences_init(struct gk20a *g, struct nvgpu_gsp_sched *gsp_sched);
void nvgpu_gsp_sequences_free(struct gk20a *g,
			struct gsp_sequences *sequences);
int nvgpu_gsp_seq_acquire(struct gk20a *g,
			struct gsp_sequences *sequences,
			struct gsp_sequence **pseq,
			gsp_callback callback, void *cb_params);
int nvgpu_gsp_seq_response_handle(struct gk20a *g,
			struct gsp_sequences *sequences,
			struct nv_flcn_msg_gsp *msg, u32 seq_id);
u8 nvgpu_gsp_seq_get_id(struct gsp_sequence *seq);
void nvgpu_gsp_seq_set_state(struct gsp_sequence *seq,
			enum gsp_seq_state state);
void gsp_seq_release(struct gsp_sequences *sequences,
			struct gsp_sequence *seq);
#endif /* NVGPU_GSP_SEQ_H */
