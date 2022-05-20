/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_SEC2_H
#define NVGPU_SEC2_H

#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/allocator.h>
#include <nvgpu/lock.h>
#include <nvgpu/falcon.h>
#include <nvgpu/sec2/seq.h>
#include <nvgpu/sec2/sec2_cmn.h>

struct gk20a;
struct nv_flcn_msg_sec2;
struct nvgpu_engine_mem_queue;

#define nvgpu_sec2_dbg(g, fmt, args...) \
	nvgpu_log(g, gpu_dbg_pmu, fmt, ##args)

#define NVGPU_SEC2_TRACE_BUFSIZE	(32U * 1024U)

struct sec2_fw {
	struct nvgpu_firmware *fw_desc;
	struct nvgpu_firmware *fw_image;
	struct nvgpu_firmware *fw_sig;
};

struct nvgpu_sec2 {
	struct gk20a *g;
	struct nvgpu_falcon flcn;
	u32 falcon_id;

	struct nvgpu_engine_mem_queue *queues[SEC2_QUEUE_NUM];

	struct sec2_sequences sequences;

	bool isr_enabled;
	struct nvgpu_mutex isr_mutex;

	struct nvgpu_allocator dmem;

	/* set to true once init received */
	bool sec2_ready;

	struct nvgpu_mem trace_buf;

	void (*remove_support)(struct nvgpu_sec2 *sec2);

	u32 command_ack;

	struct sec2_fw fw;
};

/* sec2 init */
int nvgpu_init_sec2_setup_sw(struct gk20a *g);
int nvgpu_init_sec2_support(struct gk20a *g);
int nvgpu_sec2_destroy(struct gk20a *g);

#endif /* NVGPU_SEC2_H */
