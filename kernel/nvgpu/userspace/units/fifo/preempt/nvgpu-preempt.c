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

#include <stdlib.h>
#include <unistd.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/channel.h>
#include <nvgpu/runlist.h>
#include <nvgpu/preempt.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/dma.h>
#include <nvgpu/io.h>

#include "hal/fifo/runlist_ram_gk20a.h"
#include "hal/fifo/tsg_gk20a.h"
#include "nvgpu/hw/gk20a/hw_ram_gk20a.h"
#include "nvgpu-preempt.h"
#include "nvgpu/hw/gk20a/hw_fifo_gk20a.h"

#include "../nvgpu-fifo-common.h"

#define RL_MAX_TIMESLICE_TIMEOUT ram_rl_entry_timeslice_timeout_v(U32_MAX)
#define RL_MAX_TIMESLICE_SCALE ram_rl_entry_timeslice_scale_v(U32_MAX)

#ifdef PREEMPT_UNIT_DEBUG
#define unit_verbose	unit_info
#else
#define unit_verbose(unit, msg, ...) \
	do { \
		if (0) { \
			unit_info(unit, msg, ##__VA_ARGS__); \
		} \
	} while (0)
#endif

#define MAX_STUB	2

struct stub_ctx {
	const char *name;
	u32 chid;
	u32 count;
	u32 tsgid;
	u32 pbdma_id;
};

struct stub_ctx stub[MAX_STUB];

struct preempt_unit_ctx {
	u32 branches;
};

static struct preempt_unit_ctx unit_ctx;

static void subtest_setup(u32 branches)
{
	u32 i;

	unit_ctx.branches = branches;

	memset(stub, 0, sizeof(stub));
	for (i = 0; i < MAX_STUB; i++) {
		stub[i].name = "";
		stub[i].count = 0;
		stub[i].chid = NVGPU_INVALID_CHANNEL_ID;
		stub[i].tsgid = NVGPU_INVALID_TSG_ID;
	}
}

#define branches_str test_fifo_flags_str
#define pruned test_fifo_subtest_pruned

#define F_PREEMPT_CHANNEL	BIT(0)
#define F_PREEMPT_LAST		BIT(1)

static const char *f_preempt[] = {
	"preempt_tsg",
	"preempt_channel",
};

static int stub_fifo_preempt_channel(struct gk20a *g, struct nvgpu_channel *ch)
{
	stub[0].chid = ch->chid;
	return 0;
}

static int stub_fifo_preempt_tsg(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	stub[0].tsgid = tsg->tsgid;
	return 0;
}

int test_preempt(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel *ch = NULL;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;

	u32 branches = 0U;
	int ret = UNIT_FAIL;
	int err = 0U;

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	g->ops.fifo.preempt_tsg = stub_fifo_preempt_tsg;
	g->ops.fifo.preempt_channel = stub_fifo_preempt_channel;

	for (branches = 0U; branches < F_PREEMPT_LAST;
		branches++) {

		unit_verbose(m, "%s branches=%u\n", __func__, branches);
		subtest_setup(branches);

		ch->tsgid = (branches & F_PREEMPT_CHANNEL) ?
						NVGPU_INVALID_TSG_ID : 0;

		err = nvgpu_preempt_channel(g, ch);
		unit_assert(err == 0, goto done);

		if (branches & F_PREEMPT_CHANNEL) {
			unit_assert(stub[0].chid == ch->chid, goto done);
		} else {
			unit_assert(stub[0].tsgid == ch->tsgid, goto done);
		}
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
		branches_str(branches, f_preempt));
	}

	g->ops = gops;
	return ret;
}

#define F_PREEMPT_POLL_PBDMA_NULL	BIT(0)
#define F_PREEMPT_POLL_PBDMA_BUSY	BIT(1)
#define F_PREEMPT_POLL_LAST		BIT(2)

static const char *f_preempt_poll[] = {
	"preempt_poll_pbdma_null",
	"preempt_poll_pbdma_busy",
};

static int stub_fifo_preempt_poll_pbdma_busy(struct gk20a *g, u32 tsgid,
								u32 pbdma_id)
{
	stub[0].tsgid = tsgid;
	stub[0].pbdma_id = pbdma_id;
	return -EBUSY;
}

static int stub_fifo_preempt_poll_pbdma(struct gk20a *g, u32 tsgid,
								u32 pbdma_id)
{
	stub[0].tsgid = tsgid;
	stub[0].pbdma_id = pbdma_id;
	return 0;
}

int test_preempt_poll_tsg_on_pbdma(struct unit_module *m, struct gk20a *g,
								void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_tsg *tsg;
	struct nvgpu_fifo *f = &g->fifo;

	u32 branches = 0U;
	int ret = UNIT_FAIL;
	u32 prune = F_PREEMPT_POLL_PBDMA_NULL;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);
	tsg->runlist = &f->active_runlists[0];

	for (branches = 0U; branches < F_PREEMPT_POLL_LAST;
		branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, f_preempt_poll));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
					branches_str(branches, f_preempt_poll));

		g->ops.fifo.preempt_poll_pbdma =
				(branches & F_PREEMPT_POLL_PBDMA_NULL) ?
				NULL : ((branches & F_PREEMPT_POLL_PBDMA_BUSY) ?
					stub_fifo_preempt_poll_pbdma_busy :
					stub_fifo_preempt_poll_pbdma);

		nvgpu_preempt_poll_tsg_on_pbdma(g, tsg);

		if (branches & F_PREEMPT_POLL_PBDMA_BUSY) {
			unit_assert(stub[0].pbdma_id !=
				nvgpu_ffs(f->runlists[0]->pbdma_bitmask),
				goto done);
		} else if (!(branches & F_PREEMPT_POLL_PBDMA_NULL)) {
			unit_assert(stub[0].tsgid == 0, goto done);
			unit_assert(stub[0].pbdma_id ==
				nvgpu_ffs(f->runlists[0]->pbdma_bitmask),
				goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_preempt_poll));
	}
	g->ops = gops;
	return ret;
}

int test_preempt_get_timeout(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 timeout;
	int ret = UNIT_FAIL;

	timeout = nvgpu_preempt_get_timeout(g);
	unit_assert(timeout == 0U, goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

struct unit_module_test nvgpu_preempt_tests[] = {

	UNIT_TEST(init_support, test_fifo_init_support, &unit_ctx, 0),
	UNIT_TEST(preempt, test_preempt, NULL, 0),
	UNIT_TEST(preempt_poll, test_preempt_poll_tsg_on_pbdma, NULL, 0),
	UNIT_TEST(get_timeout, test_preempt_get_timeout, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, &unit_ctx, 0),
};

UNIT_MODULE(nvgpu_preempt, nvgpu_preempt_tests, UNIT_PRIO_NVGPU_TEST);
