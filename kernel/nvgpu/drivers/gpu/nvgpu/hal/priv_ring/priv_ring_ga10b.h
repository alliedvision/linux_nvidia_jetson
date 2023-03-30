/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PRIV_RING_GA10B_H
#define NVGPU_PRIV_RING_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

/*
 * Helper macros for decoding host pri error of pattern:
 * BAD001xx - HOST_PRI_TIMEOUT.
 * BAD002xx - HOST_PRI_DECODE.
 * BAD0DAxx - HOST_PRI_SQUASH.
 * Where xx is interpreted as follows:
 * bits [7:0] = subid.
 */
#define HOST_PRIV_SUBID_MSK_VAL(x) \
	 ((x) & (nvgpu_safe_sub_u32(BIT32(8U), 1U)))

/*
 * Helper macros for decoding fecs pri floorsweep error of pattern:
 * BADF13xx - FECS_PRI_FLOORSWEEP.
 * Where xx is interpreted as follows:
 * bits [4:0] = source id.
 */
#define FECS_PRIV_SOURCEID_MSK_VAL(x) \
	 ((x) & (nvgpu_safe_sub_u32(BIT32(5U), 1U)))

/*
 * Helper macros for decoding fecs pri orphan error of pattern:
 * BADF20xx - FECS_PRI_FLOORSWEEP.
 * Where xx is interpreted as follows:
 * bits [7:0] = target ringstation.
 */
#define FECS_PRIV_ORPHAN_TARGET_RINGSTN_MSK_VAL(x) \
	 ((x) & (nvgpu_safe_sub_u32(BIT32(8U), 1U)))

/*
 * Helper macros for decoding falcon mem access violation of pattern:
 * BADF54xx - FALCON_MEM_ACCESS_PRIV_LEVEL_VIOLATION.
 * Where xx is interpreted as follows:
 * bit [7] = 0 - IMEM, 1 - DMEM.
 * bit [6] = 0 - last transaction caused violation.
 * bits [5:4] = access level.
 * bits [3:0] = existing priv level mask.
 */
#define FALCON_DMEM_VIOLATION_MSK()		BIT32(7U)
#define FALCON_MEM_VIOLATION_MSK_VIOLATION()	BIT32(6U)
#define FALCON_MEM_VIOLATION_PRIVLEVEL_ACCESS_VAL(x) \
	(((x) & (BIT32(5U) | BIT32(4U))) >> 4U)
#define FALCON_MEM_VIOLATION_PRIVLEVEL_MSK_VAL(x) \
	 ((x) & (BIT32(3U) | BIT32(2U) | BIT32(1U) | BIT32(0U)))

/*
 * Helper macros for decoding source id mask violation of pattern:
 * BADF41xx - TARGET_MASK_VIOLATION.
 * Where xx is interpreted as follows:
 * bits [7:6] = target mask:
 *              00 - all rd/wr is blocked.
 *              01 - all wr is blocked, rd is downgraded to priv_level 0.
 *              02 - rd/wr is downgraded to priv_level 0.
 *              03 - rd/wr is allowed as it is.
 * bit [5] = b'0.
 * bits [4:0] = source id.
 */
#define TARGET_MASK_VIOLATION_MSK_VAL(x) \
	(((x) & (BIT32(7U) | BIT32(6U))) >> 6U)
#define TARGET_MASK_VIOLATION_SRCID_VAL(x) \
	((x) & (BIT32(4U) | BIT32(3U) | BIT32(2U) | BIT32(1U) | BIT32(0U)))

/*
 * Helper macros for decoding PRI access violation error of pattern:
 * BADF51xx - direct PRIV_LEVEL_VIOLATION.
 * Where xx is interpreted as follows:
 * bits [7:6] = b'00
 * bits [5:4] = request_priv_level
 * bits [3:0] = rd/wr protection mask
 */
#define PRI_ACCESS_VIOLATION_MSK_VAL(x) \
	(((x) & (BIT32(3U) | BIT32(2U) | BIT32(1U) | BIT32(0U))))
#define PRI_ACCESS_VIOLATON_LEVEL_VAL(x) \
	(((x) & (BIT32(5U) | BIT32(4U))) >> 4U)

/*
 * Helper macros for decoding PRI access violation error of pattern:
 * BADF52xx - indirect PRIV_LEVEL_VIOLATION.
 * Where xx is interpreted as follows:
 * bits [7:6] = b'00
 * bits [5:4] = current_request_priv_level
 * bits [3:2] = b'00
 * bits [1:0] = orig_request_priv_level
 */
#define PRI_ACCESS_VIOLATION_CUR_REQPL_VAL(x) \
	(((x) & (BIT32(5U) | BIT32(4U))) >> 4U)
#define PRI_ACCESS_VIOLATION_ORIG_REQPL_VAL(x) \
	((x) & (BIT32(1U) | BIT32(0U)))

/*
 * Helper macros for decoding source enable violations of pattern:
 * BADF57xx and BADF59xx - direct/indirect SOURCE_ENABLE_VIOLATION.
 * Where xx is interpreted as follows:
 * bits [7:6] = request_priv_level
 * bits [5] = source violation control
 * bits [4:0] = source id
 */
#define SRC_EN_VIOLAION_CTRL_VAL(x)	(((x) & BIT32(5U)) >> 5U)
#define SRC_EN_VIOLATION_PRIV_LEVEL_VAL(x) \
	(((x) & (BIT32(7U) | BIT32(6U))) >> 6U)
#define SRC_EN_VIOLATION_SRCID_VAL(x) \
	((x) & (BIT32(4U) | BIT32(3U) | BIT32(2U) | BIT32(1U) | BIT32(0U)))

/*
 * Helper macros for decoding pri lock from security sensor of pattern:
 * BADF60xx - pri lock due to security sensor.
 * Where xx is interpreted as follows:
 * bits [7:6] = b'00
 * bits [5] = pmu_dcls
 * bits [4] = gsp_dcls
 * bits [3] = sec2_dcls
 * bits [2] = nvdclk_scpm
 * bits [1] = fuse_scm
 * bits [0] = fuse_prod
 */
#define PRI_LOCK_SEC_SENSOR_PMU_MSK()		BIT32(5U)
#define PRI_LOCK_SEC_SENSOR_GSP_MSK()		BIT32(4U)
#define PRI_LOCK_SEC_SENSOR_SEC2_MSK()		BIT32(3U)
#define PRI_LOCK_SEC_SENSOR_NVDCLK_MSK()	BIT32(2U)
#define PRI_LOCK_SEC_SENSOR_FUSE_SCM_MSK()	BIT32(1U)
#define PRI_LOCK_SEC_SENSOR_FUSE_PROD_MSK()	BIT32(0U)

/*
 * Helper macros for decoding local priv ring errors of pattern:
 * BADF53xx - LOCAL_PRIV_RING_ERR
 * bits [6:0] = local target index
 */
#define PRIV_LOCAL_TARGET_INDEX(x) \
	((x) & (BIT32(6U) | BIT32(5U) | BIT32(4U) | BIT32(3U) | BIT32(2U) |\
		BIT32(1U) | BIT32(0U)))

struct gk20a;

void ga10b_priv_ring_isr_handle_0(struct gk20a *g, u32 status0);
void ga10b_priv_ring_isr_handle_1(struct gk20a *g, u32 status1);
void ga10b_priv_ring_decode_error_code(struct gk20a *g, u32 error_code);
u32 ga10b_priv_ring_enum_ltc(struct gk20a *g);
void ga10b_priv_ring_read_pri_fence(struct gk20a *g);

#ifdef CONFIG_NVGPU_MIG
int ga10b_priv_ring_config_gr_remap_window(struct gk20a *g, u32 gr_syspipe_id,
		bool enable);
int ga10b_priv_ring_config_gpc_rs_map(struct gk20a *g, bool enable);
#endif

#endif /* NVGPU_PRIV_RING_GA10B_H */
