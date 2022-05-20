/*
 * GP10B L2
 *
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/ltc.h>
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/ecc.h>
#include <nvgpu/static_analysis.h>

#include <nvgpu/hw/gp10b/hw_ltc_gp10b.h>

#include "ltc_gm20b.h"
#include "ltc_gp10b.h"

void gp10b_ltc_init_fs_state(struct gk20a *g)
{
	gm20b_ltc_init_fs_state(g);

	gk20a_writel(g, ltc_ltca_g_axi_pctrl_r(),
			ltc_ltca_g_axi_pctrl_user_sid_f(g->ltc_streamid));

}

int gp10b_lts_ecc_init(struct gk20a *g)
{
	int err = 0;

	err = NVGPU_ECC_COUNTER_INIT_PER_LTS(ecc_sec_count);
	if (err != 0) {
		goto init_lts_err;
	}
	err = NVGPU_ECC_COUNTER_INIT_PER_LTS(ecc_ded_count);

init_lts_err:
	if (err != 0) {
		nvgpu_err(g, "ecc counter allocate failed, err=%d", err);
		nvgpu_ltc_ecc_free(g);
	}

	return err;
}
