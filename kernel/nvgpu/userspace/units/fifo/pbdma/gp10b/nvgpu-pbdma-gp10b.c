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

#include "hal/fifo/pbdma_gp10b.h"

#include <nvgpu/hw/gp10b/hw_pbdma_gp10b.h>
#include <nvgpu/hw/gp10b/hw_ram_gp10b.h>

#include "../../nvgpu-fifo-common.h"
#include "../../nvgpu-fifo-gv11b.h"
#include "nvgpu-pbdma-gp10b.h"

#ifdef PBDMA_GP10B_UNIT_DEBUG
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

int test_gp10b_pbdma_get_signature(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	unit_assert(gp10b_pbdma_get_signature(g) ==
		(g->ops.get_litter_value(g, GPU_LIT_GPFIFO_CLASS) |
			pbdma_signature_sw_zero_f()), goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

#define RL_MAX_TIMESLICE_TIMEOUT ram_rl_entry_timeslice_timeout_v(U32_MAX)
#define RL_MAX_TIMESLICE_SCALE ram_rl_entry_timeslice_scale_v(U32_MAX)

int test_gp10b_pbdma_get_fc_runlist_timeslice(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 timeslice = gp10b_pbdma_get_fc_runlist_timeslice();

	u32 timeout = timeslice & 0xFF;
	u32 timescale = (timeslice >> 12) & 0xF;
	bool enabled = ((timeslice & pbdma_runlist_timeslice_enable_true_f()) != 0);

	unit_assert(timeout <= RL_MAX_TIMESLICE_TIMEOUT, goto done);
	unit_assert(timescale <= RL_MAX_TIMESLICE_SCALE, goto done);
	unit_assert(enabled, goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_gp10b_pbdma_get_config_auth_level_privileged(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;

	unit_assert(gp10b_pbdma_get_config_auth_level_privileged() ==
		pbdma_config_auth_level_privileged_f(), goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

struct unit_module_test nvgpu_pbdma_gp10b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(get_signature, test_gp10b_pbdma_get_signature, NULL, 0),
	UNIT_TEST(get_fc_runlist_timeslice,
			test_gp10b_pbdma_get_fc_runlist_timeslice, NULL, 0),
	UNIT_TEST(get_config_auth_level_privileged,
			test_gp10b_pbdma_get_config_auth_level_privileged, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_pbdma_gp10b, nvgpu_pbdma_gp10b_tests, UNIT_PRIO_NVGPU_TEST);
