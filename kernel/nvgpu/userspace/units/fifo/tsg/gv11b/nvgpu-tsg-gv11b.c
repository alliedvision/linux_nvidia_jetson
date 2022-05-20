/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/runlist.h>
#include <nvgpu/fuse.h>
#include <nvgpu/dma.h>
#include <nvgpu/gr/ctx.h>

#include "common/gr/ctx_priv.h"
#include <nvgpu/posix/posix-fault-injection.h>

#include "hal/fifo/tsg_gk20a.h"

#include "hal/init/hal_gv11b.h"
#include "hal/fifo/tsg_gv11b.h"

#include <nvgpu/hw/gv11b/hw_ram_gv11b.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-tsg-gv11b.h"

#ifdef TSG_GV11B_UNIT_DEBUG
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

#define branches_str test_fifo_flags_str
#define pruned test_fifo_subtest_pruned

static void stub_channel_enable(struct nvgpu_channel *ch)
{
	stub[0].chid = ch->chid;
	stub[0].count++;
}

static void stub_usermode_ring_doorbell(struct nvgpu_channel *ch)
{
	stub[1].chid = ch->chid;
	stub[1].count++;
}

int test_gv11b_tsg_enable(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *ch = NULL;
	int ret = UNIT_FAIL;
	int err;

	memset(stub, 0, sizeof(stub));
	g->ops.channel.enable = stub_channel_enable;
	g->ops.usermode.ring_doorbell = stub_usermode_ring_doorbell;

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	/* standalone TSG */
	gv11b_tsg_enable(tsg);
	unit_assert(stub[0].count == 0, goto done);
	unit_assert(stub[1].count == 0, goto done);

	ch = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	/* TSG with bound channel */
	gv11b_tsg_enable(tsg);
	unit_assert(stub[0].count == 1, goto done);
	unit_assert(stub[0].chid == ch->chid, goto done);
	unit_assert(stub[1].count == 1, goto done);
	unit_assert(stub[1].chid == ch->chid, goto done);

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

#define GR_RUNQUE			0U	/* pbdma 0 */
#define ASYNC_CE_RUNQUE			2U	/* pbdma 2 */

#define F_TSG_INIT_ENG_BUF_ALREADY_EXISTS	BIT32(0)
#define F_TSG_INIT_ENG_BUF_KZALLOC_FAIL		BIT32(1)
#define F_TSG_INIT_ENG_BUF_DMA_ALLOC_FAIL_0	BIT32(2)
#define F_TSG_INIT_ENG_BUF_DMA_ALLOC_FAIL_1	BIT32(3)
#define F_TSG_INIT_ENG_BUF_LAST			BIT32(4)

int test_gv11b_tsg_init_eng_method_buffers(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_tsg _tsg, *tsg = &_tsg;
	struct nvgpu_mem dummy;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	u32 fail = F_TSG_INIT_ENG_BUF_KZALLOC_FAIL |
		   F_TSG_INIT_ENG_BUF_DMA_ALLOC_FAIL_0 |
		   F_TSG_INIT_ENG_BUF_DMA_ALLOC_FAIL_1;
	u32 prune = F_TSG_INIT_ENG_BUF_ALREADY_EXISTS | fail;
	struct nvgpu_posix_fault_inj *kmem_fi;
	struct nvgpu_posix_fault_inj *dma_fi;
	int err;
	static const char *labels[] = {
		"buf_exists",
		"kzalloc_fail",
		"dma_alloc_fail_0",
		"dma_alloc_fail_1"
	};

	kmem_fi = nvgpu_kmem_get_fault_injection();
	dma_fi = nvgpu_dma_alloc_get_fault_injection();

	for (branches = 0U; branches < F_TSG_INIT_ENG_BUF_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, labels));
			continue;
		}
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
		subtest_setup(branches);

		tsg->eng_method_buffers =
			branches & F_TSG_INIT_ENG_BUF_ALREADY_EXISTS ?
			&dummy : NULL;

		nvgpu_posix_enable_fault_injection(kmem_fi,
			branches & F_TSG_INIT_ENG_BUF_KZALLOC_FAIL ?
				true : false, 0);

		nvgpu_posix_enable_fault_injection(dma_fi, false, 0);

		if (branches & F_TSG_INIT_ENG_BUF_DMA_ALLOC_FAIL_0) {
			nvgpu_posix_enable_fault_injection(dma_fi, true, 0);
		}

		if (branches & F_TSG_INIT_ENG_BUF_DMA_ALLOC_FAIL_1) {
			nvgpu_posix_enable_fault_injection(dma_fi, true, 1);
		}

		err = g->ops.tsg.init_eng_method_buffers(g, tsg);

		if (branches & fail) {
			unit_assert(err != 0, goto done);
			unit_assert(tsg->eng_method_buffers == NULL, goto done);
		} else {
			unit_assert(err == 0, goto done);
			if ((branches & F_TSG_INIT_ENG_BUF_ALREADY_EXISTS) == 0) {
				unit_assert(tsg->eng_method_buffers != NULL,
						goto done);
				unit_assert(tsg->eng_method_buffers[
					ASYNC_CE_RUNQUE].gpu_va != 0UL,
					goto done);
				g->ops.tsg.deinit_eng_method_buffers(g, tsg);
				unit_assert(tsg->eng_method_buffers == NULL,
					goto done);
			}
		}
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);

	return ret;
}

#define F_TSG_BIND_BUF_NO_METHOD_BUF		BIT(0)
#define F_TSG_BIND_BUF_FAST_CE_RUNLIST_ID	BIT(1)
#define F_TSG_BIND_BUF_LAST			BIT(2)

int test_gv11b_tsg_bind_channel_eng_method_buffers(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_mem *eng_method_buffers;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	u32 prune = F_TSG_BIND_BUF_NO_METHOD_BUF;
	int err;
	u64 gpu_va;
	static const char *labels[] = {
		"!eng_method_buf",
		"fast_ce_runlist",
	};

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);

	ch = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	eng_method_buffers = tsg->eng_method_buffers;

	for (branches = 0U; branches < F_TSG_BIND_BUF_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, labels));
			continue;
		}
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
		subtest_setup(branches);

		tsg->eng_method_buffers =
			branches & F_TSG_BIND_BUF_NO_METHOD_BUF ?
			NULL : eng_method_buffers;

		if (branches & F_TSG_BIND_BUF_FAST_CE_RUNLIST_ID) {
			tsg->runlist = g->fifo.runlists[
				nvgpu_engine_get_fast_ce_runlist_id(g)];
			gpu_va = eng_method_buffers[ASYNC_CE_RUNQUE].gpu_va;
		} else {
			tsg->runlist = g->fifo.runlists[
				nvgpu_engine_get_gr_runlist_id(g)];
			gpu_va = eng_method_buffers[GR_RUNQUE].gpu_va;
		}

		nvgpu_mem_wr32(g, &ch->inst_block,
			ram_in_eng_method_buffer_addr_lo_w(), 0U);
		nvgpu_mem_wr32(g, &ch->inst_block,
			ram_in_eng_method_buffer_addr_hi_w(), 0U);

		g->ops.tsg.bind_channel_eng_method_buffers(tsg, ch);

		if (branches & F_TSG_BIND_BUF_NO_METHOD_BUF) {
			unit_assert(nvgpu_mem_rd32(g, &ch->inst_block,
				ram_in_eng_method_buffer_addr_lo_w()) == 0U,
				goto done);
			unit_assert(nvgpu_mem_rd32(g, &ch->inst_block,
				ram_in_eng_method_buffer_addr_hi_w()) == 0U,
				goto done);

		} else {
			unit_assert(nvgpu_mem_rd32(g, &ch->inst_block,
				ram_in_eng_method_buffer_addr_lo_w()) ==
					u64_lo32(gpu_va), goto done);
			unit_assert(nvgpu_mem_rd32(g, &ch->inst_block,
				ram_in_eng_method_buffer_addr_hi_w()) ==
					u64_hi32(gpu_va), goto done);
		}

		tsg->eng_method_buffers = eng_method_buffers;
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
	return ret;
}

#define F_TSG_UNBIND_BUF_NOT_FAULTED		BIT(0)
#define F_TSG_UNBIND_BUF_NO_METHOD_BUF		BIT(1)
#define F_TSG_UNBIND_BUF_CH_SAVED		BIT(2)
#define F_TSG_UNBIND_BUF_LAST			BIT(3)

int test_gv11b_tsg_unbind_channel_check_eng_faulted(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_channel_hw_state hw_state;
	struct nvgpu_mem *eng_method_buffers;
	u32 branches = 0U;
	int ret = UNIT_FAIL;
	u32 prune = F_TSG_UNBIND_BUF_NOT_FAULTED |
		    F_TSG_UNBIND_BUF_NO_METHOD_BUF;
	int err;
	static const char *labels[] = {
		"!eng_faulted",
		"!eng_method_buf",
		"ch_saved",
	};

	tsg = nvgpu_tsg_open(g, getpid());
	unit_assert(tsg != NULL, goto done);
	unit_assert(tsg->eng_method_buffers != NULL, goto done);
	eng_method_buffers = tsg->eng_method_buffers;

	ch = nvgpu_channel_open_new(g, ~0U, false, getpid(), getpid());
	unit_assert(ch != NULL, goto done);

	err = nvgpu_tsg_bind_channel(tsg, ch);
	unit_assert(err == 0, goto done);

	unit_assert(g->ops.tsg.unbind_channel_check_eng_faulted != NULL,
			goto done);

	for (branches = 0U; branches < F_TSG_UNBIND_BUF_LAST; branches++) {

		if (pruned(branches, prune)) {
			unit_verbose(m, "%s branches=%s (pruned)\n", __func__,
				branches_str(branches, labels));
			continue;
		}
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
		subtest_setup(branches);

		hw_state.eng_faulted =
			branches & F_TSG_UNBIND_BUF_NOT_FAULTED ?
			false : true;

		tsg->eng_method_buffers =
			branches & F_TSG_UNBIND_BUF_NO_METHOD_BUF ?
			NULL : eng_method_buffers;

		nvgpu_mem_wr32(g, &eng_method_buffers[ASYNC_CE_RUNQUE], 1,
			branches & F_TSG_UNBIND_BUF_CH_SAVED ?
			ch->chid : ~(ch->chid));
		nvgpu_mem_wr32(g, &eng_method_buffers[ASYNC_CE_RUNQUE], 0, 1);

		g->ops.tsg.unbind_channel_check_eng_faulted(tsg, ch, &hw_state);

		if (branches & F_TSG_UNBIND_BUF_CH_SAVED) {
			/* check that method count has been set to 0 */
			unit_assert(nvgpu_mem_rd32(g,
				&eng_method_buffers[ASYNC_CE_RUNQUE], 0) == 0,
				goto done);
		} else {
			/* check that method countis unchanged */
			unit_assert(nvgpu_mem_rd32(g,
				&eng_method_buffers[ASYNC_CE_RUNQUE], 0) == 1,
				goto done);
		}

		tsg->eng_method_buffers = eng_method_buffers;
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
	return ret;
}

struct unit_module_test nvgpu_tsg_gv11b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, &unit_ctx, 0),
	UNIT_TEST(gv11b_tsg_enable, test_gv11b_tsg_enable, &unit_ctx, 0),
	UNIT_TEST(gv11b_tsg_init_eng_method_buffers, \
			test_gv11b_tsg_init_eng_method_buffers, &unit_ctx, 0),
	UNIT_TEST(gv11b_tsg_bind_channel_eng_method_buffers,
			test_gv11b_tsg_bind_channel_eng_method_buffers, &unit_ctx, 0),
	UNIT_TEST(gv11b_tsg_unbind_channel_check_eng_faulted, \
			test_gv11b_tsg_unbind_channel_check_eng_faulted, &unit_ctx, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, &unit_ctx, 0),
};

UNIT_MODULE(nvgpu_tsg_gv11b, nvgpu_tsg_gv11b_tests, UNIT_PRIO_NVGPU_TEST);
