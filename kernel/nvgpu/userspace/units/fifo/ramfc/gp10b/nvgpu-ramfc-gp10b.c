/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/mm.h>
#include <nvgpu/channel.h>
#include <nvgpu/dma.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/hw/gp10b/hw_ram_gp10b.h>
#include <nvgpu/hw/gp10b/hw_pbdma_gp10b.h>

#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramfc_gk20a.h"
#include "hal/fifo/ramfc_gp10b.h"

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-ramfc-gp10b.h"

struct stub_ctx {
	u32 addr_lo;
	u32 addr_hi;
};

struct stub_ctx stub[1];

#define USERD_IOVA_ADDR_LO	1U
#define USERD_IOVA_ADDR_HI	2U

static u32 stub_pbdma_get_userd_aperture_mask(struct gk20a *g,
							struct nvgpu_mem *mem)
{
	/* Assuming mem is SYSMEM */
	return pbdma_userd_target_sys_mem_ncoh_f();
}

static u32 stub_pbdma_get_userd_addr(u32 addr_lo)
{
	stub[0].addr_lo = addr_lo;
	return 0U;
}

static u32 stub_pbdma_get_userd_hi_addr(u32 addr_hi)
{
	stub[0].addr_hi = addr_hi;
	return 1U;
}

int test_gp10b_ramfc_commit_userd(struct unit_module *m, struct gk20a *g,
								void *args)
{
	struct nvgpu_channel ch;
	int ret = UNIT_FAIL;
	int err;

	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;
	g->ops.pbdma.get_userd_aperture_mask =
					stub_pbdma_get_userd_aperture_mask;
	g->ops.pbdma.get_userd_addr = stub_pbdma_get_userd_addr;
	g->ops.pbdma.get_userd_hi_addr = stub_pbdma_get_userd_hi_addr;

	/* Aperture should be fixed = SYSMEM */
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, true);
	err = nvgpu_alloc_inst_block(g, &ch.inst_block);
	unit_assert(err == 0, goto done);

	ch.g = g;
	ch.chid = 0;
	ch.userd_iova = (((u64)USERD_IOVA_ADDR_HI << 32U) |
			USERD_IOVA_ADDR_LO) << ram_userd_base_shift_v();

	gp10b_ramfc_commit_userd(&ch);
	unit_assert(stub[0].addr_lo == USERD_IOVA_ADDR_LO, goto done);
	unit_assert(stub[0].addr_hi == (USERD_IOVA_ADDR_HI) <<
			ram_userd_base_shift_v(), goto done);
	unit_assert(nvgpu_mem_rd32(g, &ch.inst_block,
			ram_in_ramfc_w() + ram_fc_userd_w()) ==
			pbdma_userd_target_sys_mem_ncoh_f(), goto done);
	unit_assert(nvgpu_mem_rd32(g, &ch.inst_block, ram_in_ramfc_w() +
			ram_fc_userd_hi_w()) == 1U, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	nvgpu_free_inst_block(g, &ch.inst_block);
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, false);
	return ret;
}

struct unit_module_test nvgpu_ramfc_gp10b_tests[] = {
	UNIT_TEST(commit_userd, test_gp10b_ramfc_commit_userd, NULL, 0),
};

UNIT_MODULE(nvgpu_ramfc_gp10b, nvgpu_ramfc_gp10b_tests, UNIT_PRIO_NVGPU_TEST);
