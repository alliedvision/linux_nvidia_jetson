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

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/engines.h>
#include <nvgpu/engine_status.h>

#include "hal/fifo/engines_gv11b.h"

#include <nvgpu/hw/gv11b/hw_fifo_gv11b.h>
#include <nvgpu/hw/gv11b/hw_gmmu_gv11b.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-engine-gv11b.h"

#ifdef ENGINE_GV11B_UNIT_DEBUG
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

int test_gv11b_is_fault_engine_subid_gpc(struct unit_module *m,
		struct gk20a *g, void *args)
{

	int ret = UNIT_FAIL;

	unit_assert(gv11b_is_fault_engine_subid_gpc(g,
		gmmu_fault_client_type_gpc_v()) == true, goto done);
	unit_assert(gv11b_is_fault_engine_subid_gpc(g,
		gmmu_fault_client_type_hub_v()) == false, goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

struct unit_module_test nvgpu_engine_gv11b_tests[] = {
	UNIT_TEST(is_fault_engine_subid_gpc, test_gv11b_is_fault_engine_subid_gpc, NULL, 0),
};

UNIT_MODULE(nvgpu_engine_gv11b, nvgpu_engine_gv11b_tests, UNIT_PRIO_NVGPU_TEST);
