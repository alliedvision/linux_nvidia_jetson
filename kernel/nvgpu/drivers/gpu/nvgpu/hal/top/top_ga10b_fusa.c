/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/device.h>
#include <nvgpu/log.h>

#include "top_ga10b.h"

#include <nvgpu/hw/ga10b/hw_top_ga10b.h>

u32 ga10b_get_num_engine_type_entries(struct gk20a *g, u32 engine_type)
{
	(void)g;
	(void)engine_type;
	/*
	 * Will be replaced by core code function in next patch!
	 */
	return 0U;
}

static struct nvgpu_device *ga10b_top_parse_device(struct gk20a *g,
						   u32 *rows, u32 num_rows)
{
	bool valid_device_info = false;
	struct nvgpu_device *dev;

	(void)num_rows;

	/*
	 * ga10b  device info structure
	 * 31           24 23           16 15       10   8 7             0
	 * .-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-.
	 * |1|  type_enum  |  instance_id  |0 0 0 0 0|     fault_id        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |1|E|0 0 0 0|        device_pri_base            |   reset_id    |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |0|0|0 0 0 0|       runlist_pri_base        |0 0 0 0 0 0 0 0|rle|
	 * `-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
	 */

	/*
	 * Ensure we have at least 2 entries by checking row 0's chain bit. Not
	 * all devices (e.g GSP) have 3 rows populated.
	 */
	valid_device_info = (top_device_info2_row_chain_v(rows[0]) ==
			     top_device_info2_row_chain_more_v());

	/* Check for valid device info */
	if (!valid_device_info) {
		nvgpu_warn(g, "Parsed broken device from device table!");
		return NULL;
	}

	dev = nvgpu_kzalloc(g, sizeof(*dev));
	if (dev == NULL) {
		nvgpu_err(g, "OOM allocating device!");
		return NULL;
	}

	/*
	 * Many of the below fields are not valid for non-engine devices. That's
	 * ok - we can still parse the fields; they'll just be 0s.
	 */
	dev->type = top_device_info2_dev_type_enum_v(rows[0]);
	dev->inst_id = top_device_info2_dev_instance_id_v(rows[0]);
	dev->fault_id = top_device_info2_dev_fault_id_v(rows[0]);
	dev->reset_id = top_device_info2_dev_reset_id_v(rows[1]);
	dev->pri_base = top_device_info2_dev_device_pri_base_v(rows[1]) <<
		top_device_info2_dev_device_pri_base_b();

	dev->engine = top_device_info2_dev_is_engine_v(rows[1]) ==
		top_device_info2_dev_is_engine_true_v();
	dev->rleng_id = top_device_info2_dev_rleng_id_v(rows[2]);
	dev->rl_pri_base =
		top_device_info2_dev_runlist_pri_base_v(rows[2]) <<
		top_device_info2_dev_runlist_pri_base_b();

	if (dev->engine) {
		dev->engine_id = g->ops.runlist.get_engine_id_from_rleng_id(g,
								dev->rleng_id,
								dev->rl_pri_base);
		dev->runlist_id = g->ops.runlist.get_runlist_id(g,
								dev->rl_pri_base);
		dev->intr_id = g->ops.runlist.get_engine_intr_id(g,
								 dev->rl_pri_base,
								 dev->rleng_id);
		g->ops.runlist.get_pbdma_info(g,
					      dev->rl_pri_base,
					      &dev->pbdma_info);

	}

	return dev;
}

static u32 ga10b_top_table_size(struct gk20a *g)
{
	u32 cfg = nvgpu_readl(g, top_device_info_cfg_r());

	return top_device_info_cfg_num_rows_v(cfg);
}

/*
 * On Ampere there are 3 rows per device. Although the HW does leave open the
 * option for adding rows in the future, for now, let's just hard code to row
 * reads. We have to use specific rows for specific fields
 */
struct nvgpu_device *ga10b_top_parse_next_dev(struct gk20a *g, u32 *token)
{
	/*
	 * FIXME: HW define for this exists.
	 */
	#define MAX_ROWS 3

	u32 j, cfg;
	u32 rows[MAX_ROWS] = { 0 };

	cfg = nvgpu_readl(g, top_device_info_cfg_r());
	if (top_device_info_cfg_version_v(cfg) !=
			top_device_info_cfg_version_init_v()) {
		nvgpu_err(g, "device info cfg mismatch");
		return NULL;
	}

	/*
	 * Skip any empty rows. We can assume that this function won't have been
	 * called mid row, so if we see a 0U row value, then its before we've
	 * started parsing a device. Thus we can just skip it. But be careful
	 * not to run past the end of the device register array!
	 */
	while (*token < ga10b_top_table_size(g)) {
		rows[0] = nvgpu_readl(g, top_device_info2_r(*token));
		(*token)++;

		if (rows[0] != 0U) {
			break;
		}
	}

	if (*token >= ga10b_top_table_size(g)) {
		return NULL;
	}

	/*
	 * Use the *token value to index the actual table; but keep a local j
	 * index counter to limit ourselves to the max number or rows in a
	 * table. We can skip the first entry since that got read in the
	 * while-loop above.
	 */
	for (j = 1U; j < MAX_ROWS; j++) {
		rows[j] = nvgpu_readl(g, top_device_info2_r(*token));
		(*token)++;

		if (top_device_info2_row_chain_v(rows[j]) ==
		    top_device_info2_row_chain_last_v()) {
			break;
		}
	}

	return ga10b_top_parse_device(g, rows, MAX_ROWS);
}

u32 ga10b_top_get_max_rop_per_gpc(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g, top_num_rop_per_gpc_r());
	return top_num_rop_per_gpc_value_v(tmp);
}
