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

#include <nvgpu/posix/posix-fault-injection.h>

#include "hal/fifo/channel_gk20a.h"
#include "hal/fifo/channel_gm20b.h"
#include "hal/fifo/channel_gv11b.h"

#include <nvgpu/hw/gv11b/hw_ccsr_gv11b.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-channel-gv11b.h"

#ifdef CHANNEL_GV11B_UNIT_DEBUG
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

#define branches_str test_fifo_flags_str
#define pruned test_fifo_subtest_pruned

struct unit_ctx {
	struct unit_module *m;
	int count;
	int err;
	size_t size;
};

int test_gv11b_channel_unbind(struct unit_module *m,
		struct gk20a *g, void *args)
{
	bool privileged = false;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	struct nvgpu_channel *ch;
	int ret = UNIT_FAIL;

	ch = nvgpu_channel_open_new(g, runlist_id,
		privileged, getpid(), getpid());
	unit_assert(ch, goto done);
	unit_assert(nvgpu_atomic_read(&ch->bound) == 0, goto done);

	nvgpu_writel(g, ccsr_channel_inst_r(ch->chid), 0);
	nvgpu_writel(g, ccsr_channel_r(ch->chid), 0);

	g->ops.channel.bind(ch);
	unit_assert(nvgpu_atomic_read(&ch->bound) == 1, goto done);

	gv11b_channel_unbind(ch);

	unit_assert(nvgpu_readl(g, (ccsr_channel_inst_r(ch->chid)) &
		ccsr_channel_inst_bind_false_f()) != 0, goto done);
	unit_assert(nvgpu_readl(g, (ccsr_channel_r(ch->chid)) &
		ccsr_channel_enable_clr_true_f()) != 0, goto done);
	unit_assert(nvgpu_atomic_read(&ch->bound) == 0, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ch) {
		nvgpu_channel_close(ch);
	}

	return ret;
}

int test_gv11b_channel_count(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;

	unit_assert(gv11b_channel_count(g) == ccsr_channel__size_1_v(),
		goto done);
	ret = UNIT_SUCCESS;
done:
	return ret;
}


/* note: other branches covered in gk20a_channel_read_state */
#define F_CHANNEL_READ_ENG_FAULTED				BIT(0)
#define F_CHANNEL_READ_STATE_LAST				BIT(1)

static const char *f_channel_read_state[] = {
	"eng_faulted"
};

int test_gv11b_channel_read_state(struct unit_module *m,
		struct gk20a *g, void *args)
{
	bool privileged = false;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	struct nvgpu_channel *ch;
	int ret = UNIT_FAIL;
	struct nvgpu_channel_hw_state state;
	u32 branches;

	ch = nvgpu_channel_open_new(g, runlist_id,
		privileged, getpid(), getpid());
	unit_assert(ch, goto done);

	for (branches = 0U; branches < F_CHANNEL_READ_STATE_LAST; branches++) {

		bool eng_faulted = false;
		u32 v = 0;

		unit_verbose(m, "%s branches=%s\n",
			__func__, branches_str(branches, f_channel_read_state));

		if (branches & F_CHANNEL_READ_ENG_FAULTED) {
			eng_faulted = true;
			v = ccsr_channel_eng_faulted_true_v() << 23U;
		}

		nvgpu_writel(g, ccsr_channel_r(ch->chid), v);

		gv11b_channel_read_state(g, ch, &state);
		unit_assert(state.eng_faulted == eng_faulted, goto done);
	}

	ret = UNIT_SUCCESS;
done:
	if (ch) {
		nvgpu_channel_close(ch);
	}

	return ret;
}

#define F_CHANNEL_RESET_FAULTED_PBDMA				BIT(0)
#define F_CHANNEL_RESET_FAULTED_ENG				BIT(1)
#define F_CHANNEL_RESET_FAULTED_LAST				BIT(2)

static const char *f_channel_reset_faulted[] = {
	"eng",
	"pbdma",
};

int test_gv11b_channel_reset_faulted(struct unit_module *m,
		struct gk20a *g, void *args)
{
	bool privileged = false;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	struct nvgpu_channel *ch;
	int ret = UNIT_FAIL;
	u32 branches;

	ch = nvgpu_channel_open_new(g, runlist_id,
		privileged, getpid(), getpid());
	unit_assert(ch, goto done);

	for (branches = 0U; branches < F_CHANNEL_RESET_FAULTED_LAST; branches++) {

		bool eng;
		bool pbdma;
		u32 v;

		unit_verbose(m, "%s branches=%s\n",
			__func__, branches_str(branches, f_channel_reset_faulted));

		eng = (branches & F_CHANNEL_RESET_FAULTED_ENG) != 0;
		pbdma = (branches & F_CHANNEL_RESET_FAULTED_PBDMA) != 0;

		nvgpu_writel(g, ccsr_channel_r(ch->chid), 0);

		gv11b_channel_reset_faulted(g, ch, eng, pbdma);

		v = nvgpu_readl(g, ccsr_channel_r(ch->chid));
		unit_assert(!eng ||
			((v & ccsr_channel_eng_faulted_reset_f()) != 0),
			goto done);
		unit_assert(!pbdma ||
			((v & ccsr_channel_pbdma_faulted_reset_f()) != 0),
			goto done);
	}

	ret = UNIT_SUCCESS;
done:
	if (ch) {
		nvgpu_channel_close(ch);
	}

	return ret;
}

struct unit_module_test nvgpu_channel_gv11b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(unbind, test_gv11b_channel_unbind, NULL, 0),
	UNIT_TEST(count, test_gv11b_channel_count, NULL, 0),
	UNIT_TEST(read_state, test_gv11b_channel_read_state, NULL, 0),
	UNIT_TEST(reset_faulted, test_gv11b_channel_reset_faulted, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_channel_gv11b, nvgpu_channel_gv11b_tests, UNIT_PRIO_NVGPU_TEST);
