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

#include "hal/fifo/channel_gk20a.h"
#include <nvgpu/hw/gk20a/hw_ccsr_gk20a.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-channel-gk20a.h"

#ifdef CHANNEL_GK20A_UNIT_DEBUG
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

int test_gk20a_channel_enable(struct unit_module *m,
		struct gk20a *g, void *args)
{
	bool privileged = false;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	struct nvgpu_channel *ch;
	int ret = UNIT_FAIL;

	ch = nvgpu_channel_open_new(g, runlist_id,
		privileged, getpid(), getpid());
	unit_assert(ch, goto done);

	gk20a_channel_enable(ch);
	unit_assert((nvgpu_readl(ch->g, ccsr_channel_r(ch->chid))
		& ccsr_channel_enable_set_true_f()) != 0, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ch) {
		nvgpu_channel_close(ch);
	}

	return ret;
}

int test_gk20a_channel_disable(struct unit_module *m,
		struct gk20a *g, void *args)
{
	bool privileged = false;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	struct nvgpu_channel *ch;
	int ret = UNIT_FAIL;

	ch = nvgpu_channel_open_new(g, runlist_id,
		privileged, getpid(), getpid());
	unit_assert(ch, goto done);

	gk20a_channel_disable(ch);
	unit_assert((nvgpu_readl(ch->g, ccsr_channel_r(ch->chid))
		& ccsr_channel_enable_clr_true_f()) != 0, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ch) {
		nvgpu_channel_close(ch);
	}

	return ret;
}


#define F_CHANNEL_READ_STATE_NEXT				BIT(0)
#define F_CHANNEL_READ_STATE_ENABLED				BIT(1)
#define F_CHANNEL_READ_STATE_BUSY				BIT(2)
#define F_CHANNEL_READ_STATE_LAST				BIT(3)

static const char *f_channel_read_state[] = {
	"next",
	"enabled",
	"busy",
};

int test_gk20a_channel_read_state(struct unit_module *m,
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

		u32 v_next = 0;
		u32 v_enable = 0;
		u32 v_status = 0;
		u32 v_busy = 0;
		u32 v = 0;

		bool next = false;
		bool enabled = false;
		bool busy = false;

		if (branches & F_CHANNEL_READ_STATE_NEXT) {
			v_next = ccsr_channel_next_true_v();
			next = true;
		}

		if (branches & F_CHANNEL_READ_STATE_ENABLED) {
			v_enable = ccsr_channel_enable_in_use_v();
			enabled = true;
		}

		if (branches & F_CHANNEL_READ_STATE_BUSY) {
			v_busy = ccsr_channel_busy_true_v();
			busy = true;
		}

		for (v_status = ccsr_channel_status_idle_v();
			v_status <= ccsr_channel_status_on_eng_pending_acq_ctx_reload_v();
			v_status++) {

			bool ctx_reload = false;
			bool pending_acquire = false;

			unit_verbose(m, "%s branches=%s v_status=%x\n",
				__func__,
				branches_str(branches, f_channel_read_state),
				v_status);

			switch (v_status) {
			case ccsr_channel_status_pending_ctx_reload_v():
			case ccsr_channel_status_pending_acq_ctx_reload_v():
			case ccsr_channel_status_on_pbdma_ctx_reload_v():
			case ccsr_channel_status_on_pbdma_and_eng_ctx_reload_v():
			case ccsr_channel_status_on_eng_ctx_reload_v():
			case ccsr_channel_status_on_eng_pending_ctx_reload_v():
			case ccsr_channel_status_on_eng_pending_acq_ctx_reload_v():
				ctx_reload = true;
				break;

			case ccsr_channel_status_pending_acquire_v():
			case ccsr_channel_status_on_eng_pending_acquire_v():
				pending_acquire = true;
				break;

			default:
				break;
			}

			v = v_enable << 0U | v_next << 1U | v_status << 24U | v_busy << 28U;

			nvgpu_writel(g, ccsr_channel_r(ch->chid), v);

			gk20a_channel_read_state(g, ch, &state);

			unit_assert(state.next == next, goto done);
			unit_assert(state.enabled == enabled, goto done);
			unit_assert(state.busy == busy, goto done);
			unit_assert(state.ctx_reload == ctx_reload, goto done);
			unit_assert(state.pending_acquire == pending_acquire,
				goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ch) {
		nvgpu_channel_close(ch);
	}

	return ret;
}

struct unit_module_test nvgpu_channel_gk20a_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(enable, test_gk20a_channel_enable, NULL, 0),
	UNIT_TEST(disable, test_gk20a_channel_disable, NULL, 0),
	UNIT_TEST(read_state, test_gk20a_channel_read_state, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_channel_gk20a, nvgpu_channel_gk20a_tests, UNIT_PRIO_NVGPU_TEST);
