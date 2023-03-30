/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef UNIT_NVGPU_PBDMA_H
#define UNIT_NVGPU_PBDMA_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-pdbma
 *  @{
 *
 * Software Unit Test Specification for fifo/pbdma
 */

/**
 * Test specification for: test_pbdma_setup_sw
 *
 * Description: Branch coverage for nvgpu_pbdma_setup/cleanup_sw
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_pbdma_setup_sw, nvgpu_pbdma_cleanup_sw
 *
 * Input: none.
 *
 * Steps:
 * - Check memory allocation failure case for pbdma_map, using kzalloc
 *   fault injection.
 * - Check setting of unrecoverable PBDMA interrupt desc.
 *   (by using stub for g->ops.pbdma.device_fatal_0_intr_descs)
 * - Check setting of recoverable channel-specific PBDMA interrupt desc.
 *   (by using stub for g->ops.pbdma.channel_fatal_0_intr_descs)
 * - Check setting og recoverable non-channel specific PBDMA interrupt desc.
 *   (by using stub for g->ops.pbdma.restartable_0_intr_descs)
 *   In negative testing case, original state is restored after checking
 *   that nvgpu_tsg_open failed.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_pbdma_setup_sw(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_pbdma_status
 *
 * Description: Branch coverage for nvgpu_pbdma_status_* functions.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_pbdma_status_is_chsw_switch, nvgpu_pbdma_status_is_chsw_load,
 *          nvgpu_pbdma_status_is_chsw_save, nvgpu_pbdma_status_is_chsw_valid,
 *          nvgpu_pbdma_status_is_id_type_tsg,
 *          nvgpu_pbdma_status_is_next_id_type_tsg
 *
 * Input: test_fifo_init_support() run for this GPU.
 *
 * Steps:
 * - Build fake struct nvgpu_pbdma_status_info.
 * - Check that nvgpu_pbdma_status_is_chsw_switch is true when
 *   chsw status is NVGPU_PBDMA_CHSW_STATUS_SWITCH, false otherwise.
 * - Check that nvgpu_pbdma_status_is_chsw_load is true when
 *   chsw status is NVGPU_PBDMA_CHSW_STATUS_LOAD, false otherwise.
 * - Check that nvgpu_pbdma_status_is_chsw_save is true when
 *   chsw status is NVGPU_PBDMA_CHSW_STATUS_SAVE, false otherwise.
 * - Check that nvgpu_pbdma_status_is_chsw_valid is true when
 *   id_type is PBDMA_STATUS_ID_TYPE_TSGID, false otherwise.
 * - Check that nvgpu_pbdma_status_is_next_id_type_tsg is true when
 *   next_id_type is PBDMA_STATUS_NEXT_ID_TYPE_TSGID, false otherwise.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_pbdma_status(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_PBDMA_H */
