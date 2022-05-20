/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include "common/vbios/bios_sw_tu104.h"
#include "common/vbios/bios_sw_ga100.h"

#define NVGPU_PG209_MIN_VBIOS		0

struct nvgpu_vbios_board {
	u16 board_id;
	u32 vbios_version;
};

#define NVGPU_GA100_NUM_BOARDS		1

static struct nvgpu_vbios_board vbios_boards[NVGPU_GA100_NUM_BOARDS] = {
	/* PG209 SKU 200 */
	[0] = {
		.board_id = 0x0209,
		.vbios_version = 0,	/* any VBIOS for now */
	},
};

static int ga100_bios_verify_version(struct gk20a *g)
{
	struct nvgpu_vbios_board *board = NULL;
	u32 i;

	nvgpu_info(g, "VBIOS board id %04x", g->bios->vbios_board_id);

	nvgpu_info(g, "VBIOS version %08x:%02x\n",
		g->bios->vbios_version,
		g->bios->vbios_oem_version);

#if NVGPU_PG209_MIN_VBIOS
	if (g->bios->vbios_version < NVGPU_PG209_MIN_VBIOS) {
		nvgpu_err(g, "unsupported VBIOS version %08x",
			g->bios->vbios_version);
		return -EINVAL;
	}
#endif

	for (i = 0; i < NVGPU_GA100_NUM_BOARDS; i++) {
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

void nvgpu_ga100_bios_sw_init(struct gk20a *g, struct nvgpu_bios *bios)
{
	bios->init = tu104_bios_init;
	bios->verify_version = ga100_bios_verify_version;
	bios->preos_wait_for_halt = NULL;
	bios->preos_reload_check = NULL;
	bios->preos_bios = NULL;
	bios->devinit_bios = NULL;
	bios->verify_devinit = tu104_bios_verify_devinit;
}

