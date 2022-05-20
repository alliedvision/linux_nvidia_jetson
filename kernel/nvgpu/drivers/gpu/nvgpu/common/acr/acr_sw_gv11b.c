/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/fw.h>
#endif

#include "acr_wpr.h"
#include "acr_priv.h"
#include "acr_blob_alloc.h"
#include "acr_blob_construct.h"
#include "acr_bootstrap.h"
#include "acr_sw_gv11b.h"

#define RECOVERY_UCODE_BLOB_SIZE	(0U)
#define WPR_OFFSET			(0U)
#define ACR_REGIONS			(1U)

static int gv11b_bootstrap_hs_acr(struct gk20a *g, struct nvgpu_acr *acr)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	err = nvgpu_acr_bootstrap_hs_ucode(g, g->acr, &g->acr->acr);
	if (err != 0) {
		nvgpu_err(g, "ACR bootstrap failed");
	}

	return err;
}

static int gv11b_acr_patch_wpr_info_to_ucode(struct gk20a *g,
	struct nvgpu_acr *acr, struct hs_acr *acr_desc, bool is_recovery)
{
	struct nvgpu_firmware *acr_fw = acr_desc->acr_fw;
	struct acr_fw_header *acr_fw_hdr = NULL;
	struct bin_hdr *acr_fw_bin_hdr = NULL;
	struct flcn_acr_desc *acr_dmem_desc;
	u32 *acr_ucode_header = NULL;
	u32 *acr_ucode_data = NULL;
	const u32 acr_desc_offset = 2U;

	nvgpu_log_fn(g, " ");
#ifdef CONFIG_NVGPU_NON_FUSA
	if (is_recovery) {
		acr_desc->acr_dmem_desc->nonwpr_ucode_blob_size =
						RECOVERY_UCODE_BLOB_SIZE;
	} else
#endif
	{
		acr_fw_bin_hdr = (struct bin_hdr *)(void *)acr_fw->data;
		acr_fw_hdr = (struct acr_fw_header *)(void *)
			(acr_fw->data + acr_fw_bin_hdr->header_offset);

		acr_ucode_data = (u32 *)(void *)(acr_fw->data +
			acr_fw_bin_hdr->data_offset);
		acr_ucode_header = (u32 *)(void *)(acr_fw->data +
			acr_fw_hdr->hdr_offset);

		/* Patch WPR info to ucode */
		acr_dmem_desc = (struct flcn_acr_desc *)(void *)
			&(((u8 *)acr_ucode_data)[acr_ucode_header[acr_desc_offset]]);

		acr_desc->acr_dmem_desc = acr_dmem_desc;

		acr_dmem_desc->nonwpr_ucode_blob_start =
			nvgpu_mem_get_addr(g, &g->acr->ucode_blob);
		nvgpu_assert(g->acr->ucode_blob.size <= U32_MAX);
		acr_dmem_desc->nonwpr_ucode_blob_size =
			(u32)g->acr->ucode_blob.size;
		acr_dmem_desc->regions.no_regions = ACR_REGIONS;
		acr_dmem_desc->wpr_offset = WPR_OFFSET;
	}

	return 0;
}

/* LSF static config functions */
#ifdef CONFIG_NVGPU_LS_PMU
static u32 gv11b_acr_lsf_pmu(struct gk20a *g,
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
#endif

/* LSF init */
static u32 gv11b_acr_lsf_fecs(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	/* FECS LS falcon info */
	lsf->falcon_id = FALCON_ID_FECS;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;
	/*
	 * FECS LSF cold/recovery bootstrap is handled by ACR when LS PMU
	 * not present
	 */
	lsf->is_lazy_bootstrap = g->support_ls_pmu ? true : false;
	lsf->is_priv_load = false;
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_fecs_ucode_details;
	lsf->get_cmd_line_args_offset = NULL;

	return BIT32(lsf->falcon_id);
}

static u32 gv11b_acr_lsf_gpccs(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	/* GPCCS LS falcon info */
	lsf->falcon_id = FALCON_ID_GPCCS;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;
	/*
	 * GPCCS LSF cold/recovery bootstrap is handled by ACR when LS PMU
	 * not present
	 */
	lsf->is_lazy_bootstrap = g->support_ls_pmu ? true : false;
	lsf->is_priv_load = true;
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_gpccs_ucode_details;
	lsf->get_cmd_line_args_offset = NULL;

	return BIT32(lsf->falcon_id);
}

u32 gv11b_acr_lsf_config(struct gk20a *g,
	struct nvgpu_acr *acr)
{
	u32 lsf_enable_mask = 0;
#ifdef CONFIG_NVGPU_LS_PMU
	lsf_enable_mask |= gv11b_acr_lsf_pmu(g, &acr->lsf[FALCON_ID_PMU]);
#endif
	lsf_enable_mask |= gv11b_acr_lsf_fecs(g, &acr->lsf[FALCON_ID_FECS]);
	lsf_enable_mask |= gv11b_acr_lsf_gpccs(g, &acr->lsf[FALCON_ID_GPCCS]);

	return lsf_enable_mask;
}

static void gv11b_acr_default_sw_init(struct gk20a *g, struct hs_acr *acr_desc)
{
	nvgpu_log_fn(g, " ");

	acr_desc->acr_type = ACR_DEFAULT;

	if (!g->ops.pmu.is_debug_mode_enabled(g)) {
		acr_desc->acr_fw_name = HSBIN_ACR_PROD_UCODE;
	} else {
		acr_desc->acr_fw_name = HSBIN_ACR_DBG_UCODE;
	}

	acr_desc->acr_flcn = g->pmu->flcn;
	acr_desc->report_acr_engine_bus_err_status =
		nvgpu_pmu_report_bar0_pri_err_status;
	acr_desc->acr_engine_bus_err_status =
		g->ops.pmu.bar0_error_status;
	acr_desc->acr_validate_mem_integrity = g->ops.pmu.validate_mem_integrity;
}

void nvgpu_gv11b_acr_sw_init(struct gk20a *g, struct nvgpu_acr *acr)
{
	nvgpu_log_fn(g, " ");

	acr->g = g;

	acr->bootstrap_owner = FALCON_ID_PMU;

	acr->lsf_enable_mask = gv11b_acr_lsf_config(g, acr);

	gv11b_acr_default_sw_init(g, &acr->acr);

	acr->prepare_ucode_blob = nvgpu_acr_prepare_ucode_blob;
	acr->get_wpr_info = nvgpu_acr_wpr_info_sys;
	acr->alloc_blob_space = nvgpu_acr_alloc_blob_space_sys;
	acr->bootstrap_hs_acr = gv11b_bootstrap_hs_acr;
	acr->patch_wpr_info_to_ucode = gv11b_acr_patch_wpr_info_to_ucode;
}
