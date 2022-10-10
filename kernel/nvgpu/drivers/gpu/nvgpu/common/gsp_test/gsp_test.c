// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * GSP Test Functions
 *
 * Copyright (c) 2021-2022, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <nvgpu/firmware.h>
#include <nvgpu/falcon.h>
#include <nvgpu/dma.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/log.h>
#include <nvgpu/gsp.h>
#include <nvgpu/gsp/gsp_test.h>

#include "gsp_test.h"

u32 nvgpu_gsp_get_current_iteration(struct gk20a *g)
{
	u32 data = 0;
	struct nvgpu_gsp *gsp = g->gsp_stest->gsp;

	nvgpu_log_fn(g, " ");

	data = nvgpu_falcon_mailbox_read(gsp->gsp_flcn, FALCON_MAILBOX_1);

	return data;
}

u32 nvgpu_gsp_get_current_test(struct gk20a *g)
{
	u32 data = 0;
	struct nvgpu_gsp *gsp = g->gsp_stest->gsp;

	nvgpu_log_fn(g, " ");

	data = nvgpu_falcon_mailbox_read(gsp->gsp_flcn, FALCON_MAILBOX_0);

	return data;
}

bool nvgpu_gsp_get_test_fail_status(struct gk20a *g)
{
	struct nvgpu_gsp_test *gsp_stest = g->gsp_stest;

	return gsp_stest->gsp_test.stress_test_fail_status;
}

bool nvgpu_gsp_get_stress_test_start(struct gk20a *g)
{
	struct nvgpu_gsp_test *gsp_stest = g->gsp_stest;

	return gsp_stest->gsp_test.enable_stress_test;
}

bool nvgpu_gsp_get_stress_test_load(struct gk20a *g)
{
	struct nvgpu_gsp_test *gsp_stest = g->gsp_stest;

	if (gsp_stest == NULL)
		return false;

	return gsp_stest->gsp_test.load_stress_test;
}

void nvgpu_gsp_set_test_fail_status(struct gk20a *g, bool val)
{
	struct nvgpu_gsp_test *gsp_stest = g->gsp_stest;

	gsp_stest->gsp_test.stress_test_fail_status = val;
}

int nvgpu_gsp_set_stress_test_start(struct gk20a *g, bool flag)
{
	int err = 0;
	struct nvgpu_gsp *gsp = g->gsp_stest->gsp;
	struct nvgpu_gsp_test *gsp_stest = g->gsp_stest;

	nvgpu_log_fn(g, " ");

	if (flag) {
		nvgpu_info(g, "Enabling GSP test");
		nvgpu_falcon_mailbox_write(gsp->gsp_flcn, FALCON_MAILBOX_1, 0xFFFFFFFF);
	} else {
		nvgpu_info(g, "Halting GSP test");
		nvgpu_gsp_stress_test_halt(g, false);
	}

	gsp_stest->gsp_test.enable_stress_test = flag;

	return err;
}

int nvgpu_gsp_set_stress_test_load(struct gk20a *g, bool flag)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (flag)
		err = nvgpu_gsp_stress_test_bootstrap(g, flag);

	return err;
}

static void gsp_test_get_file_names(struct gk20a *g, struct gsp_fw *gsp_ucode)
{
	/*
	 * TODO Switch to GSP specific register
	 */
	if (g->ops.pmu.is_debug_mode_enabled(g)) {
		gsp_ucode->code_name = GSPDBG_RISCV_STRESS_TEST_FW_CODE;
		gsp_ucode->data_name = GSPDBG_RISCV_STRESS_TEST_FW_DATA;
		gsp_ucode->manifest_name = GSPDBG_RISCV_STRESS_TEST_FW_MANIFEST;
	} else {
		gsp_ucode->code_name = GSPPROD_RISCV_STRESS_TEST_FW_CODE;
		gsp_ucode->data_name = GSPPROD_RISCV_STRESS_TEST_FW_DATA;
		gsp_ucode->manifest_name = GSPPROD_RISCV_STRESS_TEST_FW_MANIFEST;
	}
}

void nvgpu_gsp_write_test_sysmem_addr(struct gk20a *g)
{
	struct nvgpu_gsp *gsp = g->gsp_stest->gsp;
	struct nvgpu_falcon *flcn;
	u64 sysmem_addr;
	struct nvgpu_gsp_test *gsp_stest;

	flcn = gsp->gsp_flcn;
	gsp_stest = g->gsp_stest;

	sysmem_addr = nvgpu_mem_get_addr(g, &gsp_stest->gsp_test.gsp_test_sysmem_block);

	nvgpu_falcon_mailbox_write(flcn, FALCON_MAILBOX_0, u64_lo32(sysmem_addr));

	nvgpu_falcon_mailbox_write(flcn, FALCON_MAILBOX_1, u64_hi32(sysmem_addr));
}

int nvgpu_gsp_stress_test_bootstrap(struct gk20a *g, bool start)
{
	int err = 0;
	struct nvgpu_gsp *gsp = g->gsp_stest->gsp;
	struct nvgpu_gsp_test *gsp_stest = g->gsp_stest;

	nvgpu_log_fn(g, " ");

	if (gsp_stest == NULL) {
		nvgpu_err(g, "GSP not initialized");
		err = -EFAULT;
		goto exit;
	}

	if (!start && !(gsp_stest->gsp_test.load_stress_test))
		return err;

	if (start) {
		err = nvgpu_dma_alloc_flags_sys(g,
						NVGPU_DMA_PHYSICALLY_ADDRESSED,
						SZ_64K,
						&g->gsp_stest->gsp_test.gsp_test_sysmem_block);
		if (err != 0) {
			nvgpu_err(g, "GSP test memory alloc failed");
			goto exit;
		}
	}

	gsp_stest->gsp_test.load_stress_test = true;

#ifdef CONFIG_NVGPU_FALCON_DEBUG
	err = nvgpu_gsp_debug_buf_init(g, GSP_TEST_DEBUG_BUFFER_QUEUE,
			GSP_TEST_DMESG_BUFFER_SIZE);
	if (err != 0) {
		nvgpu_err(g, "GSP sched debug buf init failed");
		goto exit;
	}
#endif

	gsp_test_get_file_names(g, &gsp->gsp_ucode);

	err = nvgpu_gsp_bootstrap_ns(g, gsp);
	if (err != 0) {
		nvgpu_err(g, "GSP bootstrap failed for stress test");
		goto exit;
	}

	/* wait for mailbox-0 update with non-zero value */
	err = nvgpu_gsp_wait_for_mailbox_update(gsp, 0x0,
				GSP_STRESS_TEST_MAILBOX_PASS, GSP_WAIT_TIME_MS);
	if (err != 0) {
		nvgpu_err(g, "gsp ucode failed to update mailbox-0");
	}

	if (gsp_stest->gsp_test.enable_stress_test) {
		nvgpu_info(g, "Restarting GSP stress test");
		nvgpu_falcon_mailbox_write(gsp->gsp_flcn, FALCON_MAILBOX_1, 0xFFFFFFFF);
	}

	return err;

exit:
	gsp_stest->gsp_test.load_stress_test = false;
	return err;
}

int nvgpu_gsp_stress_test_halt(struct gk20a *g, bool restart)
{
	int err = 0;
	struct nvgpu_gsp *gsp = g->gsp_stest->gsp;
	struct nvgpu_gsp_test *gsp_stest = g->gsp_stest;


	nvgpu_log_fn(g, " ");

	if (gsp == NULL) {
		nvgpu_info(g, "GSP not initialized");
		goto exit;
	}

	nvgpu_gsp_suspend(g, gsp);

	if (restart && (gsp_stest->gsp_test.load_stress_test == false)) {
		nvgpu_info(g, "GSP stress test not loaded ");
		goto exit;
	}

	err = nvgpu_falcon_reset(gsp->gsp_flcn);
	if (err != 0) {
		nvgpu_err(g, "gsp reset failed err=%d", err);
		goto exit;
	}

	if (!restart) {
		gsp_stest->gsp_test.load_stress_test = false;
		nvgpu_dma_free(g, &gsp_stest->gsp_test.gsp_test_sysmem_block);
	}

exit:
	return err;
}

bool nvgpu_gsp_is_stress_test(struct gk20a *g)
{
	if (g->gsp_stest->gsp_test.load_stress_test)
		return true;
	else
		return false;
}

static void gsp_test_sw_deinit(struct gk20a *g, struct nvgpu_gsp_test *gsp_stest)
{
	nvgpu_dma_free(g, &gsp_stest->gsp_test.gsp_test_sysmem_block);

	nvgpu_kfree(g, gsp_stest);
	gsp_stest = NULL;
}

void nvgpu_gsp_test_sw_deinit(struct gk20a *g)
{
	struct nvgpu_gsp_test *gsp_stest = g->gsp_stest;
	nvgpu_log_fn(g, " ");

	if (gsp_stest == NULL) {
		nvgpu_info(g, "GSP stest not initialized");
		return;
	}

	if (gsp_stest->gsp != NULL) {
		nvgpu_gsp_sw_deinit(g, gsp_stest->gsp);
	}

	if (gsp_stest != NULL) {
		gsp_test_sw_deinit(g, gsp_stest);
	}
}

int nvgpu_gsp_stress_test_sw_init(struct gk20a *g)
{
	int err = 0;
	struct nvgpu_gsp *gsp;

	nvgpu_log_fn(g, " ");

	if (g->gsp_stest != NULL) {
		/*
		 * Recovery/unrailgate case, we do not need to do gsp_stest init as
		 * gsp_stest is set during cold boot & doesn't execute gsp_stest clean
		 * up as part of power off sequence, so reuse to perform faster boot.
		 */
		nvgpu_gsp_stress_test_bootstrap(g, false);
		return err;
	}

	/* Init struct holding the gsp_stest software state */
	g->gsp_stest = (struct nvgpu_gsp_test *)
					nvgpu_kzalloc(g, sizeof(struct nvgpu_gsp_test));
	if (g->gsp_stest == NULL) {
		err = -ENOMEM;
		goto de_init;
	}

	/* Init struct holding the gsp software state */
	g->gsp_stest->gsp = (struct nvgpu_gsp *)
					nvgpu_kzalloc(g, sizeof(struct nvgpu_gsp));
	if (g->gsp_stest->gsp == NULL) {
		err = -ENOMEM;
		goto de_init;
	}

	gsp = g->gsp_stest->gsp;

	/* gsp falcon software state */
	gsp->gsp_flcn = &g->gsp_flcn;
	gsp->g = g;

	/* Init isr mutex */
	nvgpu_mutex_init(&gsp->isr_mutex);

	nvgpu_log_fn(g, " Done ");
	return err;
de_init:
	nvgpu_gsp_test_sw_deinit(g);
	return err;
}

void nvgpu_gsp_stest_isr(struct gk20a *g)
{
	struct nvgpu_gsp *gsp = g->gsp_stest->gsp;

	g->ops.gsp.gsp_isr(g, gsp);
}
