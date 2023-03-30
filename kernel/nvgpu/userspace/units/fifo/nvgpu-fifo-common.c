/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/mm.h>
#include <nvgpu/fifo/userd.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/device.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/cic_rm.h>

#include <nvgpu/posix/io.h>

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

#include "hal/init/hal_gv11b.h"

#include "nvgpu-fifo-common.h"
#include "nvgpu-fifo-gv11b.h"

static struct unit_module *global_m;

/*
 * If taken, some branches are final, e.g. the function exits.
 * There is no need to test subsequent branches combinations,
 * if one final branch is taken.
 *
 * We want to skip the subtest if:
 * - it has at least one final branch
 * - it is supposed to test some branches after this final branch
 *
 * Parameters:
 * branches		bitmask of branches to be taken for one subtest
 * final_branches	bitmask of final branches
 *
 * Note: the assumption is that branches are numbered in their
 * order of appearance in the function to be tested.
 */
bool test_fifo_subtest_pruned(u32 branches, u32 final_branches)
{
	u32 match = branches & final_branches;
	int bit;

	if (match == 0U) {
		return false;
	}
	bit = nvgpu_ffs(match) - 1;

	return (branches > BIT(bit));
}

static int test_fifo_flags_strn(char *dst, size_t size,
		const char *labels[], u32 flags)
{
	int i;
	char *p = dst;
	int len;

	for (i = 0; i < 32; i++) {
		if (flags & BIT(i)) {
			len = snprintf(p, size, "%s ", labels[i]);
			size -= len;
			p += len;
		}
	}

	return (p - dst);
}

/* not MT-safe */
char *test_fifo_flags_str(u32 flags, const char *labels[])
{
	static char buf[256];

	memset(buf, 0, sizeof(buf));
	test_fifo_flags_strn(buf, sizeof(buf), labels, flags);
	return buf;
}

u32 test_fifo_get_log2(u32 num)
{
	u32 res = 0;

	if (num == 0) {
		return 0;
	}
	while (num > 0) {
		res++;
		num >>= 1;
	}
	return res - 1U;
}

static u32 stub_gv11b_gr_init_get_no_of_sm(struct gk20a *g)
{
	return 8;
}

static void stub_gr_falcon_dump_stats(struct gk20a *g)
{
}

#ifdef CONFIG_NVGPU_USERD
static int stub_userd_setup_sw(struct gk20a *g)
{
	int err = nvgpu_userd_init_slabs(g);

	if (err != 0) {
		unit_err(global_m, "failed to init userd support");
		return err;
	}

	return 0;
}
#endif

int test_fifo_init_support(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;

	err = test_fifo_setup_gv11b_reg_space(m, g);
	if (err != 0) {
		goto fail;
	}

	if (nvgpu_posix_io_add_reg_space(g,
			 gr_fecs_feature_override_ecc_r(), 0x4) != 0) {
		unit_err(m, "Add reg space failed!\n");
		return UNIT_FAIL;
	}

	if (nvgpu_posix_io_add_reg_space(g,
			gr_fecs_feature_override_ecc_1_r(), 0x4) != 0) {
		unit_err(m, "Add reg space failed!\n");
		return UNIT_FAIL;
	}

	gv11b_init_hal(g);
	g->ops.gr.init.get_no_of_sm = stub_gv11b_gr_init_get_no_of_sm;
	g->ops.gr.falcon.dump_stats = stub_gr_falcon_dump_stats;

	global_m = m;

#ifdef CONFIG_NVGPU_USERD
	/*
	 * Regular USERD init requires bar1.vm to be initialized
	 * Use a stub in unit tests, since it will be disabled in
	 * safety build anyway.
	 */
	g->ops.userd.setup_sw = stub_userd_setup_sw;
#endif
	g->ops.ecc.ecc_init_support(g);

	/* PD cache must be initialized prior to mm init */
	err = nvgpu_pd_cache_init(g);

	g->ops.mm.init_mm_support(g);

	nvgpu_device_init(g);

	err = nvgpu_fifo_init_support(g);

	/* Do not allocate from vidmem */
	nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, true);

	err = nvgpu_cic_mon_setup(g);
	if (err != 0) {
		unit_err(m, "CIC init failed!\n");
		return UNIT_FAIL;
	}

	err = nvgpu_cic_mon_init_lut(g);
	if (err != 0) {
		unit_return_fail(m, "CIC LUT init failed\n");
	}

	err = nvgpu_cic_rm_setup(g);
	if (err != 0) {
		unit_return_fail(m, "CIC-rm init failed\n");
	}

	err = nvgpu_cic_rm_init_vars(g);
	if (err != 0) {
		unit_return_fail(m, "CIC-rm vars init failed\n");
	}

	return UNIT_SUCCESS;

fail:
	return UNIT_FAIL;
}

int test_fifo_remove_support(struct unit_module *m,
		struct gk20a *g, void *args)
{
	if (g->fifo.remove_support) {
		g->fifo.remove_support(&g->fifo);
	}

	return UNIT_SUCCESS;
}
