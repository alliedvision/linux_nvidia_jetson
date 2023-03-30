/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include "acr_sw_tu104.h"

#include <nvgpu/gk20a.h>
#include <nvgpu/firmware.h>

#include "acr_wpr.h"
#include "acr_priv.h"
#include "acr_blob_alloc.h"
#include "acr_bootstrap.h"
#include "acr_blob_construct.h"
#include "acr_sw_gv11b.h"
#include "acr_sw_tu104.h"

static int tu104_bootstrap_hs_acr(struct gk20a *g, struct nvgpu_acr *acr)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	err = nvgpu_acr_bootstrap_hs_ucode(g, g->acr, &g->acr->acr_ahesasc);
	if (err != 0) {
		nvgpu_err(g, "ACR AHESASC bootstrap failed");
		goto exit;
	}
	err = nvgpu_acr_bootstrap_hs_ucode(g, g->acr, &g->acr->acr_asb);
	if (err != 0) {
		nvgpu_err(g, "ACR ASB bootstrap failed");
		goto exit;
	}

exit:
	return err;
}

/* WPR info update */
static int tu104_acr_patch_wpr_info_to_ucode(struct gk20a *g,
	struct nvgpu_acr *acr, struct hs_acr *acr_desc,
	bool is_recovery)
{
	struct nvgpu_firmware *acr_fw = acr_desc->acr_fw;
	struct acr_fw_header *acr_fw_hdr = NULL;
	struct bin_hdr *acr_fw_bin_hdr = NULL;
	struct flcn_acr_desc *acr_dmem_desc;
	struct wpr_carveout_info wpr_inf;
	u32 *acr_ucode_header = NULL;
	u32 *acr_ucode_data = NULL;
	u64 tmp_addr;

	nvgpu_log_fn(g, " ");

	acr_fw_bin_hdr = (struct bin_hdr *)acr_fw->data;
	acr_fw_hdr = (struct acr_fw_header *)
		(acr_fw->data + acr_fw_bin_hdr->header_offset);

	acr_ucode_data = (u32 *)(acr_fw->data + acr_fw_bin_hdr->data_offset);
	acr_ucode_header = (u32 *)(acr_fw->data + acr_fw_hdr->hdr_offset);

	acr->get_wpr_info(g, &wpr_inf);

	acr_dmem_desc = (struct flcn_acr_desc *)
		&(((u8 *)acr_ucode_data)[acr_ucode_header[2U]]);

	acr_dmem_desc->nonwpr_ucode_blob_start = wpr_inf.nonwpr_base;
	nvgpu_assert(wpr_inf.size <= U32_MAX);
	acr_dmem_desc->nonwpr_ucode_blob_size = (u32)wpr_inf.size;
	acr_dmem_desc->regions.no_regions = 1U;
	acr_dmem_desc->wpr_offset = 0U;

	acr_dmem_desc->wpr_region_id = 1U;
	acr_dmem_desc->regions.region_props[0U].region_id = 1U;

	tmp_addr = (wpr_inf.wpr_base) >> 8U;
	nvgpu_assert(u64_hi32(tmp_addr) == 0U);
	acr_dmem_desc->regions.region_props[0U].start_addr = U32(tmp_addr);

	tmp_addr = ((wpr_inf.wpr_base) + wpr_inf.size) >> 8U;
	nvgpu_assert(u64_hi32(tmp_addr) == 0U);
	acr_dmem_desc->regions.region_props[0U].end_addr = U32(tmp_addr);

	tmp_addr = wpr_inf.nonwpr_base >> 8U;
	nvgpu_assert(u64_hi32(tmp_addr) == 0U);
	acr_dmem_desc->regions.region_props[0U].shadowmMem_startaddress =
		U32(tmp_addr);

	return 0;
}

/* LSF init */
static u32 tu104_acr_lsf_sec2(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	/* SEC2 LS falcon info */
	lsf->falcon_id = FALCON_ID_SEC2;
	lsf->falcon_dma_idx = NV_SEC2_DMAIDX_UCODE;
	lsf->is_lazy_bootstrap = false;
	lsf->is_priv_load = false;
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_sec2_ucode_details;
	lsf->get_cmd_line_args_offset = NULL;

	return BIT32(lsf->falcon_id);
}

static u32 tu104_acr_lsf_pmu(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	/* PMU support not required until PSTATE support is enabled */
	if (!g->support_ls_pmu) {
		/* skip adding LS PMU ucode to ACR blob */
		return 0;
	}

	/* PMU LS falcon info */
	lsf->falcon_id = FALCON_ID_PMU;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;
	lsf->is_lazy_bootstrap = false;
	lsf->is_priv_load = false;
#ifdef CONFIG_NVGPU_LS_PMU
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_pmu_ucode_details;
	lsf->get_cmd_line_args_offset = nvgpu_pmu_fw_get_cmd_line_args_offset;
#endif
	return BIT32(lsf->falcon_id);
}

static u32 tu104_acr_lsf_fecs(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	/* FECS LS falcon info */
	lsf->falcon_id = FALCON_ID_FECS;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;
	lsf->is_lazy_bootstrap = true;
	lsf->is_priv_load = true;
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_fecs_ucode_details;
	lsf->get_cmd_line_args_offset = NULL;

	return BIT32(lsf->falcon_id);
}

static u32 tu104_acr_lsf_gpccs(struct gk20a *g,
		struct acr_lsf_config *lsf)
{
	/* FECS LS falcon info */
	lsf->falcon_id = FALCON_ID_GPCCS;
	lsf->falcon_dma_idx = GK20A_PMU_DMAIDX_UCODE;
	lsf->is_lazy_bootstrap = true;
	lsf->is_priv_load = true;
	lsf->get_lsf_ucode_details = nvgpu_acr_lsf_gpccs_ucode_details;
	lsf->get_cmd_line_args_offset = NULL;

	return BIT32(lsf->falcon_id);
}

static u32 tu104_acr_lsf_conifg(struct gk20a *g,
	struct nvgpu_acr *acr)
{
	u32 lsf_enable_mask = 0;
	lsf_enable_mask |= tu104_acr_lsf_pmu(g, &acr->lsf[FALCON_ID_PMU]);
	lsf_enable_mask |= tu104_acr_lsf_fecs(g, &acr->lsf[FALCON_ID_FECS]);
	lsf_enable_mask |= tu104_acr_lsf_gpccs(g, &acr->lsf[FALCON_ID_GPCCS]);
	lsf_enable_mask |= tu104_acr_lsf_sec2(g, &acr->lsf[FALCON_ID_SEC2]);

	return lsf_enable_mask;
}

/* fusa signing enable check */
static bool tu104_acr_is_fusa_enabled(struct gk20a *g)
{
	return g->is_fusa_sku;
}

/* ACR-AHESASC(ACR hub encryption setter and signature checker) init*/
static void tu104_acr_ahesasc_v0_ucode_select(struct gk20a *g,
		struct hs_acr *acr_ahesasc)
{
	acr_ahesasc->acr_type = ACR_AHESASC_NON_FUSA;

	if (!g->ops.pmu.is_debug_mode_enabled(g)) {
		acr_ahesasc->acr_fw_name = HSBIN_ACR_AHESASC_NON_FUSA_PROD_UCODE;
	} else {
		acr_ahesasc->acr_fw_name = HSBIN_ACR_AHESASC_NON_FUSA_DBG_UCODE;
	}

}

static void tu104_acr_ahesasc_fusa_ucode_select(struct gk20a *g,
		struct hs_acr *acr_ahesasc)
{
	acr_ahesasc->acr_type = ACR_AHESASC_FUSA;

	if (!g->ops.pmu.is_debug_mode_enabled(g)) {
		acr_ahesasc->acr_fw_name = HSBIN_ACR_AHESASC_FUSA_PROD_UCODE;
	} else {
		acr_ahesasc->acr_fw_name = HSBIN_ACR_AHESASC_FUSA_DBG_UCODE;
	}
}

static void tu104_acr_ahesasc_sw_init(struct gk20a *g,
	struct hs_acr *acr_ahesasc)
{
	if (tu104_acr_is_fusa_enabled(g)) {
		tu104_acr_ahesasc_fusa_ucode_select(g, acr_ahesasc);
	} else {
		tu104_acr_ahesasc_v0_ucode_select(g, acr_ahesasc);
	}

	acr_ahesasc->acr_flcn = &g->sec2.flcn;
}

/* ACR-ASB(ACR SEC2 booter) init*/
static void tu104_acr_asb_v0_ucode_select(struct gk20a *g,
	struct hs_acr *acr_asb)
{
	acr_asb->acr_type = ACR_ASB_NON_FUSA;

	if (!g->ops.pmu.is_debug_mode_enabled(g)) {
		acr_asb->acr_fw_name = HSBIN_ACR_ASB_NON_FUSA_PROD_UCODE;
	} else {
		acr_asb->acr_fw_name = HSBIN_ACR_ASB_NON_FUSA_DBG_UCODE;
	}
}

static void tu104_acr_asb_fusa_ucode_select(struct gk20a *g,
	struct hs_acr *acr_asb)
{
	acr_asb->acr_type = ACR_ASB_FUSA;

	if (!g->ops.pmu.is_debug_mode_enabled(g)) {
		acr_asb->acr_fw_name = HSBIN_ACR_ASB_FUSA_PROD_UCODE;
	} else {
		acr_asb->acr_fw_name = HSBIN_ACR_ASB_FUSA_DBG_UCODE;
	}
}

static void tu104_acr_asb_sw_init(struct gk20a *g,
	struct hs_acr *acr_asb)
{
	if (tu104_acr_is_fusa_enabled(g)) {
		tu104_acr_asb_fusa_ucode_select(g, acr_asb);
	} else {
		tu104_acr_asb_v0_ucode_select(g, acr_asb);
	}

	acr_asb->acr_flcn = &g->gsp_flcn;
}

void nvgpu_tu104_acr_sw_init(struct gk20a *g, struct nvgpu_acr *acr)
{
	nvgpu_log_fn(g, " ");

	acr->lsf_enable_mask = tu104_acr_lsf_conifg(g, acr);

	acr->prepare_ucode_blob = nvgpu_acr_prepare_ucode_blob;
	acr->get_wpr_info = nvgpu_acr_wpr_info_vid;
	acr->alloc_blob_space = nvgpu_acr_alloc_blob_space_vid;
	acr->bootstrap_owner = FALCON_ID_GSPLITE;
	acr->bootstrap_hs_acr = tu104_bootstrap_hs_acr;
	acr->patch_wpr_info_to_ucode = tu104_acr_patch_wpr_info_to_ucode;

	/* Init ACR-AHESASC */
	tu104_acr_ahesasc_sw_init(g, &acr->acr_ahesasc);

	/* Init ACR-ASB*/
	tu104_acr_asb_sw_init(g, &acr->acr_asb);
}
