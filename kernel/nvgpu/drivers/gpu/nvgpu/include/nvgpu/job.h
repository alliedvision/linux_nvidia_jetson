/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_JOB_H
#define NVGPU_JOB_H

#include <nvgpu/list.h>
#include <nvgpu/fence.h>

struct priv_cmd_entry;
struct nvgpu_mapped_buf;
struct priv_cmd_entry;
struct nvgpu_channel;

struct nvgpu_channel_job {
	struct nvgpu_mapped_buf **mapped_buffers;
	u32 num_mapped_buffers;
	struct nvgpu_fence_type post_fence;
	struct priv_cmd_entry *wait_cmd;
	struct priv_cmd_entry *incr_cmd;
	struct nvgpu_list_node list;
};

int nvgpu_channel_alloc_job(struct nvgpu_channel *c,
		struct nvgpu_channel_job **job_out);
void nvgpu_channel_free_job(struct nvgpu_channel *c,
		struct nvgpu_channel_job *job);

void nvgpu_channel_joblist_lock(struct nvgpu_channel *c);
void nvgpu_channel_joblist_unlock(struct nvgpu_channel *c);
struct nvgpu_channel_job *nvgpu_channel_joblist_peek(struct nvgpu_channel *c);
void nvgpu_channel_joblist_add(struct nvgpu_channel *c,
		struct nvgpu_channel_job *job);
void nvgpu_channel_joblist_delete(struct nvgpu_channel *c,
		struct nvgpu_channel_job *job);

int nvgpu_channel_joblist_init(struct nvgpu_channel *c, u32 num_jobs);
void nvgpu_channel_joblist_deinit(struct nvgpu_channel *c);

#endif
