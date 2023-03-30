/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef UNIT_NVGPU_FBP_H
#define UNIT_NVGPU_FBP_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-fbp
 *  @{
 *
 * Software Unit Test Specification for nvgpu.common.fbp
 */

/**
 * Test specification for: test_fbp_setup
 *
 * Description: Setup prerequisites for tests.
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Initialize HAL function pointers.
 * - Map the register space for NV_TOP and NV_FUSE.
 * - Register read/write callback functions.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating register space;
 * - UNIT_SUCCESS otherwise
 */
int test_fbp_setup(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_fbp_free_reg_space
 *
 * Description: Free resources from test_setup()
 *
 * Test Type: Other (cleanup)
 *
 * Input: test_fbp_setup() has been executed.
 *
 * Steps:
 * - Free up NV_TOP and NV_FUSE register space.
 *
 * Output:
 * - UNIT_SUCCESS
 */
int test_fbp_free_reg_space(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_fbp_init_and_query
 *
 * Description: Verify the FBP init and config query APIs exposed by common.fbp.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_fbp_init_support, nvgpu_fbp_get_max_fbps_count, nvgpu_fbp_get_fbp_en_mask
 *
 * Input: test_fbp_setup() has been executed.
 *
 * Steps:
 * - Initialize the FBP floorsweeping status in fuse to 0xE1 by writing to fuse
 *   register fuse_status_opt_fbp_r().
 * - Initialize the  maximum number of FBPs to 8 by writing to Top register
 *   top_num_fbps_r().
 * - Call nvgpu_fbp_init_support to initialize g->fbp.
 * - Read the g->fbp->max_fbp_count using nvgpu_fbp_get_max_fbps_count().
 * - Check if the max_fbps_count is initialized and read back correctly.
 * - Read the g->fbp->fbp_en_mask using nvgpu_fbp_get_fbp_en_mask().
 * - Check if the FBP en_mask is calculated correctly and read back right too.
 * - Initialize the FBP floorsweeping status in fuse to 5(Use different value
 *   than before to check if init occurs once.
 * - Call fbp_init_support again to ensure the initialization is done once.
 * - Check if the max_fbps_count is NOT set to new value(5).
 *
 * Output:
 * - UNIT_FAIL if above API fails to init g->fbp or read back values from g->fbp
 * - UNIT_SUCCESS otherwise
 */
int test_fbp_init_and_query(struct unit_module *m, struct gk20a *g, void *args);


/**
 * Test specification for: test_fbp_remove_support
 *
 * Description: Verify the nvgpu_fbp_remove_support exposed by common.fbp.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_fbp_remove_support
 *
 * Input: test_fbp_init_and_query() has been executed.
 *
 * Steps:
 * - Confirm if g->fbp != NULL before calling fbp_remov_support API.
 * - Call fbp_remove_support to cleanup the saved FBP data.
 * - Confirm if g->fbp == NULL after cleanup.
 * - Call fbp_remove_support with fbp pointer set to NULL for branch coverage.
 *
 * Output:
 * - UNIT_FAIL if above API fails to cleanup g->fbp;
 * - UNIT_SUCCESS otherwise
 */
int test_fbp_remove_support(struct unit_module *m, struct gk20a *g, void *args);
#endif /* UNIT_NVGPU_FBP_H */
