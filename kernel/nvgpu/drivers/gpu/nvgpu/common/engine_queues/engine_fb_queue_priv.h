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

#ifndef NVGPU_ENGINE_FB_QUEUE_PRIV_H
#define NVGPU_ENGINE_FB_QUEUE_PRIV_H

#include <nvgpu/lock.h>

struct nvgpu_engine_fb_queue {
	struct gk20a *g;
	u32 flcn_id;

	/* used by nvgpu, for command LPQ/HPQ */
	struct nvgpu_mutex mutex;

	/* current write position */
	u32 position;
	/* logical queue identifier */
	u32 id;
	/* physical queue index */
	u32 index;
	/* in bytes */
	u32 size;
	/* open-flag */
	u32 oflag;

	/* members unique to the FB version of the falcon queues */
	struct {
		/* Holds super surface base address */
		struct nvgpu_mem *super_surface_mem;

		/*
		 * Holds the offset of queue data (0th element).
		 * This is used for FB Queues to hold a offset of
		 * Super Surface for this queue.
		 */
		 u32 fb_offset;

		/*
		 * Define the size of a single queue element.
		 * queues_size above is used for the number of
		 * queue elements.
		 */
		u32 element_size;

		/* To keep track of elements in use */
		u64 element_in_use;

		/*
		 * Define a pointer to a local (SYSMEM) allocated
		 * buffer to hold a single queue element
		 * it is being assembled.
		 */
		 u8 *work_buffer;
		 struct nvgpu_mutex work_buffer_mutex;

		/*
		 * Tracks how much of the current FB Queue MSG queue
		 * entry have been read. This is needed as functions read
		 * the MSG queue as a byte stream, rather
		 * than reading a whole MSG at a time.
		 */
		u32 read_position;

		/*
		 * Tail as tracked on the nvgpu "side".  Because the queue
		 * elements and its associated payload (which is also moved
		 * PMU->nvgpu through the FB CMD Queue) can't be free-ed until
		 * the command is complete, response is received and any "out"
		 * payload delivered to the client, it is necessary for the
		 * nvgpu to track it's own version of "tail".  This one is
		 * incremented as commands and completed entries are found
		 * following tail.
		 */
		u32 tail;
	} fbq;

	/* engine and queue specific ops */
	int (*tail)(struct nvgpu_engine_fb_queue *queue, u32 *tail, bool set);
	int (*head)(struct nvgpu_engine_fb_queue *queue, u32 *head, bool set);

	/* engine specific ops */
	int (*queue_head)(struct gk20a *g, u32 queue_id, u32 queue_index,
		u32 *head, bool set);
	int (*queue_tail)(struct gk20a *g, u32 queue_id, u32 queue_index,
		u32 *tail, bool set);
};

#endif /* NVGPU_ENGINE_FB_QUEUE_PRIV_H */
