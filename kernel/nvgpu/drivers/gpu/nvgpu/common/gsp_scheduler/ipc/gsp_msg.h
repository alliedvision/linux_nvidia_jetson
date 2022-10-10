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

#ifndef NVGPU_GSP_MSG_H
#define NVGPU_GSP_MSG_H

#include <nvgpu/types.h>
#include <nvgpu/gsp.h>

#include "gsp_cmd.h"
#include "../gsp_scheduler.h"

struct nvgpu_gsp;

#define GSP_CMD_FLAGS_MASK		U8(0xF0U)
#define GSP_CMD_FLAGS_STATUS		BIT8(0U)
#define GSP_CMD_FLAGS_INTR		BIT8(1U)
#define GSP_CMD_FLAGS_EVENT		BIT8(2U)
#define GSP_CMD_FLAGS_RPC_EVENT		BIT8(3U)

#define GSP_DMEM_ALLOC_ALIGNMENT	32U
#define GSP_DMEM_ALIGNMENT		4U

enum {
	NV_GSP_INIT_MSG_ID_GSP_INIT = 0U,
};

struct gsp_init_msg_gsp_init {
	u8 msg_type;
	u8 num_queues;

	struct {
		u32 queue_offset;
		u16 queue_size;
		u8  queue_phy_id;
		u8  queue_log_id;
	} q_info[GSP_QUEUE_NUM];
};

union nv_flcn_msg_gsp_init {
	u8 msg_type;
	struct gsp_init_msg_gsp_init gsp_init;
};


struct nv_flcn_msg_gsp {
	struct gsp_hdr hdr;
	union {
		union nv_flcn_msg_gsp_init init;
	} msg;
};

int nvgpu_gsp_wait_message_cond(struct gk20a *g, u32 timeout_ms,
	void *var, u8 val);

#endif /* NVGPU_GSP_MSG_H */
