/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_SEC2_CMD_IF_H
#define NVGPU_SEC2_CMD_IF_H

#include <nvgpu/sec2/lsfm.h>
#include <nvgpu/flcnif_cmn.h>
#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_sec2;

struct nv_flcn_cmd_sec2 {
	struct pmu_hdr hdr;
	union {
		union nv_sec2_acr_cmd acr;
	} cmd;
};

/* command handling methods*/
int nvgpu_sec2_cmd_post(struct gk20a *g, struct nv_flcn_cmd_sec2 *cmd,
	u32 queue_id, sec2_callback callback,
	void *cb_param, u32 timeout);

#endif /* NVGPU_SEC2_CMD_IF_H */
