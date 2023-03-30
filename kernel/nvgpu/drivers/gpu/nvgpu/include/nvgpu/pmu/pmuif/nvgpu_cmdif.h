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
#ifndef NVGPU_PMUIF_NVGPU_CMDIF_H
#define NVGPU_PMUIF_NVGPU_CMDIF_H

#include "cmn.h"
#include "init.h"
#include "ap.h"
#include "seq.h"

#define PMU_UNIT_REWIND			U8(0x00)
#define PMU_UNIT_CMDMGMT		U8(0x01)
#define PMU_UNIT_PG			U8(0x03)
#define PMU_UNIT_PG_LOADING		U8(0x06)
#define PMU_UNIT_INIT			U8(0x07)
#define PMU_UNIT_ACR			U8(0x0A)
#define PMU_UNIT_PERFMON_T18X		U8(0x11)
#define PMU_UNIT_PERFMON		U8(0x12)
#define PMU_UNIT_PERF			U8(0x13)
#define PMU_UNIT_RC			U8(0x1F)
#define PMU_UNIT_FECS_MEM_OVERRIDE	U8(0x1E)
#define PMU_UNIT_CLK			U8(0x0D)
#define PMU_UNIT_THERM			U8(0x14)
#define PMU_UNIT_PMGR			U8(0x18)
#define PMU_UNIT_VOLT			U8(0x0E)
#define PMU_UNIT_INIT_DGPU		U8(0x0F)

#define PMU_UNIT_END			U8(0x23)
#define PMU_UNIT_INVALID		U8(0xFF)

#define PMU_UNIT_TEST_START		U8(0xFE)
#define PMU_UNIT_END_SIM		U8(0xFF)
#define PMU_UNIT_TEST_END		U8(0xFF)

#define PMU_UNIT_ID_IS_VALID(id)		\
		(((id) < PMU_UNIT_END) || ((id) >= PMU_UNIT_TEST_START))

/*
 * PMU Command structures for FB queue
 */

/* Size of a single element in the CMD queue. */
#define NV_PMU_FBQ_CMD_ELEMENT_SIZE		2048U

/* Number of elements in each queue. */
#define NV_PMU_FBQ_CMD_NUM_ELEMENTS		16U

/* Total number of CMD queues. */
#define NV_PMU_FBQ_CMD_COUNT		2U

/* Size of a single element in the MSG queue. */
#define NV_PMU_FBQ_MSG_ELEMENT_SIZE		64U

#define RM_PMU_FBQ_MSG_DATA_SIZE \
	(NV_PMU_FBQ_MSG_ELEMENT_SIZE - \
	sizeof(struct nv_falcon_fbq_msgq_hdr))

/* Number of elements in each queue. */
#define NV_PMU_FBQ_MSG_NUM_ELEMENTS		16U

/* Single MSG (response) queue. */
#define NV_PMU_FBQ_MSG_COUNT		1U

/* structure for a single PMU FB CMD queue entry */
struct nv_pmu_fbq_cmd_q_element {
	struct nv_falcon_fbq_hdr fbq_hdr;

	struct {
		struct pmu_hdr hdr;
		u8 bytes[NV_PMU_FBQ_CMD_ELEMENT_SIZE -
			sizeof(struct nv_falcon_fbq_hdr) -
			sizeof(struct pmu_hdr)];
	} data;
};

/* structure for a single PMU FB MSG queue entry */
struct nv_pmu_fbq_msg_q_element {
	struct nv_falcon_fbq_msgq_hdr fbq_msg_hdr;
	struct {
		struct pmu_hdr hdr;
		u8 bytes[RM_PMU_FBQ_MSG_DATA_SIZE -
				sizeof(struct pmu_hdr)];
	} data;
};

/* structure for a single FB CMD queue */
struct nv_pmu_fbq_cmd_queue {
	struct nv_pmu_fbq_cmd_q_element element[NV_PMU_FBQ_CMD_NUM_ELEMENTS];
};

/* structure for a set of FB CMD queue */
struct nv_pmu_fbq_cmd_queues {
	struct nv_pmu_fbq_cmd_queue queue[NV_PMU_FBQ_CMD_COUNT];
};

/* structure for a single FB MSG queue */
struct nv_pmu_fbq_msg_queue {
	struct nv_pmu_fbq_msg_q_element element[NV_PMU_FBQ_MSG_NUM_ELEMENTS];
};

#endif /* NVGPU_PMUIF_NVGPU_CMDIF_H */
