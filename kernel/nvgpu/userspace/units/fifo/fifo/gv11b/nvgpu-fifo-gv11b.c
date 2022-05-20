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

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/runlist.h>
#include <nvgpu/fuse.h>
#include <nvgpu/dma.h>
#include <nvgpu/io.h>
#include <nvgpu/soc.h>
#include <os/posix/os_posix.h>

#include "hal/fifo/fifo_gv11b.h"
#include <nvgpu/hw/gv11b/hw_fifo_gv11b.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-fifo-gv11b.h"
#include "nvgpu-fifo-intr-gv11b.h"

#ifdef FIFO_GV11B_UNIT_DEBUG
#undef unit_verbose
#define unit_verbose	unit_info
#else
#define unit_verbose(unit, msg, ...) \
	do { \
		if (0) { \
			unit_info(unit, msg, ##__VA_ARGS__); \
		} \
	} while (0)
#endif

int test_gv11b_fifo_init_hw(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	int i;

	for (i = 0; i < 2; i++) {
		p->is_silicon = (i > 0);

		if (!nvgpu_platform_is_silicon(g)) {
			nvgpu_writel(g, fifo_fb_timeout_r(), 0);
			g->ptimer_src_freq = 32500000;
		}

		gv11b_init_fifo_reset_enable_hw(g);

		if (!nvgpu_platform_is_silicon(g)) {
			unit_assert(nvgpu_readl(g, fifo_fb_timeout_r()) != 0,
				goto done);
		}

		nvgpu_writel(g, fifo_userd_writeback_r(), 0);
		gv11b_init_fifo_setup_hw(g);
		unit_assert(nvgpu_readl(g, fifo_userd_writeback_r()) != 0,
				goto done);
	}

	ret = UNIT_SUCCESS;
done:
	return ret;
}

#define INVALID_ID 0xffffffffU

int test_gv11b_fifo_mmu_fault_id_to_pbdma_id(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 reg_val;
	u32 num_pbdma = 3;
	u32 fault_id_pbdma0 = 15;
	u32 pbdma_id;
	u32 fault_id;
	u32 i;

	reg_val = (fault_id_pbdma0 << 16) | num_pbdma;
	nvgpu_writel(g, fifo_cfg0_r(), reg_val);

	pbdma_id = gv11b_fifo_mmu_fault_id_to_pbdma_id(g, 1);
	unit_assert(pbdma_id == INVALID_ID, goto done);

	pbdma_id = gv11b_fifo_mmu_fault_id_to_pbdma_id(g, fault_id_pbdma0 + num_pbdma);
	unit_assert(pbdma_id == INVALID_ID, goto done);

	for (i = 0; i < num_pbdma; i++) {
		fault_id = fault_id_pbdma0 + i;
		pbdma_id = gv11b_fifo_mmu_fault_id_to_pbdma_id(g, fault_id);
		unit_assert(pbdma_id == i, goto done);
	}

	ret = UNIT_SUCCESS;
done:
	return ret;
}

struct unit_module_test nvgpu_fifo_gv11b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),

	/* fifo gv11b */
	UNIT_TEST(init_hw, test_gv11b_fifo_init_hw, NULL, 0),
	UNIT_TEST(mmu_fault_id_to_pbdma_id, test_gv11b_fifo_mmu_fault_id_to_pbdma_id, NULL, 0),

	/* fifo intr gv11b */
	UNIT_TEST(intr_0_enable, test_gv11b_fifo_intr_0_enable, NULL, 0),
	UNIT_TEST(handle_sched_error, test_gv11b_fifo_handle_sched_error, NULL, 0),
	UNIT_TEST(intr_0_isr, test_gv11b_fifo_intr_0_isr, NULL, 0),
	UNIT_TEST(intr_recover_mask, test_gv11b_fifo_intr_recover_mask, NULL, 0),

	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_fifo_gv11b, nvgpu_fifo_gv11b_tests, UNIT_PRIO_NVGPU_TEST);
