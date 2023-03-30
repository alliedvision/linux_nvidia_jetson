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
#include <nvgpu/channel_sync.h>
#include <nvgpu/dma.h>
#include <nvgpu/engines.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/tsg.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/runlist.h>

#include "hal/init/hal_gv11b.h"

#include <nvgpu/posix/posix-fault-injection.h>

#include "nvgpu/hw/gv11b/hw_top_gv11b.h"

#include "../nvgpu-fifo-common.h"
#include "../nvgpu-fifo-gv11b.h"
#include "nvgpu-engine.h"
#include "nvgpu-engine-status.h"

#define ENGINE_UNIT_DEBUG
#ifdef ENGINE_UNIT_DEBUG
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

int test_engine_setup_sw(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_init_info(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_ids(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_is_valid_runlist_id(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_get_fast_ce_runlist_id(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_get_gr_runlist_id(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_get_active_eng_info(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_interrupt_mask(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_mmu_fault_id(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_mmu_fault_id_veid(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_get_mask_on_id(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_find_busy_doing_ctxsw(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

int test_engine_get_runlist_busy_engines(struct unit_module *m,
		struct gk20a *g, void *args)
{
	return UNIT_FAIL;
}

struct unit_module_test nvgpu_engine_tests[] = {
	UNIT_TEST(setup_sw,                 test_engine_setup_sw,                 NULL, 2),
	UNIT_TEST(init_support,             test_fifo_init_support,               NULL, 2),
	UNIT_TEST(init_info,                test_engine_init_info,                NULL, 2),
	UNIT_TEST(ids,                      test_engine_ids,                      NULL, 2),
	UNIT_TEST(get_active_eng_info,      test_engine_get_active_eng_info,      NULL, 2),
	UNIT_TEST(interrupt_mask,           test_engine_interrupt_mask,           NULL, 2),
	UNIT_TEST(get_fast_ce_runlist_id,   test_engine_get_fast_ce_runlist_id,   NULL, 2),
	UNIT_TEST(get_gr_runlist_id,        test_engine_get_gr_runlist_id,        NULL, 2),
	UNIT_TEST(is_valid_runlist_id,      test_engine_is_valid_runlist_id,      NULL, 2),
	UNIT_TEST(mmu_fault_id,             test_engine_mmu_fault_id,             NULL, 2),
	UNIT_TEST(mmu_fault_id_veid,        test_engine_mmu_fault_id_veid,        NULL, 2),
	UNIT_TEST(get_mask_on_id,           test_engine_get_mask_on_id,           NULL, 2),
	UNIT_TEST(status,                   test_engine_status,                   NULL, 2),
	UNIT_TEST(find_busy_doing_ctxsw,    test_engine_find_busy_doing_ctxsw,    NULL, 2),
	UNIT_TEST(get_runlist_busy_engines, test_engine_get_runlist_busy_engines, NULL, 2),
	UNIT_TEST(remove_support,           test_fifo_remove_support,             NULL, 2),
};

UNIT_MODULE(nvgpu_engine, nvgpu_engine_tests, UNIT_PRIO_NVGPU_TEST);
