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
#include <nvgpu/device.h>
#include <nvgpu/engines.h>
#include <nvgpu/engine_status.h>

#include "hal/fifo/engine_status_gm20b.h"

#include <nvgpu/hw/gm20b/hw_fifo_gm20b.h>
#include <nvgpu/hw/gm20b/hw_top_gm20b.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-engine-gm20b.h"

#ifdef ENGINE_GM20B_UNIT_DEBUG
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

#define branches_str test_fifo_flags_str
#define pruned	test_fifo_subtest_pruned

#define F_ENGINE_READ_STATUS_BUSY		BIT(0)
#define F_ENGINE_READ_STATUS_FAULTED		BIT(1)
#define F_ENGINE_READ_STATUS_ID_TSG		BIT(2)
#define F_ENGINE_READ_STATUS_ID_NEXT_TSG	BIT(3)
#define F_ENGINE_READ_STATUS_LAST		BIT(4)

#define NUM_STATES	5

int test_gm20b_read_engine_status_info(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_engine_status_info expected;
	struct nvgpu_engine_status_info status;
	struct nvgpu_fifo *f = &g->fifo;
	u32 engine_id = 0;
	u32 ctx_id, ctx_id_type;
	u32 ctx_next_id, ctx_next_id_type;
	u32 branches = 0;
	u32 data;
	u32 ctxsw_status;
	const char *labels[] = {
		"busy",
		"faulted",
		"id_tsg",
		"id_next_tsg",
		"ctx_valid",
		"ctx_load",
		"ctx_save",
		"ctx_switch",
	};
	char *ctxsw_status_label = NULL;

	unit_assert(f->num_engines > 0, goto done);

	nvgpu_writel(g, fifo_engine_status_r(engine_id), 0xbeef);
	gm20b_read_engine_status_info(g, NVGPU_INVALID_ENG_ID, &status);
	unit_assert(status.reg_data == 0, goto done);

	for (branches = 0; branches < F_ENGINE_READ_STATUS_LAST; branches++) {

		memset(&expected, 0, sizeof(expected));
		memset(&status, 0, sizeof(status));

		data = 0U;

		if (branches & F_ENGINE_READ_STATUS_ID_TSG) {
			ctx_id = 1;
			ctx_id_type = ENGINE_STATUS_CTX_ID_TYPE_TSGID;
			data |= (fifo_engine_status_id_type_tsgid_v() << 12);

		} else {
			ctx_id = 101;
			ctx_id_type = ENGINE_STATUS_CTX_ID_TYPE_CHID;
			data |= (fifo_engine_status_id_type_chid_v() << 12);
		}
		data |= (ctx_id << 0);

		if (branches & F_ENGINE_READ_STATUS_ID_NEXT_TSG) {
			ctx_next_id = 2;
			ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID;
			data |= (fifo_engine_status_next_id_type_tsgid_v() << 28);
		} else {
			ctx_next_id = 102;
			ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_CHID;
			data |= (fifo_engine_status_next_id_type_chid_v() << 28);
		}
		data |= (ctx_next_id << 16);

		if (branches & F_ENGINE_READ_STATUS_BUSY) {
			data |= BIT(31);
			expected.is_busy = true;
		}

		if (branches & F_ENGINE_READ_STATUS_FAULTED) {
			data |= BIT(30);
			expected.is_faulted = true;
		}

		for (ctxsw_status = NVGPU_CTX_STATUS_INVALID;
			ctxsw_status <= NVGPU_CTX_STATUS_CTXSW_SWITCH;
			ctxsw_status++) {

			expected.ctx_id = ENGINE_STATUS_CTX_ID_INVALID;
			expected.ctx_id_type = ENGINE_STATUS_CTX_ID_TYPE_INVALID;
			expected.ctx_next_id = ENGINE_STATUS_CTX_NEXT_ID_INVALID;
			expected.ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_INVALID;
			data = data & ~(0x7 << 13);

			switch (ctxsw_status) {
				case NVGPU_CTX_STATUS_VALID:
					data |= (fifo_engine_status_ctx_status_valid_v() << 13);
					expected.ctx_id = ctx_id;
					expected.ctx_id_type = ctx_id_type;
					expected.ctxsw_status = NVGPU_CTX_STATUS_VALID;
					ctxsw_status_label = "valid";
					break;

				case NVGPU_CTX_STATUS_CTXSW_LOAD:
					data |= (fifo_engine_status_ctx_status_ctxsw_load_v() << 13);
					expected.ctx_next_id = ctx_next_id;
					expected.ctx_next_id_type = ctx_next_id_type;
					expected.ctxsw_status = NVGPU_CTX_STATUS_CTXSW_LOAD;
					ctxsw_status_label = "load";
					break;

				case NVGPU_CTX_STATUS_CTXSW_SAVE:
					data |= (fifo_engine_status_ctx_status_ctxsw_save_v() << 13);
					expected.ctx_id = ctx_id;
					expected.ctx_id_type = ctx_id_type;
					expected.ctxsw_status = NVGPU_CTX_STATUS_CTXSW_SAVE;
					ctxsw_status_label = "save";
					break;

				case NVGPU_CTX_STATUS_CTXSW_SWITCH:
					data |= (fifo_engine_status_ctx_status_ctxsw_switch_v() << 13);
					expected.ctx_id = ctx_id;
					expected.ctx_id_type = ctx_id_type;
					expected.ctx_next_id = ctx_next_id;
					expected.ctx_next_id_type = ctx_next_id_type;
					expected.ctxsw_status = NVGPU_CTX_STATUS_CTXSW_SWITCH;
					ctxsw_status_label = "switch";
					break;

				default:
				case NVGPU_CTX_STATUS_INVALID:
					expected.ctxsw_status = NVGPU_CTX_STATUS_INVALID;
					ctxsw_status_label = "invalid";
					break;
			}

			if (data & fifo_engine_status_ctxsw_in_progress_f()) {
				expected.ctxsw_in_progress = true;
			}

			unit_verbose(m, "%s branches=%s %s\n", __func__,
				branches_str(branches, labels), ctxsw_status_label);

			nvgpu_writel(g, fifo_engine_status_r(engine_id), data);

			gm20b_read_engine_status_info(g, engine_id, &status);

			unit_assert(status.is_busy == expected.is_busy,
					goto done);
			unit_assert(status.is_faulted == expected.is_faulted,
					goto done);
			unit_assert(status.ctxsw_in_progress ==
					expected.ctxsw_in_progress, goto done);
			unit_assert(status.ctxsw_status ==
					expected.ctxsw_status, goto done);
			unit_assert(status.ctx_id ==
					expected.ctx_id, goto done);
			unit_assert(status.ctx_id_type ==
					expected.ctx_id_type, goto done);
			unit_assert(status.ctx_next_id ==
					expected.ctx_next_id, goto done);
			unit_assert(status.ctx_next_id_type ==
					expected.ctx_next_id_type, goto done);
		}
	}
	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
	}

	return ret;
}

struct unit_module_test nvgpu_engine_gm20b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(read_engine_status_info, test_gm20b_read_engine_status_info, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_engine_gm20b, nvgpu_engine_gm20b_tests, UNIT_PRIO_NVGPU_TEST);
