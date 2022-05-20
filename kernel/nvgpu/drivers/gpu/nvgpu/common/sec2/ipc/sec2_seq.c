/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/sec2/seq.h>
#include <nvgpu/string.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/errno.h>

int nvgpu_sec2_sequences_alloc(struct gk20a *g,
			       struct sec2_sequences *sequences)
{
	sequences->seq = nvgpu_kzalloc(g, SEC2_MAX_NUM_SEQUENCES *
				       sizeof(struct sec2_sequence));
	if (sequences->seq == NULL) {
		return -ENOMEM;
	}

	nvgpu_mutex_init(&sequences->sec2_seq_lock);

	return 0;
}

void nvgpu_sec2_sequences_init(struct gk20a *g,
			       struct sec2_sequences *sequences)
{
	u32 i = 0;

	nvgpu_log_fn(g, " ");

	(void) memset(sequences->seq, 0,
		sizeof(struct sec2_sequence) * SEC2_MAX_NUM_SEQUENCES);

	(void) memset(sequences->sec2_seq_tbl, 0,
		      sizeof(sequences->sec2_seq_tbl));

	for (i = 0; i < SEC2_MAX_NUM_SEQUENCES; i++) {
		sequences->seq[i].id = (u8)i;
	}
}

void nvgpu_sec2_sequences_free(struct gk20a *g,
			       struct sec2_sequences *sequences)
{
	nvgpu_mutex_destroy(&sequences->sec2_seq_lock);
	nvgpu_kfree(g, sequences->seq);
}

int nvgpu_sec2_seq_acquire(struct gk20a *g,
			   struct sec2_sequences *sequences,
			   struct sec2_sequence **pseq,
			   sec2_callback callback, void *cb_params)
{
	struct sec2_sequence *seq;
	u32 index = 0;
	int err = 0;

	nvgpu_mutex_acquire(&sequences->sec2_seq_lock);

	index = find_first_zero_bit(sequences->sec2_seq_tbl,
		sizeof(sequences->sec2_seq_tbl));

	if (index >= sizeof(sequences->sec2_seq_tbl)) {
		nvgpu_err(g, "no free sequence available");
		nvgpu_mutex_release(&sequences->sec2_seq_lock);
		err = -EAGAIN;
		goto exit;
	}

	nvgpu_assert(index < U64(INT_MAX));
	nvgpu_set_bit(index, sequences->sec2_seq_tbl);

	nvgpu_mutex_release(&sequences->sec2_seq_lock);

	seq = &sequences->seq[index];

	seq->state = SEC2_SEQ_STATE_PENDING;
	seq->callback = callback;
	seq->cb_params = cb_params;
	seq->out_payload = NULL;

	*pseq = seq;

exit:
	return err;
}

static void sec2_seq_release(struct sec2_sequences *sequences,
			     struct sec2_sequence *seq)
{
	seq->state	= SEC2_SEQ_STATE_FREE;
	seq->callback	= NULL;
	seq->cb_params	= NULL;
	seq->out_payload = NULL;

	nvgpu_mutex_acquire(&sequences->sec2_seq_lock);
	nvgpu_clear_bit(seq->id, sequences->sec2_seq_tbl);
	nvgpu_mutex_release(&sequences->sec2_seq_lock);
}

int nvgpu_sec2_seq_response_handle(struct gk20a *g,
				   struct sec2_sequences *sequences,
				   struct nv_flcn_msg_sec2 *msg, u32 seq_id)
{
	struct sec2_sequence *seq;

	/* get the sequence info data associated with this message */
	seq = &sequences->seq[seq_id];


	if (seq->state != SEC2_SEQ_STATE_USED) {
		nvgpu_err(g, "msg for an unknown sequence %d", seq->id);
		return -EINVAL;
	}

	if (seq->callback != NULL) {
		seq->callback(g, msg, seq->cb_params, 0);
	}

	/* release the sequence so that it may be used for other commands */
	sec2_seq_release(sequences, seq);

	return 0;
}

u8 nvgpu_sec2_seq_get_id(struct sec2_sequence *seq)
{
	return seq->id;
}

void nvgpu_sec2_seq_set_state(struct sec2_sequence *seq,
			      enum sec2_seq_state state)
{
	seq->state = state;
}
