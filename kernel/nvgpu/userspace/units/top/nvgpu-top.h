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

#ifndef UNIT_NVGPU_TOP_H
#define UNIT_NVGPU_TOP_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-top
 *  @{
 *
 * Software Unit Test Specification for nvgpu.common.top
 */

/**
 * Test specification for: test_top_setup
 *
 * Description: Setup prerequisites for tests.
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Initialize common.top HAL function pointers.
 * - Map the register space for NV_TOP.
 * - Register read/write callback functions.
 * - Setup a device_info_table.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int test_top_setup(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_top_free_reg_space
 *
 * Description: Free resources from test_top_setup()
 *
 * Test Type: Other (cleanup)
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Free up NV_TOP register space.
 *
 * Output:
 * - UNIT_SUCCESS
 */
int test_top_free_reg_space(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_device_info_parse_enum
 *
 * Description: Verify the top.device_info_parse_enum HAL.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_top.device_info_parse_enum, gm20b_device_info_parse_enum
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Case 1: table entry to be parsed does not hit any error case
 *   - Initialize table entry such that:
 *	1. entry_type = enum = 2U.
 *	2. engine, reset, interrupt and runlist bits are all valid.
 *	3. engine_enum (Bits 29:26) = 4U.
 *	4. runlist_enum (Bits 24:21) = 1U.
 *	5. intr_enum (Bits 19:15) = 5U.
 *	6. reset_enum (Bits 13:9) = 6U.
 *   - So, table_entry = 0x10228C3E.
 *   - Call device_info_parse_enum HAL to parse the above table entry.
 *   - Verify if the parsed data is as expected.
 *
 * - Case 2: Setup table entry such that we hit error path branches.
 *   - Initialize table entry such that:
 *	1. entry_type = enum = 2U.
 *	2. Engine_bit = invalid = 0.
 *	3. runlist_bit = invalid = 0.
 *	4. intr_bit = invalid = 0.
 *	5. reset_bit = invalid = 0.
 *   - So, table_entry = 0x10228C02.
 *   - Call device_info_parse_enum HAL to parse the above table entry.
 *   - Verify if the parsed data is as expected.
 *
 * Output:
 * - UNIT_FAIL if above HAL does not parse enum as expected.
 * - UNIT_SUCCESS otherwise
 */
int test_device_info_parse_enum(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_get_max_gpc_count
 *
 * Description: Verify the top.get_max_gpc_count HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.get_max_gpc_count, gm20b_top_get_max_gpc_count
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to max_gpc_count (Bits 4:0) in
 *   top_num_gpcs_r() register to 4.
 * - Call get_max_gpc_count HAL.
 * - Verify the max_gpc_count is set to 4.
 * - Repeat above steps with max_gpc_count set to 0x1D so that we make sure
 *   all 5 bits are parsed.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to parse max_gpc_count.
 * - UNIT_SUCCESS otherwise
 */
int test_get_max_gpc_count(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_is_engine_gr
 *
 * Description: Verify the top.is_engine_gr HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.is_engine_gr, gm20b_is_engine_gr
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Call HAL with input equal to graphics enum = 0.
 * - Verify the HAL returns true.
 * - Call HAL with input not equal to graphics enum.
 * - Verify the HAL returns false.
 *
 * Output:
 * - UNIT_FAIL if above HAL returns false when input = graphics_enum and when
 *   it returns true when input != graphics enum.
 * - UNIT_SUCCESS otherwise
 */
int test_is_engine_gr(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_get_max_tpc_per_gpc_count
 *
 * Description: Verify the top.get_max_tpc_per_gpc_count HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.get_max_tpc_per_gpc_count,
 *          gm20b_top_get_max_tpc_per_gpc_count
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to max_tpc_per_gpc_count (Bits 4:0) in
 *   top_tpc_per_gpc_r() register to 4.
 * - Call get_max_tpc_per_gpc_count HAL.
 * - Verify the max_tpc_per_gpc_count is set to 4.
 * - Repeat above steps with max_tpc_per_gpc_count set to 0x1D so that we make
 *   sure all 5 bits are parsed.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to parse max_tpc_per_gpc_count.
 * - UNIT_SUCCESS otherwise
 */
int test_get_max_tpc_per_gpc_count(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_get_max_fbps_count
 *
 * Description: Verify the top.get_max_fbps_count HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.get_max_fbps_count, gm20b_top_get_max_fbps_count
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to max_fbps_count (Bits 4:0) in
 *   top_num_fbps_r() register to 4.
 * - Call get_max_fbps_count HAL.
 * - Verify the max_fbps_count is set to 4.
 * - Repeat above steps with max_fbps_count set to 0x1D so that we make sure
 *   all 5 bits are parsed.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to parse max_fbps_count.
 * - UNIT_SUCCESS otherwise
 */
int test_get_max_fbps_count(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_get_max_ltc_per_fbp
 *
 * Description: Verify the top.get_max_ltc_per_fbp HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.get_max_ltc_per_fbp, gm20b_top_get_max_ltc_per_fbp
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to max_ltc_per_fbp (Bits 4:0) in
 *   top_ltc_per_fbp_r() register to 4.
 * - Call get_max_ltc_per_fbp HAL.
 * - Verify the max_ltc_per_fbp is set to 4.
 * - Repeat above steps with max_ltc_per_fbp set to 0x1D so that we make sure
 *   all 5 bits are parsed.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to parse max_ltc_per_fbp.
 * - UNIT_SUCCESS otherwise
 */
int test_get_max_ltc_per_fbp(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_get_max_lts_per_ltc
 *
 * Description: Verify the top.get_max_lts_per_ltc HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.get_max_lts_per_ltc, gm20b_top_get_max_lts_per_ltc
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to max_lts_per_ltc (Bits 4:0) in
 *   top_slices_per_ltc_r() register to 4.
 * - Call get_max_lts_per_ltc HAL.
 * - Verify the max_lts_per_ltc is set to 4.
 * - Repeat above steps with max_lts_per_ltc set to 0x1D so that we make sure
 *   all 5 bits are parsed.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to parse max_lts_per_ltc.
 * - UNIT_SUCCESS otherwise
 */
int test_get_max_lts_per_ltc(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_get_num_ltcs
 *
 * Description: Verify the top.get_num_ltcs HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.get_num_ltcs, gm20b_top_get_num_ltcs
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to num_ltcs (Bits 4:0) in
 *   top_num_ltcs_r() register to 4.
 * - Call get_num_ltcs HAL.
 * - Verify the num_ltcs is set to 4.
 * - Repeat above steps with num_ltcs set to 0x1D so that we make sure
 *   all 5 bits are parsed.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to parse num_ltcs.
 * - UNIT_SUCCESS otherwise
 */
int test_get_num_ltcs(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_device_info_parse_data
 *
 * Description: Verify the top.device_info_parse_data HAL.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_top.device_info_parse_data, gv11b_device_info_parse_data
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Case 1: table entry to be parsed does not hit any error case
 *   - Initialize table entry such that:
 *	1. entry_type = data = 1.
 *	2. fault_id bit is valid.
 *	3. fault_id_enum (Bits 9:3) = 15.
 *	4. pri_base (Bits 23:12) = 0x104.
 *	5. inst_id (Bits 29:25) = 3.
 *	6. data_type = enum2 (bit 30) = 0.
 *   - So, table_entry = 0x8C10407D.
 *   - Call device_info_parse_data HAL to parse the above table entry.
 *   - Verify if the parsed data is as expected.
 *
 * - Case 2: Setup table entry such that we hit error path branch.
 *   - Initialize table entry such that:
 *	1. fault_id_bit = invalid = 0.
 *   - So, table_entry = 0x8C104079.
 *   - Call device_info_parse_data HAL to parse the above table entry.
 *   - Verify if the parsed data is as expected.
 *
 * - Case 3: Setup table_entry such that the HAL fails with -EINVAL
 *   - Initialize table entry such that:
 *	1. data_type != enum2
 *   - So, table_entry = 0xCC10407D.
 *   - Call device_info_parse_data HAL to parse the above table entry.
 *   - Verify if the retval is as expected (-EINVAL).
 *
 * Output:
 * - UNIT_FAIL if above HAL does not parse data as expected.
 * - UNIT_SUCCESS otherwise
 */
int test_device_info_parse_data(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_get_num_engine_type_entries
 *
 * Description: Verify top.get_num_engine_type_entries HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.get_num_engine_type_entries,
 *          gp10b_get_num_engine_type_entries
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - The device_info table is setup during test_top_setup().
 * - The device_info table is initialized to have 2 copy engine entries.
 * - Call get_num_engine_type_entries HAL to parse number of copy engine
 *   related entries in the device_info table.
 * - Verify that number_of_entries = 2.
 *
 * Output:
 * - UNIT_FAIL if above HAL does not return number of CE entries as expected.
 * - UNIT_SUCCESS otherwise
 */
int test_get_num_engine_type_entries(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_get_device_info
 *
 * Description: Verify top.get_device_info HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.get_device_info, gp10b_get_device_info
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - The device_info table is setup during test_top_setup().
 * - The device_info table is initialized to have one copy engine entry.
 * - Set device_info_parse_enum() and device_info_parse_data() HAL to NULL.
 *   Call get_device_info HAL and check if it returns -EINVAL when the data
 *   parsing function pointers are not initialized.
 * - Initialize the device_info_parse_enum and device_info_parse_data HAL.
 * - Call get_device_info HAL to parse copy engine related data.
 * - Here, we just make sure the call returns success; we do not check the
 *   parsed values as we have separate tests for verifying enum and data
 *   parsing code.
 * - Call get_device_info HAL to parse copy engine entry with faulty entry
 * - Verify if get_device_info HAL returns error(-EINVAL).
 * - Call top.get_device_info with NULL pointer to cover error path.
 * - Verify if the retval is as expected(-EINVAL).
 *
 * Output:
 * - UNIT_FAIL if call to get_device_info HAL fails.
 * - UNIT_SUCCESS otherwise
 */
int test_get_device_info(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_is_engine_ce
 *
 * Description: Verify the top.is_engine_ce HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.is_engine_ce, gp10b_is_engine_ce
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Call HAL with input equal to copy engine enum = 0.
 * - Verify the HAL returns true.
 * - Call HAL with input not equal to copy engine enum.
 * - Verify the HAL returns false.
 *
 * Output:
 * - UNIT_FAIL if above HAL returns false when input = copy_engine_enum and when
 *   it returns true when input != copy_engine_enum.
 * - UNIT_SUCCESS otherwise
 */
int test_is_engine_ce(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_get_num_lce
 *
 * Description: Verify the top.get_num_lce HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_top.get_num_lce, gv11b_top_get_num_lce
 *
 * Input: test_top_setup() has been executed.
 *
 * Steps:
 * - Initialize bits corresponding to num_lce (Bits 4:0) in
 *   top_num_ces_r() register to 4.
 * - Call get_num_lce HAL.
 * - Verify the num_lce is set to 4.
 * - Repeat above steps with num_lce set to 0x1D so that we make sure
 *   all 5 bits are parsed.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to parse num_lce.
 * - UNIT_SUCCESS otherwise
 */
int test_get_num_lce(struct unit_module *m, struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_TOP_H */
