/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/types.h>
#include <nvgpu/firmware.h>
#include <nvgpu/falcon.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmu/fw.h>

#include "acr_wpr.h"
#include "acr_priv.h"
#include "acr_sw_gm20b.h"
#include "acr_blob_alloc.h"
#include "acr_bootstrap.h"
#include "acr_blob_construct_v0.h"

static int gm20b_bootstrap_hs_acr(struct gk20a *g, struct nvgpu_acr *acr)
{
	int err = 0;

	(void)acr;

	nvgpu_log_fn(g, " ");

	err = nvgpu_acr_bootstrap_hs_ucode(g, g->acr, &g->acr->acr);
	if (err != 0) {
		nvgpu_err(g, "ACR bootstrap failed");
	}

	return err;
}

static int gm20b_acr_patch_wpr_info_to_ucode(struct gk20a *g,
	struct nvgpu_acr *acr, struct hs_acr *acr_desc, bool is_recovery)
{
	struct nvgpu_firmware *acr_fw = acr_desc->acr_fw;
	struct acr_fw_header *acr_fw_hdr = NULL;
	struct bin_hdr *acr_fw_bin_hdr = NULL;
	struct flcn_acr_desc_v0 *acr_dmem_desc;
	u32 *acr_ucode_header = NULL;
	u32 *acr_ucode_data = NULL;

	(void)acr;

	nvgpu_log_fn(g, " ");

	if (is_recovery) {
		acr_desc->acr_dmem_desc_v0->nonwpr_ucode_blob_size = 0U;
	} else {
		acr_fw_bin_hdr = (struct bin_hdr *)acr_fw->data;
		acr_fw_hdr = (struct acr_fw_header *)
			(acr_fw->data + acr_fw_bin_hdr->header_offset);

		acr_ucode_data = (u32 *)(acr_fw->data +
			acr_fw_bin_hdr->data_offset);

		acr_ucode_header = (u32 *)(acr_fw->data +
			acr_fw_hdr->hdr_offset);

		/* Patch WPR info to ucode */
		acr_dmem_desc = (struct flcn_acr_desc_v0 *)
			&(((u8 *)acr_ucode_data)[acr_ucode_header[2U]]);

		acr_desc->acr_dmem_desc_v0 = acr_dmem_desc;

		acr_dmem_desc->nonwpr_ucode_blob_start =
			nvgpu_mem_get_addr(g, &g->acr->ucode_blob);
		nvgpu_assert(g->acr->ucode_blob.size <= U32_MAX);
		acr_dmem_desc->nonwpr_ucode_blob_size =
			(u32)g->acr->ucode_blob.size;
		acr_dmem_desc->regions.no_regions = 1U;
		acr_dmem_desc->wpr_offset = 0U;
	}

	return 0;
}

/* LSF static config functions */
static u32 gm20b_acr_lsf_pmu(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	(void)g;
	/* PMU LS falcon info */
	lsf->falcon_id = FALCON_ID_PMU;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;
	lsf->is_lazy_bootstrap = false;
	lsf->is_priv_load = false;
#ifdef CONFIG_NVGPU_LS_PMU
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_pmu_ucode_details_v0;
	lsf->get_cmd_line_args_offset = nvgpu_pmu_fw_get_cmd_line_args_offset;
#endif
	return BIT32(lsf->falcon_id);
}

static u32 gm20b_acr_lsf_fecs(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	(void)g;
	/* FECS LS falcon info */
	lsf->falcon_id = FALCON_ID_FECS;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;
	lsf->is_lazy_bootstrap = false;
	lsf->is_priv_load = false;
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_fecs_ucode_details_v0;
	lsf->get_cmd_line_args_offset = NULL;

	return BIT32(lsf->falcon_id);
}

static u32 gm20b_acr_lsf_conifg(struct gk20a *g,
	struct nvgpu_acr *acr)
{
	u32 lsf_enable_mask = 0;

	lsf_enable_mask |= gm20b_acr_lsf_pmu(g, &acr->lsf[FALCON_ID_PMU]);
	lsf_enable_mask |= gm20b_acr_lsf_fecs(g, &acr->lsf[FALCON_ID_FECS]);

	return lsf_enable_mask;
}

static void gm20b_acr_default_sw_init(struct gk20a *g, struct hs_acr *hs_acr)
{
	nvgpu_log_fn(g, " ");

	/* ACR HS ucode type & f/w name*/
	hs_acr->acr_type = ACR_DEFAULT;

	if (!g->ops.pmu.is_debug_mode_enabled(g)) {
		hs_acr->acr_fw_name = GM20B_HSBIN_ACR_PROD_UCODE;
	} else {
		hs_acr->acr_fw_name = GM20B_HSBIN_ACR_DBG_UCODE;
	}

	/* set on which falcon ACR need to execute*/
	hs_acr->acr_flcn = g->pmu->flcn;
	hs_acr->acr_engine_bus_err_status =
		g->ops.pmu.bar0_error_status;
}

void nvgpu_gm20b_acr_sw_init(struct gk20a *g, struct nvgpu_acr *acr)
{
	nvgpu_log_fn(g, " ");

	acr->g = g;

	acr->bootstrap_owner = FALCON_ID_PMU;

	acr->lsf_enable_mask = gm20b_acr_lsf_conifg(g, acr);

	gm20b_acr_default_sw_init(g, &acr->acr);

	acr->prepare_ucode_blob = nvgpu_acr_prepare_ucode_blob_v0;
	acr->get_wpr_info = nvgpu_acr_wpr_info_sys;
	acr->alloc_blob_space = nvgpu_acr_alloc_blob_space_sys;
	acr->bootstrap_hs_acr = gm20b_bootstrap_hs_acr;
	acr->patch_wpr_info_to_ucode =
		gm20b_acr_patch_wpr_info_to_ucode;
}
