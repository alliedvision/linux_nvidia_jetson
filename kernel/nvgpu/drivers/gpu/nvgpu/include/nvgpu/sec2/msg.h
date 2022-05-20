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

#ifndef NVGPU_SEC2_MSG_H
#define NVGPU_SEC2_MSG_H

#include <nvgpu/sec2/lsfm.h>
#include <nvgpu/sec2/sec2_cmn.h>
#include <nvgpu/flcnif_cmn.h>
#include <nvgpu/types.h>

struct nvgpu_sec2;

/*
 * SEC2 Message Interfaces - SEC2 Management
 */

/*
 * Defines the identifiers various high-level types of sequencer commands and
 * messages.
 * _SEC2_INIT - sec2_init_msg_sec2_init
 */
enum {
	NV_SEC2_INIT_MSG_ID_SEC2_INIT = 0U,
};

struct sec2_init_msg_sec2_init {
	u8  msg_type;
	u8  num_queues;

	u16 os_debug_entry_point;

	struct {
		u32 queue_offset;
		u16 queue_size;
		u8  queue_phy_id;
		u8  queue_log_id;
	} q_info[SEC2_QUEUE_NUM];

	u32 nv_managed_area_offset;
	u16 nv_managed_area_size;
	/* Unused, kept for the binary compatibility */
	u8 rsvd_1[16];
	u8 rsvd_2[16];
};

union nv_flcn_msg_sec2_init {
	u8 msg_type;
	struct sec2_init_msg_sec2_init sec2_init;
};


struct nv_flcn_msg_sec2 {
	struct pmu_hdr hdr;

	union {
		union nv_flcn_msg_sec2_init init;
		union nv_sec2_acr_msg acr;
	} msg;
};

int nvgpu_sec2_process_message(struct nvgpu_sec2 *sec2);
int nvgpu_sec2_wait_message_cond(struct nvgpu_sec2 *sec2, u32 timeout_ms,
	void *var, u8 val);

#endif /* NVGPU_SEC2_MSG_H */
