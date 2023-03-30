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
#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/dma.h>
#include <nvgpu/io.h>
#include <nvgpu/runlist.h>
#include <nvgpu/device.h>

#include "hal/init/hal_gv11b.h"
#include "nvgpu/hw/gk20a/hw_fifo_gk20a.h"

#include "../nvgpu-fifo-gv11b.h"
#include "../nvgpu-fifo-common.h"
#include "nvgpu-fifo.h"

#ifdef FIFO_UNIT_DEBUG
#define unit_verbose	unit_info
#else
#define unit_verbose(unit, msg, ...) \
	do { \
		if (0) { \
			unit_info(unit, msg, ##__VA_ARGS__); \
		} \
	} while (0)
#endif

#define pruned test_fifo_subtest_pruned

#define branches_str test_fifo_flags_str

#define get_log2 test_fifo_get_log2

static const char *f_fifo_decode_status[] = {
	"invalid",
	"valid",
	"NA",
	"NA",
	"NA",
	"load",
	"save",
	"switch",
	"NOT FOUND",
};

int test_decode_pbdma_ch_eng_status(struct unit_module *m, struct gk20a *g,
								void *args)
{
	const char *pbdma_ch_eng_status = NULL;
	u32 index;
	u32 max_index = ARRAY_SIZE(f_fifo_decode_status);
	int ret = UNIT_FAIL;

	for (index = 0U; index < max_index; index++) {
		pbdma_ch_eng_status =
				nvgpu_fifo_decode_pbdma_ch_eng_status(index);

		unit_assert(strcmp(pbdma_ch_eng_status,
				f_fifo_decode_status[index]) == 0, goto done);
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s index=%u\n", __func__, index);
	}

	return ret;
}

#define F_FIFO_SUSPEND_BAR1_SUPPORTED		BIT(0)
#define F_FIFO_SUSPEND_LAST			BIT(1)

static const char *f_fifo_suspend[] = {
	"fifo suspend bar1 not supported",
	"fifo suspend bar1 supported",
};

static void stub_fifo_bar1_snooping_disable(struct gk20a *g)
{
}

static bool stub_mm_is_bar1_supported(struct gk20a *g)
{
	return true;
}

int test_fifo_suspend(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gpu_ops gops = { {0} };
	u32 reg0_val, reg1_val;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	int err = 0U;
	u32 prune = F_FIFO_SUSPEND_BAR1_SUPPORTED;

	err = test_fifo_setup_gv11b_reg_space(m, g);
	unit_assert(err == 0, goto done);

	gv11b_init_hal(g);
	gops = g->ops;
	nvgpu_device_init(g);
	g->ops.fifo.bar1_snooping_disable = stub_fifo_bar1_snooping_disable;
	err = nvgpu_fifo_init_support(g);
	unit_assert(err == 0, goto done);

	for (branches = 0U; branches < F_FIFO_SUSPEND_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%u (pruned)\n", __func__,
				branches);
			continue;
		}
		unit_verbose(m, "%s branches=%u\n", __func__, branches);

		g->ops.mm.is_bar1_supported =
			(branches & F_FIFO_SUSPEND_BAR1_SUPPORTED) ?
				stub_mm_is_bar1_supported :
				gops.mm.is_bar1_supported;

		err = nvgpu_fifo_suspend(g);
		reg0_val = nvgpu_readl(g, fifo_intr_en_0_r());
		reg1_val = nvgpu_readl(g, fifo_intr_en_1_r());
		unit_assert(reg0_val == 0U, goto done);
		unit_assert(reg1_val == 0U, goto done);
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
		branches_str(branches, f_fifo_suspend));
	}

	nvgpu_fifo_cleanup_sw_common(g);
	g->ops = gops;
	return ret;
}

int test_fifo_sw_quiesce(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gpu_ops gops = { {0} };
	u32 reg_val;
	int ret = UNIT_FAIL;
	int err;
	u32 runlist_mask;

	err = test_fifo_setup_gv11b_reg_space(m, g);
	unit_assert(err == 0, goto done);

	gv11b_init_hal(g);
	gops = g->ops;
	nvgpu_device_init(g);
	err = nvgpu_fifo_init_support(g);
	unit_assert(err == 0, goto done);

	runlist_mask = nvgpu_runlist_get_runlists_mask(g, 0U,
		ID_TYPE_UNKNOWN, 0U, 0U);
	unit_assert(runlist_mask != 0U, goto done);
	nvgpu_fifo_sw_quiesce(g);
	reg_val = nvgpu_readl(g, fifo_sched_disable_r());
	unit_assert((reg_val & runlist_mask) == runlist_mask, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	nvgpu_fifo_cleanup_sw_common(g);
	g->ops = gops;
	return ret;
}

#define F_FIFO_SETUP_SW_READY			BIT(0)
#define F_FIFO_SETUP_SW_COMMON_CH_FAIL		BIT(1)
#define F_FIFO_SETUP_SW_COMMON_TSG_FAIL		BIT(2)
#define F_FIFO_SETUP_SW_COMMON_PBDMA_FAIL	BIT(3)
#define F_FIFO_SETUP_SW_COMMON_ENGINE_FAIL	BIT(4)
/*
 * NOTE: nvgpu_engine_setup_sw() consists of 2 memory allocations.
 * Selecting branch for nvgpu_runlist_setup_sw() fail case accordingly.
 */
#define F_FIFO_SETUP_SW_COMMON_ENGINE_FAIL2	BIT(5)
#define F_FIFO_SETUP_SW_COMMON_RUNLIST_FAIL	BIT(6)
/*
 * The fifo setup too contains another allocation.
 */
#define F_FIFO_SETUP_SW_COMMON_RUNLIST_FAIL2	BIT(7)
#define F_FIFO_SETUP_SW_PBDMA_NULL		BIT(8)
#define F_FIFO_CLEANUP_SW_PBDMA_NULL		BIT(9)
#define F_FIFO_SETUP_HW_PASS			BIT(10)
#define F_FIFO_SETUP_HW_FAIL			BIT(11)
#define F_FIFO_INIT_LAST			BIT(12)

static const char *f_fifo_init[] = {
	"fifo init sw ready",
	"channel setup sw fail",
	"tsg setup sw fail",
	"pbdma setup sw fail",
	"engine setup sw fail",
	"",
	"runlist setup sw fail",
	"runlist setup 2 sw fail",
	"pbdma setup sw NULL",
	"pbdma cleanup sw NULL",
	"fifo setup hw pass",
	"fifo setup hw fail",
};

static int stub_init_fifo_setup_hw_fail(struct gk20a *g)
{
	return -1;
}

static int stub_init_fifo_setup_hw_pass(struct gk20a *g)
{
	return 0;
}

int test_init_support(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_posix_fault_inj *kmem_fi;
	struct gpu_ops gops = { {0} };

	u32 branches = 0U;
	int ret = UNIT_FAIL;
	int err = 0U;
	u32 alloc_fail = F_FIFO_SETUP_SW_COMMON_CH_FAIL |
			F_FIFO_SETUP_SW_COMMON_TSG_FAIL |
			F_FIFO_SETUP_SW_COMMON_PBDMA_FAIL |
			F_FIFO_SETUP_SW_COMMON_ENGINE_FAIL |
			F_FIFO_SETUP_SW_COMMON_RUNLIST_FAIL;
	u32 fail = F_FIFO_SETUP_HW_FAIL | alloc_fail;
	u32 prune = F_FIFO_SETUP_SW_READY | F_FIFO_SETUP_SW_PBDMA_NULL |
			F_FIFO_SETUP_HW_PASS | fail;

	kmem_fi = nvgpu_kmem_get_fault_injection();

	err = test_fifo_setup_gv11b_reg_space(m, g);
	unit_assert(err == 0, goto done);

	nvgpu_device_init(g);

	gops = g->ops;

	for (branches = 0U; branches < F_FIFO_INIT_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%u (pruned)\n", __func__,
				branches);
			continue;
		}
		unit_verbose(m, "%s branches=%u\n", __func__, branches);

		if (branches & alloc_fail) {
			nvgpu_posix_enable_fault_injection(kmem_fi, true,
					(get_log2(branches) - 1U));
		}

		if (branches & F_FIFO_SETUP_SW_READY) {
			err = nvgpu_fifo_init_support(g);
			unit_assert(err == 0, goto done);
		}

		g->ops.fifo.init_fifo_setup_hw =
			(branches & F_FIFO_SETUP_HW_FAIL) ?
				stub_init_fifo_setup_hw_fail :
				(branches & F_FIFO_SETUP_HW_PASS) ?
					stub_init_fifo_setup_hw_pass : NULL;

		g->ops.pbdma.setup_sw =
			(branches & F_FIFO_SETUP_SW_PBDMA_NULL) ?
				NULL : gops.pbdma.setup_sw;

		g->ops.pbdma.cleanup_sw =
			(branches & (F_FIFO_CLEANUP_SW_PBDMA_NULL |
					F_FIFO_SETUP_SW_PBDMA_NULL)) ?
				NULL : gops.pbdma.cleanup_sw;

		err = nvgpu_fifo_init_support(g);
		if (branches & fail) {
			nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
			if (branches & F_FIFO_CLEANUP_SW_PBDMA_NULL) {
				gops.pbdma.cleanup_sw(g);
			}
			unit_assert(err != 0, goto done);
		} else {
			unit_assert(err == 0, goto done);
			nvgpu_fifo_cleanup_sw_common(g);
		}

	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
		branches_str(branches, f_fifo_init));
	}

	g->ops = gops;
	return ret;
}

struct unit_module_test nvgpu_fifo_tests[] = {

	UNIT_TEST(init, test_init_support, NULL, 0),
	UNIT_TEST(pbdma_ch_eng_status, test_decode_pbdma_ch_eng_status, NULL, 0),
	UNIT_TEST(fifo_suspend, test_fifo_suspend, NULL, 0),
	UNIT_TEST(fifo_sw_quiesce, test_fifo_sw_quiesce, NULL, 0),
};

UNIT_MODULE(nvgpu_fifo, nvgpu_fifo_tests, UNIT_PRIO_NVGPU_TEST);
