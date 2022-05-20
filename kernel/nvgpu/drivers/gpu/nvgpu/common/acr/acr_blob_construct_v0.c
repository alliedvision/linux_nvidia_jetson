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

#include <nvgpu/firmware.h>
#include <nvgpu/pmu.h>
#include <nvgpu/falcon.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/string.h>
#include <nvgpu/bug.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/gr/gr_utils.h>

#include "acr_blob_construct_v0.h"
#include "acr_wpr.h"
#include "acr_priv.h"

#ifdef CONFIG_NVGPU_LS_PMU
int nvgpu_acr_lsf_pmu_ucode_details_v0(struct gk20a *g, void *lsf_ucode_img)
{
	struct lsf_ucode_desc_v0 *lsf_desc;
	struct nvgpu_firmware *fw_sig;
	struct nvgpu_firmware *fw_desc;
	struct nvgpu_firmware *fw_image;
	struct flcn_ucode_img_v0 *p_img = (struct flcn_ucode_img_v0 *)lsf_ucode_img;
	int err = 0;

	lsf_desc = nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc_v0));
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

	p_img->desc = (struct pmu_ucode_desc *)(void *)fw_desc->data;
	p_img->data = (u32 *)(void *)fw_image->data;
	p_img->data_size = p_img->desc->image_size;
	p_img->lsf_desc = (struct lsf_ucode_desc_v0 *)lsf_desc;

exit:
	return err;
}
#endif

int nvgpu_acr_lsf_fecs_ucode_details_v0(struct gk20a *g, void *lsf_ucode_img)
{
	struct lsf_ucode_desc_v0 *lsf_desc;
	struct nvgpu_firmware *fecs_sig;
	struct flcn_ucode_img_v0 *p_img = (struct flcn_ucode_img_v0 *)lsf_ucode_img;
	struct nvgpu_gr_falcon *gr_falcon = nvgpu_gr_get_falcon_ptr(g);
	struct nvgpu_ctxsw_ucode_segments *fecs =
			nvgpu_gr_falcon_get_fecs_ucode_segments(gr_falcon);
	int err;

	fecs_sig = nvgpu_request_firmware(g, GM20B_FECS_UCODE_SIG, 0);
	if (fecs_sig == NULL) {
		nvgpu_err(g, "failed to load fecs sig");
		return -ENOENT;
	}
	lsf_desc = nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc_v0));
	if (lsf_desc == NULL) {
		err = -ENOMEM;
		goto rel_sig;
	}
	nvgpu_memcpy((u8 *)lsf_desc, (u8 *)fecs_sig->data,
			min_t(size_t, sizeof(*lsf_desc), fecs_sig->size));

	lsf_desc->falcon_id = FALCON_ID_FECS;

	p_img->desc = nvgpu_kzalloc(g, sizeof(struct pmu_ucode_desc));
	if (p_img->desc == NULL) {
		err = -ENOMEM;
		goto free_lsf_desc;
	}

	p_img->desc->bootloader_start_offset = fecs->boot.offset;
	p_img->desc->bootloader_size = NVGPU_ALIGN(fecs->boot.size, 256U);
	p_img->desc->bootloader_imem_offset = fecs->boot_imem_offset;
	p_img->desc->bootloader_entry_point = fecs->boot_entry;

	p_img->desc->image_size = NVGPU_ALIGN(fecs->boot.size, 256U) +
		NVGPU_ALIGN(fecs->code.size, 256U) + NVGPU_ALIGN(fecs->data.size, 256U);
	p_img->desc->app_size = NVGPU_ALIGN(fecs->code.size, 256U) +
					NVGPU_ALIGN(fecs->data.size, 256U);
	p_img->desc->app_start_offset = fecs->code.offset;
	p_img->desc->app_imem_offset = 0;
	p_img->desc->app_imem_entry = 0;
	p_img->desc->app_dmem_offset = 0;
	p_img->desc->app_resident_code_offset = 0;
	p_img->desc->app_resident_code_size = fecs->code.size;
	p_img->desc->app_resident_data_offset =
				fecs->data.offset - fecs->code.offset;
	p_img->desc->app_resident_data_size = fecs->data.size;
	p_img->data = nvgpu_gr_falcon_get_surface_desc_cpu_va(gr_falcon);
	p_img->data_size = p_img->desc->image_size;

	p_img->lsf_desc = (struct lsf_ucode_desc_v0 *)lsf_desc;
	nvgpu_acr_dbg(g, "fecs fw loaded\n");
	nvgpu_release_firmware(g, fecs_sig);
	return 0;
free_lsf_desc:
	nvgpu_kfree(g, lsf_desc);
rel_sig:
	nvgpu_release_firmware(g, fecs_sig);
	return err;
}

int nvgpu_acr_lsf_gpccs_ucode_details_v0(struct gk20a *g, void *lsf_ucode_img)
{
	struct lsf_ucode_desc_v0 *lsf_desc;
	struct nvgpu_firmware *gpccs_sig;
	struct flcn_ucode_img_v0 *p_img = (struct flcn_ucode_img_v0 *)lsf_ucode_img;
	struct nvgpu_gr_falcon *gr_falcon = nvgpu_gr_get_falcon_ptr(g);
	struct nvgpu_ctxsw_ucode_segments *gpccs =
			nvgpu_gr_falcon_get_gpccs_ucode_segments(gr_falcon);
	int err;

	if (!nvgpu_is_enabled(g, NVGPU_SEC_SECUREGPCCS)) {
		return -ENOENT;
	}

	gpccs_sig = nvgpu_request_firmware(g, T18x_GPCCS_UCODE_SIG, 0);
	if (gpccs_sig == NULL) {
		nvgpu_err(g, "failed to load gpccs sig");
		return -ENOENT;
	}
	lsf_desc = nvgpu_kzalloc(g, sizeof(struct lsf_ucode_desc_v0));
	if (lsf_desc == NULL) {
		err = -ENOMEM;
		goto rel_sig;
	}
	nvgpu_memcpy((u8 *)lsf_desc, (u8 *)gpccs_sig->data,
			min_t(size_t, sizeof(*lsf_desc), gpccs_sig->size));
	lsf_desc->falcon_id = FALCON_ID_GPCCS;

	p_img->desc = nvgpu_kzalloc(g, sizeof(struct pmu_ucode_desc));
	if (p_img->desc == NULL) {
		err = -ENOMEM;
		goto free_lsf_desc;
	}

	p_img->desc->bootloader_start_offset =
		0;
	p_img->desc->bootloader_size = NVGPU_ALIGN(gpccs->boot.size, 256U);
	p_img->desc->bootloader_imem_offset = gpccs->boot_imem_offset;
	p_img->desc->bootloader_entry_point = gpccs->boot_entry;

	p_img->desc->image_size = NVGPU_ALIGN(gpccs->boot.size, 256U) +
		NVGPU_ALIGN(gpccs->code.size, 256U) +
		NVGPU_ALIGN(gpccs->data.size, 256U);
	p_img->desc->app_size = NVGPU_ALIGN(gpccs->code.size, 256U) +
		NVGPU_ALIGN(gpccs->data.size, 256U);
	p_img->desc->app_start_offset = p_img->desc->bootloader_size;
	p_img->desc->app_imem_offset = 0;
	p_img->desc->app_imem_entry = 0;
	p_img->desc->app_dmem_offset = 0;
	p_img->desc->app_resident_code_offset = 0;
	p_img->desc->app_resident_code_size = NVGPU_ALIGN(gpccs->code.size, 256U);
	p_img->desc->app_resident_data_offset =
		NVGPU_ALIGN(gpccs->data.offset, 256U) -
		NVGPU_ALIGN(gpccs->code.offset, 256U);
	p_img->desc->app_resident_data_size = NVGPU_ALIGN(gpccs->data.size, 256U);
	p_img->data = (u32 *)
		((u8 *)nvgpu_gr_falcon_get_surface_desc_cpu_va(gr_falcon) +
							gpccs->boot.offset);
	p_img->data_size = NVGPU_ALIGN(p_img->desc->image_size, 256U);
	p_img->lsf_desc = (struct lsf_ucode_desc_v0 *)lsf_desc;
	nvgpu_acr_dbg(g, "gpccs fw loaded\n");
	nvgpu_release_firmware(g, gpccs_sig);
	return 0;
free_lsf_desc:
	nvgpu_kfree(g, lsf_desc);
rel_sig:
	nvgpu_release_firmware(g, gpccs_sig);
	return err;
}

/*
 * @brief lsfm_fill_static_lsb_hdr_info
 * Populate static LSB header information using the provided ucode image
 */
static void lsfm_fill_static_lsb_hdr_info(struct gk20a *g,
	u32 falcon_id, struct lsfm_managed_ucode_img_v0 *pnode)
{
	u32 full_app_size = 0;
	u32 data = 0;

	if (pnode->ucode_img.lsf_desc != NULL) {
		nvgpu_memcpy((u8 *)&pnode->lsb_header.signature,
			(u8 *)pnode->ucode_img.lsf_desc,
			sizeof(struct lsf_ucode_desc_v0));
	}
	pnode->lsb_header.ucode_size = pnode->ucode_img.data_size;

	/* Uses a loader. that is has a desc */
	pnode->lsb_header.data_size = 0;

	/*
	 * The loader code size is already aligned (padded) such that
	 * the code following it is aligned, but the size in the image
	 * desc is not, bloat it up to be on a 256 byte alignment.
	 */
	pnode->lsb_header.bl_code_size = NVGPU_ALIGN(
		pnode->ucode_img.desc->bootloader_size,
		LSF_BL_CODE_SIZE_ALIGNMENT);
	full_app_size = NVGPU_ALIGN(pnode->ucode_img.desc->app_size,
		LSF_BL_CODE_SIZE_ALIGNMENT) +
		pnode->lsb_header.bl_code_size;
	pnode->lsb_header.ucode_size = NVGPU_ALIGN(
		pnode->ucode_img.desc->app_resident_data_offset,
		LSF_BL_CODE_SIZE_ALIGNMENT) +
		pnode->lsb_header.bl_code_size;
	pnode->lsb_header.data_size = full_app_size -
		pnode->lsb_header.ucode_size;
	/*
	 * Though the BL is located at 0th offset of the image, the VA
	 * is different to make sure that it doesn't collide the actual
	 * OS VA range
	 */
	pnode->lsb_header.bl_imem_off =
		pnode->ucode_img.desc->bootloader_imem_offset;

	pnode->lsb_header.flags = 0;

	if (falcon_id == FALCON_ID_PMU) {
		data = NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_TRUE;
		pnode->lsb_header.flags = data;
	}

	if (g->acr->lsf[falcon_id].is_priv_load) {
		pnode->lsb_header.flags |=
			NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_TRUE;
	}
}

/* Adds a ucode image to the list of managed ucode images managed. */
static int lsfm_add_ucode_img(struct gk20a *g, struct ls_flcn_mgr_v0 *plsfm,
	struct flcn_ucode_img_v0 *ucode_image, u32 falcon_id)
{

	struct lsfm_managed_ucode_img_v0 *pnode;

	pnode = nvgpu_kzalloc(g, sizeof(struct lsfm_managed_ucode_img_v0));
	if (pnode == NULL) {
		return -ENOMEM;
	}

	/* Keep a copy of the ucode image info locally */
	nvgpu_memcpy((u8 *)&pnode->ucode_img, (u8 *)ucode_image,
		sizeof(struct flcn_ucode_img_v0));

	/* Fill in static WPR header info*/
	pnode->wpr_header.falcon_id = falcon_id;
	pnode->wpr_header.bootstrap_owner = g->acr->bootstrap_owner;
	pnode->wpr_header.status = LSF_IMAGE_STATUS_COPY;

	pnode->wpr_header.lazy_bootstrap =
		(u32)g->acr->lsf[falcon_id].is_lazy_bootstrap;

	/* Fill in static LSB header info elsewhere */
	lsfm_fill_static_lsb_hdr_info(g, falcon_id, pnode);
	pnode->next = plsfm->ucode_img_list;
	plsfm->ucode_img_list = pnode;
	return 0;
}

/* Discover all managed falcon ucode images */
static int lsfm_discover_ucode_images(struct gk20a *g,
	struct ls_flcn_mgr_v0 *plsfm)
{
	struct flcn_ucode_img_v0 ucode_img;
	struct nvgpu_acr *acr = g->acr;
	u32 falcon_id;
	u32 i;
	int err = 0;

	/*
	 * Enumerate all constructed falcon objects, as we need the ucode
	 * image info and total falcon count
	 */
	for (i = 0U; i < FALCON_ID_END; i++) {
		if (nvgpu_test_bit(i, (void *)&acr->lsf_enable_mask) &&
			acr->lsf[i].get_lsf_ucode_details != NULL) {

			(void) memset(&ucode_img, 0, sizeof(ucode_img));

			if (acr->lsf[i].get_lsf_ucode_details(g,
				(void *)&ucode_img) != 0) {
				nvgpu_err(g, "LS falcon-%d ucode get failed", i);
				goto exit;
			}

			if (ucode_img.lsf_desc != NULL) {
				/*
				 * falon_id is formed by grabbing the static
				 * base falonId from the image and adding the
				 * engine-designated falcon instance.
				 */
				falcon_id = ucode_img.lsf_desc->falcon_id;

				err = lsfm_add_ucode_img(g, plsfm, &ucode_img,
					falcon_id);
				if (err != 0) {
					nvgpu_err(g, " Failed to add falcon-%d to LSFM ",
						falcon_id);
					goto exit;
				}

				plsfm->managed_flcn_cnt++;
			}
		}
	}

exit:
	return err;
}

/* Generate WPR requirements for ACR allocation request */
static int lsf_gen_wpr_requirements(struct gk20a *g, struct ls_flcn_mgr_v0 *plsfm)
{
	struct lsfm_managed_ucode_img_v0 *pnode = plsfm->ucode_img_list;
	u32 wpr_offset;

	/*
	 * Start with an array of WPR headers at the base of the WPR.
	 * The expectation here is that the secure falcon will do a single DMA
	 * read of this array and cache it internally so it's OK to pack these.
	 * Also, we add 1 to the falcon count to indicate the end of the array.
	 */
	wpr_offset = U32(sizeof(struct lsf_wpr_header_v0)) *
		(U32(plsfm->managed_flcn_cnt) + U32(1));

	/*
	 * Walk the managed falcons, accounting for the LSB structs
	 * as well as the ucode images.
	 */
	while (pnode != NULL) {
		/* Align, save off, and include an LSB header size */
		wpr_offset = NVGPU_ALIGN(wpr_offset, LSF_LSB_HEADER_ALIGNMENT);
		pnode->wpr_header.lsb_offset = wpr_offset;
		wpr_offset += (u32)sizeof(struct lsf_lsb_header_v0);

		/*
		 * Align, save off, and include the original (static)
		 * ucode image size
		 */
		wpr_offset = NVGPU_ALIGN(wpr_offset,
			LSF_UCODE_DATA_ALIGNMENT);
		pnode->lsb_header.ucode_off = wpr_offset;
		wpr_offset += pnode->ucode_img.data_size;

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
		 * generic one, which is the largest it will will ever be.
		 */
		/* Align (size bloat) and save off generic descriptor size */
		pnode->lsb_header.bl_data_size = NVGPU_ALIGN(
			(u32)sizeof(pnode->bl_gen_desc),
			LSF_BL_DATA_SIZE_ALIGNMENT);

		/* Align, save off, and include the additional BL data */
		wpr_offset = NVGPU_ALIGN(wpr_offset,
			LSF_BL_DATA_ALIGNMENT);
		pnode->lsb_header.bl_data_off = wpr_offset;
		wpr_offset += pnode->lsb_header.bl_data_size;

		/* Finally, update ucode surface size to include updates */
		pnode->full_ucode_size = wpr_offset -
			pnode->lsb_header.ucode_off;
		if (pnode->wpr_header.falcon_id != FALCON_ID_PMU) {
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
		pnode = pnode->next;
	}
	plsfm->wpr_size = wpr_offset;
	return 0;
}

/* Initialize WPR contents */
static int gm20b_pmu_populate_loader_cfg(struct gk20a *g,
	void *lsfm, u32 *p_bl_gen_desc_size)
{
	struct wpr_carveout_info wpr_inf;
	struct lsfm_managed_ucode_img_v0 *p_lsfm =
			(struct lsfm_managed_ucode_img_v0 *)lsfm;
	struct flcn_ucode_img_v0 *p_img = &(p_lsfm->ucode_img);
	struct loader_config *ldr_cfg = &(p_lsfm->bl_gen_desc.loader_cfg);
	u64 addr_base;
	struct pmu_ucode_desc *desc;
	u64 tmp;
	u32 addr_code, addr_data;

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
	addr_base = p_lsfm->lsb_header.ucode_off;
	g->acr->get_wpr_info(g, &wpr_inf);
	addr_base += wpr_inf.wpr_base;
	nvgpu_acr_dbg(g, "pmu loader cfg u32 addrbase %x\n", (u32)addr_base);
	/*From linux*/
	tmp = (addr_base +
			desc->app_start_offset +
			desc->app_resident_code_offset) >> 8;
	nvgpu_assert(tmp <= U32_MAX);
	addr_code = u64_lo32(tmp);
	nvgpu_acr_dbg(g, "app start %d app res code off %d\n",
		desc->app_start_offset, desc->app_resident_code_offset);
	tmp = (addr_base +
			desc->app_start_offset +
			desc->app_resident_data_offset) >> 8;
	nvgpu_assert(tmp <= U32_MAX);
	addr_data = u64_lo32(tmp);
	nvgpu_acr_dbg(g, "app res data offset%d\n",
		desc->app_resident_data_offset);
	nvgpu_acr_dbg(g, "bl start off %d\n", desc->bootloader_start_offset);

	/* Populate the loader_config state*/
	ldr_cfg->dma_idx = g->acr->lsf[FALCON_ID_PMU].falcon_dma_idx;
	ldr_cfg->code_dma_base = addr_code;
	ldr_cfg->code_dma_base1 = 0x0;
	ldr_cfg->code_size_total = desc->app_size;
	ldr_cfg->code_size_to_load = desc->app_resident_code_size;
	ldr_cfg->code_entry_point = desc->app_imem_entry;
	ldr_cfg->data_dma_base = addr_data;
	ldr_cfg->data_dma_base1 = 0;
	ldr_cfg->data_size = desc->app_resident_data_size;
	ldr_cfg->overlay_dma_base = addr_code;
	ldr_cfg->overlay_dma_base1 = 0x0;

	/* Update the argc/argv members*/
	ldr_cfg->argc = 1;
#ifdef CONFIG_NVGPU_LS_PMU
	nvgpu_pmu_fw_get_cmd_line_args_offset(g, &ldr_cfg->argv);
#endif
	*p_bl_gen_desc_size = (u32)sizeof(struct loader_config);
	return 0;
}

static int gm20b_flcn_populate_bl_dmem_desc(struct gk20a *g,
	void *lsfm, u32 *p_bl_gen_desc_size, u32 falconid)
{
	struct wpr_carveout_info wpr_inf;
	struct lsfm_managed_ucode_img_v0 *p_lsfm =
			(struct lsfm_managed_ucode_img_v0 *)lsfm;
	struct flcn_ucode_img_v0 *p_img = &(p_lsfm->ucode_img);
	struct flcn_bl_dmem_desc_v0 *ldr_cfg =
			&(p_lsfm->bl_gen_desc.bl_dmem_desc);
	u64 addr_base;
	struct pmu_ucode_desc *desc;
	u32 addr_code, addr_data;
	u64 tmp;

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
	addr_base = p_lsfm->lsb_header.ucode_off;
	g->acr->get_wpr_info(g, &wpr_inf);
	addr_base += wpr_inf.wpr_base;

	nvgpu_acr_dbg(g, "gen loader cfg %x u32 addrbase %x ID\n", (u32)addr_base,
			p_lsfm->wpr_header.falcon_id);
	tmp = (addr_base +
			desc->app_start_offset +
			desc->app_resident_code_offset) >> 8;
	nvgpu_assert(tmp <= U32_MAX);
	addr_code = u64_lo32(tmp);
	tmp = (addr_base +
			desc->app_start_offset +
			desc->app_resident_data_offset) >> 8;
	nvgpu_assert(tmp <= U32_MAX);
	addr_data = u64_lo32(tmp);

	nvgpu_acr_dbg(g, "gen cfg %x u32 addrcode %x & data %x load offset %xID\n",
		(u32)addr_code, (u32)addr_data, desc->bootloader_start_offset,
		p_lsfm->wpr_header.falcon_id);

	/* Populate the LOADER_CONFIG state */
	(void) memset((void *) ldr_cfg, 0, sizeof(struct flcn_bl_dmem_desc_v0));
	ldr_cfg->ctx_dma = g->acr->lsf[falconid].falcon_dma_idx;
	ldr_cfg->code_dma_base = addr_code;
	ldr_cfg->non_sec_code_size = desc->app_resident_code_size;
	ldr_cfg->data_dma_base = addr_data;
	ldr_cfg->data_size = desc->app_resident_data_size;
	ldr_cfg->code_entry_point = desc->app_imem_entry;
	*p_bl_gen_desc_size = (u32)sizeof(struct flcn_bl_dmem_desc_v0);
	return 0;
}

/* Populate falcon boot loader generic desc.*/
static int lsfm_fill_flcn_bl_gen_desc(struct gk20a *g,
		struct lsfm_managed_ucode_img_v0 *pnode)
{
	int err = -ENOENT;

	if (pnode->wpr_header.falcon_id != FALCON_ID_PMU) {
		nvgpu_acr_dbg(g, "non pmu. write flcn bl gen desc\n");
		err = gm20b_flcn_populate_bl_dmem_desc(g,
				pnode, &pnode->bl_gen_desc_size,
				pnode->wpr_header.falcon_id);
		if (err != 0) {
			nvgpu_err(g, "flcn_populate_bl_dmem_desc failed=%d",
				err);
		}
		return err;
	}

	if (pnode->wpr_header.falcon_id == FALCON_ID_PMU) {
		nvgpu_acr_dbg(g, "pmu write flcn bl gen desc\n");
		err = gm20b_pmu_populate_loader_cfg(g, pnode,
				&pnode->bl_gen_desc_size);
		if (err != 0) {
			nvgpu_err(g, "pmu_populate_loader_cfg failed=%d",
				err);
		}
		return err;
	}

	/* Failed to find the falcon requested. */
	return err;
}

static int lsfm_init_wpr_contents(struct gk20a *g, struct ls_flcn_mgr_v0 *plsfm,
	struct nvgpu_mem *ucode)
{
	struct lsfm_managed_ucode_img_v0 *pnode = plsfm->ucode_img_list;
	struct lsf_wpr_header_v0 last_wpr_hdr;
	u32 i;
	int err = 0;

	/* The WPR array is at the base of the WPR */
	pnode = plsfm->ucode_img_list;
	(void) memset(&last_wpr_hdr, 0, sizeof(struct lsf_wpr_header_v0));
	i = 0;

	/*
	 * Walk the managed falcons, flush WPR and LSB headers to FB.
	 * flush any bl args to the storage area relative to the
	 * ucode image (appended on the end as a DMEM area).
	 */
	while (pnode != NULL) {
		/* Flush WPR header to memory*/
		nvgpu_mem_wr_n(g, ucode, i * (u32)sizeof(pnode->wpr_header),
				&pnode->wpr_header,
				(u32)sizeof(pnode->wpr_header));

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
		nvgpu_mem_wr_n(g, ucode, pnode->wpr_header.lsb_offset,
				&pnode->lsb_header,
				(u32)sizeof(pnode->lsb_header));

		nvgpu_acr_dbg(g, "lsb header");
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

		/* this falcon has a boot loader and related args, flush them */
		/* Populate gen bl and flush to memory */
		err = lsfm_fill_flcn_bl_gen_desc(g, pnode);
		if (err != 0) {
			nvgpu_err(g, "bl_gen_desc failed err=%d", err);
			return err;
		}
		nvgpu_mem_wr_n(g, ucode,
				pnode->lsb_header.bl_data_off,
				&pnode->bl_gen_desc,
				pnode->bl_gen_desc_size);

		/* Copying of ucode */
		nvgpu_mem_wr_n(g, ucode, pnode->lsb_header.ucode_off,
				pnode->ucode_img.data,
				pnode->ucode_img.data_size);
		pnode = pnode->next;
		i++;
	}

	/* Tag the terminator WPR header with an invalid falcon ID. */
	last_wpr_hdr.falcon_id = FALCON_ID_INVALID;
	nvgpu_mem_wr_n(g, ucode,
			(u32)plsfm->managed_flcn_cnt *
				(u32)sizeof(struct lsf_wpr_header_v0),
			&last_wpr_hdr,
			(u32)sizeof(struct lsf_wpr_header_v0));
	return err;
}

/* Free any ucode image structure resources. */
static void lsfm_free_ucode_img_res(struct gk20a *g,
	struct flcn_ucode_img_v0 *p_img)
{
	if (p_img->lsf_desc != NULL) {
		nvgpu_kfree(g, p_img->lsf_desc);
		p_img->lsf_desc = NULL;
	}
}

/* Free any ucode image structure resources. */
static void lsfm_free_nonpmu_ucode_img_res(struct gk20a *g,
	struct flcn_ucode_img_v0 *p_img)
{
	if (p_img->lsf_desc != NULL) {
		nvgpu_kfree(g, p_img->lsf_desc);
		p_img->lsf_desc = NULL;
	}
	if (p_img->desc != NULL) {
		nvgpu_kfree(g, p_img->desc);
		p_img->desc = NULL;
	}
}

static void free_acr_resources(struct gk20a *g, struct ls_flcn_mgr_v0 *plsfm)
{
	u32 cnt = plsfm->managed_flcn_cnt;
	struct lsfm_managed_ucode_img_v0 *mg_ucode_img;
	while (cnt != 0U) {
		mg_ucode_img = plsfm->ucode_img_list;
		if (mg_ucode_img->ucode_img.lsf_desc->falcon_id ==
				FALCON_ID_PMU) {
			lsfm_free_ucode_img_res(g, &mg_ucode_img->ucode_img);
		} else {
			lsfm_free_nonpmu_ucode_img_res(g,
				&mg_ucode_img->ucode_img);
		}
		plsfm->ucode_img_list = mg_ucode_img->next;
		nvgpu_kfree(g, mg_ucode_img);
		cnt--;
	}
}

int nvgpu_acr_prepare_ucode_blob_v0(struct gk20a *g)
{
	int err = 0;
	struct ls_flcn_mgr_v0 lsfm_l, *plsfm;
	struct wpr_carveout_info wpr_inf;
	struct nvgpu_gr_falcon *gr_falcon = nvgpu_gr_get_falcon_ptr(g);

	if (g->acr->ucode_blob.cpu_va != NULL) {
		/* Recovery case, we do not need to form non WPR blob */
		return err;
	}
	plsfm = &lsfm_l;
	(void) memset((void *)plsfm, 0, sizeof(struct ls_flcn_mgr_v0));
	nvgpu_acr_dbg(g, "fetching GMMU regs\n");
	err = g->ops.fb.vpr_info_fetch(g);
	if (err != 0) {
		nvgpu_err(g, "fb.vpr_info_fetch failed err=%d", err);
		return err;
	}

	err = nvgpu_gr_falcon_init_ctxsw_ucode(g, gr_falcon);
	if (err != 0) {
		nvgpu_err(g, "gr_falcon_init_ctxsw_ucode failed err=%d", err);
		return err;
	}

	g->acr->get_wpr_info(g, &wpr_inf);
	nvgpu_acr_dbg(g, "wpr carveout base:%llx\n", wpr_inf.wpr_base);
	nvgpu_acr_dbg(g, "wpr carveout size :%llx\n", wpr_inf.size);

	/* Discover all managed falcons*/
	err = lsfm_discover_ucode_images(g, plsfm);
	nvgpu_acr_dbg(g, " Managed Falcon cnt %d\n", plsfm->managed_flcn_cnt);
	if (err != 0) {
		goto exit_err;
	}

	if ((plsfm->managed_flcn_cnt != 0U) &&
		(g->acr->ucode_blob.cpu_va == NULL)) {
		/* Generate WPR requirements */
		err = lsf_gen_wpr_requirements(g, plsfm);
		if (err != 0) {
			goto exit_err;
		}

		/* Alloc memory to hold ucode blob contents */
		err = g->acr->alloc_blob_space(g, plsfm->wpr_size
				, &g->acr->ucode_blob);
		if (err != 0) {
			goto exit_err;
		}

		nvgpu_acr_dbg(g, "managed LS falcon %d, WPR size %d bytes.\n",
			plsfm->managed_flcn_cnt, plsfm->wpr_size);
		err = lsfm_init_wpr_contents(g, plsfm, &g->acr->ucode_blob);
		if (err != 0) {
			nvgpu_kfree(g, &g->acr->ucode_blob);
			goto free_acr;
		}
	} else {
		nvgpu_acr_dbg(g, "LSFM is managing no falcons.\n");
	}
	nvgpu_acr_dbg(g, "prepare ucode blob return 0\n");

free_acr:
	free_acr_resources(g, plsfm);
exit_err:
	return err;
}
