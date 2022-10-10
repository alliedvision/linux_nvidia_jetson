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

#ifndef NVGPU_GSP_CMD_IF_H
#define NVGPU_GSP_CMD_IF_H

#include <nvgpu/types.h>
#include "../gsp_runlist.h"
#include "gsp_seq.h"

struct gk20a;

#define GSP_NV_CMDQ_LOG_ID		0U
#define GSP_NV_CMDQ_LOG_ID__LAST	0U
#define GSP_NV_MSGQ_LOG_ID		1U

#define  NV_GSP_UNIT_REWIND		NV_FLCN_UNIT_ID_REWIND
#define  NV_GSP_UNIT_NULL		0x01U
#define  NV_GSP_UNIT_INIT		0x02U
#define  NV_GSP_UNIT_DEVICES_INFO	0x03U
#define  NV_GSP_UNIT_SUBMIT_RUNLIST	0x04U
#define  NV_GSP_UNIT_END		0x0AU

#define GSP_MSG_HDR_SIZE	U32(sizeof(struct gsp_hdr))
#define GSP_CMD_HDR_SIZE	U32(sizeof(struct gsp_hdr))

struct gsp_hdr {
	u8 unit_id;
	u8 size;
	u8 ctrl_flags;
	u8 seq_id;
};

struct nv_flcn_cmd_gsp {
	struct gsp_hdr hdr;
	union {
		struct nvgpu_gsp_device_info device;
		struct nvgpu_gsp_runlist_info runlist;
	} cmd;
};

u8 gsp_unit_id_is_valid(u8 id);
/* command handling methods*/
int nvgpu_gsp_cmd_post(struct gk20a *g, struct nv_flcn_cmd_gsp *cmd,
	u32 queue_id, gsp_callback callback, void *cb_param, u32 timeout);

#endif /* NVGPU_GSP_CMD_IF_H */
