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

#include <nvgpu/firmware.h>
#include <nvgpu/pmu.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/string.h>
#include <nvgpu/bug.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/gr/gr_utils.h>

#include "nvgpu_acr_interface.h"
#include "acr_blob_construct.h"
#include "acr_wpr.h"
#include "acr_priv.h"

#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_NEXT)
#include <nvgpu_next_firmware.h>
#endif

#define APP_IMEM_OFFSET			(0)
#define APP_IMEM_ENTRY			(0)
#define APP_DMEM_OFFSET			(0)
#define APP_RESIDENT_CODE_OFFSET	(0)
#define MEMSET_VALUE			(0)
#define LSB_HDR_DATA_SIZE		(0)
#define BL_START_OFFSET			(0)

#if defined(CONFIG_NVGPU_DGPU) || defined(CONFIG_NVGPU_LS_PMU)
#define UCODE_PARAMS			(1)
#define UCODE_DESC_TOOL_VERSION		0x4U
#else
#define UCODE_PARAMS			(0)
#endif

#ifdef CONFIG_NVGPU_LS_PMU
#if defined(CONFIG_NVGPU_NON_FUSA)
#define PMU_NVRISCV_WPR_RSVD_BYTES	(0x8000)
#endif

int nvgpu_acr_lsf_pmu_ucode_details(struct gk20a *g, void *lsf_ucode_img)
{
	struct lsf_ucode_desc *lsf_desc;
	struct nvgpu_firmware *fw_sig;
	struct nvgpu_firmware *fw_desc;
	struct nvgpu_firmware *fw_image;
	struct flcn_ucode_img *p_img =
		(struct flcn_ucode_img *)lsf_ucode_img;
	struct ls_falcon_ucode_desc_v1 tmp_desc_v1;
	int err = 0;

	lsf_desc = nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc));
	if (lsf_desc == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	fw_sig = nvgpu_pmu_fw_sig_desc(g, g->pmu);
	fw_desc = nvgpu_pmu_fw_desc_desc(g, g->pmu);
	fw_image = nvgpu_pmu_fw_image_desc(g, g->pmu);

	nvgpu_memcpy((u8 *)lsf_desc, (u8 *)fw_sig->data,
		min_t(size_t, sizeof(*lsf_desc), fw_sig->size));

	lsf_desc->falcon_id = FALCON_ID_PMU;

	p_img->desc = (struct ls_falcon_ucode_desc *)(void *)fw_desc->data;
	if (p_img->desc->tools_version >= UCODE_DESC_TOOL_VERSION) {
		(void) memset((u8 *)&tmp_desc_v1, 0,
				sizeof(struct ls_falcon_ucode_desc_v1));

		nvgpu_memcpy((u8 *)&tmp_desc_v1, (u8 *)fw_desc->data,
			sizeof(struct ls_falcon_ucode_desc_v1));

		nvgpu_memcpy((u8 *)&p_img->desc->bootloader_start_offset,
			(u8 *)&tmp_desc_v1.bootloader_start_offset,
			sizeof(struct ls_falcon_ucode_desc) -
			offsetof(struct ls_falcon_ucode_desc,
			bootloader_start_offset));
	}

	p_img->data = (u32 *)(void *)fw_image->data;
	p_img->data_size = p_img->desc->app_start_offset + p_img->desc->app_size;
	p_img->lsf_desc = (struct lsf_ucode_desc *)lsf_desc;

exit:
	return err;
}

s32 nvgpu_acr_lsf_pmu_ncore_ucode_details(struct gk20a *g, void *lsf_ucode_img)
{
	struct lsf_ucode_desc *lsf_desc = NULL;
	struct lsf_ucode_desc_wrapper *lsf_desc_wrapper = NULL;
	struct nvgpu_firmware *fw_sig;
	struct nvgpu_firmware *fw_desc;
	struct nvgpu_firmware *fw_image;
	struct flcn_ucode_img *p_img =
		(struct flcn_ucode_img *)lsf_ucode_img;
	s32 err = 0;

	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		lsf_desc = nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc));
		if (lsf_desc == NULL) {
			err = -ENOMEM;
			goto exit;
		}
	} else {
		lsf_desc_wrapper =
			nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc_wrapper));
		if (lsf_desc_wrapper == NULL) {
			err = -ENOMEM;
			goto exit;
		}
	}

	fw_sig = nvgpu_pmu_fw_sig_desc(g, g->pmu);
	fw_desc = nvgpu_pmu_fw_desc_desc(g, g->pmu);
	fw_image = nvgpu_pmu_fw_image_desc(g, g->pmu);

	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		nvgpu_memcpy((u8 *)lsf_desc, (u8 *)fw_sig->data,
			min_t(size_t, sizeof(*lsf_desc), fw_sig->size));
		lsf_desc->falcon_id = FALCON_ID_PMU_NEXT_CORE;
	} else {
		nvgpu_memcpy((u8 *)lsf_desc_wrapper, (u8 *)fw_sig->data,
			min_t(size_t, sizeof(*lsf_desc_wrapper), fw_sig->size));
		lsf_desc_wrapper->lsf_ucode_desc_v2.falcon_id = FALCON_ID_PMU_NEXT_CORE;
	}

	p_img->ndesc = (struct falcon_next_core_ucode_desc *)(void *)fw_desc->data;

	p_img->data = (u32 *)(void *)fw_image->data;
	p_img->data_size = U32(fw_image->size);
	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		p_img->lsf_desc = (struct lsf_ucode_desc *)lsf_desc;
	} else {
		p_img->lsf_desc_wrapper =
			(struct lsf_ucode_desc_wrapper *)lsf_desc_wrapper;
	}

	p_img->is_next_core_img = true;

exit:
	return err;
}
#endif

int nvgpu_acr_lsf_fecs_ucode_details(struct gk20a *g, void *lsf_ucode_img)
{
	u32 tmp_size;
	u32 ver = nvgpu_safe_add_u32(g->params.gpu_arch,
					g->params.gpu_impl);
	struct lsf_ucode_desc *lsf_desc = NULL;
	struct lsf_ucode_desc_wrapper *lsf_desc_wrapper = NULL;
	struct nvgpu_firmware *fecs_sig = NULL;
	struct flcn_ucode_img *p_img =
		(struct flcn_ucode_img *)lsf_ucode_img;
	struct nvgpu_gr_falcon *gr_falcon = nvgpu_gr_get_falcon_ptr(g);
	struct nvgpu_ctxsw_ucode_segments *fecs =
			nvgpu_gr_falcon_get_fecs_ucode_segments(gr_falcon);
	int err;

	switch (ver) {
		case NVGPU_GPUID_GV11B:
			fecs_sig = nvgpu_request_firmware(g, GM20B_FECS_UCODE_SIG,
					g->acr->fw_load_flag);
			break;
		case NVGPU_GPUID_GA10B:
			if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
				fecs_sig = nvgpu_request_firmware(g,
					GM20B_FECS_UCODE_SIG,
					g->acr->fw_load_flag);
			} else {
				fecs_sig = nvgpu_request_firmware(g,
					GA10B_FECS_UCODE_PKC_SIG,
					g->acr->fw_load_flag);
			}
			break;
#ifdef CONFIG_NVGPU_DGPU
		case NVGPU_GPUID_TU104:
			fecs_sig = nvgpu_request_firmware(g, TU104_FECS_UCODE_SIG,
					g->acr->fw_load_flag);
			break;
#endif
#if defined(CONFIG_NVGPU_NON_FUSA)
		case NVGPU_GPUID_GA100:
			fecs_sig = nvgpu_request_firmware(g, GA100_FECS_UCODE_SIG,
					g->acr->fw_load_flag);
			break;
#endif

		default:
#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_NEXT)
			fecs_sig = nvgpu_next_request_fecs_firmware(g);
#endif
			break;
	}

	if (fecs_sig == NULL) {
		nvgpu_err(g, "failed to load fecs sig");
		return -ENOENT;
	}
	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		lsf_desc = nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc));
		if (lsf_desc == NULL) {
			err = -ENOMEM;
			goto rel_sig;
		}
		nvgpu_memcpy((u8 *)lsf_desc, (u8 *)fecs_sig->data,
			min_t(size_t, sizeof(*lsf_desc), fecs_sig->size));

		lsf_desc->falcon_id = FALCON_ID_FECS;
	} else {
		lsf_desc_wrapper =
			nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc_wrapper));
		if (lsf_desc_wrapper == NULL) {
			err = -ENOMEM;
			goto rel_sig;
		}
		nvgpu_memcpy((u8 *)lsf_desc_wrapper, (u8 *)fecs_sig->data,
			min_t(size_t, sizeof(*lsf_desc_wrapper), fecs_sig->size));

		lsf_desc_wrapper->lsf_ucode_desc_v2.falcon_id = FALCON_ID_FECS;
	}

	p_img->desc = nvgpu_kzalloc(g, sizeof(struct ls_falcon_ucode_desc));
	if (p_img->desc == NULL) {
		err = -ENOMEM;
		goto free_lsf_desc;
	}

	p_img->desc->bootloader_start_offset = fecs->boot.offset;
	p_img->desc->bootloader_size = NVGPU_ALIGN(fecs->boot.size,
						LSF_DATA_SIZE_ALIGNMENT);
	p_img->desc->bootloader_imem_offset = fecs->boot_imem_offset;
	p_img->desc->bootloader_entry_point = fecs->boot_entry;

	tmp_size = nvgpu_safe_add_u32(NVGPU_ALIGN(fecs->boot.size,
						LSF_DATA_SIZE_ALIGNMENT),
					NVGPU_ALIGN(fecs->code.size,
						LSF_DATA_SIZE_ALIGNMENT));
	p_img->desc->image_size = nvgpu_safe_add_u32(tmp_size,
						NVGPU_ALIGN(fecs->data.size,
						LSF_DATA_SIZE_ALIGNMENT));
	p_img->desc->app_size = nvgpu_safe_add_u32(NVGPU_ALIGN(fecs->code.size,
						LSF_DATA_SIZE_ALIGNMENT),
						NVGPU_ALIGN(fecs->data.size,
						LSF_DATA_SIZE_ALIGNMENT));
	p_img->desc->app_start_offset = fecs->code.offset;
	p_img->desc->app_imem_offset = APP_IMEM_OFFSET;
	p_img->desc->app_imem_entry = APP_IMEM_ENTRY;
	p_img->desc->app_dmem_offset = APP_DMEM_OFFSET;
	p_img->desc->app_resident_code_offset = APP_RESIDENT_CODE_OFFSET;
	p_img->desc->app_resident_code_size = fecs->code.size;
	p_img->desc->app_resident_data_offset =
		nvgpu_safe_sub_u32(fecs->data.offset, fecs->code.offset);
	p_img->desc->app_resident_data_size = fecs->data.size;
	p_img->data = nvgpu_gr_falcon_get_surface_desc_cpu_va(gr_falcon);
	p_img->data_size = p_img->desc->image_size;

	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		p_img->lsf_desc = (struct lsf_ucode_desc *)lsf_desc;
	} else {
		p_img->lsf_desc_wrapper =
			(struct lsf_ucode_desc_wrapper *)lsf_desc_wrapper;
	}

	nvgpu_acr_dbg(g, "fecs fw loaded\n");

	nvgpu_release_firmware(g, fecs_sig);

	return 0;
free_lsf_desc:
	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		nvgpu_kfree(g, lsf_desc);
	} else {
		nvgpu_kfree(g, lsf_desc_wrapper);
	}
rel_sig:
	nvgpu_release_firmware(g, fecs_sig);
	return err;
}

int nvgpu_acr_lsf_gpccs_ucode_details(struct gk20a *g, void *lsf_ucode_img)
{
	u32 tmp_size;
	u32 ver = nvgpu_safe_add_u32(g->params.gpu_arch, g->params.gpu_impl);
	struct lsf_ucode_desc *lsf_desc = NULL;
	struct lsf_ucode_desc_wrapper *lsf_desc_wrapper = NULL;
	struct nvgpu_firmware *gpccs_sig = NULL;
	struct flcn_ucode_img *p_img =
		(struct flcn_ucode_img *)lsf_ucode_img;
	struct nvgpu_gr_falcon *gr_falcon = nvgpu_gr_get_falcon_ptr(g);
	struct nvgpu_ctxsw_ucode_segments *gpccs =
			nvgpu_gr_falcon_get_gpccs_ucode_segments(gr_falcon);
	int err;

	if ((gpccs == NULL) || (gr_falcon == NULL)) {
		return -EINVAL;
	}

	if (!nvgpu_is_enabled(g, NVGPU_SEC_SECUREGPCCS)) {
		return -ENOENT;
	}

	switch (ver) {
		case NVGPU_GPUID_GV11B:
			gpccs_sig = nvgpu_request_firmware(g, T18x_GPCCS_UCODE_SIG,
					g->acr->fw_load_flag);
			break;
		case NVGPU_GPUID_GA10B:
			if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
				gpccs_sig = nvgpu_request_firmware(g,
					T18x_GPCCS_UCODE_SIG,
					g->acr->fw_load_flag);
			} else {
				gpccs_sig = nvgpu_request_firmware(g,
					GA10B_GPCCS_UCODE_PKC_SIG,
					g->acr->fw_load_flag);
			}
			break;
#ifdef CONFIG_NVGPU_DGPU
		case NVGPU_GPUID_TU104:
			gpccs_sig = nvgpu_request_firmware(g, TU104_GPCCS_UCODE_SIG,
					g->acr->fw_load_flag);
			break;
#endif
#if defined(CONFIG_NVGPU_NON_FUSA)
		case NVGPU_GPUID_GA100:
			gpccs_sig = nvgpu_request_firmware(g, GA100_GPCCS_UCODE_SIG,
					g->acr->fw_load_flag);
			break;
#endif

		default:
#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_NEXT)
			gpccs_sig = nvgpu_next_request_gpccs_firmware(g);
#endif
			break;
	}

	nvgpu_acr_dbg(g, "gpccs fw fetched from FS\n");
	if (gpccs_sig == NULL) {
		nvgpu_err(g, "failed to load gpccs sig");
		return -ENOENT;
	}
	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		lsf_desc = nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc));
		if (lsf_desc == NULL) {
			err = -ENOMEM;
			goto rel_sig;
		}
		nvgpu_memcpy((u8 *)lsf_desc, gpccs_sig->data,
			min_t(size_t, sizeof(*lsf_desc), gpccs_sig->size));
		lsf_desc->falcon_id = FALCON_ID_GPCCS;
	} else {
		lsf_desc_wrapper =
			nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc_wrapper));
		if (lsf_desc_wrapper == NULL) {
			err = -ENOMEM;
			goto rel_sig;
		}
		nvgpu_memcpy((u8 *)lsf_desc_wrapper, gpccs_sig->data,
			min_t(size_t, sizeof(*lsf_desc_wrapper), gpccs_sig->size));
		lsf_desc_wrapper->lsf_ucode_desc_v2.falcon_id = FALCON_ID_GPCCS;
	}

	nvgpu_acr_dbg(g, "gpccs fw copied to desc buffer\n");
	p_img->desc = nvgpu_kzalloc(g, sizeof(struct ls_falcon_ucode_desc));
	if (p_img->desc == NULL) {
		err = -ENOMEM;
		goto free_lsf_desc;
	}

	p_img->desc->bootloader_start_offset = BL_START_OFFSET;
	p_img->desc->bootloader_size = NVGPU_ALIGN(gpccs->boot.size,
						LSF_DATA_SIZE_ALIGNMENT);
	p_img->desc->bootloader_imem_offset = gpccs->boot_imem_offset;
	p_img->desc->bootloader_entry_point = gpccs->boot_entry;

	tmp_size = nvgpu_safe_add_u32(NVGPU_ALIGN(gpccs->boot.size,
						LSF_DATA_SIZE_ALIGNMENT),
					NVGPU_ALIGN(gpccs->code.size,
						LSF_DATA_SIZE_ALIGNMENT));

	p_img->desc->image_size = nvgpu_safe_add_u32(tmp_size,
						NVGPU_ALIGN(gpccs->data.size,
						LSF_DATA_SIZE_ALIGNMENT));
	p_img->desc->app_size =
			nvgpu_safe_add_u32(NVGPU_ALIGN(gpccs->code.size,
						LSF_DATA_SIZE_ALIGNMENT),
				NVGPU_ALIGN(gpccs->data.size,
						LSF_DATA_SIZE_ALIGNMENT));
	p_img->desc->app_start_offset = p_img->desc->bootloader_size;
	p_img->desc->app_imem_offset = APP_IMEM_OFFSET;
	p_img->desc->app_imem_entry = APP_IMEM_ENTRY;
	p_img->desc->app_dmem_offset = APP_DMEM_OFFSET;
	p_img->desc->app_resident_code_offset = APP_RESIDENT_CODE_OFFSET;
	p_img->desc->app_resident_code_size = NVGPU_ALIGN(gpccs->code.size,
						LSF_DATA_SIZE_ALIGNMENT);
	p_img->desc->app_resident_data_offset =
		nvgpu_safe_sub_u32(NVGPU_ALIGN(gpccs->data.offset,
						LSF_DATA_SIZE_ALIGNMENT),
			NVGPU_ALIGN(gpccs->code.offset,
					LSF_DATA_SIZE_ALIGNMENT));
	p_img->desc->app_resident_data_size = NVGPU_ALIGN(gpccs->data.size,
					LSF_DATA_SIZE_ALIGNMENT);
	p_img->data = (u32 *)
	(void *)((u8 *)nvgpu_gr_falcon_get_surface_desc_cpu_va(gr_falcon)
				+ gpccs->boot.offset);
	p_img->data_size = NVGPU_ALIGN(p_img->desc->image_size,
					LSF_DATA_SIZE_ALIGNMENT);

	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		p_img->lsf_desc = (struct lsf_ucode_desc *)lsf_desc;
	} else {
		p_img->lsf_desc_wrapper =
			(struct lsf_ucode_desc_wrapper *)lsf_desc_wrapper;
	}

	nvgpu_acr_dbg(g, "gpccs fw loaded\n");

	nvgpu_release_firmware(g, gpccs_sig);

	return 0;
free_lsf_desc:
	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		nvgpu_kfree(g, lsf_desc);
	} else {
		nvgpu_kfree(g, lsf_desc_wrapper);
	}
rel_sig:
	nvgpu_release_firmware(g, gpccs_sig);
	return err;
}

#ifdef CONFIG_NVGPU_DGPU
int nvgpu_acr_lsf_sec2_ucode_details(struct gk20a *g, void *lsf_ucode_img)
{
	struct nvgpu_firmware *sec2_fw, *sec2_desc, *sec2_sig;
	struct ls_falcon_ucode_desc *desc;
	struct lsf_ucode_desc *lsf_desc;
	struct flcn_ucode_img *p_img =
		(struct flcn_ucode_img *)lsf_ucode_img;
	u32 *ucode_image;
	int err = 0;

	nvgpu_acr_dbg(g, "requesting SEC2 ucode in %s", g->name);

	if (g->is_fusa_sku) {
		sec2_fw = nvgpu_request_firmware(g,
			LSF_SEC2_UCODE_IMAGE_FUSA_BIN,
			g->acr->fw_load_flag);
	} else {
		sec2_fw = nvgpu_request_firmware(g,
			LSF_SEC2_UCODE_IMAGE_BIN,
			g->acr->fw_load_flag);
	}

	if (sec2_fw == NULL) {
		nvgpu_err(g, "failed to load sec2 ucode!!");
		return -ENOENT;
	}

	ucode_image = (u32 *)sec2_fw->data;

	nvgpu_acr_dbg(g, "requesting SEC2 ucode desc in %s", g->name);
	if (g->is_fusa_sku) {
		sec2_desc = nvgpu_request_firmware(g,
			LSF_SEC2_UCODE_DESC_FUSA_BIN,
			g->acr->fw_load_flag);
	} else {
		sec2_desc = nvgpu_request_firmware(g,
			LSF_SEC2_UCODE_DESC_BIN,
			g->acr->fw_load_flag);
	}

	if (sec2_desc == NULL) {
		nvgpu_err(g, "failed to load SEC2 ucode desc!!");
		err = -ENOENT;
		goto release_img_fw;
	}

	desc = (struct ls_falcon_ucode_desc *)sec2_desc->data;

	if (g->is_fusa_sku) {
		sec2_sig = nvgpu_request_firmware(g,
			LSF_SEC2_UCODE_SIG_FUSA_BIN,
			g->acr->fw_load_flag);
	} else {
		sec2_sig = nvgpu_request_firmware(g,
			LSF_SEC2_UCODE_SIG_BIN,
			g->acr->fw_load_flag);
	}
	if (sec2_sig == NULL) {
		nvgpu_err(g, "failed to load SEC2 sig!!");
		err = -ENOENT;
		goto release_desc;
	}

	lsf_desc = nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc));
	if (lsf_desc == NULL) {
		err = -ENOMEM;
		goto release_sig;
	}

	nvgpu_memcpy((u8 *)lsf_desc, (u8 *)sec2_sig->data,
		min_t(size_t, sizeof(*lsf_desc), sec2_sig->size));

	lsf_desc->falcon_id = FALCON_ID_SEC2;

	p_img->desc = desc;
	p_img->data = ucode_image;
	p_img->data_size = desc->app_start_offset + desc->app_size;
	p_img->lsf_desc = (struct lsf_ucode_desc *)lsf_desc;

	g->sec2.fw.fw_image = sec2_fw;
	g->sec2.fw.fw_desc = sec2_desc;
	g->sec2.fw.fw_sig = sec2_sig;

	nvgpu_acr_dbg(g, "requesting SEC2 ucode in %s done", g->name);

	return err;
release_sig:
	nvgpu_release_firmware(g, sec2_sig);
release_desc:
	nvgpu_release_firmware(g, sec2_desc);
release_img_fw:
	nvgpu_release_firmware(g, sec2_fw);
	return err;
}
#endif

static void lsfm_fill_static_lsb_hdr_info_aes(struct gk20a *g,
	u32 falcon_id, struct lsfm_managed_ucode_img *pnode)
{
	u32 full_app_size = 0;
	u32 data = 0;

	if (pnode->ucode_img.lsf_desc != NULL) {
		nvgpu_memcpy((u8 *)&pnode->lsb_header.signature,
			(u8 *)pnode->ucode_img.lsf_desc,
			sizeof(struct lsf_ucode_desc));
	}

	pnode->lsb_header.ucode_size = pnode->ucode_img.data_size;

	/* Uses a loader. that is has a desc */
	pnode->lsb_header.data_size = LSB_HDR_DATA_SIZE;

	/*
	 * The loader code size is already aligned (padded) such that
	 * the code following it is aligned, but the size in the image
	 * desc is not, bloat it up to be on a 256 byte alignment.
	 */
	pnode->lsb_header.bl_code_size = NVGPU_ALIGN(
		pnode->ucode_img.desc->bootloader_size,
		LSF_BL_CODE_SIZE_ALIGNMENT);
	full_app_size = nvgpu_safe_add_u32(
			NVGPU_ALIGN(pnode->ucode_img.desc->app_size,
				LSF_BL_CODE_SIZE_ALIGNMENT),
				pnode->lsb_header.bl_code_size);

	pnode->lsb_header.ucode_size = nvgpu_safe_add_u32(NVGPU_ALIGN(
			pnode->ucode_img.desc->app_resident_data_offset,
			LSF_BL_CODE_SIZE_ALIGNMENT),
				pnode->lsb_header.bl_code_size);

	pnode->lsb_header.data_size = nvgpu_safe_sub_u32(full_app_size,
					pnode->lsb_header.ucode_size);
	/*
	 * Though the BL is located at 0th offset of the image, the VA
	 * is different to make sure that it doesn't collide the actual OS
	 * VA range
	 */
	pnode->lsb_header.bl_imem_off =
	pnode->ucode_img.desc->bootloader_imem_offset;

	pnode->lsb_header.flags = NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_FALSE;

	if (falcon_id == FALCON_ID_PMU) {
		data = NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_TRUE;
		pnode->lsb_header.flags = data;
	}

	if (g->acr->lsf[falcon_id].is_priv_load) {
		pnode->lsb_header.flags |=
			NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_TRUE;
	}

}

static void lsfm_fill_static_lsb_hdr_info_pkc(struct gk20a *g,
	u32 falcon_id, struct lsfm_managed_ucode_img *pnode)
{
	u32 full_app_size = 0;

	if (pnode->ucode_img.lsf_desc_wrapper != NULL) {
		nvgpu_memcpy((u8 *)&pnode->lsb_header_v2.signature,
			(u8 *)pnode->ucode_img.lsf_desc_wrapper,
			sizeof(struct lsf_ucode_desc_wrapper));
	}
	pnode->lsb_header_v2.ucode_size = pnode->ucode_img.data_size;
	pnode->lsb_header_v2.data_size = LSB_HDR_DATA_SIZE;

	pnode->lsb_header_v2.bl_code_size = NVGPU_ALIGN(
		pnode->ucode_img.desc->bootloader_size,
		LSF_BL_CODE_SIZE_ALIGNMENT);
	full_app_size = nvgpu_safe_add_u32(
			NVGPU_ALIGN(pnode->ucode_img.desc->app_size,
				LSF_BL_CODE_SIZE_ALIGNMENT),
				pnode->lsb_header_v2.bl_code_size);

	pnode->lsb_header_v2.ucode_size = nvgpu_safe_add_u32(NVGPU_ALIGN(
			pnode->ucode_img.desc->app_resident_data_offset,
			LSF_BL_CODE_SIZE_ALIGNMENT),
				pnode->lsb_header_v2.bl_code_size);

	pnode->lsb_header_v2.data_size = nvgpu_safe_sub_u32(full_app_size,
					pnode->lsb_header_v2.ucode_size);

	pnode->lsb_header_v2.bl_imem_off =
	pnode->ucode_img.desc->bootloader_imem_offset;

	pnode->lsb_header_v2.flags = NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_FALSE;

	if (g->acr->lsf[falcon_id].is_priv_load) {
		pnode->lsb_header_v2.flags |=
			NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_TRUE;
	}

}

/* Populate static LSB header information using the provided ucode image */
static void lsfm_fill_static_lsb_hdr_info(struct gk20a *g,
	u32 falcon_id, struct lsfm_managed_ucode_img *pnode)
{
#ifdef CONFIG_NVGPU_LS_PMU
	u32 base_size = 0;
	u32 image_padding_size = 0;
	struct falcon_next_core_ucode_desc *ndesc = pnode->ucode_img.ndesc;
#endif

	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)
			&& (!pnode->ucode_img.is_next_core_img)) {
		lsfm_fill_static_lsb_hdr_info_aes(g, falcon_id, pnode);
	} else if (nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)
			&& (!pnode->ucode_img.is_next_core_img)) {
		lsfm_fill_static_lsb_hdr_info_pkc(g, falcon_id, pnode);
	} else if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)
			&& (pnode->ucode_img.is_next_core_img)) {
		pnode->lsb_header.ucode_size = 0;
		pnode->lsb_header.data_size = 0;
		pnode->lsb_header.bl_code_size = 0;
		pnode->lsb_header.bl_imem_off = 0;
		pnode->lsb_header.bl_data_size = 0;
		pnode->lsb_header.bl_data_off = 0;
	} else {
#ifdef CONFIG_NVGPU_LS_PMU
		if (pnode->ucode_img.lsf_desc_wrapper != NULL) {
			nvgpu_memcpy((u8 *)&pnode->lsb_header_v2.signature,
				(u8 *)pnode->ucode_img.lsf_desc_wrapper,
				sizeof(struct lsf_ucode_desc_wrapper));
		}
		pnode->lsb_header_v2.ucode_size = ndesc->bootloader_offset +
						ndesc->bootloader_size +
						ndesc->bootloader_param_size;
		base_size = pnode->lsb_header_v2.ucode_size +
						ndesc->next_core_elf_size;
		image_padding_size = NVGPU_ALIGN(base_size,
					LSF_UCODE_DATA_ALIGNMENT) - base_size;
		pnode->lsb_header_v2.data_size = ndesc->next_core_elf_size +
						image_padding_size;
		pnode->lsb_header_v2.bl_code_size = 0;
		pnode->lsb_header_v2.bl_imem_off = 0;
		pnode->lsb_header_v2.bl_data_size = 0;
		pnode->lsb_header_v2.bl_data_off = 0;
#endif
	}
}

/* Adds a ucode image to the list of managed ucode images managed. */
static int lsfm_add_ucode_img(struct gk20a *g, struct ls_flcn_mgr *plsfm,
	struct flcn_ucode_img *ucode_image, u32 falcon_id)
{
	struct lsfm_managed_ucode_img *pnode;

	pnode = nvgpu_kzalloc(g, sizeof(struct lsfm_managed_ucode_img));
	if (pnode == NULL) {
		return -ENOMEM;
	}

	/* Keep a copy of the ucode image info locally */
	nvgpu_memcpy((u8 *)&pnode->ucode_img, (u8 *)ucode_image,
		sizeof(struct flcn_ucode_img));

	/* Fill in static WPR header info*/
	pnode->wpr_header.falcon_id = falcon_id;
	pnode->wpr_header.bootstrap_owner = g->acr->bootstrap_owner;
	pnode->wpr_header.status = LSF_IMAGE_STATUS_COPY;

	pnode->wpr_header.lazy_bootstrap =
		nvgpu_safe_cast_bool_to_u32(
				g->acr->lsf[falcon_id].is_lazy_bootstrap);

	/* Fill in static LSB header info elsewhere */
	lsfm_fill_static_lsb_hdr_info(g, falcon_id, pnode);
	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		pnode->wpr_header.bin_version =
			pnode->lsb_header.signature.version;
	} else {
		pnode->wpr_header.bin_version =
			pnode->lsb_header_v2.signature.lsf_ucode_desc_v2.ls_ucode_version;
	}
	pnode->next = plsfm->ucode_img_list;
	plsfm->ucode_img_list = pnode;

	return 0;
}

static int lsfm_check_and_add_ucode_image(struct gk20a *g,
		struct ls_flcn_mgr *plsfm, u32 lsf_index)
{
	struct flcn_ucode_img ucode_img;
	struct nvgpu_acr *acr = g->acr;
	u32 falcon_id = 0U;
	int err = 0;

	if (!nvgpu_test_bit(lsf_index, (void *)&acr->lsf_enable_mask)) {
		return err;
	}

	if (acr->lsf[lsf_index].get_lsf_ucode_details == NULL) {
		nvgpu_err(g, "LS falcon-%d ucode fetch details not initialized",
				lsf_index);
		return -ENOENT;
	}

	(void) memset(&ucode_img, MEMSET_VALUE, sizeof(ucode_img));

	err = acr->lsf[lsf_index].get_lsf_ucode_details(g,
			(void *)&ucode_img);
	if (err != 0) {
		nvgpu_err(g, "LS falcon-%d ucode get failed", lsf_index);
		return err;
	}

	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		falcon_id = ucode_img.lsf_desc->falcon_id;
	} else {
		falcon_id = ucode_img.lsf_desc_wrapper->lsf_ucode_desc_v2.falcon_id;
	}

	err = lsfm_add_ucode_img(g, plsfm, &ucode_img, falcon_id);
	if (err != 0) {
		nvgpu_err(g, " Failed to add falcon-%d to LSFM ", falcon_id);
		return err;
	}

	plsfm->managed_flcn_cnt++;

	return err;
}

/* Discover all managed falcon ucode images */
static int lsfm_discover_ucode_images(struct gk20a *g,
	struct ls_flcn_mgr *plsfm)
{
	u32 i;
	int err = 0;

#ifdef CONFIG_NVGPU_DGPU
	err = lsfm_check_and_add_ucode_image(g, plsfm, FALCON_ID_SEC2);
	if (err != 0) {
		return err;
	}
#endif
	/*
	 * Enumerate all constructed falcon objects, as we need the ucode
	 * image info and total falcon count
	 */
	for (i = 0U; i < FALCON_ID_END; i++) {
#ifdef CONFIG_NVGPU_DGPU
		if (i == FALCON_ID_SEC2) {
			continue;
		}
#endif
		err = lsfm_check_and_add_ucode_image(g, plsfm, i);
		if (err != 0) {
			return err;
		}
	}

	return err;
}

#ifdef CONFIG_NVGPU_DGPU
/* Discover all supported shared data falcon SUB WPRs */
static int lsfm_discover_and_add_sub_wprs(struct gk20a *g,
		struct ls_flcn_mgr *plsfm)
{
	struct lsfm_sub_wpr *pnode;
	u32 size_4K = 0;
	u32 sub_wpr_index;

	for (sub_wpr_index = 1;
		sub_wpr_index <= (u32) LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_MAX;
		sub_wpr_index++) {

		switch (sub_wpr_index) {
		case LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_PLAYREADY_SHARED_DATA:
			size_4K = LSF_SHARED_DATA_SUB_WPR_PLAYREADY_SHARED_DATA_SIZE_IN_4K;
			break;
		default:
			size_4K = 0; /* subWpr not supported */
			break;
		}

		if (size_4K != 0U) {
			pnode = nvgpu_kzalloc(g, sizeof(struct lsfm_sub_wpr));
			if (pnode == NULL) {
				return -ENOMEM;
			}

			pnode->sub_wpr_header.use_case_id = sub_wpr_index;
			pnode->sub_wpr_header.size_4K = size_4K;

			pnode->pnext = plsfm->psub_wpr_list;
			plsfm->psub_wpr_list = pnode;

			plsfm->managed_sub_wpr_count =
				nvgpu_safe_cast_u32_to_u16(nvgpu_safe_add_u32(
				plsfm->managed_sub_wpr_count, 1U));
		}
	}

	return 0;
}
#endif

static void lsf_calc_wpr_size_aes(struct lsfm_managed_ucode_img *pnode,
					u32 *wpr_off)
{
	u32 wpr_offset = *wpr_off;

	/* Align, save off and include an LSB header size */
	wpr_offset = NVGPU_ALIGN(wpr_offset, LSF_LSB_HEADER_ALIGNMENT);
	pnode->wpr_header.lsb_offset = wpr_offset;
	wpr_offset = nvgpu_safe_add_u32(wpr_offset,
				(u32)sizeof(struct lsf_lsb_header));

	/*
	 * Align, save off and include the original (static)ucode
	 * image size
	 */
	wpr_offset = NVGPU_ALIGN(wpr_offset, LSF_UCODE_DATA_ALIGNMENT);
	pnode->lsb_header.ucode_off = wpr_offset;
	wpr_offset = nvgpu_safe_add_u32(wpr_offset,
					pnode->ucode_img.data_size);

	/*
	 * For falcons that use a boot loader (BL), we append a loader
	 * desc structure on the end of the ucode image and consider this
	 * the boot loader data. The host will then copy the loader desc
	 * args to this space within the WPR region (before locking down)
	 * and the HS bin will then copy them to DMEM 0 for the loader.
	 */
	/*
	 * Track the size for LSB details filled in later
	 * Note that at this point we don't know what kind of
	 * boot loader desc, so we just take the size of the
	 * generic one, which is the largest it will ever be.
	 */
	/* Align (size bloat) and save off generic descriptor size*/
	pnode->lsb_header.bl_data_size = NVGPU_ALIGN(
		nvgpu_safe_cast_u64_to_u32(
				sizeof(pnode->bl_gen_desc)),
		LSF_BL_DATA_SIZE_ALIGNMENT);

	/*Align, save off, and include the additional BL data*/
	wpr_offset = NVGPU_ALIGN(wpr_offset, LSF_BL_DATA_ALIGNMENT);
	pnode->lsb_header.bl_data_off = wpr_offset;
	wpr_offset = nvgpu_safe_add_u32(wpr_offset,
				pnode->lsb_header.bl_data_size);

	/* Finally, update ucode surface size to include updates */
	pnode->full_ucode_size = wpr_offset -
		pnode->lsb_header.ucode_off;
	if (pnode->wpr_header.falcon_id != FALCON_ID_PMU &&
		pnode->wpr_header.falcon_id != FALCON_ID_PMU_NEXT_CORE) {
		pnode->lsb_header.app_code_off =
			pnode->lsb_header.bl_code_size;
		pnode->lsb_header.app_code_size =
			pnode->lsb_header.ucode_size -
			pnode->lsb_header.bl_code_size;
		pnode->lsb_header.app_data_off =
			pnode->lsb_header.ucode_size;
		pnode->lsb_header.app_data_size =
			pnode->lsb_header.data_size;
	}

	*wpr_off = wpr_offset;
}

static void lsf_calc_wpr_size_pkc(struct lsfm_managed_ucode_img *pnode,
					u32 *wpr_off)
{
	u32 wpr_offset = *wpr_off;

	/* Align, save off, and include an LSB header size */
	wpr_offset = NVGPU_ALIGN(wpr_offset, LSF_LSB_HEADER_ALIGNMENT);
	pnode->wpr_header.lsb_offset = wpr_offset;
	wpr_offset = nvgpu_safe_add_u32(wpr_offset,
				(u32)sizeof(struct lsf_lsb_header_v2));

	wpr_offset = NVGPU_ALIGN(wpr_offset, LSF_UCODE_DATA_ALIGNMENT);
	pnode->lsb_header_v2.ucode_off = wpr_offset;
	wpr_offset = nvgpu_safe_add_u32(wpr_offset,
					pnode->ucode_img.data_size);

	pnode->lsb_header_v2.bl_data_size = NVGPU_ALIGN(
		nvgpu_safe_cast_u64_to_u32(
				sizeof(pnode->bl_gen_desc)),
		LSF_BL_DATA_SIZE_ALIGNMENT);

	wpr_offset = NVGPU_ALIGN(wpr_offset, LSF_BL_DATA_ALIGNMENT);
	pnode->lsb_header_v2.bl_data_off = wpr_offset;
	wpr_offset = nvgpu_safe_add_u32(wpr_offset,
				pnode->lsb_header_v2.bl_data_size);

	pnode->full_ucode_size = wpr_offset -
		pnode->lsb_header_v2.ucode_off;
	if (pnode->wpr_header.falcon_id != FALCON_ID_PMU &&
		pnode->wpr_header.falcon_id != FALCON_ID_PMU_NEXT_CORE) {
		pnode->lsb_header_v2.app_code_off =
			pnode->lsb_header_v2.bl_code_size;
		pnode->lsb_header_v2.app_code_size =
			pnode->lsb_header_v2.ucode_size -
			pnode->lsb_header_v2.bl_code_size;
		pnode->lsb_header_v2.app_data_off =
			pnode->lsb_header_v2.ucode_size;
		pnode->lsb_header_v2.app_data_size =
			pnode->lsb_header_v2.data_size;
	}

	*wpr_off = wpr_offset;
}

/* Generate WPR requirements for ACR allocation request */
static int lsf_gen_wpr_requirements(struct gk20a *g,
		struct ls_flcn_mgr *plsfm)
{
	struct lsfm_managed_ucode_img *pnode = plsfm->ucode_img_list;
#ifdef CONFIG_NVGPU_DGPU
	struct lsfm_sub_wpr *pnode_sub_wpr = plsfm->psub_wpr_list;
	u32 sub_wpr_header;
#endif
	u32 wpr_offset;
	u32 flcn_cnt;

	(void)g;

	/*
	 * Start with an array of WPR headers at the base of the WPR.
	 * The expectation here is that the secure falcon will do a single DMA
	 * read of this array and cache it internally so it's OK to pack these.
	 * Also, we add 1 to the falcon count to indicate the end of the array.
	 */
	flcn_cnt = U32(plsfm->managed_flcn_cnt);
	wpr_offset = nvgpu_safe_mult_u32(U32(sizeof(struct lsf_wpr_header)),
		nvgpu_safe_add_u32(flcn_cnt, U32(1)));

#ifdef CONFIG_NVGPU_DGPU
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MULTIPLE_WPR)) {
		wpr_offset = ALIGN_UP(wpr_offset, LSF_WPR_HEADERS_TOTAL_SIZE_MAX);
		/*
		 * SUB WPR header is appended after lsf_wpr_header_v0 in WPR blob.
		 * The size is allocated as per the managed SUB WPR count.
		 */
		wpr_offset = ALIGN_UP(wpr_offset, LSF_SUB_WPR_HEADER_ALIGNMENT);
		sub_wpr_header = nvgpu_safe_mult_u32(
			U32(sizeof(struct lsf_shared_sub_wpr_header)),
			nvgpu_safe_add_u32(U32(plsfm->managed_sub_wpr_count),
						U32(1)));
		wpr_offset = nvgpu_safe_add_u32(wpr_offset, sub_wpr_header);
	}
#endif

	/*
	 * Walk the managed falcons, accounting for the LSB structs
	 * as well as the ucode images.
	 */
	while (pnode != NULL) {
		if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
			lsf_calc_wpr_size_aes(pnode, &wpr_offset);
		} else {
			lsf_calc_wpr_size_pkc(pnode, &wpr_offset);
		}
#if defined(CONFIG_NVGPU_NON_FUSA)
		/* Falcon image is cleanly partitioned between a code and
		 * data section where we don't need extra reserved space.
		 * NVRISCV image has no clear partition for code and data
		 * section, so need to reserve wpr space.
		 */
		if (pnode->wpr_header.falcon_id == FALCON_ID_PMU_NEXT_CORE) {
			wpr_offset = nvgpu_safe_add_u32(wpr_offset,
				(u32)PMU_NVRISCV_WPR_RSVD_BYTES);
		}
#endif
		pnode = pnode->next;
	}

#ifdef CONFIG_NVGPU_DGPU
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MULTIPLE_WPR)) {
		/*
		 * Walk through the sub wpr headers to accommodate
		 * sub wprs in WPR request
		 */
		while (pnode_sub_wpr != NULL) {
			wpr_offset = ALIGN_UP(wpr_offset,
					SUB_WPR_SIZE_ALIGNMENT);
			pnode_sub_wpr->sub_wpr_header.start_addr = wpr_offset;
			wpr_offset = wpr_offset +
				(pnode_sub_wpr->sub_wpr_header.size_4K
				<< SHIFT_4KB);
			pnode_sub_wpr = pnode_sub_wpr->pnext;
		}
		wpr_offset = ALIGN_UP(wpr_offset, SUB_WPR_SIZE_ALIGNMENT);
	}
#endif

	plsfm->wpr_size = wpr_offset;
	return 0;
}

/* Initialize WPR contents */
static int lsfm_populate_flcn_bl_dmem_desc(struct gk20a *g,
	void *lsfm, u32 *p_bl_gen_desc_size, u32 falconid)
{
	struct wpr_carveout_info wpr_inf;
	struct lsfm_managed_ucode_img *p_lsfm =
			(struct lsfm_managed_ucode_img *)lsfm;
	struct flcn_ucode_img *p_img = &(p_lsfm->ucode_img);
	struct flcn_bl_dmem_desc *ldr_cfg =
			&(p_lsfm->bl_gen_desc);
	u64 addr_base = 0;
	struct ls_falcon_ucode_desc *desc;
	u64 addr_code, addr_data;

	if (p_img->desc == NULL) {
		/*
		 * This means its a header based ucode,
		 * and so we do not fill BL gen desc structure
		 */
		return -EINVAL;
	}
	desc = p_img->desc;

	/*
	 * Calculate physical and virtual addresses for various portions of
	 * the PMU ucode image
	 * Calculate the 32-bit addresses for the application code, application
	 * data, and bootloader code. These values are all based on IM_BASE.
	 * The 32-bit addresses will be the upper 32-bits of the virtual or
	 * physical addresses of each respective segment.
	 */
	if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
		addr_base = p_lsfm->lsb_header.ucode_off;
	} else {
		addr_base = p_lsfm->lsb_header_v2.ucode_off;
	}
	g->acr->get_wpr_info(g, &wpr_inf);
	addr_base = nvgpu_safe_add_u64(addr_base, wpr_inf.wpr_base);

	nvgpu_acr_dbg(g, "falcon ID %x", p_lsfm->wpr_header.falcon_id);
	nvgpu_acr_dbg(g, "gen loader cfg addrbase %llx ", addr_base);
	addr_code = nvgpu_safe_add_u64(addr_base, desc->app_start_offset);
	addr_data = nvgpu_safe_add_u64(addr_code,
					desc->app_resident_data_offset);

	nvgpu_acr_dbg(g, "gen cfg addrcode %llx data %llx load offset %x",
			addr_code, addr_data, desc->bootloader_start_offset);

	/* Populate the LOADER_CONFIG state */
	(void) memset((void *) ldr_cfg, MEMSET_VALUE,
		sizeof(struct flcn_bl_dmem_desc));

	ldr_cfg->ctx_dma = g->acr->lsf[falconid].falcon_dma_idx;
	flcn64_set_dma(&ldr_cfg->code_dma_base, addr_code);
	ldr_cfg->non_sec_code_off = desc->app_resident_code_offset;
	ldr_cfg->non_sec_code_size = desc->app_resident_code_size;
	flcn64_set_dma(&ldr_cfg->data_dma_base, addr_data);
	ldr_cfg->data_size = desc->app_resident_data_size;
	ldr_cfg->code_entry_point = desc->app_imem_entry;


#if defined(CONFIG_NVGPU_DGPU) || defined(CONFIG_NVGPU_LS_PMU)
	/* Update the argc/argv members*/
	ldr_cfg->argc = UCODE_PARAMS;
	if (g->acr->lsf[falconid].get_cmd_line_args_offset != NULL) {
		g->acr->lsf[falconid].get_cmd_line_args_offset(g,
			&ldr_cfg->argv);
	}
#else
	/* Update the argc/argv members*/
	ldr_cfg->argc = UCODE_PARAMS;

#endif
	*p_bl_gen_desc_size = (u32)sizeof(struct flcn_bl_dmem_desc);
	return 0;
}

/* Populate falcon boot loader generic desc.*/
static int lsfm_fill_flcn_bl_gen_desc(struct gk20a *g,
		struct lsfm_managed_ucode_img *pnode)
{
	return lsfm_populate_flcn_bl_dmem_desc(g, pnode,
		&pnode->bl_gen_desc_size,
		pnode->wpr_header.falcon_id);
}

#ifdef CONFIG_NVGPU_DGPU
static void lsfm_init_sub_wpr_contents(struct gk20a *g,
	struct ls_flcn_mgr *plsfm, struct nvgpu_mem *ucode)
{
	struct lsfm_sub_wpr *psub_wpr_node;
	struct lsf_shared_sub_wpr_header last_sub_wpr_header;
	u32 temp_size = (u32)sizeof(struct lsf_shared_sub_wpr_header);
	u32 sub_wpr_header_offset = 0;
	u32 i = 0;

	/* SubWpr headers are placed after WPR headers */
	sub_wpr_header_offset = LSF_WPR_HEADERS_TOTAL_SIZE_MAX;

	/*
	 * Walk through the managed shared subWPRs headers
	 * and flush them to FB
	 */
	psub_wpr_node = plsfm->psub_wpr_list;
	i = 0;
	while (psub_wpr_node != NULL) {
		nvgpu_mem_wr_n(g, ucode,
			nvgpu_safe_add_u32(sub_wpr_header_offset,
			nvgpu_safe_mult_u32(i, temp_size)),
			&psub_wpr_node->sub_wpr_header, temp_size);

		psub_wpr_node = psub_wpr_node->pnext;
		i = nvgpu_safe_add_u32(i, 1U);
	}
	last_sub_wpr_header.use_case_id =
		LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_INVALID;
	nvgpu_mem_wr_n(g, ucode, nvgpu_safe_add_u32(sub_wpr_header_offset,
		nvgpu_safe_mult_u32(plsfm->managed_sub_wpr_count, temp_size)),
		&last_sub_wpr_header, temp_size);
}
#endif

static int lsfm_init_wpr_contents(struct gk20a *g,
		struct ls_flcn_mgr *plsfm, struct nvgpu_mem *ucode)
{
	struct lsfm_managed_ucode_img *pnode = plsfm->ucode_img_list;
	struct lsf_wpr_header last_wpr_hdr;
	u32 i = 0;
	u64 tmp;
	int err = 0;

	/* The WPR array is at the base of the WPR */
	pnode = plsfm->ucode_img_list;
	(void) memset(&last_wpr_hdr, MEMSET_VALUE, sizeof(struct lsf_wpr_header));

#ifdef CONFIG_NVGPU_DGPU
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MULTIPLE_WPR)) {
		lsfm_init_sub_wpr_contents(g, plsfm, ucode);
	}
#endif

	/*
	 * Walk the managed falcons, flush WPR and LSB headers to FB.
	 * flush any bl args to the storage area relative to the
	 * ucode image (appended on the end as a DMEM area).
	 */
	while (pnode != NULL) {
		/* Flush WPR header to memory*/
		nvgpu_mem_wr_n(g, ucode,
			nvgpu_safe_mult_u32(i,
				nvgpu_safe_cast_u64_to_u32(sizeof(
			pnode->wpr_header))), &pnode->wpr_header,
			nvgpu_safe_cast_u64_to_u32(sizeof(pnode->wpr_header)));

		nvgpu_acr_dbg(g, "wpr header");
		nvgpu_acr_dbg(g, "falconid :%d",
				pnode->wpr_header.falcon_id);
		nvgpu_acr_dbg(g, "lsb_offset :%x",
				pnode->wpr_header.lsb_offset);
		nvgpu_acr_dbg(g, "bootstrap_owner :%d",
			pnode->wpr_header.bootstrap_owner);
		nvgpu_acr_dbg(g, "lazy_bootstrap :%d",
				pnode->wpr_header.lazy_bootstrap);
		nvgpu_acr_dbg(g, "status :%d",
				pnode->wpr_header.status);

		/*Flush LSB header to memory*/
		if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
			nvgpu_mem_wr_n(g, ucode, pnode->wpr_header.lsb_offset,
				&pnode->lsb_header,
				nvgpu_safe_cast_u64_to_u32(
					sizeof(pnode->lsb_header)));
		} else {
			nvgpu_mem_wr_n(g, ucode, pnode->wpr_header.lsb_offset,
				&pnode->lsb_header_v2,
				nvgpu_safe_cast_u64_to_u32(
					sizeof(pnode->lsb_header_v2)));
		}

		nvgpu_acr_dbg(g, "lsb header");
		if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
			nvgpu_acr_dbg(g, "ucode_off :%x",
					pnode->lsb_header.ucode_off);
			nvgpu_acr_dbg(g, "ucode_size :%x",
					pnode->lsb_header.ucode_size);
			nvgpu_acr_dbg(g, "data_size :%x",
					pnode->lsb_header.data_size);
			nvgpu_acr_dbg(g, "bl_code_size :%x",
					pnode->lsb_header.bl_code_size);
			nvgpu_acr_dbg(g, "bl_imem_off :%x",
					pnode->lsb_header.bl_imem_off);
			nvgpu_acr_dbg(g, "bl_data_off :%x",
					pnode->lsb_header.bl_data_off);
			nvgpu_acr_dbg(g, "bl_data_size :%x",
					pnode->lsb_header.bl_data_size);
			nvgpu_acr_dbg(g, "app_code_off :%x",
					pnode->lsb_header.app_code_off);
			nvgpu_acr_dbg(g, "app_code_size :%x",
					pnode->lsb_header.app_code_size);
			nvgpu_acr_dbg(g, "app_data_off :%x",
					pnode->lsb_header.app_data_off);
			nvgpu_acr_dbg(g, "app_data_size :%x",
					pnode->lsb_header.app_data_size);
			nvgpu_acr_dbg(g, "flags :%x",
					pnode->lsb_header.flags);
		} else {
			nvgpu_acr_dbg(g, "ucode_off :%x",
					pnode->lsb_header_v2.ucode_off);
			nvgpu_acr_dbg(g, "ucode_size :%x",
					pnode->lsb_header_v2.ucode_size);
			nvgpu_acr_dbg(g, "data_size :%x",
					pnode->lsb_header_v2.data_size);
			nvgpu_acr_dbg(g, "bl_code_size :%x",
					pnode->lsb_header_v2.bl_code_size);
			nvgpu_acr_dbg(g, "bl_imem_off :%x",
					pnode->lsb_header_v2.bl_imem_off);
			nvgpu_acr_dbg(g, "bl_data_off :%x",
					pnode->lsb_header_v2.bl_data_off);
			nvgpu_acr_dbg(g, "bl_data_size :%x",
					pnode->lsb_header_v2.bl_data_size);
			nvgpu_acr_dbg(g, "app_code_off :%x",
					pnode->lsb_header_v2.app_code_off);
			nvgpu_acr_dbg(g, "app_code_size :%x",
					pnode->lsb_header_v2.app_code_size);
			nvgpu_acr_dbg(g, "app_data_off :%x",
					pnode->lsb_header_v2.app_data_off);
			nvgpu_acr_dbg(g, "app_data_size :%x",
					pnode->lsb_header_v2.app_data_size);
			nvgpu_acr_dbg(g, "flags :%x",
					pnode->lsb_header_v2.flags);
		}

		if (!pnode->ucode_img.is_next_core_img) {
			/*
			 * If this falcon has a boot loader and related args,
			 * flush them.
			 */
			/* Populate gen bl and flush to memory */
			err = lsfm_fill_flcn_bl_gen_desc(g, pnode);
			if (err != 0) {
				nvgpu_err(g, "bl_gen_desc failed err=%d", err);
				return err;
			}
			if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
				nvgpu_mem_wr_n(g, ucode,
					pnode->lsb_header.bl_data_off,
					&pnode->bl_gen_desc,
					pnode->bl_gen_desc_size);
			} else {
				nvgpu_mem_wr_n(g, ucode,
					pnode->lsb_header_v2.bl_data_off,
					&pnode->bl_gen_desc,
					pnode->bl_gen_desc_size);
			}
		}

		/* Copying of ucode */
		if (!nvgpu_is_enabled(g, NVGPU_PKC_LS_SIG_ENABLED)) {
			nvgpu_mem_wr_n(g, ucode, pnode->lsb_header.ucode_off,
				pnode->ucode_img.data,
				pnode->ucode_img.data_size);
		} else {
			nvgpu_mem_wr_n(g, ucode, pnode->lsb_header_v2.ucode_off,
				pnode->ucode_img.data,
				pnode->ucode_img.data_size);
		}

		pnode = pnode->next;
		i = nvgpu_safe_add_u32(i, 1U);
	}

	/* Tag the terminator WPR header with an invalid falcon ID. */
	last_wpr_hdr.falcon_id = FALCON_ID_INVALID;
	tmp = nvgpu_safe_mult_u32(plsfm->managed_flcn_cnt,
					(u32)sizeof(struct lsf_wpr_header));
	nvgpu_assert(tmp <= U32_MAX);
	nvgpu_mem_wr_n(g, ucode, (u32)tmp, &last_wpr_hdr,
		(u32)sizeof(struct lsf_wpr_header));

	return err;
}

/* Free any ucode image structure resources. */
static void lsfm_free_ucode_img_res(struct gk20a *g,
	struct flcn_ucode_img *p_img)
{
	if (p_img->lsf_desc != NULL) {
		nvgpu_kfree(g, p_img->lsf_desc);
		p_img->lsf_desc = NULL;
	}
	if (p_img->lsf_desc_wrapper != NULL) {
		nvgpu_kfree(g, p_img->lsf_desc_wrapper);
		p_img->lsf_desc_wrapper = NULL;
	}
}

static void lsfm_free_nonpmu_ucode_img_res(struct gk20a *g,
	struct flcn_ucode_img *p_img)
{
	if (p_img->lsf_desc != NULL) {
		nvgpu_kfree(g, p_img->lsf_desc);
		p_img->lsf_desc = NULL;
	}
	if (p_img->lsf_desc_wrapper != NULL) {
		nvgpu_kfree(g, p_img->lsf_desc_wrapper);
		p_img->lsf_desc_wrapper = NULL;
	}
	if (p_img->desc != NULL) {
		nvgpu_kfree(g, p_img->desc);
		p_img->desc = NULL;
	}
}

static void lsfm_free_sec2_ucode_img_res(struct gk20a *g,
	struct flcn_ucode_img *p_img)
{
	if (p_img->lsf_desc != NULL) {
		nvgpu_kfree(g, p_img->lsf_desc);
		p_img->lsf_desc = NULL;
	}
	p_img->data = NULL;
	p_img->desc = NULL;
}

static void free_acr_resources(struct gk20a *g, struct ls_flcn_mgr *plsfm)
{
	u32 cnt = plsfm->managed_flcn_cnt;
	struct lsfm_managed_ucode_img *mg_ucode_img;

	while (cnt != 0U) {
		mg_ucode_img = plsfm->ucode_img_list;
		if (mg_ucode_img->ucode_img.lsf_desc != NULL &&
			mg_ucode_img->ucode_img.lsf_desc->falcon_id == FALCON_ID_PMU) {
			lsfm_free_ucode_img_res(g, &mg_ucode_img->ucode_img);
		} else if (mg_ucode_img->ucode_img.lsf_desc != NULL &&
				mg_ucode_img->ucode_img.lsf_desc->falcon_id ==
				FALCON_ID_SEC2) {
			lsfm_free_sec2_ucode_img_res(g,
				&mg_ucode_img->ucode_img);
		} else {
			lsfm_free_nonpmu_ucode_img_res(g,
				&mg_ucode_img->ucode_img);
		}
		plsfm->ucode_img_list = mg_ucode_img->next;
		nvgpu_kfree(g, mg_ucode_img);
		cnt--;
	}
}

int nvgpu_acr_prepare_ucode_blob(struct gk20a *g)
{
	int err = 0;
	struct ls_flcn_mgr lsfm_l, *plsfm;

	struct wpr_carveout_info wpr_inf;
	struct nvgpu_gr_falcon *gr_falcon = nvgpu_gr_get_falcon_ptr(g);

	/* Recovery case, we do not need to form non WPR blob of ucodes */
	if (g->acr->ucode_blob.cpu_va != NULL) {
		return err;
	}


	plsfm = &lsfm_l;
	(void) memset((void *)plsfm, MEMSET_VALUE, sizeof(struct ls_flcn_mgr));
	err = nvgpu_gr_falcon_init_ctxsw_ucode(g, gr_falcon);
	if (err != 0) {
		nvgpu_err(g, "gr_falcon_init_ctxsw_ucode failed err=%d", err);
		return err;
	}

	g->acr->get_wpr_info(g, &wpr_inf);
	nvgpu_acr_dbg(g, "wpr carveout base:%llx\n", (wpr_inf.wpr_base));
	nvgpu_acr_dbg(g, "wpr carveout size :%llx\n", wpr_inf.size);

	/* Discover all managed falcons */
	err = lsfm_discover_ucode_images(g, plsfm);
	nvgpu_acr_dbg(g, " Managed Falcon cnt %d\n", plsfm->managed_flcn_cnt);
	if (err != 0) {
		goto cleanup_exit;
	}

#ifdef CONFIG_NVGPU_DGPU
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MULTIPLE_WPR)) {
		err = lsfm_discover_and_add_sub_wprs(g, plsfm);
		if (err != 0) {
			goto cleanup_exit;
		}
	}
#endif

	if ((plsfm->managed_flcn_cnt != 0U) &&
		(g->acr->ucode_blob.cpu_va == NULL)) {
		/* Generate WPR requirements */
		err = lsf_gen_wpr_requirements(g, plsfm);
		if (err != 0) {
			goto cleanup_exit;
		}

		/* Alloc memory to hold ucode blob contents */
		err = g->acr->alloc_blob_space(g, plsfm->wpr_size,
							&g->acr->ucode_blob);
		if (err != 0) {
			goto cleanup_exit;
		}

		nvgpu_acr_dbg(g, "managed LS falcon %d, WPR size %d bytes.\n",
			plsfm->managed_flcn_cnt, plsfm->wpr_size);

		err = lsfm_init_wpr_contents(g, plsfm, &g->acr->ucode_blob);
		if (err != 0) {
			nvgpu_kfree(g, &g->acr->ucode_blob);
			goto cleanup_exit;
		}
	} else {
		nvgpu_acr_dbg(g, "LSFM is managing no falcons.\n");
	}
	nvgpu_acr_dbg(g, "prepare ucode blob return 0\n");

cleanup_exit:
	free_acr_resources(g, plsfm);
	return err;
}
