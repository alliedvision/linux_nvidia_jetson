/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef ACR_BLOB_CONSTRUCT_H
#define ACR_BLOB_CONSTRUCT_H

#include <nvgpu/falcon.h>
#include <nvgpu/flcnif_cmn.h>
#include <nvgpu/pmu.h>

#include "nvgpu_acr_interface.h"

#define UCODE_NB_MAX_DATE_LENGTH  64U
struct ls_falcon_ucode_desc {
	u32 descriptor_size;
	u32 image_size;
	u32 tools_version;
	u32 app_version;
	char date[UCODE_NB_MAX_DATE_LENGTH];
	u32 bootloader_start_offset;
	u32 bootloader_size;
	u32 bootloader_imem_offset;
	u32 bootloader_entry_point;
	u32 app_start_offset;
	u32 app_size;
	u32 app_imem_offset;
	u32 app_imem_entry;
	u32 app_dmem_offset;
	u32 app_resident_code_offset;
	u32 app_resident_code_size;
	u32 app_resident_data_offset;
	u32 app_resident_data_size;
	u32 nb_imem_overlays;
	u32 nb_dmem_overlays;
	struct {u32 start; u32 size; } load_ovl[UCODE_NB_MAX_DATE_LENGTH];
	u32 compressed;
};

struct ls_falcon_ucode_desc_v1 {
	u32 descriptor_size;
	u32 image_size;
	u32 tools_version;
	u32 app_version;
	char date[UCODE_NB_MAX_DATE_LENGTH];
	u32 secure_bootloader;
	u32 bootloader_start_offset;
	u32 bootloader_size;
	u32 bootloader_imem_offset;
	u32 bootloader_entry_point;
	u32 app_start_offset;
	u32 app_size;
	u32 app_imem_offset;
	u32 app_imem_entry;
	u32 app_dmem_offset;
	u32 app_resident_code_offset;
	u32 app_resident_code_size;
	u32 app_resident_data_offset;
	u32 app_resident_data_size;
	u32 nb_imem_overlays;
	u32 nb_dmem_overlays;
	struct {u32 start; u32 size; } load_ovl[64];
	u32 compressed;
};

struct flcn_ucode_img {
	u32 *data;
	struct ls_falcon_ucode_desc *desc;
	u32 data_size;
	struct lsf_ucode_desc *lsf_desc;
	bool is_next_core_img;
	struct lsf_ucode_desc_wrapper *lsf_desc_wrapper;
#ifdef CONFIG_NVGPU_LS_PMU
	struct falcon_next_core_ucode_desc *ndesc;
#endif
};

struct lsfm_managed_ucode_img {
	struct lsfm_managed_ucode_img *next;
	struct lsf_wpr_header wpr_header;
	struct lsf_lsb_header lsb_header;
	struct lsf_lsb_header_v2 lsb_header_v2;
	struct flcn_bl_dmem_desc bl_gen_desc;
	u32 bl_gen_desc_size;
	u32 full_ucode_size;
	struct flcn_ucode_img ucode_img;
};

#ifdef CONFIG_NVGPU_DGPU
/*
 * LSF shared SubWpr Header
 *
 * use_case_id - Shared SubWpr use case ID (updated by nvgpu)
 * start_addr  - start address of subWpr (updated by nvgpu)
 * size_4K     - size of subWpr in 4K (updated by nvgpu)
 */
struct lsf_shared_sub_wpr_header {
	u32 use_case_id;
	u32 start_addr;
	u32 size_4K;
};

/*
 * LSFM SUB WPRs struct
 * pnext          : Next entry in the list, NULL if last
 * sub_wpr_header : SubWpr Header struct
 */
struct lsfm_sub_wpr {
	struct lsfm_sub_wpr *pnext;
	struct lsf_shared_sub_wpr_header sub_wpr_header;
};
#endif

struct ls_flcn_mgr {
	u16 managed_flcn_cnt;
	u32 wpr_size;
	struct lsfm_managed_ucode_img *ucode_img_list;
#ifdef CONFIG_NVGPU_DGPU
	u16 managed_sub_wpr_count;
	struct lsfm_sub_wpr *psub_wpr_list;
#endif
};

int nvgpu_acr_prepare_ucode_blob(struct gk20a *g);
#ifdef CONFIG_NVGPU_LS_PMU
int nvgpu_acr_lsf_pmu_ucode_details(struct gk20a *g, void *lsf_ucode_img);
s32 nvgpu_acr_lsf_pmu_ncore_ucode_details(struct gk20a *g, void *lsf_ucode_img);
#endif
int nvgpu_acr_lsf_fecs_ucode_details(struct gk20a *g, void *lsf_ucode_img);
int nvgpu_acr_lsf_gpccs_ucode_details(struct gk20a *g, void *lsf_ucode_img);
#ifdef CONFIG_NVGPU_DGPU
int nvgpu_acr_lsf_sec2_ucode_details(struct gk20a *g, void *lsf_ucode_img);
#endif

#endif /* ACR_BLOB_CONSTRUCT_H */
