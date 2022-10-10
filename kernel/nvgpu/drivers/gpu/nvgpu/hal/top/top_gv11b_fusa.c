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

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>

#include "top_gv11b.h"

#include <nvgpu/hw/gv11b/hw_top_gv11b.h>

u32 gv11b_top_get_num_lce(struct gk20a *g)
{
	u32 reg_val, num_lce;

	reg_val = nvgpu_readl(g, top_num_ces_r());
	num_lce = top_num_ces_value_v(reg_val);
	nvgpu_log_info(g, "num LCE: %d", num_lce);

	return num_lce;
}

u32 gv11b_top_get_max_pes_per_gpc(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g, top_num_pes_per_gpc_r());
	return top_num_pes_per_gpc_value_v(tmp);
}
int gv11b_device_info_parse_data(struct gk20a *g, u32 table_entry, u32 *inst_id,
		u32 *pri_base, u32 *fault_id)
{
	if (top_device_info_data_type_v(table_entry) !=
					top_device_info_data_type_enum2_v()) {
		nvgpu_err(g, "Unknown device_info_data_type %u",
				top_device_info_data_type_v(table_entry));
		return -EINVAL;
	}

	nvgpu_log_info(g, "Entry_data to be parsed 0x%x", table_entry);

	*pri_base = (top_device_info_data_pri_base_v(table_entry) <<
				top_device_info_data_pri_base_align_v());
	nvgpu_log_info(g, "Pri Base addr: 0x%x", *pri_base);

	if (top_device_info_data_fault_id_v(table_entry) ==
				top_device_info_data_fault_id_valid_v()) {
		*fault_id = top_device_info_data_fault_id_enum_v(table_entry);
	} else {
		*fault_id = U32_MAX;
	}
	nvgpu_log_info(g, "Fault_id: %u", *fault_id);

	*inst_id = top_device_info_data_inst_id_v(table_entry);
	nvgpu_log_info(g, "Inst_id: %u", *inst_id);

	return 0;
}
