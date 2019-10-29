/*
 * GV100 FB
 *
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/acr/nvgpu_acr.h>
#include <nvgpu/firmware.h>
#include <nvgpu/pmu.h>
#include <nvgpu/falcon.h>

#include "gk20a/gk20a.h"
#include "gv100/fb_gv100.h"
#include "gm20b/acr_gm20b.h"

#include <nvgpu/hw/gv100/hw_fb_gv100.h>
#include <nvgpu/hw/gv100/hw_falcon_gv100.h>
#include <nvgpu/hw/gv100/hw_mc_gv100.h>

#define HW_SCRUB_TIMEOUT_DEFAULT	100 /* usec */
#define HW_SCRUB_TIMEOUT_MAX		2000000 /* usec */
#define MEM_UNLOCK_TIMEOUT			3500 /* msec */

void gv100_fb_reset(struct gk20a *g)
{
	u32 val;
	int retries = HW_SCRUB_TIMEOUT_MAX / HW_SCRUB_TIMEOUT_DEFAULT;

	nvgpu_info(g, "reset gv100 fb");

	/* wait for memory to be accessible */
	do {
		u32 w = gk20a_readl(g, fb_niso_scrub_status_r());
		if (fb_niso_scrub_status_flag_v(w)) {
			nvgpu_info(g, "done");
			break;
		}
		nvgpu_udelay(HW_SCRUB_TIMEOUT_DEFAULT);
	} while (--retries);

	val = gk20a_readl(g, fb_mmu_priv_level_mask_r());
	val &= ~fb_mmu_priv_level_mask_write_violation_m();
	gk20a_writel(g, fb_mmu_priv_level_mask_r(), val);
}

int gv100_fb_memory_unlock(struct gk20a *g)
{
	struct nvgpu_firmware *mem_unlock_fw = NULL;
	struct bin_hdr *hsbin_hdr = NULL;
	struct acr_fw_header *fw_hdr = NULL;
	u32 *mem_unlock_ucode = NULL;
	u32 *mem_unlock_ucode_header = NULL;
	u32 sec_imem_dest = 0;
	u32 val = 0;
	int err = 0;

	nvgpu_log_fn(g, " ");

	nvgpu_log_info(g, "fb_mmu_vpr_info = 0x%08x",
			gk20a_readl(g, fb_mmu_vpr_info_r()));
	/*
	 * mem_unlock.bin should be written to install
	 * traps even if VPR isn’t actually supported
	 */
	mem_unlock_fw = nvgpu_request_firmware(g, "mem_unlock.bin", 0);
	if (!mem_unlock_fw) {
		nvgpu_err(g, "mem unlock ucode get fail");
		err = -ENOENT;
		goto exit;
	}

	/* Enable nvdec */
	g->ops.mc.enable(g, mc_enable_nvdec_enabled_f());

	/* nvdec falcon reset */
	nvgpu_flcn_reset(&g->nvdec_flcn);

	hsbin_hdr = (struct bin_hdr *)mem_unlock_fw->data;
	fw_hdr = (struct acr_fw_header *)(mem_unlock_fw->data +
			hsbin_hdr->header_offset);

	mem_unlock_ucode_header = (u32 *)(mem_unlock_fw->data +
		fw_hdr->hdr_offset);
	mem_unlock_ucode = (u32 *)(mem_unlock_fw->data +
		hsbin_hdr->data_offset);

	/* Patch Ucode singnatures */
	if (acr_ucode_patch_sig(g, mem_unlock_ucode,
		(u32 *)(mem_unlock_fw->data + fw_hdr->sig_prod_offset),
		(u32 *)(mem_unlock_fw->data + fw_hdr->sig_dbg_offset),
		(u32 *)(mem_unlock_fw->data + fw_hdr->patch_loc),
		(u32 *)(mem_unlock_fw->data + fw_hdr->patch_sig)) < 0) {
		nvgpu_err(g, "mem unlock patch signatures fail");
		err = -EPERM;
		goto exit;
	}

	/* Clear interrupts */
	nvgpu_flcn_set_irq(&g->nvdec_flcn, false, 0x0, 0x0);

	/* Copy Non Secure IMEM code */
	nvgpu_flcn_copy_to_imem(&g->nvdec_flcn, 0,
		(u8 *)&mem_unlock_ucode[
			mem_unlock_ucode_header[OS_CODE_OFFSET] >> 2],
		mem_unlock_ucode_header[OS_CODE_SIZE], 0, false,
		GET_IMEM_TAG(mem_unlock_ucode_header[OS_CODE_OFFSET]));

	/* Put secure code after non-secure block */
	sec_imem_dest = GET_NEXT_BLOCK(mem_unlock_ucode_header[OS_CODE_SIZE]);

	nvgpu_flcn_copy_to_imem(&g->nvdec_flcn, sec_imem_dest,
		(u8 *)&mem_unlock_ucode[
			mem_unlock_ucode_header[APP_0_CODE_OFFSET] >> 2],
		mem_unlock_ucode_header[APP_0_CODE_SIZE], 0, true,
		GET_IMEM_TAG(mem_unlock_ucode_header[APP_0_CODE_OFFSET]));

	/* load DMEM: ensure that signatures are patched */
	nvgpu_flcn_copy_to_dmem(&g->nvdec_flcn, 0, (u8 *)&mem_unlock_ucode[
		mem_unlock_ucode_header[OS_DATA_OFFSET] >> 2],
		mem_unlock_ucode_header[OS_DATA_SIZE], 0);

	nvgpu_log_info(g, "nvdec sctl reg %x\n",
		gk20a_readl(g, g->nvdec_flcn.flcn_base +
		falcon_falcon_sctl_r()));

	/* set BOOTVEC to start of non-secure code */
	nvgpu_flcn_bootstrap(&g->nvdec_flcn, 0);

	/* wait for complete & halt */
	nvgpu_flcn_wait_for_halt(&g->nvdec_flcn, MEM_UNLOCK_TIMEOUT);

	/* check mem unlock status */
	val = nvgpu_flcn_mailbox_read(&g->nvdec_flcn, 0);
	if (val) {
		nvgpu_err(g, "memory unlock failed, err %x", val);
		err = -1;
		goto exit;
	}

	nvgpu_log_info(g, "nvdec sctl reg %x\n",
		gk20a_readl(g, g->nvdec_flcn.flcn_base +
		falcon_falcon_sctl_r()));

exit:
	if (mem_unlock_fw)
		nvgpu_release_firmware(g, mem_unlock_fw);

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
