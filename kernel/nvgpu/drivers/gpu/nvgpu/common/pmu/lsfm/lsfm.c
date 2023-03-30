/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/string.h>
#include <nvgpu/types.h>
#include <nvgpu/dma.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmu/lsfm.h>

#include "lsfm_sw_gm20b.h"
#include "lsfm_sw_gp10b.h"
#ifdef CONFIG_NVGPU_DGPU
#include "lsfm_sw_gv100.h"
#include "lsfm_sw_tu104.h"
#endif

#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_NEXT)
#include <nvgpu_next_lsfm.h>
#endif

static bool is_lsfm_supported(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm)
{
	(void)pmu;
	/*
	 * Low secure falcon manager is a secure iGPU functionality to support
	 * Lazy bootstrap feature. Enabling lsfm will allow nvgpu to send cmds
	 * to lspmu to bootstrap LS falcons.
	 */
	if (nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY) &&
			(lsfm != NULL)) {
		return true;
	}

	return false;
}

static int lsfm_int_wpr_region(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm)
{
	int status = 0;

	status = nvgpu_pmu_wait_fw_ready(g, g->pmu);
	if (status != 0) {
		nvgpu_err(g, "PMU not ready to process requests");
		goto done;
	}

	if (lsfm->init_wpr_region != NULL) {
		status = lsfm->init_wpr_region(g, pmu);
	} else {
		status = -EINVAL;
		goto done;
	}

	if (status == 0) {
		pmu_wait_message_cond(g->pmu,
				nvgpu_get_poll_timeout(g),
				&lsfm->is_wpr_init_done, 1U);
		/* check again if it still not ready indicate an error */
		if (!lsfm->is_wpr_init_done) {
			nvgpu_err(g, "PMU not ready to load LSF");
			status = -ETIMEDOUT;
		}
	}

done:
	return status;
}

int nvgpu_pmu_lsfm_bootstrap_ls_falcon(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm, u32 falcon_id_mask)
{
	int status = 0;

	if (!is_lsfm_supported(g, pmu, lsfm)) {
		return 0;
	}

	/*
	 * check whether pmu is ready to bootstrap lsf, if not send
	 * the init WPR region command and wait for completion.
	 */
	if (!lsfm->is_wpr_init_done) {
		status = lsfm_int_wpr_region(g, pmu, lsfm);
		if (status != 0) {
			nvgpu_err(g, "LSF init WPR region failed");
			goto done;
		}
	}

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		if (lsfm->bootstrap_ls_falcon != NULL) {
			status = lsfm->bootstrap_ls_falcon(g, pmu, lsfm,
				falcon_id_mask);
		}
	} else {
		status = lsfm->bootstrap_ls_falcon(g, pmu, lsfm, FALCON_ID_FECS);
		if (status != 0) {
			goto done;
		}

		status = lsfm->bootstrap_ls_falcon(g, pmu, lsfm, FALCON_ID_GPCCS);
	}

done:
	if (status != 0) {
			nvgpu_err(g, "LSF Load failed");
	}

	return status;
}

int nvgpu_pmu_lsfm_ls_pmu_cmdline_args_copy(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm)
{
	if (is_lsfm_supported(g, pmu, lsfm)) {
		if (lsfm->ls_pmu_cmdline_args_copy != NULL) {
			return lsfm->ls_pmu_cmdline_args_copy(g, pmu);
		}
	}

	return 0;
}

void nvgpu_pmu_lsfm_rpc_handler(struct gk20a *g,
	struct rpc_handler_payload *rpc_payload)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct nv_pmu_rpc_struct_acr_bootstrap_gr_falcons acr_rpc;

	(void) memset(&acr_rpc, 0, sizeof(struct nv_pmu_rpc_header));
	nvgpu_memcpy((u8 *)&acr_rpc, (u8 *)rpc_payload->rpc_buff,
		sizeof(struct nv_pmu_rpc_struct_acr_bootstrap_gr_falcons));

	switch (acr_rpc.hdr.function) {
	case NV_PMU_RPC_ID_ACR_INIT_WPR_REGION:
		nvgpu_pmu_dbg(g,
			"reply NV_PMU_RPC_ID_ACR_INIT_WPR_REGION");
		pmu->lsfm->is_wpr_init_done = true;
		break;
	case NV_PMU_RPC_ID_ACR_BOOTSTRAP_GR_FALCONS:
		nvgpu_pmu_dbg(g,
			"reply NV_PMU_RPC_ID_ACR_BOOTSTRAP_GR_FALCONS");
		pmu->lsfm->loaded_falcon_id = 1U;
		break;
	case NV_PMU_RPC_ID_ACR_BOOTSTRAP_FALCON:
		nvgpu_pmu_dbg(g, "reply NV_PMU_RPC_ID_ACR_BOOTSTRAP_FALCON");
		pmu->lsfm->loaded_falcon_id = 1U;
		break;
	default:
		nvgpu_pmu_dbg(g, "unsupported ACR function");
		break;
	}
}

void nvgpu_pmu_lsfm_clean(struct gk20a *g, struct nvgpu_pmu *pmu,
		struct nvgpu_pmu_lsfm *lsfm)
{
	nvgpu_log_fn(g, " ");

	if (is_lsfm_supported(g, pmu, lsfm)) {
		lsfm->is_wpr_init_done = false;
		lsfm->loaded_falcon_id = 0U;
	}
}

int nvgpu_pmu_lsfm_init(struct gk20a *g, struct nvgpu_pmu_lsfm **lsfm)
{
	u32 ver = g->params.gpu_arch + g->params.gpu_impl;
	int err = 0;

	if (!nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
		return 0;
	}

	if (*lsfm != NULL) {
		/* skip alloc/reinit for unrailgate sequence */
		nvgpu_pmu_dbg(g, "skip lsfm init for unrailgate sequence");
		goto done;
	}

	*lsfm = (struct nvgpu_pmu_lsfm *)
		nvgpu_kzalloc(g, sizeof(struct nvgpu_pmu_lsfm));
	if (*lsfm == NULL) {
		err = -ENOMEM;
		goto done;
	}

	switch (ver) {
	case GK20A_GPUID_GM20B:
	case GK20A_GPUID_GM20B_B:
			nvgpu_gm20b_lsfm_sw_init(g, *lsfm);
		break;
	case NVGPU_GPUID_GP10B:
	case NVGPU_GPUID_GV11B:
			nvgpu_gp10b_lsfm_sw_init(g, *lsfm);
		break;
#ifdef CONFIG_NVGPU_DGPU
	case NVGPU_GPUID_GV100:
			nvgpu_gv100_lsfm_sw_init(g, *lsfm);
		break;
	case NVGPU_GPUID_TU104:
			nvgpu_tu104_lsfm_sw_init(g, *lsfm);
		break;
#endif
#if defined(CONFIG_NVGPU_NON_FUSA)
	case NVGPU_GPUID_GA10B:
		nvgpu_gv100_lsfm_sw_init(g, *lsfm);
		break;
#endif
	default:
#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_NEXT)
		if (nvgpu_next_lsfm_sw_init(g, lsfm))
#endif
		{
			nvgpu_kfree(g, *lsfm);
			err = -ENODEV;
			nvgpu_err(g, "no support for GPUID %x", ver);
		}
		break;
	}

done:
	return err;
}

void nvgpu_pmu_lsfm_deinit(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_lsfm *lsfm)
{
	if (is_lsfm_supported(g, pmu, lsfm)) {
		nvgpu_kfree(g, lsfm);
	}
	pmu->lsfm = NULL;
}
