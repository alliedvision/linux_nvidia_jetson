/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GSP
#define NVGPU_GSP
#include <nvgpu/lock.h>
#include <nvgpu/nvgpu_mem.h>
struct gk20a;
struct nvgpu_gsp;
struct nvgpu_runlist;

struct gsp_fw {
	/* gsp ucode name */
	char *code_name;
	char *data_name;
	char *manifest_name;

	/* gsp ucode */
	struct nvgpu_firmware *code;
	struct nvgpu_firmware *data;
	struct nvgpu_firmware *manifest;
};

/* GSP descriptor's */
struct nvgpu_gsp {
	struct gk20a *g;

	struct gsp_fw gsp_ucode;
	struct nvgpu_falcon *gsp_flcn;

	bool isr_enabled;
	struct nvgpu_mutex isr_mutex;
};

int nvgpu_gsp_debug_buf_init(struct gk20a *g, u32 queue_no, u32 buffer_size);
void nvgpu_gsp_suspend(struct gk20a *g, struct nvgpu_gsp *gsp);
void nvgpu_gsp_sw_deinit(struct gk20a *g, struct nvgpu_gsp *gsp);
void nvgpu_gsp_isr_mutex_acquire(struct gk20a *g, struct nvgpu_gsp *gsp);
void nvgpu_gsp_isr_mutex_release(struct gk20a *g, struct nvgpu_gsp *gsp);
bool nvgpu_gsp_is_isr_enable(struct gk20a *g, struct nvgpu_gsp *gsp);
u32 nvgpu_gsp_get_last_cmd_id(struct gk20a *g);
struct nvgpu_falcon *nvgpu_gsp_falcon_instance(struct gk20a *g);
int nvgpu_gsp_process_message(struct gk20a *g);
int nvgpu_gsp_wait_for_mailbox_update(struct nvgpu_gsp *gsp,
		u32 mailbox_index, u32 exp_value, signed int timeoutms);
int nvgpu_gsp_bootstrap_ns(struct gk20a *g, struct nvgpu_gsp *gsp);
void nvgpu_gsp_isr(struct gk20a *g);
void nvgpu_gsp_isr_support(struct gk20a *g, struct nvgpu_gsp *gsp, bool enable);
int nvgpu_gsp_wait_for_priv_lockdown_release(struct nvgpu_gsp *gsp,
		signed int timeoutms);
int nvgpu_gsp_runlist_submit(struct gk20a *g, struct nvgpu_runlist *runlist);
#endif /* NVGPU_GSP */
