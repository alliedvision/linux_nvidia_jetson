/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_COMPTAGS_H
#define NVGPU_COMPTAGS_H

#ifdef CONFIG_NVGPU_COMPRESSION

#include <nvgpu/lock.h>
#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_os_buffer;

struct gk20a_comptags {
	u32 offset;
	u32 lines;

	/*
	 * This signals whether allocation has been attempted. Observe 'lines'
	 * to see whether the comptags were actually allocated. We try alloc
	 * only once per buffer in order not to break multiple compressible-kind
	 * mappings.
	 */
	bool allocated;

	/*
	 * "enabled" indicates if the comptags are in use for mapping the buffer
	 * as compressible. Buffer comptags usage may be changed at runtime by
	 * buffer metadata re-registration. However, comptags once allocated
	 * are freed only on freeing the buffer.
	 *
	 * "enabled" implies that comptags have been successfully allocated
	 * (offset > 0 and lines > 0)
	 */
	bool enabled;

	/*
	 * Do comptags need to be cleared before mapping?
	 */
	bool needs_clear;
};

struct gk20a_comptag_allocator {
	struct gk20a *g;

	struct nvgpu_mutex lock;

	/* This bitmap starts at ctag 1. 0th cannot be taken. */
	unsigned long *bitmap;

	/* Size of bitmap, not max ctags, so one less. */
	unsigned long size;
};

/* real size here, but first (ctag 0) isn't used */
int gk20a_comptag_allocator_init(struct gk20a *g,
				 struct gk20a_comptag_allocator *allocator,
				 unsigned long size);
void gk20a_comptag_allocator_destroy(struct gk20a *g,
				     struct gk20a_comptag_allocator *allocator);

int gk20a_comptaglines_alloc(struct gk20a_comptag_allocator *allocator,
			     u32 *offset, u32 len);
void gk20a_comptaglines_free(struct gk20a_comptag_allocator *allocator,
			     u32 offset, u32 len);

/*
 * Defined by OS specific code since comptags are stored in a highly OS specific
 * way.
 */
int gk20a_alloc_comptags(struct gk20a *g, struct nvgpu_os_buffer *buf,
			 struct gk20a_comptag_allocator *allocator);
void gk20a_get_comptags(struct nvgpu_os_buffer *buf,
			struct gk20a_comptags *comptags);

/* legacy support */
void gk20a_alloc_or_get_comptags(struct gk20a *g,
				 struct nvgpu_os_buffer *buf,
				 struct gk20a_comptag_allocator *allocator,
				 struct gk20a_comptags *comptags);
/*
 * These functions must be used to synchronize comptags clear. The usage:
 *
 *   if (gk20a_comptags_start_clear(os_buf)) {
 *           COMMENT: we now hold the buffer lock for clearing
 *           bool successful = hw_clear_comptags();
 *
 *           COMMENT: mark the buf cleared (or not) and release the buffer lock
 *           gk20a_comptags_finish_clear(os_buf, successful);
 *   }
 *
 *  If gk20a_start_comptags_clear() returns false, another caller has
 *  already cleared the comptags.
 */
bool gk20a_comptags_start_clear(struct nvgpu_os_buffer *buf);
void gk20a_comptags_finish_clear(struct nvgpu_os_buffer *buf,
				 bool clear_successful);
#endif
#endif /* NVGPU_COMPTAGS_H */
