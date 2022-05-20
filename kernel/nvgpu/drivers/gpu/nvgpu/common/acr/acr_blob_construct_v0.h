/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef ACR_BLOB_CONSTRUCT_V0_H
#define ACR_BLOB_CONSTRUCT_V0_H

#include <nvgpu/falcon.h>
#include <nvgpu/flcnif_cmn.h>

/*
 * Light Secure WPR Content Alignments
 */
#define LSF_WPR_HEADER_ALIGNMENT        (256U)
#define LSF_SUB_WPR_HEADER_ALIGNMENT    (256U)
#define LSF_LSB_HEADER_ALIGNMENT        (256U)
#define LSF_BL_DATA_ALIGNMENT           (256U)
#define LSF_BL_DATA_SIZE_ALIGNMENT      (256U)
#define LSF_BL_CODE_SIZE_ALIGNMENT      (256U)
#define LSF_DATA_SIZE_ALIGNMENT         (256U)
#define LSF_CODE_SIZE_ALIGNMENT         (256U)

#define LSF_UCODE_DATA_ALIGNMENT 4096U


/* Defined for 1MB alignment */
#define SHIFT_1MB	(20U)
#define SHIFT_4KB	(12U)

/*Light Secure Bootstrap header related defines*/
#define NV_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0_FALSE       0U
#define NV_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0_TRUE        BIT32(0)
#define NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_FALSE       0U
#define NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_TRUE        BIT32(2)
#define NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_TRUE       BIT32(3)
#define NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_FALSE      (0U)

/*
 * Image Status Defines
 */
#define LSF_IMAGE_STATUS_NONE                           (0U)
#define LSF_IMAGE_STATUS_COPY                           (1U)
#define LSF_IMAGE_STATUS_VALIDATION_CODE_FAILED         (2U)
#define LSF_IMAGE_STATUS_VALIDATION_DATA_FAILED         (3U)
#define LSF_IMAGE_STATUS_VALIDATION_DONE                (4U)
#define LSF_IMAGE_STATUS_VALIDATION_SKIPPED             (5U)
#define LSF_IMAGE_STATUS_BOOTSTRAP_READY                (6U)

/*
 * Light Secure WPR Header
 * Defines state allowing Light Secure Falcon bootstrapping.
 */
struct lsf_wpr_header_v0 {
	u32 falcon_id;
	u32 lsb_offset;
	u32 bootstrap_owner;
	u32 lazy_bootstrap;
	u32 status;
};

/*
 * Light Secure Falcon Ucode Description Defines
 * This structure is prelim and may change as the ucode signing flow evolves.
 */
struct lsf_ucode_desc_v0 {
	u8  prd_keys[2][16];
	u8  dbg_keys[2][16];
	u32 b_prd_present;
	u32 b_dbg_present;
	u32 falcon_id;
};

/*
 * Light Secure Bootstrap Header
 * Defines state allowing Light Secure Falcon bootstrapping.
 */
struct lsf_lsb_header_v0 {
	struct lsf_ucode_desc_v0 signature;
	u32 ucode_off;
	u32 ucode_size;
	u32 data_size;
	u32 bl_code_size;
	u32 bl_imem_off;
	u32 bl_data_off;
	u32 bl_data_size;
	u32 app_code_off;
	u32 app_code_size;
	u32 app_data_off;
	u32 app_data_size;
	u32 flags;
};

/*
 * Union of all supported structures used by bootloaders.
 */
/* Falcon BL interfaces */
/*
 * Structure used by the boot-loader to load the rest of the code. This has
 * to be filled by NVGPU and copied into DMEM at offset provided in the
 * hsflcn_bl_desc.bl_desc_dmem_load_off.
 */
struct flcn_bl_dmem_desc_v0 {
	u32    reserved[4];        /*Should be the first element..*/
	u32    signature[4];        /*Should be the first element..*/
	u32    ctx_dma;
	u32    code_dma_base;
	u32    non_sec_code_off;
	u32    non_sec_code_size;
	u32    sec_code_off;
	u32    sec_code_size;
	u32    code_entry_point;
	u32    data_dma_base;
	u32    data_size;
	u32    code_dma_base1;
	u32    data_dma_base1;
};

/*
 * Legacy structure used by the current PMU bootloader.
 */
struct loader_config {
	u32 dma_idx;
	u32 code_dma_base;     /* upper 32-bits of 40-bit dma address */
	u32 code_size_total;
	u32 code_size_to_load;
	u32 code_entry_point;
	u32 data_dma_base;     /* upper 32-bits of 40-bit dma address */
	u32 data_size;         /* initialized data of the application  */
	u32 overlay_dma_base;  /* upper 32-bits of the 40-bit dma address */
	u32 argc;
	u32 argv;
	u16 code_dma_base1;    /* upper 7 bits of 47-bit dma address */
	u16 data_dma_base1;    /* upper 7 bits of 47-bit dma address */
	u16 overlay_dma_base1; /* upper 7 bits of the 47-bit dma address */
};

union flcn_bl_generic_desc {
	struct flcn_bl_dmem_desc_v0 bl_dmem_desc;
	struct loader_config loader_cfg;
};

struct flcn_ucode_img_v0 {
	u32  *data;
	struct pmu_ucode_desc *desc; /* only some falcons have descriptor */
	u32  data_size;
	/* NULL if not a light secure falcon. */
	struct lsf_ucode_desc_v0 *lsf_desc;
	/* True if there a resources to freed by the client. */
};

/*
 * LSFM Managed Ucode Image
 * next             : Next image the list, NULL if last.
 * wpr_header       : WPR header for this ucode image
 * lsb_header       : LSB header for this ucode image
 * bl_gen_desc      : Bootloader generic desc structure for this ucode image
 * bl_gen_desc_size : Sizeof bootloader desc structure for this ucode image
 * full_ucode_size  : Surface size required for final ucode image
 * ucode_img        : Ucode image info
 */
struct lsfm_managed_ucode_img_v0 {
	struct lsfm_managed_ucode_img_v0 *next;
	struct lsf_wpr_header_v0 wpr_header;
	struct lsf_lsb_header_v0 lsb_header;
	union flcn_bl_generic_desc bl_gen_desc;
	u32 bl_gen_desc_size;
	u32 full_ucode_size;
	struct flcn_ucode_img_v0 ucode_img;
};

/*
 * Defines the structure used to contain all generic information related to
 * the LSFM.
 *
 * Contains the Light Secure Falcon Manager (LSFM) feature related data.
 */
struct ls_flcn_mgr_v0 {
	u16 managed_flcn_cnt;
	u32 wpr_size;
	struct lsfm_managed_ucode_img_v0 *ucode_img_list;
};

int nvgpu_acr_lsf_pmu_ucode_details_v0(struct gk20a *g, void *lsf_ucode_img);
int nvgpu_acr_lsf_fecs_ucode_details_v0(struct gk20a *g, void *lsf_ucode_img);
int nvgpu_acr_lsf_gpccs_ucode_details_v0(struct gk20a *g, void *lsf_ucode_img);

int nvgpu_acr_prepare_ucode_blob_v0(struct gk20a *g);

#endif /* ACR_BLOB_CONSTRUCT_V0_H */
