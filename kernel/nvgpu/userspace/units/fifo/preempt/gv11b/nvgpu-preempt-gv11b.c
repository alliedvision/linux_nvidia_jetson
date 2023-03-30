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

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <unit/io.h>
#include <unit/unit.h>
#include <nvgpu/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/fifo.h>
#include <nvgpu/runlist.h>
#include <nvgpu/preempt.h>
#include <nvgpu/soc.h>
#include <nvgpu/pbdma_status.h>
#include <nvgpu/hw/gv11b/hw_fifo_gv11b.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <os/posix/os_posix.h>

#include "hal/fifo/preempt_gv11b.h"

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-preempt-gv11b.h"

#ifdef PREEMPT_GV11B_UNIT_DEBUG
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

struct stub_ctx {
	bool result;
	struct nvgpu_pbdma_status_info pbdma_st;
	u32 eng_stat;
	u32 eng_intr_pending;
};

struct stub_ctx stub;

struct preempt_gv11b_unit_ctx {
	u32 branches;
};

static struct preempt_gv11b_unit_ctx unit_ctx;

#define F_PREEMPT_TRIGGER_TSG			BIT(0)
#define F_PREEMPT_TRIGGER_LAST			BIT(1)

static const char *f_preempt_trigger[] = {
	"preempt_trigger_tsg",
};

int test_gv11b_fifo_preempt_trigger(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	u32 branches = 0U;
	u32 orig_reg_val = nvgpu_readl(g, fifo_preempt_r());
	u32 expected_reg_val;

	for (branches = 0U; branches < F_PREEMPT_TRIGGER_LAST; branches++) {
		unit_verbose(m, "%s branches=%s\n",
			__func__, branches_str(branches, f_preempt_trigger));

		if (branches & F_PREEMPT_TRIGGER_TSG) {
			gv11b_fifo_preempt_trigger(g, 5U, ID_TYPE_TSG);
			expected_reg_val = fifo_preempt_id_f(5U) |
					fifo_preempt_type_tsg_f();
			unit_assert(expected_reg_val ==
				nvgpu_readl(g, fifo_preempt_r()), goto done);
			nvgpu_writel(g, fifo_preempt_r(), orig_reg_val);
		} else {
			gv11b_fifo_preempt_trigger(g, 5U, ID_TYPE_CHANNEL);
			unit_assert(orig_reg_val ==
				nvgpu_readl(g, fifo_preempt_r()), goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
				branches_str(branches, f_preempt_trigger));
	}

	return ret;
}

int test_gv11b_fifo_preempt_runlists_for_rc(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 runlist_mask;
	u32 reg_val;

	nvgpu_runlist_lock_active_runlists(g);
	runlist_mask = nvgpu_runlist_get_runlists_mask(g, 0U, ID_TYPE_UNKNOWN,
				0U, 0U);
	reg_val = nvgpu_readl(g, fifo_runlist_preempt_r());

	nvgpu_fifo_preempt_runlists_for_rc(g, runlist_mask);
	unit_assert(nvgpu_readl(g, fifo_runlist_preempt_r()) ==
			(reg_val | runlist_mask), goto done);

	nvgpu_runlist_unlock_active_runlists(g);
	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	return ret;
}

static void stub_pbdma_handle_intr(struct gk20a *g, u32 pbdma_id, bool recover)
{
}

static int stub_fifo_preempt_tsg(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	return 1;
}

#define F_PREEMPT_CHANNEL_TSGID_NULL		BIT(0)
#define F_PREEMPT_CHANNEL_LAST			BIT(1)

static const char *f_preempt_channel[] = {
	"channel_tsgid_null",
};

int test_gv11b_fifo_preempt_channel(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	int err = 0;
	u32 branches = 0U;
	struct nvgpu_channel ch;
	struct gpu_ops gops = g->ops;

	ch.g = g;
	g->ops.fifo.preempt_tsg = stub_fifo_preempt_tsg;

	for (branches = 0U; branches < F_PREEMPT_CHANNEL_LAST; branches++) {
		unit_verbose(m, "%s branches=%s\n",
			__func__, branches_str(branches, f_preempt_channel));

		ch.tsgid = branches & F_PREEMPT_CHANNEL_TSGID_NULL ?
				NVGPU_INVALID_TSG_ID : 0U;

		err = gv11b_fifo_preempt_channel(g, &ch);

		if (branches & F_PREEMPT_CHANNEL_TSGID_NULL) {
			unit_assert(err == 0, goto done);
		} else {
			unit_assert(err == 1, goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
				branches_str(branches, f_preempt_channel));
	}
	g->ops = gops;
	return ret;
}

static void stub_fifo_preempt_trigger(struct gk20a *g, u32 id,
			unsigned int id_type)
{

}

static int stub_fifo_is_preempt_pending_ebusy(struct gk20a *g, u32 id,
						unsigned int id_type,
						bool preempt_retries_left)
{
	return -EBUSY;
}

static int stub_fifo_is_preempt_pending_pass(struct gk20a *g, u32 id,
						unsigned int id_type,
						bool preempt_retries_left)
{
	return 0;
}

#define F_PREEMPT_TSG_RUNLIST_ID_INVALID		BIT(0)
#define F_PREEMPT_TSG_PREEMPT_LOCKED_FAIL		BIT(1)
#define F_PREEMPT_TSG_PLATFORM_SILICON			BIT(2)
#define F_PREEMPT_TSG_LAST				BIT(3)

static const char *f_preempt_tsg[] = {
	"runlist_id_invalid",
};

int test_gv11b_fifo_preempt_tsg(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	int err = 0;
	u32 branches = 0U;
	u32 prune = F_PREEMPT_TSG_RUNLIST_ID_INVALID;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct gpu_ops gops = g->ops;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	err = nvgpu_runlist_setup_sw(g);
	unit_assert(err == 0, goto done);

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, NVGPU_INVALID_RUNLIST_ID, false,
			getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	g->ops.fifo.preempt_trigger = stub_fifo_preempt_trigger;

	for (branches = 0U; branches < F_PREEMPT_TSG_LAST; branches++) {
		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, f_preempt_tsg));
			continue;
		}
		unit_verbose(m, "%s branches=%s\n",
			__func__, branches_str(branches, f_preempt_tsg));

		tsg->runlist = branches & F_PREEMPT_TSG_RUNLIST_ID_INVALID ?
				NULL : &g->fifo.active_runlists[0];

		g->ops.fifo.is_preempt_pending =
			branches & F_PREEMPT_TSG_PREEMPT_LOCKED_FAIL ?
			stub_fifo_is_preempt_pending_ebusy :
			stub_fifo_is_preempt_pending_pass;

		p->is_silicon =
			branches & F_PREEMPT_TSG_PLATFORM_SILICON ?
			true : false;

		err = EXPECT_BUG(nvgpu_fifo_preempt_tsg(g, tsg));

		if (branches & F_PREEMPT_TSG_PREEMPT_LOCKED_FAIL) {
			if (branches & F_PREEMPT_TSG_PLATFORM_SILICON) {
				unit_assert(err == 0, goto done);
			} else {
				unit_assert(err == 1, goto done);
			}
		} else {
			unit_assert(err == 0, goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
				branches_str(branches, f_preempt_tsg));
	}
	g->ops.fifo.is_preempt_pending =
			stub_fifo_is_preempt_pending_pass;
	err = nvgpu_tsg_unbind_channel(tsg, ch, true);
	if (err != 0) {
		unit_err(m, "Cannot unbind channel\n");
	}
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	p->is_silicon = false;
	return ret;
}

static bool stub_mc_is_stall_and_eng_intr_pending_true(struct gk20a *g,
		u32 act_eng_id, u32 *eng_intr_pending)
{
	*eng_intr_pending = stub.eng_intr_pending;
	return true;
}

static bool stub_mc_is_stall_and_eng_intr_pending_false(struct gk20a *g,
		u32 act_eng_id, u32 *eng_intr_pending)
{
	*eng_intr_pending = stub.eng_intr_pending;
	return false;
}

#define F_PREEMPT_PENDING_ID_TYPE_TSG			BIT(0)
#define F_PREEMPT_PENDING_POLL_PBDMA_FAIL		BIT(1)
#define F_PREEMPT_PENDING_PLATFORM_SILICON		BIT(2)
#define F_PREEMPT_PENDING_POLL_ENG_TIMEOUT_FAIL		BIT(3)
#define F_PREEMPT_PENDING_POLL_ENG_INTR_PENDING		BIT(4)
#define F_PREEMPT_PENDING_CTX_STAT_VALID		BIT(5)
#define F_PREEMPT_PENDING_CTX_STAT_SAVE			BIT(6)
#define F_PREEMPT_PENDING_CTX_STAT_LOAD			BIT(7)
#define F_PREEMPT_PENDING_CTX_STAT_SWITCH		BIT(8)
#define F_PREEMPT_PENDING_ENG_STATUS_ID_IS_EQUAL	BIT(9)
#define F_PREEMPT_PENDING_ENG_STATUS_NEXT_ID_IS_EQUAL	BIT(10)
#define F_PREEMPT_PENDING_ENG_INTR_PENDING0		BIT(11)
#define F_PREEMPT_PENDING_POLL_ENG_PRE_SI_RETRIES	BIT(12)
#define F_PREEMPT_PENDING_LAST				BIT(13)

static const char *f_preempt_pending[] = {
	"id_type_tsg",
	"poll_pbdma_fail",
	"platform_silicon",
	"poll_eng_timeout_init_fail",
	"eng_intr_pending_true",
	"ctx_stat_valid",
	"ctx_stat_save",
	"ctx_stat_load",
	"ctx_stat_switch",
	"eng_status_id_is_equal_given_id",
	"eng_status_next_id_is_equal_given_id",
	"eng_intr_pending_is_0",
};

int test_gv11b_fifo_is_preempt_pending(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	int err = 0;
	u32 branches = 0U;
	u32 prune = F_PREEMPT_PENDING_POLL_ENG_PRE_SI_RETRIES;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct gpu_ops gops = g->ops;
	unsigned int id_type;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	u32 ctx_stat = 0U;
	u32 id = 0U, next_id = 0U;

	err = nvgpu_runlist_setup_sw(g);
	unit_assert(err == 0, goto done);

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, NVGPU_INVALID_RUNLIST_ID, false,
			getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	g->ops.pbdma.handle_intr = stub_pbdma_handle_intr;

	for (branches = 0U; branches < F_PREEMPT_PENDING_LAST; branches++) {
		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, f_preempt_pending));
			continue;
		}
		unit_verbose(m, "%s branches=%s\n",
			__func__, branches_str(branches, f_preempt_pending));

		id_type = branches & F_PREEMPT_PENDING_ID_TYPE_TSG ?
				ID_TYPE_TSG : ID_TYPE_UNKNOWN;

		p->is_silicon =
			branches & F_PREEMPT_PENDING_PLATFORM_SILICON ?
			true : false;

		if (branches & F_PREEMPT_PENDING_POLL_PBDMA_FAIL) {
			/* TODO: make the poll loop time out */
		} else if (branches & F_PREEMPT_PENDING_POLL_ENG_TIMEOUT_FAIL) {
			/* TODO: make the poll loop time out */
		}

		/*
		 * Note: Force pbdma_status invalid to skip poll pbdma
		 *       which is tested separately.
		 */
		stub.pbdma_st.chsw_status = NVGPU_PBDMA_CHSW_STATUS_INVALID;

		if (branches & F_PREEMPT_PENDING_POLL_ENG_PRE_SI_RETRIES) {
			/* Force engine status = ctxsw_switch */
			branches |= F_PREEMPT_PENDING_CTX_STAT_SWITCH;
			/* Force eng_intr_pending = 0 */
			branches |= F_PREEMPT_PENDING_ENG_INTR_PENDING0;
		}

		g->ops.mc.is_stall_and_eng_intr_pending =
			branches & F_PREEMPT_PENDING_POLL_ENG_INTR_PENDING ?
			stub_mc_is_stall_and_eng_intr_pending_true :
			stub_mc_is_stall_and_eng_intr_pending_false;

		if (branches & F_PREEMPT_PENDING_CTX_STAT_SWITCH) {
			ctx_stat =
				fifo_engine_status_ctx_status_ctxsw_switch_v();
		} else if (branches & F_PREEMPT_PENDING_CTX_STAT_VALID) {
			ctx_stat = fifo_engine_status_ctx_status_valid_v();
		} else if (branches & F_PREEMPT_PENDING_CTX_STAT_SAVE) {
			ctx_stat = fifo_engine_status_ctx_status_ctxsw_save_v();
		} else if (branches & F_PREEMPT_PENDING_CTX_STAT_LOAD) {
			ctx_stat = fifo_engine_status_ctx_status_ctxsw_load_v();
		} else {
			ctx_stat = 0U;
		}

		id = branches & F_PREEMPT_PENDING_ENG_STATUS_ID_IS_EQUAL ?
				0U : 1U;

		next_id = branches &
				F_PREEMPT_PENDING_ENG_STATUS_NEXT_ID_IS_EQUAL ?
				0U : 1U;

		stub.eng_stat = ((ctx_stat & 0x7U) << 13) | (id & 0xfffU) |
				((next_id & 0xfffU) << 16);

		stub.eng_intr_pending =
				branches & F_PREEMPT_PENDING_ENG_INTR_PENDING0 ?
				0U : 1U;

		/* Modify eng_stat for engine 0 */
		nvgpu_writel(g, fifo_engine_status_r(0U), stub.eng_stat);

		err = gv11b_fifo_is_preempt_pending(g, 0U, id_type, false);

		if (branches & F_PREEMPT_PENDING_POLL_PBDMA_FAIL) {
			unit_assert(err == -ETIMEDOUT, goto done);
		} else if (branches & F_PREEMPT_PENDING_POLL_ENG_TIMEOUT_FAIL) {
			unit_assert(err == -ETIMEDOUT, goto done);
		} else if ((branches & F_PREEMPT_PENDING_CTX_STAT_SWITCH) &&
			(branches & F_PREEMPT_PENDING_ENG_INTR_PENDING0)) {
			unit_assert(err == -EBUSY, goto done);
		} else if ((branches & F_PREEMPT_PENDING_CTX_STAT_VALID) ||
			(branches & F_PREEMPT_PENDING_CTX_STAT_SAVE)) {
			if ((branches &
				F_PREEMPT_PENDING_ENG_STATUS_ID_IS_EQUAL) &&
				(branches &
					F_PREEMPT_PENDING_ENG_INTR_PENDING0)) {
				unit_assert(err == -EBUSY, goto done);
			} else {
				unit_assert(err == 0, goto done);
			}
		} else if (branches & F_PREEMPT_PENDING_CTX_STAT_LOAD) {
			if ((branches &
			F_PREEMPT_PENDING_ENG_STATUS_NEXT_ID_IS_EQUAL) &&
			(branches & F_PREEMPT_PENDING_ENG_INTR_PENDING0)) {
				unit_assert(err == -EBUSY, goto done);
			} else {
				unit_assert(err == 0, goto done);
			}
		} else {
			unit_assert(err == 0, goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
				branches_str(branches, f_preempt_pending));
	}
	g->ops.fifo.is_preempt_pending =
			stub_fifo_is_preempt_pending_pass;
	err = nvgpu_tsg_unbind_channel(tsg, ch, true);
	if (err != 0) {
		unit_err(m, "Cannot unbind channel\n");
	}
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	nvgpu_runlist_cleanup_sw(g);
	g->ops = gops;
	p->is_silicon = false;
	return ret;
}

struct unit_module_test nvgpu_preempt_gv11b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, &unit_ctx, 0),
	UNIT_TEST(preempt_trigger, test_gv11b_fifo_preempt_trigger, NULL, 0),
	UNIT_TEST(preempt_runlists_for_rc, test_gv11b_fifo_preempt_runlists_for_rc, NULL, 0),
	UNIT_TEST(preempt_channel, test_gv11b_fifo_preempt_channel, NULL, 0),
	UNIT_TEST(preempt_tsg, test_gv11b_fifo_preempt_tsg, NULL, 0),
	UNIT_TEST(is_preempt_pending, test_gv11b_fifo_is_preempt_pending, NULL, 2),
	UNIT_TEST(remove_support, test_fifo_remove_support, &unit_ctx, 0),
};

UNIT_MODULE(nvgpu_preempt_gv11b, nvgpu_preempt_gv11b_tests, UNIT_PRIO_NVGPU_TEST);
