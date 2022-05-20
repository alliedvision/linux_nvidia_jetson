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

#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/device.h>
#include <hal/top/top_gm20b.h>
#include <hal/top/top_gp10b.h>
#include <hal/top/top_gv11b.h>
#include <nvgpu/hw/gv11b/hw_top_gv11b.h>

#include "nvgpu-top.h"

/*
 * Write callback.
 */
static void writel_access_reg_fn(struct gk20a *g,
					struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

/*
 * Read callback.
 */
static void readl_access_reg_fn(struct gk20a *g,
				struct nvgpu_reg_access *access)
{
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}

static struct nvgpu_posix_io_callbacks test_reg_callbacks = {
	/* Write APIs all can use the same accessor. */
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,

	/* Likewise for the read APIs. */
	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,
};

/* NV_TOP register space */
#define NV_TOP_START 0x00022400U
#define NV_TOP_SIZE  0x000003FFU

int test_top_setup(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 i;
	u32 entry_count = 0U;

	/* Init HAL */
	g->ops.top.device_info_parse_enum = gm20b_device_info_parse_enum;
	g->ops.top.device_info_parse_data = gv11b_device_info_parse_data;
	g->ops.top.get_max_gpc_count = gm20b_top_get_max_gpc_count;
	g->ops.top.get_max_tpc_per_gpc_count =
				gm20b_top_get_max_tpc_per_gpc_count;
	g->ops.top.get_max_fbps_count = gm20b_top_get_max_fbps_count;
	g->ops.top.get_max_ltc_per_fbp = gm20b_top_get_max_ltc_per_fbp;
	g->ops.top.get_max_lts_per_ltc = gm20b_top_get_max_lts_per_ltc;
	g->ops.top.get_num_ltcs = gm20b_top_get_num_ltcs;
	g->ops.top.get_num_lce = gv11b_top_get_num_lce;

	/* Map register space NV_TOP */
	if (nvgpu_posix_io_add_reg_space(g, NV_TOP_START, NV_TOP_SIZE) != 0) {
		unit_err(m, "%s: failed to register space: NV_TOP\n",
								__func__);
		return UNIT_FAIL;
	}

	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

	/* Setup a device_info_table
	 * We populate two entries for copy engine.
	 */
	entry_count = top_device_info__size_1_v();
	for (i = 0; i < entry_count ; i++) {
		nvgpu_posix_io_writel_reg_space(g, top_device_info_r(i), 0);
	}
	nvgpu_posix_io_writel_reg_space(g, top_device_info_r(1), 0x90228C3E);
	nvgpu_posix_io_writel_reg_space(g, top_device_info_r(2), 0x8C10407D);
	nvgpu_posix_io_writel_reg_space(g, top_device_info_r(3), 0x0000004F);
	nvgpu_posix_io_writel_reg_space(g, top_device_info_r(4), 0x94230E3E);
	nvgpu_posix_io_writel_reg_space(g, top_device_info_r(5), 0xC8104085);
	nvgpu_posix_io_writel_reg_space(g, top_device_info_r(6), 0x0000004F);

	return UNIT_SUCCESS;
}

int test_top_free_reg_space(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Free register space */
	nvgpu_posix_io_delete_reg_space(g, NV_TOP_START);

	return UNIT_SUCCESS;
}

int test_device_info_parse_enum(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_SUCCESS;
	u32 engine_id = 0U;
	u32 runlist_id = 0U;
	u32 intr_id = 0U;
	u32 reset_id = 0U;
	u32 table_entry;

	/* Initialize table entry such that:
	 * 1. entry_type = enum = 2U.
	 * 2. engine, reset, interrupt and runlist bits are all valid.
	 * 3. engine_enum (Bits 29:26) = 4U.
	 * 4. runlist_enum (Bits 24:21) = 1U.
	 * 5. intr_enum (Bits 19:15) = 5U.
	 * 6. reset_enum (Bits 13:9) = 6U.
	 */
	table_entry = 0x10228C3E;

	/* Call top.device_info_parse_enum to parse the above table entry */
	g->ops.top.device_info_parse_enum(g, table_entry, &engine_id,
						&runlist_id, &intr_id,
						&reset_id);

	/* Verify if the parsed data is as expected */
	if (engine_id != 4U) {
		unit_err(m,
			"device_info_parse_enum failed to parse engine_id.\n");
		ret = UNIT_FAIL;
	}
	if (runlist_id != 1U) {
		unit_err(m,
			"device_info_parse_enum failed to parse runlist_id.\n");
		ret = UNIT_FAIL;
	}
	if (intr_id != 5U) {
		unit_err(m,
			"device_info_parse_enum failed to parse intr_id.\n");
		ret = UNIT_FAIL;
	}
	if (reset_id != 6U) {
		unit_err(m,
			"device_info_parse_enum failed to parse reset_id.\n");
		ret = UNIT_FAIL;
	}

	/* To get additional branch coverage, Set:
	 * 1. entry_type = enum = 2
	 * 2. Engine_bit = invalid = 0.
	 * 3. runlist_bit = invalid = 0.
	 * 4. intr_bit = invalid = 0.
	 * 5. reset_bit = invalid = 0.
	 */
	table_entry = 0x10228C02;

	/* Call top.device_info_parse_enum to parse the above table entry */
	g->ops.top.device_info_parse_enum(g, table_entry, &engine_id,
						&runlist_id, &intr_id,
						&reset_id);

	/* Verify if the parsed data is as expected */
	if (engine_id != U32_MAX) {
		unit_err(m,
			"device_info_parse_enum failed to parse engine_id.\n");
		ret = UNIT_FAIL;
	}
	if (runlist_id != U32_MAX) {
		unit_err(m,
			"device_info_parse_enum failed to parse runlist_id.\n");
		ret = UNIT_FAIL;
	}
	if (intr_id != U32_MAX) {
		unit_err(m,
			"device_info_parse_enum failed to parse intr_id.\n");
		ret = UNIT_FAIL;
	}
	if (reset_id != U32_MAX) {
		unit_err(m,
			"device_info_parse_enum failed to parse reset_id.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_get_max_gpc_count(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set max_gpc_count (Bits 4:0) = 4 */
	nvgpu_posix_io_writel_reg_space(g, top_num_gpcs_r(), 0x4U);
	val = g->ops.top.get_max_gpc_count(g);
	if (val != 4) {
		unit_err(m, "max GPCs count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	/* Set max_gpc_count (Bits 4:0) = 0x1D */
	nvgpu_posix_io_writel_reg_space(g, top_num_gpcs_r(), 0xE28A321DU);
	val = g->ops.top.get_max_gpc_count(g);
	if (val != 0x1D) {
		unit_err(m, "max GPCs count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_get_max_tpc_per_gpc_count(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set max_tpc_per_gpc_count (Bits 4:0) = 4 */
	nvgpu_posix_io_writel_reg_space(g, top_tpc_per_gpc_r(), 0x4U);
	val = g->ops.top.get_max_tpc_per_gpc_count(g);
	if (val != 4) {
		unit_err(m, "TPC per GPC parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	/* Set max_tpc_per_gpc_count (Bits 4:0) = 0x1D */
	nvgpu_posix_io_writel_reg_space(g, top_tpc_per_gpc_r(), 0xE28A321DU);
	val = g->ops.top.get_max_tpc_per_gpc_count(g);
	if (val != 0x1D) {
		unit_err(m, "TPC per GPC parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_get_max_fbps_count(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set max_fbps_count (Bits 4:0) = 4 */
	nvgpu_posix_io_writel_reg_space(g, top_num_fbps_r(), 0x4U);
	val = g->ops.top.get_max_fbps_count(g);
	if (val != 4) {
		unit_err(m, "max FBPs count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	/* Set max_fbps_count (Bits 4:0) = 0x1D */
	nvgpu_posix_io_writel_reg_space(g, top_num_fbps_r(), 0xE28A321DU);
	val = g->ops.top.get_max_fbps_count(g);
	if (val != 0x1D) {
		unit_err(m, "max FBPs count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_get_max_ltc_per_fbp(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set max_ltc_per_fbp_count (Bits 4:0) = 4 */
	nvgpu_posix_io_writel_reg_space(g, top_ltc_per_fbp_r(), 0x4U);
	val = g->ops.top.get_max_ltc_per_fbp(g);
	if (val != 4) {
		unit_err(m, " LTC per FBP parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	/* Set max_ltc_per_fbp_count (Bits 4:0) = 0x1D */
	nvgpu_posix_io_writel_reg_space(g, top_ltc_per_fbp_r(), 0xE28A321DU);
	val = g->ops.top.get_max_ltc_per_fbp(g);
	if (val != 0x1D) {
		unit_err(m, "LTC per FBP parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_get_max_lts_per_ltc(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set max_lts_per_ltc_count (Bits 4:0) = 4 */
	nvgpu_posix_io_writel_reg_space(g, top_slices_per_ltc_r(), 0x4U);
	val = g->ops.top.get_max_lts_per_ltc(g);
	if (val != 4) {
		unit_err(m, " LTS per LTC parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	/* Set max_lts_per_ltc_count (Bits 4:0) = 0x1D */
	nvgpu_posix_io_writel_reg_space(g, top_slices_per_ltc_r(), 0xE28A321DU);
	val = g->ops.top.get_max_lts_per_ltc(g);
	if (val != 0x1D) {
		unit_err(m, "LTS per LTC parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_get_num_ltcs(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set num_ltcs_count (Bits 4:0) = 4 */
	nvgpu_posix_io_writel_reg_space(g, top_num_ltcs_r(), 0x4U);
	val = g->ops.top.get_num_ltcs(g);
	if (val != 4) {
		unit_err(m, "LTCs count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	/* Set num_ltcs_count (Bits 4:0) = 0x1D */
	nvgpu_posix_io_writel_reg_space(g, top_num_ltcs_r(), 0xE28A321DU);
	val = g->ops.top.get_num_ltcs(g);
	if (val != 0x1D) {
		unit_err(m, "LTCs count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_device_info_parse_data(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_SUCCESS;
	int val = 0;
	u32 inst_id = 0U;
	u32 pri_base = 0U;
	u32 fault_id = 0U;
	u32 table_entry;

	/* Initialize table entry such that:
	 * 1. entry_type = data = 1.
	 * 2. fault_id bit is valid.
	 * 3. fault_id_enum (Bits 9:3) = 15.
	 * 4. pri_base (Bits 23:12) = 0x104.
	 * 5. inst_id (Bits 29:25) = 3.
	 * 6. data_type = enum2 (bit 30) = 0.
	 */
	table_entry = 0x8C10407D;

	/* Call top.device_info_parse_data to parse the above table entry */
	val = g->ops.top.device_info_parse_data(g, table_entry, &inst_id,
						&pri_base, &fault_id);

	if (val != 0) {
		unit_err(m, "Call to top.device_info_parse_data() failed.\n");
		ret = UNIT_FAIL;
	}

	/* Verify if the parsed data is as expected */
	if (inst_id != 3U) {
		unit_err(m,
			"device_info_parse_data failed to parse inst_id.\n");
		ret = UNIT_FAIL;
	}
	if (pri_base != 0x104000U) {
		unit_err(m,
			"device_info_parse_data failed to parse pri_base.\n");
		ret = UNIT_FAIL;
	}
	if (fault_id != 15U) {
		unit_err(m,
			"device_info_parse_data failed to parse fault_id.\n");
		ret = UNIT_FAIL;
	}

	/* To get additional branch coverage, Set:
	 * 1. fault_id_bit = invalid = 0.
	 */
	table_entry = 0x8C104079;

	/* Call top.device_info_parse_data to parse the above table entry */
	val = g->ops.top.device_info_parse_data(g, table_entry, &inst_id,
						&pri_base, &fault_id);

	if (val != 0) {
		unit_err(m, "Call to top.device_info_parse_data() failed.\n");
		ret = UNIT_FAIL;
	}

	/* Verify if the parsed data is as expected */
	if (fault_id != U32_MAX) {
		unit_err(m,
			"device_info_parse_data failed to parse fault_id.\n");
		ret = UNIT_FAIL;
	}

	/* To cover an error branch, set table entry such that:
	 * 1. data_type != enum2.
	 */
	table_entry = 0xCC10407D;

	/* Call top.device_info_parse_data to parse the above table entry */
	val = g->ops.top.device_info_parse_data(g, table_entry, &inst_id,
						&pri_base, &fault_id);

	/* Verify if the retval is as expected */
	if (val != -EINVAL) {
		unit_err(m,
			"device_info_parse_data failed to parse data type.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_get_num_lce(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set num_lce_count (Bits 4:0) = 4 */
	nvgpu_posix_io_writel_reg_space(g, top_num_ces_r(), 0x4U);
	val = g->ops.top.get_num_lce(g);
	if (val != 4) {
		unit_err(m, "CE count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	/* Set num_lce_count (Bits 4:0) = 0x1D */
	nvgpu_posix_io_writel_reg_space(g, top_num_ces_r(), 0xE28A321DU);
	val = g->ops.top.get_num_lce(g);
	if (val != 0x1D) {
		unit_err(m, "CE count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

struct unit_module_test top_tests[] = {
	UNIT_TEST(top_setup,	              test_top_setup,          NULL, 0),
	UNIT_TEST(top_get_max_gpc_count,      test_get_max_gpc_count,  NULL, 0),
	UNIT_TEST(top_get_max_tpc_per_gpc_count,
				test_get_max_tpc_per_gpc_count,        NULL, 0),
	UNIT_TEST(top_get_max_fbps_count,     test_get_max_fbps_count, NULL, 0),
	UNIT_TEST(top_get_max_ltc_per_fbp,
				test_get_max_ltc_per_fbp,	       NULL, 0),
	UNIT_TEST(top_get_max_lts_per_ltc,
				test_get_max_lts_per_ltc,	       NULL, 0),
	UNIT_TEST(top_get_num_ltcs,           test_get_num_ltcs,       NULL, 0),
	UNIT_TEST(top_device_info_parse_data,
				test_device_info_parse_data,           NULL, 0),
	UNIT_TEST(top_get_num_lce,            test_get_num_lce,        NULL, 0),
	UNIT_TEST(top_free_reg_space,         test_top_free_reg_space, NULL, 0),
};

UNIT_MODULE(top, top_tests, UNIT_PRIO_NVGPU_TEST);
