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
#include <sys/types.h>
#include <unistd.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/hal_init.h>
#include <nvgpu/enabled.h>
#include <nvgpu/hw/gm20b/hw_mc_gm20b.h>
#include <nvgpu/hw/gv11b/hw_ltc_gv11b.h>
#include <nvgpu/ltc.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/ecc.h>
#include <nvgpu/netlist.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/gr/gr.h>
#include <hal/ltc/intr/ltc_intr_gv11b.h>
#include <nvgpu/vm.h>

#include "nvgpu-ltc.h"

#define NV_PMC_BOOT_0_ARCHITECTURE_GV110	(0x00000015 << \
					NVGPU_GPU_ARCHITECTURE_SHIFT)
#define NV_PMC_BOOT_0_IMPLEMENTATION_B		0xB

/*
 * Mock I/O
 */

/*
 * Write callback. Forward the write access to the mock IO framework.
 */
static void writel_access_reg_fn(struct gk20a *g,
					struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

/*
 * Read callback. Get the register value from the mock IO framework.
 */
static void readl_access_reg_fn(struct gk20a *g,
					struct nvgpu_reg_access *access)
{
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}

static struct nvgpu_posix_io_callbacks netlist_test_reg_callbacks = {
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,
	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,
};

#define NUM_LTC 2
#define NUM_SLICES 2
static u32 mock_enum_ltc(struct gk20a *g)
{
	return NUM_LTC;
}

static int mock_ecc_init_fail(struct gk20a *g)
{
	return -1;
}

int test_ltc_init_support(struct unit_module *m,
						struct gk20a *g, void *args)
{

	int err = 0;
	void (*save_void_func)(struct gk20a *g);
	int (*save_ecc_func)(struct gk20a *g);
	struct nvgpu_ltc *save_ptr;
	struct nvgpu_posix_fault_inj *kmem_fi =
			nvgpu_kmem_get_fault_injection();
	struct mm_gk20a *mm = &g->mm;
	u64 low_hole;

	if (nvgpu_posix_io_add_reg_space(g, mc_boot_0_r(), 0xfff) != 0) {
		unit_err(m, "%s: failed to create register space\n", __func__);
		return UNIT_FAIL;
	}
	if (nvgpu_posix_io_add_reg_space(g, ltc_pltcg_base_v(), 0x3ffff) != 0) {
		unit_err(m, "%s: failed to create register space\n", __func__);
		return UNIT_FAIL;
	}

	(void)nvgpu_posix_register_io(g, &netlist_test_reg_callbacks);

	/*
	 * HAL init parameters for gv11b
	*/
	g->params.gpu_arch = NV_PMC_BOOT_0_ARCHITECTURE_GV110;
	g->params.gpu_impl = NV_PMC_BOOT_0_IMPLEMENTATION_B;

	/*
	 * Initialize slices in register
	 */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltcs_ltss_cbc_param_r(),
					NUM_SLICES << 28);

	/*
	 * HAL init required for getting
	 * the falcon ops initialized.
	 */
	err = nvgpu_init_hal(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_init_hal failed\n");
	}

	err = nvgpu_cic_mon_setup(g);
	if (err != 0) {
		unit_return_fail(m, "CIC init failed\n");
	}

	err = nvgpu_cic_mon_init_lut(g);
	if (err != 0) {
		unit_return_fail(m, "CIC LUT init failed\n");
	}

	/*
	 * Init dependent ECC unit
	 */
	err = nvgpu_ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	/*
	 * Override HALs
	 */
	g->ops.priv_ring.enum_ltc = mock_enum_ltc;

	err = g->ops.ltc.init_ltc_support(g);
	if (err != 0) {
		unit_return_fail(m, "g->ops.ltc.init_ltc_support failed\n");
	}

	/*
	 * Call init again after init to get branch coverage.
	 */
	err = g->ops.ltc.init_ltc_support(g);
	if (err != 0) {
		unit_return_fail(m, "g->ops.ltc.init_ltc_support failed\n");
	}

	/*
	 * Call init again with this HAL set to NULL for branch coverage.
	 */
	save_void_func = g->ops.ltc.init_fs_state;
	g->ops.ltc.init_fs_state = NULL;
	err = g->ops.ltc.init_ltc_support(g);
	if (err != 0) {
		unit_return_fail(m, "g->ops.ltc.init_ltc_support failed\n");
	}
	/* and restore the HAL */
	g->ops.ltc.init_fs_state = save_void_func;

	/*
	 * Call init with this HAL set to NULL for branch coverage.
	 */
	save_void_func = g->ops.ltc.intr.configure;
	g->ops.ltc.intr.configure = NULL;
	err = g->ops.ltc.init_ltc_support(g);
	if (err != 0) {
		unit_return_fail(m, "g->ops.ltc.init_ltc_support failed\n");
	}
	/* and restore the HAL */
	g->ops.ltc.intr.configure = save_void_func;

	/*
	 * Call init with ecc init HAL NULL for branch coverage.
	 */
	save_ecc_func = g->ops.ltc.ecc_init;
	g->ops.ltc.ecc_init = NULL;
	err = g->ops.ltc.init_ltc_support(g);
	if (err != 0) {
		unit_return_fail(m, "g->ops.ltc.init_ltc_support failed\n");
	}

	/*
	 * Call init with a failure returned from ecc init. This frees the ltc
	 * state in gk20a, so we'll need to init again.
	 */
	g->ops.ltc.ecc_init = mock_ecc_init_fail;
	err = g->ops.ltc.init_ltc_support(g);
	if (err == 0) {
		unit_return_fail(m, "g->ops.ltc.init_ltc_support failed\n");
	}
	/* and restore the HAL */
	g->ops.ltc.ecc_init = save_ecc_func;

	/*
	 * Call init with ecc flag set to initialzed for branch coverage.
	 */
	g->ecc.initialized = true;
	err = g->ops.ltc.init_ltc_support(g);
	if (err != 0) {
		unit_return_fail(m, "g->ops.ltc.init_ltc_support failed\n");
	}
	g->ecc.initialized = false;

	/*
	 * Call init with kmem fault inj enabled for branch coverage.
	 */
	save_ptr = g->ltc;
	g->ltc = NULL;
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	err = g->ops.ltc.init_ltc_support(g);
	if (err == 0) {
		unit_return_fail(m,
			"g->ops.ltc.init_ltc_support incorrectly succeeded\n");
	}
	/* and restore everything */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	g->ltc = save_ptr;

	/*
	 * Call init one final time to setup the state variable properly for
	 * future tests.
	 */
	err = g->ops.ltc.init_ltc_support(g);
	if (err != 0) {
		unit_return_fail(m, "g->ops.ltc.init_ltc_support failed\n");
	}

	low_hole = SZ_4K * 16UL;
	mm->bar1.aperture_size = 16 << 20;
	mm->bar1.vm = nvgpu_vm_init(g,
			g->ops.mm.gmmu.get_default_big_page_size(),
			low_hole,
			0ULL,
			nvgpu_safe_sub_u64(mm->bar1.aperture_size, low_hole),
			0ULL,
			true, false, false,
			"bar1");
	if (mm->bar1.vm == NULL) {
		unit_return_fail(m, "nvgpu_vm_init failed\n");
	}

	return UNIT_SUCCESS;
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

static int ltc_ecc_init_fault_check(struct unit_module *m, struct gk20a *g,
				    unsigned int number)
{
	struct nvgpu_posix_fault_inj *kmem_fi =
			nvgpu_kmem_get_fault_injection();
	int err;

	/* Re-Init dependent ECC unit */
	err = nvgpu_ecc_init_support(g);
	if (err != 0) {
		unit_err(m, "ecc init failed\n");
		return err;
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, true, number);
	err = g->ops.ltc.ecc_init(g);
	if (err == 0) {
		unit_err(m, "nvgpu_ecc_counter_init_per_lts() failed to return error\n");
		return -1;
	}

	return 0;
}

int test_ltc_ecc_init_free(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	int err;
	struct nvgpu_ecc_stat **save_sec_ptr = g->ecc.ltc.ecc_sec_count;
	struct nvgpu_ecc_stat **save_ded_ptr = g->ecc.ltc.ecc_ded_count;
	struct nvgpu_ecc_stat **save_tstg_ecc_ptr = g->ecc.ltc.tstg_ecc_parity_count;
	struct nvgpu_ecc_stat **save_dstg_ecc_ptr = g->ecc.ltc.dstg_be_ecc_parity_count;
	struct nvgpu_posix_fault_inj *kmem_fi =
			nvgpu_kmem_get_fault_injection();

	err = g->ops.ecc.ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	err = nvgpu_netlist_init_ctx_vars(g);
	if (err != 0) {
		unit_return_fail(m, "netlist init failed\n");
	}

	nvgpu_init_gr_manager(g);

	err = nvgpu_gr_alloc(g);
	if (err != 0) {
		unit_return_fail(m, "failed to init gr\n");
	}

	g->ecc.ltc.ecc_sec_count = NULL;
	g->ecc.ltc.ecc_ded_count = NULL;
	g->ecc.ltc.tstg_ecc_parity_count = NULL;
	g->ecc.ltc.dstg_be_ecc_parity_count = NULL;

	/*
	 * Call with failure on first kzalloc for sec_ecc_count
	 */
	err = ltc_ecc_init_fault_check(m, g, 0);
	if (err) {
		unit_err(m, "sec_ecc_count alloc fault check failed\n");
		ret = UNIT_FAIL;
		goto done;
	}

	/*
	 * Call with failure on third kzalloc for the 2nd array dimension and to
	 * validate unrolling.
	 */
	err = ltc_ecc_init_fault_check(m, g, 2);
	if (err) {
		unit_err(m, "sec_ecc_count alloc for LTC 1 fault check failed\n");
		ret = UNIT_FAIL;
		goto done;
	}

	/*
	 * Call with failure on 4th kzalloc for ded_ecc_count and get more
	 * branch/line coverage.
	 */
	err = ltc_ecc_init_fault_check(m, g, 4);
	if (err) {
		unit_err(m, "dec_ecc_count alloc fault check failed\n");
		ret = UNIT_FAIL;
		goto done;
	}

	/*
	 * Call with failure on 8th kzalloc for tstg_ecc_parity_count and get more
	 * branch/line coverage.
	 */
	err = ltc_ecc_init_fault_check(m, g, 8);
	if (err) {
		unit_err(m, "tstg_ecc_parity_count alloc fault check failed\n");
		ret = UNIT_FAIL;
		goto done;
	}

	/*
	 * Call with failure on 11th kzalloc for dstg_be_ecc_parity_count and get more
	 * branch/line coverage.
	 */
	err = ltc_ecc_init_fault_check(m, g, 11);
	if (err) {
		unit_err(m, "dstg_be_ecc_parity_count alloc fault check failed\n");
		ret = UNIT_FAIL;
		goto done;
	}

	/* Re-Init dependent ECC unit */
	err = nvgpu_ecc_init_support(g);
	if (err != 0) {
		unit_return_fail(m, "ecc init failed\n");
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	err = g->ops.ltc.ecc_init(g);
	if (err != 0) {
		unit_err(m, "nvgpu_ecc_counter_init_per_lts() failed to init\n");
		ret = UNIT_FAIL;
		goto done;
	}
	nvgpu_ltc_ecc_free(g);

done:
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	g->ecc.ltc.ecc_sec_count = save_sec_ptr;
	g->ecc.ltc.ecc_ded_count = save_ded_ptr;
	g->ecc.ltc.tstg_ecc_parity_count = save_tstg_ecc_ptr;
	g->ecc.ltc.dstg_be_ecc_parity_count = save_dstg_ecc_ptr;
	nvgpu_gr_free(g);

	return ret;
}

int test_ltc_functionality_tests(struct unit_module *m,
		struct gk20a *g, void *args)
{
	u32 ltc_count;
	u32 slice_per_ltc;
	u32 cacheline_size;

#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
	g->mm.ltc_enabled_current = false;
	nvgpu_ltc_sync_enabled(g);
#endif

	ltc_count = nvgpu_ltc_get_ltc_count(g);
	if (ltc_count != NUM_LTC) {
		unit_return_fail(m, "nvgpu_ltc_get_ltc_count failed\n");
	}
	slice_per_ltc = nvgpu_ltc_get_slices_per_ltc(g);
	if (slice_per_ltc != NUM_SLICES) {
		unit_return_fail(m, "nvgpu_ltc_get_slices_per_ltc failed\n");
	}
	cacheline_size = nvgpu_ltc_get_cacheline_size(g);
	if (cacheline_size == 0) {
		unit_return_fail(m, "nvgpu_ltc_get_cacheline_size failed\n");
	}

	return UNIT_SUCCESS;
}

int test_ltc_negative_tests(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err = 0;

#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
	g->mm.ltc_enabled_current = g->mm.ltc_enabled_target;
	nvgpu_ltc_sync_enabled(g);
	g->ops.ltc.set_enabled = NULL;
	nvgpu_ltc_sync_enabled(g);
#endif

	g->ops.ltc.ltc_remove_support(g);
	g->ops.ltc.ltc_remove_support(g);
	err = g->ops.ltc.init_ltc_support(g);
	if (err != 0) {
		unit_return_fail(m, "g->ops.ltc.init_ltc_support failed\n");
	}

	return UNIT_SUCCESS;
}

int test_ltc_remove_support(struct unit_module *m,
		struct gk20a *g, void *args)
{
	g->ops.ltc.ltc_remove_support(g);
	nvgpu_vm_put(g->mm.bar1.vm);

	return UNIT_SUCCESS;
}

static int mock_l2_flush(struct gk20a *g, bool inv)
{
	return 0;
}

int test_ltc_intr(struct unit_module *m, struct gk20a *g, void *args)
{
	int err = UNIT_SUCCESS;
	const u32 offset1 = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE) *
			nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);
	int (*save_func)(struct gk20a *g, bool inv);

	/* Init counter space */
	nvgpu_init_list_node(&g->ecc.stats_list);
	err = NVGPU_ECC_COUNTER_INIT_PER_LTS(ecc_sec_count);
	if (err != 0) {
		unit_err(m, "failed to init ecc_sec_count\n");
		err = UNIT_FAIL;
		goto done;
	}
	err = NVGPU_ECC_COUNTER_INIT_PER_LTS(ecc_ded_count);
	if (err != 0) {
		unit_err(m, "failed to init ecc_ded_count\n");
		err = UNIT_FAIL;
		goto done;
	}

	err = NVGPU_ECC_COUNTER_INIT_PER_LTS(tstg_ecc_parity_count);
	if (err != 0) {
		unit_err(m, "failed to init tstg_ecc_parity_count\n");
		err = UNIT_FAIL;
		goto done;
	}

	err = NVGPU_ECC_COUNTER_INIT_PER_LTS(dstg_be_ecc_parity_count);
	if (err != 0) {
		unit_err(m, "failed to init dstg_be_ecc_parity_count\n");
		err = UNIT_FAIL;
		goto done;
	}

	/* test with no intr pending */
	g->ops.ltc.intr.isr(g, 0);

	/* test with corrected intr, expect BUG */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr3_r(),
			ltc_ltcs_ltss_intr3_ecc_corrected_m());
	EXPECT_BUG(g->ops.ltc.intr.isr(g, 0));

	/* test with intr, but no corrected or uncorrected bits */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr3_r(),
			ltc_ltcs_ltss_intr3_ecc_uncorrected_m());
	g->ops.ltc.intr.isr(g, 0);

	/* set uncorrected overflow bits */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_l2_cache_ecc_status_r(),
		ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_total_counter_overflow_m());
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr3_r(),
			ltc_ltcs_ltss_intr3_ecc_uncorrected_m());
	g->ops.ltc.intr.isr(g, 0);

	/* set uncorrected overflow bits in second instance */
	nvgpu_posix_io_writel_reg_space(g,
		ltc_ltc0_lts0_l2_cache_ecc_status_r() + offset1,
		ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_total_counter_overflow_m());
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr3_r() + offset1,
			ltc_ltcs_ltss_intr3_ecc_uncorrected_m());
	g->ops.ltc.intr.isr(g, 0);

	/*
	 * Clear the uncorrected overflow bits. And for branch
	 * coverage, set the uncorrected err count.
	 */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_l2_cache_ecc_status_r(), 0x0);
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_r(),
		ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_total_m());
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr3_r(),
			ltc_ltcs_ltss_intr3_ecc_uncorrected_m());
	g->ops.ltc.intr.isr(g, 0);

	/* set rstg bits */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_l2_cache_ecc_status_r(),
		ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_rstg_m());
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr3_r(),
			ltc_ltcs_ltss_intr3_ecc_uncorrected_m());
	EXPECT_BUG(g->ops.ltc.intr.isr(g, 0));

	/* set tstg bits */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_l2_cache_ecc_status_r(),
		ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_tstg_m());
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr3_r(),
			ltc_ltcs_ltss_intr3_ecc_uncorrected_m());
	g->ops.ltc.intr.isr(g, 0);

	/* set dstg bits */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_l2_cache_ecc_status_r(),
		ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_dstg_m());
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr3_r(),
			ltc_ltcs_ltss_intr3_ecc_uncorrected_m());
	g->ops.ltc.intr.isr(g, 0);

	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr3_r(), 0);

	/* set sec error bits */
	save_func = g->ops.mm.cache.l2_flush;
	g->ops.mm.cache.l2_flush = mock_l2_flush;
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr_r(),
			ltc_ltcs_ltss_intr_ecc_sec_error_pending_f());
	g->ops.ltc.intr.isr(g, 0);
	g->ops.mm.cache.l2_flush = save_func;

	/* set ded error bits */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr_r(),
			ltc_ltcs_ltss_intr_ecc_ded_error_pending_f());
	g->ops.ltc.intr.isr(g, 0);

	/* For branch coverage, set sec error bits and make l2 flush fail */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr_r(),
			ltc_ltcs_ltss_intr_ecc_sec_error_pending_f());
	EXPECT_BUG(g->ops.ltc.intr.isr(g, 0));

	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_intr_r(), 0);

done:
	nvgpu_ltc_ecc_free(g);

	return err;
}

int test_ltc_intr_configure(struct unit_module *m,
				struct gk20a *g, void *args)
{
	u32 val;
#ifdef CONFIG_NVGPU_NON_FUSA
	void (*save_func)(struct gk20a *g, bool en);
#endif

	g->ops.ltc.intr.configure(g);
	val = nvgpu_posix_io_readl_reg_space(g, ltc_ltcs_ltss_intr_r());
	if ((val & (ltc_ltcs_ltss_intr_en_ecc_sec_error_enabled_f() |
		    ltc_ltcs_ltss_intr_en_ecc_ded_error_enabled_f())) !=
		(ltc_ltcs_ltss_intr_en_ecc_sec_error_enabled_f() |
		 ltc_ltcs_ltss_intr_en_ecc_ded_error_enabled_f())) {
		unit_return_fail(m, "failed to configure intr\n");
	}

#ifdef CONFIG_NVGPU_NON_FUSA
	/* for branch coverage test case where this HAL isn't configured */
	save_func = g->ops.ltc.intr.en_illegal_compstat;
	g->ops.ltc.intr.en_illegal_compstat = NULL;
#endif
	g->ops.ltc.intr.configure(g);
	val = nvgpu_posix_io_readl_reg_space(g, ltc_ltcs_ltss_intr_r());
	if ((val & (ltc_ltcs_ltss_intr_en_ecc_sec_error_enabled_f() |
		    ltc_ltcs_ltss_intr_en_ecc_ded_error_enabled_f())) !=
		(ltc_ltcs_ltss_intr_en_ecc_sec_error_enabled_f() |
		 ltc_ltcs_ltss_intr_en_ecc_ded_error_enabled_f())) {
		unit_return_fail(m, "failed to configure intr\n");
	}

#ifdef CONFIG_NVGPU_NON_FUSA
	g->ops.ltc.intr.en_illegal_compstat = save_func;
#endif

	return UNIT_SUCCESS;
}

int test_determine_L2_size_bytes(struct unit_module *m,
				struct gk20a *g, void *args)
{
	const u32 slice_size = 2;
	const u32 slice_per_l2 = 2;
	const u64 expected_size = g->ltc->ltc_count * slice_size * 1024 * slice_per_l2;
	u64 val;

	nvgpu_posix_io_writel_reg_space(g, ltc_ltc0_lts0_tstg_info_1_r(),
			(slice_size << 0) |
			(slice_per_l2 << 16));
	val = g->ops.ltc.determine_L2_size_bytes(g);
	if (val != expected_size) {
		unit_return_fail(m, "incorrect L2 size reported %lld, expected %lld\n",
				 val, expected_size);
	}

	return UNIT_SUCCESS;
}

#ifdef CONFIG_NVGPU_NON_FUSA
int test_ltc_intr_en_illegal_compstat(struct unit_module *m,
				struct gk20a *g, void *args)
{
	u32 val;

	/* clear the reg to be sure */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltcs_ltss_intr_r(), 0);

	g->ops.ltc.intr.en_illegal_compstat(g, true);
	val = nvgpu_posix_io_readl_reg_space(g, ltc_ltcs_ltss_intr_r());
	if ((val & ltc_ltcs_ltss_intr_en_illegal_compstat_m()) == 0) {
		unit_return_fail(m, "failed to enable illegal compstat\n");
	}

	g->ops.ltc.intr.en_illegal_compstat(g, false);
	val = nvgpu_posix_io_readl_reg_space(g, ltc_ltcs_ltss_intr_r());
	if ((val & ltc_ltcs_ltss_intr_en_illegal_compstat_m()) != 0) {
		unit_return_fail(m, "failed to disable illegal compstat\n");
	}

	return UNIT_SUCCESS;
}

int test_ltc_set_enabled(struct unit_module *m,	struct gk20a *g, void *args)
{
	u32 val;

	/* clear reg */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltcs_ltss_tstg_set_mgmt_2_r(),
					0x0);
	g->ops.ltc.set_enabled(g, true);
	val = nvgpu_posix_io_readl_reg_space(g,
					ltc_ltcs_ltss_tstg_set_mgmt_2_r());
	/* enabling is actually disabling bypass, so logic seems backwards */
	if ((val & ltc_ltcs_ltss_tstg_set_mgmt_2_l2_bypass_mode_enabled_f())
								!= 0) {
		unit_return_fail(m, "not enabled\n");
	}

	/* clear reg */
	nvgpu_posix_io_writel_reg_space(g, ltc_ltcs_ltss_tstg_set_mgmt_2_r(),
					0x0);
	g->ops.ltc.set_enabled(g, false);
	val = nvgpu_posix_io_readl_reg_space(g,
					ltc_ltcs_ltss_tstg_set_mgmt_2_r());
	/* enabling is actually disabling bypass, so logic seems backwards */
	if ((val & ltc_ltcs_ltss_tstg_set_mgmt_2_l2_bypass_mode_enabled_f())
								== 0) {
		unit_return_fail(m, "not disabled\n");
	}

	return UNIT_SUCCESS;
}
#endif

int test_flush_ltc(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	int i;
	u32 stride =  nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);

	/* make it appear the clean & invalidate completed */
	for (i = 0; i < NUM_LTC; i++) {
		nvgpu_posix_io_writel_reg_space(g,
				ltc_ltc0_ltss_tstg_cmgmt1_r() + (i * stride),
				0x0);
		nvgpu_posix_io_writel_reg_space(g,
				ltc_ltc0_ltss_tstg_cmgmt0_r() + (i * stride),
				0x0);
	}

	g->ops.ltc.flush(g);

	/*
	 * make it appear the clean & invalidate are pending to validate
	 * timeouts
	 */
	for (i = 0; i < NUM_LTC; i++) {
		nvgpu_posix_io_writel_reg_space(g,
			ltc_ltc0_ltss_tstg_cmgmt1_r() + (i * stride),
			ltc_ltc0_ltss_tstg_cmgmt1_clean_pending_f());
		nvgpu_posix_io_writel_reg_space(g,
			ltc_ltc0_ltss_tstg_cmgmt0_r() + (i * stride),
			ltc_ltc0_ltss_tstg_cmgmt0_invalidate_pending_f());
	}

	g->ops.ltc.flush(g);

	return ret;
}

struct unit_module_test nvgpu_ltc_tests[] = {
	UNIT_TEST(ltc_init_support, test_ltc_init_support, NULL, 0),
	UNIT_TEST(ltc_ecc_init_free, test_ltc_ecc_init_free, NULL, 0),
	UNIT_TEST(ltc_functionality_tests, test_ltc_functionality_tests,
								NULL, 0),
	UNIT_TEST(ltc_intr, test_ltc_intr, NULL, 0),
	UNIT_TEST(ltc_intr_configure, test_ltc_intr_configure, NULL, 0),
	UNIT_TEST(ltc_determine_L2_size, test_determine_L2_size_bytes, NULL, 0),
#ifdef CONFIG_NVGPU_NON_FUSA
	UNIT_TEST(ltc_intr_en_illegal_compstat,
				test_ltc_intr_en_illegal_compstat, NULL, 0),
	UNIT_TEST(ltc_set_enabled, test_ltc_set_enabled, NULL, 0),
#endif
	UNIT_TEST(ltc_flush, test_flush_ltc, NULL, 0),
	UNIT_TEST(ltc_negative_tests, test_ltc_negative_tests, NULL, 0),
	UNIT_TEST(ltc_remove_support, test_ltc_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu-ltc, nvgpu_ltc_tests, UNIT_PRIO_NVGPU_TEST);
