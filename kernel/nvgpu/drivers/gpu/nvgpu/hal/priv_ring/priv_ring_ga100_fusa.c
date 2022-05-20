/*
 * GA100 priv ring
 *
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log.h>
#include <nvgpu/timers.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/static_analysis.h>

#include "priv_ring_ga100.h"

#include <nvgpu/hw/ga100/hw_pri_ringmaster_ga100.h>

#ifdef CONFIG_NVGPU_MIG
#include <nvgpu/grmgr.h>

int ga100_priv_ring_config_gpc_rs_map(struct gk20a *g, bool enable)
{
	u32 reg_val;
	u32 index;
	u32 local_id;
	u32 logical_gpc_id = 0U;
	struct nvgpu_gr_syspipe *gr_syspipe;

	for (index = 0U; index < g->mig.num_gpu_instances; index++) {
		if (!nvgpu_grmgr_is_mig_type_gpu_instance(
				&g->mig.gpu_instance[index])) {
			nvgpu_log(g, gpu_dbg_mig, "skip physical instance[%u]",
				index);
			continue;
		}
		gr_syspipe = &g->mig.gpu_instance[index].gr_syspipe;

		for (local_id = 0U; local_id < gr_syspipe->num_gpc; local_id++) {
			logical_gpc_id = gr_syspipe->gpcs[local_id].logical_id;
			reg_val = nvgpu_readl(g, pri_ringmaster_gpc_rs_map_r(
				logical_gpc_id));

			if (enable) {
				reg_val = set_field(reg_val,
					pri_ringmaster_gpc_rs_map_smc_engine_id_m(),
					pri_ringmaster_gpc_rs_map_smc_engine_id_f(
						gr_syspipe->gr_syspipe_id));
				reg_val = set_field(reg_val,
					pri_ringmaster_gpc_rs_map_smc_engine_local_cluster_id_m(),
					pri_ringmaster_gpc_rs_map_smc_engine_local_cluster_id_f(
						local_id));
				reg_val = set_field(reg_val,
					pri_ringmaster_gpc_rs_map_smc_valid_m(),
					pri_ringmaster_gpc_rs_map_smc_valid_f(
						pri_ringmaster_gpc_rs_map_smc_valid_true_v()));
			} else {
				reg_val = set_field(reg_val,
					pri_ringmaster_gpc_rs_map_smc_valid_m(),
					pri_ringmaster_gpc_rs_map_smc_valid_f(
						pri_ringmaster_gpc_rs_map_smc_valid_false_v()));
			}

			nvgpu_writel(g, pri_ringmaster_gpc_rs_map_r(logical_gpc_id),
				reg_val);

			nvgpu_log(g, gpu_dbg_mig,
				"[%d] gpu_instance_id[%u] gr_syspipe_id[%u] gr_instance_id[%u] "
					"local_gpc_id[%u] physical_id[%u] logical_id[%u] "
					"gpcgrp_id[%u] reg_val[%x] enable[%d] ",
				index,
				g->mig.gpu_instance[index].gpu_instance_id,
				gr_syspipe->gr_syspipe_id,
				gr_syspipe->gr_instance_id,
				local_id,
				gr_syspipe->gpcs[local_id].physical_id,
				gr_syspipe->gpcs[local_id].logical_id,
				gr_syspipe->gpcs[local_id].gpcgrp_id,
				reg_val,
				enable);
		}
		/*
		 * Do a dummy read on last written GPC to ensure that RS_MAP has been acked
		 * by all slave ringstations.
		 */
		reg_val = nvgpu_readl(g, pri_ringmaster_gpc_rs_map_r(
			logical_gpc_id));
	}

	return 0;
}
#endif
