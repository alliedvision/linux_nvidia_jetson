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

#ifndef ACR_BOOTSTRAP_H
#define ACR_BOOTSTRAP_H

#include "nvgpu_acr_interface.h"

struct gk20a;
struct nvgpu_acr;
struct hs_acr;

struct flcn_acr_region_prop_v0 {
	u32 start_addr;
	u32 end_addr;
	u32 region_id;
	u32 read_mask;
	u32 write_mask;
	u32 client_mask;
};

struct flcn_acr_regions_v0 {
	u32 no_regions;
	struct flcn_acr_region_prop_v0 region_props[NVGPU_FLCN_ACR_MAX_REGIONS];
};

struct flcn_acr_desc_v0 {
	union {
		u32 reserved_dmem[(LSF_BOOTSTRAP_OWNER_RESERVED_DMEM_SIZE/4)];
		u32 signatures[4];
	} ucode_reserved_space;
	/*Always 1st*/
	u32 wpr_region_id;
	u32 wpr_offset;
	u32 mmu_mem_range;
	struct flcn_acr_regions_v0 regions;
	u32 nonwpr_ucode_blob_size;
	u64 nonwpr_ucode_blob_start;
};

struct bin_hdr {
	/* 0x10de */
	u32 bin_magic;
	/* versioning of bin format */
	u32 bin_ver;
	/* Entire image size including this header */
	u32 bin_size;
	/*
	 * Header offset of executable binary metadata,
	 * start @ offset- 0x100 *
	 */
	u32 header_offset;
	/*
	 * Start of executable binary data, start @
	 * offset- 0x200
	 */
	u32 data_offset;
	/* Size of executable binary */
	u32 data_size;
};

struct acr_fw_header {
	u32 sig_dbg_offset;
	u32 sig_dbg_size;
	u32 sig_prod_offset;
	u32 sig_prod_size;
	u32 patch_loc;
	u32 patch_sig;
	u32 hdr_offset; /* This header points to acr_ucode_header_t210_load */
	u32 hdr_size; /* Size of above header */
};

/* ACR Falcon descriptor's */
struct hs_acr {
#define ACR_DEFAULT		0U
#define ACR_AHESASC_NON_FUSA	1U
#define ACR_ASB_NON_FUSA	2U
#define ACR_AHESASC_FUSA	3U
#define ACR_ASB_FUSA		4U
	u32 acr_type;

	/* ACR ucode */
	const char *acr_fw_name;
	const char *acr_code_name;
	const char *acr_data_name;
	const char *acr_manifest_name;
	struct nvgpu_firmware *code_fw;
	struct nvgpu_firmware *data_fw;
	struct nvgpu_firmware *manifest_fw;
	struct nvgpu_firmware *acr_fw;

	union{
		struct flcn_acr_desc_v0 *acr_dmem_desc_v0;
		struct flcn_acr_desc *acr_dmem_desc;
	};

	struct nvgpu_mem acr_falcon2_sysmem_desc;
	struct flcn2_acr_desc acr_sysmem_desc;
	struct nvgpu_mem ls_pmu_desc;

	/* Falcon used to execute ACR ucode */
	struct nvgpu_falcon *acr_flcn;

	void (*report_acr_engine_bus_err_status)(struct gk20a *g,
		u32 bar0_status, u32 error_type);
	int (*acr_engine_bus_err_status)(struct gk20a *g, u32 *bar0_status,
		u32 *error_type);
	bool (*acr_validate_mem_integrity)(struct gk20a *g);
};

int nvgpu_acr_wait_for_completion(struct gk20a *g, struct hs_acr *acr_desc,
	u32 timeout);
int nvgpu_acr_bootstrap_hs_ucode(struct gk20a *g, struct nvgpu_acr *acr,
	struct hs_acr *acr_desc);

int nvgpu_acr_bootstrap_hs_ucode_riscv(struct gk20a *g, struct nvgpu_acr *acr);

#endif /* ACR_BOOTSTRAP_H */
