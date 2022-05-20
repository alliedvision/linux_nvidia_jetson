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

#include <unit/unit.h>
#include <unit/io.h>

#include <nvgpu/io.h>
#include <nvgpu/enabled.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/device.h>
#include <nvgpu/hw/gp10b/hw_fuse_gp10b.h>
#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>
#include <nvgpu/hw/gv11b/hw_therm_gv11b.h>

#include "hal/init/hal_gv11b.h"
#include "hal/power_features/cg/gating_reglist.h"
#include "hal/power_features/cg/gv11b_gating_reglist.h"
#include "../fifo/nvgpu-fifo-common.h"

#include "nvgpu-cg.h"

struct cg_test_data {
	u32 cg_type;
	void (*load_enable)(struct gk20a *g);
	u32 domain_count;
	const struct gating_desc *domain_descs[16];
	void (*gating_funcs[16])(struct gk20a *g, bool prod);
	u32 domain_desc_sizes[16];
};

static struct cg_test_data blcg_fb_ltc = {
	.cg_type = NVGPU_GPU_CAN_BLCG,
	.load_enable = nvgpu_cg_blcg_fb_ltc_load_enable,
	.domain_count = 2,
};

static struct cg_test_data blcg_fifo = {
	.cg_type = NVGPU_GPU_CAN_BLCG,
	.load_enable = nvgpu_cg_blcg_fifo_load_enable,
	.domain_count = 1,
};

static struct cg_test_data blcg_pmu = {
	.cg_type = NVGPU_GPU_CAN_BLCG,
	.load_enable = nvgpu_cg_blcg_pmu_load_enable,
	.domain_count = 1,
};

static struct cg_test_data blcg_ce = {
	.cg_type = NVGPU_GPU_CAN_BLCG,
	.load_enable = nvgpu_cg_blcg_ce_load_enable,
	.domain_count = 1,
};

static struct cg_test_data blcg_gr = {
	.cg_type = NVGPU_GPU_CAN_BLCG,
	.load_enable = nvgpu_cg_blcg_gr_load_enable,
	.domain_count = 1,
};

static struct cg_test_data slcg_fb_ltc = {
	.cg_type = NVGPU_GPU_CAN_SLCG,
	.load_enable = nvgpu_cg_slcg_fb_ltc_load_enable,
	.domain_count = 2,
};

static struct cg_test_data slcg_priring = {
	.cg_type = NVGPU_GPU_CAN_SLCG,
	.load_enable = nvgpu_cg_slcg_priring_load_enable,
	.domain_count = 1,
};

static struct cg_test_data slcg_fifo = {
	.cg_type = NVGPU_GPU_CAN_SLCG,
	.load_enable = nvgpu_cg_slcg_fifo_load_enable,
	.domain_count = 1,
};

struct cg_test_data slcg_pmu = {
	.cg_type = NVGPU_GPU_CAN_SLCG,
	.load_enable = nvgpu_cg_slcg_pmu_load_enable,
	.domain_count = 1,
};

struct cg_test_data slcg_therm = {
	.cg_type = NVGPU_GPU_CAN_SLCG,
	.load_enable = nvgpu_cg_slcg_therm_load_enable,
	.domain_count = 1,
};

struct cg_test_data slcg_ce2 = {
	.cg_type = NVGPU_GPU_CAN_SLCG,
	.load_enable = nvgpu_cg_slcg_ce2_load_enable,
	.domain_count = 1,
};

struct cg_test_data slcg_gr_load_gating_prod = {
	.cg_type = NVGPU_GPU_CAN_SLCG,
	.load_enable = nvgpu_cg_init_gr_load_gating_prod,
	.domain_count = 6,
};

struct cg_test_data blcg_gr_load_gating_prod = {
	.cg_type = NVGPU_GPU_CAN_BLCG,
	.load_enable = nvgpu_cg_init_gr_load_gating_prod,
	.domain_count = 4,
};

#define INIT_BLCG_DOMAIN_TEST_DATA(param)	({\
	struct cg_test_data *tmp = &blcg_##param; \
	tmp->domain_descs[0] = gv11b_blcg_##param##_get_gating_prod(); \
	tmp->gating_funcs[0] = g->ops.cg.blcg_##param##_load_gating_prod; \
	tmp->domain_desc_sizes[0] = gv11b_blcg_##param##_gating_prod_size(); \
	})

static void init_blcg_fb_ltc_data(struct gk20a *g)
{
	blcg_fb_ltc.domain_descs[0] = gv11b_blcg_fb_get_gating_prod();
	blcg_fb_ltc.gating_funcs[0] = g->ops.cg.blcg_fb_load_gating_prod;
	blcg_fb_ltc.domain_desc_sizes[0] = gv11b_blcg_fb_gating_prod_size();
	blcg_fb_ltc.domain_descs[1] = gv11b_blcg_ltc_get_gating_prod();
	blcg_fb_ltc.gating_funcs[1] = g->ops.cg.blcg_ltc_load_gating_prod;
	blcg_fb_ltc.domain_desc_sizes[1] = gv11b_blcg_ltc_gating_prod_size();
}

static void init_blcg_fifo_data(struct gk20a *g)
{
	INIT_BLCG_DOMAIN_TEST_DATA(fifo);
}

static void init_blcg_pmu_data(struct gk20a *g)
{
	INIT_BLCG_DOMAIN_TEST_DATA(pmu);
}

static void init_blcg_ce_data(struct gk20a *g)
{
	INIT_BLCG_DOMAIN_TEST_DATA(ce);
}

static void init_blcg_gr_data(struct gk20a *g)
{
	INIT_BLCG_DOMAIN_TEST_DATA(gr);
}

static void init_blcg_gr_load_gating_data(struct gk20a *g)
{
	blcg_gr_load_gating_prod.domain_descs[0] =
					gv11b_blcg_bus_get_gating_prod();
	blcg_gr_load_gating_prod.gating_funcs[0] =
					g->ops.cg.blcg_bus_load_gating_prod;
	blcg_gr_load_gating_prod.domain_desc_sizes[0] =
					gv11b_blcg_bus_gating_prod_size();
	blcg_gr_load_gating_prod.domain_descs[1] =
					gv11b_blcg_gr_get_gating_prod();
	blcg_gr_load_gating_prod.gating_funcs[1] =
					g->ops.cg.blcg_gr_load_gating_prod;
	blcg_gr_load_gating_prod.domain_desc_sizes[1] =
					gv11b_blcg_gr_gating_prod_size();
	blcg_gr_load_gating_prod.domain_descs[2] =
					gv11b_blcg_xbar_get_gating_prod();
	blcg_gr_load_gating_prod.gating_funcs[2] =
					g->ops.cg.blcg_xbar_load_gating_prod;
	blcg_gr_load_gating_prod.domain_desc_sizes[2] =
					gv11b_blcg_xbar_gating_prod_size();
	blcg_gr_load_gating_prod.domain_descs[3] =
					gv11b_blcg_hshub_get_gating_prod();
	blcg_gr_load_gating_prod.gating_funcs[3] =
					g->ops.cg.blcg_hshub_load_gating_prod;
	blcg_gr_load_gating_prod.domain_desc_sizes[3] =
					gv11b_blcg_hshub_gating_prod_size();
}

#define INIT_SLCG_DOMAIN_TEST_DATA(param)	({\
	struct cg_test_data *tmp = &slcg_##param; \
	tmp->domain_descs[0] = gv11b_slcg_##param##_get_gating_prod(); \
	tmp->gating_funcs[0] = g->ops.cg.slcg_##param##_load_gating_prod; \
	tmp->domain_desc_sizes[0] = gv11b_slcg_##param##_gating_prod_size(); \
	})

static void init_slcg_fb_ltc_data(struct gk20a *g)
{
	slcg_fb_ltc.domain_descs[0] = gv11b_slcg_fb_get_gating_prod();
	slcg_fb_ltc.gating_funcs[0] = g->ops.cg.slcg_fb_load_gating_prod;
	slcg_fb_ltc.domain_desc_sizes[0] = gv11b_slcg_fb_gating_prod_size();
	slcg_fb_ltc.domain_descs[1] = gv11b_slcg_ltc_get_gating_prod();
	slcg_fb_ltc.gating_funcs[1] = g->ops.cg.slcg_ltc_load_gating_prod;
	slcg_fb_ltc.domain_desc_sizes[1] = gv11b_slcg_ltc_gating_prod_size();
}

static void init_slcg_priring_data(struct gk20a *g)
{
	INIT_SLCG_DOMAIN_TEST_DATA(priring);
}

static void init_slcg_fifo_data(struct gk20a *g)
{
	INIT_SLCG_DOMAIN_TEST_DATA(fifo);
}

static void init_slcg_pmu_data(struct gk20a *g)
{
	INIT_SLCG_DOMAIN_TEST_DATA(pmu);
}

static void init_slcg_therm_data(struct gk20a *g)
{
	INIT_SLCG_DOMAIN_TEST_DATA(therm);
}

static void init_slcg_ce2_data(struct gk20a *g)
{
	INIT_SLCG_DOMAIN_TEST_DATA(ce2);
}

static void init_slcg_gr_load_gating_data(struct gk20a *g)
{
	slcg_gr_load_gating_prod.domain_descs[0] =
					gv11b_slcg_bus_get_gating_prod();
	slcg_gr_load_gating_prod.gating_funcs[0] =
					g->ops.cg.slcg_bus_load_gating_prod;
	slcg_gr_load_gating_prod.domain_desc_sizes[0] =
					gv11b_slcg_bus_gating_prod_size();
	slcg_gr_load_gating_prod.domain_descs[1] =
					gv11b_slcg_chiplet_get_gating_prod();
	slcg_gr_load_gating_prod.gating_funcs[1] =
					g->ops.cg.slcg_chiplet_load_gating_prod;
	slcg_gr_load_gating_prod.domain_desc_sizes[1] =
					gv11b_slcg_chiplet_gating_prod_size();
	slcg_gr_load_gating_prod.domain_descs[2] =
					gv11b_slcg_gr_get_gating_prod();
	slcg_gr_load_gating_prod.gating_funcs[2] =
					g->ops.cg.slcg_gr_load_gating_prod;
	slcg_gr_load_gating_prod.domain_desc_sizes[2] =
					gv11b_slcg_gr_gating_prod_size();
	slcg_gr_load_gating_prod.domain_descs[3] =
					gv11b_slcg_perf_get_gating_prod();
	slcg_gr_load_gating_prod.gating_funcs[3] =
					g->ops.cg.slcg_perf_load_gating_prod;
	slcg_gr_load_gating_prod.domain_desc_sizes[3] =
					gv11b_slcg_perf_gating_prod_size();
	slcg_gr_load_gating_prod.domain_descs[4] =
					gv11b_slcg_xbar_get_gating_prod();
	slcg_gr_load_gating_prod.gating_funcs[4] =
					g->ops.cg.slcg_xbar_load_gating_prod;
	slcg_gr_load_gating_prod.domain_desc_sizes[4] =
					gv11b_slcg_xbar_gating_prod_size();
	slcg_gr_load_gating_prod.domain_descs[5] =
					gv11b_slcg_hshub_get_gating_prod();
	slcg_gr_load_gating_prod.gating_funcs[5] =
					g->ops.cg.slcg_hshub_load_gating_prod;
	slcg_gr_load_gating_prod.domain_desc_sizes[5] =
					gv11b_slcg_hshub_gating_prod_size();
}

static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
	nvgpu_posix_io_record_access(g, access);
}

static void readl_access_reg_fn(struct gk20a *g,
			    struct nvgpu_reg_access *access)
{
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}


static struct nvgpu_posix_io_callbacks cg_callbacks = {
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

static int init_test_env(struct unit_module *m, struct gk20a *g, void *args)
{
	nvgpu_posix_register_io(g, &cg_callbacks);

	/*
	 * Fuse register fuse_opt_priv_sec_en_r() is read during init_hal hence
	 * add it to reg space
	 */
	if (nvgpu_posix_io_add_reg_space(g,
					 fuse_opt_priv_sec_en_r(), 0x4) != 0) {
		unit_err(m, "Add reg space failed!\n");
		return UNIT_FAIL;
	}

	if (nvgpu_posix_io_add_reg_space(g,
					 fuse_opt_ecc_en_r(), 0x4) != 0) {
		unit_err(m, "Add reg space failed!\n");
		return UNIT_FAIL;
	}

	if (nvgpu_posix_io_add_reg_space(g,
		fuse_opt_feature_fuses_override_disable_r(), 0x4) != 0) {
		unit_err(m, "Add reg space failed!\n");
		return UNIT_FAIL;
	}

	if (nvgpu_posix_io_add_reg_space(g,
			 gr_fecs_feature_override_ecc_r(), 0x4) != 0) {
		unit_err(m, "Add reg space failed!\n");
		return UNIT_FAIL;
	}

	if (nvgpu_posix_io_add_reg_space(g,
			gr_fecs_feature_override_ecc_1_r(), 0x4) != 0) {
		unit_err(m, "Add reg space failed!\n");
		return UNIT_FAIL;
	}

	gv11b_init_hal(g);

	init_blcg_fb_ltc_data(g);
	init_blcg_fifo_data(g);
	init_blcg_pmu_data(g);
	init_blcg_ce_data(g);
	init_blcg_gr_data(g);
	init_blcg_gr_load_gating_data(g);

	init_slcg_fb_ltc_data(g);
	init_slcg_priring_data(g);
	init_slcg_fifo_data(g);
	init_slcg_pmu_data(g);
	init_slcg_therm_data(g);
	init_slcg_ce2_data(g);
	init_slcg_gr_load_gating_data(g);

	return UNIT_SUCCESS;
}

static int add_domain_gating_regs(struct gk20a *g,
				  const struct gating_desc *desc, u32 size)
{
	int err = 0;
	u32 i, j;

	for (i = 0; i < size; i++) {
		if (nvgpu_posix_io_add_reg_space(g, desc[i].addr, 0x4) != 0) {
			err = -ENOMEM;
			goto clean_regs;
		}
	}

	return 0;

clean_regs:

	for (j = 0; j < i; j++) {
		nvgpu_posix_io_delete_reg_space(g, desc[j].addr);
	}

	return err;
}

static void delete_domain_gating_regs(struct gk20a *g,
				      const struct gating_desc *desc, u32 size)
{
	u32 i;

	for (i = 0; i < size; i++) {
		nvgpu_posix_io_delete_reg_space(g, desc[i].addr);
	}
}

static void invalid_load_enabled(struct gk20a *g,
				 struct cg_test_data *test_data)
{
	u32 i, j;

	for (i = 0; i < test_data->domain_count; i++) {
		for (j = 0; j < test_data->domain_desc_sizes[i]; j++) {
			nvgpu_writel(g, test_data->domain_descs[i][j].addr,
				     0xdeadbeed);
		}
	}
}

static int verify_load_enabled(struct gk20a *g, struct cg_test_data *test_data,
			       bool prod)
{
	u32 i, j, value;
	int mismatch = 0;

	for (i = 0; i < test_data->domain_count; i++) {
		for (j = 0; j < test_data->domain_desc_sizes[i]; j++) {
			value =
			    nvgpu_readl(g, test_data->domain_descs[i][j].addr);
			if (prod == true &&
			    value != test_data->domain_descs[i][j].prod) {
				mismatch = 1;
				goto out;
			} else if (prod == false &&
			value != test_data->domain_descs[i][j].disable) {
				mismatch = 1;
				goto out;
			}
		}
	}

out:
	return mismatch;
}

static void load_test_data_non_prod(struct gk20a *g,
				    struct cg_test_data *test_data)
{
	u32 i;

	for (i = 0; i < test_data->domain_count; i++) {
		test_data->gating_funcs[i](g, false);
	}
}

int test_cg(struct unit_module *m, struct gk20a *g, void *args)
{
	struct cg_test_data *test_data = (struct cg_test_data *) args;
	struct gpu_ops gops_temp;
	u32 i;
	int err;

	for (i = 0; i < test_data->domain_count; i++) {
		err = add_domain_gating_regs(g, test_data->domain_descs[i],
					     test_data->domain_desc_sizes[i]);
		if (err != 0) {
			return UNIT_FAIL;
		}
	}

	invalid_load_enabled(g, test_data);

	/**
	 * Test scenario where enabled flag and platform capability are
	 * not set.
	 */
	test_data->load_enable(g);
	err = verify_load_enabled(g, test_data, true);
	if (err == 0) {
		unit_err(m, "enabled flag and platform capability "
			    "not yet set\n");
		return UNIT_FAIL;
	}

	/** Tests if platform capability is checked setting enabled flag. */
	nvgpu_set_enabled(g, test_data->cg_type, true);
	test_data->load_enable(g);
	err = verify_load_enabled(g, test_data, true);
	if (err == 0) {
		unit_err(m, "platform capability not yet set\n");
		return UNIT_FAIL;
	}

	/** Tests if enabled flag is checked setting platform capability. */
	nvgpu_set_enabled(g, test_data->cg_type, false);
	if (test_data->cg_type == NVGPU_GPU_CAN_BLCG) {
		g->blcg_enabled = true;
	} else if (test_data->cg_type == NVGPU_GPU_CAN_SLCG) {
		g->slcg_enabled = true;
	}
	test_data->load_enable(g);
	err = verify_load_enabled(g, test_data, true);
	if (err == 0) {
		unit_err(m, "enabled flag not yet set\n");
		return UNIT_FAIL;
	}

	/** Tests if gating registers are setup as expected. */
	nvgpu_set_enabled(g, test_data->cg_type, true);
	test_data->load_enable(g);
	err = verify_load_enabled(g, test_data, true);
	if (err != 0) {
		unit_err(m, "gating registers prod mismatch\n");
		return UNIT_FAIL;
	}

	load_test_data_non_prod(g, test_data);
	err = verify_load_enabled(g, test_data, false);
	if (err != 0) {
		unit_err(m, "gating registers disable mismatch\n");
		return UNIT_FAIL;
	}

	/* Tests if CG hals are checked for NULL before invoking. */
	memcpy((u8 *)&gops_temp, (u8 *)&g->ops, sizeof(struct gpu_ops));
	memset(&g->ops, 0, sizeof(struct gpu_ops));

	invalid_load_enabled(g, test_data);

	test_data->load_enable(g);
	err = verify_load_enabled(g, test_data, true);
	if (err == 0) {
		unit_err(m, "CG hals not initialized\n");
		return UNIT_FAIL;
	}

	memcpy((u8 *)&g->ops, (u8 *)&gops_temp, sizeof(struct gpu_ops));

	/** Cleanup */
	for (i = 0; i < test_data->domain_count; i++) {
		delete_domain_gating_regs(g, test_data->domain_descs[i],
					  test_data->domain_desc_sizes[i]);
	}

	nvgpu_set_enabled(g, test_data->cg_type, false);

	g->blcg_enabled = false;
	g->slcg_enabled = false;

	/* Check that no invalid register access occurred */
	if (nvgpu_posix_io_get_error_code(g) != 0) {
		unit_return_fail(m, "Invalid register accessed\n");
	}

	return UNIT_SUCCESS;
}

static int elcg_add_engine_therm_regs(struct gk20a *g)
{
	u32 i;
	const struct nvgpu_device *dev;
	struct nvgpu_fifo *f = &g->fifo;

	for (i = 0U; i < f->num_engines; i++) {
		dev = f->active_engines[i];

		if (nvgpu_posix_io_add_reg_space(g,
			therm_gate_ctrl_r(dev->engine_id), 0x4) != 0) {
			return UNIT_FAIL;
		}
	}

	return UNIT_SUCCESS;
}

static void elcg_delete_engine_therm_regs(struct gk20a *g)
{
	u32 i;
	const struct nvgpu_device *dev;
	struct nvgpu_fifo *f = &g->fifo;

	for (i = 0U; i < f->num_engines; i++) {
		dev = f->active_engines[i];

		nvgpu_posix_io_delete_reg_space(g,
			therm_gate_ctrl_r(dev->engine_id));
	}
}

static int verify_elcg_status(struct gk20a *g, u32 cg_mode)
{
	u32 i;
	const struct nvgpu_device *dev;
	struct nvgpu_fifo *f = &g->fifo;
	int err = UNIT_SUCCESS;
	u32 gate_r;

	for (i = 0; i < f->num_engines; i++) {
		dev = f->active_engines[i];
		gate_r = nvgpu_readl(g, therm_gate_ctrl_r(dev->engine_id));

		if (cg_mode == ELCG_RUN) {
			if (get_field(gate_r,
					therm_gate_ctrl_eng_clk_m()) !=
					therm_gate_ctrl_eng_clk_run_f()) {
				err = UNIT_FAIL;
				break;
			}

			if (get_field(gate_r,
					therm_gate_ctrl_idle_holdoff_m()) !=
					therm_gate_ctrl_idle_holdoff_on_f()) {
				err = UNIT_FAIL;
				break;
			}
		} else if (cg_mode == ELCG_AUTO) {
			if (get_field(gate_r,
					therm_gate_ctrl_eng_clk_m()) !=
					therm_gate_ctrl_eng_clk_auto_f()) {
				err = UNIT_FAIL;
				break;
			}
		}
	}

	return err;
}

static int test_elcg_api(struct gk20a *g, int exp_err)
{
	int err;

	nvgpu_cg_elcg_enable_no_wait(g);
	err = verify_elcg_status(g, ELCG_AUTO);
	if (err != exp_err) {
		return UNIT_FAIL;
	}

	nvgpu_cg_elcg_disable_no_wait(g);
	err = verify_elcg_status(g, ELCG_RUN);
	if (err != exp_err) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_elcg(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;

	err = test_fifo_init_support(m, g, NULL);
	if (err != 0) {
		unit_return_fail(m, "failed to init fifo support\n");
		return err;
	}

	err = elcg_add_engine_therm_regs(g);
	if (err != 0) {
		unit_return_fail(m, "failed to add engine therm registers\n");
		return err;
	}

	if (test_elcg_api(g, UNIT_FAIL) != UNIT_SUCCESS) {
		unit_return_fail(m, "enabled flag not yet set\n");
	}

	nvgpu_set_enabled(g, NVGPU_GPU_CAN_ELCG, true);

	if (test_elcg_api(g, UNIT_FAIL) != UNIT_SUCCESS) {
		unit_return_fail(m, "platform capability not yet set\n");
	}

	g->elcg_enabled = true;

	if (test_elcg_api(g, UNIT_SUCCESS) != UNIT_SUCCESS) {
		unit_return_fail(m,
				 "elcg enable disable not setup correctly\n");
	}

	/* Check that no invalid register access occurred */
	if (nvgpu_posix_io_get_error_code(g) != 0) {
		unit_return_fail(m, "Invalid register accessed\n");
	}

	elcg_delete_engine_therm_regs(g);

	err = test_fifo_remove_support(m, g, NULL);
	if (err != 0) {
		unit_return_fail(m, "failed to remove fifo support\n");
		return err;
	}

	return UNIT_SUCCESS;
}

struct unit_module_test cg_tests[] = {
	UNIT_TEST(init, init_test_env, NULL, 0),

	UNIT_TEST(blcg_fb_ltc, test_cg, &blcg_fb_ltc, 0),
	UNIT_TEST(blcg_fifo,   test_cg, &blcg_fifo, 0),
	UNIT_TEST(blcg_ce,     test_cg, &blcg_ce, 0),
	UNIT_TEST(blcg_pmu,    test_cg, &blcg_pmu, 0),
	UNIT_TEST(blcg_gr,     test_cg, &blcg_gr, 0),

	UNIT_TEST(slcg_fb_ltc,  test_cg, &slcg_fb_ltc, 0),
	UNIT_TEST(slcg_priring, test_cg, &slcg_priring, 0),
	UNIT_TEST(slcg_fifo,    test_cg, &slcg_fifo, 0),
	UNIT_TEST(slcg_pmu,     test_cg, &slcg_pmu, 0),
	UNIT_TEST(slcg_therm,   test_cg, &slcg_therm, 0),
	UNIT_TEST(slcg_ce2,     test_cg, &slcg_ce2, 0),

	UNIT_TEST(slcg_gr_load_gating_prod, test_cg,
		  &slcg_gr_load_gating_prod, 0),
	UNIT_TEST(blcg_gr_load_gating_prod, test_cg,
		  &blcg_gr_load_gating_prod, 0),

	UNIT_TEST(elcg, test_elcg, NULL, 0),
};

UNIT_MODULE(cg, cg_tests, UNIT_PRIO_NVGPU_TEST);
