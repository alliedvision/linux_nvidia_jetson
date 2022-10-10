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
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <unit/io.h>
#include <unit/unit.h>
#include <unit/utils.h>

#include <nvgpu/channel.h>
#include <nvgpu/channel_sync.h>
#include <nvgpu/dma.h>
#include <nvgpu/engines.h>
#include <nvgpu/tsg.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/runlist.h>
#include <nvgpu/debug.h>
#include <nvgpu/thread.h>
#include <nvgpu/channel_user_syncpt.h>

#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/posix-nvhost.h>

#include "../nvgpu-fifo-common.h"
#include "nvgpu-channel.h"

#define MAX_STUB	2

struct stub_ctx {
	u32 count;
	u32 chid;
	u32 tsgid;
};

struct stub_ctx stub[MAX_STUB];

struct channel_unit_ctx {
	u32 branches;
	struct stub_ctx stub[MAX_STUB];
};

static struct channel_unit_ctx unit_ctx;

static void subtest_setup(u32 branches)
{
	u32 i;

	unit_ctx.branches = branches;
	memset(stub, 0, sizeof(stub));
	for (i = 0; i < MAX_STUB; i++) {
		stub[i].chid = NVGPU_INVALID_CHANNEL_ID;
	}
}

#define subtest_pruned	test_fifo_subtest_pruned
#define branches_str	test_fifo_flags_str

#define F_CHANNEL_SETUP_SW_VZALLOC_FAIL				BIT(0)
#define F_CHANNEL_SETUP_SW_REF_COND_FAIL			BIT(1)
#define F_CHANNEL_SETUP_SW_LAST					BIT(2)

static const char *f_channel_setup_sw[] = {
	"vzalloc_fail",
	"cond_init failure"
};

static u32 stub_channel_count(struct gk20a *g)
{
	return 32;
}

int test_channel_setup_sw(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_fifo *f = &g->fifo;
	struct nvgpu_posix_fault_inj *kmem_fi;
	struct nvgpu_posix_fault_inj *l_cond_fi;
	u32 branches;
	int ret = UNIT_FAIL;
	int err;
	u32 fail = F_CHANNEL_SETUP_SW_VZALLOC_FAIL |
			F_CHANNEL_SETUP_SW_REF_COND_FAIL;

	u32 prune = fail;

	kmem_fi = nvgpu_kmem_get_fault_injection();
	l_cond_fi = nvgpu_cond_get_fault_injection();

	g->ops.channel.count = stub_channel_count;

	for (branches = 0U; branches < F_CHANNEL_SETUP_SW_LAST; branches++) {

		if (subtest_pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n",
				__func__,
				branches_str(branches, f_channel_setup_sw));
			continue;
		}
		subtest_setup(branches);

		nvgpu_posix_enable_fault_injection(kmem_fi,
			branches & F_CHANNEL_SETUP_SW_VZALLOC_FAIL ?
			true : false, 0);

		/* Insert condition fault after some channels are initialized */
		if ((branches & F_CHANNEL_SETUP_SW_REF_COND_FAIL) != 0U) {
			nvgpu_posix_enable_fault_injection(l_cond_fi, true, 5);
		}

		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_setup_sw));

		err = nvgpu_channel_setup_sw(g);

		if (branches & fail) {
			nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
			nvgpu_posix_enable_fault_injection(l_cond_fi, false, 0);
			unit_assert(err != 0, goto done);
			unit_assert(f->channel == NULL, goto done);
		} else {
			unit_assert(err == 0, goto done);
			nvgpu_channel_cleanup_sw(g);
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_setup_sw));
	}
	g->ops = gops;
	return ret;
}

#define F_CHANNEL_OPEN_ENGINE_NOT_VALID		BIT(0)
#define F_CHANNEL_OPEN_PRIVILEGED		BIT(1)
#define F_CHANNEL_OPEN_ALLOC_CH_FAIL		BIT(2)
#define F_CHANNEL_OPEN_ALLOC_CH_WARN0		BIT(3)
#define F_CHANNEL_OPEN_ALLOC_CH_WARN1		BIT(4)
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
#define F_CHANNEL_OPEN_ALLOC_CH_AGGRESSIVE	BIT(5)
#define F_CHANNEL_OPEN_BUG_ON			BIT(6)
#define F_CHANNEL_OPEN_ALLOC_INST_FAIL		BIT(7)
#define F_CHANNEL_OPEN_NOTIFIER_WQ_INIT_FAIL	BIT(8)
#define F_CHANNEL_OPEN_SEMAPHORE_WQ_INIT_FAIL	BIT(9)
#define F_CHANNEL_OPEN_LAST			BIT(10)
#else
#define F_CHANNEL_OPEN_BUG_ON			BIT(5)
#define F_CHANNEL_OPEN_ALLOC_INST_FAIL		BIT(6)
#define F_CHANNEL_OPEN_NOTIFIER_WQ_INIT_FAIL	BIT(7)
#define F_CHANNEL_OPEN_SEMAPHORE_WQ_INIT_FAIL	BIT(8)
#define F_CHANNEL_OPEN_LAST			BIT(9)
#endif


static const char *f_channel_open[] = {
	"engine_not_valid",
	"privileged",
	"alloc_ch_fail",
	"alloc_ch_warn0",
	"alloc_ch_warn1",
	"aggressive_destroy",
	"bug_on",
	"alloc_inst_fail",
	"notifier_wq_init_fail",
	"semaphore_wq_init_fail",
};

static int stub_channel_alloc_inst_ENOMEM(struct gk20a *g,
		struct nvgpu_channel *ch)
{
	return -ENOMEM;
}

static int test_channel_open_bvec(struct unit_module *m,
		struct gk20a *g, void *vargs, bool priviledged)
{
	struct nvgpu_channel *ch = NULL;
	int ret = UNIT_FAIL;
	u32 gr_runlist_id = nvgpu_engine_get_gr_runlist_id(g);
	u32 valid_runlist_ids[][2] = {{0, 1}};
	u32 invalid_runlist_ids[][2] = {{2, U32_MAX}};
	u32 runlist_id, runlist_id_range;
	u32 (*working_list)[2];
	/*
	 * i is to loop through valid and invalid cases
	 * j is to loop through different ranges within ith case
	 * states is for min, max and median
	 */
	u32 i, j, states;
	const char *string_cases[] = {"Valid", "Invalid"};
	const char *string_states[] = {"Min", "Max", "Mid"};
	u32 runlist_range_difference;

	/* loop through valid and invalid cases */
	for (i = 0; i < 2; i++) {
		/* select appropriate iteration size */
		runlist_id_range = (i == 0) ? ARRAY_SIZE(valid_runlist_ids) : ARRAY_SIZE(invalid_runlist_ids);
		/* select correct working list */
		working_list =  (i == 0) ? valid_runlist_ids : invalid_runlist_ids;
		for (j = 0; j < runlist_id_range; j++) {
			for (states = 0; states < 3; states++) {
				/* check for min runlist id */
				if (states == 0)
					runlist_id = working_list[j][0];
				else if (states == 1) {
					/* check for max valid runlist id */
					runlist_id = working_list[j][1];
				} else {
					runlist_range_difference = working_list[j][1] - working_list[j][0];
					/* Check for random runlist id in range */
					if (runlist_range_difference > 1)
						runlist_id = get_random_u32(working_list[j][0] + 1, working_list[j][1] - 1);
					else
						continue;
				}
				unit_info(m, "BVEC testing for nvgpu_channel_open_new with runlist id = 0x%08x(%s range [0x%08x - 0x%08x]  %s)\n", runlist_id, string_cases[i], working_list[j][0], working_list[j][1], string_states[states]);

				ch = nvgpu_channel_open_new(g, runlist_id, priviledged, getpid(), getpid());
				if (i == 0)
					unit_assert(ch != NULL && ch->runlist->id == runlist_id, goto done);
				else
					unit_assert(ch != NULL && ch->runlist->id == gr_runlist_id, goto done);

				/* Clearing for success cases */
				if (ch != NULL) {
					nvgpu_channel_close(ch);
					ch = NULL;
				}
			}
		}
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
		if (ch != NULL) {
			nvgpu_channel_close(ch);
		}
	}

	return ret;
}

int test_channel_open(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct nvgpu_fifo *f = &g->fifo;
	struct nvgpu_fifo fifo = g->fifo;
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel *ch, *next_ch;
	struct nvgpu_posix_fault_inj *l_cond_fi;
	u32 branches;
	int ret = UNIT_FAIL;
	u32 fail =
		F_CHANNEL_OPEN_ALLOC_CH_FAIL |
		F_CHANNEL_OPEN_BUG_ON |
		F_CHANNEL_OPEN_ALLOC_INST_FAIL |
		F_CHANNEL_OPEN_NOTIFIER_WQ_INIT_FAIL |
		F_CHANNEL_OPEN_SEMAPHORE_WQ_INIT_FAIL;
	u32 prune = fail |
		F_CHANNEL_OPEN_ALLOC_CH_WARN0 |
		F_CHANNEL_OPEN_ALLOC_CH_WARN1;
	u32 runlist_id;
	bool privileged;
	int err;
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	void (*os_channel_open)(struct nvgpu_channel *ch) =
		g->os_channel.open;
#endif

	l_cond_fi = nvgpu_cond_get_fault_injection();

	for (branches = 0U; branches < F_CHANNEL_OPEN_LAST; branches++) {

		if (subtest_pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, f_channel_open));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_open));

		next_ch =
			nvgpu_list_empty(&f->free_chs) ? NULL :
			nvgpu_list_first_entry(&f->free_chs,
				nvgpu_channel, free_chs);
		unit_assert(next_ch != NULL, goto done);

		runlist_id =
			branches & F_CHANNEL_OPEN_ENGINE_NOT_VALID ?
			NVGPU_INVALID_RUNLIST_ID : NVGPU_ENGINE_GR;

		privileged =
			branches & F_CHANNEL_OPEN_PRIVILEGED ?
			true : false;

		if (branches & F_CHANNEL_OPEN_ALLOC_CH_FAIL) {
			nvgpu_init_list_node(&f->free_chs);
		}

		if (branches & F_CHANNEL_OPEN_ALLOC_CH_WARN0) {
			nvgpu_atomic_inc(&next_ch->ref_count);
		}

		if (branches & F_CHANNEL_OPEN_ALLOC_CH_WARN1) {
			next_ch->referenceable = false;
		}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		if (branches & F_CHANNEL_OPEN_ALLOC_CH_AGGRESSIVE) {
			g->aggressive_sync_destroy_thresh += 1U;
			f->used_channels += 2U;
		}
#endif

		if (branches & F_CHANNEL_OPEN_NOTIFIER_WQ_INIT_FAIL) {
			nvgpu_posix_enable_fault_injection(l_cond_fi, true, 0);
		}

		if (branches & F_CHANNEL_OPEN_SEMAPHORE_WQ_INIT_FAIL) {
			nvgpu_posix_enable_fault_injection(l_cond_fi, true, 1);
		}

		g->ops.channel.alloc_inst =
			branches & F_CHANNEL_OPEN_ALLOC_INST_FAIL ?
			stub_channel_alloc_inst_ENOMEM :
			gops.channel.alloc_inst;

		if (branches & F_CHANNEL_OPEN_BUG_ON) {
			next_ch->g = (void *)1;
		}

		err = EXPECT_BUG(
			ch = nvgpu_channel_open_new(g, runlist_id,
				privileged, getpid(), getpid());
		);

		if (branches & F_CHANNEL_OPEN_BUG_ON) {
			next_ch->g = NULL;
			unit_assert(err != 0, goto done);
			/* add to head to increase visibility of timing-related bugs */
			nvgpu_list_add(&next_ch->free_chs, &f->free_chs);
			f->used_channels -= 1U;
		} else {
			unit_assert(err == 0, goto done);
		};

		if (branches & F_CHANNEL_OPEN_ALLOC_CH_WARN1) {
			next_ch->referenceable = true;
		}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		if (branches & F_CHANNEL_OPEN_ALLOC_CH_AGGRESSIVE) {
			g->aggressive_sync_destroy_thresh -= 1U;
			f->used_channels -= 2U;
			unit_assert(g->aggressive_sync_destroy, goto done);
			g->aggressive_sync_destroy = false;
		}
#endif

		if (branches & fail) {
			nvgpu_posix_enable_fault_injection(l_cond_fi, false, 0);
			if (branches & F_CHANNEL_OPEN_ALLOC_CH_FAIL) {
				f->free_chs = fifo.free_chs;
			}

			if (branches & F_CHANNEL_OPEN_ALLOC_CH_WARN0) {
				nvgpu_atomic_dec(&ch->ref_count);
			}
			unit_assert(ch == NULL, goto done);
		} else {
			unit_assert(ch != NULL, goto done);
			unit_assert(ch->g == g, goto done);
			unit_assert(nvgpu_list_empty(&ch->free_chs), goto done);

			nvgpu_channel_close(ch);
			ch = NULL;

			err = test_channel_open_bvec(m, g, vargs, privileged);
			unit_assert(err == 0, goto done);
		}
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_open));
	}

	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	g->ops = gops;
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	g->os_channel.open = os_channel_open;
#endif
	return ret;
}

#define F_CHANNEL_CLOSE_ALREADY_FREED			BIT(0)
#define F_CHANNEL_CLOSE_FORCE				BIT(1)
#define F_CHANNEL_CLOSE_DYING				BIT(2)
#define F_CHANNEL_CLOSE_TSG_BOUND			BIT(3)
#define F_CHANNEL_CLOSE_TSG_UNBIND_FAIL			BIT(4)
#define F_CHANNEL_CLOSE_OS_CLOSE			BIT(5)
#define F_CHANNEL_CLOSE_NON_REFERENCEABLE		BIT(6)
#define F_CHANNEL_CLOSE_FREE_SUBCTX			BIT(7)
#define F_CHANNEL_CLOSE_USER_SYNC			BIT(8)
#define F_CHANNEL_CLOSE_NONZERO_DESTROY_THRESH_64	BIT(9)
#define F_CHANNEL_CLOSE_NONZERO_DESTROY_THRESH_1	BIT(10)
#define F_CHANNEL_CLOSE_DETERMINISTIC			BIT(11)
#define F_CHANNEL_CLOSE_DETERMINISTIC_RAILGATE_ALLOWED	BIT(12)
#define F_CHANNEL_WAIT_UNTIL_COUNTER			BIT(13)
#define F_CHANNEL_CLOSE_AS_BOUND			BIT(14)
#define F_CHANNEL_CLOSE_LAST				BIT(15)

static const char *f_channel_close[] = {
	"already_freed",
	"force",
	"dying",
	"tsg_bound",
	"tsg_unbind_fail",
	"os_close",
	"non_referenceable",
	"free_subctx",
	"user_sync",
	"destroy_thresh_64",
	"destroy_thresh_1",
	"deterministic",
	"deterministic_railgate_allowed",
	"as_bound",
};

static int thread_reset_function(void *arg)
{
	struct nvgpu_channel *ch = (struct nvgpu_channel *)arg;

	sleep(1);
	nvgpu_atomic_set(&ch->ref_count, 1);

	return 0;
}

static void stub_os_channel_close(struct nvgpu_channel *ch, bool force)
{
	stub[0].chid = ch->chid;
}

static void stub_gr_intr_flush_channel_tlb(struct gk20a *g)
{
}

static bool channel_close_pruned(u32 branches, u32 final)
{
	u32 branches_init = branches;

	if (subtest_pruned(branches, final)) {
		return true;
	}

	branches &= ~F_CHANNEL_CLOSE_TSG_UNBIND_FAIL;


	if ((branches & F_CHANNEL_CLOSE_AS_BOUND) == 0) {
		branches &= ~F_CHANNEL_CLOSE_FREE_SUBCTX;
	}

	if (branches < branches_init) {
		return true;
	}

	return false;
}

int test_channel_close(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	u32 fail = F_CHANNEL_CLOSE_ALREADY_FREED |
		   F_CHANNEL_CLOSE_NON_REFERENCEABLE;
	u32 prune = (u32)(F_CHANNEL_CLOSE_USER_SYNC) |
			F_CHANNEL_CLOSE_DETERMINISTIC_RAILGATE_ALLOWED |
			F_CHANNEL_WAIT_UNTIL_COUNTER | fail;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	void (*os_channel_close)(struct nvgpu_channel *ch, bool force) =
		g->os_channel.close;
	bool privileged = false;
	bool force;
	int err = 0;
	struct mm_gk20a mm;
	struct vm_gk20a vm;
	struct nvgpu_thread thread_reset;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	g->ops.gr.intr.flush_channel_tlb = stub_gr_intr_flush_channel_tlb;

	for (branches = 0U; branches < F_CHANNEL_CLOSE_LAST; branches++) {

		if (channel_close_pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, f_channel_close));
			continue;
		}
		subtest_setup(branches);

		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_close));

		ch = nvgpu_channel_open_new(g, runlist_id,
				privileged, getpid(), getpid());
		unit_assert(ch != NULL, goto done);

		ch->usermode_submit_enabled = true;

		force = branches & F_CHANNEL_CLOSE_FORCE ? true : false;

		nvgpu_set_enabled(g, NVGPU_DRIVER_IS_DYING,
			branches & F_CHANNEL_CLOSE_DYING ?  true : false);

		g->os_channel.close = branches & F_CHANNEL_CLOSE_OS_CLOSE ?
			stub_os_channel_close : NULL;

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		g->aggressive_sync_destroy_thresh =
			branches & F_CHANNEL_CLOSE_NONZERO_DESTROY_THRESH_64 ?
			64U :
			(branches & F_CHANNEL_CLOSE_NONZERO_DESTROY_THRESH_1) ?
			1U : 0U;
#endif

		if (branches & F_CHANNEL_CLOSE_TSG_BOUND) {
			err = nvgpu_tsg_bind_channel(tsg, ch);
			unit_assert(err == 0, goto done);
		}

		ch->referenceable =
			branches & F_CHANNEL_CLOSE_NON_REFERENCEABLE ?
			false : true;

		if (branches & F_CHANNEL_CLOSE_AS_BOUND) {
			memset(&mm, 0, sizeof(mm));
			memset(&vm, 0, sizeof(vm));
			mm.g = g;
			vm.mm = &mm;
			ch->vm = &vm;
			nvgpu_ref_init(&vm.ref);
			nvgpu_ref_get(&vm.ref);
		} else {
			ch->vm = NULL;
		}

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
		if (branches & F_CHANNEL_CLOSE_DETERMINISTIC) {
			/* Compensate for atomic dec in gk20a_idle() */
			nvgpu_atomic_set(&g->usage_count, 1);
			ch->deterministic = true;
		}

		if (branches & F_CHANNEL_CLOSE_DETERMINISTIC_RAILGATE_ALLOWED) {
			ch->deterministic = true;
			ch->deterministic_railgate_allowed = true;
		}
#endif

		g->ops.gr.setup.free_subctx =
			branches & F_CHANNEL_CLOSE_FREE_SUBCTX ?
			gops.gr.setup.free_subctx : NULL;

		if (branches & F_CHANNEL_CLOSE_USER_SYNC) {
			/* TODO: stub ch->user_sync */
		}

		if (branches & F_CHANNEL_WAIT_UNTIL_COUNTER) {
			nvgpu_atomic_set(&ch->ref_count, 2);
			err = nvgpu_thread_create(&thread_reset, (void *)ch,
				&thread_reset_function, "reset_thread");
			unit_assert(err == 0, goto done);
		}

		if (branches & F_CHANNEL_CLOSE_ALREADY_FREED) {
			nvgpu_channel_close(ch);
		}

		if (force) {
			err = EXPECT_BUG(nvgpu_channel_kill(ch));
		} else {
			err = EXPECT_BUG(nvgpu_channel_close(ch));
		}

		if (branches & F_CHANNEL_WAIT_UNTIL_COUNTER) {
			nvgpu_thread_join(&thread_reset);
		}

		if (branches & F_CHANNEL_CLOSE_ALREADY_FREED) {
			unit_assert(err != 0, goto done);
			unit_assert(ch->g == NULL, goto done);
			continue;
		}

		if ((branches & F_CHANNEL_CLOSE_USER_SYNC) != 0U) {
			unit_assert(ch->user_sync == NULL, goto done);
		}

		if (branches & fail) {
			unit_assert(ch->g != NULL, goto done);
			unit_assert(nvgpu_list_empty(&ch->free_chs), goto done);

			if (branches & F_CHANNEL_CLOSE_ALREADY_FREED) {
				continue;
			}
			ch->referenceable = true;
			nvgpu_channel_kill(ch);
			continue;
		}

		if (branches & F_CHANNEL_CLOSE_DYING) {
			/* when driver is dying, tsg unbind is skipped */
			nvgpu_init_list_node(&tsg->ch_list);
			nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
		} else {
			unit_assert(!nvgpu_list_empty(&ch->free_chs),
				goto done);
			unit_assert(nvgpu_list_empty(&tsg->ch_list), goto done);
		}

		if (branches & F_CHANNEL_CLOSE_OS_CLOSE) {
			unit_assert(stub[0].chid == ch->chid, goto done);
		}

		if (!(branches & F_CHANNEL_CLOSE_AS_BOUND)) {
			goto unbind;
		}

		if (branches & F_CHANNEL_CLOSE_FREE_SUBCTX) {
			unit_assert(ch->subctx == NULL, goto done);
		}

		if (ch->subctx != NULL) {
			if (g->ops.gr.setup.free_subctx != NULL) {
				g->ops.gr.setup.free_subctx(ch);
			}
			ch->subctx = NULL;
		}

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
		ch->deterministic = false;
		ch->deterministic_railgate_allowed = false;
#endif
		unit_assert(ch->usermode_submit_enabled == false, goto done);

		/* we took an extra reference to avoid nvgpu_vm_remove_ref */
		unit_assert(nvgpu_ref_put_return(&vm.ref, NULL), goto done);

		unit_assert(ch->user_sync == NULL, goto done);

unbind:
		/*
		 * branches not taken in safety build:
		 * - ch->sync != NULL
		 * - allow railgate for deterministic channel
		 * - unlink all debug sessions
		 * - free pre-allocated resources
		 * - channel refcount tracking
		 */
		unit_assert(ch->g == NULL, goto done);
		unit_assert(!ch->referenceable, goto done);
		unit_assert(!nvgpu_list_empty(&ch->free_chs), goto done);

		ch = NULL;
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_close));
	}
	nvgpu_set_enabled(g, NVGPU_DRIVER_IS_DYING, false);
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	g->os_channel.close = os_channel_close;
	return ret;
}

#define F_CHANNEL_SETUP_BIND_NO_AS				BIT(0)
#define F_CHANNEL_SETUP_BIND_USERMODE_ENABLED			BIT(1)
#define F_CHANNEL_SETUP_BIND_USERMODE_ALLOC_BUF_NULL		BIT(2)
#define F_CHANNEL_SETUP_BIND_USERMODE_ALLOC_BUF_FAIL		BIT(3)
#define F_CHANNEL_SETUP_BIND_USERMODE_SETUP_RAMFC_FAIL		BIT(4)
#define F_CHANNEL_SETUP_BIND_USERMODE_UPDATE_RL_FAIL		BIT(5)
#define F_CHANNEL_SETUP_BIND_USERMODE_TSGID_INVALID		BIT(6)
#define F_CHANNEL_SETUP_BIND_USERMODE_SUPPORT_DETERMINISTIC	BIT(7)
#define F_CHANNEL_SETUP_BIND_USERMODE_POWER_REF_COUNT_FAIL	BIT(8)
#define F_CHANNEL_SETUP_BIND_NON_USERMODE_DETERMINISTIC		BIT(9)
#define F_CHANNEL_SETUP_BIND_USERMODE_OS_CH_USERMODE_BUF	BIT(10)
#define F_CHANNEL_SETUP_GPFIFO_ENTRIES_OUT_OF_BOUND		BIT(11)
#define F_CHANNEL_SETUP_BIND_LAST				BIT(12)

static const char *f_channel_setup_bind[] = {
	"no_as",
	"usermode_enabled",
	"alloc_buf_null",
	"alloc_buf_fail",
	"setup_ramfc_fail",
	"update_rl_fail",
	"invalid tsgid",
	"support determininstic",
	"power ref count fail",
	"non usermode determinstic channel",
	"os_channel free usermode buffer",
};

static int stub_os_channel_alloc_usermode_buffers(struct nvgpu_channel *ch,
		struct nvgpu_setup_bind_args *args)
{
	int err;
	struct gk20a *g = ch->g;

	err = nvgpu_dma_alloc(g, NVGPU_CPU_PAGE_SIZE, &ch->usermode_userd);
	if (err != 0) {
		return err;
	}

	err = nvgpu_dma_alloc(g, NVGPU_CPU_PAGE_SIZE, &ch->usermode_gpfifo);
	if (err != 0) {
		return err;
	}

	stub[0].chid = ch->chid;
	return err;
}

static int stub_os_channel_alloc_usermode_buffers_ENOMEM(
		struct nvgpu_channel *ch, struct nvgpu_setup_bind_args *args)
{
	return -ENOMEM;
}

static int stub_runlist_update(struct gk20a *g, struct nvgpu_runlist *rl,
		struct nvgpu_channel *ch, bool add, bool wait_for_finish)
{
	stub[1].chid = ch->chid;
	return 0;
}

static int stub_runlist_update_ETIMEDOUT(struct gk20a *g,
					 struct nvgpu_runlist *rl,
					 struct nvgpu_channel *ch, bool add,
					 bool wait_for_finish)
{
	return -ETIMEDOUT;
}

static int stub_ramfc_setup_EINVAL(struct nvgpu_channel *ch, u64 gpfifo_base,
		u32 gpfifo_entries, u64 pbdma_acquire_timeout, u32 flags)
{
	return -EINVAL;
}

static int stub_mm_l2_flush(struct gk20a *g, bool invalidate)
{
	return 0;
}

static void stub_os_channel_free_usermode_buffers(struct nvgpu_channel *c)
{

}

int test_channel_setup_bind(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_posix_fault_inj *l_nvgpu_fi;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	u32 fail =
		F_CHANNEL_SETUP_BIND_NO_AS |
		F_CHANNEL_SETUP_BIND_USERMODE_ENABLED |
		F_CHANNEL_SETUP_BIND_USERMODE_ALLOC_BUF_NULL |
		F_CHANNEL_SETUP_BIND_USERMODE_ALLOC_BUF_FAIL |
		F_CHANNEL_SETUP_BIND_USERMODE_SETUP_RAMFC_FAIL |
		F_CHANNEL_SETUP_BIND_USERMODE_UPDATE_RL_FAIL |
		F_CHANNEL_SETUP_BIND_USERMODE_TSGID_INVALID |
		F_CHANNEL_SETUP_BIND_USERMODE_POWER_REF_COUNT_FAIL |
		F_CHANNEL_SETUP_BIND_NON_USERMODE_DETERMINISTIC |
		F_CHANNEL_SETUP_BIND_USERMODE_OS_CH_USERMODE_BUF |
		F_CHANNEL_SETUP_GPFIFO_ENTRIES_OUT_OF_BOUND;
	u32 prune = (u32)(F_CHANNEL_SETUP_BIND_USERMODE_SUPPORT_DETERMINISTIC) |
			fail;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	u32 tsgid_orig;
	bool privileged = false;
	int err;
	struct nvgpu_mem pdb_mem;
	struct mm_gk20a mm;
	struct vm_gk20a vm;
	int (*alloc_usermode_buffers)(struct nvgpu_channel *c,
		struct nvgpu_setup_bind_args *args) =
			g->os_channel.alloc_usermode_buffers;
	struct nvgpu_setup_bind_args bind_args;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	g->ops.gr.intr.flush_channel_tlb = stub_gr_intr_flush_channel_tlb;
	g->ops.mm.cache.l2_flush = stub_mm_l2_flush;	/* bug 2621189 */

	memset(&mm, 0, sizeof(mm));
	memset(&vm, 0, sizeof(vm));
	mm.g = g;
	vm.mm = &mm;
	ch->vm = &vm;
	err = nvgpu_dma_alloc(g, NVGPU_CPU_PAGE_SIZE, &pdb_mem);
	unit_assert(err == 0, goto done);
	vm.pdb.mem = &pdb_mem;

	memset(&bind_args, 0, sizeof(bind_args));
	bind_args.num_gpfifo_entries = 32;
	tsgid_orig = ch->tsgid;

	l_nvgpu_fi = nvgpu_nvgpu_get_fault_injection();

	for (branches = 0U; branches < F_CHANNEL_SETUP_BIND_LAST; branches++) {

		if (subtest_pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches,
					f_channel_setup_bind));
			continue;
		}
		subtest_setup(branches);

		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_setup_bind));

		ch->vm = branches & F_CHANNEL_SETUP_BIND_NO_AS ?
			NULL : &vm;

		if (branches & F_CHANNEL_SETUP_BIND_USERMODE_ENABLED) {
			ch->usermode_submit_enabled = true;
		}

		g->os_channel.alloc_usermode_buffers = branches &
			F_CHANNEL_SETUP_BIND_USERMODE_ALLOC_BUF_NULL ?
				NULL : stub_os_channel_alloc_usermode_buffers;

		if (branches & F_CHANNEL_SETUP_BIND_USERMODE_ALLOC_BUF_FAIL) {
			g->os_channel.alloc_usermode_buffers =
				stub_os_channel_alloc_usermode_buffers_ENOMEM;
		}

#ifdef CONFIG_NVGPU_IOCTL_NON_FUSA
		if (branches &
			F_CHANNEL_SETUP_BIND_USERMODE_SUPPORT_DETERMINISTIC) {
			bind_args.flags |=
				NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC;
		}

		if (branches &
			F_CHANNEL_SETUP_BIND_USERMODE_POWER_REF_COUNT_FAIL) {
			bind_args.flags |=
				NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC;
			ch->usermode_submit_enabled = false;
			nvgpu_posix_enable_fault_injection(l_nvgpu_fi, true, 0);
		}

#endif
		if (branches &
			F_CHANNEL_SETUP_BIND_NON_USERMODE_DETERMINISTIC) {
			bind_args.flags |=
				NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC;
			bind_args.flags &=
				~NVGPU_SETUP_BIND_FLAGS_USERMODE_SUPPORT;
		} else {
			bind_args.flags |=
				NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC;
			bind_args.flags |=
				NVGPU_SETUP_BIND_FLAGS_USERMODE_SUPPORT;
		}

		if (branches & F_CHANNEL_SETUP_GPFIFO_ENTRIES_OUT_OF_BOUND) {
			bind_args.num_gpfifo_entries = -1;
		}

		ch->tsgid = branches &
			F_CHANNEL_SETUP_BIND_USERMODE_TSGID_INVALID ?
				NVGPU_INVALID_TSG_ID : tsgid_orig;

		g->ops.runlist.update = branches &
			F_CHANNEL_SETUP_BIND_USERMODE_UPDATE_RL_FAIL ?
				stub_runlist_update_ETIMEDOUT :
				stub_runlist_update;

		g->ops.ramfc.setup = branches &
			F_CHANNEL_SETUP_BIND_USERMODE_SETUP_RAMFC_FAIL ?
				stub_ramfc_setup_EINVAL : gops.ramfc.setup;

		if (branches &
			F_CHANNEL_SETUP_BIND_USERMODE_OS_CH_USERMODE_BUF) {
			g->ops.ramfc.setup = stub_ramfc_setup_EINVAL;
			g->os_channel.free_usermode_buffers =
				stub_os_channel_free_usermode_buffers;
		}

		err = nvgpu_channel_setup_bind(ch, &bind_args);

		if (branches & fail) {
			nvgpu_posix_enable_fault_injection(
							l_nvgpu_fi, false, 0);
			unit_assert(err != 0, goto done);
			unit_assert(!nvgpu_mem_is_valid(&ch->usermode_userd),
				goto done);
			unit_assert(!nvgpu_mem_is_valid(&ch->usermode_gpfifo),
				goto done);
			ch->usermode_submit_enabled = false;
			unit_assert(nvgpu_atomic_read(&ch->bound) == false,
				goto done);
			g->os_channel.free_usermode_buffers = NULL;
			bind_args.num_gpfifo_entries = 32;
		} else {
			unit_assert(err == 0, goto done);
			unit_assert(stub[0].chid == ch->chid, goto done);
			unit_assert(ch->usermode_submit_enabled == true,
				goto done);
			unit_assert(ch->userd_iova != 0U, goto done);
			unit_assert(stub[1].chid == ch->chid, goto done);
			unit_assert(nvgpu_atomic_read(&ch->bound) == true,
				goto done);
			nvgpu_dma_free(g, &ch->usermode_userd);
			nvgpu_dma_free(g, &ch->usermode_gpfifo);
			ch->userd_iova = 0U;
#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
			ch->deterministic = false;
#endif
			nvgpu_atomic_set(&ch->bound, false);
		}
		bind_args.flags &=
				~NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC;
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_setup_bind));
	}
	nvgpu_set_enabled(g, NVGPU_DRIVER_IS_DYING, false);
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	nvgpu_dma_free(g, &pdb_mem);
	g->os_channel.alloc_usermode_buffers = alloc_usermode_buffers;
	g->ops = gops;
	return ret;
}

#define F_CHANNEL_ALLOC_INST_ENOMEM				BIT(0)
#define F_CHANNEL_ALLOC_INST_LAST				BIT(1)

static const char *f_channel_alloc_inst[] = {
	"nomem",
};

int test_channel_alloc_inst(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	u32 branches = 0U;
	u32 fail = F_CHANNEL_ALLOC_INST_ENOMEM;
	u32 prune = fail;
	int ret = UNIT_FAIL;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;
	struct nvgpu_posix_fault_inj *dma_fi;
	int err;

	dma_fi = nvgpu_dma_alloc_get_fault_injection();

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	for (branches = 0U; branches < F_CHANNEL_ALLOC_INST_LAST; branches++) {

		if (subtest_pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches,
					f_channel_alloc_inst));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_alloc_inst));

		nvgpu_posix_enable_fault_injection(dma_fi,
			branches & F_CHANNEL_ALLOC_INST_ENOMEM ?
				true : false, 0);

		err = nvgpu_channel_alloc_inst(g, ch);

		if (branches & fail) {
			unit_assert(err != 0, goto done);
			unit_assert(ch->inst_block.aperture ==
					APERTURE_INVALID, goto done);
		} else {
			unit_assert(err == 0, goto done);
			unit_assert(ch->inst_block.aperture !=
					APERTURE_INVALID, goto done);
		}

		nvgpu_channel_free_inst(g, ch);
		unit_assert(ch->inst_block.aperture == APERTURE_INVALID,
			goto done);
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_alloc_inst));
	}
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	return ret;
}

/*
 * channel non-referenceable case is covered when no match is found
 * since we looked up all possible channels.
 */
#define F_CHANNEL_FROM_INST_NO_INIT			BIT(0)
#define F_CHANNEL_FROM_INST_NO_CHANNEL			BIT(1)
#define F_CHANNEL_FROM_INST_MATCH_A			BIT(2)
#define F_CHANNEL_FROM_INST_MATCH_B			BIT(3)
#define F_CHANNEL_FROM_INST_LAST			BIT(4)

static const char *f_channel_from_inst[] = {
	"no_init",
	"no_channel",
	"match_a",
	"match_b",
};

int test_channel_from_inst(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_channel *chA = NULL;
	struct nvgpu_channel *chB = NULL;
	struct nvgpu_fifo *f = &g->fifo;
	struct nvgpu_fifo fifo = g->fifo;
	u32 branches = 0U;
	u32 found =
		F_CHANNEL_FROM_INST_MATCH_A |
		F_CHANNEL_FROM_INST_MATCH_B;
	u32 prune =  found |
		F_CHANNEL_FROM_INST_NO_INIT |
		F_CHANNEL_FROM_INST_NO_CHANNEL;
	int ret = UNIT_FAIL;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	u64 inst_ptr;
	bool privileged = false;

	chA = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(chA != NULL, goto done);

	chB = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(chB != NULL, goto done);

	unit_assert(f->num_channels > 0U, goto done);

	for (branches = 0U; branches < F_CHANNEL_FROM_INST_LAST; branches++) {

		if (subtest_pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches,
					f_channel_from_inst));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_from_inst));

		if (branches & F_CHANNEL_FROM_INST_NO_INIT) {
			f->channel = NULL;
		}

		if (branches & F_CHANNEL_FROM_INST_NO_CHANNEL) {
			f->num_channels = 0U;
		}

		inst_ptr = (u64)-1;

		if (branches & F_CHANNEL_FROM_INST_MATCH_A) {
			inst_ptr = nvgpu_inst_block_addr(g, &chA->inst_block);
		}

		if (branches & F_CHANNEL_FROM_INST_MATCH_B) {
			inst_ptr = nvgpu_inst_block_addr(g, &chB->inst_block);
		}

		ch = nvgpu_channel_refch_from_inst_ptr(g, inst_ptr);

		if (branches & found) {
			if (branches & F_CHANNEL_FROM_INST_MATCH_A) {
				unit_assert(ch == chA, goto done);
			}
			if (branches & F_CHANNEL_FROM_INST_MATCH_B) {
				unit_assert(ch == chB, goto done);
			}
			unit_assert(nvgpu_atomic_read(&ch->ref_count) == 2,
				goto done);
			nvgpu_channel_put(ch);
		} else {
			f->channel = fifo.channel;
			f->num_channels = fifo.num_channels;
			unit_assert(ch == NULL, goto done);
		}
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_from_inst));
	}
	if (chA != NULL) {
		nvgpu_channel_close(chA);
	}
	if (chB != NULL) {
		nvgpu_channel_close(chB);
	}
	return ret;
}

static void stub_tsg_enable(struct nvgpu_tsg *tsg)
{
	stub[0].tsgid = tsg->tsgid;
}

static void stub_tsg_disable(struct nvgpu_tsg *tsg)
{
	stub[1].tsgid = tsg->tsgid;
}

int test_channel_enable_disable_tsg(struct unit_module *m,
						struct gk20a *g, void *vargs)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	u32 branches = 0U;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;
	int err;
	int ret = UNIT_FAIL;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	g->ops.tsg.enable = stub_tsg_enable;
	g->ops.tsg.disable = stub_tsg_disable;

	subtest_setup(branches);

	err = nvgpu_channel_enable_tsg(g, ch);
	unit_assert(stub[0].tsgid = tsg->tsgid, goto done);

	err = nvgpu_channel_disable_tsg(g, ch);
	unit_assert(stub[1].tsgid = tsg->tsgid, goto done);

	subtest_setup(branches);

	err = nvgpu_tsg_unbind_channel(tsg, ch, true);
	unit_assert(err == 0, goto done);

	err = nvgpu_channel_enable_tsg(g, ch);
	unit_assert(err != 0, goto done);

	err = nvgpu_channel_disable_tsg(g, ch);
	unit_assert(err != 0, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	return ret;
}

#define F_CHANNEL_ABORT_TSG		BIT(0)
#define F_CHANNEL_ABORT_LAST		BIT(1)

static const char *f_channel_abort[] = {
	"tsg not null",
};

int test_channel_abort(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;
	int err;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	for (branches = 0U; branches < F_CHANNEL_ABORT_LAST; branches++) {
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_abort));

		if ((branches & F_CHANNEL_ABORT_TSG) != 0U) {
			err = nvgpu_tsg_bind_channel(tsg, ch);
			unit_assert(err == 0, goto done);
		}

		nvgpu_channel_abort(ch, false);
		unit_assert(ch->unserviceable == true, goto done);
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_abort));
	}
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	return ret;
}

#define F_CHANNEL_MARK_ERROR_COND_BROADCAST_FAIL	BIT(0)
#define F_CHANNEL_MARK_ERROR_LAST			BIT(1)

static const char *f_channel_mark_error[] = {
	"condition_broadcast_fail",
};

int test_channel_mark_error(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	u32 branches = 0U;
	int ret = UNIT_FAIL;

	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;
	bool err;

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	for (branches = 0U; branches < F_CHANNEL_MARK_ERROR_LAST; branches++) {

		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_mark_error));

		if ((branches & F_CHANNEL_MARK_ERROR_COND_BROADCAST_FAIL)
									!= 0) {
			ch->semaphore_wq.initialized = false;
			ch->notifier_wq.initialized = false;
		}

		err = nvgpu_channel_mark_error(g, ch);
		unit_assert(err == false, goto done);
		unit_assert(ch->unserviceable == true, goto done);

		ch->semaphore_wq.initialized = true;
		ch->notifier_wq.initialized = true;
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_mark_error));
	}
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}

	return ret;
}

int test_channel_sw_quiesce(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_fifo *f = &g->fifo;
	int ret = UNIT_FAIL;

	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);
	unit_assert(f->num_channels > 0U, goto done);

	nvgpu_channel_sw_quiesce(g);
	unit_assert(ch->unserviceable == true, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}

	return ret;
}

#define F_CHANNEL_DETERMINISTIC_IDLE_UNIDLE		BIT(0)
#define F_CHANNEL_DETERMINISTIC_IDLE_RAILGATE_ALLOWED	BIT(1)
#define F_CHANNEL_DETERMINISTIC_UNIDLE_GK20ABUSY_FAIL	BIT(2)
#define F_CHANNEL_DETERMINISTIC_IDLE_LAST		BIT(3)

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
static const char *f_channel_deterministic_idle_unidle[] = {
	"deterministic_channel",
	"determinstic_railgate_allowed",
	"gk20a_busy_fail",
};

int test_channel_deterministic_idle_unidle(struct unit_module *m,
						struct gk20a *g, void *vargs)
{
	struct nvgpu_posix_fault_inj *l_nvgpu_fi;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_mem pdb_mem;
	struct mm_gk20a mm;
	struct vm_gk20a vm;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;
	int err;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	int gpu_usage_count_initial;

	struct nvgpu_setup_bind_args bind_args;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	memset(&mm, 0, sizeof(mm));
	memset(&vm, 0, sizeof(vm));
	mm.g = g;
	vm.mm = &mm;
	ch->vm = &vm;
	err = nvgpu_dma_alloc(g, NVGPU_CPU_PAGE_SIZE, &pdb_mem);
	unit_assert(err == 0, goto done);
	vm.pdb.mem = &pdb_mem;

	g->ops.gr.intr.flush_channel_tlb = stub_gr_intr_flush_channel_tlb;
	g->ops.mm.cache.l2_flush = stub_mm_l2_flush;	/* bug 2621189 */
	g->os_channel.alloc_usermode_buffers =
					stub_os_channel_alloc_usermode_buffers;
	g->ops.runlist.update = stub_runlist_update;

	(void)memset(&bind_args, 0, sizeof(bind_args));
	bind_args.num_gpfifo_entries = 32;

	bind_args.flags |= NVGPU_SETUP_BIND_FLAGS_USERMODE_SUPPORT;

	l_nvgpu_fi = nvgpu_nvgpu_get_fault_injection();

	for (branches = 0U; branches < F_CHANNEL_DETERMINISTIC_IDLE_LAST;
								branches++) {
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches,
					f_channel_deterministic_idle_unidle));

		if ((branches & F_CHANNEL_DETERMINISTIC_IDLE_UNIDLE) != 0) {
			bind_args.flags |=
				NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC;
		} else {
			bind_args.flags &=
				~NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC;
		}

		err = nvgpu_channel_setup_bind(ch, &bind_args);
		unit_assert(err == 0, goto done);
		unit_assert(nvgpu_atomic_read(&ch->bound) == true, goto done);

		ch->deterministic_railgate_allowed = (branches &
			F_CHANNEL_DETERMINISTIC_IDLE_RAILGATE_ALLOWED) ?
			true : false;

		nvgpu_posix_enable_fault_injection(l_nvgpu_fi, ((branches &
			F_CHANNEL_DETERMINISTIC_UNIDLE_GK20ABUSY_FAIL) != 0) ?
			true : false, 0);

		gpu_usage_count_initial = g->usage_count.v;

		nvgpu_channel_deterministic_idle(g);
		if ((u64)(branches & 0x3U) ==
			(F_CHANNEL_DETERMINISTIC_IDLE_UNIDLE &
			~F_CHANNEL_DETERMINISTIC_IDLE_RAILGATE_ALLOWED)) {
			unit_assert(g->usage_count.v ==
				(gpu_usage_count_initial - 1), goto done);
		} else {

			unit_assert(g->usage_count.v == gpu_usage_count_initial,
				goto done);
		}

		nvgpu_channel_deterministic_unidle(g);
		if (branches == ((F_CHANNEL_DETERMINISTIC_IDLE_UNIDLE |
			F_CHANNEL_DETERMINISTIC_UNIDLE_GK20ABUSY_FAIL) &
			~F_CHANNEL_DETERMINISTIC_IDLE_RAILGATE_ALLOWED)) {
			unit_assert(g->usage_count.v ==
				(gpu_usage_count_initial - 1), goto done);
		} else {
			unit_assert(g->usage_count.v == gpu_usage_count_initial,
				goto done);
		}

		nvgpu_dma_free(g, &ch->usermode_userd);
		nvgpu_dma_free(g, &ch->usermode_gpfifo);
		ch->userd_iova = 0U;
		ch->deterministic = false;
		ch->usermode_submit_enabled = false;
		nvgpu_atomic_set(&ch->bound, false);
		nvgpu_posix_enable_fault_injection(l_nvgpu_fi, false, 0);
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches,
					f_channel_deterministic_idle_unidle));
	}
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}

	return ret;
}
#endif

#define F_CHANNEL_SUSPEND_RESUME_UNSERVICEABLE_CH		BIT(0)
#define F_CHANNEL_SUSPEND_RESUME_INVALID_TSGID			BIT(1)
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
#define F_CHANNEL_SUSPEND_RESUME_CH_WRK_CMPL_CNCL_SYNC		BIT(2)
#define F_CHANNEL_SUSPEND_RESUME_CHS_LAST			BIT(3)
#else
#define F_CHANNEL_SUSPEND_RESUME_CHS_LAST			BIT(2)
#endif

static const char *f_channel_suspend_resume[] = {
	"suspend_resume_unserviceable_channels",
	"invalid_tsgid",
	"work_completion_cancel_sync",
};

static int stub_fifo_preempt_tsg(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	stub[0].tsgid = tsg->tsgid;
	return 0;
}

static int stub_fifo_preempt_channel(struct gk20a *g, struct nvgpu_channel *ch)
{
	stub[0].chid = ch->chid;
	return -1;
}

static int stub_runlist_reload(struct gk20a *g, struct nvgpu_runlist *rl,
			 struct nvgpu_runlist_domain *domain,
			 bool add, bool wait_for_finish)
{
	return 0;
}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
static void stub_channel_work_completion_cancel_sync(struct nvgpu_channel *ch)
{

}
#endif

int test_channel_suspend_resume_serviceable_chs(struct unit_module *m,
						struct gk20a *g, void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;
	bool err;
	u32 orig_ch_tsgid;
	u32 branches = 0U;
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	u32 prune = F_CHANNEL_SUSPEND_RESUME_UNSERVICEABLE_CH |
			F_CHANNEL_SUSPEND_RESUME_INVALID_TSGID |
			F_CHANNEL_SUSPEND_RESUME_CH_WRK_CMPL_CNCL_SYNC;
#else
	u32 prune = F_CHANNEL_SUSPEND_RESUME_UNSERVICEABLE_CH |
			F_CHANNEL_SUSPEND_RESUME_INVALID_TSGID;
#endif
	int ret = UNIT_FAIL;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);
	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	g->ops.fifo.preempt_tsg = stub_fifo_preempt_tsg;
	g->ops.fifo.preempt_channel = stub_fifo_preempt_channel;
	g->ops.runlist.reload = stub_runlist_reload;
	orig_ch_tsgid = ch->tsgid;

	for (branches = 0U; branches < F_CHANNEL_SUSPEND_RESUME_CHS_LAST;
								branches++) {
		if (subtest_pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches,
						f_channel_suspend_resume));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches,
					f_channel_suspend_resume));

		if (branches & F_CHANNEL_SUSPEND_RESUME_UNSERVICEABLE_CH) {
			nvgpu_channel_set_unserviceable(ch);
		} else {
			ch->unserviceable = false;
		}
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		g->os_channel.work_completion_cancel_sync = branches &
			F_CHANNEL_SUSPEND_RESUME_CH_WRK_CMPL_CNCL_SYNC ?
			stub_channel_work_completion_cancel_sync : NULL;
#endif

		ch->tsgid = branches & F_CHANNEL_SUSPEND_RESUME_INVALID_TSGID ?
				NVGPU_INVALID_TSG_ID : orig_ch_tsgid;

		err = nvgpu_channel_suspend_all_serviceable_ch(g);
		unit_assert(err == 0, goto done);
		err = nvgpu_channel_resume_all_serviceable_ch(g);

		if (branches & F_CHANNEL_SUSPEND_RESUME_INVALID_TSGID) {
			unit_assert(stub[0].chid == ch->chid, goto done);
		} else if (branches &
				F_CHANNEL_SUSPEND_RESUME_UNSERVICEABLE_CH) {
			unit_assert(err == 0, goto done);
		} else {
			unit_assert(stub[0].tsgid == ch->tsgid, goto done);
		}
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches,
					f_channel_suspend_resume));
	}
	if (ch != NULL) {
		nvgpu_tsg_unbind_channel(tsg, ch, true);
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}

	return ret;
}

#define F_CHANNEL_DEBUG_DUMP_INFOS_ALLOC_FAIL		BIT(0)
#define F_CHANNEL_DEBUG_DUMP_INFO_ALLOC_FAIL		BIT(1)
#define F_CHANNEL_DEBUG_DUMP_LAST			BIT(2)

static const char *f_channel_debug_dump[] = {
	"infos_alloc_fail",
	"info_alloc_fail",
};

static void stub_channel_read_state(struct gk20a *g, struct nvgpu_channel *ch,
		struct nvgpu_channel_hw_state *state)
{
	stub[0].chid = ch->chid;
}

static void stub_ramfc_capture_ram_dump(struct gk20a *g,
		struct nvgpu_channel *ch, struct nvgpu_channel_dump_info *info)
{
	stub[1].chid = ch->chid;
}

int test_channel_debug_dump(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct gpu_ops gops = g->ops;
	struct nvgpu_posix_fault_inj *kmem_fi;
	struct nvgpu_debug_context o = {
		.fn = NULL
	};

	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;
	bool err;
	u32 branches = 0U;
	u32 fail = F_CHANNEL_DEBUG_DUMP_INFOS_ALLOC_FAIL |
			F_CHANNEL_DEBUG_DUMP_INFO_ALLOC_FAIL;
	u32 prune = fail;
	int ret = UNIT_FAIL;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	kmem_fi = nvgpu_kmem_get_fault_injection();

	g->ops.channel.read_state = stub_channel_read_state;
	g->ops.ramfc.capture_ram_dump = stub_ramfc_capture_ram_dump;

	for (branches = 0U; branches < F_CHANNEL_DEBUG_DUMP_LAST;
								branches++) {
		if (subtest_pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, f_channel_debug_dump));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_debug_dump));

		if (branches & F_CHANNEL_DEBUG_DUMP_INFOS_ALLOC_FAIL) {
			nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
		}
		if (branches & F_CHANNEL_DEBUG_DUMP_INFO_ALLOC_FAIL) {
			nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
		}

		nvgpu_channel_debug_dump_all(g, &o);
		if (branches & fail) {
			nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
		} else {
			unit_assert(stub[0].chid == ch->chid, goto done);
			unit_assert(stub[1].chid == ch->chid, goto done);
		}
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_debug_dump));
	}
	if (ch != NULL) {
		nvgpu_tsg_unbind_channel(tsg, ch, true);
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}

	g->ops = gops;
	return ret;
}

#define F_CHANNEL_SEMAPHORRE_WAKEUP_DETERMINISTIC_CH		BIT(0)
#define F_CHANNEL_SEMAPHORRE_WAKEUP_COND_BROADCAST_FAIL		BIT(1)
#define F_CHANNEL_SEMAPHORRE_WAKEUP_CH_NOT_BOUND		BIT(2)
#define F_CHANNEL_SEMAPHORRE_WAKEUP_LAST			BIT(3)

static const char *f_channel_semaphore_wakeup[] = {
	"deterministic_channel",
	"condition_broadcast_fail",
	"channel_not_bound",
};

static u32 global_count;

static int stub_mm_fb_flush(struct gk20a *g)
{
	stub[0].count = global_count++;
	return 0;
}

int test_channel_semaphore_wakeup(struct unit_module *m,
						struct gk20a *g, void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_mem pdb_mem;
	struct mm_gk20a mm;
	struct vm_gk20a vm;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;
	bool err;
	u32 branches = 0U;
	u32 prune = F_CHANNEL_SEMAPHORRE_WAKEUP_CH_NOT_BOUND;
	int ret = UNIT_FAIL;

	struct nvgpu_setup_bind_args bind_args;

	global_count = 0;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	memset(&mm, 0, sizeof(mm));
	memset(&vm, 0, sizeof(vm));
	mm.g = g;
	vm.mm = &mm;
	ch->vm = &vm;
	err = nvgpu_dma_alloc(g, NVGPU_CPU_PAGE_SIZE, &pdb_mem);
	unit_assert(err == 0, goto done);
	vm.pdb.mem = &pdb_mem;

	g->ops.gr.intr.flush_channel_tlb = stub_gr_intr_flush_channel_tlb;
	g->ops.mm.cache.l2_flush = stub_mm_l2_flush;	/* bug 2621189 */
	g->os_channel.alloc_usermode_buffers =
					stub_os_channel_alloc_usermode_buffers;
	g->ops.runlist.update = stub_runlist_update;
	g->ops.mm.cache.fb_flush = stub_mm_fb_flush;

	memset(&bind_args, 0, sizeof(bind_args));
	bind_args.num_gpfifo_entries = 32;

	bind_args.flags |= NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC;
	bind_args.flags |= NVGPU_SETUP_BIND_FLAGS_USERMODE_SUPPORT;

	err = nvgpu_channel_setup_bind(ch, &bind_args);
	unit_assert(err == 0, goto done);
	unit_assert(nvgpu_atomic_read(&ch->bound) == true, goto done);

	for (branches = 0U; branches < F_CHANNEL_SEMAPHORRE_WAKEUP_LAST;
								branches++) {
		if (subtest_pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches,
						f_channel_semaphore_wakeup));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_semaphore_wakeup));

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
		if (branches & F_CHANNEL_SEMAPHORRE_WAKEUP_DETERMINISTIC_CH) {
			ch->deterministic = true;
		}
#endif

		ch->semaphore_wq.initialized = branches &
			F_CHANNEL_SEMAPHORRE_WAKEUP_COND_BROADCAST_FAIL ?
			false : true;

		if (branches & F_CHANNEL_SEMAPHORRE_WAKEUP_CH_NOT_BOUND) {
			nvgpu_atomic_set(&ch->bound, false);
		} else {
			nvgpu_atomic_set(&ch->bound, true);
		}

		nvgpu_channel_semaphore_wakeup(g, false);
		unit_assert(stub[0].count == (global_count - 1U), goto done);

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
		ch->deterministic = false;
#endif
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_channel_semaphore_wakeup));
	}
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}

	return ret;
}

int test_channel_from_invalid_id(struct unit_module *m, struct gk20a *g,
								void *args)
{
	struct nvgpu_channel *ch = NULL;
	int ret = UNIT_FAIL;

	ch = nvgpu_channel_from_id(g, NVGPU_INVALID_CHANNEL_ID);
	unit_assert(ch == NULL, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}
	return ret;
}

int test_nvgpu_channel_from_id_bvec(struct unit_module *m,
					struct gk20a *g, void *args)
{
	struct nvgpu_channel *ch = NULL;
	int ret = UNIT_FAIL;
	/* One channel is already opened by default */
	int num_channels_to_open = g->fifo.num_channels;
	u32 valid_chids[][2] = {{0, g->fifo.num_channels - 1}};
	u32 invalid_chids[][2] = {{g->fifo.num_channels, U32_MAX}};
	u32 (*working_list)[2];
	u32 chid, chid_ranges;
	/*
	 * i is to loop through valid and invalid cases
	 * j is to loop through different ranges within ith case
	 * states is for min, max and median
	 */
	u32 i, j, states;
	int c;
	const char *string_cases[] = {"Valid", "Invalid"};
	const char *string_states[] = {"Min", "Max", "Mid"};
	u32 chid_range_difference;

	struct nvgpu_channel **ch_list = (struct nvgpu_channel **)calloc(sizeof(struct nvgpu_channel *), num_channels_to_open);

	for (c = 0; c < num_channels_to_open; c++) {
		ch_list[c] = nvgpu_channel_open_new(g, -1, false, getpid(), getpid());
		if (ch_list[c] == NULL) {
			unit_err(m, "Unable to create channels\n");
			goto done;
		}
	}

	/* loop through valid and invalid cases */
	for (i = 0; i < 2; i++) {
		/* select appropriate iteration size */
		chid_ranges = (i == 0) ? ARRAY_SIZE(valid_chids) : ARRAY_SIZE(invalid_chids);
		/* select correct working list */
		working_list = (i == 0) ? valid_chids : invalid_chids;
		for (j = 0; j < chid_ranges; j++) {
			for (states = 0; states < 3; states++) {
				/* check for min chid */
				if (states == 0)
					chid = working_list[j][0];
				else if (states == 1) {
					/* check for max valid chid */
					chid = working_list[j][1];
				} else {
					chid_range_difference = working_list[j][1] - working_list[j][0];
					/* Check for random chid in range */
					if (chid_range_difference > 1)
						chid = get_random_u32(working_list[j][0] + 1, working_list[j][1] - 1);
					else
						continue;
				}

				unit_info(m, "BVEC testing for nvgpu_channel_from_id with chid = 0x%08x(%s range [0x%08x - 0x%08x] %s)\n", chid, string_cases[i], working_list[j][0], working_list[j][1], string_states[states]);
				ch = nvgpu_channel_from_id(g, chid);
				if (i == 0)
					unit_assert(ch != NULL, goto done);
				else
					unit_assert(ch == NULL, goto done);

				/* Clearing for success cases */
				if (ch != NULL) {
					nvgpu_channel_put(ch);
					ch = NULL;
				}
			}
		}
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	while (--c >= 0 ) {
		nvgpu_channel_close(ch_list[c]);
	}

	return ret;
}

int test_channel_put_warn(struct unit_module *m, struct gk20a *g, void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_fifo *f = &g->fifo;
	int ret = UNIT_FAIL;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);
	unit_assert(f->num_channels > 0U, goto done);

	/* condition broadcast fail */
	ch->ref_count_dec_wq.initialized = false;

	nvgpu_atomic_set(&ch->ref_count, 2);
	ch->referenceable = true;
	nvgpu_channel_put(ch);

	/*
	 * Note: channel ref_count value is 1 now
	 * This function call will reduce count to 0
	 */
	nvgpu_channel_put(ch);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}
	if (ch != NULL) {
		nvgpu_atomic_set(&ch->ref_count, 1);
		nvgpu_channel_close(ch);
	}

	return ret;
}

int test_ch_referenceable_cleanup(struct unit_module *m, struct gk20a *g,
								void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_fifo *f = &g->fifo;
	int err = 0;
	int ret = UNIT_FAIL;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;

	ch = nvgpu_channel_open_new(g, runlist_id,
			privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);
	unit_assert(f->num_channels > 0U, goto done);

	nvgpu_channel_cleanup_sw(g);
	unit_assert(f->channel == NULL, goto done);

	/* Reset environment variables */
	err = nvgpu_channel_setup_sw(g);
	unit_assert(err == 0, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}
	return ret;
}

int test_channel_abort_cleanup(struct unit_module *m, struct gk20a *g,
								void *vargs)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg;
	struct mm_gk20a mm;
	struct vm_gk20a vm;
	int err = 0;
	int ret = UNIT_FAIL;
	u32 runlist_id = NVGPU_INVALID_RUNLIST_ID;
	bool privileged = false;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	g->ops.gr.intr.flush_channel_tlb = stub_gr_intr_flush_channel_tlb;

	ch = nvgpu_channel_open_new(g, runlist_id,
				privileged, getpid(), getpid());
	unit_assert(ch != NULL, goto done);
	ch->usermode_submit_enabled = true;

	/* Channel requires to be as_bound */
	memset(&mm, 0, sizeof(mm));
	memset(&vm, 0, sizeof(vm));
	mm.g = g;
	vm.mm = &mm;
	ch->vm = &vm;
	nvgpu_ref_init(&vm.ref);
	nvgpu_ref_get(&vm.ref);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	err = nvgpu_tsg_unbind_channel(tsg, ch, true);
	unit_assert(err == 0, goto done);

	nvgpu_channel_close(ch);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}
	return ret;
}

static void stub_mm_init_inst_block(struct nvgpu_mem *inst_block,
			      struct vm_gk20a *vm, u32 big_page_size)
{
	stub[0].count = big_page_size;
}

int test_nvgpu_channel_commit_va(struct unit_module *m, struct gk20a *g,
								void *vargs)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel ch;
	struct vm_gk20a vm;
	int ret = UNIT_FAIL;

	memset(&vm, 0, sizeof(vm));
	ch.g = g;
	ch.vm = &vm;
	g->ops.mm.init_inst_block = stub_mm_init_inst_block;
	vm.gmmu_page_sizes[GMMU_PAGE_SIZE_BIG] =
					nvgpu_safe_cast_u64_to_u32(SZ_1K);

	nvgpu_channel_commit_va(&ch);
	unit_assert(stub[0].count == SZ_1K, goto done);
	vm.gmmu_page_sizes[GMMU_PAGE_SIZE_BIG] = 0;

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}
	g->ops = gops;
	return ret;
}

int test_nvgpu_get_gpfifo_entry_size(struct unit_module *m, struct gk20a *g,
								void *vargs)
{
	if (nvgpu_get_gpfifo_entry_size() != 8U) {
		unit_return_fail(m, "posix gpfifo entry size is non-zero\n");
	}

	return UNIT_SUCCESS;
}

int test_trace_write_pushbuffers(struct unit_module *m, struct gk20a *g,
								void *vargs)
{
	struct nvgpu_channel ch;
#ifndef CONFIG_DEBUG_FS
	trace_write_pushbuffers(&ch, 1U);
#endif

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_channel_tests[] = {
	UNIT_TEST(setup_sw, test_channel_setup_sw, &unit_ctx, 0),
	UNIT_TEST(init_support, test_fifo_init_support, &unit_ctx, 0),
	UNIT_TEST(open, test_channel_open, &unit_ctx, 0),
	UNIT_TEST(close, test_channel_close, &unit_ctx, 0),
	UNIT_TEST(setup_bind, test_channel_setup_bind, &unit_ctx, 0),
	UNIT_TEST(alloc_inst, test_channel_alloc_inst, &unit_ctx, 0),
	UNIT_TEST(from_inst, test_channel_from_inst, &unit_ctx, 0),
	UNIT_TEST(enable_disable_tsg,
			test_channel_enable_disable_tsg, &unit_ctx, 0),
	UNIT_TEST(ch_abort, test_channel_abort, &unit_ctx, 0),
	UNIT_TEST(mark_error, test_channel_mark_error, &unit_ctx, 0),
	UNIT_TEST(sw_quiesce, test_channel_sw_quiesce, &unit_ctx, 0),
#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
	UNIT_TEST(idle_unidle, test_channel_deterministic_idle_unidle, &unit_ctx, 0),
#endif
	UNIT_TEST(suspend_resume, test_channel_suspend_resume_serviceable_chs, &unit_ctx, 0),
	UNIT_TEST(debug_dump, test_channel_debug_dump, &unit_ctx, 0),
	UNIT_TEST(semaphore_wakeup, test_channel_semaphore_wakeup, &unit_ctx, 0),
	UNIT_TEST(channel_from_invalid_id, test_channel_from_invalid_id, &unit_ctx, 0),
	UNIT_TEST(nvgpu_channel_from_chid_bvec, test_nvgpu_channel_from_id_bvec, &unit_ctx, 0),
	UNIT_TEST(channel_put_warn, test_channel_put_warn, &unit_ctx, 0),
	UNIT_TEST(referenceable_cleanup, test_ch_referenceable_cleanup, &unit_ctx, 0),
	UNIT_TEST(abort_cleanup, test_channel_abort_cleanup, &unit_ctx, 0),
	UNIT_TEST(channel_commit_va, test_nvgpu_channel_commit_va, &unit_ctx, 2),
	UNIT_TEST(get_gpfifo_entry_size, test_nvgpu_get_gpfifo_entry_size, &unit_ctx, 0),
	UNIT_TEST(trace_write_pushbuffers, test_trace_write_pushbuffers, &unit_ctx, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, &unit_ctx, 0),
};

UNIT_MODULE(nvgpu_channel, nvgpu_channel_tests, UNIT_PRIO_NVGPU_TEST);
