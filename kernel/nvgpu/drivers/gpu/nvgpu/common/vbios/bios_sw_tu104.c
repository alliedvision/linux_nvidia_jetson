/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/timers.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>

#include "bios_sw_gv100.h"
#include "bios_sw_tu104.h"

#define NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_MASK \
		0xFFU
#define NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_COMPLETED \
		0xFFU

#define NVGPU_PG189_MIN_VBIOS		0x90041800U

#define NVGPU_PG189_0600_VBIOS		0x90049500U
#define NVGPU_PG189_0600_QS_VBIOS	0x9004A200U
#define NVGPU_PG189_0601_VBIOS		0x90045a00U
#define NVGPU_PG189_0610_QS_VBIOS	0x90049100U
#define NVGPU_PG189_0601_QS_VBIOS	0x90049600U

struct nvgpu_vbios_board {
	u16 board_id;
	u32 vbios_version;
};

#define NVGPU_PG189_NUM_VBIOS_BOARDS	5U

static struct nvgpu_vbios_board vbios_boards[NVGPU_PG189_NUM_VBIOS_BOARDS] = {
	/* SKU 600 ES/CS, SKU 606*/
	[0] = {
		.board_id = 0x0068,
		.vbios_version = NVGPU_PG189_0600_VBIOS,
	},
	/* SKU 600 QS */
	[1] = {
		.board_id = 0x0183,
		.vbios_version = NVGPU_PG189_0600_QS_VBIOS,
	},
	/* SKU 601 CS */
	[2] = {
		.board_id = 0x00E8,
		.vbios_version = NVGPU_PG189_0601_VBIOS,
	},
	/* SKU 610 QS */
	[3] = {
		.board_id = 0x01a3,
		.vbios_version = NVGPU_PG189_0610_QS_VBIOS,
	},
	/* SKU 601 QS */
	[4] = {
		.board_id = 0x01cc,
		.vbios_version = NVGPU_PG189_0601_QS_VBIOS,
	},
};

static int tu104_bios_verify_version(struct gk20a *g)
{
	struct nvgpu_vbios_board *board = NULL;
	u32 i;

	nvgpu_info(g, "VBIOS board id %04x", g->bios->vbios_board_id);

	nvgpu_info(g, "VBIOS version %08x:%02x\n",
		g->bios->vbios_version,
		g->bios->vbios_oem_version);

	if (g->bios->vbios_version < NVGPU_PG189_MIN_VBIOS) {
		nvgpu_err(g, "unsupported VBIOS version %08x",
			g->bios->vbios_version);
		return -EINVAL;
	}

	for (i = 0; i < NVGPU_PG189_NUM_VBIOS_BOARDS; i++) {
		if (g->bios->vbios_board_id == vbios_boards[i].board_id) {
			board = &vbios_boards[i];
		}
	}

	if (board == NULL) {
		nvgpu_warn(g, "unknown board id %04x",
			g->bios->vbios_board_id);
		return 0;
	}

	if ((board->vbios_version != 0U) &&
		(g->bios->vbios_version < board->vbios_version)) {
		nvgpu_warn(g, "VBIOS version should be at least %08x",
			board->vbios_version);
	}

	return 0;
}

int tu104_bios_verify_devinit(struct gk20a *g)
{
	struct nvgpu_timeout timeout;
	u32 val;
	u32 aon_secure_scratch_reg;

	nvgpu_timeout_init_cpu_timer(g, &timeout, NVGPU_BIOS_DEVINIT_VERIFY_TIMEOUT_MS);

	do {
		aon_secure_scratch_reg = g->ops.bios.get_aon_secure_scratch_reg(g, 0);
		val = nvgpu_readl(g, aon_secure_scratch_reg);
		val &= NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_MASK;

		if (val == NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_COMPLETED) {
			nvgpu_log_info(g, "devinit complete");
			return 0;
		}

		nvgpu_udelay(NVGPU_BIOS_DEVINIT_VERIFY_DELAY_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	return -ETIMEDOUT;
}

int tu104_bios_init(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		return 0;
	}
#endif

	return gv100_bios_init(g);
}

void nvgpu_tu104_bios_sw_init(struct gk20a *g,
		struct nvgpu_bios *bios)
{
	bios->init = tu104_bios_init;
	bios->verify_version = tu104_bios_verify_version;
	bios->preos_wait_for_halt = NULL;
	bios->preos_reload_check = NULL;
	bios->preos_bios = NULL;
	bios->devinit_bios = NULL;
	bios->verify_devinit = tu104_bios_verify_devinit;
}

