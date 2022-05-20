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

#include "hal/fifo/userd_gk20a.h"

#include <nvgpu/hw/gk20a/hw_ram_gk20a.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-userd-gk20a.h"

#ifdef USERD_GK20A_UNIT_DEBUG
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

int test_gk20a_userd_entry_size(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 size = gk20a_userd_entry_size(g);

	unit_assert(size == ram_userd_chan_size_v(), goto done);
	ret = UNIT_SUCCESS;
done:
	return ret;
}

struct unit_module_test nvgpu_userd_gk20a_tests[] = {
	UNIT_TEST(entry_size, test_gk20a_userd_entry_size, NULL, 0),
};

UNIT_MODULE(nvgpu_userd_gk20a, nvgpu_userd_gk20a_tests, UNIT_PRIO_NVGPU_TEST);
