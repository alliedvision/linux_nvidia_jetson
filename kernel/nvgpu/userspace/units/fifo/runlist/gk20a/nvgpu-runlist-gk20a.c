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

#include <nvgpu/posix/io.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/runlist.h>
#include <nvgpu/fuse.h>
#include <nvgpu/dma.h>
#include <nvgpu/io.h>

#include "hal/fifo/runlist_fifo_gk20a.h"

#include <nvgpu/hw/gk20a/hw_ram_gk20a.h>
#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>


#include "../../nvgpu-fifo-common.h"
#include "nvgpu-runlist-gk20a.h"

#ifdef RUNLIST_GK20A_UNIT_DEBUG
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

int test_gk20a_runlist_length_max(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;

	unit_assert(gk20a_runlist_length_max(g) ==
			fifo_eng_runlist_length_max_v(), goto done);
	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_gk20a_runlist_hw_submit(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_fifo *f = &g->fifo;
	int ret = UNIT_FAIL;
	u32 runlist_id = nvgpu_engine_get_gr_runlist_id(g);
	struct nvgpu_runlist *runlist = g->fifo.runlists[runlist_id];
	u32 count;

	nvgpu_rl_domain_alloc(g, "(default)");

	for (count = 0; count < 2; count++) {

		nvgpu_writel(g, fifo_runlist_r(), 0);
		nvgpu_writel(g, fifo_runlist_base_r(), 0);

		runlist->domain->mem_hw->count = count;

		gk20a_runlist_hw_submit(g, f->runlists[runlist_id]);
		if (count == 0) {
			unit_assert(nvgpu_readl(g, fifo_runlist_base_r()) == 0,
					goto done);
		} else {
			unit_assert(nvgpu_readl(g, fifo_runlist_base_r()) != 0,
					goto done);
		}
		unit_assert(nvgpu_readl(g, fifo_runlist_r()) ==
			(fifo_runlist_engine_f(runlist_id) |
				fifo_eng_runlist_length_f(count)), goto done);
	}

	ret = UNIT_SUCCESS;

done:
	return ret;
}

struct unit_ctx {
	struct unit_module *m;
	u32 addr;
	u32 count;
	u32 val_when_count_is_non_zero;
	u32 val_when_count_is_zero;
};

struct unit_ctx unit_ctx;
/*
 * Write callback. Forward the write access to the mock IO framework.
 */
static void writel_access_reg_fn(struct gk20a *g,
		struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

/*
 * Read callback. Get the register value from the mock IO framework.
 */
static void readl_access_reg_fn(struct gk20a *g,
		struct nvgpu_reg_access *access)
{
	if (access->addr == unit_ctx.addr) {
		if (unit_ctx.count > 0) {
			unit_ctx.count--;
			access->value = unit_ctx.val_when_count_is_non_zero;
		} else {
			access->value = unit_ctx.val_when_count_is_zero;
		}
		unit_verbose(unit_ctx.m, "count=%u val=%x\n",
			unit_ctx.count, access->value);
	} else {
		access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
	}
}

static struct nvgpu_posix_io_callbacks test_reg_callbacks = {
	/* Write APIs all can use the same accessor. */
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,

	/* Likewise for the read APIs. */
	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
};


int test_gk20a_runlist_wait_pending(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct unit_ctx *ctx = &unit_ctx;
	u32 runlist_id = nvgpu_engine_get_gr_runlist_id(g);
	struct nvgpu_runlist *runlist = g->fifo.runlists[runlist_id];
	u32 timeout = g->poll_timeout_default;
	int err;

	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

	g->poll_timeout_default = 10; /* ms */

	ctx->m = m;
	ctx->addr = fifo_eng_runlist_r(runlist->id);
	ctx->val_when_count_is_non_zero = fifo_eng_runlist_pending_true_f();
	ctx->val_when_count_is_zero = 0;

	/* no wait */
	ctx->count = 0;
	err = gk20a_runlist_wait_pending(g, runlist);
	unit_assert(err == 0, goto done);

	/* 1 loop */
	ctx->count = 1;
	err = gk20a_runlist_wait_pending(g, runlist);
	unit_assert(err == 0, goto done);

	/* 2 loops */
	ctx->count = 2;
	err = gk20a_runlist_wait_pending(g, runlist);
	unit_assert(err == 0, goto done);

	/* timeout  */
	ctx->count = U32_MAX;
	err = gk20a_runlist_wait_pending(g, runlist);
	unit_assert(err == -ETIMEDOUT, goto done);

	ret = UNIT_SUCCESS;

done:
	g->poll_timeout_default = timeout;
	return ret;
}

int test_gk20a_runlist_write_state(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 i;
	u32 v;
	u32 mask;

	for (i = 0; i < 2; i++) {
		v = i * U32_MAX;
		for (mask = 0; mask < 4; mask++) {
			nvgpu_writel(g, fifo_sched_disable_r(), v);
			gk20a_runlist_write_state(g, mask, RUNLIST_DISABLED);
			unit_assert(nvgpu_readl(g, fifo_sched_disable_r()) == (v | mask),
					goto done);

			nvgpu_writel(g, fifo_sched_disable_r(), v);
			gk20a_runlist_write_state(g, mask, RUNLIST_ENABLED);
			unit_assert(nvgpu_readl(g, fifo_sched_disable_r()) == (v & ~mask),
					goto done);
		}
	}

	ret = UNIT_SUCCESS;

done:
	return ret;
}

struct unit_module_test nvgpu_runlist_gk20a_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(length_max, test_gk20a_runlist_length_max, NULL, 0),
	UNIT_TEST(hw_submit, test_gk20a_runlist_hw_submit, NULL, 0),
	UNIT_TEST(wait_pending, test_gk20a_runlist_wait_pending, NULL, 0),
	UNIT_TEST(write_state, test_gk20a_runlist_write_state, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_runlist_gk20a, nvgpu_runlist_gk20a_tests, UNIT_PRIO_NVGPU_TEST);
