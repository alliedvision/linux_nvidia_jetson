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

#include "hal/fifo/fifo_gk20a.h"
#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-fifo-gk20a.h"
#include "nvgpu-fifo-intr-gk20a.h"

#define FIFO_GK20A_UNIT_DEBUG
#ifdef FIFO_GK20A_UNIT_DEBUG
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

#define UNIT_MAX_PBDMA	32

int test_gk20a_get_timeslices(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 rl_timeslice = gk20a_fifo_get_runlist_timeslice(g);
	u32 pb_timeslice = gk20a_fifo_get_pb_timeslice(g);

	/* check that timeslices are enabled */
	unit_assert((rl_timeslice & fifo_runlist_timeslice_enable_true_f()) !=
				0, goto done);
	unit_assert((pb_timeslice & fifo_pb_timeslice_enable_true_f()) != 0,
				goto done);

	/* check that timeslices are non-zero */
	unit_assert((rl_timeslice & 0xFF) != 0, goto done);
	unit_assert((pb_timeslice & 0xFF) != 0, goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

struct unit_module_test nvgpu_fifo_gk20a_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),

	/* fifo gk20a */
	UNIT_TEST(get_timeslices, test_gk20a_get_timeslices, NULL, 0),

	/* fifo intr gk20a */
	UNIT_TEST(intr_1_enable, test_gk20a_fifo_intr_1_enable, NULL, 0),
	UNIT_TEST(intr_1_isr, test_gk20a_fifo_intr_1_isr, NULL, 0),
	UNIT_TEST(intr_handle_chsw_error, test_gk20a_fifo_intr_handle_chsw_error, NULL, 0),
	UNIT_TEST(intr_handle_runlist_event, test_gk20a_fifo_intr_handle_runlist_event, NULL, 0),
	UNIT_TEST(pbdma_isr, test_gk20a_fifo_pbdma_isr, NULL, 0),

	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_fifo_gk20a, nvgpu_fifo_gk20a_tests, UNIT_PRIO_NVGPU_TEST);
