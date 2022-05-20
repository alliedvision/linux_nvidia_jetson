/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/fuse.h>

#include "nvgpu-fuse-priv.h"
#include "nvgpu-fuse-gp10b.h"

#define GP10B_FUSE_REG_BASE		0x00021000U
#define GP10B_FUSE_OPT_SEC_DEBUG_EN	(GP10B_FUSE_REG_BASE+0x218U)
#define GP10B_FUSE_OPT_ECC_EN		(GP10B_FUSE_REG_BASE+0x228U)
#define GP10B_FUSE_OPT_FEATURE_FUSES_OVERRIDE_DISABLE \
					(GP10B_FUSE_REG_BASE+0x3f0U)
#define GP10B_FUSE_OPT_PRIV_SEC_EN	(GP10B_FUSE_REG_BASE+0x434U)

/* for common init args */
struct fuse_test_args gp10b_init_args = {
	.gpu_arch = 0x13,
	.gpu_impl = 0xb,
	.fuse_base_addr = GP10B_FUSE_REG_BASE,
	.sec_fuse_addr = GP10B_FUSE_OPT_PRIV_SEC_EN,
};

int test_fuse_gp10b_check_sec(struct unit_module *m,
			      struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	int result;
	u32 i;

	nvgpu_posix_io_writel_reg_space(g, GP10B_FUSE_OPT_PRIV_SEC_EN, 0x1);

	gcplex_config = GCPLEX_CONFIG_WPR_ENABLED_MASK &
			~GCPLEX_CONFIG_VPR_AUTO_FETCH_DISABLE_MASK;

	for (i = 0; i < 2; i++) {
		nvgpu_posix_io_writel_reg_space(g, GP10B_FUSE_OPT_SEC_DEBUG_EN,
						i);

		result = g->ops.fuse.check_priv_security(g);
		if (result != 0) {
			unit_err(m, "%s: fuse_check_priv_security returned "
				 "error %d\n", __func__, result);
			ret = UNIT_FAIL;
		}

		if (!nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
			unit_err(m, "%s: NVGPU_SEC_PRIVSECURITY disabled\n",
				 __func__);
			ret = UNIT_FAIL;
		}

		if (!nvgpu_is_enabled(g, NVGPU_SEC_SECUREGPCCS)) {
			unit_err(m, "%s: NVGPU_SEC_SECUREGPCCS disabled\n",
				 __func__);
			ret = UNIT_FAIL;
		}
	}

	return ret;
}

int test_fuse_gp10b_check_gcplex_fail(struct unit_module *m,
				      struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	int result;

	g->ops.fuse.read_gcplex_config_fuse = read_gcplex_config_fuse_fail;
	result = g->ops.fuse.check_priv_security(g);
	if (result == 0) {
		unit_err(m, "%s: fuse_check_priv_security should have returned "
			 " error\n", __func__);
		ret = UNIT_FAIL;
	}

	g->ops.fuse.read_gcplex_config_fuse = read_gcplex_config_fuse_pass;

	return ret;
}

int test_fuse_gp10b_check_sec_invalid_gcplex(struct unit_module *m,
					     struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	int result;
	u32 gcplex_values[] = {
		0,
		~GCPLEX_CONFIG_WPR_ENABLED_MASK &
			GCPLEX_CONFIG_VPR_AUTO_FETCH_DISABLE_MASK,
		GCPLEX_CONFIG_WPR_ENABLED_MASK |
			GCPLEX_CONFIG_VPR_AUTO_FETCH_DISABLE_MASK,
	};
	int gcplex_entries = sizeof(gcplex_values)/sizeof(gcplex_values[0]);
	int i;

	g->ops.fuse.read_gcplex_config_fuse = read_gcplex_config_fuse_pass;

	nvgpu_posix_io_writel_reg_space(g, GP10B_FUSE_OPT_PRIV_SEC_EN, 0x1);

	for (i = 0; i < gcplex_entries; i++) {
		gcplex_config = gcplex_values[i];
		result = g->ops.fuse.check_priv_security(g);
		if (result == 0) {
			unit_err(m, "%s: fuse_check_priv_security should have returned "
				"error, i = %d, gcplex_config = %x\n",
				__func__, i, gcplex_config);
			ret = UNIT_FAIL;
		}
	}

	return ret;
}


int test_fuse_gp10b_check_non_sec(struct unit_module *m,
				  struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	int result;

	nvgpu_posix_io_writel_reg_space(g, GP10B_FUSE_OPT_PRIV_SEC_EN, 0x0);

	result = g->ops.fuse.check_priv_security(g);
	if (result != 0) {
		unit_err(m, "%s: fuse_check_priv_security returned "
				"error %d\n", __func__, result);
		ret = UNIT_FAIL;
	}

	if (nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
		unit_err(m, "%s: NVGPU_SEC_PRIVSECURITY enabled\n", __func__);
		ret = UNIT_FAIL;
	}

	if (nvgpu_is_enabled(g, NVGPU_SEC_SECUREGPCCS)) {
		unit_err(m, "%s: NVGPU_SEC_SECUREGPCCS enabled\n", __func__);
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_fuse_gp10b_ecc(struct unit_module *m,
			struct gk20a *g, void *__args)
{
	bool result;
	int ret = UNIT_SUCCESS;

	nvgpu_posix_io_writel_reg_space(g, GP10B_FUSE_OPT_ECC_EN, 0x0);

	result = g->ops.fuse.is_opt_ecc_enable(g);

	if (result) {
		unit_err(m, "%s: ECC should be disabled\n", __func__);
		ret = UNIT_FAIL;
	}

	nvgpu_posix_io_writel_reg_space(g, GP10B_FUSE_OPT_ECC_EN, 0x1);

	result = g->ops.fuse.is_opt_ecc_enable(g);

	if (!result) {
		unit_err(m, "%s: ECC should be enabled\n", __func__);
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_fuse_gp10b_feature_override_disable(struct unit_module *m,
			struct gk20a *g, void *__args)
{
	bool result;
	int ret = UNIT_SUCCESS;

	nvgpu_posix_io_writel_reg_space(g,
			GP10B_FUSE_OPT_FEATURE_FUSES_OVERRIDE_DISABLE, 0x0);

	result = g->ops.fuse.is_opt_feature_override_disable(g);

	if (result) {
		unit_err(m, "%s: Feature Override should be false\n", __func__);
		ret = UNIT_FAIL;
	}

	nvgpu_posix_io_writel_reg_space(g,
			GP10B_FUSE_OPT_FEATURE_FUSES_OVERRIDE_DISABLE, 0x0);

	result = g->ops.fuse.is_opt_feature_override_disable(g);

	if (result) {
		unit_err(m, "%s: Feature Override should be true\n", __func__);
		ret = UNIT_FAIL;
	}

	return ret;
}

#ifdef CONFIG_NVGPU_SIM
/* Verify when FMODEL is enabled, fuse module reports non-secure */
int test_fuse_gp10b_check_fmodel(struct unit_module *m,
				 struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	int result;

	nvgpu_set_enabled(g, NVGPU_IS_FMODEL, true);

	result = g->ops.fuse.check_priv_security(g);
	if (result != 0) {
		unit_err(m, "%s: fuse_check_priv_security returned "
				"error %d\n", __func__, result);
		ret = UNIT_FAIL;
	}

	if (nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
		unit_err(m, "%s: NVGPU_SEC_PRIVSECURITY enabled\n", __func__);
		ret = UNIT_FAIL;
	}

	if (nvgpu_is_enabled(g, NVGPU_SEC_SECUREGPCCS)) {
		unit_err(m, "%s: NVGPU_SEC_SECUREGPCCS enabled\n", __func__);
		ret = UNIT_FAIL;
	}

	nvgpu_set_enabled(g, NVGPU_IS_FMODEL, false);
	return ret;
}
#endif
