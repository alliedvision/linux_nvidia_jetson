/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <unit/io.h>
#include <unit/unit.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/hal_init.h>
#include <nvgpu/enabled.h>
#include <common/gr/gr_priv.h>

#include "nvgpu-fuse.h"
#include "nvgpu-fuse-priv.h"
#include "nvgpu-fuse-gp10b.h"
#include "nvgpu-fuse-gm20b.h"
#include "common/gr/gr_config_priv.h"
#ifdef CONFIG_NVGPU_DGPU
#include "nvgpu-fuse-tu104.h"
#endif


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

/*
 * Overrides for the fuse functionality
 */

u32 gcplex_config;

/* Return pass and value for reading gcplex */
int read_gcplex_config_fuse_pass(struct gk20a *g, u32 *val)
{
	*val = gcplex_config;
	return 0;
}

/* Return fail for reading gcplex */
int read_gcplex_config_fuse_fail(struct gk20a *g, u32 *val)
{
	return -ENODEV;
}

int test_fuse_device_common_init(struct unit_module *m,
				struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	int result;
	struct fuse_test_args *args = (struct fuse_test_args *)__args;
	struct nvgpu_gr gr = {0};
	struct nvgpu_gr_config config = {0};

	/* Create fuse register space */
	if (nvgpu_posix_io_add_reg_space(g, args->fuse_base_addr, 0x1fff) != 0) {
		unit_err(m, "%s: failed to create register space\n",
			 __func__);
		return UNIT_FAIL;
	}

	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	g->params.gpu_arch = args->gpu_arch << NVGPU_GPU_ARCHITECTURE_SHIFT;
	g->params.gpu_impl = args->gpu_impl;
#else
	g->params.gpu_arch = NV_PMC_BOOT_0_ARCHITECTURE_GV110;
	g->params.gpu_impl = NV_PMC_BOOT_0_IMPLEMENTATION_B;
#endif

	g->gr = &gr;
	gr.config = &config;
	nvgpu_posix_io_writel_reg_space(g, args->sec_fuse_addr, 0x0);

	result = nvgpu_init_hal(g);
	if (result != 0) {
		unit_err(m, "%s: nvgpu_init_hal returned error %d\n",
			__func__, result);
		ret = UNIT_FAIL;
	}

	g->ops.fuse.read_gcplex_config_fuse = read_gcplex_config_fuse_pass;
	nvgpu_posix_io_writel_reg_space(g, GM20B_TOP_NUM_GPCS,
		GM20B_MAX_GPC_COUNT);

	return ret;
}

int test_fuse_device_common_cleanup(struct unit_module *m,
				    struct gk20a *g, void *__args)
{
	struct fuse_test_args *args = (struct fuse_test_args *)__args;

	nvgpu_posix_io_delete_reg_space(g, args->fuse_base_addr);

	return 0;
}

struct unit_module_test fuse_tests[] = {
	UNIT_TEST(fuse_gp10b_init, test_fuse_device_common_init,
		  &gp10b_init_args, 0),
	UNIT_TEST(fuse_gp10b_check_sec, test_fuse_gp10b_check_sec, NULL, 0),
	UNIT_TEST(fuse_gp10b_check_gcplex_fail,
		  test_fuse_gp10b_check_gcplex_fail,
		  NULL,
		  0),
	UNIT_TEST(fuse_gp10b_check_sec_invalid_gcplex,
		  test_fuse_gp10b_check_sec_invalid_gcplex,
		  NULL,
		  0),
	UNIT_TEST(fuse_gp10b_check_non_sec,
		  test_fuse_gp10b_check_non_sec,
		  NULL,
		  0),
	UNIT_TEST(fuse_gp10b_ecc, test_fuse_gp10b_ecc, NULL, 0),
	UNIT_TEST(fuse_gp10b_feature_override_disable,
		  test_fuse_gp10b_feature_override_disable, NULL, 0),
#ifdef CONFIG_NVGPU_SIM
	UNIT_TEST(fuse_gp10b_check_fmodel, test_fuse_gp10b_check_fmodel, NULL, 0),
#endif
	UNIT_TEST(fuse_gp10b_cleanup, test_fuse_device_common_cleanup,
		  &gp10b_init_args, 0),

	UNIT_TEST(fuse_gm20b_init, test_fuse_device_common_init,
		  &gm20b_init_args, 0),
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	UNIT_TEST(fuse_gm20b_check_sec, test_fuse_gm20b_check_sec, NULL, 0),
	UNIT_TEST(fuse_gm20b_check_sec_invalid_gcplex,
		  test_fuse_gm20b_check_sec_invalid_gcplex,
		  NULL,
		  0),
	UNIT_TEST(fuse_gm20b_check_gcplex_fail,
		  test_fuse_gm20b_check_gcplex_fail,
		  NULL,
		  0),
	UNIT_TEST(fuse_gm20b_check_non_sec,
		  test_fuse_gm20b_check_non_sec,
		  NULL,
		  0),
#endif
	UNIT_TEST(fuse_gm20b_basic_fuses, test_fuse_gm20b_basic_fuses, NULL, 0),
	UNIT_TEST(test_fuse_gm20b_basic_fuses_bvec, test_fuse_gm20b_basic_fuses_bvec, NULL, 0),
#ifdef CONFIG_NVGPU_SIM
	UNIT_TEST(fuse_gm20b_check_fmodel, test_fuse_gm20b_check_fmodel, NULL, 0),
#endif
	UNIT_TEST(fuse_gm20b_cleanup, test_fuse_device_common_cleanup,
		  &gm20b_init_args, 0),

#ifdef CONFIG_NVGPU_DGPU
	UNIT_TEST(fuse_tu104_init, test_fuse_device_common_init,
		  &tu104_init_args, 0),
	UNIT_TEST(fuse_tu104_vin_cal_rev, test_fuse_tu104_vin_cal_rev, NULL, 0),
	UNIT_TEST(fuse_tu104_vin_cal_slope_intercept,
		  test_fuse_tu104_vin_cal_slope_intercept,
		  NULL,
		  0),
	UNIT_TEST(fuse_tu104_cleanup, test_fuse_device_common_cleanup,
		  &tu104_init_args, 0),
#endif
};

UNIT_MODULE(fuse, fuse_tests, UNIT_PRIO_NVGPU_TEST);
