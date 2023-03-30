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
#include <nvgpu/io.h>
#include <nvgpu/soc.h>

#include "hal/init/hal_gv11b.h"
#include "hal/fifo/pbdma_gv11b.h"

#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/hw/gv11b/hw_pbdma_gv11b.h>

#include "../../nvgpu-fifo-common.h"
#include "../../nvgpu-fifo-gv11b.h"
#include "nvgpu-pbdma-gv11b.h"

#ifdef PBDMA_GV11B_UNIT_DEBUG
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
};

int test_gv11b_pbdma_setup_hw(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 num_pbdma;
	u32 pbdma_id;
	u32 timeout;

	num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);
	unit_assert(num_pbdma > 0, goto done);

	gv11b_pbdma_setup_hw(g);
	if (nvgpu_platform_is_silicon(g)) {
		for (pbdma_id = 0; pbdma_id < num_pbdma; pbdma_id++)
		{
			timeout = nvgpu_readl(g, pbdma_timeout_r(pbdma_id));
			unit_assert(get_field(timeout, pbdma_timeout_period_m()) ==
				pbdma_timeout_period_max_f(), goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:

	return ret;
}

int test_gv11b_pbdma_intr_enable(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 num_pbdma;
	u32 pbdma_id;
	bool enable;
	u32 i;

	num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);
	unit_assert(num_pbdma > 0, goto done);

	for (i = 0 ; i < 2; i++) {
		enable = (i > 0);

		for (pbdma_id = 0; pbdma_id < num_pbdma; pbdma_id++) {
			u32 pattern = (0xbeef << 16) + pbdma_id;
			nvgpu_writel(g, pbdma_intr_stall_r(pbdma_id), pattern);
			nvgpu_writel(g, pbdma_intr_en_0_r(pbdma_id), 0);
			nvgpu_writel(g, pbdma_intr_stall_1_r(pbdma_id),
				pattern | pbdma_intr_stall_1_hce_illegal_op_enabled_f());
			nvgpu_writel(g, pbdma_intr_en_1_r(pbdma_id), 0);
		}

		gv11b_pbdma_intr_enable(g, enable);

		for (pbdma_id = 0; pbdma_id < num_pbdma; pbdma_id++) {
			u32 pattern = (0xbeef << 16) + pbdma_id;
			u32 intr_0 = nvgpu_readl(g, pbdma_intr_stall_r(pbdma_id));
			u32 intr_1 = nvgpu_readl(g, pbdma_intr_stall_1_r(pbdma_id));
			u32 intr_en_0 = nvgpu_readl(g, pbdma_intr_en_0_r(pbdma_id));
			u32 intr_en_1 = nvgpu_readl(g, pbdma_intr_en_1_r(pbdma_id));

			if (enable) {
				unit_assert(intr_en_0 == pattern, goto done);
				unit_assert(intr_en_1 == (pattern &
					~pbdma_intr_stall_1_hce_illegal_op_enabled_f()),
					goto done);
			} else {
				unit_assert(intr_en_0 == 0, goto done);
				unit_assert(intr_en_1 == 0, goto done);
			}
			unit_assert(intr_0 != 0, goto done);
			unit_assert(intr_1 != 0, goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:

	return ret;
}

#define PBDMA_NUM_INTRS_0	3
#define INVALID_ERR_NOTIFIER	U32_MAX

static u32 pbdma_method_r(u32 pbdma_id, u32 method_index)
{
	u32 stride = pbdma_method1_r(pbdma_id) - pbdma_method0_r(pbdma_id);
	return pbdma_method0_r(pbdma_id) + stride * method_index;
}

int test_gv11b_pbdma_handle_intr_0(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_fifo *f = &g->fifo;
	struct gpu_ops gops = g->ops;
	u32 branches = 0;
	u32 pbdma_intrs[PBDMA_NUM_INTRS_0] = {
		pbdma_intr_0_memreq_pending_f(),
		pbdma_intr_0_clear_faulted_error_pending_f(),
		pbdma_intr_0_eng_reset_pending_f(),
	};
	const char *labels[] = {
		"memreq",
		"clear_faulted",
		"eng_reset",
	};
	u32 pbdma_id = 0;
	u32 pbdma_intr_0;
	u32 err_notifier;
	bool recover;
	int i;

	unit_assert((f->intr.pbdma.device_fatal_0 &
			pbdma_intr_0_memreq_pending_f()) != 0, goto done);

	for (branches = 0; branches < BIT(PBDMA_NUM_INTRS_0); branches++) {

		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));

		pbdma_intr_0 = 0;
		for (i = 0; i < PBDMA_NUM_INTRS_0; i++) {
			if (branches & BIT(i)) {
				pbdma_intr_0 |= pbdma_intrs[i];
			}
		}
		err_notifier = INVALID_ERR_NOTIFIER;

		nvgpu_writel(g, pbdma_intr_0_r(pbdma_id), pbdma_intr_0);
		nvgpu_writel(g, pbdma_method_r(pbdma_id, 0), 0);

		recover = gv11b_pbdma_handle_intr_0(g, pbdma_id, pbdma_intr_0, &err_notifier);

		if (pbdma_intr_0 == 0) {
			unit_assert(!recover, goto done);
		}

		if (pbdma_intr_0 & pbdma_intr_0_memreq_pending_f()) {
			unit_assert(recover, goto done);
		}

		if (pbdma_intr_0 & pbdma_intr_0_clear_faulted_error_pending_f()) {
			unit_assert(recover, goto done);
			unit_assert(nvgpu_readl(g,
				pbdma_method_r(pbdma_id, 0)) != 0, goto done);
		} else {
			unit_assert(nvgpu_readl(g,
				pbdma_method_r(pbdma_id, 0)) == 0, goto done);
		}

		if (pbdma_intr_0 & pbdma_intr_0_eng_reset_pending_f()) {
			unit_assert(recover, goto done);
		}
	}

	recover = gv11b_pbdma_handle_intr_0(g, pbdma_id,
		pbdma_intr_0_memack_extra_pending_f(), &err_notifier);
	recover = gv11b_pbdma_handle_intr_0(g, pbdma_id,
		pbdma_intr_0_gpfifo_pending_f(), &err_notifier);
	recover = gv11b_pbdma_handle_intr_0(g, pbdma_id,
		pbdma_intr_0_clear_faulted_error_pending_f(), &err_notifier);
	recover = gv11b_pbdma_handle_intr_0(g, pbdma_id,
		pbdma_intr_0_signature_pending_f(), &err_notifier);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
	}
	g->ops = gops;
	return ret;
}

#define F_PBDMA_INTR_1_CTXNOTVALID_IN		BIT(0)
#define F_PBDMA_INTR_1_CTXNOTVALID_READ		BIT(1)
#define F_PBDMA_INTR_1_HCE			BIT(2)
#define F_PBDMA_INTR_1_LAST			BIT(3)

int test_gv11b_pbdma_handle_intr_1(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 branches;
	const char *labels[] = {
		"ctxnotvalid_in",
		"ctxnotvalid_readl",
		"hce"
	};
	u32 pbdma_id = 0;
	u32 pbdma_intr_1;
	u32 err_notifier;
	bool recover;

	for (branches = 0; branches < F_PBDMA_INTR_1_LAST; branches++) {

		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));

		pbdma_intr_1 = 0;

		if (branches & F_PBDMA_INTR_1_CTXNOTVALID_IN) {
			pbdma_intr_1 |= pbdma_intr_1_ctxnotvalid_pending_f();
		}

		if (branches & F_PBDMA_INTR_1_CTXNOTVALID_READ) {
			nvgpu_writel(g, pbdma_intr_1_r(pbdma_id),
					pbdma_intr_1_ctxnotvalid_pending_f());
		} else {
			nvgpu_writel(g, pbdma_intr_1_r(pbdma_id), 0);
		}

		if (branches & F_PBDMA_INTR_1_HCE) {
			pbdma_intr_1 |= BIT(0); /* HCE_RE_ILLEGAL_OP */
		}

		err_notifier = INVALID_ERR_NOTIFIER;

		recover = gv11b_pbdma_handle_intr_1(g, pbdma_id, pbdma_intr_1, &err_notifier);

		if (pbdma_intr_1 == 0) {
			unit_assert(!recover, goto done);
		}

		if (((branches & F_PBDMA_INTR_1_CTXNOTVALID_IN) &&
			(branches & F_PBDMA_INTR_1_CTXNOTVALID_READ)) ||
			(branches & F_PBDMA_INTR_1_HCE)) {
			unit_assert(recover, goto done);
		} else {
			unit_assert(!recover, goto done);
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

int test_gv11b_pbdma_intr_descs(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_fifo *f = &g->fifo;
	u32 intr_descs = (f->intr.pbdma.device_fatal_0 |
			  f->intr.pbdma.channel_fatal_0 |
			  f->intr.pbdma.restartable_0);
	u32 channel_fatal_0 = gv11b_pbdma_channel_fatal_0_intr_descs();

	unit_assert(channel_fatal_0 != 0, goto done);
	unit_assert((intr_descs & channel_fatal_0) == channel_fatal_0,
		goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;

}

int test_gv11b_pbdma_get_fc(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;

	unit_assert(gv11b_pbdma_get_fc_pb_header() ==
		(pbdma_pb_header_method_zero_f() |
		 pbdma_pb_header_subchannel_zero_f() |
		 pbdma_pb_header_level_main_f() |
		 pbdma_pb_header_first_true_f() |
		 pbdma_pb_header_type_inc_f()), goto done);

	unit_assert(gv11b_pbdma_get_fc_target(NULL) ==
		(pbdma_target_engine_sw_f() |
		 pbdma_target_eng_ctx_valid_true_f() |
		 pbdma_target_ce_ctx_valid_true_f()), goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_gv11b_pbdma_set_channel_info_veid(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 subctx_id;

	for (subctx_id = 0; subctx_id < 64; subctx_id ++) {
		unit_assert(gv11b_pbdma_set_channel_info_veid(subctx_id) ==
			pbdma_set_channel_info_veid_f(subctx_id), goto done);
	}

	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_gv11b_pbdma_config_userd_writeback_enable(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;

	unit_assert(gv11b_pbdma_config_userd_writeback_enable(0U) ==
		pbdma_config_userd_writeback_enable_f(), goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

struct unit_module_test nvgpu_pbdma_gv11b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(setup_hw, test_gv11b_pbdma_setup_hw, NULL, 0),
	UNIT_TEST(intr_enable, test_gv11b_pbdma_intr_enable, NULL, 0),
	UNIT_TEST(handle_intr_0, test_gv11b_pbdma_handle_intr_0, NULL, 0),
	UNIT_TEST(handle_intr_1, test_gv11b_pbdma_handle_intr_1, NULL, 0),
	UNIT_TEST(intr_descs, test_gv11b_pbdma_intr_descs, NULL, 0),
	UNIT_TEST(get_fc, test_gv11b_pbdma_get_fc, NULL, 0),
	UNIT_TEST(set_channel_info_veid,
		test_gv11b_pbdma_set_channel_info_veid, NULL, 0),
	UNIT_TEST(config_userd_writeback_enable,
		test_gv11b_pbdma_config_userd_writeback_enable, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_pbdma_gv11b, nvgpu_pbdma_gv11b_tests, UNIT_PRIO_NVGPU_TEST);
