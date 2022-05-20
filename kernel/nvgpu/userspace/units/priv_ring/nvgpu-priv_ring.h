/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef UNIT_NVGPU_PRIV_RING_H
#define UNIT_NVGPU_PRIV_RING_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-priv_ring
 *  @{
 *
 * Software Unit Test Specification for nvgpu.common.priv_ring
 */

/**
 * Test specification for: test_priv_ring_setup
 *
 * Description: Setup prerequisites for tests.
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Initialize common.priv_ring and few other necessary HAL function pointers.
 * - Map the register space for NV_PRIV_MASTER, NV_PRIV_SYS, NV_PRIV_GPC
 *   and NV_PMC
 * - Register read/write callback functions.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int test_priv_ring_setup(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_priv_ring_free_reg_space
 *
 * Description: Free resources from test_priv_ring_setup()
 *
 * Test Type: Other (cleanup)
 *
 * Input: test_priv_ring_setup() has been executed.
 *
 * Steps:
 * - Free up NV_PRIV_MASTER, NV_PRIV_SYS, NV_PRIV_GPC and NV_PMC register space.
 *
 * Output:
 * - UNIT_SUCCESS
 */
int test_priv_ring_free_reg_space(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_enable_priv_ring
 *
 * Description: Verify the priv_ring.enable_priv_ring HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_priv_ring.enable_priv_ring, gm20b_enable_priv_ring
 *
 * Input: test_priv_ring_setup() has been executed.
 *
 * Steps:
 * - Call enable_priv_ring() HAL.
 * - Read back the registers to make sure intended values are written.
 *      pri_ringmaster_command_r = 0x4
 *      pri_ringstation_sys_decode_config_r = 0x2
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to enable interrupts.
 * - UNIT_SUCCESS otherwise.
 */
int test_enable_priv_ring(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_set_ppriv_timeout_settings
 *
 * Description: Verify the priv_ring.set_ppriv_timeout_settings HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_priv_ring.set_ppriv_timeout_settings,
 *          gm20b_priv_set_timeout_settings
 *
 * Input: test_priv_ring_setup() has been executed.
 *
 * Steps:
 * - Call set_ppriv_timeout_settings HAL to set the timeout values to 0x800.
 * - Read back the registers to make sure the timeouts are set to 0x800.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to set timeouts.
 * - UNIT_SUCCESS otherwise.
 */
int test_set_ppriv_timeout_settings(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_enum_ltc
 *
 * Description: Verify the priv_ring.enum_ltc HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_priv_ring.enum_ltc, gm20b_priv_ring_enum_ltc.
 *
 * Input: test_priv_ring_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to count (Bits 4:0) in
 *   pri_ringmaster_enum_ltc_r() to 0x1D to make sure all 5 bits are parsed.
 * - Call enum_ltc() HAL.
 * - Verify that the HAL returns .
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to LTC count.
 * - UNIT_SUCCESS otherwise
 */
int test_enum_ltc(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_get_gpc_count
 *
 * Description: Verify the priv_ring.get_gpc_count HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_priv_ring.get_gpc_count, gm20b_priv_ring_get_gpc_count
 *
 * Input: test_priv_ring_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to count (Bits 4:0) in
 *   pri_ringmaster_enum_gpc_r() to 0x1D to make sure all 5 bits are parsed.
 * - Call get_gpc_count() HAL.
 * - Verify that the HAL returns 4.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to GPC count.
 * - UNIT_SUCCESS otherwise
 */
int test_get_gpc_count(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_get_fbp_count
 *
 * Description: Verify the priv_ring.get_fbp_count HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_priv_ring.get_fbp_count, gm20b_priv_ring_get_fbp_count
 *
 * Input: test_priv_ring_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to count (Bits 4:0) in
 *   pri_ringmaster_enum_fbp_r() to 0x1D to make sure all 5 bits are parsed.
 * - Call get_fbp_count() HAL.
 * - Verify that the HAL returns 4.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to FBP count.
 * - UNIT_SUCCESS otherwise
 */
int test_get_fbp_count(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_priv_ring_isr
 *
 * Description: Verify the priv_ring.isr HAL.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_priv_ring.isr, gp10b_priv_ring_isr
 *
 * Input: test_priv_ring_setup() has been executed.
 *
 * Steps:
 * - Set status0 such that:
 *    1. start_conn_fault (Bit 0:0) = 1.
 *    2. disconnect_fault (Bit 1:1)  = 1.
 *    3. overflow_fault (Bit 2:2) = 1.
 *    4. gbl_write_error (Bit 8:8) = 1.
 *    So status0 = 0x00000107.
 * - Set status1 such that:
 *    1. gbl_write_error (Bit 31:0) = 0x14.
 * - Set Count field in pri_ringmaster_enum_gpc_r to 0x1D.
 * - Call priv_ring ISR and clear the interrupts using readl callback.
 * - For increasing branch coverage:
 *    1. Call ISR with g->ops.priv_ring.decode_error_code = NULL.
 *    2. To cover negative case in for loop, call ISR with
 *       g->ops.priv_ring.get_gpc_count(g) = 0.
 *    3. Call the ISR again without clearing the interrupts and setting
 *       status0 and status1 to 0.
 *
 * Output:
 * - UNIT_SUCCESS
 */
int test_priv_ring_isr(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_decode_error_code
 *
 * Description: Verify the priv_ring.decode_error_code HAL.
 *
 * Test Type: Feature, Error injection, Boundary Value
 *
 * Targets: gops_priv_ring.decode_error_code, gp10b_decode_error_code
 *
 * Input: test_priv_ring_setup() has been executed.
 * Equivalence classes:
 * engine_id
 * - Valid : {0 - U32_MAX}
 *
 * Steps:
 * - Call decode_error_code HAL with different error_codes covering all the
 *   branches (0xBADF1xxx, 0xBADF2xxx, 0xBADF3xxx, 0xBADF5xxx).
 * - Include error codes with reference the largest index for each of the error
 *   types.
 * - Include boundary values and one random number in between the range [0 - U32_MAX]
 *
 * Output:
 * - UNIT_SUCCESS
 */
int test_decode_error_code(struct unit_module *m, struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_PRIV_RING_H */
