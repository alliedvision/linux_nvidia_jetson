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
#include <nvgpu/pbdma.h>
#include <nvgpu/pbdma_status.h>
#include <nvgpu/dma.h>
#include <nvgpu/io.h>

#include "hal/fifo/pbdma_status_gm20b.h"

#include <nvgpu/hw/gm20b/hw_fifo_gm20b.h>

#include "../../nvgpu-fifo-common.h"
#include "../../nvgpu-fifo-gv11b.h"
#include "nvgpu-pbdma-status-gm20b.h"

#ifdef PBDMA_STATUS_GM20B_UNIT_DEBUG
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

#define F_PBDMA_INFO_CTX_IS_TSG		BIT(0)
#define F_PBDMA_INFO_NEXT_CTX_IS_TSG	BIT(1)
#define F_PBDMA_INFO_LAST		BIT(2)

#define NUM_PBDMA_STATUS_CHAN	5

int test_gm20b_read_pbdma_status_info(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	const int pbdma_id = 0;
	u32 pbdma_reg_status = 0;
	u32 id = 1;
	u32 next_id = 5;
	u32 id_type;
	u32 next_id_type;
	struct nvgpu_pbdma_status_info status;
	u32 i;
	u32 branches = 0;

	int pbdma_status_chan[NUM_PBDMA_STATUS_CHAN] = {
		fifo_pbdma_status_chan_status_valid_v(),
		fifo_pbdma_status_chan_status_chsw_load_v(),
		fifo_pbdma_status_chan_status_chsw_save_v(),
		fifo_pbdma_status_chan_status_chsw_switch_v(),
		2 /* invalid */
	};

	u32 expected_chsw_status[NUM_PBDMA_STATUS_CHAN] = {
		NVGPU_PBDMA_CHSW_STATUS_VALID,
		NVGPU_PBDMA_CHSW_STATUS_LOAD,
		NVGPU_PBDMA_CHSW_STATUS_SAVE,
		NVGPU_PBDMA_CHSW_STATUS_SWITCH,
		NVGPU_PBDMA_CHSW_STATUS_INVALID
	};

	bool id_valid;
	bool next_id_valid;

	for (i = 0; i < NUM_PBDMA_STATUS_CHAN; i++) {
		id = (id + 1) & 0xfff;
		next_id = (next_id + 1) & 0xfff;

		id_valid =
			((pbdma_status_chan[i] == fifo_pbdma_status_chan_status_valid_v()) ||
			 (pbdma_status_chan[i] == fifo_pbdma_status_chan_status_chsw_save_v()) ||
			 (pbdma_status_chan[i] == fifo_pbdma_status_chan_status_chsw_switch_v()));

		next_id_valid =
			((pbdma_status_chan[i] == fifo_pbdma_status_chan_status_chsw_load_v()) ||
			 (pbdma_status_chan[i] == fifo_pbdma_status_chan_status_chsw_switch_v()));

		for (branches = 0; branches < F_PBDMA_INFO_LAST; branches++) {

			id_type = branches & F_PBDMA_INFO_CTX_IS_TSG ?
				fifo_pbdma_status_id_type_tsgid_v() :
				fifo_pbdma_status_id_type_chid_v();

			next_id_type = branches & F_PBDMA_INFO_NEXT_CTX_IS_TSG ?
				fifo_pbdma_status_next_id_type_tsgid_v() :
				fifo_pbdma_status_next_id_type_chid_v();

			pbdma_reg_status =
				(pbdma_status_chan[i] << 13) |
				(id << 0) |
				(id_type << 12) |
				(next_id << 16) |
				(next_id_type << 28);

			nvgpu_writel(g, fifo_pbdma_status_r(pbdma_id),
				pbdma_reg_status);

			gm20b_read_pbdma_status_info(g, pbdma_id, &status);

			unit_assert(status.pbdma_reg_status == pbdma_reg_status,
				goto done);
			unit_assert(status.chsw_status == expected_chsw_status[i],
				goto done);

			if (id_valid) {
				unit_assert(status.id == id, goto done);
				unit_assert(status.id_type == id_type, goto done);
			} else {
				unit_assert(status.id == PBDMA_STATUS_ID_INVALID,
					goto done);
				unit_assert(status.id_type ==
					PBDMA_STATUS_ID_TYPE_INVALID, goto done);
			}

			if (next_id_valid) {
				unit_assert(status.next_id == next_id, goto done);
				unit_assert(status.next_id_type == next_id_type,
					goto done);
			} else {
				unit_assert(status.next_id ==
					PBDMA_STATUS_NEXT_ID_INVALID, goto done);
				unit_assert(status.next_id_type ==
					PBDMA_STATUS_NEXT_ID_TYPE_INVALID, goto done);
			}
		}
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s i=%u branches=%08x\n", __func__, i, branches);
	}

	return ret;
}
