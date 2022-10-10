/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/config.h>

#include "gr_config_gm20b.h"

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

int gm20b_gr_config_init_sm_id_table(struct gk20a *g,
		struct nvgpu_gr_config *gr_config)
{
	u32 gpc, tpc;
	u32 sm_id = 0;
	u32 num_sm;

	num_sm = nvgpu_safe_mult_u32(nvgpu_gr_config_get_tpc_count(gr_config),
				     nvgpu_gr_config_get_sm_count_per_tpc(gr_config));
	nvgpu_gr_config_set_no_of_sm(gr_config, num_sm);

	(void)g;

	for (tpc = 0;
	     tpc < nvgpu_gr_config_get_max_tpc_per_gpc_count(gr_config);
	     tpc++) {
		for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(gr_config); gpc++) {

			if (tpc < nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc)) {
				struct nvgpu_sm_info *sm_info =
					nvgpu_gr_config_get_sm_info(gr_config, sm_id);
				nvgpu_gr_config_set_sm_info_tpc_index(sm_info, tpc);
				nvgpu_gr_config_set_sm_info_gpc_index(sm_info, gpc);
				nvgpu_gr_config_set_sm_info_sm_index(sm_info, 0);
				nvgpu_gr_config_set_sm_info_global_tpc_index(sm_info, sm_id);
				sm_id = nvgpu_safe_add_u32(sm_id, 1U);
			}
		}
	}
	nvgpu_assert(num_sm == sm_id);
	return 0;
}
