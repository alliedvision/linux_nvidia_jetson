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

#ifndef NVGPU_ENGINE_FB_QUEUE_H
#define NVGPU_ENGINE_FB_QUEUE_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_engine_fb_queue;

struct nvgpu_engine_fb_queue_params {
	struct gk20a *g;
	u32 flcn_id;

	/* logical queue identifier */
	u32 id;
	/* physical queue index */
	u32 index;
	/* in bytes */
	u32 size;
	/* open-flag */
	u32 oflag;

	/* fb queue params*/
	/* Holds the offset of queue data (0th element) */
	u32 fbq_offset;

	/* fb queue element size*/
	u32 fbq_element_size;

	/* Holds super surface base address */
	struct nvgpu_mem *super_surface_mem;

	/* engine specific ops */
	int (*queue_head)(struct gk20a *g, u32 queue_id, u32 queue_index,
		u32 *head, bool set);
	int (*queue_tail)(struct gk20a *g, u32 queue_id, u32 queue_index,
		u32 *tail, bool set);
};

/* queue public functions */
int nvgpu_engine_fb_queue_init(struct nvgpu_engine_fb_queue **queue_p,
	struct nvgpu_engine_fb_queue_params params);
bool nvgpu_engine_fb_queue_is_empty(struct nvgpu_engine_fb_queue *queue);
int nvgpu_engine_fb_queue_pop(struct nvgpu_engine_fb_queue *queue,
	void *data, u32 size, u32 *bytes_read);
int nvgpu_engine_fb_queue_push(struct nvgpu_engine_fb_queue *queue,
	void *data, u32 size);
void nvgpu_engine_fb_queue_free(struct nvgpu_engine_fb_queue **queue_p);
u32 nvgpu_engine_fb_queue_get_position(struct nvgpu_engine_fb_queue *queue);
u32 nvgpu_engine_fb_queue_get_element_size(struct nvgpu_engine_fb_queue *queue);
u32 nvgpu_engine_fb_queue_get_offset(struct nvgpu_engine_fb_queue *queue);
u8 *nvgpu_engine_fb_queue_get_work_buffer(struct nvgpu_engine_fb_queue *queue);
int nvgpu_engine_fb_queue_free_element(struct nvgpu_engine_fb_queue *queue,
		u32 queue_pos);
void nvgpu_engine_fb_queue_lock_work_buffer(
	struct nvgpu_engine_fb_queue *queue);
void nvgpu_engine_fb_queue_unlock_work_buffer(
	struct nvgpu_engine_fb_queue *queue);

#endif /* NVGPU_ENGINE_FB_QUEUE_H */
