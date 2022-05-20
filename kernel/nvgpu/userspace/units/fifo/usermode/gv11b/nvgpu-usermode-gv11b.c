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
#include <nvgpu/runlist.h>
#include <nvgpu/tsg.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/io_usermode.h>

#include "hal/fifo/usermode_gv11b.h"

#include <nvgpu/hw/gv11b/hw_usermode_gv11b.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-usermode-gv11b.h"

#ifdef USERMODE_GV11B_UNIT_DEBUG
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

#define branches_str 	test_fifo_flags_str

int test_gv11b_usermode(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u64 base = gv11b_usermode_base(g);
	u64 bus_base = gv11b_usermode_bus_base(g);
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_fifo *f = &g->fifo;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;
	u32 hw_chid;
	u32 token;
	u32 val;

	unit_assert(base == usermode_cfg0_r(), goto done);
	unit_assert(bus_base == usermode_cfg0_r(), goto done);

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);
	hw_chid = f->channel_base + ch->chid;

	token = gv11b_usermode_doorbell_token(ch);
	unit_assert(token == usermode_notify_channel_pending_id_f(hw_chid),
			goto done);

	nvgpu_usermode_writel(g, usermode_notify_channel_pending_r(), 0);
	gv11b_usermode_ring_doorbell(ch);
	val = nvgpu_readl(g, usermode_notify_channel_pending_r());
	unit_assert(val == token, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	return ret;
}

struct unit_module_test nvgpu_usermode_gv11b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(usermode, test_gv11b_usermode, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_usermode_gv11b, nvgpu_usermode_gv11b_tests, UNIT_PRIO_NVGPU_TEST);
