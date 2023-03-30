/*
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

#include <nvgpu/bios.h>
#include <nvgpu/nvgpu_common.h>
#include <nvgpu/timers.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>

#include "bios_sw_gv100.h"

#define BIOS_SIZE 0x90000

#define PMU_BOOT_TIMEOUT_DEFAULT	100U /* usec */
#define PMU_BOOT_TIMEOUT_MAX		2000000U /* usec */

#define SCRATCH_PREOS_PROGRESS	6U
#define PREOS_PROGRESS_MASK(r)		(((r) >> 12U) & 0xfU)
#define PREOS_PROGRESS_NOT_STARTED	0U
#define PREOS_PROGRESS_STARTED		1U
#define PREOS_PROGRESS_EXIT		2U
#define PREOS_PROGRESS_EXIT_SECUREMODE	3U
#define PREOS_PROGRESS_ABORTED		6U

#define SCRATCH_PMU_EXIT_AND_HALT	1U
#define PMU_EXIT_AND_HALT_SET(r, v)	(((r) & ~0x200U) | (v))
#define PMU_EXIT_AND_HALT_YES		BIT32(9)

#define SCRATCH_PRE_OS_RELOAD		1U
#define PRE_OS_RELOAD_SET(r, v)		(((r) & ~0x100U) | (v))
#define PRE_OS_RELOAD_YES		BIT32(8)


void gv100_bios_preos_reload_check(struct gk20a *g)
{
	u32 progress = g->ops.bus.read_sw_scratch(g, SCRATCH_PREOS_PROGRESS);

	if (PREOS_PROGRESS_MASK(progress) != PREOS_PROGRESS_NOT_STARTED) {
		u32 reload = g->ops.bus.read_sw_scratch(g,
				SCRATCH_PRE_OS_RELOAD);

		g->ops.bus.write_sw_scratch(g, SCRATCH_PRE_OS_RELOAD,
			PRE_OS_RELOAD_SET(reload, PRE_OS_RELOAD_YES));
	}
}

int gv100_bios_preos_wait_for_halt(struct gk20a *g)
{
	int err = -EINVAL;
	u32 progress;
	u32 tmp;
	bool preos_completed;
	struct nvgpu_timeout timeout;

	nvgpu_udelay(PMU_BOOT_TIMEOUT_DEFAULT);

	/* Check the progress */
	progress = g->ops.bus.read_sw_scratch(g, SCRATCH_PREOS_PROGRESS);

	if (PREOS_PROGRESS_MASK(progress) == PREOS_PROGRESS_STARTED) {
		err = 0;

		/* Complete the handshake */
		tmp = g->ops.bus.read_sw_scratch(g, SCRATCH_PMU_EXIT_AND_HALT);

		g->ops.bus.write_sw_scratch(g, SCRATCH_PMU_EXIT_AND_HALT,
			PMU_EXIT_AND_HALT_SET(tmp, PMU_EXIT_AND_HALT_YES));

		nvgpu_timeout_init_retry(g, &timeout,
			   PMU_BOOT_TIMEOUT_MAX /
				PMU_BOOT_TIMEOUT_DEFAULT);

		do {
			progress = g->ops.bus.read_sw_scratch(g,
							SCRATCH_PREOS_PROGRESS);
			preos_completed = g->ops.falcon.is_falcon_cpu_halted(
					g->pmu->flcn) &&
					(PREOS_PROGRESS_MASK(progress) ==
					PREOS_PROGRESS_EXIT);

			nvgpu_udelay(PMU_BOOT_TIMEOUT_DEFAULT);
		} while (!preos_completed &&
			(nvgpu_timeout_expired(&timeout) == 0));
	}

	return err;
}

int gv100_bios_devinit(struct gk20a *g)
{
	int err = 0;
	bool devinit_completed;
	struct nvgpu_timeout timeout;
	u32 top_scratch1_reg;

	nvgpu_log_fn(g, " ");

	if (nvgpu_falcon_reset(g->pmu->flcn) != 0) {
		err = -ETIMEDOUT;
		goto out;
	}

	err = nvgpu_falcon_copy_to_imem(g->pmu->flcn,
			g->bios->devinit.bootloader_phys_base,
			g->bios->devinit.bootloader,
			g->bios->devinit.bootloader_size,
			0, 0, g->bios->devinit.bootloader_phys_base >> 8);
	if (err != 0) {
		nvgpu_err(g, "bios devinit bootloader copy failed %d", err);
		goto out;
	}

	err = nvgpu_falcon_copy_to_imem(g->pmu->flcn, g->bios->devinit.phys_base,
					g->bios->devinit.ucode,
					g->bios->devinit.size,
					0, 1, g->bios->devinit.phys_base >> 8);
	if (err != 0) {
		nvgpu_err(g, "bios devinit ucode copy failed %d", err);
		goto out;
	}

	err = nvgpu_falcon_copy_to_dmem(g->pmu->flcn,
				g->bios->devinit.dmem_phys_base,
				g->bios->devinit.dmem,
				g->bios->devinit.dmem_size,
				0);
	if (err != 0) {
		nvgpu_err(g, "bios devinit dmem copy failed %d", err);
		goto out;
	}

	err = nvgpu_falcon_copy_to_dmem(g->pmu->flcn,
				g->bios->devinit_tables_phys_base,
				g->bios->devinit_tables,
				g->bios->devinit_tables_size,
				0);
	if (err != 0) {
		nvgpu_err(g, "fbios devinit tables copy failed %d", err);
		goto out;
	}

	err = nvgpu_falcon_copy_to_dmem(g->pmu->flcn,
		g->bios->devinit_script_phys_base,
		g->bios->bootscripts,
		g->bios->bootscripts_size,
		0);
	if (err != 0) {
		nvgpu_err(g, "bios devinit bootscripts copy failed %d", err);
		goto out;
	}

	err = nvgpu_falcon_bootstrap(g->pmu->flcn,
				g->bios->devinit.code_entry_point);
	if (err != 0) {
		nvgpu_err(g, "falcon bootstrap failed %d", err);
		goto out;
	}

	nvgpu_timeout_init_retry(g, &timeout,
				 PMU_BOOT_TIMEOUT_MAX /
				 PMU_BOOT_TIMEOUT_DEFAULT);

	do {
		top_scratch1_reg = g->ops.top.read_top_scratch1_reg(g);
		devinit_completed = ((g->ops.falcon.is_falcon_cpu_halted(
				g->pmu->flcn) != 0U) &&
				(g->ops.top.top_scratch1_devinit_completed(g,
				top_scratch1_reg)) != 0U);

		nvgpu_udelay(PMU_BOOT_TIMEOUT_DEFAULT);
	} while (!devinit_completed && (nvgpu_timeout_expired(&timeout) == 0));

	if (nvgpu_timeout_peek_expired(&timeout)) {
		err = -ETIMEDOUT;
		goto out;
	}

	err = nvgpu_falcon_clear_halt_intr_status(g->pmu->flcn,
		nvgpu_get_poll_timeout(g));
	if (err != 0) {
		nvgpu_err(g, "falcon_clear_halt_intr_status failed %d", err);
		goto out;
	}

out:
	nvgpu_log_fn(g, "done");
	return err;
}

int gv100_bios_init(struct gk20a *g)
{
	unsigned int i;
	int err;

	nvgpu_log_fn(g, " ");

	if (g->bios_is_init) {
		return 0;
	}

	nvgpu_log_info(g, "reading bios from EEPROM");
	g->bios->size = BIOS_SIZE;
	g->bios->data = nvgpu_vmalloc(g, BIOS_SIZE);
	if (g->bios->data == NULL) {
		return -ENOMEM;
	}

	if (g->ops.xve.disable_shadow_rom != NULL) {
		g->ops.xve.disable_shadow_rom(g);
	}

	for (i = 0U; i < g->bios->size/4U; i++) {
		u32 val = be32_to_cpu(gk20a_readl(g, 0x300000U + i*4U));

		g->bios->data[(i*4U)] = (val >> 24U) & 0xffU;
		g->bios->data[(i*4U)+1U] = (val >> 16U) & 0xffU;
		g->bios->data[(i*4U)+2U] = (val >> 8U) & 0xffU;
		g->bios->data[(i*4U)+3U] = val & 0xffU;
	}

	if (g->ops.xve.enable_shadow_rom != NULL) {
		g->ops.xve.enable_shadow_rom(g);
	}

	err = nvgpu_bios_parse_rom(g);
	if (err != 0) {
		goto free_firmware;
	}

	if (g->bios->verify_version != NULL) {
		if (g->bios->verify_version(g) < 0) {
			err = -EINVAL;
			goto free_firmware;
		}
	}

	nvgpu_log_fn(g, "done");

	err = nvgpu_bios_devinit(g, g->bios);
	if (err != 0) {
		nvgpu_err(g, "devinit failed");
		goto free_firmware;
	}

	if (nvgpu_is_enabled(g, NVGPU_PMU_RUN_PREOS) &&
		(g->bios->preos_bios != NULL)) {
			err = g->bios->preos_bios(g);
			if (err != 0) {
				nvgpu_err(g, "pre-os failed");
				goto free_firmware;
			}
	}

	if (g->bios->verify_devinit != NULL) {
		err = g->bios->verify_devinit(g);
		if (err != 0) {
				nvgpu_err(g, "devinit status verification failed");
				goto free_firmware;
		}
	}

	g->bios_is_init = true;

	return 0;

free_firmware:
	if (g->bios->data != NULL) {
			nvgpu_vfree(g, g->bios->data);
	}
	return err;
}

int gv100_bios_preos(struct gk20a *g)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (nvgpu_falcon_reset(g->pmu->flcn) != 0) {
		err = -ETIMEDOUT;
		goto out;
	}

	if (g->bios->preos_reload_check != NULL) {
		g->bios->preos_reload_check(g);
	}

	err = nvgpu_falcon_copy_to_imem(g->pmu->flcn,
		g->bios->preos.bootloader_phys_base,
		g->bios->preos.bootloader,
		g->bios->preos.bootloader_size,
		0, 0, g->bios->preos.bootloader_phys_base >> 8);

	if (err != 0) {
		nvgpu_err(g, "bios preos bootloader copy failed %d", err);
		goto out;
	}

	err = nvgpu_falcon_copy_to_imem(g->pmu->flcn, g->bios->preos.phys_base,
		g->bios->preos.ucode,
		g->bios->preos.size,
		0, 1, g->bios->preos.phys_base >> 8);

	if (err != 0) {
		nvgpu_err(g, "bios preos ucode copy failed %d", err);
		goto out;
	}

	err = nvgpu_falcon_copy_to_dmem(g->pmu->flcn, g->bios->preos.dmem_phys_base,
		g->bios->preos.dmem,
		g->bios->preos.dmem_size,
		0);

	if (err != 0) {
		nvgpu_err(g, "bios preos dmem copy failed %d", err);
		goto out;
	}

	err = nvgpu_falcon_bootstrap(g->pmu->flcn,
				g->bios->preos.code_entry_point);

	if (err != 0) {
		nvgpu_err(g, "falcon bootstrap failed %d", err);
		goto out;
	}

	err = nvgpu_bios_preos_wait_for_halt(g, g->bios);
	if (err != 0) {
		nvgpu_err(g, "preos_wait_for_halt failed %d", err);
		goto out;
	}

	err = nvgpu_falcon_clear_halt_intr_status(g->pmu->flcn,
			nvgpu_get_poll_timeout(g));
	if (err != 0) {
		nvgpu_err(g, "falcon_clear_halt_intr_status failed %d", err);
		goto out;
	}

out:
	nvgpu_log_fn(g, "done");
	return err;
}

void nvgpu_gv100_bios_sw_init(struct gk20a *g,
		struct nvgpu_bios *bios)
{
	bios->init = gv100_bios_init;
	bios->verify_version = NULL;
	bios->preos_wait_for_halt = gv100_bios_preos_wait_for_halt;
	bios->preos_reload_check = gv100_bios_preos_reload_check;
	bios->preos_bios = gv100_bios_preos;
	bios->devinit_bios = gv100_bios_devinit;
	bios->verify_devinit = NULL;
}
