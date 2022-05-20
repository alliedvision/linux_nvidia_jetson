/*
 * general perf structures & definitions
 *
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

#ifndef PMU_PERF_H_
#define PMU_PERF_H_

#include "vfe_equ.h"
#include "vfe_var.h"
#include "change_seq.h"
#include "pstate.h"

/* PERF RPC ID Definitions */
#define NV_PMU_RPC_ID_PERF_VFE_CALLBACK                          0x01U
#define NV_PMU_RPC_ID_PERF_SEQ_COMPLETION                        0x02U
#define NV_PMU_RPC_ID_PERF_PSTATES_INVALIDATE                    0x03U

/*
 * Defines the structure that holds data
 * used to execute LOAD RPC.
 */
struct nv_pmu_rpc_struct_perf_load {
	/* [IN/OUT] Must be first field in RPC structure */
	struct nv_pmu_rpc_header hdr;
	bool b_load;
	u32 scratch[1];
};

/*
 * Simply a union of all specific PERF messages. Forms the general packet
 * exchanged between the Kernel and PMU when sending and receiving PERF messages
 * (respectively).
 */

struct pmu_nvgpu_rpc_perf_event {
	struct pmu_hdr msg_hdr;
	struct pmu_nvgpu_rpc_header rpc_hdr;
};

struct perf_vfe_invalidate {
	bool state_change;
	struct nvgpu_cond wq;
	struct nvgpu_thread state_task;
};

struct nvgpu_pmu_perf {
	struct vfe_vars vfe_varobjs;
	struct vfe_equs vfe_equobjs;
	struct pstates pstatesobjs;
	struct perf_vfe_invalidate vfe_init;
	struct change_seq_pmu changeseq_pmu;
};

#endif /* PMU_PERF_H_ */
