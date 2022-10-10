/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/timers.h>
#include <nvgpu/pmu.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/bug.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/pmu/lsfm.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/gr/gr_instances.h>

#include "lsfm_sw_gv100.h"

#define LSF_INDEX_MASK_DEFAULT	0x0U

static int gv100_pmu_lsfm_init_acr_wpr_region(struct gk20a *g,
	struct nvgpu_pmu *pmu)
{
	struct nv_pmu_rpc_struct_acr_init_wpr_region rpc;
	int status = 0;

	(void) memset(&rpc, 0,
		sizeof(struct nv_pmu_rpc_struct_acr_init_wpr_region));
	rpc.wpr_regionId = 0x1U;
	rpc.wpr_offset = 0x0U;
	nvgpu_pmu_dbg(g, "Post NV_PMU_RPC_ID_ACR_INIT_WPR_REGION");
	PMU_RPC_EXECUTE(status, pmu, ACR, INIT_WPR_REGION, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
	}

	/*
	 * Add delay of 2ms after init region command sent to LSPMU(LSPMU
	 * takes ~350usec to complete the command process) and before polling
	 * the irqstat register in nvgpu to engage priv lockdown in LSPMU.
	 * This additional delay will help to skip reading the irqstat
	 * incorrectly at corner case during the priv lockdown process.
	 */
	nvgpu_msleep(2);

	return status;
}

static int gv100_pmu_lsfm_bootstrap_ls_falcon(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm, u32 falcon_id_mask)
{
	struct nv_pmu_rpc_struct_acr_bootstrap_gr_falcons rpc;
	u32 flags = PMU_ACR_CMD_BOOTSTRAP_FALCON_FLAGS_RESET_YES;
	int status = 0;

	if (falcon_id_mask == 0U) {
		return -EINVAL;
	}

	if ((falcon_id_mask &
		~(BIT32(FALCON_ID_FECS) |
		  BIT32(FALCON_ID_GPCCS))) != 0U) {
		return -EINVAL;
	}

	lsfm->loaded_falcon_id = 0U;

	(void) memset(&rpc, 0,
		sizeof(struct nv_pmu_rpc_struct_acr_bootstrap_gr_falcons));
	rpc.falcon_id_mask = falcon_id_mask;
	rpc.flags = flags;
	nvgpu_pmu_dbg(g, "Post NV_PMU_RPC_ID_ACR_BOOTSTRAP_GR_FALCONS");
	PMU_RPC_EXECUTE(status, pmu, ACR, BOOTSTRAP_GR_FALCONS, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC, status=0x%x", status);
		goto exit;
	}

	/*
	 * Add delay of 5ms after bootstrap command sent to LSPMU(LSPMU takes
	 * ~3.5msec to complete the command process) and before polling
	 * the irqstat register in nvgpu to engage priv lockdown in LSPMU.
	 * This additional delay will help to skip reading the irqstat
	 * incorrectly at corner case during the priv lockdown process.
	 */
	nvgpu_msleep(5);

	pmu_wait_message_cond(g->pmu, nvgpu_get_poll_timeout(g),
		&lsfm->loaded_falcon_id, 1U);

	if (lsfm->loaded_falcon_id != 1U) {
		status =  -ETIMEDOUT;
	}

exit:
	return status;
}

static u32 fetch_gpc_falcon_idx_mask(struct gk20a *g)
{
	u32 gpc_falcon_idx_mask = 0U;

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		gpc_falcon_idx_mask = nvgpu_grmgr_get_gr_logical_gpc_mask(g,
			nvgpu_gr_get_cur_instance_id(g));
	} else {
		u32 gpc_fs_mask;
		struct nvgpu_gr_config *gr_config = nvgpu_gr_get_config_ptr(g);

		gpc_fs_mask = nvgpu_gr_config_get_gpc_mask(gr_config);
		gpc_falcon_idx_mask =
			nvgpu_safe_sub_u32(
			(1U << U32(hweight32(gpc_fs_mask))), 1U);
	}

	return gpc_falcon_idx_mask;
}

static int gv100_pmu_lsfm_bootstrap_ls_falcon_eng(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm, u32 falcon_id)
{
	struct nv_pmu_rpc_struct_acr_bootstrap_falcon rpc;
	u32 flags = PMU_ACR_CMD_BOOTSTRAP_FALCON_FLAGS_RESET_YES;
	int status = 0;

	lsfm->loaded_falcon_id = 0U;

	(void) memset(&rpc, 0,
		sizeof(struct nv_pmu_rpc_struct_acr_bootstrap_falcon));

	rpc.falcon_id = falcon_id;
	rpc.flags = flags;

	rpc.engine_instance =
			nvgpu_grmgr_get_gr_syspipe_id(g,
				nvgpu_gr_get_cur_instance_id(g));
	rpc.engine_index_mask = LSF_INDEX_MASK_DEFAULT;

	if (falcon_id == FALCON_ID_GPCCS) {
		rpc.engine_index_mask = fetch_gpc_falcon_idx_mask(g);
	}

	PMU_RPC_EXECUTE(status, pmu, ACR, BOOTSTRAP_FALCON, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC, status=0x%x", status);
		goto exit;
	}

	/*
	 * Add delay of 5ms after bootstrap command sent to LSPMU(LSPMU takes
	 * ~3.5msec to complete the command process) and before polling
	 * the irqstat register in nvgpu to engage priv lockdown in LSPMU.
	 * This additional delay will help to skip reading the irqstat
	 * incorrectly at corner case during the priv lockdown process.
	 */
	nvgpu_msleep(5);

	pmu_wait_message_cond(g->pmu, nvgpu_get_poll_timeout(g),
		&lsfm->loaded_falcon_id, 1U);

	if (lsfm->loaded_falcon_id != 1U) {
		status =  -ETIMEDOUT;
	}
exit:
	return status;
}

int gv100_update_lspmu_cmdline_args_copy(struct gk20a *g,
	struct nvgpu_pmu *pmu)
{
	u32 cmd_line_args_offset = 0U;
	u32 dmem_size = 0U;
	int err = 0;

	err = nvgpu_falcon_get_mem_size(pmu->flcn, MEM_DMEM, &dmem_size);
	if (err != 0) {
		nvgpu_err(g, "dmem size request failed");
		return -EINVAL;
	}

	cmd_line_args_offset = dmem_size -
		pmu->fw->ops.get_cmd_line_args_size(pmu);

	/*Copying pmu cmdline args*/
	pmu->fw->ops.set_cmd_line_args_cpu_freq(pmu, 0U);
	pmu->fw->ops.set_cmd_line_args_secure_mode(pmu, 0U);
	pmu->fw->ops.set_cmd_line_args_trace_size(
		pmu, PMU_RTOS_TRACE_BUFSIZE);
	pmu->fw->ops.set_cmd_line_args_trace_dma_base(pmu);
	pmu->fw->ops.set_cmd_line_args_trace_dma_idx(
		pmu, GK20A_PMU_DMAIDX_VIRT);
	if (pmu->fw->ops.config_cmd_line_args_super_surface != NULL) {
		pmu->fw->ops.config_cmd_line_args_super_surface(pmu);
	}

	return nvgpu_falcon_copy_to_dmem(pmu->flcn, cmd_line_args_offset,
		(u8 *)(pmu->fw->ops.get_cmd_line_args_ptr(pmu)),
		pmu->fw->ops.get_cmd_line_args_size(pmu), 0U);
}

void nvgpu_gv100_lsfm_sw_init(struct gk20a *g, struct nvgpu_pmu_lsfm *lsfm)
{
	nvgpu_log_fn(g, " ");

	lsfm->is_wpr_init_done = false;
	lsfm->loaded_falcon_id = 0U;

	lsfm->init_wpr_region = gv100_pmu_lsfm_init_acr_wpr_region;
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		lsfm->bootstrap_ls_falcon = gv100_pmu_lsfm_bootstrap_ls_falcon;
	} else {
		lsfm->bootstrap_ls_falcon = gv100_pmu_lsfm_bootstrap_ls_falcon_eng;
	}
	lsfm->ls_pmu_cmdline_args_copy = gv100_update_lspmu_cmdline_args_copy;
}
