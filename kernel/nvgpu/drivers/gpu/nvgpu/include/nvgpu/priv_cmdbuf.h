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

#ifndef NVGPU_PRIV_CMDBUF_H
#define NVGPU_PRIV_CMDBUF_H

#include <nvgpu/types.h>

struct gk20a;
struct vm_gk20a;
struct priv_cmd_entry;
struct priv_cmd_queue;

int nvgpu_priv_cmdbuf_queue_alloc(struct vm_gk20a *vm,
		u32 job_count, struct priv_cmd_queue **queue);
void nvgpu_priv_cmdbuf_queue_free(struct priv_cmd_queue *q);

int nvgpu_priv_cmdbuf_alloc(struct priv_cmd_queue *q, u32 size,
		struct priv_cmd_entry **e);
void nvgpu_priv_cmdbuf_rollback(struct priv_cmd_queue *q,
		struct priv_cmd_entry *e);
void nvgpu_priv_cmdbuf_free(struct priv_cmd_queue *q,
		struct priv_cmd_entry *e);

void nvgpu_priv_cmdbuf_append(struct gk20a *g, struct priv_cmd_entry *e,
		u32 *data, u32 entries);
void nvgpu_priv_cmdbuf_append_zeros(struct gk20a *g, struct priv_cmd_entry *e,
		u32 entries);

void nvgpu_priv_cmdbuf_finish(struct gk20a *g, struct priv_cmd_entry *e,
		u64 *gva, u32 *size);

#endif
