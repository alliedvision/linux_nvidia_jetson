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

#include <nvgpu/posix/bug.h>

#include "hal/fifo/channel_gm20b.h"
#include <nvgpu/hw/gm20b/hw_ccsr_gm20b.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-channel-gm20b.h"

#ifdef CHANNEL_GM20B_UNIT_DEBUG
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

int test_gm20b_channel_bind(struct unit_module *m,
		struct gk20a *g, void *args)
{
	bool privileged = false;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	struct nvgpu_channel *ch;
	int ret = UNIT_FAIL;
	u32 chid;
	int err;

	ch = nvgpu_channel_open_new(g, runlist_id,
		privileged, getpid(), getpid());
	unit_assert(ch, goto done);
	unit_assert(nvgpu_atomic_read(&ch->bound) == 0, goto done);

	nvgpu_writel(g, ccsr_channel_inst_r(ch->chid), 0);
	nvgpu_writel(g, ccsr_channel_r(ch->chid), 0);

	gm20b_channel_bind(ch);

	unit_assert(nvgpu_readl(g, ccsr_channel_inst_r(ch->chid)) != 0, goto done);
	unit_assert(nvgpu_readl(g, ccsr_channel_r(ch->chid)) != 0, goto done);
	unit_assert(nvgpu_atomic_read(&ch->bound) == 1, goto done);

	nvgpu_atomic_set(&ch->bound, 0);

	chid = ch->chid;
	ch->chid = U32_MAX;
	err = EXPECT_BUG(gm20b_channel_bind(ch));
	ch->chid = chid;
	unit_assert(err != 0, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ch) {
		nvgpu_channel_close(ch);
	}

	return ret;
}

int test_gm20b_channel_force_ctx_reload(struct unit_module *m,
		struct gk20a *g, void *args)
{
	bool privileged = false;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	struct nvgpu_channel *ch;
	int ret = UNIT_FAIL;
	u32 chid;
	int err;

	ch = nvgpu_channel_open_new(g, runlist_id,
		privileged, getpid(), getpid());
	unit_assert(ch, goto done);

	nvgpu_writel(g, ccsr_channel_r(ch->chid), 0);
	gm20b_channel_force_ctx_reload(ch);
	unit_assert((nvgpu_readl(g, ccsr_channel_r(ch->chid)) &
		ccsr_channel_force_ctx_reload_true_f()) != 0, goto done);

	chid = ch->chid;
	ch->chid = U32_MAX;
	err = EXPECT_BUG(gm20b_channel_force_ctx_reload(ch));
	ch->chid = chid;
	unit_assert(err != 0, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ch) {
		nvgpu_channel_close(ch);
	}

	return ret;
}

struct unit_module_test nvgpu_channel_gm20b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(bind, test_gm20b_channel_bind, NULL, 0),
	UNIT_TEST(force_ctx_reload, test_gm20b_channel_force_ctx_reload, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_channel_gm20b, nvgpu_channel_gm20b_tests, UNIT_PRIO_NVGPU_TEST);
