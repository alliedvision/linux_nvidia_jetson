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

#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/fuse.h>
#include <nvgpu/hal_init.h>

#include "nvgpu-fuse-priv.h"
#include "nvgpu-fuse-gm20b.h"

/* register definitions for this block */
#define GM20B_FUSE_OPT_SEC_DEBUG_EN		(GM20B_FUSE_REG_BASE+0x218U)
#define GM20B_FUSE_STATUS_OPT_PRIV_SEC_EN	(GM20B_FUSE_REG_BASE+0x434U)
#define GM20B_FUSE_CTRL_OPT_TPC_GPC		(GM20B_FUSE_REG_BASE+0x838U)
#define GM20B_FUSE_STATUS_OPT_FBIO		(GM20B_FUSE_REG_BASE+0xC14U)
#define GM20B_FUSE_STATUS_OPT_GPC		(GM20B_FUSE_REG_BASE+0xC1CU)
#define GM20B_FUSE_STATUS_OPT_TPC_GPC		(GM20B_FUSE_REG_BASE+0xC38U)
#define GM20B_FUSE_STATUS_OPT_FBP		(GM20B_FUSE_REG_BASE+0xD38U)
#define GM20B_FUSE_STATUS_OPT_ROP_L2_FBP	(GM20B_FUSE_REG_BASE+0xD70U)
#define GM20B_MAX_FBPS_COUNT			32U

/* for common init args */
struct fuse_test_args gm20b_init_args = {
	.gpu_arch = 0x12,
	.gpu_impl = 0xb,
	.fuse_base_addr = GM20B_FUSE_REG_BASE,
	.sec_fuse_addr = GM20B_FUSE_STATUS_OPT_PRIV_SEC_EN,
};

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
int test_fuse_gm20b_check_sec(struct unit_module *m,
			      struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	int result;
	u32 i;

	nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_STATUS_OPT_PRIV_SEC_EN,
					0x1);

	gcplex_config = GCPLEX_CONFIG_WPR_ENABLED_MASK &
			~GCPLEX_CONFIG_VPR_AUTO_FETCH_DISABLE_MASK;

	for (i = 0; i < 2; i++) {
		/* ACR enable/disable */
		nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_OPT_SEC_DEBUG_EN,
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

		if (nvgpu_is_enabled(g, NVGPU_SEC_SECUREGPCCS)) {
			unit_err(m, "%s: NVGPU_SEC_SECUREGPCCS enabled\n",
				 __func__);
			ret = UNIT_FAIL;
		}
	}

	return ret;
}

int test_fuse_gm20b_check_gcplex_fail(struct unit_module *m,
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

int test_fuse_gm20b_check_sec_invalid_gcplex(struct unit_module *m,
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

	nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_STATUS_OPT_PRIV_SEC_EN,
					0x1);

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

int test_fuse_gm20b_check_non_sec(struct unit_module *m,
				  struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	int result;

	nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_STATUS_OPT_PRIV_SEC_EN,
					0x0);

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
#endif /* CONFIG_NVGPU_HAL_NON_FUSA */

int test_fuse_gm20b_basic_fuses(struct unit_module *m,
				struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	u32 set, val, i;

	for (set = 0; set <= 1; set++) {
		unit_info(m, "set for basic fuses = %u\n", set);

		nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_STATUS_OPT_FBIO,
						set);
		val = g->ops.fuse.fuse_status_opt_fbio(g);
		if (val != set) {
			unit_err(m, "%s: FBIO fuse incorrect %u != %u\n",
				__func__, val, set);
			ret = UNIT_FAIL;
		}

		nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_STATUS_OPT_FBP,
						set);
		val = g->ops.fuse.fuse_status_opt_fbp(g);
		if (val != set) {
			unit_err(m, "%s: FBP fuse incorrect %u != %u\n",
				__func__, val, set);
			ret = UNIT_FAIL;
		}

		for (i = 0; i < GM20B_MAX_FBPS_COUNT; i++) {
			nvgpu_posix_io_writel_reg_space(g,
					GM20B_FUSE_STATUS_OPT_ROP_L2_FBP+(i*4U),
					set+i);
		}
		for (i = 0; i < GM20B_MAX_FBPS_COUNT; i++) {
			val = g->ops.fuse.fuse_status_opt_l2_fbp(g, i);
			if (val != (set+i)) {
				unit_err(m,
					 "%s: ROP_L2_FBP incorrect %u != %u\n",
					__func__, val, set+i);
				ret = UNIT_FAIL;
				break;
			}
		}

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
		nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_STATUS_OPT_GPC,
						set);
		/*
		 * the fuse_status_opt_gpc() function pointer isn't set
		 * for gm20b
		 */
		val = gm20b_fuse_status_opt_gpc(g);
		if (val != set) {
			unit_err(m, "%s: GPC fuse incorrect %u != %u\n",
				__func__, val, set);
			ret = UNIT_FAIL;
		}
#endif

		for (i = 0; i < GM20B_MAX_GPC_COUNT; i++) {
			g->ops.fuse.fuse_ctrl_opt_tpc_gpc(g, i, set*i);
		}
		for (i = 0; i < GM20B_MAX_GPC_COUNT; i++) {
			val = nvgpu_posix_io_readl_reg_space(g,
					GM20B_FUSE_CTRL_OPT_TPC_GPC+(i*4U));
			if (val != (set*i)) {
				unit_err(m,
					 "%s TPC CTRL incorrect %u != %u\n",
					__func__, val, set*i);
				ret = UNIT_FAIL;
				break;
			}
		}

		for (i = 0; i < GM20B_MAX_GPC_COUNT; i++) {
			nvgpu_posix_io_writel_reg_space(g,
					GM20B_FUSE_STATUS_OPT_TPC_GPC+(i*4U),
					set*i);

		}

		for (i = 0; i < GM20B_MAX_GPC_COUNT; i++) {
			val = g->ops.fuse.fuse_status_opt_tpc_gpc(g, i);
			if (val != (set*i)) {
				unit_err(m,
					 "%s TPC STATUS incorrect %u != %u\n",
					__func__, val, set*i);
				ret = UNIT_FAIL;
				break;
			}
		}

		nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_OPT_SEC_DEBUG_EN,
						set);
		val = g->ops.fuse.fuse_opt_sec_debug_en(g);
		if (val != set) {
			unit_err(m,
				"%s: SEC_DEBUG_EN fuse incorrect %u != %u\n",
				__func__, val, set);
			ret = UNIT_FAIL;
		}

		nvgpu_posix_io_writel_reg_space(g,
					GM20B_FUSE_STATUS_OPT_PRIV_SEC_EN,
					set);
		val = g->ops.fuse.fuse_opt_priv_sec_en(g);
		if (val != set) {
			unit_err(m,
				"%s: PRIV_SEC_EN fuse incorrect %u != %u\n",
				__func__, val, set);
			ret = UNIT_FAIL;
		}
	}

	return ret;
}

int test_fuse_gm20b_basic_fuses_bvec(struct unit_module *m,
				struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	u32 set, val, i;

/* GPC in range */
	i = 0;
	set = 0;
	nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_STATUS_OPT_TPC_GPC+(i*4U),
				set*i);
	val = g->ops.fuse.fuse_status_opt_tpc_gpc(g, i);
	if (val != (set*i)) {
		unit_err(m, "%s TPC STATUS incorrect for gpc %u %u != %u\n",
			__func__, i, val, set*i);
		ret = UNIT_FAIL;
	}

/* GPC in range */
	i = GM20B_MAX_GPC_COUNT - 1;
	set = 4;
	nvgpu_posix_io_writel_reg_space(g, GM20B_FUSE_STATUS_OPT_TPC_GPC+(i*4U),
				set*i);
	val = g->ops.fuse.fuse_status_opt_tpc_gpc(g, i);
	if (val != (set*i)) {
		unit_err(m, "%s TPC STATUS incorrect for gpc %u %u != %u\n",
			__func__, i, val, set*i);
		ret = UNIT_FAIL;
	}

/* GPC is equal to MAX */
	i = GM20B_MAX_GPC_COUNT;
	val = EXPECT_BUG(g->ops.fuse.fuse_status_opt_tpc_gpc(g, i));
	if (val == 0) {
		unit_err(m, "%s TPC STATUS incorrect for gpc %u %u != %u\n",
			__func__, i, val, set*i);
		ret = UNIT_FAIL;
	}

/* GPC is more than MAX */
	i = GM20B_MAX_GPC_COUNT + 1;
	val = EXPECT_BUG(g->ops.fuse.fuse_status_opt_tpc_gpc(g, i));
	if (val == 0) {
		unit_err(m, "%s TPC STATUS incorrect for gpc %u %u != %u\n",
			__func__, i, val, set*i);
		ret = UNIT_FAIL;
	}

	return ret;
}

#ifdef CONFIG_NVGPU_SIM
/* Verify when FMODEL is enabled, fuse module reports non-secure */
int test_fuse_gm20b_check_fmodel(struct unit_module *m,
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

	if (!nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
		unit_err(m, "%s: NVGPU_SEC_PRIVSECURITY disabled\n", __func__);
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
