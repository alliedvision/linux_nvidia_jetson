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
#include <nvgpu/netlist.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/enabled.h>
#include <nvgpu/debug.h>
#include <nvgpu/gr/hwpm_map.h>
#include <nvgpu/firmware.h>
#include <nvgpu/sizes.h>
#include <nvgpu/mm.h>
#include <nvgpu/acr.h>
#include <nvgpu/gr/gr_utils.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/lsfm.h>
#include <nvgpu/pmu/pmu_pg.h>
#endif
#ifdef CONFIG_NVGPU_DGPU
#include <nvgpu/sec2/lsfm.h>
#endif
#include <nvgpu/dma.h>
#include <nvgpu/static_analysis.h>

#include "gr_falcon_priv.h"

#define NVGPU_FECS_UCODE_IMAGE	"fecs.bin"
#define NVGPU_GPCCS_UCODE_IMAGE	"gpccs.bin"

struct nvgpu_gr_falcon *nvgpu_gr_falcon_init_support(struct gk20a *g)
{
	struct nvgpu_gr_falcon *falcon;

	nvgpu_log_fn(g, " ");

	falcon = nvgpu_kzalloc(g, sizeof(*falcon));
	if (falcon == NULL) {
		return falcon;
	}

	nvgpu_mutex_init(&falcon->fecs_mutex);
	falcon->coldboot_bootstrap_done = false;

	return falcon;
}

void nvgpu_gr_falcon_suspend(struct gk20a *g, struct nvgpu_gr_falcon *falcon)
{
	nvgpu_log_fn(g, " ");

	if (falcon == NULL) {
		return;
	}
	falcon->coldboot_bootstrap_done = false;
}

void nvgpu_gr_falcon_remove_support(struct gk20a *g,
				struct nvgpu_gr_falcon *falcon)
{
	nvgpu_log_fn(g, " ");

	if (falcon == NULL) {
		return;
	}
	nvgpu_kfree(g, falcon);
}

#ifdef CONFIG_NVGPU_POWER_PG
int nvgpu_gr_falcon_bind_fecs_elpg(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_LS_PMU
	struct nvgpu_pmu *pmu = g->pmu;
	struct mm_gk20a *mm = &g->mm;
	int err = 0;
	u32 size;
	u32 data;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	size = 0;

	err = g->ops.gr.falcon.ctrl_ctxsw(g,
		NVGPU_GR_FALCON_METHOD_REGLIST_DISCOVER_IMAGE_SIZE, 0U, &size);
	if (err != 0) {
		nvgpu_err(g,
			"fail to query fecs pg buffer size");
		return err;
	}

	nvgpu_log(g, gpu_dbg_gr, "FECS PG buffer size = %u", size);

	err = nvgpu_pmu_pg_buf_alloc(g, pmu, size);
	if (err != 0) {
		nvgpu_err(g, "failed to allocate pg_buf memory");
		return err;
	}

	data = g->ops.gr.falcon.get_fecs_current_ctx_data(g,
						&mm->pmu.inst_block);
	err = g->ops.gr.falcon.ctrl_ctxsw(g,
		NVGPU_GR_FALCON_METHOD_REGLIST_BIND_INSTANCE, data, NULL);
	if (err != 0) {
		nvgpu_err(g,
			"fail to bind pmu inst to gr");
		return err;
	}

	data = u64_lo32(nvgpu_pmu_pg_buf_get_gpu_va(g, pmu) >> 8);
	err = g->ops.gr.falcon.ctrl_ctxsw(g,
		NVGPU_GR_FALCON_METHOD_REGLIST_SET_VIRTUAL_ADDRESS, data, NULL);
	if (err != 0) {
		nvgpu_err(g,
			"fail to set pg buffer pmu va");
		return err;
	}

	nvgpu_log(g, gpu_dbg_gr, "done");
	return err;
#else
	return 0;
#endif
}
#endif

int nvgpu_gr_falcon_init_ctxsw(struct gk20a *g, struct nvgpu_gr_falcon *falcon)
{
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	err = g->ops.gr.falcon.load_ctxsw_ucode(g, falcon);
	if (err != 0) {
		goto out;
	}

	err = g->ops.gr.falcon.wait_ctxsw_ready(g);

out:
	if (err != 0) {
		nvgpu_err(g, "fail");
	} else {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	}

	return err;
}

int nvgpu_gr_falcon_init_ctx_state(struct gk20a *g,
		struct nvgpu_gr_falcon *falcon)
{
	struct nvgpu_gr_falcon_query_sizes *sizes = &falcon->sizes;
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	/* fecs init ramchain */
	err = g->ops.gr.falcon.init_ctx_state(g, sizes);
	if (err != 0) {
		goto out;
	}

out:
	if (err != 0) {
		nvgpu_err(g, "fail");
	} else {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	}

	return err;
}

u32 nvgpu_gr_falcon_get_golden_image_size(struct nvgpu_gr_falcon *falcon)
{
	return falcon->sizes.golden_image_size;
}

#ifdef CONFIG_NVGPU_DEBUGGER
u32 nvgpu_gr_falcon_get_pm_ctxsw_image_size(struct nvgpu_gr_falcon *falcon)
{
	return falcon->sizes.pm_ctxsw_image_size;
}
#endif

#ifdef CONFIG_NVGPU_GFXP
u32 nvgpu_gr_falcon_get_preempt_image_size(struct nvgpu_gr_falcon *falcon)
{
	return falcon->sizes.preempt_image_size;
}
#endif /* CONFIG_NVGPU_GFXP */

#ifdef CONFIG_NVGPU_GRAPHICS
u32 nvgpu_gr_falcon_get_zcull_image_size(struct nvgpu_gr_falcon *falcon)
{
	return falcon->sizes.zcull_image_size;
}
#endif /* CONFIG_NVGPU_GRAPHICS */

static int nvgpu_gr_falcon_init_ctxsw_ucode_vaspace(struct gk20a *g,
					struct nvgpu_gr_falcon *falcon)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->pmu.vm;
	struct nvgpu_ctxsw_ucode_info *ucode_info = &falcon->ctxsw_ucode_info;
	int err;

	err = nvgpu_alloc_inst_block(g, &ucode_info->inst_blk_desc);
	if (err != 0) {
		return err;
	}

	g->ops.mm.init_inst_block(&ucode_info->inst_blk_desc, vm, 0);

	/* Map ucode surface to GMMU */
	ucode_info->surface_desc.gpu_va = nvgpu_gmmu_map(vm,
					&ucode_info->surface_desc,
					0, /* flags */
					gk20a_mem_flag_read_only,
					false,
					ucode_info->surface_desc.aperture);
	if (ucode_info->surface_desc.gpu_va == 0ULL) {
		nvgpu_err(g, "failed to update gmmu ptes");
		return -ENOMEM;
	}

	return 0;
}

static void nvgpu_gr_falcon_init_ctxsw_ucode_segment(
	struct nvgpu_ctxsw_ucode_segment *p_seg, u32 *offset, u32 size)
{
	u32 ucode_offset;

	p_seg->offset = *offset;
	p_seg->size = size;
	ucode_offset = nvgpu_safe_add_u32(*offset, size);
	*offset = NVGPU_ALIGN(ucode_offset, 256U);
}

static void nvgpu_gr_falcon_init_ctxsw_ucode_segments(
	struct nvgpu_ctxsw_ucode_segments *segments, u32 *offset,
	struct nvgpu_ctxsw_bootloader_desc *bootdesc,
	u32 code_size, u32 data_size)
{
	u32 boot_size = NVGPU_ALIGN(bootdesc->size, sizeof(u32));

	segments->boot_entry = bootdesc->entry_point;
	segments->boot_imem_offset = bootdesc->imem_offset;
	nvgpu_gr_falcon_init_ctxsw_ucode_segment(&segments->boot,
							offset, boot_size);
	nvgpu_gr_falcon_init_ctxsw_ucode_segment(&segments->code,
							offset, code_size);
	nvgpu_gr_falcon_init_ctxsw_ucode_segment(&segments->data,
							offset, data_size);
}

static void nvgpu_gr_falcon_copy_ctxsw_ucode_segments(
	struct gk20a *g,
	struct nvgpu_mem *dst,
	struct nvgpu_ctxsw_ucode_segments *segments,
	u32 *bootimage,
	u32 *code, u32 *data)
{
	unsigned int i;

	nvgpu_mem_wr_n(g, dst, segments->boot.offset, bootimage,
			segments->boot.size);
	nvgpu_mem_wr_n(g, dst, segments->code.offset, code,
			segments->code.size);
	nvgpu_mem_wr_n(g, dst, segments->data.offset, data,
			segments->data.size);

	/* compute a "checksum" for the boot binary to detect its version */
	segments->boot_signature = 0;
	for (i = 0; i < (segments->boot.size / sizeof(u32)); i++) {
		segments->boot_signature = nvgpu_gr_checksum_u32(
				segments->boot_signature, bootimage[i]);
	}
}

int nvgpu_gr_falcon_init_ctxsw_ucode(struct gk20a *g,
					struct nvgpu_gr_falcon *falcon)
{
	struct nvgpu_ctxsw_bootloader_desc *fecs_boot_desc;
	struct nvgpu_ctxsw_bootloader_desc *gpccs_boot_desc;
	struct nvgpu_firmware *fecs_fw;
	struct nvgpu_firmware *gpccs_fw;
	u32 *fecs_boot_image;
	u32 *gpccs_boot_image;
	struct nvgpu_ctxsw_ucode_info *ucode_info = &falcon->ctxsw_ucode_info;
	u32 ucode_size;
	int err = 0;

	nvgpu_log(g, gpu_dbg_gr, "Requst and copy FECS/GPCCS firmwares");

	fecs_fw = nvgpu_request_firmware(g, NVGPU_FECS_UCODE_IMAGE, 0);
	if (fecs_fw == NULL) {
		nvgpu_err(g, "failed to load fecs ucode!!");
		return -ENOENT;
	}

	fecs_boot_desc = (void *)fecs_fw->data;
	fecs_boot_image = (void *)(fecs_fw->data +
				sizeof(struct nvgpu_ctxsw_bootloader_desc));

	gpccs_fw = nvgpu_request_firmware(g, NVGPU_GPCCS_UCODE_IMAGE, 0);
	if (gpccs_fw == NULL) {
		nvgpu_release_firmware(g, fecs_fw);
		nvgpu_err(g, "failed to load gpccs ucode!!");
		return -ENOENT;
	}

	gpccs_boot_desc = (void *)gpccs_fw->data;
	gpccs_boot_image = (void *)(gpccs_fw->data +
				sizeof(struct nvgpu_ctxsw_bootloader_desc));

	ucode_size = 0;
	nvgpu_gr_falcon_init_ctxsw_ucode_segments(&ucode_info->fecs,
		&ucode_size, fecs_boot_desc,
		nvgpu_safe_mult_u32(
		nvgpu_netlist_get_fecs_inst_count(g), (u32)sizeof(u32)),
		nvgpu_safe_mult_u32(
		nvgpu_netlist_get_fecs_data_count(g), (u32)sizeof(u32)));
	nvgpu_gr_falcon_init_ctxsw_ucode_segments(&ucode_info->gpccs,
		&ucode_size, gpccs_boot_desc,
		nvgpu_safe_mult_u32(
		nvgpu_netlist_get_gpccs_inst_count(g), (u32)sizeof(u32)),
		nvgpu_safe_mult_u32(
		nvgpu_netlist_get_gpccs_data_count(g), (u32)sizeof(u32)));

	err = nvgpu_dma_alloc_sys(g, ucode_size, &ucode_info->surface_desc);
	if (err != 0) {
		goto clean_up;
	}

	nvgpu_gr_falcon_copy_ctxsw_ucode_segments(g,
		&ucode_info->surface_desc,
		&ucode_info->fecs,
		fecs_boot_image,
		nvgpu_netlist_get_fecs_inst_list(g),
		nvgpu_netlist_get_fecs_data_list(g));

	nvgpu_release_firmware(g, fecs_fw);
	fecs_fw = NULL;

	nvgpu_gr_falcon_copy_ctxsw_ucode_segments(g,
		&ucode_info->surface_desc,
		&ucode_info->gpccs,
		gpccs_boot_image,
		nvgpu_netlist_get_gpccs_inst_list(g),
		nvgpu_netlist_get_gpccs_data_list(g));

	nvgpu_release_firmware(g, gpccs_fw);
	gpccs_fw = NULL;

	err = nvgpu_gr_falcon_init_ctxsw_ucode_vaspace(g, falcon);
	if (err != 0) {
		goto clean_up;
	}

	return 0;

clean_up:
	nvgpu_dma_free(g, &ucode_info->surface_desc);

	if (gpccs_fw != NULL) {
		nvgpu_release_firmware(g, gpccs_fw);
		gpccs_fw = NULL;
	}
	if (fecs_fw != NULL) {
		nvgpu_release_firmware(g, fecs_fw);
		fecs_fw = NULL;
	}

	return err;
}

static void nvgpu_gr_falcon_bind_instblk(struct gk20a *g,
					struct nvgpu_gr_falcon *falcon)
{
	struct nvgpu_ctxsw_ucode_info *ucode_info =
					&falcon->ctxsw_ucode_info;
	u64 inst_ptr;

	if (g->ops.gr.falcon.bind_instblk == NULL) {
		return;
        }

	inst_ptr = nvgpu_inst_block_addr(g, &ucode_info->inst_blk_desc);

	g->ops.gr.falcon.bind_instblk(g, &ucode_info->inst_blk_desc,
					inst_ptr);

}

#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
static void nvgpu_gr_falcon_load_dmem(struct gk20a *g)
{
	u32 ucode_u32_size;
	const u32 *ucode_u32_data;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	ucode_u32_size = nvgpu_netlist_get_gpccs_data_count(g);
	ucode_u32_data = (const u32 *)nvgpu_netlist_get_gpccs_data_list(g);
	g->ops.gr.falcon.load_gpccs_dmem(g, ucode_u32_data, ucode_u32_size);

	ucode_u32_size = nvgpu_netlist_get_fecs_data_count(g);
	ucode_u32_data = (const u32 *)nvgpu_netlist_get_fecs_data_list(g);
	g->ops.gr.falcon.load_fecs_dmem(g, ucode_u32_data, ucode_u32_size);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
}

static void nvgpu_gr_falcon_load_imem(struct gk20a *g)
{
	u32 ucode_u32_size;
	const u32 *ucode_u32_data;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	ucode_u32_size = nvgpu_netlist_get_gpccs_inst_count(g);
	ucode_u32_data = (const u32 *)nvgpu_netlist_get_gpccs_inst_list(g);
	g->ops.gr.falcon.load_gpccs_imem(g, ucode_u32_data, ucode_u32_size);


	ucode_u32_size = nvgpu_netlist_get_fecs_inst_count(g);
	ucode_u32_data = (const u32 *)nvgpu_netlist_get_fecs_inst_list(g);
	g->ops.gr.falcon.load_fecs_imem(g, ucode_u32_data, ucode_u32_size);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
}

static void nvgpu_gr_falcon_load_ctxsw_ucode_header(struct gk20a *g,
	u64 addr_base, struct nvgpu_ctxsw_ucode_segments *segments,
	u32 reg_offset)
{
	u32 addr_code32 = u64_lo32(nvgpu_safe_add_u64(addr_base,
					segments->code.offset) >> 8);
	u32 addr_data32 = u64_lo32(nvgpu_safe_add_u64(addr_base,
					segments->data.offset) >> 8);

	g->ops.gr.falcon.load_ctxsw_ucode_header(g, reg_offset,
		segments->boot_signature, addr_code32, addr_data32,
		segments->code.size, segments->data.size);
}

static void nvgpu_gr_falcon_load_ctxsw_ucode_boot(struct gk20a *g,
	u64 addr_base, struct nvgpu_ctxsw_ucode_segments *segments,
	u32 reg_offset)
{
	u32 addr_load32 = u64_lo32(nvgpu_safe_add_u64(addr_base,
				segments->boot.offset) >> 8);
	u32 blocks = (nvgpu_safe_add_u32(segments->boot.size, 0xFFU)
								& ~0xFFU) >> 8;
	u32 dst = segments->boot_imem_offset;

	g->ops.gr.falcon.load_ctxsw_ucode_boot(g, reg_offset,
		segments->boot_entry, addr_load32, blocks, dst);

}

static void nvgpu_gr_falcon_load_ctxsw_ucode_segments(
		struct gk20a *g, u64 addr_base,
		struct nvgpu_ctxsw_ucode_segments *segments, u32 reg_offset)
{

	/* Copy falcon bootloader into dmem */
	nvgpu_gr_falcon_load_ctxsw_ucode_header(g, addr_base,
						segments, reg_offset);
	nvgpu_gr_falcon_load_ctxsw_ucode_boot(g,
					addr_base, segments, reg_offset);
}

static void nvgpu_gr_falcon_load_with_bootloader(struct gk20a *g,
					struct nvgpu_gr_falcon *falcon)
{
	struct nvgpu_ctxsw_ucode_info *ucode_info =
					&falcon->ctxsw_ucode_info;
	u64 addr_base = ucode_info->surface_desc.gpu_va;

	nvgpu_log(g, gpu_dbg_gr, " ");

	nvgpu_gr_falcon_bind_instblk(g, falcon);

	nvgpu_gr_falcon_load_ctxsw_ucode_segments(g, addr_base,
		&falcon->ctxsw_ucode_info.fecs, 0);

	nvgpu_gr_falcon_load_ctxsw_ucode_segments(g, addr_base,
		&falcon->ctxsw_ucode_info.gpccs,
		g->ops.gr.falcon.get_gpccs_start_reg_offset());
}

int nvgpu_gr_falcon_load_ctxsw_ucode(struct gk20a *g,
					struct nvgpu_gr_falcon *falcon)
{
	int err;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		g->ops.gr.falcon.configure_fmodel(g);
	}
#endif

	/*
	 * In case bootloader is not supported, revert to the old way of
	 * loading gr ucode, without the faster bootstrap routine.
	 */
	if (!nvgpu_is_enabled(g, NVGPU_GR_USE_DMA_FOR_FW_BOOTSTRAP)) {
		nvgpu_gr_falcon_load_dmem(g);
		nvgpu_gr_falcon_load_imem(g);
		g->ops.gr.falcon.start_ucode(g);
	} else {
		if (!falcon->skip_ucode_init) {
			err =  nvgpu_gr_falcon_init_ctxsw_ucode(g, falcon);
			if (err != 0) {
				return err;
			}
		}
		nvgpu_gr_falcon_load_with_bootloader(g, falcon);
		falcon->skip_ucode_init = true;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	return 0;
}

static void nvgpu_gr_falcon_load_gpccs_with_bootloader(struct gk20a *g,
					struct nvgpu_gr_falcon *falcon)
{
	struct nvgpu_ctxsw_ucode_info *ucode_info =
					&falcon->ctxsw_ucode_info;
	u64 addr_base = ucode_info->surface_desc.gpu_va;

	nvgpu_gr_falcon_bind_instblk(g, falcon);

	nvgpu_gr_falcon_load_ctxsw_ucode_segments(g, addr_base,
		&falcon->ctxsw_ucode_info.gpccs,
		g->ops.gr.falcon.get_gpccs_start_reg_offset());
}
#endif

#if defined(CONFIG_NVGPU_DGPU) || defined(CONFIG_NVGPU_LS_PMU)
static int gr_falcon_sec2_or_ls_pmu_bootstrap(struct gk20a *g,
				 bool *bootstrap, u32 falcon_id_mask)
{
	int err = 0;
	bool bootstrap_set = false;

#ifdef CONFIG_NVGPU_DGPU
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_SEC2_RTOS)) {
		bootstrap_set = true;
		nvgpu_log(g, gpu_dbg_gr, "bootstrap by SEC2");

		err = nvgpu_sec2_bootstrap_ls_falcons(g,
			&g->sec2, FALCON_ID_FECS);
		if (err == 0) {
			err = nvgpu_sec2_bootstrap_ls_falcons(g,
				&g->sec2, FALCON_ID_GPCCS);
		}
	} else
#endif
#ifdef CONFIG_NVGPU_LS_PMU
	if (g->support_ls_pmu) {
		bootstrap_set = true;
		nvgpu_log(g, gpu_dbg_gr, "bootstrap by LS PMU");

		err = nvgpu_pmu_lsfm_bootstrap_ls_falcon(g,
				g->pmu, g->pmu->lsfm,
				falcon_id_mask);
	}
#endif

	*bootstrap = bootstrap_set;
	return err;
}

static int gr_falcon_sec2_or_ls_pmu_recovery_bootstrap(struct gk20a *g)
{
	int err = 0;
	bool bootstrap = false;
	u32 falcon_idmask = BIT32(FALCON_ID_FECS) | BIT32(FALCON_ID_GPCCS);

	err = gr_falcon_sec2_or_ls_pmu_bootstrap(g,
				&bootstrap,
				falcon_idmask);
	if ((err == 0) && (!bootstrap)) {
		err = nvgpu_acr_bootstrap_hs_acr(g, g->acr);
		if (err != 0) {
			nvgpu_err(g,
				"ACR GR LSF bootstrap failed");
		}
	}

	return err;
}

static int gr_falcon_sec2_or_ls_pmu_coldboot_bootstrap(struct gk20a *g)
{
	int err = 0;
	u8 falcon_id_mask = 0;
	bool bootstrap = false;

	if (!nvgpu_is_enabled(g, NVGPU_SEC_SECUREGPCCS)) {
		return err;
	}

	if (nvgpu_acr_is_lsf_lazy_bootstrap(g, g->acr,
					FALCON_ID_FECS)) {
		falcon_id_mask |= BIT8(FALCON_ID_FECS);
	}
	if (nvgpu_acr_is_lsf_lazy_bootstrap(g, g->acr,
					FALCON_ID_GPCCS)) {
		falcon_id_mask |= BIT8(FALCON_ID_GPCCS);
	}

	err = gr_falcon_sec2_or_ls_pmu_bootstrap(g,
				&bootstrap,
				(u32)falcon_id_mask);
	if ((err == 0) && (!bootstrap)) {
		/* GR falcons bootstrapped by ACR */
		nvgpu_log(g, gpu_dbg_gr, "bootstrap by ACR");
		err = 0;
	}

	return err;
}
#endif

static int gr_falcon_recovery_bootstrap(struct gk20a *g,
					struct nvgpu_gr_falcon *falcon)
{
	int err = 0;

#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
	if (!nvgpu_is_enabled(g, NVGPU_SEC_SECUREGPCCS)) {
		nvgpu_gr_falcon_load_gpccs_with_bootloader(g, falcon);
#ifdef CONFIG_NVGPU_LS_PMU
		err = nvgpu_pmu_lsfm_bootstrap_ls_falcon(g, g->pmu,
				g->pmu->lsfm, BIT32(FALCON_ID_FECS));
#endif
	} else
#endif
	{
		/* bind WPR VA inst block */
		nvgpu_gr_falcon_bind_instblk(g, falcon);
#if defined(CONFIG_NVGPU_DGPU) || defined(CONFIG_NVGPU_LS_PMU)
		err = gr_falcon_sec2_or_ls_pmu_recovery_bootstrap(g);
#else
		err = nvgpu_acr_bootstrap_hs_acr(g, g->acr);
		if (err != 0) {
			nvgpu_err(g,
				"ACR GR LSF bootstrap failed");
		}
#endif
	}

	return err;
}

static void gr_falcon_coldboot_bootstrap(struct gk20a *g,
					struct nvgpu_gr_falcon *falcon)
{
#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
	if (!nvgpu_is_enabled(g, NVGPU_SEC_SECUREGPCCS)) {
		nvgpu_gr_falcon_load_gpccs_with_bootloader(g, falcon);
	} else
#endif
	{
		/* bind WPR VA inst block */
		nvgpu_gr_falcon_bind_instblk(g, falcon);
	}
}

int nvgpu_gr_falcon_load_secure_ctxsw_ucode(struct gk20a *g,
					struct nvgpu_gr_falcon *falcon)
{
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		g->ops.gr.falcon.configure_fmodel(g);
	}
#endif

	if (falcon->coldboot_bootstrap_done) {
		nvgpu_log(g, gpu_dbg_gr, "recovery bootstrap");

		/* this must be recovery so bootstrap fecs and gpccs */
		err = gr_falcon_recovery_bootstrap(g, falcon);
		if (err != 0) {
			nvgpu_err(g, "Unable to recover GR falcon");
			return err;
		}

	} else {
		nvgpu_log(g, gpu_dbg_gr, "coldboot bootstrap");

		/* cold boot or rg exit */
		falcon->coldboot_bootstrap_done = true;
		gr_falcon_coldboot_bootstrap(g, falcon);
#if defined(CONFIG_NVGPU_DGPU) || defined(CONFIG_NVGPU_LS_PMU)
		err = gr_falcon_sec2_or_ls_pmu_coldboot_bootstrap(g);
		if (err != 0) {
			nvgpu_err(g, "Unable to boot GPCCS");
			return err;
		}
#endif
	}

	g->ops.gr.falcon.start_gpccs(g);
	g->ops.gr.falcon.start_fecs(g);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");

	return 0;
}

struct nvgpu_ctxsw_ucode_segments *nvgpu_gr_falcon_get_fecs_ucode_segments(
					struct nvgpu_gr_falcon *falcon)
{
	return &falcon->ctxsw_ucode_info.fecs;
}
struct nvgpu_ctxsw_ucode_segments *nvgpu_gr_falcon_get_gpccs_ucode_segments(
					struct nvgpu_gr_falcon *falcon)
{
	return &falcon->ctxsw_ucode_info.gpccs;
}
void *nvgpu_gr_falcon_get_surface_desc_cpu_va(struct nvgpu_gr_falcon *falcon)
{
	return falcon->ctxsw_ucode_info.surface_desc.cpu_va;
}
#ifdef CONFIG_NVGPU_ENGINE_RESET
struct nvgpu_mutex *nvgpu_gr_falcon_get_fecs_mutex(
					struct nvgpu_gr_falcon *falcon)
{
	return &falcon->fecs_mutex;
}
#endif
