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
#include <nvgpu/pbdma.h>
#include <nvgpu/pbdma_status.h>
#include <nvgpu/dma.h>
#include <nvgpu/io.h>

#include "hal/fifo/pbdma_gm20b.h"

#include <nvgpu/hw/gm20b/hw_pbdma_gm20b.h>

#include "../../nvgpu-fifo-common.h"
#include "../../nvgpu-fifo-gv11b.h"
#include "nvgpu-pbdma-gm20b.h"
#include "nvgpu-pbdma-status-gm20b.h"

#ifdef PBDMA_GM20B_UNIT_DEBUG
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
	u32 branches;
	struct {
		struct {
			u32 count;
		} pbdma_handle_intr_0;
		struct {
			u32 count;
		} pbdma_handle_intr_1;
	} stubs;
};

static struct unit_ctx u;

static void subtest_setup(struct unit_module *m, u32 branches)
{
	u.m = m;
	u.branches = branches;
	memset(&u.stubs, 0, sizeof(u.stubs));
}

static u32 _man(u32 timeout)
{
	return (timeout & pbdma_acquire_timeout_man_max_f()) >> 15;
}

static u32 _exp(u32 timeout)
{
	return (timeout & pbdma_acquire_timeout_exp_max_f()) >> 11;
}

static bool is_timeout_valid(struct unit_module *m, u32 timeout, u64 ms) {

	u64 man = _man(timeout);
	u64 exp = _exp(timeout);
	u64 actual_ns = (1024UL * (u64)man) << (u64)exp;
	u64 expected_ns;
	u64 delta;
	u64 max_delta = (1024UL << exp);
	u64 max_ns = ((1024UL * (u64)pbdma_acquire_timeout_man_max_v()) <<
			pbdma_acquire_timeout_exp_max_v());

	unit_assert((timeout & 0x3ff) == (pbdma_acquire_retry_man_2_f() |
				pbdma_acquire_retry_exp_2_f()), goto done);
	if (ms == 0) {
		unit_assert((timeout & pbdma_acquire_timeout_en_enable_f()) ==
				0, goto done);
		return true;
	} else {
		unit_assert((timeout & pbdma_acquire_timeout_en_enable_f()) !=
				0, goto done);
	}

	unit_verbose(m, "ms = %llu\n", ms);
	unit_verbose(m, "max_ns = %llu\n", max_ns);

	expected_ns = (ms * 80 * 1000000UL) / 100;
	expected_ns = min(expected_ns, max_ns);
	unit_verbose(m, "expected_ns = %llu\n", expected_ns);
	unit_verbose(m, "actual_ns = %llu\n", actual_ns);

	unit_verbose(m, "man = %x\n", (u32)man);
	unit_verbose(m, "exp = %x\n", (u32)exp);

	delta = (expected_ns > actual_ns ?
			expected_ns - actual_ns : actual_ns - expected_ns);
	unit_verbose(m, "max delta = %llu\n", max_delta);
	unit_verbose(m, "delta = %llu\n", delta);
	unit_assert(delta < max_delta, goto done);

	return true;
done:
	return false;
}

int test_gm20b_pbdma_acquire_val(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 timeout;
	u64 ms;
	int i;
	int err;

	timeout = gm20b_pbdma_acquire_val(0);
	unit_assert(is_timeout_valid(m, timeout, 0), goto done);

	for (i = 0; i < 32; i++) {
		ms = (1ULL << i);
		timeout = gm20b_pbdma_acquire_val(ms);
		unit_assert(is_timeout_valid(m, timeout, ms), goto done);
	}

	err = EXPECT_BUG(gm20b_pbdma_acquire_val(U64_MAX));
	unit_assert(err != 0, goto done);

	ret = UNIT_SUCCESS;
done:

	return ret;
}

#define INVALID_ERR_NOTIFIER	U32_MAX

#define PBDMA_NUM_INTRS	6

#define METHOD_NO_SUBCH	0
#define METHOD_SUBCH5	(5 << 16)
#define METHOD_SUBCH6	(6 << 16)
#define METHOD_SUBCH7	(7 << 16)

int test_gm20b_pbdma_handle_intr_0(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_fifo *f = &g->fifo;
	struct gpu_ops gops = g->ops;
	u32 branches = 0;
	u32 pbdma_intrs[PBDMA_NUM_INTRS] = {
		pbdma_intr_0_memreq_pending_f(),
		pbdma_intr_0_acquire_pending_f(),
		pbdma_intr_0_pbentry_pending_f(),
		pbdma_intr_0_method_pending_f(),
		pbdma_intr_0_pbcrc_pending_f(),
		pbdma_intr_0_device_pending_f()
	};
	const char *labels[] = {
		"memreq",
		"acquire",
		"pbentry",
		"method",
		"pbcrc",
		"device",
	};
	u32 pbdma_id = 0;
	u32 pbdma_intr_0;
	u32 err_notifier;
	bool recover;
	int i;
	int err;

	unit_assert((f->intr.pbdma.device_fatal_0 &
			pbdma_intr_0_memreq_pending_f()) != 0, goto done);

	for (branches = 0; branches < BIT(PBDMA_NUM_INTRS); branches++) {

		subtest_setup(m, branches);
		unit_verbose(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));

		pbdma_intr_0 = 0;
		for (i = 0; i < PBDMA_NUM_INTRS; i++) {
			if (branches & BIT(i)) {
				pbdma_intr_0 |= pbdma_intrs[i];
			}
		}
		err_notifier = INVALID_ERR_NOTIFIER;

		nvgpu_writel(g, pbdma_intr_0_r(pbdma_id), pbdma_intr_0);
		nvgpu_writel(g, pbdma_method0_r(pbdma_id), METHOD_SUBCH5);
		nvgpu_writel(g, pbdma_method1_r(pbdma_id), METHOD_NO_SUBCH);
		nvgpu_writel(g, pbdma_method2_r(pbdma_id), METHOD_SUBCH6);
		nvgpu_writel(g, pbdma_method3_r(pbdma_id), METHOD_SUBCH7);
		nvgpu_writel(g, pbdma_pb_header_r(pbdma_id), 0);

		recover = gm20b_pbdma_handle_intr_0(g, pbdma_id, pbdma_intr_0, &err_notifier);

		if (pbdma_intr_0 == 0) {
			unit_assert(!recover, goto done);
		}

		if (pbdma_intr_0 & pbdma_intr_0_memreq_pending_f()) {
			unit_assert(recover, goto done);
		}

		if (pbdma_intr_0 & pbdma_intr_0_acquire_pending_f()) {
			if (nvgpu_is_timeouts_enabled(g)) {
				unit_assert(recover, goto done);
				unit_assert(err_notifier !=
					INVALID_ERR_NOTIFIER, goto done);
			} else {
				unit_assert(!recover, goto done);
			}
		}

		if (pbdma_intr_0 & pbdma_intr_0_pbentry_pending_f()) {
			unit_assert(recover, goto done);
			unit_assert(nvgpu_readl(g,
				pbdma_pb_header_r(pbdma_id)) != 0, goto done);
			unit_assert(nvgpu_readl(g,
				pbdma_method0_r(pbdma_id)) != METHOD_SUBCH5,
				goto done);
		}

		if (pbdma_intr_0 & pbdma_intr_0_method_pending_f()) {
			unit_assert(recover, goto done);
			unit_assert(nvgpu_readl(g,
				pbdma_method0_r(pbdma_id)) != METHOD_SUBCH5,
				goto done);
		}

		if (pbdma_intr_0 & pbdma_intr_0_pbcrc_pending_f()) {
			unit_assert(recover, goto done);
			unit_assert(err_notifier != INVALID_ERR_NOTIFIER,
				goto done);
		}

		if (pbdma_intr_0 & pbdma_intr_0_device_pending_f()) {
			unit_assert(recover, goto done);
			unit_assert(nvgpu_readl(g,
				pbdma_pb_header_r(pbdma_id)) != 0, goto done);
			unit_assert(nvgpu_readl(g,
				pbdma_method0_r(pbdma_id)) != METHOD_SUBCH5,
				goto done);
			unit_assert(nvgpu_readl(g,
				pbdma_method1_r(pbdma_id)) == METHOD_NO_SUBCH,
				goto done);
			unit_assert(nvgpu_readl(g,
				pbdma_method2_r(pbdma_id)) != METHOD_SUBCH6,
				goto done);
			unit_assert(nvgpu_readl(g,
				pbdma_method3_r(pbdma_id)) != METHOD_SUBCH7,
				goto done);
		}
	}


	/* trigger assert in pbdma_method1_r() */
	pbdma_id = (0x100000000ULL - (u64)pbdma_method1_r(0) + 8191ULL) / 8192ULL;
	err = EXPECT_BUG(
		recover = gm20b_pbdma_handle_intr_0(g,
			pbdma_id, /* invalid pbdma_id */
			pbdma_intr_0_device_pending_f(),
			&err_notifier)
	);
	unit_assert(err != 0, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
			branches_str(branches, labels));
	}
	g->ops = gops;
	return ret;
}

int test_gm20b_pbdma_read_data(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 pbdma_id = 0;

	for (pbdma_id = 0;
	     pbdma_id < nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);
	     pbdma_id++) {
		u32 pattern = (0xbeef << 16) + pbdma_id;
		nvgpu_writel(g, pbdma_hdr_shadow_r(pbdma_id), pattern);
		unit_assert(gm20b_pbdma_read_data(g, pbdma_id) == pattern,
				goto done);
	}

	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_gm20b_pbdma_intr_descs(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_fifo *f = &g->fifo;
	u32 intr_descs = (f->intr.pbdma.device_fatal_0 |
			  f->intr.pbdma.channel_fatal_0 |
	      		  f->intr.pbdma.restartable_0);
	u32 fatal_0 = gm20b_pbdma_device_fatal_0_intr_descs();
	u32 restartable_0 = gm20b_pbdma_restartable_0_intr_descs();

	unit_assert(fatal_0 != 0, goto done);
	unit_assert(restartable_0 != 0, goto done);
	unit_assert((intr_descs & fatal_0) == fatal_0, goto done);
	unit_assert((intr_descs & restartable_0) == restartable_0, goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_gm20b_pbdma_format_gpfifo_entry(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_gpfifo_entry gpfifo_entry;
	u64 pb_gpu_va = 0x12deadbeefULL;
	u32 method_size = 5;

	memset(&gpfifo_entry, 0, sizeof(gpfifo_entry));
	gm20b_pbdma_format_gpfifo_entry(g, &gpfifo_entry, pb_gpu_va, method_size);
	unit_assert(gpfifo_entry.entry0 == 0xdeadbeef, goto done);
	unit_assert(gpfifo_entry.entry1 == (0x12 |
		pbdma_gp_entry1_length_f(method_size)), goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_gm20b_pbdma_get_gp_base(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u64 gpfifo_base = 0x12deadbeefULL;
	u32 base_lo;
	u32 base_hi;
	u32 n;
	int err;

	err =  EXPECT_BUG(gm20b_pbdma_get_gp_base_hi(gpfifo_base, 0));
	unit_assert(err != 0, goto done);

	for (n = 1; n < 16; n++) {
		base_lo = gm20b_pbdma_get_gp_base(gpfifo_base);
		base_hi = gm20b_pbdma_get_gp_base_hi(gpfifo_base, 1 << n);
		unit_assert(base_lo == pbdma_gp_base_offset_f(
			u64_lo32(gpfifo_base >> pbdma_gp_base_rsvd_s())),
			goto done);
		unit_assert(base_hi ==
			(pbdma_gp_base_hi_offset_f(u64_hi32(gpfifo_base)) |
			 pbdma_gp_base_hi_limit2_f(n)), goto done);
	}

	ret = UNIT_SUCCESS;
done:
	return ret;
}

#define PBDMA_SUBDEVICE_ID  1U

int test_gm20b_pbdma_get_fc_subdevice(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;

	unit_assert(gm20b_pbdma_get_fc_subdevice() ==
		(pbdma_subdevice_id_f(PBDMA_SUBDEVICE_ID) |
		 pbdma_subdevice_status_active_f() |
		 pbdma_subdevice_channel_dma_enable_f()), goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_gm20b_pbdma_get_ctrl_hce_priv_mode_yes(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;

	unit_assert(gm20b_pbdma_get_ctrl_hce_priv_mode_yes() ==
		pbdma_hce_ctrl_hce_priv_mode_yes_f(), goto done);

	ret = UNIT_SUCCESS;
done:
	return ret;
}


int test_gm20b_pbdma_get_userd(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u32 addr_lo = 0xdeadbeef;
	u32 addr_hi = 0x12;
	struct nvgpu_mem mem;
	u32 mask = 0xaaaa;
	int err;

	unit_assert(gm20b_pbdma_get_userd_addr(addr_lo) ==
			pbdma_userd_addr_f(addr_lo), goto done);
	unit_assert(gm20b_pbdma_get_userd_hi_addr(addr_hi) ==
			pbdma_userd_hi_addr_f(addr_hi), goto done);

	mem.aperture = APERTURE_INVALID;
	err = EXPECT_BUG(mask = gm20b_pbdma_get_userd_aperture_mask(g, &mem));
	unit_assert(err != 0, goto done);
	unit_assert(mask == 0xaaaa, goto done);

	if (nvgpu_is_enabled(g, NVGPU_MM_HONORS_APERTURE)) {
		mem.aperture = APERTURE_SYSMEM;
		unit_assert(gm20b_pbdma_get_userd_aperture_mask(g, &mem) ==
				pbdma_userd_target_sys_mem_ncoh_f(), goto done);
		mem.aperture = APERTURE_SYSMEM_COH;
		unit_assert(gm20b_pbdma_get_userd_aperture_mask(g, &mem) ==
				pbdma_userd_target_sys_mem_coh_f(), goto done);
		mem.aperture = APERTURE_VIDMEM;
		unit_assert(gm20b_pbdma_get_userd_aperture_mask(g, &mem) ==
				pbdma_userd_target_vid_mem_f(), goto done);
	} else {
		mem.aperture = APERTURE_SYSMEM;
		unit_assert(gm20b_pbdma_get_userd_aperture_mask(g, &mem) ==
				pbdma_userd_target_vid_mem_f(), goto done);
		mem.aperture = APERTURE_SYSMEM_COH;
		unit_assert(gm20b_pbdma_get_userd_aperture_mask(g, &mem) ==
				pbdma_userd_target_vid_mem_f(), goto done);
		mem.aperture = APERTURE_VIDMEM;
		unit_assert(gm20b_pbdma_get_userd_aperture_mask(g, &mem) ==
				pbdma_userd_target_vid_mem_f(), goto done);
	}

	ret = UNIT_SUCCESS;
done:
	return ret;
}


struct unit_module_test nvgpu_pbdma_gm20b_tests[] = {
	UNIT_TEST(init_support, test_fifo_init_support, NULL, 0),
	UNIT_TEST(pbdma_acquire_val, test_gm20b_pbdma_acquire_val, NULL, 0),
	UNIT_TEST(pbdma_handle_intr_0, test_gm20b_pbdma_handle_intr_0, NULL, 0),
	UNIT_TEST(pbdma_read_data, test_gm20b_pbdma_read_data, NULL, 0),
	UNIT_TEST(pbdma_intr_descs, test_gm20b_pbdma_intr_descs, NULL, 0),
	UNIT_TEST(pbdma_format_gpfifo_entry, test_gm20b_pbdma_format_gpfifo_entry, NULL, 0),
	UNIT_TEST(pbdma_get_gp_base, test_gm20b_pbdma_get_gp_base, NULL, 0),
	UNIT_TEST(pbdma_get_fc_subdevice, test_gm20b_pbdma_get_fc_subdevice, NULL, 0),
	UNIT_TEST(pbdma_get_ctrl_hce_priv_mode_yes, test_gm20b_pbdma_get_ctrl_hce_priv_mode_yes, NULL, 0),
	UNIT_TEST(pbdma_get_userd, test_gm20b_pbdma_get_userd, NULL, 0),

	/* pbdma status */
	UNIT_TEST(read_pbdma_status_info, test_gm20b_read_pbdma_status_info, NULL, 0),
	UNIT_TEST(remove_support, test_fifo_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu_pbdma_gm20b, nvgpu_pbdma_gm20b_tests, UNIT_PRIO_NVGPU_TEST);
