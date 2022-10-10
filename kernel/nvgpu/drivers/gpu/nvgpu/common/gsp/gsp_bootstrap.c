/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/falcon.h>
#include <nvgpu/dma.h>
#include <nvgpu/timers.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/firmware.h>
#include <nvgpu/io.h>
#include <nvgpu/gsp.h>
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
#include <nvgpu/gsp/gsp_test.h>
#endif

static void gsp_release_firmware(struct gk20a *g, struct nvgpu_gsp *gsp)
{
	if (gsp->gsp_ucode.manifest != NULL) {
		nvgpu_release_firmware(g, gsp->gsp_ucode.manifest);
	}

	if (gsp->gsp_ucode.code != NULL) {
		nvgpu_release_firmware(g, gsp->gsp_ucode.code);
	}

	if (gsp->gsp_ucode.data != NULL) {
		nvgpu_release_firmware(g, gsp->gsp_ucode.data);
	}
}

static int gsp_read_firmware(struct gk20a *g, struct nvgpu_gsp *gsp,
		struct gsp_fw *gsp_ucode)
{
	const char *code_name = gsp_ucode->code_name;
	const char *data_name = gsp_ucode->data_name;
	const char *manifest_name = gsp_ucode->manifest_name;

	nvgpu_log_fn(g, " ");

	gsp_ucode->manifest = nvgpu_request_firmware(g,
		manifest_name, NVGPU_REQUEST_FIRMWARE_NO_WARN);
	if (gsp_ucode->manifest == NULL) {
		nvgpu_err(g, "%s ucode get failed", manifest_name);
		goto fw_release;
	}

	gsp_ucode->code = nvgpu_request_firmware(g,
		code_name, NVGPU_REQUEST_FIRMWARE_NO_WARN);
	if (gsp_ucode->code == NULL) {
		nvgpu_err(g, "%s ucode get failed", code_name);
		goto fw_release;
	}

	gsp_ucode->data = nvgpu_request_firmware(g,
		data_name, NVGPU_REQUEST_FIRMWARE_NO_WARN);
	if (gsp_ucode->data == NULL) {
		nvgpu_err(g, "%s ucode get failed", data_name);
		goto fw_release;
	}

	return 0;

fw_release:
	gsp_release_firmware(g, gsp);
	return -ENOENT;
}

static int gsp_ucode_load_and_bootstrap(struct gk20a *g,
		struct nvgpu_falcon *flcn, struct gsp_fw *gsp_ucode)
{
	u32 dmem_size = 0U;
	u32 code_size = (u32)gsp_ucode->code->size;
	u32 data_size = (u32)gsp_ucode->data->size;
	u32 manifest_size = (u32)gsp_ucode->manifest->size;
	int err = 0;

	nvgpu_log_fn(g, " ");

	g->ops.falcon.set_bcr(flcn);
	err = nvgpu_falcon_get_mem_size(flcn, MEM_DMEM, &dmem_size);
	if (err != 0) {
		nvgpu_err(g, "gsp NVRISCV get DMEM size failed");
		goto exit;
	}

	if ((data_size + manifest_size) > dmem_size) {
		nvgpu_err(g, "gsp DMEM might overflow");
		err = -ENOMEM;
		goto exit;
	}

	err = nvgpu_falcon_copy_to_imem(flcn, 0x0, gsp_ucode->code->data,
			code_size, 0, true, 0x0);
	if (err != 0) {
		nvgpu_err(g, "gsp NVRISCV code copy to IMEM failed");
		goto exit;
	}

	err = nvgpu_falcon_copy_to_dmem(flcn, 0x0, gsp_ucode->data->data,
			data_size, 0x0);
	if (err != 0) {
		nvgpu_err(g, "gsp NVRISCV data copy to DMEM failed");
		goto exit;
	}

	err = nvgpu_falcon_copy_to_dmem(flcn, (dmem_size - manifest_size),
			gsp_ucode->manifest->data, manifest_size, 0x0);
	if (err != 0) {
		nvgpu_err(g, "gsp NVRISCV manifest copy to DMEM failed");
		goto exit;
	}

	/*
	 * Write zero value to mailbox-0 register which is updated by
	 * gsp ucode to denote its return status.
	 */
	nvgpu_falcon_mailbox_write(flcn, FALCON_MAILBOX_0, 0x0U);
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
	/*
	 * Update the address of the allocated sysmem block in the
	 * mailbox register for stress test.
	 */
	if (nvgpu_gsp_get_stress_test_load(g)) {
		nvgpu_gsp_write_test_sysmem_addr(g);
	}
#endif

	g->ops.falcon.bootstrap(flcn, 0x0);
exit:
	return err;
}

int nvgpu_gsp_wait_for_mailbox_update(struct nvgpu_gsp *gsp,
		u32 mailbox_index, u32 exp_value, signed int timeoutms)
{
	u32 mail_box_data = 0;
	struct nvgpu_falcon *flcn = gsp->gsp_flcn;

	nvgpu_log_fn(flcn->g, " ");

	do {
		mail_box_data = flcn->g->ops.falcon.mailbox_read(
				flcn, mailbox_index);
		if (mail_box_data == exp_value) {
			nvgpu_info(flcn->g,
				"gsp mailbox-0 updated successful with 0x%x",
				mail_box_data);
			break;
		}

		if (timeoutms <= 0) {
			nvgpu_err(flcn->g, "gsp mailbox check timedout");
			return -1;
		}

		nvgpu_msleep(10);
		timeoutms -= 10;

	} while (true);

	return 0;
}

int nvgpu_gsp_wait_for_priv_lockdown_release(struct nvgpu_gsp *gsp,
		signed int timeoutms)
{
	struct nvgpu_falcon *flcn = gsp->gsp_flcn;

	nvgpu_log_fn(gsp->g, " ");

	do {
		if (!gsp->g->ops.falcon.is_priv_lockdown(flcn)) {
			break;
		}

		if (timeoutms <= 0) {
			nvgpu_err(gsp->g, "gsp priv lockdown release timedout");
			return -1;
		}

		nvgpu_msleep(10);
		timeoutms -= 10;
	} while (true);

	return 0;
}

int nvgpu_gsp_bootstrap_ns(struct gk20a *g, struct nvgpu_gsp *gsp)
{
	int err = 0;
	struct gsp_fw *gsp_ucode = &gsp->gsp_ucode;
	struct nvgpu_falcon *flcn = gsp->gsp_flcn;

	nvgpu_log_fn(g, " ");

	err = gsp_read_firmware(g, gsp, gsp_ucode);
	if (err != 0) {
		nvgpu_err(g, "gsp firmware reading failed");
		goto exit;
	}

	/* core reset */
	err = nvgpu_falcon_reset(flcn);
	if (err != 0) {
		nvgpu_err(g, "gsp core reset failed err=%d", err);
		goto exit;
	}

	/* Enable required interrupts support and isr */
	nvgpu_gsp_isr_support(g, gsp, true);

	err = gsp_ucode_load_and_bootstrap(g, flcn, gsp_ucode);
	if (err != 0) {
		nvgpu_err(g, "gsp load and bootstrap failed");
		goto exit;
	}

	err = nvgpu_falcon_wait_for_nvriscv_brom_completion(flcn);
	if (err != 0) {
		nvgpu_err(g, "gsp BROM failed");
	}

exit:
	gsp_release_firmware(g, gsp);
	return err;
}
