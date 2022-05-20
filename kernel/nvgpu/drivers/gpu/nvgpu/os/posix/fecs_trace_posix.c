/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/gr/fecs_trace.h>

int vgpu_alloc_user_buffer(struct gk20a *g, void **buf, size_t *size);
void vgpu_fecs_trace_data_update(struct gk20a *g);
void vgpu_get_mmap_user_buffer_info(struct gk20a *g,
				void **mmapaddr, size_t *mmapsize);

int nvgpu_gr_fecs_trace_ring_alloc(struct gk20a *g,
		void **buf, size_t *size)
{
	return -EINVAL;
}

int nvgpu_gr_fecs_trace_ring_free(struct gk20a *g)
{
	return -EINVAL;
}

void nvgpu_gr_fecs_trace_get_mmap_buffer_info(struct gk20a *g,
				void **mmapaddr, size_t *mmapsize)
{
}

int nvgpu_gr_fecs_trace_write_entry(struct gk20a *g,
		struct nvgpu_gpu_ctxsw_trace_entry *entry)
{
	return -EINVAL;
}

void nvgpu_gr_fecs_trace_wake_up(struct gk20a *g, int vmid)
{
}

void nvgpu_gr_fecs_trace_add_tsg_reset(struct gk20a *g, struct nvgpu_tsg *tsg)
{
}

u8 nvgpu_gpu_ctxsw_tags_to_common_tags(u8 tags)
{
	return 0;
}

int vgpu_alloc_user_buffer(struct gk20a *g, void **buf, size_t *size)
{
	return -EINVAL;
}

void vgpu_fecs_trace_data_update(struct gk20a *g)
{
}

void vgpu_get_mmap_user_buffer_info(struct gk20a *g,
				void **mmapaddr, size_t *mmapsize)
{
}
