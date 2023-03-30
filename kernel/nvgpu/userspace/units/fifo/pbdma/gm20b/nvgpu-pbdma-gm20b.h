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
#ifndef UNIT_NVGPU_PBDMA_GM20B_H
#define UNIT_NVGPU_PBDMA_GM20B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-pbdma-gm20b
 *  @{
 *
 * Software Unit Test Specification for fifo/pbdma/gm20b
 */

/**
 * Test specification for: test_gm20b_pbdma_acquire_val
 *
 * Description: Branch coverage for PBDMA acquire timeout.
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.acquire_val, gm20b_pbdma_acquire_val
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Acquire timeout varying from 0 to 2^32 ms.
 * - Compute expected value in ns. It should be the minimum of:
 *   - 80% of requested timeout.
 *   - maximum timeout that can be specified with mantissa and exponent.
 * - Compute actual value in ns from mantissa and exponent.
 * - Check that delta between expected and actual values is lower than
 *   1024 * (1 << exponent).
 * - Check that BUG_ON occurs on overflow while converting ms to ns.
 * - Check that enable bit is not set when 0 is passed to
 *   gm20b_pbdma_acquire_val.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_acquire_val(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gm20b_pbdma_handle_intr
 *
 * Description: Branch coverage for PBDMA interrupt handler
 *
 * Targets: gops_pbdma.handle_intr, gm20b_pbdma_handle_intr
 *
 * Test Type: Feature
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Set pbdma_intr_0_r and pbdma_intr_1_r registers.
 * - Check cases where stalling interrupts are pending:
 *   - Set pbdma_intr_0_r to non-zero value.
 *   - Check that handle_intr_0 is called using stub.
 *   - Check that read_pbdma_status_info is called when
 *     handle_intr_0 returns true (recovery needed).
 * - Check cases where non-stalling interrupts are pending:
 *   - Set pbdma_intr_1_r to non-zero value.
 *   - Check that handle_intr_1 is called using stub.
 *   - Check that read_pbdma_status_info is called when
 *     handle_intr_1 returns true (recovery needed).
 * - Check that error notifier is set, when provided.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_handle_intr(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gm20b_pbdma_handle_intr_0
 *
 * Description: Branch coverage for PBDMA stalling interrupt handler
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.handle_intr_0, gm20b_pbdma_handle_intr_0,
 *          gops_pbdma.reset_header, gm20b_pbdma_reset_header,
 *          gm20b_pbdma_reset_method
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Set pbdma_intr_0 and check interrupt handling for combinations of
 *   the following interrupts:
 *   - pbdma_intr_0_memreq: Check that recover is true.
 *   - pbdma_intr_0_acquire: Check that recover is true when timeouts are
 *     enabled, false otherwise. Check that error notifier is set when
 *     timeouts are enable.
 *   - pbdma_intr_0_pbentry: Check that pbdma method0 and headers have been
 *     reset, and that recover is true.
 *   - pbdma_intr_0_method: Check that method0 has been reset, and that
 *     recover is true.
 *   - pbdma_intr_0_pbcrc: Check that recover is true and that error notifier
 *     has been set.
 *   - pbdma_intr_0_device: Check that all pbdma subch methods and header
 *     have been reset and that recover is true.
 * - Check that recover is false, when none of above interrupt is raised.
 * - Check that BUG() occurs when passing an invalid pbdma_id that
 *   causes an overflow in register computation.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_handle_intr_0(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gm20b_pbdma_read_data
 *
 * Description: Branch coverage for reading PBDMA header shadow.
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.read_data, gm20b_pbdma_read_data
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - For each H/W PBDMA id, set pbdma_hdr_shadow_r(pbdma_id) with a pattern,
 *   and read it back with gm20b_pbdma_read_data.
 * - Check that value matches pattern.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_read_data(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gm20b_pbdma_intr_descs
 *
 * Description: Branch coverage for interrupt descriptors.
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.device_fatal_0_intr_descs,
 *          gm20b_pbdma_device_fatal_0_intr_descs,
 *          gops_pbdma.restartable_0_intr_descs,
 *          gm20b_pbdma_restartable_0_intr_descs
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that g->fifo->intr_descs contains the interrupts
 *   specified in gm20b_pbdma_device_fatal_0_intr_descs and
 *   gm20b_pbdma_restartable_0_intr_descs.
 * - Check that fatal_0 and restartable_0 interrupts masks are non-zero.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_intr_descs(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gm20b_pbdma_format_gpfifo_entry
 *
 * Description: Format a GPFIFO entry.
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.format_gpfifo_entry, gm20b_pbdma_format_gpfifo_entry
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Zero-initialize a gpfifo_entry then call gm20b_pbdma_format_gpfifo_entry
 *   with dummy pb_gpu_va and method_size.
 * - Check that pb_gpu_va and method_size are properly encoded in
 *   gp_fifo_entry.entry0 and gp_fifo_entry.entry1.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_format_gpfifo_entry(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gm20b_pbdma_get_gp_base
 *
 * Description: Branch coverage for GPFIFO base.
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.get_gp_base, gm20b_pbdma_get_gp_base,
 *          gops_pbdma.get_gp_base_hi, gm20b_pbdma_get_gp_base_hi
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that BUG() is raised when invoking gm20b_pbdma_get_gp_base_hi
 *   with 0 gpfifo_entry (because of ilog2).
 * - For each power of 2 between 1 and 16:
 *  - Call gm20b_pbdma_get_gp_base and gm20b_pbdma_get_gp_base_hi with
 *    dummy gpfifo_base.
 *  - Check that address and number of entries are properly encoded.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_get_gp_base(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gm20b_pbdma_get_fc_subdevice
 *
 * Description: Check RAMFC wrappers for instance block init
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.get_fc_subdevice, gm20b_pbdma_get_fc_subdevice
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that value returned by test_gm20b_pbdma_get_fc_subdevice is
 *   consistent with H/W manuals.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_get_fc_subdevice(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gm20b_pbdma_get_ctrl_hce_priv_mode_yes
 *
 * Description: Check RAMFC wrappers for instance block init
 *
 * Test Type: Feature based
 *
 * Targets: gops_pbdma.get_ctrl_hce_priv_mode_yes,
 *          gm20b_pbdma_get_ctrl_hce_priv_mode_yes
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that value returned by gm20b_pbdma_get_ctrl_hce_priv_mode_yes
 *   is consistent with H/W manuals.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_get_ctrl_hce_priv_mode_yes(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gm20b_pbdma_get_userd_addr
 *
 * Description: Check USERD HALs for instance block init
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.get_userd_addr, gm20b_pbdma_get_userd_addr,
 *          gops_pbdma.get_userd_hi_addr, gm20b_pbdma_get_userd_hi_addr,
 *          gops_pbdma.get_userd_aperture_mask,
 *          gm20b_pbdma_get_userd_aperture_mask
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that lo and hi addresses returned by gm20b_pbdma_get_userd_addr and
 *   gm20b_pbdma_get_userd_hi_addr match format from H/W manuals.
 * - Check that BUG() is returned when gm20b_pbdma_get_userd_aperture_mask
 *   is called with and invalid aperture.
 * - Check aperture masks returned by gm20b_pbdma_get_userd_aperture_mask
 *   for APERTURE_SYSMEM, APERTURE_SYSMEM_COH and APERTURE_VIDMEM.
 * - Check that aperture mask is always pbdma_userd_target_vid_mem_f() when
 *   NVGPU_MM_HONORS_APERTURE is false.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_pbdma_get_userd(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_PBDMA_GM20B_H */
