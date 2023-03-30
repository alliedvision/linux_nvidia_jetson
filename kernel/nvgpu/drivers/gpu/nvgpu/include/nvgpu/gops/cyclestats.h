/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_CYCLESTATS_H
#define NVGPU_GOPS_CYCLESTATS_H

#ifdef CONFIG_NVGPU_CYCLESTATS
struct gops_css {
	int (*enable_snapshot)(struct nvgpu_channel *ch,
			struct gk20a_cs_snapshot_client *client);
	void (*disable_snapshot)(struct gk20a *g);
	int (*check_data_available)(struct nvgpu_channel *ch,
					u32 *pending,
					bool *hw_overflow);
	void (*set_handled_snapshots)(struct gk20a *g, u32 num);
	u32 (*allocate_perfmon_ids)(struct gk20a_cs_snapshot *data,
					u32 count);
	u32 (*release_perfmon_ids)(struct gk20a_cs_snapshot *data,
					u32 start,
					u32 count);
	int (*detach_snapshot)(struct nvgpu_channel *ch,
			struct gk20a_cs_snapshot_client *client);
	bool (*get_overflow_status)(struct gk20a *g);
	u32 (*get_pending_snapshots)(struct gk20a *g);
	u32 (*get_max_buffer_size)(struct gk20a *g);
};
#endif

#endif /* NVGPU_GOPS_CYCLESTATS_H */
