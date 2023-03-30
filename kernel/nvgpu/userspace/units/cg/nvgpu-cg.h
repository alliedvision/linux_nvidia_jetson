/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-cg
 *  @{
 *
 * Software Unit Test Specification for cg
 */

/**
 * Test specification for: test_cg
 *
 * Description: The cg unit shall be able to setup the clock gating register
 * values as specified in the hal reglist structures for BLCG/SLCG.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_cg_blcg_fb_load_enable, nvgpu_cg_blcg_fifo_load_enable,
 *          nvgpu_cg_blcg_ce_load_enable, nvgpu_cg_blcg_pmu_load_enable,
 *	    nvgpu_cg_blcg_gr_load_enable, nvgpu_cg_slcg_fb_load_enable,
 *	    nvgpu_cg_slcg_priring_load_enable, nvgpu_cg_slcg_fifo_load_enable,
 *	    nvgpu_cg_slcg_pmu_load_enable, nvgpu_cg_slcg_therm_load_enable,
 *	    nvgpu_cg_slcg_ce2_load_enable, nvgpu_cg_init_gr_load_gating_prod,
 *          nvgpu_cg_blcg_ltc_load_enable, nvgpu_cg_slcg_ltc_load_enable
 *
 * Input: The struct specifying type of clock gating, target nvgpu routine
 * that handles the setup, clock gating domain descriptors.
 *
 * Steps:
 * - Initialize the test environment:
 *   - Register read/write IO callbacks.
 *   - Add relevant fuse registers to the register space.
 *   - Initialize hal to setup the hal functions.
 *   - Initialize slcg and blcg gating register data by querying through
 *     nvgpu exported functions.
 * - Add the domain gating registers to the register space.
 * - Load invalid values in the gating registers.
 * - Invoke the nvgpu function to load the clock gating values.
 *   - Verify that load is not enabled as BLCG/SLCG enabled flag isn't set.
 * - Enable BLCG/SLCG enabled flag.
 * - Invoke the nvgpu function to load the clock gating values.
 *   - Verify that load is not enabled as platform capability isn't set.
 * - Disable BLCG/SLCG enabled flag.
 * - Set the platform capability.
 * - Invoke the nvgpu function to load the clock gating values.
 *   - Verify that load is not enabled as enabled flag isn't set.
 * - Enable BLCG/SLCG enabled flag.
 * - Invoke the nvgpu function to load the clock gating values.
 *   - Verify that load is enabled.
 * - Invoke the nvgpu functions to load the non-prod clock gating values.
 *   - Verify that load is not enabled.
 * - Set all CG gpu_ops to NULL.
 * - Invoke the nvgpu function to load the clock gating values.
 *   - Verify that load is not enabled as HALs are not set.
 * - Restore the CG gpu_ops.
 * - Any invalid accesses by nvgpu will be caught through ABORTs and
 *   test fails if ABORTs are encountered.
 * - Delete domain gating registers from the registere space.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_cg(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_elcg
 *
 * Description: The cg unit shall be able to setup the engine therm register
 * values to enable/disable ELCG.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_cg_elcg_enable_no_wait, nvgpu_cg_elcg_disable_no_wait
 *
 * Input: None
 *
 * Steps:
 * - Initialize the test environment:
 *   - Register read/write IO callbacks.
 *   - Add relevant fuse registers to the register space.
 *   - Initialize hal to setup the hal functions.
 * - Initialize fifo support to configure ELCG at engine level.
 * - Add the engine therm registers to the register space.
 * - Invoke the nvgpu function to enable/disable ELCG.
 *   - Verify that cg mode isn't set in therm registers as ELCG enabled flag
 *     isn't set.
 * - Enable ELCG enabled flag.
 * - Invoke the nvgpu function to enable/disable ELCG.
 *   - Verify that cg mode isn't set in therm registers as ELCG platform
 *     capability isn't set.
 * - Set the platform capability.
 * - Invoke the nvgpu function to enable/disable ELCG.
 *   - Verify that cg mode is set in therm registers.
 * - Any invalid accesses by nvgpu will be caught through ABORTs and
 *   test fails if ABORTs are encountered.
 * - Delete engine therm registers from the registere space.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_elcg(struct unit_module *m, struct gk20a *g, void *args);
