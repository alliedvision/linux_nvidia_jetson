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
#ifndef UNIT_NVGPU_GR_CONFIG_H
#define UNIT_NVGPU_GR_CONFIG_H

#include <nvgpu/types.h>

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-gr-config
 *  @{
 *
 * Software Unit Test Specification for common.gr.config
 */

/**
 * Test specification for: test_gr_config_init.
 *
 * Description: Setup for common.gr.config unit. This test helps
 * to read the GR engine configuration and stores the configuration
 * values in the #nvgpu_gr_config struct.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_gr_config_init,
 *          gm20b_gr_config_get_pes_tpc_mask,
 *          gm20b_gr_config_get_pd_dist_skip_table_size,
 *          gm20b_gr_config_get_tpc_count_in_gpc,
 *          gm20b_gr_config_get_gpc_tpc_mask
 *
 * Input: None
 *
 * Steps:
 * -  Call nvgpu_gr_config_init
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_config_init(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_config_deinit.
 *
 * Description: Cleanup common.gr.config unit.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_gr_config_deinit
 *
 * Input: #test_gr_init_setup and #test_gr_config_init
 *        must have been executed successfully.
 *
 * Steps:
 * -  Call nvgpu_gr_config_deinit
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_config_deinit(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_config_count.
 *
 * Description: This test helps to verify whether the configurations
 *              read from the h/w matches with locally stored informations
 *              for a particular chip.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_gr_config_get_max_gpc_count,
 *          nvgpu_gr_config_get_max_tpc_count,
 *          nvgpu_gr_config_get_max_tpc_per_gpc_count,
 *          nvgpu_gr_config_get_gpc_count,
 *          nvgpu_gr_config_get_tpc_count,
 *          nvgpu_gr_config_get_ppc_count,
 *          nvgpu_gr_config_get_pe_count_per_gpc,
 *          nvgpu_gr_config_get_sm_count_per_tpc,
 *          nvgpu_gr_config_get_gpc_mask,
 *          nvgpu_gr_config_get_gpc_ppc_count,
 *          nvgpu_gr_config_get_gpc_skip_mask,
 *          nvgpu_gr_config_get_gpc_tpc_count,
 *          nvgpu_gr_config_get_pes_tpc_count,
 *          nvgpu_gr_config_get_pes_tpc_mask,
 *          nvgpu_gr_config_get_base_count_gpc_tpc,
 *          nvgpu_gr_config_get_base_mask_gpc_tpc
 *
 * Input: #test_gr_init_setup and #test_gr_config_init
 *        must have been executed successfully.
 *
 * Steps:
 * -  Read configuration count and mask informations from the driver
 *    which got stored as part of the nvgpu_gr_config_init.
 *    Compare those values against the locally maintained table.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_config_count(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_config_set_get.
 *
 * Description: This test helps to verify whether the write and read back
 *              reflect the same value. This test helps to verify the
 *              configuration values can be changed as part of floorsweeping.
 *
 * Test Type: Feature, Error guessing, Boundary Value
 *
 * Equivalence classes:
 * Variable  : sm_id (nvgpu_gr_config_get_sm_info)
 * - Valid   : {0, (SM count - 1)}
 * - Invalid : {SM count, U32_MAX}
 * For GV11b, SM count = 8
 *
 * Targets: nvgpu_gr_config_set_no_of_sm,
 *          nvgpu_gr_config_get_no_of_sm,
 *          nvgpu_gr_config_get_sm_info,
 *          nvgpu_gr_config_set_sm_info_gpc_index,
 *          nvgpu_gr_config_get_sm_info_gpc_index,
 *          nvgpu_gr_config_set_sm_info_tpc_index,
 *          nvgpu_gr_config_get_sm_info_tpc_index,
 *          nvgpu_gr_config_set_sm_info_global_tpc_index,
 *          nvgpu_gr_config_get_sm_info_global_tpc_index,
 *          nvgpu_gr_config_set_sm_info_sm_index,
 *          nvgpu_gr_config_get_sm_info_sm_index,
 *          nvgpu_gr_config_set_gpc_tpc_mask,
 *          nvgpu_gr_config_get_gpc_tpc_mask
 *
 * Input: #test_gr_init_setup and #test_gr_config_init
 *        must have been executed successfully.
 *
 * Steps:
 * -  Random values are set for various configuration and read back to
 *    check those values.
 * -  For BVEC testing of nvgpu_gr_config_get_sm_info,
 *      - Get the 'SM count' based on TPC count and number of SMs per TPC.
 *      - Call nvgpu_gr_config_get_sm_info with input sm_id at boundary
 *        values - min boundary(0), max boundary(SM count - 1) and once
 *        with random value in valid range. nvgpu_gr_config_get_sm_info
 *        should return non NULL pointer with sm_info populated.
 *      - Call nvgpu_gr_config_get_sm_info with input sm_id at boundary
 *        values - min boundary(SM count), max boundary(U32_MAX) and once
 *        with random value in invalid range. nvgpu_gr_config_get_sm_info
 *        should return NULL pointer.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_config_set_get(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_config_error_injection.
 *
 * Description: This test helps to verify whether the kernel handles all
 *              possible error conditions for memory allocation failure. Also
 *		provide different configurations in common.gr unit.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_gr_config_init,
 *          nvgpu_gr_config_deinit,
 *          gops_gr_config.init_sm_id_table,
 *          gv100_gr_config_init_sm_id_table,
 *          nvgpu_gr_get_config_ptr
 *
 * Input: #test_gr_init_setup must have been executed successfully.
 *
 * Steps:
 * -  Force memory allocation failures for various structures within
 *    nvgpu_gr_config_init call.
 * -  Set for various configuration like pes_tpc_count, gpc_tpc_mask,
 *    gpc_count by adding stub function for various gr.config hal and
 *    call nvgpu_gr_config_init.
 * -  Force memory allocation failures with
 *    g->ops.gr.config.init_sm_id_table call.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_config_error_injection(struct unit_module *m, struct gk20a *g, void *args);
#endif /* UNIT_NVGPU_GR_CONFIG_H */

/**
 * @}
 */

