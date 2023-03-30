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


#include <stdlib.h>

#include <unit/unit.h>
#include <unit/io.h>
#include <unit/utils.h>

#include <nvgpu/posix/io.h>
#include <nvgpu/posix/kmem.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/gr/config.h>

#include <nvgpu/hw/gv11b/hw_proj_gv11b.h>

#include "common/gr/gr_config_priv.h"

#include "../nvgpu-gr.h"
#include "nvgpu-gr-config.h"

static struct nvgpu_gr_config *unit_gr_config;

struct gr_gops_config_orgs {
	u32 (*get_litter_value)(struct gk20a *g,
		int value);
	u32 (*priv_ring_get_gpc_count)(struct gk20a *g);
	u32 (*get_pes_tpc_mask)(struct gk20a *g,
		struct nvgpu_gr_config *gr_config, u32 gpc, u32 tpc);
};

struct gr_config_litvalues {
	u32 pes_per_num;
	bool sm_per_num;
	u32 pes_tpc_mask;
};

struct gr_config_litvalues gr_test_config_lits;
struct gr_gops_config_orgs gr_test_config_gops;

static void gr_test_config_save_gops(struct gk20a *g)
{
	gr_test_config_gops.get_litter_value =
		g->ops.get_litter_value;
	gr_test_config_gops.priv_ring_get_gpc_count =
		g->ops.priv_ring.get_gpc_count;
	gr_test_config_gops.get_pes_tpc_mask =
		g->ops.gr.config.get_pes_tpc_mask;
}

static void gr_test_config_restore_gops(struct gk20a *g)
{
	g->ops.get_litter_value =
		gr_test_config_gops.get_litter_value;
	g->ops.priv_ring.get_gpc_count =
		gr_test_config_gops.priv_ring_get_gpc_count;
	g->ops.gr.config.get_pes_tpc_mask =
		gr_test_config_gops.get_pes_tpc_mask;
}

static u32 gr_test_config_get_pes_tpc_mask(struct gk20a *g,
		struct nvgpu_gr_config *gr_config, u32 gpc, u32 tpc)
{
	gr_test_config_lits.pes_tpc_mask += 1;
	if (gr_test_config_lits.pes_tpc_mask == 2) {
		return 0x1F;
	}else if (gr_test_config_lits.pes_tpc_mask == 3) {
		return 0x2F;
	}else if (gr_test_config_lits.pes_tpc_mask > 3) {
		return 0xF;
	}else {
		return 0;
	}
}

static u32 gr_test_config_priv_ring_get_gpc_count(struct gk20a *g)
{
	return 0;
}

static u32 gr_test_config_litter_value(struct gk20a *g, int value)
{
	u32 val = 0;
	switch (value) {
	case GPU_LIT_NUM_PES_PER_GPC:
		if (gr_test_config_lits.pes_per_num >= 3) {
			val = proj_scal_litter_num_pes_per_gpc_v();
		} else if (gr_test_config_lits.pes_per_num >= 1) {
			val = 1;
		} else {
			val = GK20A_GR_MAX_PES_PER_GPC + 1;
		}
		gr_test_config_lits.pes_per_num += 1;
		break;
	case GPU_LIT_NUM_SM_PER_TPC:
		if (gr_test_config_lits.sm_per_num) {
			val = proj_scal_litter_num_sm_per_tpc_v();
		} else {
			val = 0; /* Set zero sm per tpc */
		}
		gr_test_config_lits.sm_per_num = true;
		break;
	}

	return val;
}

int test_gr_config_init(struct unit_module *m,
		struct gk20a *g, void *args)
{
	unit_gr_config = nvgpu_gr_config_init(g);
	if (unit_gr_config == NULL) {
		unit_return_fail(m, "nvgpu_gr_config_init returned fail\n");
	}

	return UNIT_SUCCESS;
}

int test_gr_config_deinit(struct unit_module *m,
		struct gk20a *g, void *args)
{
	if (unit_gr_config != NULL) {
		nvgpu_gr_config_deinit(g, unit_gr_config);
		return UNIT_SUCCESS;
	}

	return UNIT_FAIL;
}

int test_gr_config_count(struct unit_module *m,
		struct gk20a *g, void *args)
{
	u32 val = 0U;
	u32 *reg_base = NULL;
	u32 gindex = 0U, pindex = 0U;
	u32 pes_tpc_val = 0U;
	u32 pes_tpc_count[GK20A_GR_MAX_PES_PER_GPC] = {0x2, 0x2, 0x0};
	u32 pes_tpc_mask[GK20A_GR_MAX_PES_PER_GPC] = {0x5, 0xa, 0x0};
	u32 gpc_tpc_mask = 0xf;
	u32 gpc_skip_mask = 0x0;
	u32 gpc_tpc_count = 0x4;
	u32 gpc_ppc_count = 0x2;

	struct nvgpu_gr_config gv11b_gr_config = {
		.g = NULL,
		.max_gpc_count = 0x1,
		.max_tpc_per_gpc_count = 0x4,
		.max_tpc_count = 0x4,
		.gpc_count = 0x1,
		.tpc_count = 0x4,
		.ppc_count = 0x2,
		.pe_count_per_gpc = 0x2,
		.sm_count_per_tpc = 0x2,
		.gpc_ppc_count = &gpc_ppc_count,
		.gpc_tpc_count = &gpc_tpc_count,
		.pes_tpc_count = {NULL, NULL, NULL},
		.gpc_mask = 0x1,
		.gpc_tpc_mask = &gpc_tpc_mask,
		.pes_tpc_mask = {NULL, NULL, NULL},
		.gpc_skip_mask = &gpc_skip_mask,
		.no_of_sm = 0x0,
		.sm_to_cluster = NULL,
	};

	gv11b_gr_config.pes_tpc_mask[0] = &pes_tpc_mask[0];
	gv11b_gr_config.pes_tpc_mask[1] = &pes_tpc_mask[1];
	gv11b_gr_config.pes_tpc_count[0] = &pes_tpc_count[0];
	gv11b_gr_config.pes_tpc_count[1] = &pes_tpc_count[1];

	/*
	 * Compare the config registers value against
	 * gv11b silicon following poweron
	 */

	val = nvgpu_gr_config_get_max_gpc_count(unit_gr_config);
	if (val != gv11b_gr_config.max_gpc_count) {
		unit_return_fail(m, "mismatch in max_gpc_count\n");
	}

	val = nvgpu_gr_config_get_max_tpc_count(unit_gr_config);
	if (val != gv11b_gr_config.max_tpc_count) {
		unit_return_fail(m, "mismatch in max_tpc_count\n");
	}

	val = nvgpu_gr_config_get_max_tpc_per_gpc_count(unit_gr_config);
	if (val != gv11b_gr_config.max_tpc_per_gpc_count) {
		unit_return_fail(m, "mismatch in max_tpc_per_gpc_count\n");
	}

	val = nvgpu_gr_config_get_gpc_count(unit_gr_config);
	if (val != gv11b_gr_config.gpc_count) {
		unit_return_fail(m, "mismatch in gpc_count\n");
	}

	val = nvgpu_gr_config_get_tpc_count(unit_gr_config);
	if (val != gv11b_gr_config.tpc_count) {
		unit_return_fail(m, "mismatch in tpc_count\n");
	}

	val = nvgpu_gr_config_get_ppc_count(unit_gr_config);
	if (val != gv11b_gr_config.ppc_count) {
		unit_return_fail(m, "mismatch in ppc_count\n");
	}

	val = nvgpu_gr_config_get_pe_count_per_gpc(unit_gr_config);
	if (val != gv11b_gr_config.pe_count_per_gpc) {
		unit_err(m, "mismatch in pe_count_per_gpc\n");
	}

	val = nvgpu_gr_config_get_sm_count_per_tpc(unit_gr_config);
	if (val != gv11b_gr_config.sm_count_per_tpc) {
		unit_err(m, "mismatch in sm_count_per_tpc\n");
	}

	val = nvgpu_gr_config_get_gpc_mask(unit_gr_config);
	if (val != gv11b_gr_config.gpc_mask) {
		unit_return_fail(m, "mismatch in gpc_mask\n");
	}

	for (gindex = 0U; gindex < gv11b_gr_config.gpc_count;
							gindex++) {
		val = nvgpu_gr_config_get_gpc_ppc_count(unit_gr_config,
							gindex);
		if (val != gv11b_gr_config.gpc_ppc_count[gindex]) {
			unit_return_fail(m, "mismatch in gpc_ppc_count\n");
		}

		val = nvgpu_gr_config_get_gpc_skip_mask(unit_gr_config,
							gindex);
		if (val != gv11b_gr_config.gpc_skip_mask[gindex]) {
			unit_return_fail(m, "mismatch in gpc_skip_mask\n");
		}

		val = nvgpu_gr_config_get_gpc_tpc_count(unit_gr_config,
							gindex);
		if (val != gv11b_gr_config.gpc_tpc_count[gindex]) {
			unit_return_fail(m, "mismatch in gpc_tpc_count\n");
		}

		for (pindex = 0U; pindex < gv11b_gr_config.gpc_count;
							pindex++) {
			pes_tpc_val =
				gv11b_gr_config.pes_tpc_count[pindex][gindex];
			val = nvgpu_gr_config_get_pes_tpc_count(
					unit_gr_config, gindex, pindex);
			if (val != pes_tpc_val) {
				unit_return_fail(m,
					"mismatch in pes_tpc_count\n");
			}

			pes_tpc_val =
				gv11b_gr_config.pes_tpc_mask[pindex][gindex];
			val = nvgpu_gr_config_get_pes_tpc_mask(
					unit_gr_config, gindex, pindex);
			if (val != pes_tpc_val) {
				unit_return_fail(m,
					"mismatch in pes_tpc_count\n");
			}
		}
	}

	/*
	 * Check for valid memory
	 */
	reg_base = nvgpu_gr_config_get_base_mask_gpc_tpc(unit_gr_config);
	if (reg_base == NULL) {
		unit_return_fail(m, "Invalid gpc_tpc_mask_base\n");
	}

	reg_base = nvgpu_gr_config_get_base_count_gpc_tpc(unit_gr_config);
	if (reg_base == NULL) {
		unit_return_fail(m, "Invalid gpc_tpc_count_base\n");
	}

	return UNIT_SUCCESS;
}

int test_gr_config_set_get(struct unit_module *m,
		struct gk20a *g, void *args)
{
	u32 gindex = 0U;
	u32 val = 0U;
	struct nvgpu_sm_info *sm_info;
	u32 num_sm =  nvgpu_gr_config_get_tpc_count(unit_gr_config) *
                 nvgpu_gr_config_get_sm_count_per_tpc(unit_gr_config);
	u32 i, j, states, sm_id, sm_id_range, sm_range_difference;
	u32 valid_sm_ids[][2] = {{0, num_sm-1}};
	u32 invalid_sm_ids[][2] = {{num_sm, U32_MAX}};
	u32 (*working_list)[2];
	const char *string_cases[] = {"Valid", "Invalid"};
	const char *string_states[] = {"Min", "Max", "Random Mid"};

	srand(0);

	num_sm = nvgpu_gr_config_get_tpc_count(unit_gr_config) *
		 nvgpu_gr_config_get_sm_count_per_tpc(unit_gr_config);
	nvgpu_gr_config_set_no_of_sm(unit_gr_config, num_sm);
	if (num_sm != nvgpu_gr_config_get_no_of_sm(unit_gr_config)) {
		unit_return_fail(m, "mismatch in no_of_sm\n");
	}

	/*
	 * i is to loop through valid and invalid cases
	 * j is to loop through different ranges within ith case
	 * states is for min, max and median
	 */

	/* loop through valid and invalid cases */
	for (i = 0; i < 2; i++) {
		/* select appropriate iteration size */
		sm_id_range = (i == 0) ? ARRAY_SIZE(valid_sm_ids) : ARRAY_SIZE(invalid_sm_ids);
		/* select correct working list */
		working_list =  (i == 0) ? valid_sm_ids : invalid_sm_ids;
		for (j = 0; j < sm_id_range; j++) {
			for (states = 0; states < 3; states++) {
				/* check for min sm id */
				if (states == 0)
					sm_id = working_list[j][0];
				else if (states == 1) {
					/* check for max valid sm id */
					sm_id = working_list[j][1];
				} else {
					sm_range_difference = working_list[j][1] - working_list[j][0];
					/* Check for random sm id in range */
					if (sm_range_difference > 1)
						sm_id = get_random_u32(working_list[j][0] + 1, working_list[j][1] - 1);
					else
						continue;
				}

				unit_info(m, "BVEC testing for nvgpu_gr_config_get_sm_info with sm id = %u(%s range %s) done\n", sm_id, string_cases[i], string_states[states]);
				sm_info = nvgpu_gr_config_get_sm_info(unit_gr_config, sm_id);
				if (i == 0) {
					if (sm_info == NULL)
						unit_return_fail(m, "SM_id valid range check failed.\n");
				} else {
					if (sm_info != NULL)
						unit_return_fail(m, "SM_id invalid range check failed.\n");
				}
			}
		}
	}

	/*
	 * Set random value and read back
	 */
	sm_info = nvgpu_gr_config_get_sm_info(unit_gr_config, 0);
	val = (u32)rand();
	nvgpu_gr_config_set_sm_info_gpc_index(sm_info, val);
	if (val != nvgpu_gr_config_get_sm_info_gpc_index(sm_info)) {
		unit_return_fail(m, "mismatch in sm_info_gindex\n");
	}

	val = (u32)rand();
	nvgpu_gr_config_set_sm_info_tpc_index(sm_info, val);
	if (val != nvgpu_gr_config_get_sm_info_tpc_index(sm_info)) {
		unit_return_fail(m, "mismatch in sm_info_tpc_index\n");
	}

	val = (u32)rand();
	nvgpu_gr_config_set_sm_info_global_tpc_index(sm_info, val);
	if (val !=
		nvgpu_gr_config_get_sm_info_global_tpc_index(sm_info)) {
		unit_return_fail(m, "mismatch in sm_info_global_tpc_index\n");
	}

	val = (u32)rand();
	nvgpu_gr_config_set_sm_info_sm_index(sm_info, val);
	if (val != nvgpu_gr_config_get_sm_info_sm_index(sm_info)) {
		unit_return_fail(m, "mismatch in sm_info_sm_index\n");
	}

	for (gindex = 0U; gindex < unit_gr_config->gpc_count;
							gindex++) {
		val = (u32)rand();
		nvgpu_gr_config_set_gpc_tpc_mask(unit_gr_config, gindex, val);
		if (val !=
		    nvgpu_gr_config_get_gpc_tpc_mask(unit_gr_config, gindex)) {
			unit_return_fail(m, "mismatch in gpc_tpc_mask\n");
		}
	}

	return UNIT_SUCCESS;
}

static int gr_test_invalid_gpc_count(struct gk20a *g)
{
	struct nvgpu_gr_config *gr_conf;

	gr_test_config_restore_gops(g);
	g->ops.priv_ring.get_gpc_count =
		gr_test_config_priv_ring_get_gpc_count;
	gr_conf = nvgpu_gr_config_init(g);
	g->ops.priv_ring.get_gpc_count =
		gr_test_config_gops.priv_ring_get_gpc_count;
	if (gr_conf != NULL) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

static int gr_test_diff_pes_tpc_mask(struct gk20a *g)
{
	struct nvgpu_gr_config *gr_conf;

	gr_test_config_restore_gops(g);
	g->ops.gr.config.get_pes_tpc_mask =
		gr_test_config_get_pes_tpc_mask;
	gr_conf = nvgpu_gr_config_init(g);
	if (gr_conf == NULL) {
		return UNIT_FAIL;
	}
	nvgpu_gr_config_deinit(g, gr_conf);
	return UNIT_SUCCESS;
}

static int gr_test_invalid_litter_values(struct gk20a *g)
{
	struct nvgpu_gr_config *gr_conf;
	int i;

	gr_test_config_restore_gops(g);
	g->ops.get_litter_value = gr_test_config_litter_value;

	for ( i = 0; i < 2; i++) {
		gr_conf = nvgpu_gr_config_init(g);
		if (gr_conf != NULL) {
			return UNIT_FAIL;
		}
	}
	return UNIT_SUCCESS;
}

static int gr_test_diff_gpc_skip_mask(struct gk20a *g)
{
	struct nvgpu_gr_config *gr_conf;
	int i, err;

	gr_test_config_restore_gops(g);
	g->ops.get_litter_value = gr_test_config_litter_value;

	gr_conf = nvgpu_gr_config_init(g);
	if (gr_conf == NULL) {
		return UNIT_FAIL;
	}
	nvgpu_gr_config_deinit(g, gr_conf);

	for (i = 0; i < 2; i++) {
		err = gr_test_diff_pes_tpc_mask(g);
		if (err != 0) {
			return UNIT_FAIL;
		}
	}

	return UNIT_SUCCESS;
}

static int gr_test_invalid_pes_with_sm_id(struct gk20a *g,
				struct nvgpu_gr_config *gr_conf)
{
	int err;

	/* Set pes tpc mask same */
	u32 pes_tpc_mask = gr_conf->pes_tpc_mask[1][0];

	gr_conf->pes_tpc_mask[1][0] = gr_conf->pes_tpc_mask[0][0];
	err = g->ops.gr.config.init_sm_id_table(g, gr_conf);

	gr_conf->pes_tpc_mask[1][0] = pes_tpc_mask;

	return err;
}

int test_gr_config_error_injection(struct unit_module *m,
		struct gk20a *g, void *args)
{

	int err, i;
	struct nvgpu_gr_config *gr_conf;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	gr_test_config_save_gops(g);
	memset(&gr_test_config_lits, 0,
		sizeof(struct gr_config_litvalues));

	/* Fail gr_config struct mem allocation */
	for (i = 0; i < 9; i++) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, i);
		gr_conf = nvgpu_gr_config_init(g);
		if (gr_conf != NULL) {
			unit_return_fail(m,
				"nvgpu_gr_config_init alloc test failed\n");
		}
		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	}

	/* Fail with zero gpc_count */
	err = gr_test_invalid_gpc_count(g);
	if (err != 0) {
		unit_return_fail(m,
			"gr_test_invalid_gpc_count test failed\n");
	}

	/* Fail with wrong config litter values */
	err = gr_test_invalid_litter_values(g);
	if (err != 0) {
		unit_return_fail(m,
			"gr_test_invalid_gpc_count test failed\n");
	}

	/* Pass with diff gpc_skip_mask */
	err = gr_test_diff_gpc_skip_mask(g);
	if (err != 0) {
		unit_return_fail(m,
			"gr_test_invalid_pes_tpc_mask test failed\n");
	}

	/*
	 * Pass with diff pes_tpc_mask.
	 * Run this after gr_test_diff_gpc_skip_mask() so that this
	 * test receives appropriate pes_tpc_mask from
	 * gr_test_config_get_pes_tpc_mask().
	 */
	err = gr_test_diff_pes_tpc_mask(g);
	if (err != 0) {
		unit_return_fail(m,
			"gr_test_invalid_pes_tpc_mask test failed\n");
	}

	gr_test_config_restore_gops(g);

	gr_conf = nvgpu_gr_config_init(g);

	/* Fail sm_id table mem allocation */
	for (i = 0; i < 5; i++) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, i);
		err = g->ops.gr.config.init_sm_id_table(g, gr_conf);
		if (err == 0) {
			unit_return_fail(m,
				"init_sm_id_table alloc failed\n");
		}
		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	}

	/* Fail sm_id table with wrong pes mask value */
	err = gr_test_invalid_pes_with_sm_id(g, gr_conf);
	if (err == 0) {
		unit_return_fail(m,
			"gr_test_invalid_pes_with_sm_id test failed\n");
	}

	nvgpu_gr_config_deinit(g, gr_conf);

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_gr_config_tests[] = {
	UNIT_TEST(gr_init_setup, test_gr_init_setup, NULL, 0),
	UNIT_TEST(config_init, test_gr_config_init, NULL, 0),
	UNIT_TEST(config_check_init, test_gr_config_count, NULL, 0),
	UNIT_TEST(config_check_set_get, test_gr_config_set_get, NULL, 0),
	UNIT_TEST(config_error_injection,
			test_gr_config_error_injection, NULL, 0),
	UNIT_TEST(config_deinit, test_gr_config_deinit, NULL, 0),
	UNIT_TEST(gr_remove_setup, test_gr_remove_setup, NULL, 0),
};

UNIT_MODULE(nvgpu_gr_config, nvgpu_gr_config_tests, UNIT_PRIO_NVGPU_TEST);
