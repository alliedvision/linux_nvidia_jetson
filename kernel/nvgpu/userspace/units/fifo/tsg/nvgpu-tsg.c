/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/error_notifier.h>
#include <nvgpu/tsg.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/fifo/userd.h>
#include <nvgpu/runlist.h>
#include <nvgpu/fuse.h>
#include <nvgpu/dma.h>
#include <nvgpu/gr/ctx.h>

#include "common/gr/ctx_priv.h"
#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/posix-channel.h>

#include "hal/fifo/tsg_gk20a.h"

#include "hal/init/hal_gv11b.h"

#include "../nvgpu-fifo-common.h"
#include "nvgpu-tsg.h"

#ifdef TSG_UNIT_DEBUG
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

struct tsg_unit_ctx {
	u32 branches;
};

static struct tsg_unit_ctx unit_ctx;

#define MAX_STUB	4

struct stub_ctx {
	const char *name;
	u32 count;
	u32 chid;
	u32 tsgid;
	u32 runlist_mask;
	u32 runlist_state;
};

struct stub_ctx stub[MAX_STUB];

static void subtest_setup(u32 branches)
{
	u32 i;

	unit_ctx.branches = branches;

	memset(stub, 0, sizeof(stub));
	for (i = 0; i < MAX_STUB; i++) {
		stub[i].name = "";
		stub[i].count = 0;
		stub[i].chid = NVGPU_INVALID_CHANNEL_ID;
		stub[i].tsgid = NVGPU_INVALID_TSG_ID;
	}
}

#define pruned	test_fifo_subtest_pruned
#define branches_str test_fifo_flags_str

#define F_TSG_OPEN_ACQUIRE_CH_FAIL		BIT(0)
#define F_TSG_OPEN_SM_FAIL			BIT(1)
#define F_TSG_OPEN_ALLOC_SM_FAIL		BIT(2)
#define F_TSG_OPEN_ALLOC_SM_KZALLOC_FAIL	BIT(3)
#define F_TSG_OPEN_ALLOC_GR_FAIL		BIT(4)
#define F_TSG_OPEN_NO_INIT_BUF			BIT(5)
#define F_TSG_OPEN_INIT_BUF_FAIL		BIT(6)
#define F_TSG_OPEN_NO_OPEN_HAL			BIT(7)
#define F_TSG_OPEN_OPEN_HAL_FAIL		BIT(8)
#define F_TSG_OPEN_LAST				BIT(9)

static int stub_tsg_init_eng_method_buffers(struct gk20a *g,
	struct nvgpu_tsg *tsg)
{
	if (unit_ctx.branches & F_TSG_OPEN_INIT_BUF_FAIL) {
		return -ENOMEM;
	}
	return 0;
}


static int stub_tsg_open(struct nvgpu_tsg *tsg)
{
	if (unit_ctx.branches & F_TSG_OPEN_OPEN_HAL_FAIL) {
		return -EINVAL;
	}
	return 0;
}

static u32 stub_gr_init_get_no_of_sm_0(struct gk20a *g)
{
	return 0;
}

int test_tsg_open(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_fifo *f = &g->fifo;
	struct gpu_ops gops = g->ops;
	u32 num_channels = f->num_channels;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_tsg *next_tsg = NULL;
	struct nvgpu_posix_fault_inj *kmem_fi;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	u32 fail = F_TSG_OPEN_ACQUIRE_CH_FAIL |
		   F_TSG_OPEN_SM_FAIL |
		   F_TSG_OPEN_ALLOC_SM_FAIL |
		   F_TSG_OPEN_ALLOC_SM_KZALLOC_FAIL |
		   F_TSG_OPEN_ALLOC_GR_FAIL |
		   F_TSG_OPEN_INIT_BUF_FAIL |
		   F_TSG_OPEN_OPEN_HAL_FAIL;
	u32 prune = fail;
	u32 tsgid;
	const char *labels[] = {
		"acquire_ch_fail",
		"sm_fail",
		"alloc_sm_fail",
		"alloc_sm_kzalloc_fail",
		"alloc_gr_fail",
		"no_init_buf",
		"init_buf_fail",
		"no_open_hal",
		"open_hal_fail"
	};

	kmem_fi = nvgpu_kmem_get_fault_injection();

	unit_assert(nvgpu_tsg_default_timeslice_us(g) ==
		NVGPU_TSG_TIMESLICE_DEFAULT_US, goto done);

	unit_assert(nvgpu_tsg_check_and_get_from_id(g, NVGPU_INVALID_TSG_ID) ==
		NULL, goto done);

	for (branches = 0U; branches < F_TSG_OPEN_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, labels));
			continue;
		}
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
		subtest_setup(branches);

		/* find next tsg (if acquire succeeds) */
		next_tsg = NULL;
		for (tsgid = 0U; tsgid < f->num_channels; tsgid++) {
			if (!f->tsg[tsgid].in_use) {
				next_tsg = &f->tsg[tsgid];
				break;
			}
		}
		unit_assert(next_tsg != NULL, goto done);

		f->num_channels =
			branches & F_TSG_OPEN_ACQUIRE_CH_FAIL ?
			0U : num_channels;

		g->ops.gr.init.get_no_of_sm =
			branches & F_TSG_OPEN_SM_FAIL ?
			stub_gr_init_get_no_of_sm_0 :
			gops.gr.init.get_no_of_sm;

		next_tsg->sm_error_states =
			branches & F_TSG_OPEN_ALLOC_SM_FAIL ?
			(void*)1 : NULL;

		g->ops.tsg.init_eng_method_buffers =
			branches & F_TSG_OPEN_NO_INIT_BUF ?
			NULL : stub_tsg_init_eng_method_buffers;

		g->ops.tsg.open =
			branches & F_TSG_OPEN_NO_OPEN_HAL ?
			NULL : stub_tsg_open;

		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

		if (branches & F_TSG_OPEN_ALLOC_SM_KZALLOC_FAIL) {
			nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
		}

		if (branches & F_TSG_OPEN_ALLOC_GR_FAIL) {
			nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
		}

		tsg = nvgpu_tsg_open(g, getpid());

		f->tsg[tsgid].sm_error_states = NULL;

		if (branches & fail) {
			f->num_channels = num_channels;
			unit_assert(tsg == NULL, goto done);
		} else {
			unit_assert(tsg != NULL, goto done);
			unit_assert(nvgpu_tsg_get_from_id(g, tsg->tsgid) ==
				tsg, goto done);
			unit_assert(nvgpu_tsg_check_and_get_from_id(g,
				tsg->tsgid) == tsg, goto done);
			nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
			tsg = NULL;
		}
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	f->num_channels = num_channels;
	return ret;
}

#define F_TSG_BIND_CHANNEL_CH_BOUND		BIT(0)
#define F_TSG_BIND_CHANNEL_RL_MISMATCH		BIT(1)
#define F_TSG_BIND_CHANNEL_ACTIVE		BIT(2)
#define F_TSG_BIND_CHANNEL_BIND_HAL		BIT(3)
#define F_TSG_BIND_CHANNEL_BIND_HAL_ERR		BIT(4)
#define F_TSG_BIND_CHANNEL_ENG_METHOD_BUFFER	BIT(5)
#define F_TSG_BIND_CHANNEL_ASYNC_ID		BIT(6)
#define F_TSG_BIND_CHANNEL_LAST			BIT(7)

static const char *f_tsg_bind[] = {
	"ch_bound",
	"rl_mismatch",
	"active",
	"bind_hal",
	"eng_method_buffer",
};

static int stub_tsg_bind_channel(struct nvgpu_tsg *tsg,
			struct nvgpu_channel *ch)
{
	if (unit_ctx.branches & F_TSG_BIND_CHANNEL_BIND_HAL_ERR) {
		return -EINVAL;
	}

	return 0;
}

int test_tsg_bind_channel(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_tsg tsg_save;
	struct nvgpu_channel *chA = NULL;
	struct nvgpu_channel *chB = NULL;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_runlist *runlist = NULL;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	int err;
	u32 fail = F_TSG_BIND_CHANNEL_CH_BOUND |
		    F_TSG_BIND_CHANNEL_RL_MISMATCH |
		    F_TSG_BIND_CHANNEL_ACTIVE |
		    F_TSG_BIND_CHANNEL_BIND_HAL_ERR;
	u32 prune = fail;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	chA = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(chA != NULL, goto done);

	chB = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(chB != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, chA);
	unit_assert(err == 0, goto done);

	tsg_save = *tsg;

	for (branches = 0U; branches < F_TSG_BIND_CHANNEL_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, f_tsg_bind));
			continue;
		}
		subtest_setup(branches);
		ch = chB;

		if (branches & F_TSG_BIND_CHANNEL_ASYNC_ID) {
			ch->subctx_id = CHANNEL_INFO_VEID0 + 1;
		} else {
			ch->subctx_id = 0U;
		}

		/* ch already bound */
		if (branches & F_TSG_BIND_CHANNEL_CH_BOUND) {
			ch = chA;
		}

		/* runlist id mismatch */
		tsg->runlist =
			branches & F_TSG_BIND_CHANNEL_RL_MISMATCH ?
			NULL : tsg_save.runlist;

		/* ch already already active */
		runlist = tsg->runlist;
		if (branches & F_TSG_BIND_CHANNEL_ACTIVE) {
			nvgpu_set_bit(ch->chid, runlist->domain->active_channels);
		} else {
			nvgpu_clear_bit(ch->chid, runlist->domain->active_channels);
		}

		if ((branches & F_TSG_BIND_CHANNEL_BIND_HAL) ||
		    (branches & F_TSG_BIND_CHANNEL_BIND_HAL_ERR)) {
			g->ops.tsg.bind_channel = stub_tsg_bind_channel;
		} else {
			g->ops.tsg.bind_channel = NULL;
		}

		g->ops.tsg.bind_channel_eng_method_buffers =
			branches & F_TSG_BIND_CHANNEL_ENG_METHOD_BUFFER ?
			gops.tsg.bind_channel_eng_method_buffers : NULL;

		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_bind));

		err = nvgpu_tsg_bind_channel(tsg, ch);

		if (branches & fail) {
			if (!(branches & F_TSG_BIND_CHANNEL_CH_BOUND)) {
				unit_assert(nvgpu_tsg_from_ch(ch) == NULL,
					goto done);
			}
			unit_assert(err != 0, goto done);
		} else {
			unit_assert(err == 0, goto done);
			unit_assert(!nvgpu_list_empty(&tsg->ch_list),
				goto done);
			unit_assert(nvgpu_tsg_from_ch(ch) == tsg, goto done);

			err = nvgpu_tsg_unbind_channel(tsg, ch, true);
			unit_assert(err == 0, goto done);
			unit_assert(ch->tsgid == NVGPU_INVALID_TSG_ID,
				goto done);
		}
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_bind));
	}
	if (chA != NULL) {
		nvgpu_channel_close(chA);
	}
	if (chB != NULL) {
		nvgpu_channel_close(chB);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	return ret;
}

#define F_TSG_UNBIND_CHANNEL_ABORT_RUNLIST_UPDATE_FAIL	BIT(0)
#define F_TSG_UNBIND_CHANNEL_UNSERVICEABLE		BIT(1)
#define F_TSG_UNBIND_CHANNEL_PREEMPT_TSG_FAIL		BIT(2)
#define F_TSG_UNBIND_CHANNEL_CHECK_HW_STATE		BIT(3)
#define F_TSG_UNBIND_CHANNEL_CHECK_HW_STATE_FAIL	BIT(4)
#define F_TSG_UNBIND_CHANNEL_RUNLIST_UPDATE_FAIL	BIT(5)
#define F_TSG_UNBIND_CHANNEL_UNBIND_HAL			BIT(6)
#define F_TSG_UNBIND_CHANNEL_UNBIND_HAL_FAIL		BIT(7)
#define F_TSG_UNBIND_CHANNEL_ABORT_CLEAN_UP_NULL	BIT(8)
#define F_TSG_UNBIND_CHANNEL_LAST			BIT(9)

static int stub_fifo_preempt_tsg_EINVAL(
		struct gk20a *g, struct nvgpu_tsg *tsg)
{
	return -EINVAL;
}

static int stub_tsg_unbind_channel_check_hw_state(
		struct nvgpu_tsg *tsg, struct nvgpu_channel *ch)
{
	if (unit_ctx.branches & F_TSG_UNBIND_CHANNEL_CHECK_HW_STATE_FAIL) {
		return -EINVAL;
	}

	return 0;
}

static int stub_tsg_unbind_channel(struct nvgpu_tsg *tsg,
		struct nvgpu_channel *ch)
{
	if (unit_ctx.branches & F_TSG_UNBIND_CHANNEL_UNBIND_HAL_FAIL) {
		return -EINVAL;
	}

	return 0;
}

static int stub_runlist_update_EINVAL(
		struct gk20a *g, struct nvgpu_runlist *rl,
		struct nvgpu_channel *ch, bool add, bool wait_for_finish)
{
	stub[0].count++;
	if (stub[0].count == 1 && (unit_ctx.branches &
			F_TSG_UNBIND_CHANNEL_RUNLIST_UPDATE_FAIL)) {
		return -EINVAL;
	}
	if (stub[0].count == 2 && (unit_ctx.branches &
			F_TSG_UNBIND_CHANNEL_ABORT_RUNLIST_UPDATE_FAIL)) {
		return -EINVAL;
	}
	return 0;
}

int test_tsg_unbind_channel(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *chA = NULL;
	struct nvgpu_channel *chB = NULL;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	int err;
	const char *labels[] = {
		"abort_runlist_update_fail",
		"unserviceable",
		"preempt_tsg_fail",
		"check_hw_state",
		"check_hw_state_fail",
		"runlist_update_fail",
		"unbind_hal",
		"unbind_hal_fail",
		"abort_cleanup_null"
	};
	u32 fail =
		F_TSG_UNBIND_CHANNEL_PREEMPT_TSG_FAIL |
		F_TSG_UNBIND_CHANNEL_CHECK_HW_STATE_FAIL |
		F_TSG_UNBIND_CHANNEL_RUNLIST_UPDATE_FAIL |
		F_TSG_UNBIND_CHANNEL_UNBIND_HAL_FAIL;

	/*
	 * do not prune F_TSG_UNBIND_CHANNEL_UNBIND_HAL_FAIL, to
	 * exercise following abort path.
	 */
	u32 prune =
		F_TSG_UNBIND_CHANNEL_PREEMPT_TSG_FAIL |
		F_TSG_UNBIND_CHANNEL_CHECK_HW_STATE_FAIL |
		F_TSG_UNBIND_CHANNEL_RUNLIST_UPDATE_FAIL;

	for (branches = 0U; branches < F_TSG_UNBIND_CHANNEL_LAST; branches++) {
		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, labels));
			continue;
		}

		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));

		/*
		 * tsg unbind tears down TSG in case of failure:
		 * we need to create tsg + bind channel for each test
		 */
		tsg = nvgpu_tsg_open(g, getpid());
		unit_assert(tsg != NULL, goto done);

		chA = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
		unit_assert(chA != NULL, goto done);

		chB = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
		unit_assert(chB != NULL, goto done);

		err = nvgpu_tsg_bind_channel(tsg, chA);
		unit_assert(err == 0, goto done);

		err = nvgpu_tsg_bind_channel(tsg, chB);
		unit_assert(err == 0, goto done);

		chA->unserviceable =
			branches & F_TSG_UNBIND_CHANNEL_UNSERVICEABLE ?
			true : false;

		g->ops.fifo.preempt_tsg =
			branches & F_TSG_UNBIND_CHANNEL_PREEMPT_TSG_FAIL ?
			stub_fifo_preempt_tsg_EINVAL :
			gops.fifo.preempt_tsg;

		if ((branches & F_TSG_UNBIND_CHANNEL_CHECK_HW_STATE) ||
		    (branches & F_TSG_UNBIND_CHANNEL_CHECK_HW_STATE_FAIL)) {
			g->ops.tsg.unbind_channel_check_hw_state =
				stub_tsg_unbind_channel_check_hw_state;
		} else {
			g->ops.tsg.unbind_channel_check_hw_state = NULL;
		}

		g->ops.runlist.update =
			branches & F_TSG_UNBIND_CHANNEL_RUNLIST_UPDATE_FAIL ?
			stub_runlist_update_EINVAL :
			gops.runlist.update;

		if (branches & F_TSG_UNBIND_CHANNEL_RUNLIST_UPDATE_FAIL ||
		    branches & F_TSG_UNBIND_CHANNEL_ABORT_RUNLIST_UPDATE_FAIL) {
			g->ops.runlist.update =
				stub_runlist_update_EINVAL;
		}

		if ((branches & F_TSG_UNBIND_CHANNEL_UNBIND_HAL) ||
		    (branches & F_TSG_UNBIND_CHANNEL_UNBIND_HAL_FAIL)) {
			g->ops.tsg.unbind_channel = stub_tsg_unbind_channel;
		} else {
			g->ops.tsg.unbind_channel = NULL;
		}

		g->ops.channel.abort_clean_up =
			branches & F_TSG_UNBIND_CHANNEL_ABORT_CLEAN_UP_NULL ?
			NULL : gops.channel.abort_clean_up;

		err = nvgpu_tsg_unbind_channel(tsg, chA, true);

		if (branches & fail) {
			/* check that TSG has been torn down */
			unit_assert(err != 0, goto done);
			unit_assert(chA->unserviceable, goto done);
			unit_assert(chB->unserviceable, goto done);
			unit_assert(chA->tsgid == NVGPU_INVALID_TSG_ID,
				goto done);
		} else {

			if (branches & F_TSG_UNBIND_CHANNEL_ABORT_CLEAN_UP_NULL) {
				gops.channel.abort_clean_up(chA);
			}

			unit_assert(chA->tsgid == NVGPU_INVALID_TSG_ID,
				goto done);
			unit_assert(nvgpu_list_empty(&chA->ch_entry),
				goto done);
			/* check that TSG has not been torn down */
			unit_assert(!chB->unserviceable, goto done);
			unit_assert(!nvgpu_list_empty(&chB->ch_entry),
				goto done);
			unit_assert(!nvgpu_list_empty(&tsg->ch_list),
				goto done);
		}

		nvgpu_channel_close(chA);
		nvgpu_channel_close(chB);
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
		chA = NULL;
		chB = NULL;
		tsg = NULL;
	}

	ret = UNIT_SUCCESS;

done:
	if (ret == UNIT_FAIL) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
	}
	if (chA != NULL) {
		nvgpu_channel_close(chA);
	}
	if (chB != NULL) {
		nvgpu_channel_close(chB);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	return ret;
}

#define F_TSG_RELEASE_NO_RELEASE_HAL	BIT(0)
#define F_TSG_RELEASE_GR_CTX		BIT(1)
#define F_TSG_RELEASE_MEM		BIT(2)
#define F_TSG_RELEASE_VM		BIT(3)
#define F_TSG_RELEASE_ENG_BUFS		BIT(4)
#define F_TSG_RELEASE_SM_ERR_STATES	BIT(5)
#define F_TSG_RELEASE_LAST		BIT(6)


static void stub_tsg_release(struct nvgpu_tsg *tsg)
{
}

static void stub_tsg_deinit_eng_method_buffers(struct gk20a *g,
		struct nvgpu_tsg *tsg)
{
	stub[0].name = __func__;
	stub[0].tsgid = tsg->tsgid;
}

static void stub_gr_setup_free_gr_ctx(struct gk20a *g,
		struct vm_gk20a *vm, struct nvgpu_gr_ctx *gr_ctx)
{
	stub[1].name = __func__;
	stub[1].count++;
}


int test_tsg_release(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_fifo *f = &g->fifo;
	struct gpu_ops gops = g->ops;
	struct nvgpu_tsg *tsg = NULL;
	struct vm_gk20a vm;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	struct nvgpu_mem mem;
	u32 free_gr_ctx_mask =
		F_TSG_RELEASE_GR_CTX|F_TSG_RELEASE_MEM|F_TSG_RELEASE_VM;
	const char *labels[] = {
		"no_release_hal",
		"gr_ctx",
		"mem",
		"vm",
		"eng_bufs",
		"sm_err_states"
	};

	for (branches = 0U; branches < F_TSG_RELEASE_LAST; branches++) {

		if (!(branches & F_TSG_RELEASE_GR_CTX) &&
				(branches & F_TSG_RELEASE_MEM)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, labels));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));

		tsg = nvgpu_tsg_open(g, getpid());
		unit_assert(tsg != NULL, goto done);
		unit_assert(tsg->gr_ctx != NULL, goto done);
		unit_assert(tsg->gr_ctx->mem.aperture ==
				APERTURE_INVALID, goto done);

		g->ops.tsg.release =
			branches & F_TSG_RELEASE_NO_RELEASE_HAL ?
			NULL : stub_tsg_release;

		if (!(branches & F_TSG_RELEASE_GR_CTX)) {
			nvgpu_free_gr_ctx_struct(g, tsg->gr_ctx);
			tsg->gr_ctx = NULL;
		}

		if (branches & F_TSG_RELEASE_MEM) {
			nvgpu_dma_alloc(g, NVGPU_CPU_PAGE_SIZE, &mem);
			tsg->gr_ctx->mem = mem;
		}

		if (branches & F_TSG_RELEASE_VM) {
			tsg->vm = &vm;
			/* prevent nvgpu_vm_remove */
			nvgpu_ref_init(&vm.ref);
			nvgpu_ref_get(&vm.ref);
		} else {
			tsg->vm = NULL;
		}

		if ((branches & free_gr_ctx_mask) == free_gr_ctx_mask) {
			g->ops.gr.setup.free_gr_ctx =
				stub_gr_setup_free_gr_ctx;
		}

		g->ops.tsg.deinit_eng_method_buffers =
			branches & F_TSG_RELEASE_ENG_BUFS ?
			stub_tsg_deinit_eng_method_buffers : NULL;

		if (branches & F_TSG_RELEASE_SM_ERR_STATES) {
			unit_assert(tsg->sm_error_states != NULL, goto done);
		} else {
			nvgpu_kfree(g, tsg->sm_error_states);
			tsg->sm_error_states = NULL;
		}

		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);

		if ((branches & free_gr_ctx_mask) == free_gr_ctx_mask) {
			unit_assert(tsg->gr_ctx == NULL, goto done);
		} else {
			g->ops.gr.setup.free_gr_ctx =
				gops.gr.setup.free_gr_ctx;

			if (branches & F_TSG_RELEASE_MEM) {
				nvgpu_dma_free(g, &mem);
			}

			if (tsg->gr_ctx != NULL) {
				nvgpu_free_gr_ctx_struct(g, tsg->gr_ctx);
				tsg->gr_ctx = NULL;
			}
			unit_assert(stub[1].count == 0, goto done);
		}

		if (branches & F_TSG_RELEASE_ENG_BUFS) {
			unit_assert(stub[0].tsgid == tsg->tsgid, goto done);
		}

		unit_assert(!f->tsg[tsg->tsgid].in_use, goto done);
		unit_assert(tsg->gr_ctx == NULL, goto done);
		unit_assert(tsg->vm == NULL, goto done);
		unit_assert(tsg->sm_error_states == NULL, goto done);
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
	}
	g->ops = gops;
	return ret;
}

#define F_TSG_UNBIND_CHANNEL_CHECK_HW_NEXT		BIT(0)
#define F_TSG_UNBIND_CHANNEL_CHECK_HW_NEXT_CLR		BIT(1)
#define F_TSG_UNBIND_CHANNEL_CHECK_HW_CTX_RELOAD	BIT(2)
#define F_TSG_UNBIND_CHANNEL_CHECK_HW_ENG_FAULTED	BIT(3)
#define F_TSG_UNBIND_CHANNEL_CHECK_HW_LAST		BIT(4)

static const char *f_tsg_unbind_channel_check_hw[] = {
	"next",
	"next clear",
	"ctx_reload",
	"eng_faulted",
};

static void stub_channel_read_state_NEXT(struct gk20a *g,
		struct nvgpu_channel *ch, struct nvgpu_channel_hw_state *state)
{
	state->next = true;
}

static void stub_channel_read_state_NEXT_CLR(struct gk20a *g,
		struct nvgpu_channel *ch, struct nvgpu_channel_hw_state *state)
{
	state->next = false;
}

int test_tsg_unbind_channel_check_hw_state(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	int err;
	u32 prune = F_TSG_UNBIND_CHANNEL_CHECK_HW_NEXT;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	for (branches = 0; branches < F_TSG_UNBIND_CHANNEL_CHECK_HW_LAST;
			branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches,
					f_tsg_unbind_channel_check_hw));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_unbind_channel_check_hw));

		if (branches & F_TSG_UNBIND_CHANNEL_CHECK_HW_NEXT) {
			g->ops.channel.read_state =
				stub_channel_read_state_NEXT;
		} else if (branches & F_TSG_UNBIND_CHANNEL_CHECK_HW_NEXT_CLR) {
			g->ops.channel.read_state =
				stub_channel_read_state_NEXT_CLR;
		} else {
			g->ops.channel.read_state =  gops.channel.read_state;
		}

		g->ops.tsg.unbind_channel_check_ctx_reload =
			branches & F_TSG_UNBIND_CHANNEL_CHECK_HW_CTX_RELOAD ?
			gops.tsg.unbind_channel_check_ctx_reload : NULL;

		g->ops.tsg.unbind_channel_check_eng_faulted =
			branches & F_TSG_UNBIND_CHANNEL_CHECK_HW_ENG_FAULTED ?
			gops.tsg.unbind_channel_check_eng_faulted : NULL;

		err = nvgpu_tsg_unbind_channel_check_hw_state(tsg, ch);

		if (branches & F_TSG_UNBIND_CHANNEL_CHECK_HW_NEXT) {
			unit_assert(err != 0, goto done);
		} else {
			unit_assert(err == 0, goto done);
		}
	}
	ret = UNIT_SUCCESS;

done:
	if (ret == UNIT_FAIL) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_unbind_channel_check_hw));
	}
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	return ret;
}

int test_tsg_sm_error_state_set_get(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	int ret = UNIT_FAIL;
	int err = 0;
	int num_sm = g->ops.gr.init.get_no_of_sm(g);
	u32 valid_sm_id[][2] = {{0, num_sm - 1}};
	u32 invalid_sm_id[][2] = {{num_sm, U32_MAX}};
	u32 i = 0, j = 0, sm_id_range, states, sm_id, t = 0, z = 0;
	u32 (*working_list)[2];
	const char *string_states[] = {"Min", "Max", "Mid"};
	struct nvgpu_tsg_sm_error_state *sm_error_states = NULL;
	const struct nvgpu_tsg_sm_error_state *get_error_state = NULL;
	u32 sm_error_states_values[] = {0, 0, 0, 0};
	u64 hww_warp_esr_pc = 0;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	sm_error_states = tsg->sm_error_states;

	//check for SM_ERROR_STATE null
	tsg->sm_error_states = NULL;
	err = nvgpu_tsg_store_sm_error_state(tsg, 0, 0, 0, 0, 0, 0);
	unit_assert(err != 0, goto done);

	tsg->sm_error_states = sm_error_states;
	err = nvgpu_tsg_store_sm_error_state(tsg, 0, 0, 0, 0, 0, 0);
	unit_assert(err == 0, goto done);

	//check for SM_ERROR_STATE null
	tsg->sm_error_states = NULL;
	get_error_state = nvgpu_tsg_get_sm_error_state(tsg, 0);
	unit_assert(get_error_state == NULL, goto done);
	tsg->sm_error_states = sm_error_states;

	/* valid, invalid sm_ids */
	for (i = 0; i < 2; i++) {
		working_list = (i == 0) ? valid_sm_id : invalid_sm_id;
		sm_id_range = (i == 0) ? ARRAY_SIZE(valid_sm_id) : ARRAY_SIZE(invalid_sm_id);
		for (j = 0; j < sm_id_range; j++) {
			for (states = 0; states < 3; states++) {
				if (states == 0) {
					sm_id = working_list[j][0];
				} else if (states == 1) {
					sm_id = working_list[j][1];
				} else {
					if (working_list[j][1] - working_list[j][0] > 1) {
						sm_id = get_random_u32(working_list[j][0] + 1, working_list[j][1] - 1);
					} else {
						continue;
					}
				}

				/* Invalid SM_ID case */
				if (i == 1) {
					unit_info(m, "BVEC testing for nvgpu_tsg_store_sm_error_state with sm_id = 0x%08x(Invalid range %s) \n", sm_id, string_states[states]);
					err = nvgpu_tsg_store_sm_error_state(tsg, sm_id, 0, 0, 0, 0, 0);
					unit_assert(err != 0, goto done);

					unit_info(m, "BVEC testing for nvgpu_tsg_get_sm_error_state with sm_id = 0x%08x(Invalid range %s) \n", sm_id, string_states[states]);
					get_error_state = nvgpu_tsg_get_sm_error_state(tsg, sm_id);
					unit_assert(get_error_state == NULL, goto done);
				} else {
					for (t = 0; t < 3; t++) {
						/* Loop to fill the SM error values */
						for (z = 0; z < 4; z++) {
							if (t == 0) {
								/* Default 0*/
							} else if (t == 1) {
								sm_error_states_values[z] = U32_MAX;
								hww_warp_esr_pc = U32_MAX;
							} else {
								sm_error_states_values[z] = get_random_u32(1, U32_MAX - 1);
								hww_warp_esr_pc = 2ULL * U32_MAX;
							}
						}

						unit_info(m, "BVEC testing for nvgpu_tsg_store_sm_error_state with sm_id = 0x%08x(Valid range %s)\n", sm_id, string_states[t]);
						unit_info(m, "hww_global_esr = 0x%08x\n", sm_error_states_values[0]);
						unit_info(m, "hww_warp_esr = 0x%08x\n", sm_error_states_values[1]);
						unit_info(m, "hww_warp_esr_pc = 0x%016llx\n", hww_warp_esr_pc);
						unit_info(m, "hww_global_esr_report_mask = 0x%08x\n", sm_error_states_values[2]);
						unit_info(m, "hww_warp_esr_report_mask = 0x%08x\n", sm_error_states_values[3]);

						err = nvgpu_tsg_store_sm_error_state(tsg, sm_id,
							sm_error_states_values[0], sm_error_states_values[1], hww_warp_esr_pc,
							sm_error_states_values[2], sm_error_states_values[3]);
						unit_assert(err == 0, goto done);

						unit_info(m, "BVEC testing for nvgpu_tsg_get_sm_error_state with sm_id = %u(Valid range %s) \n", sm_id, string_states[t]);
						get_error_state = nvgpu_tsg_get_sm_error_state(tsg, sm_id);
						unit_assert(get_error_state != NULL, goto done);

						unit_assert(get_error_state->hww_global_esr == sm_error_states_values[0], goto done);
						unit_assert(get_error_state->hww_warp_esr == sm_error_states_values[1], goto done);
						unit_assert(get_error_state->hww_warp_esr_pc == hww_warp_esr_pc, goto done);
						unit_assert(get_error_state->hww_global_esr_report_mask == sm_error_states_values[2], goto done);
						unit_assert(get_error_state->hww_warp_esr_report_mask == sm_error_states_values[3], goto done);
					}
				}
			}
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret == UNIT_FAIL) {
		unit_err(m, "branches=%s\n", __func__);
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

#define F_UNBIND_CHANNEL_CHECK_CTX_RELOAD_SET		BIT(0)
#define F_UNBIND_CHANNEL_CHECK_CTX_RELOAD_CHID_MATCH	BIT(1)
#define F_UNBIND_CHANNEL_CHECK_CTX_RELOAD_LAST		BIT(2)

static const char *f_unbind_channel_check_ctx_reload[] = {
	"reload_set",
	"chid_match",
};

static void stub_channel_force_ctx_reload(struct nvgpu_channel *ch)
{
	stub[0].name = __func__;
	stub[0].chid = ch->chid;
}

int test_tsg_unbind_channel_check_ctx_reload(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	struct nvgpu_channel_hw_state hw_state;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *chA = NULL;
	struct nvgpu_channel *chB = NULL;
	int err;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	chA = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(chA != NULL, goto done);

	chB = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(chB != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, chA);
	unit_assert(err == 0, goto done);

	g->ops.channel.force_ctx_reload = stub_channel_force_ctx_reload;

	for (branches = 0; branches < F_UNBIND_CHANNEL_CHECK_CTX_RELOAD_LAST;
			branches++) {

		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches,
				f_unbind_channel_check_ctx_reload));

		hw_state.ctx_reload =
			branches & F_UNBIND_CHANNEL_CHECK_CTX_RELOAD_SET ?
			true : false;

		if ((branches & F_UNBIND_CHANNEL_CHECK_CTX_RELOAD_SET) &&
		    (branches & F_UNBIND_CHANNEL_CHECK_CTX_RELOAD_CHID_MATCH)) {
			unit_assert(nvgpu_tsg_bind_channel(tsg, chB) == 0,
				goto done);
		}

		nvgpu_tsg_unbind_channel_check_ctx_reload(tsg, chA, &hw_state);

		if ((branches & F_UNBIND_CHANNEL_CHECK_CTX_RELOAD_SET) &&
		    (branches & F_UNBIND_CHANNEL_CHECK_CTX_RELOAD_CHID_MATCH)) {
			nvgpu_tsg_unbind_channel(tsg, chB, true);
			unit_assert(stub[0].chid == chB->chid, goto done);
		}
	}
	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches,
				f_unbind_channel_check_ctx_reload));
	}
	if (chA != NULL) {
		nvgpu_channel_close(chA);
	}
	if (chB != NULL) {
		nvgpu_channel_close(chB);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	return ret;
}

#define F_TSG_ENABLE_CH			BIT(0)
#define F_TSG_ENABLE_STUB		BIT(1)
#define F_TSG_ENABLE_LAST		BIT(2)

static const char *f_tsg_enable[] = {
	"ch",
	"stub"
};

static void stub_channel_enable(struct nvgpu_channel *ch)
{
	stub[0].name = __func__;
	stub[0].chid = ch->chid;
	stub[0].count++;
}

static void stub_usermode_ring_doorbell(struct nvgpu_channel *ch)
{
	stub[1].name = __func__;
	stub[1].chid = ch->chid;
	stub[1].count++;
}

static void stub_channel_disable(struct nvgpu_channel *ch)
{
	stub[2].name = __func__;
	stub[2].chid = ch->chid;
	stub[2].count++;
}

int test_tsg_enable(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_tsg *tsgA = NULL;
	struct nvgpu_tsg *tsgB = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *chA = NULL;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	int err;

	tsgA = nvgpu_tsg_open(g, getpid());
	unit_assert(tsgA != NULL, goto done);

	tsgB = nvgpu_tsg_open(g, getpid());
	unit_assert(tsgB != NULL, goto done);

	chA = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(chA != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsgA, chA);
	unit_assert(err == 0, goto done);

	g->ops.channel.disable = stub_channel_disable;

	for (branches = 0U; branches < F_TSG_ENABLE_LAST; branches++) {

		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_enable));

		tsg = branches & F_TSG_ENABLE_CH ?
			tsgA : tsgB;

		g->ops.channel.enable =
			branches & F_TSG_ENABLE_STUB ?
			stub_channel_enable : gops.channel.enable;

		g->ops.usermode.ring_doorbell =
			branches & F_TSG_ENABLE_STUB ?
			stub_usermode_ring_doorbell :
			gops.usermode.ring_doorbell;

		g->ops.tsg.enable(tsg);

		if (branches & F_TSG_ENABLE_STUB) {
			if (tsg == tsgB) {
				unit_assert(stub[0].count == 0, goto done);
				unit_assert(stub[1].count == 0, goto done);
			}

			if (tsg == tsgA) {
				unit_assert(stub[0].chid == chA->chid,
					goto done);
				unit_assert(stub[1].count > 0, goto done);
			}
		}

		g->ops.channel.disable =
			branches & F_TSG_ENABLE_STUB ?
			stub_channel_disable : gops.channel.disable;

		g->ops.tsg.disable(tsg);

		if (branches & F_TSG_ENABLE_STUB) {
			if (tsg == tsgB) {
				unit_assert(stub[2].count == 0, goto done);
			}

			if (tsg == tsgA) {
				unit_assert(stub[2].chid == chA->chid,
					goto done);
			}
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_enable));
	}
	if (chA != NULL) {
		nvgpu_channel_close(chA);
	}
	if (tsgA != NULL) {
		nvgpu_ref_put(&tsgA->refcount, nvgpu_tsg_release);
	}
	if (tsgB != NULL) {
		nvgpu_ref_put(&tsgB->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	return ret;
}

int test_tsg_check_and_get_from_id(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_tsg *tsg;
	int ret = UNIT_FAIL;

	tsg = nvgpu_tsg_check_and_get_from_id(g, NVGPU_INVALID_TSG_ID);
	unit_assert(tsg == NULL, goto done);

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	unit_assert(nvgpu_tsg_check_and_get_from_id(g, tsg->tsgid) == tsg,
		goto done);
	nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_tsg_check_and_get_from_id_bvec(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_fifo *f = &g->fifo;
	int ret = UNIT_FAIL;
	u32 valid_tsg_ids[][2] = {{0, f->num_channels - 1}};
	u32 invalid_tsg_ids[][2] = {{f->num_channels, U32_MAX}};
	u32 tsgid, tsgid_range_len;
	u32 (*working_list)[2];
	/*
	 * i is to loop through valid and invalid cases
	 * j is to loop through different ranges within ith case
	 * states is for min, max and median
	 */
	u32 i, j, states;
	const char *string_cases[] = {"Valid", "Invalid"};
	const char *string_states[] = {"Min", "Max", "Mid"};
	u32 tsgid_range_difference;

	/* loop through valid and invalid cases */
	for (i = 0; i < 2; i++) {
		/* select appropriate iteration size */
		tsgid_range_len = (i == 0) ? ARRAY_SIZE(valid_tsg_ids) : ARRAY_SIZE(invalid_tsg_ids);
		/* select correct working list */
		working_list =  (i == 0) ? valid_tsg_ids : invalid_tsg_ids;
		for (j = 0; j < tsgid_range_len; j++) {
			for (states = 0; states < 3; states++) {
				/* check for min tsgid */
				if (states == 0)
					tsgid = working_list[j][0];
				else if (states == 1) {
					/* check for max tsgid */
					tsgid = working_list[j][1];
				} else {
					tsgid_range_difference = working_list[j][1] - working_list[j][0];
					/* Check for random tsgid in range */
					if (tsgid_range_difference > 1)
						tsgid = get_random_u32(working_list[j][0] + 1, working_list[j][1] - 1);
					else
						continue;
				}

				unit_info(m, "BVEC testing for nvgpu_tsg_check_and_get_from_id with tsgid =  0x%08x(%s range [0x%08x - 0x%08x] %s)\n", tsgid, string_cases[i], working_list[j][0], working_list[j][1], string_states[states]);

				if (i == 0)
					unit_assert(nvgpu_tsg_check_and_get_from_id(g, tsgid) != NULL, goto done);
				else
					unit_assert(nvgpu_tsg_check_and_get_from_id(g,tsgid) == NULL, goto done);
			}
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	return ret;
}

#define F_TSG_ABORT_CH_ABORT_CLEANUP_NULL	BIT(0)
#define F_TSG_ABORT_PREEMPT			BIT(1)
#define F_TSG_ABORT_CH				BIT(2)
#define F_TSG_ABORT_NON_ABORTABLE		BIT(3)
#define F_TSG_ABORT_CH_NON_REFERENCABLE		BIT(4)
#define F_TSG_ABORT_LAST			BIT(5)

static const char *f_tsg_abort[] = {
	"preempt",
	"ch",
	"ch_abort_cleanup_null",
	"non_abortable",
	"non_referenceable"
};

static int stub_fifo_preempt_tsg(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	stub[0].tsgid = tsg->tsgid;
	return 0;
}

static void stub_channel_abort_clean_up(struct nvgpu_channel *ch)
{
	stub[1].chid = ch->chid;
}

int test_tsg_abort(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_tsg *tsgA = NULL;
	struct nvgpu_tsg *tsgB = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *chA = NULL;
	bool preempt = false;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	u32 prune = F_TSG_ABORT_NON_ABORTABLE;
	int err;

	tsgA = nvgpu_tsg_open(g, getpid());
	unit_assert(tsgA != NULL, goto done);

	tsgB = nvgpu_tsg_open(g, getpid());
	unit_assert(tsgB != NULL, goto done);

	chA = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(chA != NULL, goto done);

	for (branches = 0U; branches < F_TSG_ABORT_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, f_tsg_abort));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_abort));

		g->ops.channel.abort_clean_up =
			branches & F_TSG_ABORT_CH_ABORT_CLEANUP_NULL ?
			NULL : stub_channel_abort_clean_up;

		g->ops.fifo.preempt_tsg = stub_fifo_preempt_tsg;

		tsg = branches & F_TSG_ABORT_CH ? tsgA : tsgB;

		tsg->abortable =
			branches & F_TSG_ABORT_NON_ABORTABLE ? false : true;

		preempt = branches & F_TSG_ABORT_PREEMPT ? true : false;

		if (branches & F_TSG_ABORT_CH_ABORT_CLEANUP_NULL) {
			g->ops.channel.abort_clean_up = NULL;
		}

		if (branches & F_TSG_ABORT_CH_NON_REFERENCABLE) {
			chA->referenceable = false;
		}

		if (chA->tsgid == NVGPU_INVALID_TSG_ID) {
			err = nvgpu_tsg_bind_channel(tsgA, chA);
			unit_assert(err == 0, goto done);
		}

		nvgpu_tsg_abort(g, tsg, preempt);

		unit_assert(preempt == (stub[0].tsgid == tsg->tsgid),
			goto done);

		unit_assert(chA->unserviceable ==
			((tsg == tsgA) && (chA->referenceable)), goto done);

		if (!((branches & F_TSG_ABORT_CH_ABORT_CLEANUP_NULL) ||
		      (branches & F_TSG_ABORT_CH_NON_REFERENCABLE))) {
			unit_assert((stub[1].chid == chA->chid) ==
				(tsg == tsgA), goto done);
			unit_assert((stub[1].chid ==
				NVGPU_INVALID_CHANNEL_ID) == (tsg == tsgB),
				goto done);
		}

		tsg->abortable = true;
		chA->unserviceable = false;
		chA->referenceable = true;
	}

	ret = UNIT_SUCCESS;

done:
	if (ret == UNIT_FAIL) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_abort));
	}
	if (chA != NULL) {
		nvgpu_channel_close(chA);
	}
	if (tsgA != NULL) {
		nvgpu_ref_put(&tsgA->refcount, nvgpu_tsg_release);
	}
	if (tsgB != NULL) {
		nvgpu_ref_put(&tsgB->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	return ret;
}

#define F_TSG_SETUP_SW_VZALLOC_FAIL		BIT(0)
#define F_TSG_SETUP_SW_LAST			BIT(1)

static const char *f_tsg_setup_sw[] = {
	"vzalloc_fail",
};

int test_tsg_setup_sw(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_posix_fault_inj *kmem_fi;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	int err;
	u32 fail = F_TSG_SETUP_SW_VZALLOC_FAIL;
	u32 prune = fail;

	kmem_fi = nvgpu_kmem_get_fault_injection();

	for (branches = 0U; branches < F_TSG_SETUP_SW_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, f_tsg_setup_sw));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_setup_sw));

		nvgpu_posix_enable_fault_injection(kmem_fi,
			branches & F_TSG_SETUP_SW_VZALLOC_FAIL ?
			true : false, 0);

		err = nvgpu_tsg_setup_sw(g);

		if (branches & fail) {
			unit_assert(err != 0, goto done);
		} else {
			unit_assert(err == 0, goto done);
			nvgpu_tsg_cleanup_sw(g);
		}
	}

	ret = UNIT_SUCCESS;
done:
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, f_tsg_setup_sw));
	}
	g->ops = gops;
	return ret;
}

#define F_TSG_MARK_ERROR_NO_CHANNEL		BIT(0)
#define F_TSG_MARK_ERROR_NON_REFERENCABLE	BIT(1)
#define F_TSG_MARK_ERROR_VERBOSE		BIT(2)
#define F_TSG_MARK_ERROR_LAST			BIT(3)

int test_tsg_mark_error(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *ch = NULL;
	int ret = UNIT_FAIL;
	bool verbose;
	int err;
	u32 branches;
	static const char *labels[] = {
		"no_channel",
		"non_referencable",
		"verbose",
	};
	u32 prune =
		F_TSG_MARK_ERROR_NO_CHANNEL |
		F_TSG_MARK_ERROR_NON_REFERENCABLE;
	struct nvgpu_posix_channel ch_priv;

	for (branches = 0U; branches < F_TSG_MARK_ERROR_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, labels));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));

		tsg = nvgpu_tsg_open(g, getpid());
		unit_assert(tsg != NULL, goto done);

		ch = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
		unit_assert(ch != NULL, goto done);

		ch->os_priv = &ch_priv;
		ch_priv.err_notifier.error = U32_MAX;
		ch_priv.err_notifier.status = 0;

		if ((branches & F_TSG_MARK_ERROR_NO_CHANNEL) == 0) {
			err = nvgpu_tsg_bind_channel(tsg, ch);
			unit_assert(err == 0, goto done);
		}

		if (branches & F_TSG_MARK_ERROR_NON_REFERENCABLE) {
			ch->referenceable = false;
		}

		ch->ctxsw_timeout_debug_dump =
			branches & F_TSG_MARK_ERROR_VERBOSE ? true : false;

		nvgpu_tsg_set_error_notifier(g, tsg,
			NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT);

		verbose = nvgpu_tsg_mark_error(g, tsg);

		if ((branches & F_TSG_MARK_ERROR_NO_CHANNEL) ||
		    (branches & F_TSG_MARK_ERROR_NON_REFERENCABLE)) {
			unit_assert(!verbose, goto done);
		}

		if (branches & F_TSG_MARK_ERROR_VERBOSE) {
			unit_assert(verbose, goto done);
		} else {
			unit_assert(!verbose, goto done);
		}

		nvgpu_channel_close(ch);
		ch = NULL;
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
		tsg = NULL;
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
	}
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	return ret;
}

int test_nvgpu_tsg_set_error_notifier_bvec(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *ch = NULL;
	int ret = 0;

	u32 valid_error_notifier_ids[][2] = {{NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT, NVGPU_ERR_NOTIFIER_PBDMA_PUSHBUFFER_CRC_MISMATCH}};
	u32 invalid_error_notifier_ids[][2] = {{NVGPU_ERR_NOTIFIER_PBDMA_PUSHBUFFER_CRC_MISMATCH + 1, U32_MAX}};
	u32 (*working_list)[2];
	u32 error_code, error_notifier_range_len;
	/*
	 * i is to loop through valid and invalid cases
	 * j is to loop through different ranges within ith case
	 * states is for min, max and median
	 */
	u32 i, j, states;
	const char *string_cases[] = {"Valid", "Invalid"};
	const char *string_states[] = {"Min", "Max", "Mid"};
	u32 tsgid_range_difference;

	struct nvgpu_posix_channel ch_priv;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	ch->os_priv = &ch_priv;
	ch_priv.err_notifier.error = 0U;

	ret = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(ret == 0, goto done);

	ret = UNIT_FAIL;

	/* loop through valid and invalid cases */
	for (i = 0; i < 2; i++) {
		/* select appropriate iteration size */
		error_notifier_range_len = (i == 0) ? ARRAY_SIZE(valid_error_notifier_ids) : ARRAY_SIZE(invalid_error_notifier_ids);
		/* select correct working list */
		working_list = (i == 0) ? valid_error_notifier_ids : invalid_error_notifier_ids;
		for (j = 0; j < error_notifier_range_len; j++) {
			for (states = 0; states < 3; states++) {
				/* check for min error code */
				if (states == 0)
					error_code = working_list[j][0];
				else if (states == 1) {
					/* check for max error code */
					error_code = working_list[j][1];
				} else {
					tsgid_range_difference = working_list[j][1] - working_list[j][0];
					/* Check for random error code in range */
					if (tsgid_range_difference > 1)
						error_code = get_random_u32(working_list[j][0] + 1, working_list[j][1] - 1);
					else
						continue;
				}

				ch_priv.err_notifier.error = 0;
				ch_priv.err_notifier.status = 0;

				unit_info(m, "BVEC testing for nvgpu_tsg_set_error_notifier with id =  0x%08x(%s range [0x%08x - 0x%08x] %s)\n", error_code, string_cases[i], working_list[j][0], working_list[j][1], string_states[states]);

				nvgpu_tsg_set_error_notifier(g, tsg, error_code);
				if (i == 0) {
					unit_assert(ch_priv.err_notifier.error == error_code, goto done);
				} else {
					unit_assert(ch_priv.err_notifier.error != error_code , goto done);
				}

			}
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	if (ch != NULL) {
		nvgpu_channel_close(ch);
		ch = NULL;
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
		tsg = NULL;
	}

	return ret;
}

int test_tsg_set_ctx_mmu_error(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *ch = NULL;
	int ret = UNIT_FAIL;
	struct nvgpu_posix_channel ch_priv;
	int err;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	ch->os_priv = &ch_priv;
	ch_priv.err_notifier.error = U32_MAX;
	ch_priv.err_notifier.status = 0;

	nvgpu_tsg_set_ctx_mmu_error(g, tsg);

	unit_assert(ch_priv.err_notifier.error ==
			NVGPU_ERR_NOTIFIER_FIFO_ERROR_MMU_ERR_FLT, goto done);
	unit_assert(ch_priv.err_notifier.status != 0, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	return ret;
}

#define F_TSG_RESET_FAULTED_NO_RESET_HAL		BIT(0)
#define F_TSG_RESET_FAULTED_TSG_NULL			BIT(1)
#define F_TSG_RESET_FAULTED_LAST			BIT(2)

static void stub_channel_reset_faulted(struct gk20a *g, struct nvgpu_channel *ch,
		bool eng, bool pbdma)
{
	stub[0].name = __func__;
	stub[0].chid = ch->chid;
}

int test_tsg_reset_faulted_eng_pbdma(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *ch = NULL;
	int ret = UNIT_FAIL;
	struct gpu_ops gops = g->ops;
	int err;
	u32 branches;
	static const char *labels[] = {
		"no_reset_hal",
		"tsg_null"
	};
	u32 fail =
		F_TSG_RESET_FAULTED_NO_RESET_HAL |
		F_TSG_RESET_FAULTED_TSG_NULL;
	u32 prune = fail;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	for (branches = 0U; branches < F_TSG_MARK_ERROR_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, labels));
			continue;
		}
		subtest_setup(branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));

		g->ops.channel.reset_faulted =
			branches & F_TSG_RESET_FAULTED_NO_RESET_HAL ?
			NULL : stub_channel_reset_faulted;

		if (branches & F_TSG_RESET_FAULTED_TSG_NULL) {
			nvgpu_tsg_reset_faulted_eng_pbdma(g, NULL, true, true);
		} else {
			nvgpu_tsg_reset_faulted_eng_pbdma(g, tsg, true, true);
		}

		if (branches & fail) {
			unit_assert(stub[0].chid != ch->chid, goto done);
		} else {
			unit_assert(stub[0].chid == ch->chid, goto done);
		}
	}

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



struct unit_module_test nvgpu_tsg_tests[] = {
	UNIT_TEST(setup_sw, test_tsg_setup_sw, &unit_ctx, 0),
	UNIT_TEST(init_support, test_fifo_init_support, &unit_ctx, 0),
	UNIT_TEST(open, test_tsg_open, &unit_ctx, 0),
	UNIT_TEST(release, test_tsg_release, &unit_ctx, 0),
	UNIT_TEST(get_from_id, test_tsg_check_and_get_from_id, &unit_ctx, 0),
	UNIT_TEST(get_from_id_bvec, test_tsg_check_and_get_from_id_bvec, &unit_ctx, 0),
	UNIT_TEST(bind_channel, test_tsg_bind_channel, &unit_ctx, 2),
	UNIT_TEST(unbind_channel, test_tsg_unbind_channel, &unit_ctx, 0),
	UNIT_TEST(unbind_channel_check_hw_state,
		test_tsg_unbind_channel_check_hw_state, &unit_ctx, 0),
	UNIT_TEST(sm_error_states, test_tsg_sm_error_state_set_get, &unit_ctx, 0),
	UNIT_TEST(unbind_channel_check_ctx_reload,
		test_tsg_unbind_channel_check_ctx_reload, &unit_ctx, 0),
	UNIT_TEST(enable_disable, test_tsg_enable, &unit_ctx, 0),
	UNIT_TEST(abort, test_tsg_abort, &unit_ctx, 0),
	UNIT_TEST(mark_error, test_tsg_mark_error, &unit_ctx, 0),
	UNIT_TEST(bvec_nvgpu_tsg_set_error_notifier, test_nvgpu_tsg_set_error_notifier_bvec, &unit_ctx, 0),
	UNIT_TEST(set_ctx_mmu_error, test_tsg_set_ctx_mmu_error, &unit_ctx, 0),
	UNIT_TEST(reset_faulted_eng_pbdma, test_tsg_reset_faulted_eng_pbdma, &unit_ctx, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, &unit_ctx, 0),
};

UNIT_MODULE(nvgpu_tsg, nvgpu_tsg_tests, UNIT_PRIO_NVGPU_TEST);
