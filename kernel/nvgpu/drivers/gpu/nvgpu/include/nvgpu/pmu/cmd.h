/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PMU_CMD_H
#define NVGPU_PMU_CMD_H

#include <nvgpu/flcnif_cmn.h>
#include <nvgpu/pmu/pmuif/perfmon.h>
#include <nvgpu/pmu/pmuif/pg.h>
#include <nvgpu/pmu/pmuif/acr.h>
#include <nvgpu/pmu/pmuif/pmgr.h>
#include <nvgpu/pmu/pmuif/rpc.h>

struct gk20a;
struct pmu_payload;
struct nvgpu_pmu;
struct pmu_msg;
struct pmu_sequence;
struct falcon_payload_alloc;

typedef void (*pmu_callback)(struct gk20a *g, struct pmu_msg *msg, void *param,
		u32 status);

struct pmu_cmd {
	struct pmu_hdr hdr;
	union {
		struct pmu_perfmon_cmd perfmon;
		struct pmu_pg_cmd pg;
		struct pmu_zbc_cmd zbc;
		struct pmu_acr_cmd acr;
		struct nv_pmu_boardobj_cmd obj;
		struct nv_pmu_pmgr_cmd pmgr;
		struct nv_pmu_rpc_cmd rpc;
	} cmd;
};

/* send a cmd to pmu */
int nvgpu_pmu_cmd_post(struct gk20a *g, struct pmu_cmd *cmd,
		struct pmu_payload *payload,
		u32 queue_id, pmu_callback callback, void *cb_param);

/* PMU RPC */
int nvgpu_pmu_rpc_execute(struct nvgpu_pmu *pmu, struct nv_pmu_rpc_header *rpc,
	u16 size_rpc, u16 size_scratch, pmu_callback caller_cb,
	void *caller_cb_param, bool is_copy_back);


/* RPC */
#define PMU_RPC_EXECUTE(_stat, _pmu, _unit, _func, _prpc, _size)\
	do {                                                 \
		(void) memset(&((_prpc)->hdr), 0, sizeof((_prpc)->hdr));\
		\
		(_prpc)->hdr.unit_id   = PMU_UNIT_##_unit;       \
		(_prpc)->hdr.function = NV_PMU_RPC_ID_##_unit##_##_func;\
		(_prpc)->hdr.flags    = 0x0;    \
		\
		_stat = nvgpu_pmu_rpc_execute(_pmu, &((_prpc)->hdr),    \
			(u16)(sizeof(*(_prpc)) - sizeof((_prpc)->scratch)), \
			(_size), NULL, NULL, false);	\
	} while (false)

/* RPC blocking call to copy back data from PMU to  _prpc */
#define PMU_RPC_EXECUTE_CPB(_stat, _pmu, _unit, _func, _prpc, _size)\
	do {                                                 \
		(void) memset(&((_prpc)->hdr), 0, sizeof((_prpc)->hdr));\
		\
		(_prpc)->hdr.unit_id   = PMU_UNIT_##_unit;       \
		(_prpc)->hdr.function = NV_PMU_RPC_ID_##_unit##_##_func;\
		(_prpc)->hdr.flags    = 0x0;    \
		\
		_stat = nvgpu_pmu_rpc_execute(_pmu, &((_prpc)->hdr),    \
			(u16)(sizeof(*(_prpc)) - sizeof((_prpc)->scratch)),\
			(_size), NULL, NULL, true);	\
	} while (false)

/* RPC non-blocking with call_back handler option */
#define PMU_RPC_EXECUTE_CB(_stat, _pmu, _unit, _func, _prpc, _size, _cb, _cbp)\
	do {                                                 \
		(void) memset(&((_prpc)->hdr), 0, sizeof((_prpc)->hdr));\
		\
		(_prpc)->hdr.unit_id   = PMU_UNIT_##_unit;       \
		(_prpc)->hdr.function = NV_PMU_RPC_ID_##_unit##_##_func;\
		(_prpc)->hdr.flags    = 0x0;    \
		\
		_stat = nvgpu_pmu_rpc_execute(_pmu, &((_prpc)->hdr),    \
			(sizeof(*(_prpc)) - sizeof((_prpc)->scratch)),\
			(_size), _cb, _cbp, false);	\
	} while (false)

#endif /* NVGPU_PMU_CMD_H*/
