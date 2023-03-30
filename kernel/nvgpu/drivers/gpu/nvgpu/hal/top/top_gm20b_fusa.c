/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/device.h>
#include <nvgpu/types.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/kmem.h>

#include "top_gm20b.h"

#include <nvgpu/hw/gm20b/hw_top_gm20b.h>

void gm20b_device_info_parse_enum(struct gk20a *g, u32 table_entry,
				u32 *engine_id, u32 *runlist_id,
				u32 *intr_id, u32 *reset_id)
{
	nvgpu_log_info(g, "Entry_enum to be parsed 0x%x", table_entry);

	if (top_device_info_engine_v(table_entry) ==
					top_device_info_engine_valid_v()) {
		*engine_id = top_device_info_engine_enum_v(table_entry);
	} else {
		*engine_id = U32_MAX;
	}
	nvgpu_log_info(g, "Engine_id: %u", *engine_id);

	if (top_device_info_runlist_v(table_entry) ==
					top_device_info_runlist_valid_v()) {
		*runlist_id = top_device_info_runlist_enum_v(table_entry);
	} else {
		*runlist_id = U32_MAX;
	}
	nvgpu_log_info(g, "Runlist_id: %u", *runlist_id);

	if (top_device_info_intr_v(table_entry) ==
					top_device_info_intr_valid_v()) {
		*intr_id = top_device_info_intr_enum_v(table_entry);
	} else {
		*intr_id = U32_MAX;
	}
	nvgpu_log_info(g, "Intr_id: %u", *intr_id);

	if (top_device_info_reset_v(table_entry) ==
					top_device_info_reset_valid_v()) {
		*reset_id = top_device_info_reset_enum_v(table_entry);
	} else {
		*reset_id = U32_MAX;
	}
	nvgpu_log_info(g, "Reset_id: %u", *reset_id);

}

/*
 * Parse the device starting at *token. This will return a valid device struct
 * pointer if a device was detected and parsed, NULL otherwise.
 */
struct nvgpu_device *gm20b_top_parse_next_dev(struct gk20a *g, u32 *token)
{
	int ret;
	u32 table_entry;
	u32 entry;
	u32 entry_enum = 0;
	u32 entry_engine = 0;
	u32 entry_data = 0;
	struct nvgpu_device *dev;

	while (true) {
		/*
		 * The core code relies on us to manage the index - a.k.a the
		 * token. If token crosses the device table size, then break and
		 * return NULL to signify we've hit the end of the dev list.
		 */
		if (*token >= top_device_info__size_1_v()) {
			return NULL;
		}

		/*
		 * Once we have read a register we'll never have to read it
		 * again so always increment before doing anything further.
		 */
		table_entry = nvgpu_readl(g, top_device_info_r(*token));
		(*token)++;

		entry = top_device_info_entry_v(table_entry);

		if (entry == top_device_info_entry_not_valid_v()) {
			/*
			 * Empty section of the table. We'll skip these
			 * internally so that the common device manager is
			 * unaware of the holes in the device register array.
			 */
			continue;
		} else if (entry == top_device_info_entry_enum_v()) {
			entry_enum = table_entry;
		} else if (entry == top_device_info_entry_data_v()) {
			entry_data = table_entry;
		} else if (entry == top_device_info_entry_engine_type_v()) {
			entry_engine = table_entry;
		} else {
			nvgpu_err(g, "Invalid entry type in device_info table");
			return NULL;
		}

		/*
		 * If we need to chain then we need to read the register in the
		 * table. Otherwise, if chain is false, then we have parsed all
		 * the relevant registers for this table entry.
		 */
		if (top_device_info_chain_v(table_entry) ==
		    top_device_info_chain_enable_v()) {
			continue;
		}

		/*
		 * If we get here we have sufficient data to parse a device. E.g
		 * chain was set to 0.
		 */
		dev = nvgpu_kzalloc(g, sizeof(*dev));
		if (dev == NULL) {
			nvgpu_err(g, "TOP: OOM allocating nvgpu_device struct");
			return NULL;
		}

		dev->type = top_device_info_type_enum_v(entry_engine);
		g->ops.top.device_info_parse_enum(g,
						  entry_enum,
						  &dev->engine_id,
						  &dev->runlist_id,
						  &dev->intr_id,
						  &dev->reset_id);
		ret = g->ops.top.device_info_parse_data(g,
							entry_data,
							&dev->inst_id,
							&dev->pri_base,
							&dev->fault_id);
		if (ret != 0) {
			nvgpu_err(g,
				  "TOP: error parsing Data Entry 0x%x",
				  entry_data);
			nvgpu_kfree(g, dev);
			return NULL;
		}

		/*
		 * SW hack: override the HW inst_id field for COPY1 and 2.
		 * Although each CE on gm20b is considered its own device type
		 * that's not really very sensible. HW fixes this in future
		 * chips, but for now, set the inst_id field to a more intuitive
		 * value.
		 *
		 * It's possible this can be fixed in the future such that nvgpu
		 * does not rely on this; it'd make a lot of code less arbitrary.
		 */
		if (dev->type == NVGPU_DEVTYPE_COPY1) {
			dev->inst_id = 1U;
		}
		if (dev->type == NVGPU_DEVTYPE_COPY2) {
			dev->inst_id = 2U;
		}


		break;
	}

	return dev;
}

u32 gm20b_top_get_max_gpc_count(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g, top_num_gpcs_r());
	return top_num_gpcs_value_v(tmp);
}

u32 gm20b_top_get_max_tpc_per_gpc_count(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g, top_tpc_per_gpc_r());
	return top_tpc_per_gpc_value_v(tmp);
}

u32 gm20b_top_get_max_fbps_count(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g, top_num_fbps_r());
	return top_num_fbps_value_v(tmp);
}

u32 gm20b_top_get_max_ltc_per_fbp(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g,  top_ltc_per_fbp_r());
	return top_ltc_per_fbp_value_v(tmp);
}

u32 gm20b_top_get_max_lts_per_ltc(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g,  top_slices_per_ltc_r());
	return top_slices_per_ltc_value_v(tmp);
}

u32 gm20b_top_get_num_ltcs(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g, top_num_ltcs_r());
	return top_num_ltcs_value_v(tmp);
}
