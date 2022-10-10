/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/firmware.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/dma.h>
#include <nvgpu/pmu.h>
#include <nvgpu/grmgr.h>
#include <nvgpu/soc.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/fw.h>
#endif

#include "common/acr/acr_wpr.h"
#include "common/acr/acr_priv.h"
#include "common/acr/acr_blob_alloc.h"
#include "common/acr/acr_blob_construct.h"
#include "common/acr/acr_bootstrap.h"
#include "common/acr/acr_sw_gv11b.h"
#include "acr_sw_ga10b.h"

#define RECOVERY_UCODE_BLOB_SIZE      (0U)
#define WPR_OFFSET                    (0U)

#define GSPDBG_RISCV_ACR_FW_MANIFEST  "acr-gsp.manifest.encrypt.bin.out.bin"
#define GSPDBG_RISCV_ACR_FW_CODE      "acr-gsp.text.encrypt.bin"
#define GSPDBG_RISCV_ACR_FW_DATA      "acr-gsp.data.encrypt.bin"

#define GSPPROD_RISCV_ACR_FW_MANIFEST "acr-gsp.manifest.encrypt.bin.out.bin.prod"
#define GSPPROD_RISCV_ACR_FW_CODE     "acr-gsp.text.encrypt.bin.prod"
#define GSPPROD_RISCV_ACR_FW_DATA     "acr-gsp.data.encrypt.bin.prod"

#define GSPDBG_RISCV_ACR_FW_SAFETY_MANIFEST  "acr-gsp-safety.manifest.encrypt.bin.out.bin"
#define GSPDBG_RISCV_ACR_FW_SAFETY_CODE      "acr-gsp-safety.text.encrypt.bin"
#define GSPDBG_RISCV_ACR_FW_SAFETY_DATA      "acr-gsp-safety.data.encrypt.bin"

#define GSPPROD_RISCV_ACR_FW_SAFETY_MANIFEST "acr-gsp-safety.manifest.encrypt.bin.out.bin.prod"
#define GSPPROD_RISCV_ACR_FW_SAFETY_CODE     "acr-gsp-safety.text.encrypt.bin.prod"
#define GSPPROD_RISCV_ACR_FW_SAFETY_DATA     "acr-gsp-safety.data.encrypt.bin.prod"

static int ga10b_bootstrap_hs_acr(struct gk20a *g, struct nvgpu_acr *acr)
{
	int err = 0;

	(void)acr;

	nvgpu_log_fn(g, " ");

	err = nvgpu_acr_bootstrap_hs_ucode_riscv(g, g->acr);
	if (err != 0) {
		nvgpu_err(g, "ACR bootstrap failed");
	}

	return err;
}

static int ga10b_acr_patch_wpr_info_to_ucode(struct gk20a *g,
	struct nvgpu_acr *acr, struct hs_acr *acr_desc, bool is_recovery)
{
	int err = 0;
#ifdef CONFIG_NVGPU_LS_PMU
	struct nvgpu_mem *ls_pmu_desc = &acr_desc->ls_pmu_desc;
	struct nvgpu_firmware *fw_desc;
#endif
	struct nvgpu_mem *acr_falcon2_sysmem_desc =
					&acr_desc->acr_falcon2_sysmem_desc;
	struct flcn2_acr_desc *acr_sysmem_desc = &acr_desc->acr_sysmem_desc;

	(void)acr;
	(void)is_recovery;

	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_NON_FUSA
	if (is_recovery) {
		/*
		 * In case of recovery ucode blob size is 0 as it has already
		 * been authenticated during cold boot.
		 */
		if (!nvgpu_mem_is_valid(&acr_desc->acr_falcon2_sysmem_desc)) {
			nvgpu_err(g, "invalid mem acr_falcon2_sysmem_desc");
			return -EINVAL;
                }
		acr_sysmem_desc->nonwpr_ucode_blob_size =
						RECOVERY_UCODE_BLOB_SIZE;
	} else
#endif
        {
		/*
		 * Alloc space for  sys mem space to which interface struct is
		 * copied.
		 */
		if (!nvgpu_mem_is_valid(acr_falcon2_sysmem_desc)) {
			err = nvgpu_dma_alloc_flags_sys(g,
				NVGPU_DMA_PHYSICALLY_ADDRESSED,
				sizeof(struct flcn2_acr_desc),
				acr_falcon2_sysmem_desc);
			if (err != 0) {
				nvgpu_err(g, "alloc for sysmem desc failed");
				goto end;
			}
		} else {
			acr_sysmem_desc->nonwpr_ucode_blob_size =
						RECOVERY_UCODE_BLOB_SIZE;
			goto load;
		}

#ifdef CONFIG_NVGPU_LS_PMU
		if(g->support_ls_pmu &&
			nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
			err = nvgpu_dma_alloc_flags_sys(g,
				NVGPU_DMA_PHYSICALLY_ADDRESSED,
				sizeof(struct falcon_next_core_ucode_desc),
				ls_pmu_desc);
			if (err != 0) {
				goto end;
			}

			fw_desc = nvgpu_pmu_fw_desc_desc(g, g->pmu);

			nvgpu_mem_wr_n(g, ls_pmu_desc, 0U,
				fw_desc->data,
				sizeof(struct falcon_next_core_ucode_desc));

			acr_sysmem_desc->ls_pmu_desc =
					nvgpu_mem_get_addr(g, ls_pmu_desc);
		}
#endif
		/*
		 * Start address of non wpr sysmem region holding ucode blob.
		 */
		acr_sysmem_desc->nonwpr_ucode_blob_start =
			nvgpu_mem_get_addr(g, &g->acr->ucode_blob);
		/*
		 * LS ucode blob size.
		 */
		nvgpu_assert(g->acr->ucode_blob.size <= U32_MAX);
		acr_sysmem_desc->nonwpr_ucode_blob_size =
			(u32)g->acr->ucode_blob.size;
		/*
		 * Max regions to be used by acr. Cannot be 0U.
		 */
		acr_sysmem_desc->regions.no_regions = 1U;
		/*
		 * Offset from the WPR region holding the wpr header
		 */
		acr_sysmem_desc->wpr_offset = WPR_OFFSET;

		if (nvgpu_is_enabled(g, NVGPU_SUPPORT_EMULATE_MODE) &&
				(g->emulate_mode < EMULATE_MODE_MAX_CONFIG)) {
			acr_sysmem_desc->gpu_mode &= (~EMULATE_MODE_MASK);
			acr_sysmem_desc->gpu_mode |= g->emulate_mode;
		} else {
			acr_sysmem_desc->gpu_mode &= (~EMULATE_MODE_MASK);
		}

		if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
			acr_sysmem_desc->gpu_mode |= MIG_MODE;
		} else {
			acr_sysmem_desc->gpu_mode &= (u32)(~MIG_MODE);
		}

		if (nvgpu_platform_is_simulation(g)) {
			acr_sysmem_desc->gpu_mode |= ACR_SIMULATION_MODE;
		} else {
			acr_sysmem_desc->gpu_mode &= (u32)(~ACR_SIMULATION_MODE);
		}
	}

load:
	/*
	 * Push the acr descriptor data to sysmem.
	 */
	nvgpu_mem_wr_n(g, acr_falcon2_sysmem_desc, 0U,
				acr_sysmem_desc,
					sizeof(struct flcn2_acr_desc));

end:
	return err;
}

/* LSF static config functions */
#ifdef CONFIG_NVGPU_LS_PMU
static u32 ga10b_acr_lsf_pmu(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	if (!g->support_ls_pmu) {
		/* skip adding LS PMU ucode to ACR blob */
		return 0;
	}

	/* PMU LS falcon info */
	lsf->falcon_id = FALCON_ID_PMU;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;
	lsf->is_lazy_bootstrap = false;
	lsf->is_priv_load = false;
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_pmu_ucode_details;
	lsf->get_cmd_line_args_offset = nvgpu_pmu_fw_get_cmd_line_args_offset;

	return BIT32(lsf->falcon_id);
}

static u32 ga10b_acr_lsf_pmu_next_core(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	nvgpu_log_fn(g, " ");

	if (!g->support_ls_pmu) {
		/* skip adding LS PMU ucode to ACR blob */
		return 0;
	}

	/* PMU LS falcon info */
	lsf->falcon_id = FALCON_ID_PMU_NEXT_CORE;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;
	lsf->is_lazy_bootstrap = false;
	lsf->is_priv_load = false;
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_pmu_ncore_ucode_details;
	lsf->get_cmd_line_args_offset = NULL;

	return BIT32(lsf->falcon_id);
}
#endif

/* LSF init */
static u32 ga10b_acr_lsf_fecs(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	/* FECS LS falcon info */
	lsf->falcon_id = FALCON_ID_FECS;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;

	/*
	 * Lazy bootstrap is a secure iGPU feature where LS falcons(FECS and
	 * GPCCS) are bootstrapped by LSPMU in both cold boot and recovery boot.
	 * As there is no ACR running after boot, we need LSPMU to bootstrap LS
	 * falcons to support recovery.
	 * In absence of LSPMU, ACR will bootstrap LS falcons but recovery is
	 * not supported.
	 */
	lsf->is_lazy_bootstrap = g->support_ls_pmu ? true : false;
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		lsf->is_priv_load = true;
	} else {
		lsf->is_priv_load = false;
	}
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_fecs_ucode_details;
	lsf->get_cmd_line_args_offset = NULL;

	return BIT32(lsf->falcon_id);
}

static u32 ga10b_acr_lsf_gpccs(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	/* GPCCS LS falcon info */
	lsf->falcon_id = FALCON_ID_GPCCS;

	/*
	 * Lazy bootstrap is a secure iGPU feature where LS falcons(FECS and
	 * GPCCS) are bootstrapped by LSPMU in both cold boot and recovery boot.
	 * As there is no ACR running after boot, we need LSPMU to bootstrap LS
	 * falcons to support recovery.
	 * In absence of LSPMU, ACR will bootstrap LS falcons but recovery is
	 * not supported.
	 */
	lsf->is_lazy_bootstrap = g->support_ls_pmu ? true : false;
	lsf->is_priv_load = true;
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_gpccs_ucode_details;
	lsf->get_cmd_line_args_offset = NULL;

	return BIT32(lsf->falcon_id);
}

static u32 ga10b_acr_lsf_config(struct gk20a *g,
	struct nvgpu_acr *acr)
{
	u32 lsf_enable_mask = 0U;
#ifdef CONFIG_NVGPU_LS_PMU
	if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
		lsf_enable_mask |= ga10b_acr_lsf_pmu_next_core(g,
				&acr->lsf[FALCON_ID_PMU_NEXT_CORE]);
	} else {
		lsf_enable_mask |= ga10b_acr_lsf_pmu(g, &acr->lsf[FALCON_ID_PMU]);
	}
#endif
	lsf_enable_mask |= ga10b_acr_lsf_fecs(g, &acr->lsf[FALCON_ID_FECS]);
	lsf_enable_mask |= ga10b_acr_lsf_gpccs(g, &acr->lsf[FALCON_ID_GPCCS]);

	return lsf_enable_mask;
}

#ifndef CONFIG_NVGPU_NON_FUSA
static void ga10b_acr_safety_ucode_select(struct gk20a *g,
		struct hs_acr *riscv_hs)
{
	if (g->ops.pmu.is_debug_mode_enabled(g)) {
		riscv_hs->acr_code_name = GSPDBG_RISCV_ACR_FW_SAFETY_CODE;
		riscv_hs->acr_data_name = GSPDBG_RISCV_ACR_FW_SAFETY_DATA;
		riscv_hs->acr_manifest_name = GSPDBG_RISCV_ACR_FW_SAFETY_MANIFEST;
	} else {
		riscv_hs->acr_code_name = GSPPROD_RISCV_ACR_FW_SAFETY_CODE;
		riscv_hs->acr_data_name = GSPPROD_RISCV_ACR_FW_SAFETY_DATA;
		riscv_hs->acr_manifest_name = GSPPROD_RISCV_ACR_FW_SAFETY_MANIFEST;
	}
}
#else

static void ga10b_acr_non_safety_ucode_select(struct gk20a *g,
		struct hs_acr *riscv_hs)
{
	if (g->ops.pmu.is_debug_mode_enabled(g)) {
		riscv_hs->acr_code_name = GSPDBG_RISCV_ACR_FW_CODE;
		riscv_hs->acr_data_name = GSPDBG_RISCV_ACR_FW_DATA;
		riscv_hs->acr_manifest_name = GSPDBG_RISCV_ACR_FW_MANIFEST;
	} else {
		riscv_hs->acr_code_name = GSPPROD_RISCV_ACR_FW_CODE;
		riscv_hs->acr_data_name = GSPPROD_RISCV_ACR_FW_DATA;
		riscv_hs->acr_manifest_name = GSPPROD_RISCV_ACR_FW_MANIFEST;
	}
}
#endif

static void ga10b_acr_default_sw_init(struct gk20a *g, struct hs_acr *riscv_hs)
{
	nvgpu_log_fn(g, " ");

	riscv_hs->acr_type = ACR_DEFAULT;

#ifndef CONFIG_NVGPU_NON_FUSA
	ga10b_acr_safety_ucode_select(g, riscv_hs);
#else
	ga10b_acr_non_safety_ucode_select(g, riscv_hs);
#endif

	riscv_hs->acr_flcn = &g->gsp_flcn;
	riscv_hs->report_acr_engine_bus_err_status =
		nvgpu_pmu_report_bar0_pri_err_status;
	riscv_hs->acr_engine_bus_err_status =
		g->ops.pmu.bar0_error_status;
	riscv_hs->acr_validate_mem_integrity = g->ops.gsp.validate_mem_integrity;
}

static void ga10b_acr_sw_init(struct gk20a *g, struct nvgpu_acr *acr)
{
	nvgpu_log_fn(g, " ");

	acr->g = g;

	acr->bootstrap_owner = FALCON_ID_GSPLITE;

	acr->lsf_enable_mask = ga10b_acr_lsf_config(g, acr);

	ga10b_acr_default_sw_init(g, &acr->acr_asc);

	acr->prepare_ucode_blob = nvgpu_acr_prepare_ucode_blob;
	acr->get_wpr_info = nvgpu_acr_wpr_info_sys;
	acr->alloc_blob_space = nvgpu_acr_alloc_blob_space_sys;
	acr->bootstrap_hs_acr = ga10b_bootstrap_hs_acr;
	acr->patch_wpr_info_to_ucode = ga10b_acr_patch_wpr_info_to_ucode;
}


void nvgpu_ga10b_acr_sw_init(struct gk20a *g, struct nvgpu_acr *acr)
{
	nvgpu_log_fn(g, " ");
	acr->g = g;

	if (nvgpu_falcon_is_falcon2_enabled(&g->gsp_flcn)) {
		nvgpu_set_enabled(g, NVGPU_ACR_NEXT_CORE_ENABLED, true);
		nvgpu_set_enabled(g, NVGPU_PKC_LS_SIG_ENABLED, true);
		nvgpu_acr_dbg(g, "enabling PKC and next core for GSP/n");
	}

	/* TODO: Make it generic for PMU/GSP */
	if (nvgpu_is_enabled(g, NVGPU_ACR_NEXT_CORE_ENABLED)) {
		nvgpu_acr_dbg(g, "Booting RISCV core in Peregrine");
		ga10b_acr_sw_init(g, acr);
	} else {
		nvgpu_acr_dbg(g, "Booting Falcon core in Peregrine");
		nvgpu_gv11b_acr_sw_init(g, g->acr);
		acr->lsf_enable_mask = ga10b_acr_lsf_config(g, acr);
	}
}
