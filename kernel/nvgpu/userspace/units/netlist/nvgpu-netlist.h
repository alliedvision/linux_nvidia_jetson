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
#ifndef UNIT_NVGPU_NETLIST_H
#define UNIT_NVGPU_NETLIST_H

#include <nvgpu/types.h>

/** @addtogroup SWUTS-netlist
 *  @{
 *
 * Software Unit Test Specification for netlist
 */

/**
 * Test specification for: test_netlist_init_support
 *
 * Description: The netlist unit shall query and populate
 * all ctxsw region info from ctxsw firmware.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_netlist_init_ctx_vars,
 *          gv11b_netlist_is_firmware_defined,
 *          gv11b_netlist_get_name,
 *          nvgpu_netlist_alloc_u32_list,
 *          nvgpu_netlist_alloc_aiv_list,
 *          nvgpu_netlist_alloc_av_list,
 *          nvgpu_netlist_alloc_av64_list
 *
 * Input: None
 *
 * Steps:
 * - Initialize the test environment for netlist unit testing:
 *   - Setup gv11b register spaces for hals to read emulated values.
 *   - Register read/write IO callbacks.
 *   - Setup init parameters to setup gv11b arch.
 *   - Initialize hal to setup the hal functions.
 * - Call nvgpu_netlist_init_ctx_vars to populate ctxsw region info
 *   from ctxsw firmware.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_netlist_init_support(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_netlist_query_tests
 *
 * Description: This test queries data related to different
 * ctxsw bundels and fecs/gpccs related info.
 * Checks whether valid data is retured or not.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_netlist_get_sw_non_ctx_load_av_list,
 *          nvgpu_netlist_get_sw_ctx_load_aiv_list,
 *          nvgpu_netlist_get_sw_method_init_av_list,
 *          nvgpu_netlist_get_sw_bundle_init_av_list,
 *          nvgpu_netlist_get_sw_veid_bundle_init_av_list,
 *          nvgpu_netlist_get_sw_bundle64_init_av64_list,
 *          nvgpu_netlist_get_fecs_inst_count,
 *          nvgpu_netlist_get_fecs_data_count,
 *          nvgpu_netlist_get_gpccs_inst_count,
 *          nvgpu_netlist_get_gpccs_data_count,
 *          nvgpu_netlist_get_fecs_inst_list,
 *          nvgpu_netlist_get_fecs_data_list,
 *          nvgpu_netlist_get_gpccs_inst_list,
 *          nvgpu_netlist_get_gpccs_data_list
 *
 * Input: None
 *
 * Steps:
 * - Call nvgpu_netlist_get_sw_non_ctx_load_av_list
 * - Call nvgpu_netlist_get_sw_ctx_load_aiv_list
 * - Call nvgpu_netlist_get_sw_method_init_av_list
 * - Call nvgpu_netlist_get_sw_bundle_init_av_list
 * - Call nvgpu_netlist_get_sw_veid_bundle_init_av_list
 * - Call nvgpu_netlist_get_sw_bundle64_init_av64_list
 * - Call nvgpu_netlist_get_fecs_inst_count
 * - Call nvgpu_netlist_get_fecs_data_count
 * - Call nvgpu_netlist_get_gpccs_inst_count
 * - Call nvgpu_netlist_get_gpccs_data_count
 * - Call nvgpu_netlist_get_fecs_inst_list
 * - Call nvgpu_netlist_get_fecs_data_list
 * - Call nvgpu_netlist_get_gpccs_inst_list
 * - Call nvgpu_netlist_get_gpccs_data_list
 * Checked called functions returns correct data
 *
 * Output: Returns PASS if returned data is valid. FAIL otherwise.
 */

int test_netlist_query_tests(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_netlist_negative_tests
 *
 * Description: This test covers negative paths in netlist init.
 *
 * Test Type: Feature, Error Injection
 *
 * Targets: nvgpu_netlist_init_ctx_vars,
 *          nvgpu_netlist_deinit_ctx_vars
 *
 * Input: None
 *
 * Steps:
 * - Call nvgpu_netlist_init_ctx_vars after already initilized netlist
 * - Call nvgpu_netlist_deinit_ctx_vars
 * - Call nvgpu_netlist_init_ctx_vars injecting allocation failures.
 * - Set HALs with no netlist defined and invalid netlist check
 * - Call nvgpu_netlist_init_ctx_vars with above test HALs
 * - Restore orginals HALs
 * - Call nvgpu_netlist_init_ctx_vars with correct HALs
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_netlist_negative_tests(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_netlist_remove_support
 *
 * Description: The netlist unit removes all populated netlist
 * region info.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_netlist_deinit_ctx_vars
 *
 * Input: None
 *
 * Steps:
 * - Call nvgpu_netlist_deinit_ctx_vars
 *
 * Output: Returns PASS
 */
int test_netlist_remove_support(struct unit_module *m,
		struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_NETLIST_H */
