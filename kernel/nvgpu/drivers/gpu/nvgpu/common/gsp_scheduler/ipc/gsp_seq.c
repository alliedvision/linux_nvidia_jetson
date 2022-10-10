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

#include <nvgpu/string.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/errno.h>
#include <nvgpu/gsp.h>

#include "../gsp_scheduler.h"
#include "gsp_seq.h"

static void gsp_sequences_init(struct gk20a *g,
			struct gsp_sequences *sequences)
{
	u16 i = 0;

	nvgpu_log_fn(g, " ");

	(void) memset(sequences->seq, 0,
		sizeof(*sequences->seq) * GSP_MAX_NUM_SEQUENCES);

	(void) memset(sequences->gsp_seq_tbl, 0,
		sizeof(sequences->gsp_seq_tbl));

	for (i = 0; i < GSP_MAX_NUM_SEQUENCES; i++) {
		sequences->seq[i].id = (u8)i;
	}
}

int nvgpu_gsp_sequences_init(struct gk20a *g, struct nvgpu_gsp_sched *gsp_sched)
{
	int err = 0;
	struct gsp_sequences *seqs;

	nvgpu_log_fn(g, " ");

	seqs = (struct gsp_sequences *) nvgpu_kzalloc(g, sizeof(*seqs->seq));
	if (seqs == NULL) {
		nvgpu_err(g, "GSP sequences allocation failed");
		return -ENOMEM;
	}

	seqs->seq = nvgpu_kzalloc(g,
			GSP_MAX_NUM_SEQUENCES * sizeof(*seqs->seq));
	if (seqs->seq == NULL) {
		nvgpu_err(g, "GSP sequence allocation failed");
		nvgpu_kfree(g, seqs);
		return -ENOMEM;
	}

	gsp_sched->sequences = seqs;
	gsp_sched->sequences->seq = seqs->seq;

	nvgpu_mutex_init(&seqs->gsp_seq_lock);

	gsp_sequences_init(g, seqs);

	return err;
}

void nvgpu_gsp_sequences_free(struct gk20a *g,
			struct gsp_sequences *sequences)
{
	nvgpu_mutex_destroy(&sequences->gsp_seq_lock);
	nvgpu_kfree(g, sequences->seq);
	nvgpu_kfree(g, sequences);
}

int nvgpu_gsp_seq_acquire(struct gk20a *g,
			struct gsp_sequences *sequences,
			struct gsp_sequence **pseq,
			gsp_callback callback, void *cb_params)
{
	struct gsp_sequence *seq;
	u32 index = 0;
	int err = 0;

	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&sequences->gsp_seq_lock);

	index = (u32)find_first_zero_bit(sequences->gsp_seq_tbl,
			GSP_MAX_NUM_SEQUENCES);

	if (index >= GSP_MAX_NUM_SEQUENCES) {
		nvgpu_err(g, "no free sequence available");
		nvgpu_mutex_release(&sequences->gsp_seq_lock);
		err = -EAGAIN;
		goto exit;
	}

	nvgpu_assert(index < U64(INT_MAX));
	nvgpu_set_bit(index, sequences->gsp_seq_tbl);

	nvgpu_mutex_release(&sequences->gsp_seq_lock);

	seq = &sequences->seq[index];

	seq->state = GSP_SEQ_STATE_PENDING;
	seq->callback = callback;
	seq->cb_params = cb_params;
	seq->out_payload = NULL;

	*pseq = seq;

exit:
	return err;
}

void gsp_seq_release(struct gsp_sequences *sequences,
			struct gsp_sequence *seq)
{
	seq->state	= GSP_SEQ_STATE_FREE;
	seq->callback	= NULL;
	seq->cb_params	= NULL;
	seq->out_payload = NULL;

	nvgpu_mutex_acquire(&sequences->gsp_seq_lock);
	nvgpu_clear_bit(seq->id, sequences->gsp_seq_tbl);
	nvgpu_mutex_release(&sequences->gsp_seq_lock);
}

int nvgpu_gsp_seq_response_handle(struct gk20a *g,
				struct gsp_sequences *sequences,
				struct nv_flcn_msg_gsp *msg, u32 seq_id)
{
	struct gsp_sequence *seq;

	nvgpu_log_fn(g, " ");

	/* get the sequence info data associated with this message */
	seq = &sequences->seq[seq_id];


	if (seq->state != GSP_SEQ_STATE_USED) {
		nvgpu_err(g, "msg for an unknown sequence %d", seq->id);
		return -EINVAL;
	}

	if (seq->callback != NULL) {
		seq->callback(g, msg, seq->cb_params, 0);
	}

	/* release the sequence so that it may be used for other commands */
	gsp_seq_release(sequences, seq);

	return 0;
}

u8 nvgpu_gsp_seq_get_id(struct gsp_sequence *seq)
{
	return seq->id;
}

void nvgpu_gsp_seq_set_state(struct gsp_sequence *seq, enum gsp_seq_state state)
{
	seq->state = state;
}
