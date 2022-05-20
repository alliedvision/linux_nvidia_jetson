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

#include <nvgpu/posix/io.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <os/posix/os_posix.h>

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/netlist.h>
#include <nvgpu/engines.h>
#include <nvgpu/pbdma.h>
#include "common/gr/gr_priv.h"
#include "common/gr/gr_config_priv.h"
#include "common/netlist/netlist_priv.h"

#include "../nvgpu-gr.h"
#include "nvgpu-gr-init-hal-gv11b.h"

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>
#include <nvgpu/hw/gv11b/hw_fifo_gv11b.h>
#include <nvgpu/hw/gv11b/hw_top_gv11b.h>

#define DUMMY_SIZE	0xF0U

static int dummy_l2_flush(struct gk20a *g, bool invalidate)
{
	return 0;
}

struct gr_ecc_scrub_reg_rec {
	u32 addr;
	u32 scrub_done;
};

struct gr_ecc_scrub_reg_rec ecc_scrub_data[] = {
	{
		.addr = gr_pri_gpc0_tpc0_sm_lrf_ecc_control_r(),
		.scrub_done =
			(gr_pri_gpc0_tpc0_sm_lrf_ecc_control_scrub_qrfdp0_init_f() |
			gr_pri_gpc0_tpc0_sm_lrf_ecc_control_scrub_qrfdp1_init_f() |
			gr_pri_gpc0_tpc0_sm_lrf_ecc_control_scrub_qrfdp2_init_f() |
			gr_pri_gpc0_tpc0_sm_lrf_ecc_control_scrub_qrfdp3_init_f() |
			gr_pri_gpc0_tpc0_sm_lrf_ecc_control_scrub_qrfdp4_init_f() |
			gr_pri_gpc0_tpc0_sm_lrf_ecc_control_scrub_qrfdp5_init_f() |
			gr_pri_gpc0_tpc0_sm_lrf_ecc_control_scrub_qrfdp6_init_f() |
			gr_pri_gpc0_tpc0_sm_lrf_ecc_control_scrub_qrfdp7_init_f()),
	},
	{
		.addr = gr_pri_gpc0_tpc0_sm_l1_data_ecc_control_r(),
		.scrub_done =
			(gr_pri_gpc0_tpc0_sm_l1_data_ecc_control_scrub_el1_0_init_f() |
			gr_pri_gpc0_tpc0_sm_l1_data_ecc_control_scrub_el1_1_init_f()),
	},
	{
		.addr = gr_pri_gpc0_tpc0_sm_l1_tag_ecc_control_r(),
		.scrub_done =
			 (gr_pri_gpc0_tpc0_sm_l1_tag_ecc_control_scrub_el1_0_init_f() |
			  gr_pri_gpc0_tpc0_sm_l1_tag_ecc_control_scrub_el1_1_init_f() |
			  gr_pri_gpc0_tpc0_sm_l1_tag_ecc_control_scrub_pixprf_init_f() |
			  gr_pri_gpc0_tpc0_sm_l1_tag_ecc_control_scrub_miss_fifo_init_f()),
	},
	{
		.addr = gr_pri_gpc0_tpc0_sm_cbu_ecc_control_r(),
		.scrub_done =
			 (gr_pri_gpc0_tpc0_sm_cbu_ecc_control_scrub_warp_sm0_init_f() |
			  gr_pri_gpc0_tpc0_sm_cbu_ecc_control_scrub_warp_sm1_init_f() |
			  gr_pri_gpc0_tpc0_sm_cbu_ecc_control_scrub_barrier_sm0_init_f() |
			  gr_pri_gpc0_tpc0_sm_cbu_ecc_control_scrub_barrier_sm1_init_f()),
	},
	{
		.addr = gr_pri_gpc0_tpc0_sm_icache_ecc_control_r(),
		.scrub_done =
			 (gr_pri_gpc0_tpc0_sm_icache_ecc_control_scrub_l0_data_init_f() |
			  gr_pri_gpc0_tpc0_sm_icache_ecc_control_scrub_l0_predecode_init_f() |
			  gr_pri_gpc0_tpc0_sm_icache_ecc_control_scrub_l1_data_init_f() |
			  gr_pri_gpc0_tpc0_sm_icache_ecc_control_scrub_l1_predecode_init_f()),
	},
};

int test_gr_init_hal_ecc_scrub_reg(struct unit_module *m,
		struct gk20a *g, void *args)
{
	u32 i;
	int err;
	struct nvgpu_gr_config *config = nvgpu_gr_get_config_ptr(g);

	/* Code coverage */
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_ICACHE, false);
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_CBU, false);
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_L1_TAG, false);
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_L1_DATA, false);
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_LRF, false);

	err = g->ops.gr.init.ecc_scrub_reg(g, config);
	if (err != 0) {
		unit_return_fail(m, "ECC scrub failed");
	}

	/* Re-enable the features */
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_ICACHE, true);
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_CBU, true);
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_L1_TAG, true);
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_L1_DATA, true);
	nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_LRF, true);

	for (i = 0;
	     i < (sizeof(ecc_scrub_data) / sizeof(struct gr_ecc_scrub_reg_rec));
	     i++) {
		/* Set incorrect values of scrub_done so that scrub wait times out */
		nvgpu_writel(g,
			ecc_scrub_data[i].addr,
			~(ecc_scrub_data[i].scrub_done));

		err = g->ops.gr.init.ecc_scrub_reg(g, config);
		if (err == 0) {
			unit_return_fail(m, "Timeout was expected");
		}

		/* Set correct values of scrub_done so that scrub wait is successful */
		nvgpu_writel(g,
			ecc_scrub_data[i].addr,
			ecc_scrub_data[i].scrub_done);
	}

	/* No error injection, should be successful */
	err = g->ops.gr.init.ecc_scrub_reg(g, config);
	if (err != 0) {
		unit_return_fail(m, "ECC scrub failed");
	}

	return UNIT_SUCCESS;
}

int test_gr_init_hal_wait_empty(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err;
	int i;

	/* gr_status is non-zero, gr_activity are zero, expect failure */
	nvgpu_writel(g, gr_status_r(), BIT32(7));
	nvgpu_writel(g, gr_activity_0_r(), 0);
	nvgpu_writel(g, gr_activity_1_r(), 0);
	nvgpu_writel(g, gr_activity_2_r(), 0);
	nvgpu_writel(g, gr_activity_4_r(), 0);

	err = g->ops.gr.init.wait_empty(g);
	if (err == 0) {
		return UNIT_FAIL;
	}

	/* gr_status is non-zero, gr_activity are non-zero, expect failure */
	nvgpu_writel(g, gr_status_r(), BIT32(7));
	nvgpu_writel(g, gr_activity_0_r(), 0x4);
	nvgpu_writel(g, gr_activity_1_r(), 0x4);
	nvgpu_writel(g, gr_activity_2_r(), 0x4);
	nvgpu_writel(g, gr_activity_4_r(), 0x4);

	err = g->ops.gr.init.wait_empty(g);
	if (err == 0) {
		return UNIT_FAIL;
	}

	/* gr_status is zero, gr_activity are non-zero, expect failure */
	nvgpu_writel(g, gr_status_r(), 0);
	for (i = 1; i < 16; i++) {
		if (i & 0x1) {
			nvgpu_writel(g, gr_activity_0_r(), 0x2);
		} else {
			nvgpu_writel(g, gr_activity_0_r(), 0x104);
		}
		if (i & 0x2) {
			nvgpu_writel(g, gr_activity_1_r(), 0x2);
		} else {
			nvgpu_writel(g, gr_activity_1_r(), 0x104);
		}
		if (i & 0x4) {
			nvgpu_writel(g, gr_activity_2_r(), 0x2);
		} else {
			nvgpu_writel(g, gr_activity_2_r(), 0x0);
		}
		if (i & 0x8) {
			nvgpu_writel(g, gr_activity_4_r(), 0x2);
		} else {
			nvgpu_writel(g, gr_activity_4_r(), 0x104);
		}

		err = g->ops.gr.init.wait_empty(g);
		if (err == 0) {
			return UNIT_FAIL;
		}
	}

	/* Both gr_status and gr_activity registers are zero, expect success */
	nvgpu_writel(g, gr_status_r(), 0);
	nvgpu_writel(g, gr_activity_0_r(), 0);
	nvgpu_writel(g, gr_activity_1_r(), 0);
	nvgpu_writel(g, gr_activity_2_r(), 0);
	nvgpu_writel(g, gr_activity_4_r(), 0);

	err = g->ops.gr.init.wait_empty(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_gr_init_hal_wait_idle(struct unit_module *m,
		struct gk20a *g, void *args)
{
	u32 i;
	u32 val;
	int err;
	bool expected_pass;
	u32 entry_count;
	struct nvgpu_fifo *f = &g->fifo;

	/* Configure GR engine in DEVICE_INFO registers */
	entry_count = top_device_info__size_1_v();
	for (i = 0; i < entry_count ; i++) {
		nvgpu_writel(g, top_device_info_r(i), 0);
	}

	nvgpu_writel(g, top_device_info_r(1), 0x8006183E);
	nvgpu_writel(g, top_device_info_r(2), 0x80000105);
	nvgpu_writel(g, top_device_info_r(3), 0x00000003);

	/* Fifo is uninitialized, so need to set this */
	f->g = g;

	/*
	 * PBDMA and ENGINE data should be initialized to detect
	 * GR engine ID in g->ops.gr.init.wait_idle.
	 */
	err = nvgpu_pbdma_setup_sw(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	err = nvgpu_engine_setup_sw(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	/*
	 * Set combinations of gr/fifo status registers.
	 * g->ops.gr.init.wait_idle will timeout only when context is valid
	 * and either on GR engine or during ctxsw operation.
	 * That means timeout is triggered only three times as below -
	 * - Ctx status is valid, GR engine is busy, ctxsw not in progress.
	 * - Ctx status is valid, GR engine is not busy, ctxsw in progress.
	 * - Ctx status is valid, GR engine is busy, ctxsw in progress.
	 * - In all other cases wait will pass.
	 */
	for (i = 1; i < 8; i++) {
		if ((i & 0x1)) {
			/* GR status reports busy. */
			nvgpu_writel(g, gr_engine_status_r(), 0x1);
		} else {
			/* GR status reports idle. */
			nvgpu_writel(g, gr_engine_status_r(), 0x0);
		}

		nvgpu_writel(g, fifo_engine_status_r(0), 0x0);

		if ((i & 0x2)) {
			/* Set ctx status to invalid. */
		} else {
			/* Set ctx status to valid. */
			val = nvgpu_readl(g, fifo_engine_status_r(0));
			val |= BIT32(13U);
			nvgpu_writel(g, fifo_engine_status_r(0), val);
		}

		if ((i & 0x4)) {
			/* Set ctxsw status to in progress. */
			val = nvgpu_readl(g, fifo_engine_status_r(0));
			val |= BIT32(15U);
			nvgpu_writel(g, fifo_engine_status_r(0), val);
		} else {
			/* Set ctxsw status to not in progress. */
		}

		/*
		 * This condition statement mimicks the timeout check
		 * statement in gm20b_gr_init_wait_idle().
		 */
		if ((i & 0x2) || (!(i & 0x1) && !(i & 0x4))) {
			expected_pass = true;
		} else {
			expected_pass = false;
		}

		err = g->ops.gr.init.wait_idle(g);
		if (err == -EAGAIN && expected_pass) {
			return UNIT_FAIL;
		}
		if (err == 0 && !expected_pass) {
			return UNIT_FAIL;
		}
	}

	/* Set all status registers to idle/inactive */
	nvgpu_writel(g, gr_engine_status_r(), 0x0);
	nvgpu_writel(g, fifo_engine_status_r(0), 0x0);

	/* Success */
	err = g->ops.gr.init.wait_idle(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	/* Cleanup */
	nvgpu_pbdma_cleanup_sw(g);
	nvgpu_engine_cleanup_sw(g);

	/*
	 * Need to set explicitly to avoid looping through
	 * engine enums in suspend test.
	 */
	f->num_engines = 0;

	return UNIT_SUCCESS;
}

int test_gr_init_hal_wait_fe_idle(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err;

	/* Set FE status active */
	nvgpu_writel(g, gr_status_r(), BIT32(2U));

	/* Should fail */
	err = g->ops.gr.init.wait_fe_idle(g);
	if (err != -EAGAIN) {
		return UNIT_FAIL;
	}

	/* Success */
	nvgpu_writel(g, gr_status_r(), 0);
	err = g->ops.gr.init.wait_fe_idle(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_gr_init_hal_fe_pwr_mode(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_posix_fault_inj *readl_fi =
		nvgpu_readl_get_fault_injection();

	/* Trigger timeout by default */
	err = g->ops.gr.init.fe_pwr_mode_force_on(g, true);
	if (err != -ETIMEDOUT) {
		return UNIT_FAIL;
	}

	/* Inject readl error so that timeout is not hit */
	nvgpu_posix_enable_fault_injection(readl_fi, true, 0);
	err = g->ops.gr.init.fe_pwr_mode_force_on(g, true);
	if (err != 0) {
		return UNIT_FAIL;
	}

	/* Call with flag set to false, should pass */
	err = g->ops.gr.init.fe_pwr_mode_force_on(g, false);
	if (err != 0) {
		return UNIT_FAIL;
	}

	nvgpu_posix_enable_fault_injection(readl_fi, false, 0);

	return UNIT_SUCCESS;
}

static u32 gr_get_max_u32(struct gk20a *g)
{
	return 0xFFFFFFFF;
}

static int test_gr_init_hal_get_nonpes_aware_tpc(struct gk20a *g)
{
	u32 val_bk;
	struct nvgpu_gr_config *config = nvgpu_gr_get_config_ptr(g);

	/* Set gpc_ppc_count to 0 for code coverage */
	val_bk = config->gpc_ppc_count[0];
	config->gpc_ppc_count[0] = 0;

	/*
	 * gpc_ppc_count can never be 0 so we are not interested
	 * in checking return value.
	 */
	g->ops.gr.init.get_nonpes_aware_tpc(g, 0, 0, config);
	config->gpc_ppc_count[0] = val_bk;

	return UNIT_SUCCESS;
}

static int test_gr_init_hal_sm_id_config(struct gk20a *g)
{
	int err;
	u32 val_bk;
	u32 *tpc_sm_id;
	struct nvgpu_gr_config *config = nvgpu_gr_get_config_ptr(g);

	/* Set tpc_count = 2 and sm_count to 4 for code coverage */
        tpc_sm_id = nvgpu_kcalloc(g, g->ops.gr.init.get_sm_id_size(), sizeof(u32));
        if (tpc_sm_id == NULL) {
                return UNIT_FAIL;
        }

	val_bk = config->tpc_count;
	config->tpc_count = 2;
	config->no_of_sm = 4;

	err = g->ops.gr.init.sm_id_config(g, tpc_sm_id, config, NULL, false);
	if (err != 0) {
                return UNIT_FAIL;
	}

	/* Restore tpc_count and sm_count */
	config->tpc_count = val_bk;
	config->no_of_sm = val_bk * 2;
	nvgpu_kfree(g, tpc_sm_id);

	return UNIT_SUCCESS;
}

static int test_gr_init_hal_fs_state(struct gk20a *g)
{
	u32 val_bk;
	u32 reg_val;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	/*
	 * Trigger g->ops.gr.init.fs_state with combinations of
	 * is_soc_t194_a01 and gpu_arch.
	 */
	val_bk = g->params.gpu_arch;

	p->is_soc_t194_a01 = true;
	g->params.gpu_arch = 0;
	g->ops.gr.init.fs_state(g);

	/* Backup gr_scc_debug_r() value */
	reg_val = nvgpu_readl(g, gr_scc_debug_r());

	p->is_soc_t194_a01 = true;
	g->params.gpu_arch = val_bk;
	g->ops.gr.init.fs_state(g);

	/*
	 * gr_scc_debug_r() should be updated when SOC is A01 and
	 * GPU is GV11B.
	 */
	if (reg_val == nvgpu_readl(g, gr_scc_debug_r())) {
		return UNIT_FAIL;
	}

	p->is_soc_t194_a01 = false;
	g->params.gpu_arch = 0;
	g->ops.gr.init.fs_state(g);

	p->is_soc_t194_a01 = false;
	g->params.gpu_arch = val_bk;
	g->ops.gr.init.fs_state(g);

	return UNIT_SUCCESS;
}

static int test_gr_init_hal_get_cb_size(struct gk20a *g)
{
	u32 val;
	struct nvgpu_gr_config *config = nvgpu_gr_get_config_ptr(g);

	/* g->ops.gr.init.get_attrib_cb_size should return alternate value */
	g->ops.gr.init.get_attrib_cb_default_size = gr_get_max_u32;
	val = g->ops.gr.init.get_attrib_cb_size(g, config->tpc_count);
	if (val != (gr_gpc0_ppc0_cbm_beta_cb_size_v_f(~0) / config->tpc_count)) {
		return UNIT_FAIL;
	}

	/* g->ops.gr.init.get_alpha_cb_size should return alternate value */
	g->ops.gr.init.get_alpha_cb_default_size = gr_get_max_u32;
	val = g->ops.gr.init.get_alpha_cb_size(g, config->tpc_count);
	if (val != (gr_gpc0_ppc0_cbm_alpha_cb_size_v_f(~0) / config->tpc_count)) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

static int test_gr_init_hal_pd_skip_table_gpc(struct gk20a *g)
{
	u32 i;
	struct nvgpu_gr_config *config = nvgpu_gr_get_config_ptr(g);

	/*
	 * Set gpc_skip_mask and make sure register
	 * value is reflected in each loop
	 */
	for (i = 0; i < gr_pd_dist_skip_table__size_1_v(); i++) {
		if (i < nvgpu_gr_config_get_gpc_count(config)) {
			config->gpc_skip_mask[i] = 0x1;

			g->ops.gr.init.pd_skip_table_gpc(g, config);
			if (nvgpu_readl(g, gr_pd_dist_skip_table_r(i / 4)) == 0x0) {
				return UNIT_FAIL;
			}

			config->gpc_skip_mask[i] = 0x0;
		}
	}

	/* All skip_masks are unset in above loop already */
	g->ops.gr.init.pd_skip_table_gpc(g, config);

	/* This register should be 0 if all skip_masks are zero */
	if (nvgpu_readl(g, gr_pd_dist_skip_table_r(0)) != 0x0) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

static int test_gr_init_wait_idle_fail(struct gk20a *g)
{
	return -1;
}

static int test_gr_init_wait_idle_success(struct gk20a *g)
{
	return 0;
}

static int test_gr_init_hal_load_sw_veid_bundle(struct gk20a *g)
{
	int err;
	g->ops.gr.init.wait_idle = test_gr_init_wait_idle_fail;

	/* Should fail */
	err = g->ops.gr.init.load_sw_veid_bundle(g,
			&g->netlist_vars->sw_veid_bundle_init);
	if (err == 0) {
		return UNIT_FAIL;
	}

	g->ops.gr.init.wait_idle = test_gr_init_wait_idle_success;

	/* Should pass */
	err = g->ops.gr.init.load_sw_veid_bundle(g,
			&g->netlist_vars->sw_veid_bundle_init);
	if (err != 0) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

static int test_gr_init_hal_load_sw_bundle_init(struct gk20a *g)
{
	int err;
	g->ops.gr.init.wait_idle = test_gr_init_wait_idle_fail;

	/* Should fail */
	err = g->ops.gr.init.load_sw_bundle_init(g,
			&g->netlist_vars->sw_bundle_init);
	if (err == 0) {
		return UNIT_FAIL;
	}

	g->ops.gr.init.wait_idle = test_gr_init_wait_idle_success;

	/* Should pass */
	err = g->ops.gr.init.load_sw_bundle_init(g,
			&g->netlist_vars->sw_bundle_init);
	if (err != 0) {
		return UNIT_FAIL;
	}

	g->ops.gr.init.wait_fe_idle = test_gr_init_wait_idle_fail;

	/* Should fail */
	err = g->ops.gr.init.load_sw_bundle_init(g,
			&g->netlist_vars->sw_bundle_init);
	if (err == 0) {
		return UNIT_FAIL;
	}

	g->ops.gr.init.wait_fe_idle = test_gr_init_wait_idle_success;

	/* Should pass */
	err = g->ops.gr.init.load_sw_bundle_init(g,
			&g->netlist_vars->sw_bundle_init);
	if (err != 0) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

static int test_gr_init_hal_load_method_init(struct gk20a *g)
{
	u32 val;

	/* Set dummy value into the register */
	nvgpu_writel(g, gr_pri_mme_shadow_ram_data_r(), 0xDEADBEEF);

	/* Set count = 0, so that no write is performed */
	val = g->netlist_vars->sw_method_init.count;
	g->netlist_vars->sw_method_init.count = 0;

	g->ops.gr.init.load_method_init(g,
		&g->netlist_vars->sw_method_init);

	/* Ensure register was not written */
	if (nvgpu_readl(g, gr_pri_mme_shadow_ram_data_r()) != 0xDEADBEEF) {
		return UNIT_FAIL;
	}

	/* Restore the count */
	g->netlist_vars->sw_method_init.count = val;

	g->ops.gr.init.load_method_init(g,
		&g->netlist_vars->sw_method_init);

	/* Make sure register was written */
	if (nvgpu_readl(g, gr_pri_mme_shadow_ram_data_r()) == 0xDEADBEEF) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_gr_init_hal_config_error_injection(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	int ret = UNIT_SUCCESS;

	ret = test_gr_init_hal_get_nonpes_aware_tpc(g);
	if (ret != UNIT_SUCCESS) {
		goto fail;
	}

	ret = test_gr_init_hal_sm_id_config(g);
	if (ret != UNIT_SUCCESS) {
		goto fail;
	}

	ret = test_gr_init_hal_fs_state(g);
	if (ret != UNIT_SUCCESS) {
		goto fail;
	}

	ret = test_gr_init_hal_get_cb_size(g);
	if (ret != UNIT_SUCCESS) {
		goto fail;
	}

	ret = test_gr_init_hal_pd_skip_table_gpc(g);
	if (ret != UNIT_SUCCESS) {
		goto fail;
	}

	ret = test_gr_init_hal_load_sw_veid_bundle(g);
	if (ret != UNIT_SUCCESS) {
		goto fail;
	}

	ret = test_gr_init_hal_load_sw_bundle_init(g);
	if (ret != UNIT_SUCCESS) {
		goto fail;
	}

	ret = test_gr_init_hal_load_method_init(g);
	if (ret != UNIT_SUCCESS) {
		goto fail;
	}

fail:
	g->ops = gops;
	return ret;
}

static u32 test_gr_get_min_gpm_fifo_depth(struct gk20a *g)
{
	return 0;
}

int test_gr_init_hal_error_injection(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err;
	struct vm_gk20a *vm;
	struct nvgpu_gr_ctx_desc *desc;
	struct nvgpu_gr_ctx *gr_ctx = NULL;
	u32 size;
	struct gpu_ops gops = g->ops;

	g->ops.mm.cache.l2_flush = dummy_l2_flush;

	vm = nvgpu_vm_init(g, SZ_4K, SZ_4K << 10,
		nvgpu_safe_sub_u64(1ULL << 37, SZ_4K << 10),
		(1ULL << 32), 0ULL,
		false, false, false, "dummy");
	if (!vm) {
		unit_return_fail(m, "failed to allocate VM");
	}

	/* Setup gr_ctx and patch_ctx */
	desc = nvgpu_gr_ctx_desc_alloc(g);
	if (!desc) {
		unit_return_fail(m, "failed to allocate memory");
	}

	gr_ctx = nvgpu_alloc_gr_ctx_struct(g);
	if (!gr_ctx) {
		unit_return_fail(m, "failed to allocate memory");
	}

	nvgpu_gr_ctx_set_size(desc, NVGPU_GR_CTX_CTX, DUMMY_SIZE);
	err = nvgpu_gr_ctx_alloc(g, gr_ctx, desc, vm);
	if (err != 0) {
		unit_return_fail(m, "failed to allocate context");
	}

	nvgpu_gr_ctx_set_size(desc, NVGPU_GR_CTX_PATCH_CTX, DUMMY_SIZE);
	err = nvgpu_gr_ctx_alloc_patch_ctx(g, gr_ctx, desc, vm);
	if (err != 0) {
		unit_return_fail(m, "failed to allocate patch context");
	}

	/* global_ctx = false and arbitrary size */
	g->ops.gr.init.commit_global_pagepool(g, gr_ctx, 0x12345678,
		DUMMY_SIZE, false, false);

	/* Verify correct size is set */
	size = nvgpu_readl(g, gr_scc_pagepool_r());
	if ((size & 0x3FF) != DUMMY_SIZE) {
		unit_return_fail(m, "expected size not set");
	}

	/*
	 * Trigger these HALs with tpc_count = 0, only for code coverage.
	 * We are not interested in return values since tpc_count cannot be
	 * 0 in any practical case.
	 */
	EXPECT_BUG(g->ops.gr.init.get_attrib_cb_size(g, 0));
	EXPECT_BUG(g->ops.gr.init.get_alpha_cb_size(g, 0));

	/*
	 * Make g->ops.gr.init.get_min_gpm_fifo_depth return zero, so that
	 * we choose data as 0 in gp10b_gr_init_commit_global_bundle_cb()
	 * and program it.
	 * Ensure that 0 was programmed in corresponding field in
	 * register gr_pd_ab_dist_cfg2_r() by reading it back.
	 */
	g->ops.gr.init.get_min_gpm_fifo_depth = test_gr_get_min_gpm_fifo_depth;
	g->ops.gr.init.commit_global_bundle_cb(g, gr_ctx, 0xffff, 0xffff, false);
	if (nvgpu_readl(g, gr_pd_ab_dist_cfg2_r()) !=
			g->ops.gr.init.get_bundle_cb_token_limit(g)) {
		unit_return_fail(m, "expected value not set");
	}

	g->ops = gops;

	/* cleanup */
	nvgpu_gr_ctx_free_patch_ctx(g, vm, gr_ctx);
	nvgpu_free_gr_ctx_struct(g, gr_ctx);
	nvgpu_gr_ctx_desc_free(g, desc);
	nvgpu_vm_put(vm);

	return UNIT_SUCCESS;
}
