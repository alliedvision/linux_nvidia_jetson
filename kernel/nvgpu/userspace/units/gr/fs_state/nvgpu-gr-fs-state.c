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

#include <unit/unit.h>
#include <unit/io.h>

#include <nvgpu/posix/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/fs_state.h>
#include <nvgpu/gr/gr_utils.h>

#include <nvgpu/posix/posix-fault-injection.h>

#include "common/gr/gr_priv.h"

#include "../nvgpu-gr.h"
#include "nvgpu-gr-fs-state.h"

static u32 gr_get_number_of_sm(struct gk20a *g)
{
	return 0;
}

int test_gr_fs_state_error_injection(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err;
	struct gpu_ops gops = g->ops;
	struct nvgpu_gr_config *config = nvgpu_gr_get_config_ptr(g);
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	/* Fail g->ops.gr.config.init_sm_id_table() */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
	err = nvgpu_gr_fs_state_init(g, config);
	if (err == 0) {
		return UNIT_FAIL;
	}

	/* Fail gr_load_sm_id_config() */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 13);
	err = nvgpu_gr_fs_state_init(g, config);
	if (err == 0) {
		return UNIT_FAIL;
	}

	/* Passing case */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	err = nvgpu_gr_fs_state_init(g, config);
	if (err != 0) {
		return UNIT_FAIL;
	}

	/* No SM is detected - failing case */
	g->ops.gr.init.get_no_of_sm = gr_get_number_of_sm;
	err = EXPECT_BUG(nvgpu_gr_fs_state_init(g, config));
	if (err == 0) {
		return UNIT_FAIL;
	}

	/* Restore gops */
	g->ops = gops;

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_gr_fs_state_tests[] = {
	UNIT_TEST(gr_fs_state_setup, test_gr_init_setup_ready, NULL, 0),
	UNIT_TEST(gr_fs_state_error_injection, test_gr_fs_state_error_injection, NULL, 2),
	UNIT_TEST(gr_fs_state_cleanup, test_gr_init_setup_cleanup, NULL, 0),
};

UNIT_MODULE(nvgpu_gr_fs_state, nvgpu_gr_fs_state_tests, UNIT_PRIO_NVGPU_TEST);
