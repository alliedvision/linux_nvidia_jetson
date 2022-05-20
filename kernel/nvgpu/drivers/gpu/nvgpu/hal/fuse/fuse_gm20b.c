/*
 * GM20B FUSE
 *
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/fuse.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>

#include "fuse_gm20b.h"

#include <nvgpu/hw/gm20b/hw_fuse_gm20b.h>

int gm20b_fuse_check_priv_security(struct gk20a *g)
{
	u32 gcplex_config;
	bool is_wpr_enabled = false;
	bool is_auto_fetch_disable = false;

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		nvgpu_set_enabled(g, NVGPU_SEC_PRIVSECURITY, true);
		nvgpu_set_enabled(g, NVGPU_SEC_SECUREGPCCS, false);
		nvgpu_log(g, gpu_dbg_info, "priv sec is enabled in fmodel");
		return 0;
	}
#endif

	if (g->ops.fuse.read_gcplex_config_fuse(g, &gcplex_config) != 0) {
		nvgpu_err(g, "err reading gcplex config fuse, check fuse clk");
		return -EINVAL;
	}

	nvgpu_set_enabled(g, NVGPU_SEC_SECUREGPCCS, false);

	if (nvgpu_readl(g, fuse_opt_priv_sec_en_r()) != 0U) {
		/*
		 * all falcons have to boot in LS mode and this needs
		 * wpr_enabled set to 1 and vpr_auto_fetch_disable
		 * set to 0. In this case gmmu tries to pull wpr
		 * and vpr settings from tegra mc
		 */
		nvgpu_set_enabled(g, NVGPU_SEC_PRIVSECURITY, true);
		is_wpr_enabled =
			(gcplex_config & GCPLEX_CONFIG_WPR_ENABLED_MASK) != 0U;
		is_auto_fetch_disable =
			(gcplex_config & GCPLEX_CONFIG_VPR_AUTO_FETCH_DISABLE_MASK) != 0U;
		if (is_wpr_enabled && !is_auto_fetch_disable) {
			if (nvgpu_readl(g, fuse_opt_sec_debug_en_r()) != 0U) {
				nvgpu_log(g, gpu_dbg_info,
						"gcplex_config = 0x%08x, "
						"secure mode: ACR debug",
						gcplex_config);
			} else {
				nvgpu_log(g, gpu_dbg_info,
						"gcplex_config = 0x%08x, "
						"secure mode: ACR non debug",
						gcplex_config);
			}
		} else {
			nvgpu_err(g, "gcplex_config = 0x%08x "
				"invalid wpr_enabled/vpr_auto_fetch_disable "
				"with priv_sec_en", gcplex_config);
			/* do not try to boot GPU */
			return -EINVAL;
		}
	} else {
		nvgpu_set_enabled(g, NVGPU_SEC_PRIVSECURITY, false);
		nvgpu_log(g, gpu_dbg_info,
				"gcplex_config = 0x%08x, non secure mode",
				gcplex_config);
	}

	return 0;
}

u32 gm20b_fuse_status_opt_gpc(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_status_opt_gpc_r());
}
