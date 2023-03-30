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

#include <nvgpu/posix/posix-fault-injection.h>

#include "hal/fifo/engine_status_gv100.h"

#include <nvgpu/hw/gv100/hw_fifo_gv100.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-engine-gv100.h"

//#define ENGINE_GV100_UNIT_DEBUG
#ifdef ENGINE_GV100_UNIT_DEBUG
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
#define pruned test_fifo_subtest_pruned

struct unit_ctx {
	struct unit_module *m;
	u32 engine_id;
};

struct unit_ctx unit_ctx;

int test_gv100_read_engine_status_info(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	u32 engine_id = nvgpu_engine_get_gr_id(g);
	struct nvgpu_engine_status_info status;
	int ret = UNIT_FAIL;

	/* gm20b_read_engine_status_info covered separately */

	nvgpu_writel(g, fifo_engine_status_r(engine_id), 0);
	gv100_read_engine_status_info(g, engine_id, &status);
	unit_assert(status.in_reload_status == false, goto done);

	nvgpu_writel(g, fifo_engine_status_r(engine_id), BIT(29));
	gv100_read_engine_status_info(g, engine_id, &status);
	unit_assert(status.in_reload_status == true, goto done);

	ret = UNIT_SUCCESS;
done:
	g->ops = gops;
	return ret;
}

#define F_ENGINE_DUMP_CTX_IS_TSG		BIT(0)
#define F_ENGINE_DUMP_NEXT_CTX_IS_TSG		BIT(1)
#define F_ENGINE_DUMP_IN_RELOAD_STATUS		BIT(2)
#define F_ENGINE_DUMP_IS_FAULTED		BIT(3)
#define F_ENGINE_DUMP_IS_BUSY			BIT(4)
#define F_ENGINE_DUMP_LAST			BIT(5)

static u32 stub_get_litter_value(struct gk20a *g, int value)
{
	/*
	 * Pretend there are as many engines as possible branches.
	 * Each engine_id will cover one combination of branches.
	 */
	return F_ENGINE_DUMP_LAST;
}

static u32 stub_get_litter_value_0(struct gk20a *g, int value)
{
	return 0;
}

static void stub_read_engine_status_info(struct gk20a *g, u32 engine_id,
		struct nvgpu_engine_status_info *status)
{
	u32 branches = engine_id;

	unit_verbose(unit_ctx.m, "engine_id=%u\n", engine_id);

	memset(status, 0, sizeof(*status));

	status->ctx_id_type = branches & F_ENGINE_DUMP_CTX_IS_TSG ?
		ENGINE_STATUS_CTX_ID_TYPE_TSGID : ENGINE_STATUS_CTX_ID_TYPE_CHID;

	status->ctx_next_id_type = branches & F_ENGINE_DUMP_NEXT_CTX_IS_TSG ?
		ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID : ENGINE_STATUS_CTX_NEXT_ID_TYPE_CHID;

	status->in_reload_status = branches & F_ENGINE_DUMP_IN_RELOAD_STATUS ?
		true : false;

	status->is_faulted = branches & F_ENGINE_DUMP_IS_FAULTED ?
		true : false;

	status->is_busy = branches & F_ENGINE_DUMP_IS_BUSY ?
		true : false;

	unit_ctx.engine_id = engine_id;
}

int test_gv100_dump_engine_status(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_debug_context o;
	int ret = UNIT_FAIL;
	u32 num_engines;

	unit_ctx.m = m;

	g->ops.get_litter_value = stub_get_litter_value;
	num_engines = g->ops.get_litter_value(g, GPU_LIT_HOST_NUM_ENGINES);
	unit_verbose(unit_ctx.m, "num_engines=%u\n", num_engines);

	g->ops.engine_status.read_engine_status_info = stub_read_engine_status_info;

	unit_ctx.engine_id = 0;
	gv100_dump_engine_status(g, &o);
	unit_assert(unit_ctx.engine_id == (num_engines - 1), goto done);

	unit_ctx.engine_id = (u32)~0;
	g->ops.get_litter_value = stub_get_litter_value_0;
	gv100_dump_engine_status(g, &o);
	unit_assert(unit_ctx.engine_id == (u32)~0, goto done);

	ret = UNIT_SUCCESS;
done:
	g->ops = gops;
	return ret;
}

struct unit_module_test nvgpu_engine_gv100_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(read_engine_status_info, test_gv100_read_engine_status_info, NULL, 0),
	UNIT_TEST(dump_engine_status_info, test_gv100_dump_engine_status, NULL, 1),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_engine_gv100, nvgpu_engine_gv100_tests, UNIT_PRIO_NVGPU_TEST);
