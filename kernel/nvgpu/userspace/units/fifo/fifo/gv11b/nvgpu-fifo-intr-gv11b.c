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
#include <nvgpu/runlist.h>
#include <nvgpu/fuse.h>
#include <nvgpu/dma.h>
#include <nvgpu/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/nvgpu_err.h>

#include "hal/fifo/fifo_gv11b.h"
#include "hal/fifo/fifo_intr_gv11b.h"

#include <nvgpu/hw/gv11b/hw_fifo_gv11b.h>

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-fifo-intr-gv11b.h"

#define FIFO_GV11B_INTR_UNIT_DEBUG
#ifdef FIFO_GV11B_INTR_UNIT_DEBUG
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

#define branches_str	test_fifo_flags_str

struct unit_ctx {
	bool fifo_ctxsw_timeout_enable;
	bool pbdma_intr_enable;
};

static struct unit_ctx u;

static void stub_fifo_ctxsw_timeout_enable(struct gk20a *g, bool enable)
{
	u.fifo_ctxsw_timeout_enable = enable;
}

static void stub_pbdma_intr_enable(struct gk20a *g, bool enable)
{
	u.pbdma_intr_enable = enable;
}

int test_gv11b_fifo_intr_0_enable(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct gpu_ops gops = g->ops;

	g->ops.fifo.ctxsw_timeout_enable = stub_fifo_ctxsw_timeout_enable;
	g->ops.pbdma.intr_enable = stub_pbdma_intr_enable;

	gv11b_fifo_intr_0_enable(g, true);
	unit_assert(u.fifo_ctxsw_timeout_enable, goto done);
	unit_assert(u.pbdma_intr_enable, goto done);
	unit_assert(nvgpu_readl(g, fifo_intr_runlist_r()) == U32_MAX,
			goto done);
	unit_assert(nvgpu_readl(g, fifo_intr_0_r()) == U32_MAX, goto done);
	unit_assert(nvgpu_readl(g, fifo_intr_en_0_r()) != 0, goto done);

	gv11b_fifo_intr_0_enable(g, false);
	unit_assert(!u.fifo_ctxsw_timeout_enable, goto done);
	unit_assert(!u.pbdma_intr_enable, goto done);
	unit_assert(nvgpu_readl(g, fifo_intr_en_0_r()) == 0, goto done);

	ret = UNIT_SUCCESS;
done:
	g->ops = gops;
	return ret;
}

#define SCHED_ERROR_CODE_RL_REQ_TIMEOUT 	0x0000000c

int test_gv11b_fifo_handle_sched_error(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;

	/* valid sched error code */
	nvgpu_writel(g, fifo_intr_sched_error_r(), SCHED_ERROR_CODE_RL_REQ_TIMEOUT);
	gv11b_fifo_handle_sched_error(g);

	/* invalid sched error code */
	nvgpu_writel(g, fifo_intr_sched_error_r(), U32_MAX);
	gv11b_fifo_handle_sched_error(g);

	/* valid sched error code + "recovery" */
	nvgpu_writel(g, fifo_intr_sched_error_r(), SCHED_ERROR_CODE_BAD_TSG);
	gv11b_fifo_handle_sched_error(g);

	unit_assert(ret != UNIT_SUCCESS, goto done);
	ret = UNIT_SUCCESS;
done:
	return ret;
}

#define FIFO_NUM_INTRS_0	9


static void writel_access_reg_fn(struct gk20a *g,
		struct nvgpu_reg_access *access)
{
	u32 value = access->value;

	if (access->addr == fifo_intr_0_r() ||
	    access->addr == fifo_intr_ctxsw_timeout_r()) {
		/* write clears interrupts */
		value = nvgpu_posix_io_readl_reg_space(g, access->addr) &
				~access->value;
	}

	nvgpu_posix_io_writel_reg_space(g, access->addr, value);
}

static void readl_access_reg_fn(struct gk20a *g,
		struct nvgpu_reg_access *access)
{
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}

static void stub_gr_falcon_dump_stats(struct gk20a *g)
{
}

#define FIFO_INTR_0_ERR_MASK \
		(fifo_intr_0_bind_error_pending_f() |	\
		fifo_intr_0_sched_error_pending_f() |	\
		fifo_intr_0_chsw_error_pending_f() |	\
		fifo_intr_0_memop_timeout_pending_f() | \
		fifo_intr_0_lb_error_pending_f())

int test_gv11b_fifo_intr_0_isr(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_fifo *f = &g->fifo;
	struct gpu_ops gops = g->ops;
	u32 branches = 0;
	u32 fifo_intrs[FIFO_NUM_INTRS_0] = {
		2,	/* not handled */
		fifo_intr_0_bind_error_pending_f(),
		fifo_intr_0_chsw_error_pending_f(),
		fifo_intr_0_memop_timeout_pending_f(),
		fifo_intr_0_lb_error_pending_f(),
		fifo_intr_0_runlist_event_pending_f(),
		fifo_intr_0_pbdma_intr_pending_f(),
		fifo_intr_0_sched_error_pending_f(),
		fifo_intr_0_ctxsw_timeout_pending_f(),
	};
	const char *labels[] = {
		"invalid",
		"bind_err",
		"chsw_err",
		"memop_timeout",
		"lb_err",
		"runlist_event",
		"pbdma_intr",
		"sched_err",
		"ctxsw_timeout",
	};
	u32 fifo_intr_0;
	u32 val;
	int i;
	u32 intr_0_handled_mask = 0;
	struct nvgpu_posix_io_callbacks *old_io;
	struct nvgpu_posix_io_callbacks new_io = {
		.readl = readl_access_reg_fn,
		.writel = writel_access_reg_fn
	};

	old_io = nvgpu_posix_register_io(g, &new_io);

	for (i = 1; i < FIFO_NUM_INTRS_0; i++) {
		intr_0_handled_mask |= fifo_intrs[i];
	}

	nvgpu_posix_io_writel_reg_space(g, fifo_intr_sched_error_r(),
			SCHED_ERROR_CODE_RL_REQ_TIMEOUT);

	g->ops.gr.falcon.dump_stats =
		stub_gr_falcon_dump_stats;

	unit_assert(f->sw_ready, goto done);
	for (branches = 0; branches < BIT(FIFO_NUM_INTRS_0); branches++) {

		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));

		fifo_intr_0 = 0;
		for (i = 0; i < FIFO_NUM_INTRS_0; i++) {
			if (branches & BIT(i)) {
				fifo_intr_0 |= fifo_intrs[i];
			}
		}
		nvgpu_posix_io_writel_reg_space(g, fifo_intr_0_r(),
			fifo_intr_0);
		gv11b_fifo_intr_0_isr(g);
		val = nvgpu_posix_io_readl_reg_space(g, fifo_intr_0_r());
		unit_assert((val & intr_0_handled_mask) == 0, goto done);
		unit_assert((val & ~intr_0_handled_mask) ==
			(fifo_intr_0 & ~intr_0_handled_mask), goto done);
	}

	f->sw_ready = false;
	nvgpu_posix_io_writel_reg_space(g, fifo_intr_0_r(), 0xcafe);
	gv11b_fifo_intr_0_isr(g);
	unit_assert(nvgpu_posix_io_readl_reg_space(g, fifo_intr_0_r()) == 0,
			goto done);
	f->sw_ready = true;

	ret = UNIT_SUCCESS;
done:
	(void) nvgpu_posix_register_io(g, old_io);
	g->ops = gops;
	return ret;
}

int test_gv11b_fifo_intr_recover_mask(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 intr_en_0;
	u32 val;
	struct nvgpu_posix_io_callbacks *old_io;
	struct nvgpu_posix_io_callbacks new_io = {
		.readl = readl_access_reg_fn,
		.writel = writel_access_reg_fn
	};

	old_io = nvgpu_posix_register_io(g, &new_io);

	gv11b_fifo_intr_0_enable(g, true);
	intr_en_0 = nvgpu_posix_io_readl_reg_space(g, fifo_intr_en_0_r());
	unit_assert((intr_en_0 & fifo_intr_0_ctxsw_timeout_pending_f()) != 0,
			goto done);

	nvgpu_posix_io_writel_reg_space(g,
		fifo_intr_ctxsw_timeout_r(), 0xcafe);
	gv11b_fifo_intr_set_recover_mask(g);
	intr_en_0 = nvgpu_posix_io_readl_reg_space(g, fifo_intr_en_0_r());
	unit_assert((intr_en_0 & fifo_intr_0_ctxsw_timeout_pending_f()) == 0,
			goto done);
	val = nvgpu_posix_io_readl_reg_space(g, fifo_intr_ctxsw_timeout_r());
	unit_assert(val == 0, goto done);

	gv11b_fifo_intr_unset_recover_mask(g);
	intr_en_0 = nvgpu_posix_io_readl_reg_space(g, fifo_intr_en_0_r());
	unit_assert((intr_en_0 & fifo_intr_0_ctxsw_timeout_pending_f()) != 0,
			goto done);

	ret = UNIT_SUCCESS;
done:
	(void) nvgpu_posix_register_io(g, old_io);
	return ret;
}