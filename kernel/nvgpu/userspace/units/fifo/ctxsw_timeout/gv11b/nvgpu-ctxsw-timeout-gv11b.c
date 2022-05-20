/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/posix/io.h>

#include <nvgpu/soc.h>
#include <nvgpu/tsg.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/hw/gv11b/hw_fifo_gv11b.h>

#include "hal/init/hal_gv11b.h"

#include "os/posix/os_posix.h"

#include <nvgpu/posix/posix-fault-injection.h>

#include <hal/fifo/ctxsw_timeout_gv11b.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-ctxsw-timeout-gv11b.h"

#ifdef CTXSW_TIMEOUT_GV11B_UNIT_DEBUG
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
	u32 timeout_info;

};
struct unit_ctx u;

#define F_CTXSW_TIMEOUT_ENABLE				BIT(0)
#define F_CTXSW_TIMEOUT_PLATFORM_SILICON		BIT(1)
#define F_CTXSW_TIMEOUT_ENABLE_LAST			BIT(2)

static const char *f_ctxsw_timeout_enable[] = {
	"timeout_enable",
	"platform_is_silicon",
};

int test_gv11b_fifo_ctxsw_timeout_enable(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 branches;
	bool enable;
	u32 timeout;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	g->ptimer_src_freq = PTIMER_REF_FREQ_HZ;
	g->ctxsw_timeout_period_ms = 100U;

	for (branches = 0; branches < F_CTXSW_TIMEOUT_ENABLE_LAST; branches++) {

		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_ctxsw_timeout_enable));

		enable = branches & F_CTXSW_TIMEOUT_ENABLE ? true : false;
		p->is_silicon = branches & F_CTXSW_TIMEOUT_PLATFORM_SILICON ?
							true : false;

		gv11b_fifo_ctxsw_timeout_enable(g, enable);

		timeout = nvgpu_readl(g, fifo_eng_ctxsw_timeout_r());

		if (!(branches & F_CTXSW_TIMEOUT_ENABLE)) {
			unit_assert((timeout &
				fifo_eng_ctxsw_timeout_detection_m()) ==
				fifo_eng_ctxsw_timeout_detection_disabled_f(),
				goto done);
		} else if (branches & F_CTXSW_TIMEOUT_PLATFORM_SILICON) {
			unit_assert((timeout &
				fifo_eng_ctxsw_timeout_detection_m()) ==
				fifo_eng_ctxsw_timeout_detection_enabled_f(),
				goto done);

			timeout &= ~fifo_eng_ctxsw_timeout_detection_m();

			unit_assert(timeout == (g->ctxsw_timeout_period_ms *
							1000U), goto done);
		} else {
			unit_assert((timeout &
				fifo_eng_ctxsw_timeout_period_m()) ==
				fifo_eng_ctxsw_timeout_period_max_f(),
				goto done);

			unit_assert((timeout &
				fifo_eng_ctxsw_timeout_detection_m()) ==
				fifo_eng_ctxsw_timeout_detection_disabled_f(),
				goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_ctxsw_timeout_enable));
	}
	g->ptimer_src_freq = 0U;
	g->ctxsw_timeout_period_ms = 0U;
	return ret;
}

static void writel_access_reg_fn(struct gk20a *g,
	struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

static void readl_access_reg_fn(struct gk20a *g,
		struct nvgpu_reg_access *access)
{
	if (access->addr == fifo_intr_ctxsw_timeout_info_r(1U)) {
		access->value = u.timeout_info;
	} else {
		access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
	}
}

#define F_ENG_TIMEDOUT_NONE			BIT(0)
#define F_CTX_STATUS_0				BIT(1)
#define F_CTX_STATUS_1				BIT(2)
#define F_CTX_STATUS_3				BIT(3)
#define F_TSGID_INVALID				BIT(4)
#define F_INFO_STATUS_2				BIT(5)
#define F_INFO_STATUS_3				BIT(6)
#define F_HANDLE_CTXSW_TIMEOUT_ENABLE_LAST	BIT(7)

static const char *f_handle_timeout_enable[] = {
	"no_engines_timeout_pending",
	"ctx_status_is_0",
	"ctx_status_is_1",
	"ctx_status_is_3",
	"tsgid_is_invalid",
	"info_status_is_2",
	"info_status_is_3",
};

int test_gv11b_fifo_handle_ctxsw_timeout(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 branches;
	u32 prune = F_ENG_TIMEDOUT_NONE | F_CTX_STATUS_3;
	bool ret_bool;
	u32 ctxsw_timeout_engine_orig = nvgpu_readl(g,
					fifo_intr_ctxsw_timeout_r());
	u32 ctxsw_timeout_engine = 0U;
	u32 timeout_info = 0U, ctx_status = 0U, tsgid = 0U, info_status = 0U;
	struct nvgpu_posix_io_callbacks *old_io;
	struct nvgpu_posix_io_callbacks new_io = {
		.readl = readl_access_reg_fn,
		.writel = writel_access_reg_fn
	};

	old_io = nvgpu_posix_register_io(g, &new_io);
	u.m = m;

	for (branches = 0; branches < F_HANDLE_CTXSW_TIMEOUT_ENABLE_LAST;
								branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches,
						f_handle_timeout_enable));
			continue;
		}

		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_handle_timeout_enable));

		/* Set pending timeout engines to be 0 or all except eng_id0 */
		ctxsw_timeout_engine = branches & F_ENG_TIMEDOUT_NONE ?
				0U : 0xff012ffeU;
		nvgpu_writel(g, fifo_intr_ctxsw_timeout_r(),
				ctxsw_timeout_engine);

		ctx_status = branches & F_CTX_STATUS_0 ? 0U :
				branches & F_CTX_STATUS_1 ? 1U : 2U;
		ctx_status = branches & F_CTX_STATUS_3 ? 3U : ctx_status;
		tsgid = branches & F_TSGID_INVALID ? NVGPU_INVALID_TSG_ID : 0U;
		info_status = branches & F_INFO_STATUS_2 ? 2U :
				branches & F_INFO_STATUS_3 ? 3U : 1U;

		timeout_info = ((ctx_status & 0x3U) << 14U) |
				((info_status & 0x3U) << 30U);

		if (ctx_status ==
			fifo_intr_ctxsw_timeout_info_ctxsw_state_load_v()) {
			timeout_info |= ((tsgid & 0x3fffU) << 16U);
		} else {
			timeout_info |= (tsgid & 0x3fffU);
		}

		u.timeout_info = timeout_info;

		ret_bool = gv11b_fifo_handle_ctxsw_timeout(g);

		unit_assert(ret_bool == false, goto done);
		unit_assert(ctxsw_timeout_engine == nvgpu_readl(g,
				fifo_intr_ctxsw_timeout_r()), goto done);

		nvgpu_writel(g, fifo_intr_ctxsw_timeout_r(),
				ctxsw_timeout_engine_orig);
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_handle_timeout_enable));
	}
	(void) nvgpu_posix_register_io(g, old_io);

	return ret;
}

struct unit_module_test nvgpu_ctxsw_timeout_gv11b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(ctxsw_timeout_enable, test_gv11b_fifo_ctxsw_timeout_enable, NULL, 0),
	UNIT_TEST(handle_ctxsw_timeout, test_gv11b_fifo_handle_ctxsw_timeout, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_ctxsw_timeout_gv11b, nvgpu_ctxsw_timeout_gv11b_tests, UNIT_PRIO_NVGPU_TEST);
