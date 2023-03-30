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

#include <nvgpu/gk20a.h>
#include <nvgpu/pmu.h>
#include <nvgpu/falcon.h>
#include <nvgpu/hal_init.h>
#include <nvgpu/netlist.h>
#include <nvgpu/fuse.h>

#include <nvgpu/gr/gr.h>

#include <nvgpu/posix/io.h>
#include <nvgpu/posix/mock-regs.h>
#include <nvgpu/posix/soc_fuse.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/hw/gk20a/hw_falcon_gk20a.h>
#include <nvgpu/hw/gp10b/hw_fuse_gp10b.h>
#include <nvgpu/hw/gv11b/hw_pwr_gv11b.h>
#include <nvgpu/hw/gv11b/hw_pwr_gv11b.h>

#include "hal/pmu/pmu_gk20a.h"

#include "../falcon/falcon_utf.h"
#include "../gr/nvgpu-gr-gv11b.h"
#include "nvgpu-pmu.h"

struct utf_falcon *pmu_flcn;

#define NUM_REG_SPACES 2U
static struct nvgpu_mock_iospace reg_spaces[NUM_REG_SPACES] = {
	[0] = { /* NV_PLTCG_LTCS_REGSPACE */
		.base = 0x17E200,
		.size = 0x100,
		.data = NULL,
	},
	[1] = { /* NV_PFB_HSHUB_ACTIVE_LTCS REGSPACE */
		.base = 0x1FBC20,
		.size = 0x4,
		.data = NULL,
	},
};


static bool stub_gv11b_is_pmu_supported(struct gk20a *g)
{
	/*
	 * return true to set g->ops.pmu.is_pmu_supported
	 * true for branch coverage
	 */
	return true;
}

static struct utf_falcon *pmu_flcn_from_addr(struct gk20a *g, u32 addr)
{
	struct utf_falcon *flcn = NULL;
	u32 flcn_base;

	if (pmu_flcn == NULL || pmu_flcn->flcn == NULL) {
		return NULL;
	}

	flcn_base = pmu_flcn->flcn->flcn_base;
	if ((addr >= flcn_base) &&
	    (addr < (flcn_base + UTF_FALCON_MAX_REG_OFFSET))) {
		flcn = pmu_flcn;
	}

	return flcn;
}

static void writel_access_reg_fn(struct gk20a *g,
				 struct nvgpu_reg_access *access)
{
	struct utf_falcon *flcn = NULL;

	flcn = pmu_flcn_from_addr(g, access->addr);
	if (flcn != NULL) {
		nvgpu_utf_falcon_writel_access_reg_fn(g, flcn, access);
	} else {
		nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
	}
	nvgpu_posix_io_record_access(g, access);
}

static void readl_access_reg_fn(struct gk20a *g,
				struct nvgpu_reg_access *access)
{
	struct utf_falcon *flcn = NULL;

	flcn = pmu_flcn_from_addr(g, access->addr);
	if (flcn != NULL) {
		nvgpu_utf_falcon_readl_access_reg_fn(g, flcn, access);
	} else {
		access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
	}
}

static int tegra_fuse_readl_access_reg_fn(unsigned long offset, u32 *value)
{
	if (offset == FUSE_GCPLEX_CONFIG_FUSE_0) {
		*value = GCPLEX_CONFIG_WPR_ENABLED_MASK;
	}
	return 0;
}

static struct nvgpu_posix_io_callbacks utf_falcon_reg_callbacks = {
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,

	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,

	.tegra_fuse_readl = tegra_fuse_readl_access_reg_fn,
};

static void utf_falcon_register_io(struct gk20a *g)
{
	nvgpu_posix_register_io(g, &utf_falcon_reg_callbacks);
}

static int add_reg_space(struct unit_module *m, struct gk20a *g)
{
	int err;
	u32 i;

	for (i = 0; i < NUM_REG_SPACES; i++) {
		struct nvgpu_mock_iospace *iospace = &reg_spaces[i];

		err = nvgpu_posix_io_add_reg_space(g, iospace->base,
						   iospace->size);
		nvgpu_assert(err == 0);
	}

	return 0;
}

static void nvgpu_init_gr_manager(struct gk20a *g)
{
	struct nvgpu_gpu_instance *gpu_instance = &g->mig.gpu_instance[0];
	struct nvgpu_gr_syspipe *gr_syspipe = &gpu_instance->gr_syspipe;

	g->mig.max_gpc_count = 1;
	g->mig.gpc_count = 1;
	g->mig.num_gpu_instances = 1;
	g->mig.num_gr_sys_pipes_enabled = 1;
	gr_syspipe->gr_instance_id = 0U;
	gr_syspipe->gr_syspipe_id = 0U;
	gr_syspipe->num_gpc = 1;
}

static int init_pmu_falcon_test_env(struct unit_module *m, struct gk20a *g)
{
	int err = 0;

	/*
	 * Initialise GR registers
	 */
	if (add_reg_space(m, g) == UNIT_FAIL) {
		unit_err(m, "failed to get initialized GR reg space\n");
		return UNIT_FAIL;
	}

	utf_falcon_register_io(g);

	/*
	 * Fuse register fuse_opt_priv_sec_en_r() is read during init_hal hence
	 * add it to reg space
	 */
	if (nvgpu_posix_io_add_reg_space(g,
					fuse_opt_priv_sec_en_r(), 0x4) != 0) {
		unit_err(m, "Add reg space failed!\n");
		return -ENOMEM;
	}

	/* HAL init required for getting the falcon ops initialized. */
	err = nvgpu_init_hal(g);
	if (err != 0) {
		return -ENODEV;
	}

	/* Initialize utf & nvgpu falcon for test usage */
	pmu_flcn = nvgpu_utf_falcon_init(m, g, FALCON_ID_PMU);
	if (pmu_flcn == NULL) {
		return -ENODEV;
	}

	err = g->ops.ecc.ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	err = nvgpu_netlist_init_ctx_vars(g);
	if (err != 0) {
		unit_return_fail(m, "netlist init failed\n");
	}

	nvgpu_init_gr_manager(g);

	nvgpu_set_enabled(g, NVGPU_SEC_SECUREGPCCS, true);

	err = nvgpu_gr_alloc(g);
	if (err != 0) {
		unit_err(m, " Gr allocation failed!\n");
		return -ENOMEM;
	}

	err = nvgpu_pd_cache_init(g);
	if (err != 0) {
		unit_err(m, " PD cache allocation failed!\n");
		return -ENOMEM;
	}

	return 0;
}

int test_pmu_early_init(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	/*
	 * initialize falcon
	 */
	if (init_pmu_falcon_test_env(m, g) != 0) {
		unit_return_fail(m, "Module init failed\n");
	}

	/*
	 * initialize the ECC init support
	 * and MM and LTC support
	 */
	err = g->ops.ecc.ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	err = g->ops.mm.init_mm_support(g);
	if (err != 0) {
		unit_return_fail(m, "failed to init gk20a mm");
	}

	err = g->ops.ltc.init_ltc_support(g);
	if (err != 0) {
		unit_return_fail(m, "failed to init gk20a ltc");
	}

	/*
	 * Case 1: nvgpu_pmu_early_init() fails due to memory
	 * allocation failure
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	err = nvgpu_pmu_early_init(g);

	if (err != -ENOMEM) {
		unit_return_fail(m,
			"nvgpu_pmu_early_init init didn't fail as expected\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	nvgpu_pmu_remove_support(g, g->pmu);

	/*
	 * case 2: Inject memory allocation failure
	 * to fail g->ops.pmu.ecc_init(g)
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
	err = nvgpu_pmu_early_init(g);

	if (err != -ENOMEM) {
		unit_return_fail(m,
			"nvgpu_pmu_early_init init didn't fail as expected\n");
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	nvgpu_pmu_remove_support(g, g->pmu);

	/*
	 * case 3: Inject memory allocation failure
	 * to fail g->ops.pmu.ecc_init(g)
	 */
	err = g->ops.ecc.ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, true, 2);
	err = nvgpu_pmu_early_init(g);

	if (err != -ENOMEM) {
		unit_return_fail(m,
			"nvgpu_pmu_early_init init didn't fail as expected\n");
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	nvgpu_pmu_remove_support(g, g->pmu);


	/*
	 * Case 4: nvgpu_pmu_early_init() passes
	 */
	err = g->ops.ecc.ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	err = nvgpu_pmu_early_init(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_pmu_early_init failed\n");
	}

	nvgpu_pmu_remove_support(g, g->pmu);

	/*
	 * Case 5: branch coverage by setting
	 * g->ecc.initialized = false
	 */
	g->ecc.initialized = false;
	err =  nvgpu_pmu_early_init(g);

	nvgpu_pmu_remove_support(g, g->pmu);

	g->ecc.initialized = true;

	/*
	 * case 6: Adding branch coverage and fail
	 * scenario by setting g->support_ls_pmu = false
	 */
	g->support_ls_pmu = false;
	err =  nvgpu_pmu_early_init(g);
	if (err != 0) {
		unit_return_fail(m, "support_ls_pmu failed\n");
	}

	err = g->ops.pmu.ecc_init(g);

	nvgpu_pmu_remove_support(g, g->pmu);

	if (err != 0) {
		unit_return_fail(m, "pmu ecc init failed\n");
	}

	/*
	 * case 7: Adding branch coverage
	 * By setting g->ops.pmu.is_pmu_supported
	 * to true
	 */
	g->support_ls_pmu = true;
	g->ecc.initialized = false;
	g->ops.pmu.is_pmu_supported = stub_gv11b_is_pmu_supported;
	err = nvgpu_pmu_early_init(g);

	nvgpu_pmu_remove_support(g, g->pmu);
	/*
	 * case 8: Adding branch coverage
	 * By setting g->ops.pmu.ecc_init
	 * to NULL
	 */

	g->ops.pmu.ecc_init = NULL;
	g->ops.pmu.ecc_free = NULL;
	err =  nvgpu_pmu_early_init(g);

	nvgpu_pmu_remove_support(g, g->pmu);

	return UNIT_SUCCESS;
}

int test_pmu_remove_support(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int err;

	err =  nvgpu_pmu_early_init(g);
	if (err != 0) {
		unit_return_fail(m, "support_ls_pmu failed\n");
	}

	/* case 1: nvgpu_pmu_remove_support() passes */
	nvgpu_pmu_remove_support(g, g->pmu);
	if (g->pmu != NULL) {
		unit_return_fail(m, "nvgpu_pmu_remove_support failed\n");
	}

	return UNIT_SUCCESS;
}

int test_pmu_reset(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int err;

	/* initialize falcon */
	if (init_pmu_falcon_test_env(m, g) != 0) {
		unit_return_fail(m, "Module init failed\n");
	}

	err = g->ops.ecc.ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	/* initialize PMU */
	err =  nvgpu_pmu_early_init(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_pmu_early_init failed\n");
	}

	/*
	 * Case 1: reset passes
	 */
	err = nvgpu_falcon_reset(g->pmu->flcn);
	if (err != 0 || (g->ops.pmu.is_engine_in_reset(g) != false)) {
		unit_return_fail(m, "nvgpu_pmu_reset failed\n");
	}

	/*
	 * Case 2: Set the falcon_falcon_idlestate_r register to 0x1
	 * to make the falcon busy so that idle wait function fails
	 * This case covers failig branch of the reset function
	 */
	nvgpu_posix_io_writel_reg_space(g, (pmu_flcn->flcn->flcn_base +
					falcon_falcon_idlestate_r()), 0x1);
	err = nvgpu_falcon_reset(g->pmu->flcn);
	if (err == -ETIMEDOUT) {
		unit_info(m, "nvgpu_pmu_reset failed as expected\n");
	} else {
		return UNIT_FAIL;
	}

	/*
	 * Set the register back to default value
	 */
	nvgpu_posix_io_writel_reg_space(g, (pmu_flcn->flcn->flcn_base +
					falcon_falcon_idlestate_r()), 0x0);


	/*
	 * Case 3: Fail scenario
	 * Set the falcon dmactl register to 0x2 (IMEM_SCRUBBING_PENDING)
	 * which results in -ETIMEDOUT error
	 */
	nvgpu_utf_falcon_set_dmactl(g, pmu_flcn, 0x2);
	err = nvgpu_falcon_reset(g->pmu->flcn);

	if (err == 0) {
		unit_return_fail(m, "nvgpu_pmu_reset failed\n");
	}

	/*
	 * Case 4: set pwr_falcon_engine_r true
	 * to fail gv11b_pmu_is_engine_in_reset()
	 *
	 */
	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_engine_r(),
					pwr_falcon_engine_reset_true_f());
	err = nvgpu_falcon_reset(g->pmu->flcn);

	if (err == -ETIMEDOUT) {
		unit_info(m, "nvgpu_pmu_reset failed as expected\n");
	} else {
		return UNIT_FAIL;
	}
	/*
	 * set back the register to default value
	 */

	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_engine_r(),
					pwr_falcon_engine_reset_false_f());

	err = nvgpu_falcon_reset(g->pmu->flcn);

	/*
	 * Case 5:
	 * Set g->is_fusa_sku = true
	 * to get branch coverage
	 */
	g->is_fusa_sku = true;
	err = nvgpu_falcon_reset(g->pmu->flcn);
	g->is_fusa_sku = false;

	/*
	 * Case 6:
	 * g->ops.pmu.pmu_enable_irq to NULL
	 * to achieve branch coverage
	 *
	 */
	g->ops.pmu.pmu_enable_irq = NULL;
	err = nvgpu_falcon_reset(g->pmu->flcn);

	return UNIT_SUCCESS;
}

int test_pmu_isr(struct unit_module *m,
				struct gk20a *g, void *args)
{	int err;
	u32 ecc_value, ecc_intr_value;
	struct nvgpu_pmu *pmu = g->pmu;

	pmu->isr_enabled = true;

	/*
	 * initialize falcon
	 */
	if (init_pmu_falcon_test_env(m, g) != 0) {
		unit_return_fail(m, "Module init failed\n");
	}

	if (nvgpu_posix_io_add_reg_space(g,
			pwr_pmu_ecc_intr_status_r(), 0x4) != 0) {
		unit_err(m, "Add pwr_pmu_ecc_intr_status_r() reg space failed!\n");
		return -ENOMEM;
	}

	if (nvgpu_posix_io_add_reg_space(g,
			pwr_pmu_falcon_ecc_status_r(), 0x4) != 0) {
		unit_err(m, "Add pwr_pmu_falcon_ecc_status_r() reg space failed!\n");
		return -ENOMEM;
	}

	if (nvgpu_posix_io_add_reg_space(g,
			pwr_pmu_falcon_ecc_address_r(), 0x4) != 0) {
		unit_err(m, "Add pwr_pmu_falcon_ecc_address_r() reg space failed!\n");
		return -ENOMEM;
	}

	if (nvgpu_posix_io_add_reg_space(g,
			pwr_pmu_falcon_ecc_corrected_err_count_r(), 0x4) != 0) {
		unit_err(m, "Add pwr_pmu_falcon_ecc_corrected_err_count_r() reg"
							"space failed!\n");
		return -ENOMEM;
	}

	if (nvgpu_posix_io_add_reg_space(g,
			pwr_pmu_falcon_ecc_uncorrected_err_count_r(), 0x4) != 0) {
		unit_err(m, "Add pwr_pmu_falcon_ecc_uncorrected_err_count_r()"
							"reg space failed!\n");
		return -ENOMEM;
	}

	err = g->ops.ecc.ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	/*
	 * initialize PMU
	 */
	err =  nvgpu_pmu_early_init(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_pmu_early_init failed\n");
	}

	/*
	 * Set the IRQ stat and mask registers
	 */
	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqstat_r(),
				pwr_falcon_irqstat_ext_ecc_parity_true_f());

	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqmask_r(),
				pwr_falcon_irqstat_ext_ecc_parity_true_f());

	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqdest_r(),
				pwr_falcon_irqstat_ext_ecc_parity_true_f());
	g->ops.pmu.pmu_isr(g);

	/*
	 * case 2: more branch coverage
	 */
	ecc_value = pwr_pmu_falcon_ecc_status_corrected_err_imem_m() |
			pwr_pmu_falcon_ecc_status_corrected_err_dmem_m() |
			pwr_pmu_falcon_ecc_status_uncorrected_err_imem_m() |
			pwr_pmu_falcon_ecc_status_uncorrected_err_dmem_m() |
			pwr_pmu_falcon_ecc_status_corrected_err_total_counter_overflow_m() |
			pwr_pmu_falcon_ecc_status_uncorrected_err_total_counter_overflow_m();

	/*
	 * intr 1 = 0x3
	 */
	ecc_intr_value = pwr_pmu_ecc_intr_status_corrected_m() |
				pwr_pmu_ecc_intr_status_uncorrected_m();

	nvgpu_posix_io_writel_reg_space(g, pwr_pmu_ecc_intr_status_r(),
						ecc_intr_value);

	nvgpu_posix_io_writel_reg_space(g, pwr_pmu_falcon_ecc_status_r(),
						ecc_value);
	EXPECT_BUG(g->ops.pmu.pmu_isr(g));

	/*
	 * Set pwr_pmu_ecc_intr_status_r to
	 * pwr_pmu_ecc_intr_status_uncorrected_m() to cover branches
	 */
	nvgpu_posix_io_writel_reg_space(g, pwr_pmu_falcon_ecc_status_r(),
						ecc_value);
	nvgpu_posix_io_writel_reg_space(g, pwr_pmu_ecc_intr_status_r(),
					pwr_pmu_ecc_intr_status_uncorrected_m());
	EXPECT_BUG(g->ops.pmu.pmu_isr(g));

	/*
	 * Set pwr_pmu_ecc_intr_status_r to
	 * pwr_pmu_ecc_intr_status_corrected_m() to cover branches
	 */
	nvgpu_posix_io_writel_reg_space(g, pwr_pmu_falcon_ecc_status_r(),
						ecc_value);
	nvgpu_posix_io_writel_reg_space(g, pwr_pmu_ecc_intr_status_r(),
					pwr_pmu_ecc_intr_status_corrected_m());
	EXPECT_BUG(g->ops.pmu.pmu_isr(g));

	/*
	 * intr 1 = 0x1
	 */
	nvgpu_posix_io_writel_reg_space(g, pwr_pmu_ecc_intr_status_r(),
					pwr_pmu_ecc_intr_status_corrected_m());
	g->ops.pmu.pmu_isr(g);

	/*
	 * intr 1 = 0x2
	 */
	nvgpu_posix_io_writel_reg_space(g, pwr_pmu_ecc_intr_status_r(),
					pwr_pmu_ecc_intr_status_uncorrected_m());
	g->ops.pmu.pmu_isr(g);

	/*
	 * Case 3: Covering branches in the function
	 * gv11b_pmu_handle_ext_irq()
	 */
	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqstat_r(), 0x1);
	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqmask_r(), 0x1);

	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqdest_r(), 0x1);

	g->ops.pmu.pmu_isr(g);

	/*
	 * case 4: Covering branch for intr = 0 in gk20a_pmu_isr
	 */
	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqmask_r(),
				pwr_falcon_irqstat_ext_ecc_parity_true_f());

	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqdest_r(),
				pwr_falcon_irqstat_ext_ecc_parity_true_f());

	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqstat_r(), 0x0);
	g->ops.pmu.pmu_isr(g);

	/*
	 * case 5: branch coverage for
	 * g->ops.pmu.handle_ext_irq = NULL
	 */
	nvgpu_posix_io_writel_reg_space(g, pwr_falcon_irqstat_r(),
				pwr_falcon_irqstat_ext_ecc_parity_true_f());

	g->ops.pmu.handle_ext_irq = NULL;
	g->ops.pmu.pmu_isr(g);

	/*
	 * case 6: pmu->isr_enabled = false
	 */
	pmu->isr_enabled = false;
	g->ops.pmu.pmu_isr(g);

	return UNIT_SUCCESS;
}

int test_is_pmu_supported(struct unit_module *m,
				struct gk20a *g, void *args)
{
	bool status;
	int err;
	/*
	 * initialize falcon
	 */
	if (init_pmu_falcon_test_env(m, g) != 0) {
		unit_return_fail(m, "Module init failed\n");
	}

	err = g->ops.ecc.ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	/*
	 * initialize PMU
	 */
	err =  nvgpu_pmu_early_init(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_pmu_early_init failed\n");
	}

	status = g->ops.pmu.is_pmu_supported(g);
	if (status != false) {
		unit_err(m, "test_is_pmu_supported failed\n");
	}
	return UNIT_SUCCESS;

}
static int free_falcon_test_env(struct unit_module *m, struct gk20a *g,
					void *__args)
{
	u32 i = 0;

	for (i = 0; i < NUM_REG_SPACES; i++) {
		nvgpu_posix_io_delete_reg_space(g, reg_spaces[i].base);
	}

	nvgpu_utf_falcon_free(g, pmu_flcn);

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_pmu_tests[] = {
	UNIT_TEST(pmu_early_init, test_pmu_early_init, NULL, 0),
	UNIT_TEST(pmu_supported, test_is_pmu_supported, NULL, 0),
	UNIT_TEST(pmu_remove_support, test_pmu_remove_support, NULL, 0),
	UNIT_TEST(pmu_reset, test_pmu_reset, NULL, 0),
	UNIT_TEST(pmu_isr, test_pmu_isr, NULL, 0),

	UNIT_TEST(falcon_free_test_env, free_falcon_test_env, NULL, 0),
};

UNIT_MODULE(nvgpu-pmu, nvgpu_pmu_tests, UNIT_PRIO_NVGPU_TEST);
