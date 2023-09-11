/*
 * Copyright (c) 2023, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>

#include <nvgpu/hw/ga10b/hw_gmmu_ga10b.h>

#include "mm_ga10b.h"

u32 ga10b_mm_bar2_vm_size(struct gk20a *g)
{
	u32 num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);
	u32 buffer_size;
	u32 fb_size;

	/* Calculate engine method buffer size */
	buffer_size =  nvgpu_safe_add_u32(nvgpu_safe_mult_u32((9U + 1U + 3U),
				g->ops.ce.get_num_pce(g)), 2U);
	buffer_size = nvgpu_safe_mult_u32((27U * 5U), buffer_size);
	buffer_size = PAGE_ALIGN(buffer_size);
	nvgpu_log_info(g, "method buffer size in bytes %u", buffer_size);

	buffer_size = nvgpu_safe_mult_u32(num_pbdma, buffer_size);

	buffer_size = nvgpu_safe_mult_u32(buffer_size, g->ops.channel.count(g));

	nvgpu_log_info(g, "method buffer size in bytes for max TSGs %u",
		       buffer_size);

	/*
	 * Calculate fault buffers size.
	 * Max entries take care of 1 entry used for full detection.
	 */
	fb_size = nvgpu_safe_add_u32(g->ops.channel.count(g), 1U);
	fb_size = nvgpu_safe_mult_u32(fb_size, gmmu_fault_buf_size_v());

	/* Consider replayable and non replayable faults */
	fb_size = nvgpu_safe_mult_u32(fb_size, 2U);

	nvgpu_log_info(g, "fault buffers size in bytes %u", fb_size);

	buffer_size = nvgpu_safe_add_u32(buffer_size, fb_size);

	/* Add PAGE_SIZE for vab buffer size */
	buffer_size = nvgpu_safe_add_u32(buffer_size, PAGE_SIZE);

	buffer_size = PAGE_ALIGN(buffer_size);

	nvgpu_log_info(g, "bar2 vm size in bytes %u", buffer_size);

	return buffer_size;
}
