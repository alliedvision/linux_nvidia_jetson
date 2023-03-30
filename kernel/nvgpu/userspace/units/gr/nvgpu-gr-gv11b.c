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
#include <unistd.h>

#include <unit/io.h>

#include <nvgpu/posix/io.h>
#include <nvgpu/posix/soc_fuse.h>
#include <nvgpu/posix/mock-regs.h>

#include <nvgpu/gk20a.h>

#include "hal/fuse/fuse_gm20b.h"

#include <nvgpu/hw/gv11b/hw_mc_gv11b.h>
#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

#include "nvgpu-gr-gv11b.h"

#define NUM_REG_SPACES 1U
static struct nvgpu_mock_iospace reg_spaces[NUM_REG_SPACES] = {
	[0] = { /* NV_PLTCG_LTCS_REGSPACE */
		.base = 0x17E200,
		.size = 0x100,
		.data = NULL,
	},
};

static void delete_reg_space(struct unit_module *m, struct gk20a *g)
{
	u32 i = 0;

	for (i = 0; i < NUM_REG_SPACES; i++) {
		nvgpu_posix_io_delete_reg_space(g, reg_spaces[i].base);
	}
}

static int add_reg_space(struct unit_module *m, struct gk20a *g)
{
	u32 i;
	int err;

	for (i = 0; i < NUM_REG_SPACES; i++) {
		struct nvgpu_mock_iospace *iospace = &reg_spaces[i];

		err = nvgpu_posix_io_add_reg_space(g, iospace->base,
						   iospace->size);
		nvgpu_assert(err == 0);
	}

	return 0;
}

int test_gr_setup_gv11b_reg_space(struct unit_module *m, struct gk20a *g)
{
	nvgpu_assert(add_reg_space(m, g) == 0);

	return 0;
}

void test_gr_cleanup_gv11b_reg_space(struct unit_module *m, struct gk20a *g)
{
	delete_reg_space(m, g);
}
