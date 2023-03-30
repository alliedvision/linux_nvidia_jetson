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

#include <nvgpu/io.h>
#include <nvgpu/mm.h>
#include <nvgpu/fbp.h>
#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/bug.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/utils.h>

#include "perf_gv11b.h"

#include <nvgpu/hw/gv11b/hw_perf_gv11b.h>

#define PMM_ROUTER_OFFSET	0x200U

bool gv11b_perf_get_membuf_overflow_status(struct gk20a *g)
{
	const u32 st = perf_pmasys_control_membuf_status_overflowed_f();
	return st == (nvgpu_readl(g, perf_pmasys_control_r()) & st);
}

u32 gv11b_perf_get_membuf_pending_bytes(struct gk20a *g)
{
	return nvgpu_readl(g, perf_pmasys_mem_bytes_r());
}

void gv11b_perf_set_membuf_handled_bytes(struct gk20a *g,
	u32 entries, u32 entry_size)
{
	if (entries > 0U) {
		nvgpu_writel(g, perf_pmasys_mem_bump_r(), entries * entry_size);
	}
}

void gv11b_perf_membuf_reset_streaming(struct gk20a *g)
{
	u32 engine_status;
	u32 num_unread_bytes;

	engine_status = nvgpu_readl(g, perf_pmasys_enginestatus_r());
	WARN_ON(0U ==
		(engine_status & perf_pmasys_enginestatus_rbufempty_empty_f()));

	nvgpu_writel(g, perf_pmasys_control_r(),
		perf_pmasys_control_membuf_clear_status_doit_f());

	num_unread_bytes = nvgpu_readl(g, perf_pmasys_mem_bytes_r());
	if (num_unread_bytes != 0U) {
		nvgpu_writel(g, perf_pmasys_mem_bump_r(), num_unread_bytes);
	}
}

void gv11b_perf_enable_membuf(struct gk20a *g, u32 size, u64 buf_addr)
{
	u32 addr_lo;
	u32 addr_hi;

	addr_lo = u64_lo32(buf_addr);
	addr_hi = u64_hi32(buf_addr);

	nvgpu_writel(g, perf_pmasys_outbase_r(), addr_lo);
	nvgpu_writel(g, perf_pmasys_outbaseupper_r(),
		perf_pmasys_outbaseupper_ptr_f(addr_hi));
	nvgpu_writel(g, perf_pmasys_outsize_r(), size);
}

void gv11b_perf_disable_membuf(struct gk20a *g)
{
	nvgpu_writel(g, perf_pmasys_outbase_r(), 0);
	nvgpu_writel(g, perf_pmasys_outbaseupper_r(),
			perf_pmasys_outbaseupper_ptr_f(0));
	nvgpu_writel(g, perf_pmasys_outsize_r(), 0);
}

void gv11b_perf_bind_mem_bytes_buffer_addr(struct gk20a *g, u64 buf_addr)
{
	u32 addr_lo;

	/*
	 * For mem bytes addr, the upper 8 bits of the 40bit VA is taken
	 * from perf_pmasys_channel_outbaseupper_r(), so only consider
	 * the lower 32bits in the buf_addr and discard the rest.
	 */
	buf_addr = u64_lo32(buf_addr);
	buf_addr = buf_addr >> perf_pmasys_mem_bytes_addr_ptr_b();
	addr_lo = nvgpu_safe_cast_u64_to_u32(buf_addr);

	nvgpu_writel(g, perf_pmasys_mem_bytes_addr_r(),
		perf_pmasys_mem_bytes_addr_ptr_f(addr_lo));
}

int gv11b_perf_update_get_put(struct gk20a *g, u64 bytes_consumed,
		bool update_available_bytes, u64 *put_ptr,
		bool *overflowed)
{
	u32 val;

	if (bytes_consumed != 0U) {
		nvgpu_writel(g, perf_pmasys_mem_bump_r(), (u32)bytes_consumed);
	}

	if (update_available_bytes) {
		val = nvgpu_readl(g, perf_pmasys_control_r());
		val = set_field(val, perf_pmasys_control_update_bytes_m(),
				     perf_pmasys_control_update_bytes_doit_f());
		nvgpu_writel(g, perf_pmasys_control_r(), val);
	}

	if (put_ptr) {
		*put_ptr = (u64)nvgpu_readl(g, perf_pmasys_mem_head_r());
	}

	if (overflowed) {
		*overflowed = g->ops.perf.get_membuf_overflow_status(g);
	}

	return 0;
}

void gv11b_perf_init_inst_block(struct gk20a *g, struct nvgpu_mem *inst_block)
{
	u32 inst_block_ptr = nvgpu_inst_block_ptr(g, inst_block);

	nvgpu_writel(g, perf_pmasys_mem_block_r(),
		     perf_pmasys_mem_block_base_f(inst_block_ptr) |
		     perf_pmasys_mem_block_valid_true_f() |
		     nvgpu_aperture_mask(g, inst_block,
				perf_pmasys_mem_block_target_sys_ncoh_f(),
				perf_pmasys_mem_block_target_sys_coh_f(),
				perf_pmasys_mem_block_target_lfb_f()));
}

void gv11b_perf_deinit_inst_block(struct gk20a *g)
{
	nvgpu_writel(g, perf_pmasys_mem_block_r(),
			perf_pmasys_mem_block_base_f(0) |
			perf_pmasys_mem_block_valid_false_f() |
			perf_pmasys_mem_block_target_f(0));
}

u32 gv11b_perf_get_pmmsys_per_chiplet_offset(void)
{
	return (perf_pmmsys_extent_v() - perf_pmmsys_base_v() + 1U);
}

u32 gv11b_perf_get_pmmgpc_per_chiplet_offset(void)
{
	return (perf_pmmgpc_extent_v() - perf_pmmgpc_base_v() + 1U);
}

u32 gv11b_perf_get_pmmfbp_per_chiplet_offset(void)
{
	return (perf_pmmfbp_extent_v() - perf_pmmfbp_base_v() + 1U);
}

static const u32 hwpm_sys_perfmon_regs[] =
{
	/* This list is autogenerated. Do not edit. */
	0x00240040,
	0x00240044,
	0x00240048,
	0x0024004c,
	0x00240050,
	0x00240054,
	0x00240058,
	0x0024005c,
	0x00240060,
	0x00240064,
	0x00240068,
	0x0024006c,
	0x00240070,
	0x00240074,
	0x00240078,
	0x0024007c,
	0x00240080,
	0x00240084,
	0x00240088,
	0x0024008c,
	0x00240090,
	0x00240094,
	0x00240098,
	0x0024009c,
	0x002400a0,
	0x002400a4,
	0x002400a8,
	0x002400ac,
	0x002400b0,
	0x002400b4,
	0x002400b8,
	0x002400bc,
	0x002400c0,
	0x002400c4,
	0x002400c8,
	0x002400cc,
	0x002400d0,
	0x002400d4,
	0x002400d8,
	0x002400dc,
	0x002400e0,
	0x002400e4,
	0x002400e8,
	0x002400ec,
	0x002400f8,
	0x002400fc,
	0x00240104,
	0x00240108,
	0x0024010c,
	0x00240110,
	0x00240120,
	0x00240114,
	0x00240118,
	0x0024011c,
	0x00240124,
};

static const u32 hwpm_gpc_perfmon_regs[] =
{
	/* This list is autogenerated. Do not edit. */
	0x00278040,
	0x00278044,
	0x00278048,
	0x0027804c,
	0x00278050,
	0x00278054,
	0x00278058,
	0x0027805c,
	0x00278060,
	0x00278064,
	0x00278068,
	0x0027806c,
	0x00278070,
	0x00278074,
	0x00278078,
	0x0027807c,
	0x00278080,
	0x00278084,
	0x00278088,
	0x0027808c,
	0x00278090,
	0x00278094,
	0x00278098,
	0x0027809c,
	0x002780a0,
	0x002780a4,
	0x002780a8,
	0x002780ac,
	0x002780b0,
	0x002780b4,
	0x002780b8,
	0x002780bc,
	0x002780c0,
	0x002780c4,
	0x002780c8,
	0x002780cc,
	0x002780d0,
	0x002780d4,
	0x002780d8,
	0x002780dc,
	0x002780e0,
	0x002780e4,
	0x002780e8,
	0x002780ec,
	0x002780f8,
	0x002780fc,
	0x00278104,
	0x00278108,
	0x0027810c,
	0x00278110,
	0x00278120,
	0x00278114,
	0x00278118,
	0x0027811c,
	0x00278124,
};

static const u32 hwpm_fbp_perfmon_regs[] =
{
	/* This list is autogenerated. Do not edit. */
	0x0027c040,
	0x0027c044,
	0x0027c048,
	0x0027c04c,
	0x0027c050,
	0x0027c054,
	0x0027c058,
	0x0027c05c,
	0x0027c060,
	0x0027c064,
	0x0027c068,
	0x0027c06c,
	0x0027c070,
	0x0027c074,
	0x0027c078,
	0x0027c07c,
	0x0027c080,
	0x0027c084,
	0x0027c088,
	0x0027c08c,
	0x0027c090,
	0x0027c094,
	0x0027c098,
	0x0027c09c,
	0x0027c0a0,
	0x0027c0a4,
	0x0027c0a8,
	0x0027c0ac,
	0x0027c0b0,
	0x0027c0b4,
	0x0027c0b8,
	0x0027c0bc,
	0x0027c0c0,
	0x0027c0c4,
	0x0027c0c8,
	0x0027c0cc,
	0x0027c0d0,
	0x0027c0d4,
	0x0027c0d8,
	0x0027c0dc,
	0x0027c0e0,
	0x0027c0e4,
	0x0027c0e8,
	0x0027c0ec,
	0x0027c0f8,
	0x0027c0fc,
	0x0027c104,
	0x0027c108,
	0x0027c10c,
	0x0027c110,
	0x0027c120,
	0x0027c114,
	0x0027c118,
	0x0027c11c,
	0x0027c124,
};

const u32 *gv11b_perf_get_hwpm_sys_perfmon_regs(u32 *count)
{
	*count = sizeof(hwpm_sys_perfmon_regs) / sizeof(hwpm_sys_perfmon_regs[0]);
	return hwpm_sys_perfmon_regs;
}

const u32 *gv11b_perf_get_hwpm_gpc_perfmon_regs(u32 *count)
{
	*count = sizeof(hwpm_gpc_perfmon_regs) / sizeof(hwpm_gpc_perfmon_regs[0]);
	return hwpm_gpc_perfmon_regs;
}

const u32 *gv11b_perf_get_hwpm_fbp_perfmon_regs(u32 *count)
{
	*count = sizeof(hwpm_fbp_perfmon_regs) / sizeof(hwpm_fbp_perfmon_regs[0]);
	return hwpm_fbp_perfmon_regs;
}

void gv11b_perf_set_pmm_register(struct gk20a *g, u32 offset, u32 val,
		u32 num_chiplets, u32 chiplet_stride, u32 num_perfmons)
{
	u32 perfmon_index = 0;
	u32 chiplet_index = 0;
	u32 reg_offset = 0;

	for (chiplet_index = 0; chiplet_index < num_chiplets; chiplet_index++) {
		for (perfmon_index = 0; perfmon_index < num_perfmons;
						perfmon_index++) {
			reg_offset = offset + perfmon_index *
				perf_pmmsys_perdomain_offset_v() +
				chiplet_index * chiplet_stride;
			nvgpu_writel(g, reg_offset, val);
		}
	}
}

void gv11b_perf_get_num_hwpm_perfmon(struct gk20a *g, u32 *num_sys_perfmon,
				u32 *num_fbp_perfmon, u32 *num_gpc_perfmon)
{
	int err;
	u32 buf_offset_lo, buf_offset_addr, num_offsets;
	u32 perfmon_index = 0;

	for (perfmon_index = 0; perfmon_index <
			perf_pmmsys_engine_sel__size_1_v();
			perfmon_index++) {
		err = g->ops.gr.get_pm_ctx_buffer_offsets(g,
				perf_pmmsys_engine_sel_r(perfmon_index),
				1,
				&buf_offset_lo,
				&buf_offset_addr,
				&num_offsets);
		if (err != 0) {
			break;
		}
	}
	*num_sys_perfmon = perfmon_index;

	for (perfmon_index = 0; perfmon_index <
			perf_pmmfbp_engine_sel__size_1_v();
			perfmon_index++) {
		err = g->ops.gr.get_pm_ctx_buffer_offsets(g,
				perf_pmmfbp_engine_sel_r(perfmon_index),
				1,
				&buf_offset_lo,
				&buf_offset_addr,
				&num_offsets);
		if (err != 0) {
			break;
		}
	}
	*num_fbp_perfmon = perfmon_index;

	for (perfmon_index = 0; perfmon_index <
			perf_pmmgpc_engine_sel__size_1_v();
			perfmon_index++) {
		err = g->ops.gr.get_pm_ctx_buffer_offsets(g,
				perf_pmmgpc_engine_sel_r(perfmon_index),
				1,
				&buf_offset_lo,
				&buf_offset_addr,
				&num_offsets);
		if (err != 0) {
			break;
		}
	}
	*num_gpc_perfmon = perfmon_index;
}

void gv11b_perf_reset_hwpm_pmm_registers(struct gk20a *g)
{
	u32 count;
	const u32 *perfmon_regs;
	u32 i;

	perfmon_regs = g->ops.perf.get_hwpm_sys_perfmon_regs(&count);

	for (i = 0U; i < count; i++) {
		g->ops.perf.set_pmm_register(g, perfmon_regs[i], 0U, 1U,
			g->ops.perf.get_pmmsys_per_chiplet_offset(),
			g->num_sys_perfmon);
	}

	/*
	 * All the registers are broadcast ones so trigger
	 * g->ops.gr.set_pmm_register() only with 1 chiplet even for
	 * GPC and FBP chiplets.
	 */
	perfmon_regs = g->ops.perf.get_hwpm_fbp_perfmon_regs(&count);

	for (i = 0U; i < count; i++) {
		g->ops.perf.set_pmm_register(g, perfmon_regs[i], 0U, 1U,
			g->ops.perf.get_pmmfbp_per_chiplet_offset(),
			g->num_fbp_perfmon);
	}

	perfmon_regs = g->ops.perf.get_hwpm_gpc_perfmon_regs(&count);

	for (i = 0U; i < count; i++) {
		g->ops.perf.set_pmm_register(g, perfmon_regs[i], 0U, 1U,
			g->ops.perf.get_pmmgpc_per_chiplet_offset(),
			g->num_gpc_perfmon);
	}

	if (g->ops.priv_ring.read_pri_fence != NULL) {
		g->ops.priv_ring.read_pri_fence(g);
	}
}

void gv11b_perf_init_hwpm_pmm_register(struct gk20a *g)
{
	g->ops.perf.set_pmm_register(g, perf_pmmsys_engine_sel_r(0), 0xFFFFFFFFU,
		1U, g->ops.perf.get_pmmsys_per_chiplet_offset(),
		g->num_sys_perfmon);
	g->ops.perf.set_pmm_register(g, perf_pmmfbp_engine_sel_r(0), 0xFFFFFFFFU,
		nvgpu_fbp_get_num_fbps(g->fbp),
		g->ops.perf.get_pmmfbp_per_chiplet_offset(),
		g->num_fbp_perfmon);
	g->ops.perf.set_pmm_register(g, perf_pmmgpc_engine_sel_r(0), 0xFFFFFFFFU,
		nvgpu_gr_config_get_gpc_count(nvgpu_gr_get_config_ptr(g)),
		g->ops.perf.get_pmmgpc_per_chiplet_offset(),
		g->num_gpc_perfmon);
}

void gv11b_perf_pma_stream_enable(struct gk20a *g, bool enable)
{
	u32 reg_val;

	reg_val = nvgpu_readl(g, perf_pmasys_control_r());

	if (enable) {
		reg_val = set_field(reg_val,
				perf_pmasys_control_stream_m(),
				perf_pmasys_control_stream_enable_f());
	} else {
		reg_val = set_field(reg_val,
				perf_pmasys_control_stream_m(),
				perf_pmasys_control_stream_disable_f());
	}

	nvgpu_writel(g, perf_pmasys_control_r(), reg_val);
}

void gv11b_perf_disable_all_perfmons(struct gk20a *g)
{
	g->ops.perf.set_pmm_register(g, perf_pmmsys_control_r(0U), 0U, 1U,
		g->ops.perf.get_pmmsys_per_chiplet_offset(),
		g->num_sys_perfmon);

	g->ops.perf.set_pmm_register(g, perf_pmmfbp_fbps_control_r(0U), 0U, 1U,
		g->ops.perf.get_pmmfbp_per_chiplet_offset(),
		g->num_fbp_perfmon);

	g->ops.perf.set_pmm_register(g, perf_pmmgpc_gpcs_control_r(0U), 0U, 1U,
		g->ops.perf.get_pmmgpc_per_chiplet_offset(),
		g->num_gpc_perfmon);

	if (g->ops.priv_ring.read_pri_fence != NULL) {
		g->ops.priv_ring.read_pri_fence(g);
	}
}

static int poll_for_pmm_router_idle(struct gk20a *g, u32 offset, u32 timeout_ms)
{
	struct nvgpu_timeout timeout;
	u32 reg_val;
	u32 status;

	nvgpu_timeout_init_cpu_timer(g, &timeout, timeout_ms);

	do {
		reg_val = nvgpu_readl(g, offset);
		status = perf_pmmsysrouter_enginestatus_status_v(reg_val);

		if ((status == perf_pmmsysrouter_enginestatus_status_empty_v()) ||
		    (status == perf_pmmsysrouter_enginestatus_status_quiescent_v())) {
			return 0;
		}

		nvgpu_usleep_range(20, 40);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	return -ETIMEDOUT;
}

int gv11b_perf_wait_for_idle_pmm_routers(struct gk20a *g)
{
	u32 num_gpc, num_fbp;
	int err;
	u32 i;

	num_gpc = nvgpu_gr_config_get_gpc_count(nvgpu_gr_get_config_ptr(g));
	num_fbp = nvgpu_fbp_get_num_fbps(g->fbp);

	/* wait for all perfmons to report idle */
	err = poll_for_pmm_router_idle(g, perf_pmmsysrouter_perfmonstatus_r(), 1);
	if (err != 0) {
		return err;
	}

	for (i = 0U; i < num_gpc; ++i) {
		err = poll_for_pmm_router_idle(g,
			perf_pmmgpcrouter_perfmonstatus_r() + (i * PMM_ROUTER_OFFSET),
			1);
		if (err != 0) {
			return err;
		}
	}

	for (i = 0U; i < num_fbp; ++i) {
		err = poll_for_pmm_router_idle(g,
			perf_pmmfbprouter_perfmonstatus_r() + (i * PMM_ROUTER_OFFSET),
			1);
		if (err != 0) {
			return err;
		}
	}

	/* wait for all routers to report idle */
	err = poll_for_pmm_router_idle(g, perf_pmmsysrouter_enginestatus_r(), 1);
	if (err != 0) {
		return err;
	}

	for (i = 0U; i < num_gpc; ++i) {
		err = poll_for_pmm_router_idle(g,
			perf_pmmgpcrouter_enginestatus_r() + (i * PMM_ROUTER_OFFSET),
			1);
		if (err != 0) {
			return err;
		}
	}

	for (i = 0U; i < num_fbp; ++i) {
		err = poll_for_pmm_router_idle(g,
			perf_pmmfbprouter_enginestatus_r() + (i * PMM_ROUTER_OFFSET),
			1);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}

int gv11b_perf_wait_for_idle_pma(struct gk20a *g)
{
	struct nvgpu_timeout timeout;
	u32 status, rbufempty_status;
	u32 timeout_ms = 1;
	u32 reg_val;

	nvgpu_timeout_init_cpu_timer(g, &timeout, timeout_ms);

	do {
		reg_val = nvgpu_readl(g, perf_pmasys_enginestatus_r());

		status = perf_pmasys_enginestatus_status_v(reg_val);
		rbufempty_status = perf_pmasys_enginestatus_rbufempty_v(reg_val);

		if ((status == perf_pmasys_enginestatus_status_empty_v()) &&
		    (rbufempty_status == perf_pmasys_enginestatus_rbufempty_empty_v())) {
			return 0;
		}

		nvgpu_usleep_range(20, 40);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	return -ETIMEDOUT;
}
