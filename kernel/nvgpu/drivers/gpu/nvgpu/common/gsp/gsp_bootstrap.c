/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#include "gsp_priv.h"
#include "gsp_bootstrap.h"

#define GSP_WAIT_TIME_MS 10000U
#define GSP_DBG_RISCV_FW_MANIFEST  "sample-gsp.manifest.encrypt.bin.out.bin"
#define GSP_DBG_RISCV_FW_CODE      "sample-gsp.text.encrypt.bin"
#define GSP_DBG_RISCV_FW_DATA      "sample-gsp.data.encrypt.bin"

#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
#define GSPDBG_RISCV_STRESS_TEST_FW_MANIFEST  "gsp-stress.manifest.encrypt.bin.out.bin"
#define GSPDBG_RISCV_STRESS_TEST_FW_CODE      "gsp-stress.text.encrypt.bin"
#define GSPDBG_RISCV_STRESS_TEST_FW_DATA      "gsp-stress.data.encrypt.bin"

#define GSPPROD_RISCV_STRESS_TEST_FW_MANIFEST  "gsp-stress.manifest.encrypt.bin.out.bin.prod"
#define GSPPROD_RISCV_STRESS_TEST_FW_CODE      "gsp-stress.text.encrypt.bin.prod"
#define GSPPROD_RISCV_STRESS_TEST_FW_DATA      "gsp-stress.data.encrypt.bin.prod"

#define GSP_STRESS_TEST_MAILBOX_PASS 0xAAAAAAAA
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

static int gsp_read_firmware(struct gk20a *g, struct gsp_fw *gsp_ucode)
{
	const char *gsp_code_name;
	const char *gsp_data_name;
	const char *gsp_manifest_name;

	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
	if (g->gsp->gsp_test.load_stress_test) {
		/*
		 * TODO Switch to GSP specific register
		 */
		if (g->ops.pmu.is_debug_mode_enabled(g)) {
			gsp_code_name = GSPDBG_RISCV_STRESS_TEST_FW_CODE;
			gsp_data_name = GSPDBG_RISCV_STRESS_TEST_FW_DATA;
			gsp_manifest_name = GSPDBG_RISCV_STRESS_TEST_FW_MANIFEST;
		} else {
			gsp_code_name = GSPPROD_RISCV_STRESS_TEST_FW_CODE;
			gsp_data_name = GSPPROD_RISCV_STRESS_TEST_FW_DATA;
			gsp_manifest_name = GSPPROD_RISCV_STRESS_TEST_FW_MANIFEST;
		}
	} else
#endif
	{
		gsp_code_name = GSP_DBG_RISCV_FW_CODE;
		gsp_data_name = GSP_DBG_RISCV_FW_DATA;
		gsp_manifest_name = GSP_DBG_RISCV_FW_MANIFEST;
	}

	gsp_ucode->manifest = nvgpu_request_firmware(g,
		gsp_manifest_name, NVGPU_REQUEST_FIRMWARE_NO_WARN);
	if (gsp_ucode->manifest == NULL) {
		nvgpu_err(g, "%s ucode get failed", gsp_manifest_name);
		goto fw_release;
	}

	gsp_ucode->code = nvgpu_request_firmware(g,
		gsp_code_name, NVGPU_REQUEST_FIRMWARE_NO_WARN);
	if (gsp_ucode->code == NULL) {
		nvgpu_err(g, "%s ucode get failed", gsp_code_name);
		goto fw_release;
	}

	gsp_ucode->data = nvgpu_request_firmware(g,
		gsp_data_name, NVGPU_REQUEST_FIRMWARE_NO_WARN);
	if (gsp_ucode->data == NULL) {
		nvgpu_err(g, "%s ucode get failed", gsp_data_name);
		goto fw_release;
	}

	return 0;

fw_release:
	gsp_release_firmware(g, g->gsp);
	return -ENOENT;
}

#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
static void gsp_write_test_sysmem_addr(struct nvgpu_gsp *gsp)
{
	struct gk20a *g;
	struct nvgpu_falcon *flcn;
	u64 sysmem_addr;

	g = gsp->g;
	flcn = gsp->gsp_flcn;

	sysmem_addr = nvgpu_mem_get_addr(g, &gsp->gsp_test.gsp_test_sysmem_block);

	nvgpu_falcon_mailbox_write(flcn, FALCON_MAILBOX_0, u64_lo32(sysmem_addr));

	nvgpu_falcon_mailbox_write(flcn, FALCON_MAILBOX_1, u64_hi32(sysmem_addr));

}
#endif

static int gsp_ucode_load_and_bootstrap(struct gk20a *g,
		struct nvgpu_falcon *flcn,
		struct gsp_fw *gsp_ucode)
{
	u32 dmem_size = 0U;
	u32 code_size = gsp_ucode->code->size;
	u32 data_size = gsp_ucode->data->size;
	u32 manifest_size = gsp_ucode->manifest->size;
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
	if (g->gsp->gsp_test.load_stress_test)
		gsp_write_test_sysmem_addr(g->gsp);
#endif

	g->ops.falcon.bootstrap(flcn, 0x0);
exit:
	return err;
}

static int gsp_check_for_brom_completion(struct nvgpu_falcon *flcn,
		signed int timeoutms)
{
	u32 reg = 0;

	nvgpu_log_fn(flcn->g, " ");

	do {
		reg = flcn->g->ops.falcon.get_brom_retcode(flcn);
		if (flcn->g->ops.falcon.check_brom_passed(reg)) {
			break;
		}

		if (timeoutms <= 0) {
			nvgpu_err(flcn->g, "gsp BROM execution check timedout");
			goto exit;
		}

		nvgpu_msleep(10);
		timeoutms -= 10;

	} while (true);

	if ((reg & 0x3) == 0x2) {
		nvgpu_err(flcn->g, "gsp BROM execution failed");
		goto exit;
	}

	return 0;
exit:
	flcn->g->ops.falcon.dump_brom_stats(flcn);
	return -1;
}

static int gsp_wait_for_mailbox_update(struct nvgpu_gsp *gsp,
		u32 mailbox_index, signed int timeoutms)
{
	u32 mail_box_data = 0;
	u32 pass_val = 0;
	struct nvgpu_falcon *flcn = gsp->gsp_flcn;

	nvgpu_log_fn(flcn->g, " ");

#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
	if (gsp->gsp_test.load_stress_test) {
		pass_val = GSP_STRESS_TEST_MAILBOX_PASS;
	}
#endif

	do {
		mail_box_data = flcn->g->ops.falcon.mailbox_read(
				flcn, mailbox_index);
		if (mail_box_data != 0U) {
			if ((pass_val == 0U) || (mail_box_data == pass_val)) {
				nvgpu_info(flcn->g,
					"gsp mailbox-0 updated successful with 0x%x",
					mail_box_data);
				break;
			}
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

int gsp_bootstrap_ns(struct gk20a *g, struct nvgpu_gsp *gsp)
{
	int err = 0;
	struct gsp_fw *gsp_ucode = &gsp->gsp_ucode;
	struct nvgpu_falcon *flcn = gsp->gsp_flcn;

	nvgpu_log_fn(g, " ");

	err = gsp_read_firmware(g, gsp_ucode);
	if (err != 0) {
		nvgpu_err(g, "gsp firmware reading failed");
		goto exit;
	}

	/* core reset */
	err = nvgpu_falcon_reset(gsp->gsp_flcn);
	if (err != 0) {
		nvgpu_err(g, "gsp core reset failed err=%d", err);
		goto exit;
	}

	/* Enable required interrupts support and isr */
	nvgpu_gsp_isr_support(g, true);

	/* setup falcon apertures */
	if (flcn->flcn_engine_dep_ops.setup_bootstrap_config != NULL) {
		flcn->flcn_engine_dep_ops.setup_bootstrap_config(flcn->g);
	}

	err = gsp_ucode_load_and_bootstrap(g, gsp->gsp_flcn, gsp_ucode);
	if (err != 0) {
		nvgpu_err(g, "gsp load and bootstrap failed");
		goto exit;
	}

	err = gsp_check_for_brom_completion(gsp->gsp_flcn, GSP_WAIT_TIME_MS);
	if (err != 0) {
		nvgpu_err(g, "gsp BROM failed");
		goto exit;
	}

	/* wait for mailbox-0 update with non-zero value */
	err = gsp_wait_for_mailbox_update(gsp, 0x0, GSP_WAIT_TIME_MS);
	if (err != 0) {
		nvgpu_err(g, "gsp ucode failed to update mailbox-0");
	}

exit:
	gsp_release_firmware(g, g->gsp);
	return err;
}
