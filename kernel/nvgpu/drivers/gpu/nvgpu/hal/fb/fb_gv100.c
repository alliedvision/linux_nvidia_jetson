/*
 * GV100 FB
 *
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

#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/nvgpu_common.h>
#include <nvgpu/kmem.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/firmware.h>
#include <nvgpu/pmu.h>
#include <nvgpu/falcon.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/timers.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/firmware.h>
#include <nvgpu/mc.h>

#include "fb_gv100.h"

#include <nvgpu/hw/gv100/hw_fb_gv100.h>

#define HW_SCRUB_TIMEOUT_DEFAULT	100 /* usec */
#define HW_SCRUB_TIMEOUT_MAX		2000000 /* usec */
#define MEM_UNLOCK_TIMEOUT			3500 /* msec */

#define MEM_UNLOCK_PROD_BIN		"mem_unlock.bin"
#define MEM_UNLOCK_DBG_BIN		"mem_unlock_dbg.bin"

struct mem_unlock_bin_hdr {
	u32 bin_magic;
	u32 bin_ver;
	u32 bin_size;
	u32 header_offset;
	u32 data_offset;
	u32 data_size;
};

struct mem_unlock_fw_header {
	u32 sig_dbg_offset;
	u32 sig_dbg_size;
	u32 sig_prod_offset;
	u32 sig_prod_size;
	u32 patch_loc;
	u32 patch_sig;
	u32 hdr_offset;
	u32 hdr_size;
};

void gv100_fb_reset(struct gk20a *g)
{
	u32 val;
	int retries = HW_SCRUB_TIMEOUT_MAX / HW_SCRUB_TIMEOUT_DEFAULT;

	nvgpu_log_info(g, "reset gv100 fb");

	/* wait for memory to be accessible */
	do {
		u32 w = gk20a_readl(g, fb_niso_scrub_status_r());
		if (fb_niso_scrub_status_flag_v(w) != 0U) {
			nvgpu_log_info(g, "done");
			break;
		}
		nvgpu_udelay(HW_SCRUB_TIMEOUT_DEFAULT);
		--retries;
	} while (retries != 0);

	val = gk20a_readl(g, fb_mmu_priv_level_mask_r());
	val &= ~fb_mmu_priv_level_mask_write_violation_m();
	gk20a_writel(g, fb_mmu_priv_level_mask_r(), val);
}

/*
 * Patch signatures into ucode image
 */
static int fb_ucode_patch_sig(struct gk20a *g,
	unsigned int *p_img, unsigned int *p_prod_sig,
	unsigned int *p_dbg_sig, unsigned int *p_patch_loc,
	unsigned int *p_patch_ind, u32 sig_size)
{
	unsigned int i, j, *p_sig;

	if (!g->ops.pmu.is_debug_mode_enabled(g)) {
		p_sig = p_prod_sig;
	} else {
		p_sig = p_dbg_sig;
	}

	/* Patching logic:*/
	sig_size = sig_size / 4U;
	for (i = 0U; i < sizeof(*p_patch_loc)>>2U; i++) {
		for (j = 0U; j < sig_size; j++) {
			p_img[nvgpu_safe_add_u32((p_patch_loc[i]>>2U), j)] =
				p_sig[nvgpu_safe_add_u32((p_patch_ind[i]<<2U),
					j)];
		}
	}

	return 0;
}

int gv100_fb_memory_unlock(struct gk20a *g)
{
	struct nvgpu_firmware *mem_unlock_fw = NULL;
	struct mem_unlock_bin_hdr *hs_bin_hdr = NULL;
	struct mem_unlock_fw_header *fw_hdr = NULL;
	u32 *ucode_header = NULL;
	u32 *ucode = NULL;
	u32 data = 0;
	int err = 0;

	nvgpu_log_fn(g, " ");

	/*
	 * mem_unlock.bin should be written to install
	 * traps even if VPR isn’t actually supported
	 */
	if (!g->ops.pmu.is_debug_mode_enabled(g)) {
		mem_unlock_fw = nvgpu_request_firmware(g, MEM_UNLOCK_PROD_BIN, 0);
	} else {
		mem_unlock_fw = nvgpu_request_firmware(g, MEM_UNLOCK_DBG_BIN, 0);
	}
	if (mem_unlock_fw == NULL) {
		nvgpu_err(g, "mem unlock ucode get fail");
		err = -ENOENT;
		goto exit;
	}

	/* Enable nvdec */
	err = nvgpu_mc_reset_units(g, NVGPU_UNIT_NVDEC);
	if (err != 0) {
		nvgpu_err(g, "Failed to reset NVDEC unit");
	}

	hs_bin_hdr = (struct mem_unlock_bin_hdr *)(void *)mem_unlock_fw->data;
	fw_hdr = (struct mem_unlock_fw_header *)(void *)(mem_unlock_fw->data +
			hs_bin_hdr->header_offset);
	ucode_header = (u32 *)(void *)(mem_unlock_fw->data +
					fw_hdr->hdr_offset);
	ucode = (u32 *)(void *)(mem_unlock_fw->data + hs_bin_hdr->data_offset);

	/* Patch Ucode signatures */
	if (fb_ucode_patch_sig(g, ucode,
		(u32 *)(void *)(mem_unlock_fw->data + fw_hdr->sig_prod_offset),
		(u32 *)(void *)(mem_unlock_fw->data + fw_hdr->sig_dbg_offset),
		(u32 *)(void *)(mem_unlock_fw->data + fw_hdr->patch_loc),
		(u32 *)(void *)(mem_unlock_fw->data + fw_hdr->patch_sig),
		fw_hdr->sig_dbg_size) < 0) {
		nvgpu_err(g, "mem unlock ucode patch signatures fail");
		err = -EPERM;
		goto exit;
	}

	err = nvgpu_falcon_hs_ucode_load_bootstrap(&g->nvdec_flcn, ucode,
			ucode_header);
	if (err != 0) {
		nvgpu_err(g, "mem unlock ucode load & bootstrap failed");
		goto exit;
	}

	if (nvgpu_falcon_wait_for_halt(&g->nvdec_flcn,
		MEM_UNLOCK_TIMEOUT) != 0) {
		nvgpu_err(g, "mem unlock ucode boot timed out");
#ifdef CONFIG_NVGPU_FALCON_DEBUG
		nvgpu_falcon_dump_stats(&g->nvdec_flcn);
#endif
		goto exit;
	}

	data = nvgpu_falcon_mailbox_read(&g->nvdec_flcn, FALCON_MAILBOX_0);
	if (data != 0U) {
		nvgpu_err(g, "mem unlock ucode boot failed, err %x", data);
		goto exit;
	}

exit:
	if (mem_unlock_fw != NULL) {
		nvgpu_release_firmware(g, mem_unlock_fw);
	}

	nvgpu_log_fn(g, "done, status - %d", err);

	return err;
}

int gv100_fb_init_nvlink(struct gk20a *g)
{
	u32 data;
	u32 mask = g->nvlink.enabled_links;

	/* Map enabled link to SYSMEM */
	data = nvgpu_readl(g, fb_hshub_config0_r());
	data = set_field(data, fb_hshub_config0_sysmem_nvlink_mask_m(),
			fb_hshub_config0_sysmem_nvlink_mask_f(mask));
	nvgpu_writel(g, fb_hshub_config0_r(), data);

	return 0;
}

int gv100_fb_enable_nvlink(struct gk20a *g)
{
	u32 data;

	nvgpu_log(g, gpu_dbg_nvlink|gpu_dbg_info, "enabling nvlink");

	/* Enable nvlink for NISO FBHUB */
	data = nvgpu_readl(g, fb_niso_cfg1_r());
	data = set_field(data, fb_niso_cfg1_sysmem_nvlink_m(),
		fb_niso_cfg1_sysmem_nvlink_enabled_f());
	nvgpu_writel(g, fb_niso_cfg1_r(), data);

	return 0;
}

int gv100_fb_set_atomic_mode(struct gk20a *g)
{
	u32 data;

	/* Setup atomics */
	data = nvgpu_readl(g, fb_mmu_ctrl_r());
	data = set_field(data, fb_mmu_ctrl_atomic_capability_mode_m(),
		fb_mmu_ctrl_atomic_capability_mode_rmw_f());
	nvgpu_writel(g, fb_mmu_ctrl_r(), data);

	data = nvgpu_readl(g, fb_hsmmu_pri_mmu_ctrl_r());
	data = set_field(data, fb_hsmmu_pri_mmu_ctrl_atomic_capability_mode_m(),
		    fb_hsmmu_pri_mmu_ctrl_atomic_capability_mode_rmw_f());
	nvgpu_writel(g, fb_hsmmu_pri_mmu_ctrl_r(), data);

	data = nvgpu_readl(g, fb_fbhub_num_active_ltcs_r());
	data = set_field(data, fb_fbhub_num_active_ltcs_hub_sys_atomic_mode_m(),
		    fb_fbhub_num_active_ltcs_hub_sys_atomic_mode_use_rmw_f());
	nvgpu_writel(g, fb_fbhub_num_active_ltcs_r(), data);

	data = nvgpu_readl(g, fb_hshub_num_active_ltcs_r());
	data = set_field(data, fb_hshub_num_active_ltcs_hub_sys_atomic_mode_m(),
		    fb_hshub_num_active_ltcs_hub_sys_atomic_mode_use_rmw_f());
	nvgpu_writel(g, fb_hshub_num_active_ltcs_r(), data);

	return 0;
}

#ifdef CONFIG_NVGPU_DGPU
size_t gv100_fb_get_vidmem_size(struct gk20a *g)
{
	u32 range = gk20a_readl(g, fb_mmu_local_memory_range_r());
	u32 mag = fb_mmu_local_memory_range_lower_mag_v(range);
	u32 scale = fb_mmu_local_memory_range_lower_scale_v(range);
	u32 ecc = fb_mmu_local_memory_range_ecc_mode_v(range);
	size_t bytes = ((size_t)mag << scale) * SZ_1M;

	if (ecc != 0U) {
		bytes = bytes / 16U * 15U;
	}

	return bytes;
}
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
void gv100_fb_set_mmu_debug_mode(struct gk20a *g, bool enable)
{
	u32 data, fb_ctrl, hsmmu_ctrl;

	if (enable) {
		fb_ctrl = fb_mmu_debug_ctrl_debug_enabled_f();
		hsmmu_ctrl = fb_hsmmu_pri_mmu_debug_ctrl_debug_enabled_f();
		g->mmu_debug_ctrl = true;
	} else {
		fb_ctrl = fb_mmu_debug_ctrl_debug_disabled_f();
		hsmmu_ctrl = fb_hsmmu_pri_mmu_debug_ctrl_debug_disabled_f();
		g->mmu_debug_ctrl = false;
	}

	data = nvgpu_readl(g, fb_mmu_debug_ctrl_r());
	data = set_field(data, fb_mmu_debug_ctrl_debug_m(), fb_ctrl);
	nvgpu_writel(g, fb_mmu_debug_ctrl_r(), data);

	data = nvgpu_readl(g, fb_hsmmu_pri_mmu_debug_ctrl_r());
	data = set_field(data,
			fb_hsmmu_pri_mmu_debug_ctrl_debug_m(), hsmmu_ctrl);
	nvgpu_writel(g, fb_hsmmu_pri_mmu_debug_ctrl_r(), data);
}
#endif
