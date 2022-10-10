/*
 * GK20A Graphics
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/dma.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/bug.h>
#include <nvgpu/enabled.h>
#include <nvgpu/debugger.h>
#include <nvgpu/netlist.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/string.h>
#include <nvgpu/regops.h>
#include <nvgpu/gr/subctx.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/gr/gr_intr.h>
#include <nvgpu/gr/obj_ctx.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/hwpm_map.h>
#include <nvgpu/preempt.h>
#include <nvgpu/power_features/pg.h>

#include "gr_gk20a.h"
#include "gr_pri_gk20a.h"

#include "common/gr/gr_priv.h"

#include <nvgpu/hw/gk20a/hw_gr_gk20a.h>

int gr_gk20a_update_smpc_ctxsw_mode(struct gk20a *g,
				    struct nvgpu_tsg *tsg,
				    bool enable_smpc_ctxsw)
{
	int ret;

	nvgpu_log_fn(g, " ");

	g->ops.tsg.disable(tsg);

	ret = g->ops.fifo.preempt_tsg(g, tsg);
	if (ret != 0) {
		nvgpu_err(g, "failed to preempt TSG");
		goto out;
	}

	ret = nvgpu_gr_ctx_set_smpc_mode(g, tsg->gr_ctx, enable_smpc_ctxsw);

out:
	g->ops.tsg.enable(tsg);
	return ret;
}

int gr_gk20a_update_hwpm_ctxsw_mode(struct gk20a *g,
				  u32 gr_instance_id,
				  struct nvgpu_tsg *tsg,
				  u32 mode)
{
	struct nvgpu_channel *ch;
	struct nvgpu_gr_ctx *gr_ctx;
	bool skip_update = false;
	int err;
	int ret;
	struct nvgpu_gr *gr = nvgpu_gr_get_instance_ptr(g, gr_instance_id);

	nvgpu_log_fn(g, " ");

	gr_ctx = tsg->gr_ctx;

	if (mode != NVGPU_GR_CTX_HWPM_CTXSW_MODE_NO_CTXSW) {
		nvgpu_gr_ctx_set_size(gr->gr_ctx_desc,
			NVGPU_GR_CTX_PM_CTX,
			nvgpu_gr_hwpm_map_get_size(gr->hwpm_map));

		ret = nvgpu_gr_ctx_alloc_pm_ctx(g, gr_ctx,
			gr->gr_ctx_desc, tsg->vm);
		if (ret != 0) {
			nvgpu_err(g,
				"failed to allocate pm ctxt buffer");
			return ret;
		}

		if ((mode == NVGPU_GR_CTX_HWPM_CTXSW_MODE_STREAM_OUT_CTXSW) &&
			(g->ops.perf.init_hwpm_pmm_register != NULL)) {
			g->ops.perf.init_hwpm_pmm_register(g);
		}
	}

	ret = nvgpu_gr_ctx_prepare_hwpm_mode(g, gr_ctx, mode, &skip_update);
	if (ret != 0) {
		return ret;
	}
	if (skip_update) {
		return 0;
	}

	g->ops.tsg.disable(tsg);

	ret = g->ops.fifo.preempt_tsg(g, tsg);
	if (ret != 0) {
		nvgpu_err(g, "failed to preempt TSG");
		goto out;
	}

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);

	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		if (ch->subctx != NULL) {
			err = nvgpu_gr_ctx_set_hwpm_mode(g, gr_ctx, false);
			if (err != 0) {
				nvgpu_err(g, "chid: %d set_hwpm_mode failed",
					ch->chid);
				ret = err;
				continue;
			}
			nvgpu_gr_subctx_set_hwpm_mode(g, ch->subctx, gr_ctx);
		} else {
			ret = nvgpu_gr_ctx_set_hwpm_mode(g, gr_ctx, true);
			break;
		}
	}

	nvgpu_rwsem_up_read(&tsg->ch_list_lock);

out:
	g->ops.tsg.enable(tsg);

	return ret;
}

int gk20a_gr_lock_down_sm(struct gk20a *g,
			 u32 gpc, u32 tpc, u32 sm, u32 global_esr_mask,
			 bool check_errors)
{
	u32 offset = nvgpu_gr_gpc_offset(g, gpc) + nvgpu_gr_tpc_offset(g, tpc);
	u32 dbgr_control0;

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			"GPC%d TPC%d SM%d: assert stop trigger", gpc, tpc, sm);

	/* assert stop trigger */
	dbgr_control0 =
		gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset);
	dbgr_control0 |= gr_gpc0_tpc0_sm_dbgr_control0_stop_trigger_enable_f();
	gk20a_writel(g,
		gr_gpc0_tpc0_sm_dbgr_control0_r() + offset, dbgr_control0);

	return g->ops.gr.wait_for_sm_lock_down(g, gpc, tpc, sm, global_esr_mask,
			check_errors);
}

bool gk20a_gr_sm_debugger_attached(struct gk20a *g)
{
	u32 dbgr_control0 = gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r());

	/* check if an sm debugger is attached.
	 * assumption: all SMs will have debug mode enabled/disabled
	 * uniformly. */
	if (gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_v(dbgr_control0) ==
			gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_on_v()) {
		return true;
	}

	return false;
}

/* This function will decode a priv address and return the partition type and numbers. */
int gr_gk20a_decode_priv_addr(struct gk20a *g, u32 addr,
			      enum ctxsw_addr_type *addr_type,
			      u32 *gpc_num, u32 *tpc_num, u32 *ppc_num, u32 *rop_num,
			      u32 *broadcast_flags)
{
	u32 gpc_addr;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	/* setup defaults */
	*addr_type = CTXSW_ADDR_TYPE_SYS;
	*broadcast_flags = PRI_BROADCAST_FLAGS_NONE;
	*gpc_num = 0;
	*tpc_num = 0;
	*ppc_num = 0;
	*rop_num  = 0;

	if (pri_is_gpc_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_GPC;
		gpc_addr = pri_gpccs_addr_mask(g, addr);
		if (pri_is_gpc_addr_shared(g, addr)) {
			*addr_type = CTXSW_ADDR_TYPE_GPC;
			*broadcast_flags |= PRI_BROADCAST_FLAGS_GPC;
		} else {
			*gpc_num = pri_get_gpc_num(g, addr);
		}

		if (pri_is_ppc_addr(g, gpc_addr)) {
			*addr_type = CTXSW_ADDR_TYPE_PPC;
			if (pri_is_ppc_addr_shared(g, gpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_PPC;
				return 0;
			}
		}
		if (nvgpu_gr_is_tpc_addr(g, gpc_addr)) {
			*addr_type = CTXSW_ADDR_TYPE_TPC;
			if (pri_is_tpc_addr_shared(g, gpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_TPC;
				return 0;
			}
			*tpc_num = nvgpu_gr_get_tpc_num(g, gpc_addr);
		}
		return 0;
	} else if (pri_is_rop_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_ROP;
		if (pri_is_rop_addr_shared(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_ROP;
			return 0;
		}
		*rop_num = pri_get_rop_num(g, addr);
		return 0;
	} else if (g->ops.ltc.pri_is_ltc_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_LTCS;
		if (g->ops.ltc.is_ltcs_ltss_addr(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_LTCS;
		} else if (g->ops.ltc.is_ltcn_ltss_addr(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_LTSS;
		}
		return 0;
	} else if (pri_is_fbpa_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_FBPA;
		if (pri_is_fbpa_addr_shared(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_FBPA;
			return 0;
		}
		return 0;
	} else if ((g->ops.gr.is_egpc_addr != NULL) &&
		   g->ops.gr.is_egpc_addr(g, addr)) {
			return g->ops.gr.decode_egpc_addr(g,
					addr, addr_type, gpc_num,
					tpc_num, broadcast_flags);
	} else {
		*addr_type = CTXSW_ADDR_TYPE_SYS;
		return 0;
	}
	/* PPC!?!?!?! */

	/*NOTREACHED*/
	return -EINVAL;
}

void gr_gk20a_split_fbpa_broadcast_addr(struct gk20a *g, u32 addr,
				      u32 num_fbpas,
				      u32 *priv_addr_table, u32 *t)
{
	u32 fbpa_id;

	for (fbpa_id = 0; fbpa_id < num_fbpas; fbpa_id++) {
		priv_addr_table[(*t)++] = pri_fbpa_addr(g,
				pri_fbpa_addr_mask(g, addr), fbpa_id);
	}
}

int gr_gk20a_split_ppc_broadcast_addr(struct gk20a *g, u32 addr,
				      u32 gpc_num,
				      u32 *priv_addr_table, u32 *t)
{
	u32 ppc_num;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	for (ppc_num = 0;
	     ppc_num < nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_num);
	     ppc_num++) {
		priv_addr_table[(*t)++] = pri_ppc_addr(g, pri_ppccs_addr_mask(addr),
		gpc_num, ppc_num);
	}

	return 0;
}

/*
 * The context buffer is indexed using BE broadcast addresses and GPC/TPC
 * unicast addresses. This function will convert a BE unicast address to a BE
 * broadcast address and split a GPC/TPC broadcast address into a table of
 * GPC/TPC addresses.  The addresses generated by this function can be
 * successfully processed by gr_gk20a_find_priv_offset_in_buffer
 */
int gr_gk20a_create_priv_addr_table(struct gk20a *g,
					   u32 addr,
					   u32 *priv_addr_table,
					   u32 *num_registers)
{
	enum ctxsw_addr_type addr_type;
	u32 gpc_num, tpc_num, ppc_num, rop_num;
	u32 priv_addr, gpc_addr;
	u32 broadcast_flags;
	u32 t;
	int err;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	struct nvgpu_gr_config *gr_config = gr->config;

	t = 0;
	*num_registers = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	err = g->ops.gr.decode_priv_addr(g, addr, &addr_type,
					&gpc_num, &tpc_num, &ppc_num, &rop_num,
					&broadcast_flags);
	nvgpu_log(g, gpu_dbg_gpu_dbg, "addr_type = %d", addr_type);
	if (err != 0) {
		return err;
	}

	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_ROP)) {
		/*
		 * The ROP broadcast registers are included in the compressed
		 * PRI table. Convert a ROP unicast address to a broadcast
		 * address so that we can look up the offset.
		 */
		if ((addr_type == CTXSW_ADDR_TYPE_ROP) &&
		    ((broadcast_flags & PRI_BROADCAST_FLAGS_ROP) == 0U)) {
			priv_addr_table[t++] = pri_rop_shared_addr(g, addr);
		} else {
			priv_addr_table[t++] = addr;
		}

		*num_registers = t;
		return 0;
	}

	/* The GPC/TPC unicast registers are included in the compressed PRI
	 * tables. Convert a GPC/TPC broadcast address to unicast addresses so
	 * that we can look up the offsets. */
	if ((broadcast_flags & PRI_BROADCAST_FLAGS_GPC) != 0U) {
		for (gpc_num = 0;
		     gpc_num < nvgpu_gr_config_get_gpc_count(gr_config);
		     gpc_num++) {

			if ((broadcast_flags & PRI_BROADCAST_FLAGS_TPC) != 0U) {
				for (tpc_num = 0;
				     tpc_num < nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc_num);
				     tpc_num++) {
					priv_addr_table[t++] =
						pri_tpc_addr(g,
							pri_tpccs_addr_mask(g, addr),
							gpc_num, tpc_num);
				}

			} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_PPC) != 0U) {
				err = gr_gk20a_split_ppc_broadcast_addr(g, addr, gpc_num,
							       priv_addr_table, &t);
				if (err != 0) {
					return err;
				}
			} else {
				priv_addr = pri_gpc_addr(g,
						pri_gpccs_addr_mask(g, addr),
						gpc_num);

				gpc_addr = pri_gpccs_addr_mask(g, priv_addr);
				tpc_num = nvgpu_gr_get_tpc_num(g, gpc_addr);
				if (tpc_num >= nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc_num)) {
					continue;
				}

				priv_addr_table[t++] = priv_addr;
			}
		}
	} else if (((addr_type == CTXSW_ADDR_TYPE_EGPC) ||
			(addr_type == CTXSW_ADDR_TYPE_ETPC)) &&
			(g->ops.gr.egpc_etpc_priv_addr_table != NULL)) {
		nvgpu_log(g, gpu_dbg_gpu_dbg, "addr_type : EGPC/ETPC");
		g->ops.gr.egpc_etpc_priv_addr_table(g, addr, gpc_num, tpc_num,
				broadcast_flags, priv_addr_table, &t);
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_LTSS) != 0U) {
		g->ops.ltc.split_lts_broadcast_addr(g, addr,
							priv_addr_table, &t);
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_LTCS) != 0U) {
		g->ops.ltc.split_ltc_broadcast_addr(g, addr,
							priv_addr_table, &t);
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_FBPA) != 0U) {
		g->ops.gr.split_fbpa_broadcast_addr(g, addr,
				nvgpu_get_litter_value(g, GPU_LIT_NUM_FBPAS),
				priv_addr_table, &t);
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_GPC) == 0U) {
		if ((broadcast_flags & PRI_BROADCAST_FLAGS_TPC) != 0U) {
			for (tpc_num = 0;
			     tpc_num < nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc_num);
			     tpc_num++) {
				priv_addr_table[t++] =
					pri_tpc_addr(g,
						pri_tpccs_addr_mask(g, addr),
						gpc_num, tpc_num);
			}
		} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_PPC) != 0U) {
			err = gr_gk20a_split_ppc_broadcast_addr(g,
					addr, gpc_num, priv_addr_table, &t);
		} else {
			priv_addr_table[t++] = addr;
		}
	}

	*num_registers = t;
	return 0;
}

int gr_gk20a_get_ctx_buffer_offsets(struct gk20a *g,
				    u32 addr,
				    u32 max_offsets,
				    u32 *offsets, u32 *offset_addrs,
				    u32 *num_offsets)
{
	u32 i;
	u32 priv_offset = 0;
	u32 *priv_registers;
	u32 num_registers = 0;
	int err = 0;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);
	u32 potential_offsets = nvgpu_gr_config_get_max_gpc_count(gr->config) *
		nvgpu_gr_config_get_max_tpc_per_gpc_count(gr->config) *
		sm_per_tpc;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	/* implementation is crossed-up if either of these happen */
	if (max_offsets > potential_offsets) {
		nvgpu_log_fn(g, "max_offsets > potential_offsets");
		return -EINVAL;
	}

	if (!nvgpu_gr_obj_ctx_is_golden_image_ready(gr->golden_image)) {
		nvgpu_log_fn(g, "no context switch header info to work with");
		return -ENODEV;
	}

	priv_registers = nvgpu_kzalloc(g, sizeof(u32) * potential_offsets);
	if (priv_registers == NULL) {
		nvgpu_log_fn(g, "failed alloc for potential_offsets=%d", potential_offsets);
		err = -ENOMEM;
		goto cleanup;
	}
	(void) memset(offsets,      0, sizeof(u32) * max_offsets);
	(void) memset(offset_addrs, 0, sizeof(u32) * max_offsets);
	*num_offsets = 0;

	g->ops.gr.create_priv_addr_table(g, addr, &priv_registers[0],
			&num_registers);

	if ((max_offsets > 1U) && (num_registers > max_offsets)) {
		nvgpu_log_fn(g, "max_offsets = %d, num_registers = %d",
				max_offsets, num_registers);
		err = -EINVAL;
		goto cleanup;
	}

	if ((max_offsets == 1U) && (num_registers > 1U)) {
		num_registers = 1;
	}

	for (i = 0; i < num_registers; i++) {
		err = g->ops.gr.find_priv_offset_in_buffer(g,
			  priv_registers[i],
			  nvgpu_gr_obj_ctx_get_local_golden_image_ptr(
				gr->golden_image),
			  (u32)nvgpu_gr_obj_ctx_get_golden_image_size(
				gr->golden_image),
			  &priv_offset);
		if (err != 0) {
			nvgpu_log_fn(g, "Could not determine priv_offset for addr:0x%x",
				      addr); /*, grPriRegStr(addr)));*/
			goto cleanup;
		}

		offsets[i] = priv_offset;
		offset_addrs[i] = priv_registers[i];
	}

	*num_offsets = num_registers;
cleanup:
	if (priv_registers != NULL) {
		nvgpu_kfree(g, priv_registers);
	}

	return err;
}

int gr_gk20a_get_pm_ctx_buffer_offsets(struct gk20a *g,
				       u32 addr,
				       u32 max_offsets,
				       u32 *offsets, u32 *offset_addrs,
				       u32 *num_offsets)
{
	u32 i;
	u32 priv_offset = 0;
	u32 *priv_registers;
	u32 num_registers = 0;
	int err = 0;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);
	u32 potential_offsets = nvgpu_gr_config_get_max_gpc_count(gr->config) *
		nvgpu_gr_config_get_max_tpc_per_gpc_count(gr->config) *
		sm_per_tpc;

	nvgpu_log(g, gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	/* implementation is crossed-up if either of these happen */
	if (max_offsets > potential_offsets) {
		return -EINVAL;
	}

	if (!nvgpu_gr_obj_ctx_is_golden_image_ready(gr->golden_image)) {
		nvgpu_log_fn(g, "no context switch header info to work with");
		return -ENODEV;
	}

	priv_registers = nvgpu_kzalloc(g, sizeof(u32) * potential_offsets);
	if (priv_registers == NULL) {
		nvgpu_log_fn(g, "failed alloc for potential_offsets=%d", potential_offsets);
		return -ENOMEM;
	}
	(void) memset(offsets,      0, sizeof(u32) * max_offsets);
	(void) memset(offset_addrs, 0, sizeof(u32) * max_offsets);
	*num_offsets = 0;

	g->ops.gr.create_priv_addr_table(g, addr, priv_registers,
			&num_registers);

	if ((max_offsets > 1U) && (num_registers > max_offsets)) {
		err = -EINVAL;
		goto cleanup;
	}

	if ((max_offsets == 1U) && (num_registers > 1U)) {
		num_registers = 1;
	}

	for (i = 0; i < num_registers; i++) {
		err = nvgpu_gr_hwmp_map_find_priv_offset(g, gr->hwpm_map,
						  priv_registers[i],
						  &priv_offset, gr->config);
		if (err != 0) {
			nvgpu_log_fn(g, "Could not determine priv_offset for addr:0x%x",
				      addr); /*, grPriRegStr(addr)));*/
			goto cleanup;
		}

		offsets[i] = priv_offset;
		offset_addrs[i] = priv_registers[i];
	}

	*num_offsets = num_registers;
cleanup:
	nvgpu_kfree(g, priv_registers);

	return err;
}

/* Setup some register tables.  This looks hacky; our
 * register/offset functions are just that, functions.
 * So they can't be used as initializers... TBD: fix to
 * generate consts at least on an as-needed basis.
 */
static const u32 _num_ovr_perf_regs = 17;
static u32 _ovr_perf_regs[17] = { 0, };
/* Following are the blocks of registers that the ucode
 stores in the extended region.*/

void gk20a_gr_init_ovr_sm_dsm_perf(void)
{
	if (_ovr_perf_regs[0] != 0U) {
		return;
	}

	_ovr_perf_regs[0] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control_sel0_r();
	_ovr_perf_regs[1] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control_sel1_r();
	_ovr_perf_regs[2] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control0_r();
	_ovr_perf_regs[3] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control5_r();
	_ovr_perf_regs[4] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_status1_r();
	_ovr_perf_regs[5] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter0_control_r();
	_ovr_perf_regs[6] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter1_control_r();
	_ovr_perf_regs[7] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter2_control_r();
	_ovr_perf_regs[8] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter3_control_r();
	_ovr_perf_regs[9] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter4_control_r();
	_ovr_perf_regs[10] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter5_control_r();
	_ovr_perf_regs[11] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter6_control_r();
	_ovr_perf_regs[12] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter7_control_r();
	_ovr_perf_regs[13] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter4_r();
	_ovr_perf_regs[14] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter5_r();
	_ovr_perf_regs[15] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter6_r();
	_ovr_perf_regs[16] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter7_r();

}

/* TBD: would like to handle this elsewhere, at a higher level.
 * these are currently constructed in a "test-then-write" style
 * which makes it impossible to know externally whether a ctx
 * write will actually occur. so later we should put a lazy,
 *  map-and-hold system in the patch write state */
int gr_gk20a_ctx_patch_smpc(struct gk20a *g,
			    u32 addr, u32 data,
			    struct nvgpu_gr_ctx *gr_ctx)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 num_gpc = nvgpu_gr_config_get_gpc_count(gr->config);
	u32 num_tpc;
	u32 tpc, gpc, reg;
	u32 chk_addr;
	u32 num_ovr_perf_regs = 0;
	u32 *ovr_perf_regs = NULL;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	g->ops.gr.init_ovr_sm_dsm_perf();
	g->ops.gr.init_sm_dsm_reg_info();
	g->ops.gr.get_ovr_perf_regs(g, &num_ovr_perf_regs, &ovr_perf_regs);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	for (reg = 0; reg < num_ovr_perf_regs; reg++) {
		for (gpc = 0; gpc < num_gpc; gpc++)  {
			num_tpc = nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc);
			for (tpc = 0; tpc < num_tpc; tpc++) {
				chk_addr = ((gpc_stride * gpc) +
					    (tpc_in_gpc_stride * tpc) +
					    ovr_perf_regs[reg]);
				if (chk_addr != addr) {
					continue;
				}
				/* reset the patch count from previous
				   runs,if ucode has already processed
				   it */
				nvgpu_gr_ctx_reset_patch_count(g, gr_ctx);

				nvgpu_gr_ctx_patch_write(g, gr_ctx,
							 addr, data, true);

				nvgpu_gr_ctx_set_patch_ctx(g, gr_ctx,
						true);

				/* we're not caching these on cpu side,
				   but later watch for it */
				return 0;
			}
		}
	}

	return 0;
}

#define ILLEGAL_ID	~U32(0U)

void gk20a_gr_get_ovr_perf_regs(struct gk20a *g, u32 *num_ovr_perf_regs,
					       u32 **ovr_perf_regs)
{
	(void)g;
	*num_ovr_perf_regs = _num_ovr_perf_regs;
	*ovr_perf_regs = _ovr_perf_regs;
}

int gr_gk20a_find_priv_offset_in_ext_buffer(struct gk20a *g,
						u32 addr,
						u32 *context_buffer,
						u32 context_buffer_size,
						u32 *priv_offset)
{
	u32 i;
	u32 gpc_num, tpc_num;
	u32 num_gpcs;
	u32 chk_addr;
	u32 ext_priv_offset, ext_priv_size;
	u32 *context;
	u32 offset_to_segment, offset_to_segment_end;
	u32 sm_dsm_perf_reg_id = ILLEGAL_ID;
	u32 sm_dsm_perf_ctrl_reg_id = ILLEGAL_ID;
	u32 num_ext_gpccs_ext_buffer_segments;
	u32 inter_seg_offset;
	u32 max_tpc_count;
	u32 *sm_dsm_perf_ctrl_regs = NULL;
	u32 num_sm_dsm_perf_ctrl_regs = 0;
	u32 *sm_dsm_perf_regs = NULL;
	u32 num_sm_dsm_perf_regs = 0;
	u32 buffer_segments_size = 0;
	u32 marker_size = 0;
	u32 control_register_stride = 0;
	u32 perf_register_stride = 0;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc_base = nvgpu_get_litter_value(g, GPU_LIT_GPC_BASE);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 tpc_gpc_mask = (tpc_in_gpc_stride - 1U);

	(void)context_buffer_size;

	/* Only have TPC registers in extended region, so if not a TPC reg,
	   then return error so caller can look elsewhere. */
	if (pri_is_gpc_addr(g, addr))   {
		u32 gpc_addr = 0;
		gpc_num = pri_get_gpc_num(g, addr);
		gpc_addr = pri_gpccs_addr_mask(g, addr);
		if (nvgpu_gr_is_tpc_addr(g, gpc_addr)) {
			tpc_num = nvgpu_gr_get_tpc_num(g, gpc_addr);
		} else {
			return -EINVAL;
		}

		nvgpu_log_info(g, " gpc = %d tpc = %d",
				gpc_num, tpc_num);
	} else if ((g->ops.gr.is_etpc_addr != NULL) &&
				g->ops.gr.is_etpc_addr(g, addr)) {
			g->ops.gr.get_egpc_etpc_num(g, addr, &gpc_num, &tpc_num);
			gpc_base = g->ops.gr.get_egpc_base(g);
	} else {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
				"does not exist in extended region");
		return -EINVAL;
	}

	buffer_segments_size = g->ops.gr.ctxsw_prog.hw_get_extended_buffer_segments_size_in_bytes();
	/* note below is in words/num_registers */
	marker_size = g->ops.gr.ctxsw_prog.hw_extended_marker_size_in_bytes() >> 2;

	context = context_buffer;
	/* sanity check main header */
	if (!g->ops.gr.ctxsw_prog.check_main_image_header_magic(context)) {
		nvgpu_err(g,
			   "Invalid main header: magic value");
		return -EINVAL;
	}
	num_gpcs = g->ops.gr.ctxsw_prog.get_num_gpcs(context);
	if (gpc_num >= num_gpcs) {
		nvgpu_err(g,
		   "GPC 0x%08x is greater than total count 0x%08x!",
			   gpc_num, num_gpcs);
		return -EINVAL;
	}

	g->ops.gr.ctxsw_prog.get_extended_buffer_size_offset(context,
		&ext_priv_size, &ext_priv_offset);
	if (0U == ext_priv_size) {
		nvgpu_log_info(g, " No extended memory in context buffer");
		return -EINVAL;
	}

	offset_to_segment = ext_priv_offset * 256U;
	offset_to_segment_end = offset_to_segment +
		(ext_priv_size * buffer_segments_size);

	/* check local header magic */
	context += (g->ops.gr.ctxsw_prog.hw_get_fecs_header_size() >> 2);
	if (!g->ops.gr.ctxsw_prog.check_local_header_magic(context)) {
		nvgpu_err(g,
			   "Invalid local header: magic value");
		return -EINVAL;
	}

	/*
	 * See if the incoming register address is in the first table of
	 * registers. We check this by decoding only the TPC addr portion.
	 * If we get a hit on the TPC bit, we then double check the address
	 * by computing it from the base gpc/tpc strides.  Then make sure
	 * it is a real match.
	 */
	g->ops.gr.get_sm_dsm_perf_regs(g, &num_sm_dsm_perf_regs,
				       &sm_dsm_perf_regs,
				       &perf_register_stride);

	g->ops.gr.init_sm_dsm_reg_info();

	for (i = 0; i < num_sm_dsm_perf_regs; i++) {
		if ((addr & tpc_gpc_mask) == (sm_dsm_perf_regs[i] & tpc_gpc_mask)) {
			sm_dsm_perf_reg_id = i;

			nvgpu_log_info(g, "register match: 0x%08x",
					sm_dsm_perf_regs[i]);

			chk_addr = (gpc_base + gpc_stride * gpc_num) +
				   tpc_in_gpc_base +
				   (tpc_in_gpc_stride * tpc_num) +
				   (sm_dsm_perf_regs[sm_dsm_perf_reg_id] & tpc_gpc_mask);

			if (chk_addr != addr) {
				nvgpu_err(g,
				   "Oops addr miss-match! : 0x%08x != 0x%08x",
					   addr, chk_addr);
				return -EINVAL;
			}
			break;
		}
	}

	/* Didn't find reg in supported group 1.
	 *  so try the second group now */
	g->ops.gr.get_sm_dsm_perf_ctrl_regs(g, &num_sm_dsm_perf_ctrl_regs,
				       &sm_dsm_perf_ctrl_regs,
				       &control_register_stride);

	if (ILLEGAL_ID == sm_dsm_perf_reg_id) {
		for (i = 0; i < num_sm_dsm_perf_ctrl_regs; i++) {
			if ((addr & tpc_gpc_mask) ==
			    (sm_dsm_perf_ctrl_regs[i] & tpc_gpc_mask)) {
				sm_dsm_perf_ctrl_reg_id = i;

				nvgpu_log_info(g, "register match: 0x%08x",
						sm_dsm_perf_ctrl_regs[i]);

				chk_addr = (gpc_base + gpc_stride * gpc_num) +
					   tpc_in_gpc_base +
					   tpc_in_gpc_stride * tpc_num +
					   (sm_dsm_perf_ctrl_regs[sm_dsm_perf_ctrl_reg_id] &
					    tpc_gpc_mask);

				if (chk_addr != addr) {
					nvgpu_err(g,
						   "Oops addr miss-match! : 0x%08x != 0x%08x",
						   addr, chk_addr);
					return -EINVAL;

				}

				break;
			}
		}
	}

	if ((ILLEGAL_ID == sm_dsm_perf_ctrl_reg_id) &&
	    (ILLEGAL_ID == sm_dsm_perf_reg_id)) {
		return -EINVAL;
	}

	/* Skip the FECS extended header, nothing there for us now. */
	offset_to_segment += buffer_segments_size;

	/* skip through the GPCCS extended headers until we get to the data for
	 * our GPC.  The size of each gpc extended segment is enough to hold the
	 * max tpc count for the gpcs,in 256b chunks.
	 */

	max_tpc_count = nvgpu_gr_config_get_max_tpc_per_gpc_count(gr->config);

	num_ext_gpccs_ext_buffer_segments = (u32)((max_tpc_count + 1U) / 2U);

	offset_to_segment += (num_ext_gpccs_ext_buffer_segments *
			      buffer_segments_size * gpc_num);

	/* skip the head marker to start with */
	inter_seg_offset = marker_size;

	if (ILLEGAL_ID != sm_dsm_perf_ctrl_reg_id) {
		/* skip over control regs of TPC's before the one we want.
		 *  then skip to the register in this tpc */
		inter_seg_offset = inter_seg_offset +
			(tpc_num * control_register_stride) +
			sm_dsm_perf_ctrl_reg_id;
	} else {
		return -EINVAL;
	}

	/* set the offset to the segment offset plus the inter segment offset to
	 *  our register */
	offset_to_segment += (inter_seg_offset * 4U);

	/* last sanity check: did we somehow compute an offset outside the
	 * extended buffer? */
	if (offset_to_segment > offset_to_segment_end) {
		nvgpu_err(g,
			   "Overflow ctxsw buffer! 0x%08x > 0x%08x",
			   offset_to_segment, offset_to_segment_end);
		return -EINVAL;
	}

	*priv_offset = offset_to_segment;

	return 0;
}


int
gr_gk20a_process_context_buffer_priv_segment(struct gk20a *g,
					     enum ctxsw_addr_type addr_type,
					     u32 pri_addr,
					     u32 gpc_num, u32 num_tpcs,
					     u32 num_ppcs, u32 ppc_mask,
					     u32 *priv_offset)
{
	u32 i;
	u32 address, base_address;
	u32 sys_offset, gpc_offset, tpc_offset, ppc_offset;
	u32 ppc_num, tpc_num, tpc_addr, gpc_addr, ppc_addr;
	struct netlist_aiv_list *list;
	struct netlist_aiv *reg;
	u32 gpc_base = nvgpu_get_litter_value(g, GPU_LIT_GPC_BASE);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_BASE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	(void)ppc_mask;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "pri_addr=0x%x", pri_addr);

	if (!g->netlist_valid) {
		return -EINVAL;
	}

	/* Process the SYS/BE segment. */
	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_ROP)) {
		list = nvgpu_netlist_get_sys_ctxsw_regs(g);
		for (i = 0; i < list->count; i++) {
			reg = &list->l[i];
			address    = reg->addr;
			sys_offset = reg->index;

			if (pri_addr == address) {
				*priv_offset = sys_offset;
				return 0;
			}
		}
	}

	/* Process the TPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_TPC) {
		for (tpc_num = 0; tpc_num < num_tpcs; tpc_num++) {
			list = nvgpu_netlist_get_tpc_ctxsw_regs(g);
			for (i = 0; i < list->count; i++) {
				reg = &list->l[i];
				address = reg->addr;
				tpc_addr = pri_tpccs_addr_mask(g, address);
				base_address = gpc_base +
					(gpc_num * gpc_stride) +
					tpc_in_gpc_base +
					(tpc_num * tpc_in_gpc_stride);
				address = base_address + tpc_addr;
				/*
				 * The data for the TPCs is interleaved in the context buffer.
				 * Example with num_tpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				tpc_offset = (reg->index * num_tpcs) + (tpc_num * 4U);

				if (pri_addr == address) {
					*priv_offset = tpc_offset;
					return 0;
				}
			}
		}
	} else if ((addr_type == CTXSW_ADDR_TYPE_EGPC) ||
		(addr_type == CTXSW_ADDR_TYPE_ETPC)) {
		if (g->ops.gr.get_egpc_base == NULL) {
			return -EINVAL;
		}

		for (tpc_num = 0; tpc_num < num_tpcs; tpc_num++) {
			list = nvgpu_netlist_get_etpc_ctxsw_regs(g);
			for (i = 0; i < list->count; i++) {
				reg = &list->l[i];
				address = reg->addr;
				tpc_addr = pri_tpccs_addr_mask(g, address);
				base_address = g->ops.gr.get_egpc_base(g) +
					(gpc_num * gpc_stride) +
					tpc_in_gpc_base +
					(tpc_num * tpc_in_gpc_stride);
				address = base_address + tpc_addr;
				/*
				 * The data for the TPCs is interleaved in the context buffer.
				 * Example with num_tpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				tpc_offset = (reg->index * num_tpcs) + (tpc_num * 4U);

				if (pri_addr == address) {
					*priv_offset = tpc_offset;
					nvgpu_log(g,
						gpu_dbg_fn | gpu_dbg_gpu_dbg,
						"egpc/etpc priv_offset=0x%#08x",
						*priv_offset);
					return 0;
				}
			}
		}
	}


	/* Process the PPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_PPC) {
		for (ppc_num = 0; ppc_num < num_ppcs; ppc_num++) {
			list = nvgpu_netlist_get_ppc_ctxsw_regs(g);
			for (i = 0; i < list->count; i++) {
				reg = &list->l[i];
				address = reg->addr;
				ppc_addr = pri_ppccs_addr_mask(address);
				base_address = gpc_base +
					(gpc_num * gpc_stride) +
					ppc_in_gpc_base +
					(ppc_num * ppc_in_gpc_stride);
				address = base_address + ppc_addr;
				/*
				 * The data for the PPCs is interleaved in the context buffer.
				 * Example with numPpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				ppc_offset = (reg->index * num_ppcs) + (ppc_num * 4U);

				if (pri_addr == address)  {
					*priv_offset = ppc_offset;
					return 0;
				}
			}
		}
	}


	/* Process the GPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_GPC) {
		list = nvgpu_netlist_get_gpc_ctxsw_regs(g);
		for (i = 0; i < list->count; i++) {
			reg = &list->l[i];

			address = reg->addr;
			gpc_addr = pri_gpccs_addr_mask(g, address);
			gpc_offset = reg->index;

			base_address = gpc_base + (gpc_num * gpc_stride);
			address = base_address + gpc_addr;

			if (pri_addr == address) {
				*priv_offset = gpc_offset;
				return 0;
			}
		}
	}
	return -EINVAL;
}

int gr_gk20a_determine_ppc_configuration(struct gk20a *g,
					u32 *context,
					u32 *num_ppcs, u32 *ppc_mask,
					u32 *reg_ppc_count)
{
	u32 num_pes_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_PES_PER_GPC);
	u32 ppc_count = nvgpu_netlist_get_ppc_ctxsw_regs_count(g);

	/*
	 * if there is only 1 PES_PER_GPC, then we put the PES registers
	 * in the GPC reglist, so we can't error out if ppc.count == 0
	 */
	if ((!g->netlist_valid) ||
		((ppc_count == 0U) && (num_pes_per_gpc > 1U))) {
		return -EINVAL;
	}

	g->ops.gr.ctxsw_prog.get_ppc_info(context, num_ppcs, ppc_mask);
	*reg_ppc_count = ppc_count;

	return 0;
}

int gr_gk20a_get_offset_in_gpccs_segment(struct gk20a *g,
					enum ctxsw_addr_type addr_type,
					u32 num_tpcs,
					u32 num_ppcs,
					u32 reg_list_ppc_count,
					u32 *__offset_in_segment)
{
	u32 offset_in_segment = 0;
	u32 tpc_count = nvgpu_netlist_get_tpc_ctxsw_regs_count(g);
	u32 etpc_count = nvgpu_netlist_get_etpc_ctxsw_regs_count(g);

	if (addr_type == CTXSW_ADDR_TYPE_TPC) {
		/*
		 * reg = nvgpu_netlist_get_tpc_ctxsw_regs(g)->l;
		 * offset_in_segment = 0;
		 */
	} else if ((addr_type == CTXSW_ADDR_TYPE_EGPC) ||
			(addr_type == CTXSW_ADDR_TYPE_ETPC)) {
		offset_in_segment = ((tpc_count * num_tpcs) << 2);

		nvgpu_log(g, gpu_dbg_info | gpu_dbg_gpu_dbg,
			"egpc etpc offset_in_segment 0x%#08x",
			offset_in_segment);
	} else if (addr_type == CTXSW_ADDR_TYPE_PPC) {
		/*
		 * The ucode stores TPC data before PPC data.
		 * Advance offset past TPC data to PPC data.
		 */
		offset_in_segment =
			(((tpc_count + etpc_count) * num_tpcs) << 2);
	} else if (addr_type == CTXSW_ADDR_TYPE_GPC) {
		/*
		 * The ucode stores TPC/PPC data before GPC data.
		 * Advance offset past TPC/PPC data to GPC data.
		 *
		 * Note 1 PES_PER_GPC case
		 */
		u32 num_pes_per_gpc = nvgpu_get_litter_value(g,
				GPU_LIT_NUM_PES_PER_GPC);
		if (num_pes_per_gpc > 1U) {
			offset_in_segment =
				((((tpc_count + etpc_count) * num_tpcs) << 2) +
					((reg_list_ppc_count * num_ppcs) << 2));
		} else {
			offset_in_segment =
				(((tpc_count + etpc_count) * num_tpcs) << 2);
		}
	} else {
		nvgpu_log_fn(g, "Unknown address type.");
		return -EINVAL;
	}

	*__offset_in_segment = offset_in_segment;
	return 0;
}

/*
 *  This function will return the 32 bit offset for a priv register if it is
 *  present in the context buffer. The context buffer is in CPU memory.
 */
int gr_gk20a_find_priv_offset_in_buffer(struct gk20a *g,
					       u32 addr,
					       u32 *context_buffer,
					       u32 context_buffer_size,
					       u32 *priv_offset)
{
	u32 i;
	int err;
	enum ctxsw_addr_type addr_type;
	u32 broadcast_flags;
	u32 gpc_num, tpc_num, ppc_num, rop_num;
	u32 num_gpcs, num_tpcs, num_ppcs;
	u32 offset;
	u32 sys_priv_offset, gpc_priv_offset;
	u32 ppc_mask, reg_list_ppc_count;
	u32 *context;
	u32 offset_to_segment, offset_in_segment = 0;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	err = g->ops.gr.decode_priv_addr(g, addr, &addr_type,
					&gpc_num, &tpc_num, &ppc_num, &rop_num,
					&broadcast_flags);
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"addr_type = %d, broadcast_flags: %08x",
			addr_type, broadcast_flags);
	if (err != 0) {
		return err;
	}

	context = context_buffer;
	if (!g->ops.gr.ctxsw_prog.check_main_image_header_magic(context)) {
		nvgpu_err(g,
			   "Invalid main header: magic value");
		return -EINVAL;
	}
	num_gpcs = g->ops.gr.ctxsw_prog.get_num_gpcs(context);

	/* Parse the FECS local header. */
	context += (g->ops.gr.ctxsw_prog.hw_get_fecs_header_size() >> 2);
	if (!g->ops.gr.ctxsw_prog.check_local_header_magic(context)) {
		nvgpu_err(g,
			   "Invalid FECS local header: magic value");
		return -EINVAL;
	}

	sys_priv_offset =
	       g->ops.gr.ctxsw_prog.get_local_priv_register_ctl_offset(context);
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "sys_priv_offset=0x%x", sys_priv_offset);

	/* If found in Ext buffer, ok. If not, continue on. */
	err = gr_gk20a_find_priv_offset_in_ext_buffer(g,
				      addr, context_buffer,
				      context_buffer_size, priv_offset);
	if (err == 0) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"offset found in Ext buffer");
		return err;
	}

	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_ROP) ||
	    (addr_type == CTXSW_ADDR_TYPE_LTS_MAIN)) {
		/* Find the offset in the FECS segment. */
		offset_to_segment = sys_priv_offset * 256U;

		err = g->ops.gr.process_context_buffer_priv_segment(g,
					   addr_type, addr,
					   0, 0, 0, 0,
					   &offset);
		if (err != 0) {
			return err;
		}

		*priv_offset = (offset_to_segment + offset);
		return 0;
	}

	if ((gpc_num + 1U) > num_gpcs)  {
		nvgpu_err(g,
			   "GPC %d not in this context buffer.",
			   gpc_num);
		return -EINVAL;
	}

	/* Parse the GPCCS local header(s).*/
	for (i = 0; i < num_gpcs; i++) {
		context +=
			(g->ops.gr.ctxsw_prog.hw_get_gpccs_header_size() >> 2);
		if (!g->ops.gr.ctxsw_prog.check_local_header_magic(context)) {
			nvgpu_err(g,
				   "Invalid GPCCS local header: magic value");
			return -EINVAL;

		}
		gpc_priv_offset = g->ops.gr.ctxsw_prog.get_local_priv_register_ctl_offset(context);

		if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
			err = gr_gk20a_determine_ppc_configuration(g, context,
							&num_ppcs, &ppc_mask,
							&reg_list_ppc_count);
			if (err != 0) {
				nvgpu_err(g,
					"determine ppc configuration failed");
				return err;
			}
		} else {
			num_ppcs = 0U;
			ppc_mask = 0x0U;
			reg_list_ppc_count = 0U;
		}

		num_tpcs = g->ops.gr.ctxsw_prog.get_num_tpcs(context);

		if ((i == gpc_num) && ((tpc_num + 1U) > num_tpcs)) {
			nvgpu_err(g,
			   "GPC %d TPC %d not in this context buffer.",
				   gpc_num, tpc_num);
			return -EINVAL;
		}

		/* Find the offset in the GPCCS segment.*/
		if (i == gpc_num) {
			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"gpc_priv_offset 0x%#08x",
					gpc_priv_offset);
			offset_to_segment = gpc_priv_offset * 256U;

			err = g->ops.gr.get_offset_in_gpccs_segment(g,
					addr_type,
					num_tpcs, num_ppcs, reg_list_ppc_count,
					&offset_in_segment);
			if (err != 0) {
				return -EINVAL;
			}

			offset_to_segment += offset_in_segment;
			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
				"offset_to_segment 0x%#08x",
				offset_to_segment);

			err = g->ops.gr.process_context_buffer_priv_segment(g,
							   addr_type, addr,
							   i, num_tpcs,
							   num_ppcs, ppc_mask,
							   &offset);
			if (err != 0) {
				return -EINVAL;
			}

			*priv_offset = offset_to_segment + offset;
			return 0;
		}
	}

	return -EINVAL;
}

static struct nvgpu_channel *gk20a_get_resident_ctx(struct gk20a *g, u32 *tsgid)
{
	u32 curr_gr_ctx;
	struct nvgpu_channel *curr_ch;

	curr_gr_ctx = g->ops.gr.falcon.get_current_ctx(g);

	/* when contexts are unloaded from GR, the valid bit is reset
	 * but the instance pointer information remains intact. So the
	 * valid bit must be checked to be absolutely certain that a
	 * valid context is currently resident.
	 */
	if (gr_fecs_current_ctx_valid_v(curr_gr_ctx) == 0U) {
		return NULL;
	}

	curr_ch = nvgpu_gr_intr_get_channel_from_ctx(g, curr_gr_ctx, tsgid);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
		  "curr_gr_chid=%d curr_tsgid=%d",
		  (curr_ch != NULL) ? curr_ch->chid : U32_MAX, *tsgid);

	return curr_ch;
}

bool gk20a_is_channel_ctx_resident(struct nvgpu_channel *ch)
{
	u32 curr_gr_tsgid;
	struct gk20a *g = ch->g;
	struct nvgpu_channel *curr_ch;
	bool ret = false;
	struct nvgpu_tsg *tsg;

	curr_ch = gk20a_get_resident_ctx(g, &curr_gr_tsgid);
	if (curr_ch == NULL) {
		return false;
	}

	if (ch->chid == curr_ch->chid) {
		ret = true;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if ((tsg != NULL) && (tsg->tsgid == curr_gr_tsgid)) {
		ret = true;
	}

	nvgpu_channel_put(curr_ch);
	return ret;
}

bool gk20a_is_tsg_ctx_resident(struct nvgpu_tsg *tsg)
{
	u32 curr_gr_tsgid;
	struct gk20a *g = tsg->g;
	struct nvgpu_channel *curr_ch;
	bool ret = false;

	curr_ch = gk20a_get_resident_ctx(g, &curr_gr_tsgid);
	if (curr_ch == NULL) {
		return false;
	}

	if ((tsg->tsgid == curr_gr_tsgid) &&
	    (tsg->tsgid == curr_ch->tsgid)) {
		ret = true;
	}

	nvgpu_channel_put(curr_ch);
	return ret;
}

static int gr_exec_ctx_ops(struct nvgpu_tsg *tsg,
			    struct nvgpu_dbg_reg_op *ctx_ops, u32 num_ops,
			    u32 num_ctx_wr_ops, u32 num_ctx_rd_ops,
			    bool ctx_resident)
{
	struct gk20a *g = tsg->g;
	struct nvgpu_gr_ctx *gr_ctx;
	bool gr_ctx_ready = false;
	bool pm_ctx_ready = false;
	struct nvgpu_mem *current_mem = NULL;
	u32 i, j, offset, v;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);
	u32 max_offsets = nvgpu_gr_config_get_max_gpc_count(gr->config) *
		nvgpu_gr_config_get_max_tpc_per_gpc_count(gr->config) *
		sm_per_tpc;
	u32 *offsets = NULL;
	u32 *offset_addrs = NULL;
	u32 ctx_op_nr, num_ctx_ops[2] = {num_ctx_wr_ops, num_ctx_rd_ops};
	int err = 0, pass;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "wr_ops=%d rd_ops=%d",
		   num_ctx_wr_ops, num_ctx_rd_ops);

	gr_ctx = tsg->gr_ctx;

	if (ctx_resident) {
		for (pass = 0; pass < 2; pass++) {
			ctx_op_nr = 0;
			for (i = 0; i < num_ops; ++i) {
				if (ctx_op_nr >= num_ctx_ops[pass]) {
					break;
				}

				/*
				 * Move to next op if current op is invalid.
				 * Execution will reach here only if CONTINUE_ON_ERROR
				 * mode is requested.
				 */
				if (ctx_ops[i].status != REGOP(STATUS_SUCCESS)) {
					continue;
				}

				/* only do ctx ops and only on the right pass */
				if ((ctx_ops[i].type == REGOP(TYPE_GLOBAL)) ||
				    (((pass == 0) && reg_op_is_read(ctx_ops[i].op)) ||
				     ((pass == 1) && !reg_op_is_read(ctx_ops[i].op)))) {
					continue;
				}

				offset = ctx_ops[i].offset;

				if (pass == 0) { /* write pass */
					v = gk20a_readl(g, offset);
					v &= ~ctx_ops[i].and_n_mask_lo;
					v |= ctx_ops[i].value_lo;
					gk20a_writel(g, offset, v);

					nvgpu_log(g, gpu_dbg_gpu_dbg,
						   "direct wr: offset=0x%x v=0x%x",
						   offset, v);

					if (ctx_ops[i].op == REGOP(WRITE_64)) {
						v = gk20a_readl(g, offset + 4U);
						v &= ~ctx_ops[i].and_n_mask_hi;
						v |= ctx_ops[i].value_hi;
						gk20a_writel(g, offset + 4U, v);

						nvgpu_log(g, gpu_dbg_gpu_dbg,
							   "direct wr: offset=0x%x v=0x%x",
							   offset + 4U, v);
					}

				} else { /* read pass */
					ctx_ops[i].value_lo =
						gk20a_readl(g, offset);

					nvgpu_log(g, gpu_dbg_gpu_dbg,
						   "direct rd: offset=0x%x v=0x%x",
						   offset, ctx_ops[i].value_lo);

					if (ctx_ops[i].op == REGOP(READ_64)) {
						ctx_ops[i].value_hi =
							gk20a_readl(g, offset + 4U);

						nvgpu_log(g, gpu_dbg_gpu_dbg,
							   "direct rd: offset=0x%x v=0x%x",
							   offset, ctx_ops[i].value_lo);
					} else {
						ctx_ops[i].value_hi = 0;
					}
				}
				ctx_op_nr++;
			}
		}
		goto cleanup;
	}

	/* they're the same size, so just use one alloc for both */
	offsets = nvgpu_kzalloc(g, 2U * sizeof(u32) * max_offsets);
	if (offsets == NULL) {
		err = -ENOMEM;
		goto cleanup;
	}
	offset_addrs = offsets + max_offsets;

	nvgpu_gr_ctx_patch_write_begin(g, gr_ctx, false);

	err = nvgpu_pg_elpg_ms_protected_call(g,
			g->ops.mm.cache.l2_flush(g, true));
	if (err != 0) {
		nvgpu_err(g, "l2_flush failed");
		goto cleanup;
	}

	/* write to appropriate place in context image,
	 * first have to figure out where that really is */

	/* first pass is writes, second reads */
	for (pass = 0; pass < 2; pass++) {
		ctx_op_nr = 0;
		for (i = 0; i < num_ops; ++i) {
			u32 num_offsets;

			if (ctx_op_nr >= num_ctx_ops[pass]) {
					break;
			}

			/*
			 * Move to next op if current op is invalid.
			 * Execution will reach here only if CONTINUE_ON_ERROR
			 * mode is requested.
			 */
			if (ctx_ops[i].status != REGOP(STATUS_SUCCESS)) {
				continue;
			}

			/* only do ctx ops and only on the right pass */
			if ((ctx_ops[i].type == REGOP(TYPE_GLOBAL)) ||
			    (((pass == 0) && reg_op_is_read(ctx_ops[i].op)) ||
			     ((pass == 1) && !reg_op_is_read(ctx_ops[i].op)))) {
				continue;
			}

			err = g->ops.gr.get_ctx_buffer_offsets(g,
						ctx_ops[i].offset,
						max_offsets,
						offsets, offset_addrs,
						&num_offsets);
			if (err == 0) {
				if (!gr_ctx_ready) {
					gr_ctx_ready = true;
				}
				current_mem = nvgpu_gr_ctx_get_ctx_mem(gr_ctx);
			} else {
				err = gr_gk20a_get_pm_ctx_buffer_offsets(g,
							ctx_ops[i].offset,
							max_offsets,
							offsets, offset_addrs,
							&num_offsets);
				if (err != 0) {
					nvgpu_err(g, "ctx op invalid offset: offset=0x%x",
					   ctx_ops[i].offset);
					ctx_ops[i].status =
						REGOP(STATUS_INVALID_OFFSET);
					continue;
				}
				if (!pm_ctx_ready) {
					/* Make sure ctx buffer was initialized */
					if (!nvgpu_mem_is_valid(nvgpu_gr_ctx_get_pm_ctx_mem(gr_ctx))) {
						nvgpu_err(g,
							"Invalid ctx buffer");
						err = -EINVAL;
						goto cleanup;
					}
					pm_ctx_ready = true;
				}
				current_mem = nvgpu_gr_ctx_get_pm_ctx_mem(gr_ctx);
			}

			for (j = 0; j < num_offsets; j++) {
				/* sanity check gr ctxt offsets,
				 * don't write outside, worst case
				 */
				if ((current_mem == nvgpu_gr_ctx_get_ctx_mem(gr_ctx)) &&
						(offsets[j] >=
						 nvgpu_gr_obj_ctx_get_golden_image_size(
							gr->golden_image))) {
					continue;
				}
				if (pass == 0) { /* write pass */
					v = nvgpu_mem_rd(g, current_mem, offsets[j]);
					v &= ~ctx_ops[i].and_n_mask_lo;
					v |= ctx_ops[i].value_lo;
					nvgpu_mem_wr(g, current_mem, offsets[j], v);

					nvgpu_log(g, gpu_dbg_gpu_dbg,
						   "context wr: offset=0x%x v=0x%x",
						   offsets[j], v);

					if (ctx_ops[i].op == REGOP(WRITE_64)) {
						v = nvgpu_mem_rd(g, current_mem, offsets[j] + 4U);
						v &= ~ctx_ops[i].and_n_mask_hi;
						v |= ctx_ops[i].value_hi;
						nvgpu_mem_wr(g, current_mem, offsets[j] + 4U, v);

						nvgpu_log(g, gpu_dbg_gpu_dbg,
							   "context wr: offset=0x%x v=0x%x",
							   offsets[j] + 4U, v);
					}

					if (current_mem == nvgpu_gr_ctx_get_ctx_mem(gr_ctx) &&
							g->ops.gr.ctx_patch_smpc != NULL) {
						/* check to see if we need to add a special fix
						   for some of the SMPC perf regs */
						g->ops.gr.ctx_patch_smpc(g,
							offset_addrs[j],
							v, gr_ctx);
					}
				} else { /* read pass */
					ctx_ops[i].value_lo =
						nvgpu_mem_rd(g, current_mem, offsets[0]);

					nvgpu_log(g, gpu_dbg_gpu_dbg, "context rd: offset=0x%x v=0x%x",
						   offsets[0], ctx_ops[i].value_lo);

					if (ctx_ops[i].op == REGOP(READ_64)) {
						ctx_ops[i].value_hi =
							nvgpu_mem_rd(g, current_mem, offsets[0] + 4U);

						nvgpu_log(g, gpu_dbg_gpu_dbg,
							   "context rd: offset=0x%x v=0x%x",
							   offsets[0] + 4U, ctx_ops[i].value_hi);
					} else {
						ctx_ops[i].value_hi = 0;
					}
				}
			}
			ctx_op_nr++;
		}
	}

 cleanup:
	if (offsets != NULL) {
		nvgpu_kfree(g, offsets);
	}

	if (nvgpu_gr_ctx_get_patch_ctx_mem(gr_ctx)->cpu_va != NULL) {
		nvgpu_gr_ctx_patch_write_end(g, gr_ctx, gr_ctx_ready);
	}

	return err;
}

int gr_gk20a_exec_ctx_ops(struct nvgpu_tsg *tsg,
			  struct nvgpu_dbg_reg_op *ctx_ops, u32 num_ops,
			  u32 num_ctx_wr_ops, u32 num_ctx_rd_ops,
			  u32 *flags)
{
	struct gk20a *g = tsg->g;
	int err, tmp_err;
	bool ctx_resident;

	/* disable channel switching.
	 * at that point the hardware state can be inspected to
	 * determine if the context we're interested in is current.
	 */
	err = nvgpu_gr_disable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "unable to stop gr ctxsw");
		/* this should probably be ctx-fatal... */
		return err;
	}

	ctx_resident = gk20a_is_tsg_ctx_resident(tsg);
	if (ctx_resident) {
		*flags |= NVGPU_REG_OP_FLAG_DIRECT_OPS;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "is curr ctx=%d",
		  ctx_resident);

	err = gr_exec_ctx_ops(tsg, ctx_ops, num_ops, num_ctx_wr_ops,
				      num_ctx_rd_ops, ctx_resident);

	tmp_err = nvgpu_gr_enable_ctxsw(g);
	if (tmp_err != 0) {
		nvgpu_err(g, "unable to restart ctxsw!");
		err = tmp_err;
	}

	return err;
}

int gk20a_gr_wait_for_sm_lock_down(struct gk20a *g, u32 gpc, u32 tpc, u32 sm,
		u32 global_esr_mask, bool check_errors)
{
	bool locked_down;
	bool no_error_pending;
	u32 delay = POLL_DELAY_MIN_US;
	bool mmu_debug_mode_enabled = g->ops.fb.is_debug_mode_enabled(g);
	u32 offset = nvgpu_gr_gpc_offset(g, gpc) + nvgpu_gr_tpc_offset(g, tpc);
	u32 dbgr_status0 = 0, dbgr_control0 = 0;
	u64 warps_valid = 0, warps_paused = 0, warps_trapped = 0;
	struct nvgpu_timeout timeout;
	u32 warp_esr;

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
		"GPC%d TPC%d SM%d: locking down SM", gpc, tpc, sm);

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	/* wait for the sm to lock down */
	do {
		u32 global_esr = g->ops.gr.intr.get_sm_hww_global_esr(g,
						gpc, tpc, sm);
		dbgr_status0 = gk20a_readl(g,
				gr_gpc0_tpc0_sm_dbgr_status0_r() + offset);

		warp_esr = g->ops.gr.intr.get_sm_hww_warp_esr(g, gpc, tpc, sm);

		locked_down =
		    (gr_gpc0_tpc0_sm_dbgr_status0_locked_down_v(dbgr_status0) ==
		     gr_gpc0_tpc0_sm_dbgr_status0_locked_down_true_v());
		no_error_pending =
			check_errors &&
			(gr_gpc0_tpc0_sm_hww_warp_esr_error_v(warp_esr) ==
			 gr_gpc0_tpc0_sm_hww_warp_esr_error_none_v()) &&
			((global_esr & ~global_esr_mask) == 0U);

		if (locked_down || no_error_pending) {
			nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
				  "GPC%d TPC%d SM%d: locked down SM",
					gpc, tpc, sm);
			return 0;
		}

		/* if an mmu fault is pending and mmu debug mode is not
		 * enabled, the sm will never lock down. */
		if (!mmu_debug_mode_enabled &&
		     (g->ops.mc.is_mmu_fault_pending(g))) {
			nvgpu_err(g,
				"GPC%d TPC%d: mmu fault pending,"
				" SM%d will never lock down!", gpc, tpc, sm);
			return -EFAULT;
		}

		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	dbgr_control0 = gk20a_readl(g,
				gr_gpc0_tpc0_sm_dbgr_control0_r() + offset);

	/* 64 bit read */
	warps_valid = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_1_r() + offset) << 32;
	warps_valid |= gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_r() + offset);

	/* 64 bit read */
	warps_paused = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_1_r() + offset) << 32;
	warps_paused |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_r() + offset);

	/* 64 bit read */
	warps_trapped = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_1_r() + offset) << 32;
	warps_trapped |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_r() + offset);

	nvgpu_err(g,
		"GPC%d TPC%d: timed out while trying to lock down SM", gpc, tpc);
	nvgpu_err(g,
		"STATUS0(0x%x)=0x%x CONTROL0=0x%x VALID_MASK=0x%llx PAUSE_MASK=0x%llx TRAP_MASK=0x%llx",
		gr_gpc0_tpc0_sm_dbgr_status0_r() + offset, dbgr_status0, dbgr_control0,
		warps_valid, warps_paused, warps_trapped);

	return -ETIMEDOUT;
}

void gk20a_gr_suspend_single_sm(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm,
		u32 global_esr_mask, bool check_errors)
{
	int err;
	u32 dbgr_control0;
	u32 offset = nvgpu_gr_gpc_offset(g, gpc) + nvgpu_gr_tpc_offset(g, tpc);

	/* if an SM debugger isn't attached, skip suspend */
	if (!g->ops.gr.sm_debugger_attached(g)) {
		nvgpu_err(g,
			"SM debugger not attached, skipping suspend!");
		return;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
		"suspending gpc:%d, tpc:%d, sm%d", gpc, tpc, sm);

	/* assert stop trigger. */
	dbgr_control0 = gk20a_readl(g,
				gr_gpc0_tpc0_sm_dbgr_control0_r() + offset);
	dbgr_control0 |= gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_enable_f();
	gk20a_writel(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset,
			dbgr_control0);

	err = g->ops.gr.wait_for_sm_lock_down(g, gpc, tpc, sm,
			global_esr_mask, check_errors);
	if (err != 0) {
		nvgpu_err(g,
			"SuspendSm failed");
		return;
	}
}

void gk20a_gr_suspend_all_sms(struct gk20a *g,
		u32 global_esr_mask, bool check_errors)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc, tpc, sm;
	int err;
	u32 dbgr_control0;
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);

	/* if an SM debugger isn't attached, skip suspend */
	if (!g->ops.gr.sm_debugger_attached(g)) {
		nvgpu_err(g,
			"SM debugger not attached, skipping suspend!");
		return;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "suspending all sms");
	/* assert stop trigger. uniformity assumption: all SMs will have
	 * the same state in dbg_control0.
	 */
	dbgr_control0 =
		gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r());
	dbgr_control0 |= gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_enable_f();

	/* broadcast write */
	gk20a_writel(g,
		gr_gpcs_tpcs_sm_dbgr_control0_r(), dbgr_control0);

	for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(gr->config); gpc++) {
		for (tpc = 0;
		     tpc < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc);
		     tpc++) {
			for (sm = 0; sm < sm_per_tpc; sm++) {
				err = g->ops.gr.wait_for_sm_lock_down(g,
					gpc, tpc, sm,
					global_esr_mask, check_errors);
				if (err != 0) {
					nvgpu_err(g, "SuspendAllSms failed");
					return;
				}
			}
		}
	}
}

void gk20a_gr_resume_single_sm(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm)
{
	u32 dbgr_control0;
	u32 offset;

	(void)sm;

	/*
	 * The following requires some clarification. Despite the fact that both
	 * RUN_TRIGGER and STOP_TRIGGER have the word "TRIGGER" in their
	 *  names, only one is actually a trigger, and that is the STOP_TRIGGER.
	 * Merely writing a 1(_TASK) to the RUN_TRIGGER is not sufficient to
	 * resume the gpu - the _STOP_TRIGGER must explicitly be set to 0
	 * (_DISABLE) as well.

	* Advice from the arch group:  Disable the stop trigger first, as a
	* separate operation, in order to ensure that the trigger has taken
	* effect, before enabling the run trigger.
	*/

	offset = nvgpu_gr_gpc_offset(g, gpc) + nvgpu_gr_tpc_offset(g, tpc);

	/*De-assert stop trigger */
	dbgr_control0 =
		gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset);
	dbgr_control0 = set_field(dbgr_control0,
			gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_m(),
			gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_disable_f());
	gk20a_writel(g,
		gr_gpc0_tpc0_sm_dbgr_control0_r() + offset, dbgr_control0);

	/* Run trigger */
	dbgr_control0 |= gr_gpcs_tpcs_sm_dbgr_control0_run_trigger_task_f();
	gk20a_writel(g,
		gr_gpc0_tpc0_sm_dbgr_control0_r() + offset, dbgr_control0);
}

void gk20a_gr_resume_all_sms(struct gk20a *g)
{
	u32 dbgr_control0;
	/*
	 * The following requires some clarification. Despite the fact that both
	 * RUN_TRIGGER and STOP_TRIGGER have the word "TRIGGER" in their
	 *  names, only one is actually a trigger, and that is the STOP_TRIGGER.
	 * Merely writing a 1(_TASK) to the RUN_TRIGGER is not sufficient to
	 * resume the gpu - the _STOP_TRIGGER must explicitly be set to 0
	 * (_DISABLE) as well.

	* Advice from the arch group:  Disable the stop trigger first, as a
	* separate operation, in order to ensure that the trigger has taken
	* effect, before enabling the run trigger.
	*/

	/*De-assert stop trigger */
	dbgr_control0 =
		gk20a_readl(g, gr_gpcs_tpcs_sm_dbgr_control0_r());
	dbgr_control0 &= ~gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_enable_f();
	gk20a_writel(g,
		gr_gpcs_tpcs_sm_dbgr_control0_r(), dbgr_control0);

	/* Run trigger */
	dbgr_control0 |= gr_gpcs_tpcs_sm_dbgr_control0_run_trigger_task_f();
	gk20a_writel(g,
		gr_gpcs_tpcs_sm_dbgr_control0_r(), dbgr_control0);
}

int gr_gk20a_set_sm_debug_mode(struct gk20a *g,
	struct nvgpu_channel *ch, u64 sms, bool enable)
{
	struct nvgpu_dbg_reg_op *ops;
	unsigned int i = 0, sm_id;
	int err;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(ch);
	u32 flags = NVGPU_REG_OP_FLAG_MODE_ALL_OR_NONE;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 no_of_sm = nvgpu_gr_config_get_no_of_sm(gr->config);

	if (tsg == NULL) {
		return -EINVAL;
	}

	ops = nvgpu_kcalloc(g, no_of_sm, sizeof(*ops));
	if (ops == NULL) {
		return -ENOMEM;
	}

	for (sm_id = 0; sm_id < no_of_sm; sm_id++) {
		u32 gpc, tpc;
		u32 tpc_offset, gpc_offset, reg_offset, reg_mask, reg_val;
		struct nvgpu_sm_info *sm_info;

		if ((sms & BIT64(sm_id)) == 0ULL) {
			continue;
		}
		sm_info = nvgpu_gr_config_get_sm_info(gr->config, sm_id);
		gpc = nvgpu_gr_config_get_sm_info_gpc_index(sm_info);
		tpc = nvgpu_gr_config_get_sm_info_tpc_index(sm_info);

		tpc_offset = tpc_in_gpc_stride * tpc;
		gpc_offset = gpc_stride * gpc;
		reg_offset = tpc_offset + gpc_offset;

		ops[i].op = REGOP(WRITE_32);
		ops[i].type = REGOP(TYPE_GR_CTX);
		ops[i].offset  = gr_gpc0_tpc0_sm_dbgr_control0_r() + reg_offset;

		reg_mask = 0;
		reg_val = 0;
		if (enable) {
			reg_mask |= gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_m();
			reg_val |= gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_on_f();
			reg_mask |= gr_gpc0_tpc0_sm_dbgr_control0_stop_on_any_warp_m();
			reg_val |= gr_gpc0_tpc0_sm_dbgr_control0_stop_on_any_warp_disable_f();
			reg_mask |= gr_gpc0_tpc0_sm_dbgr_control0_stop_on_any_sm_m();
			reg_val |= gr_gpc0_tpc0_sm_dbgr_control0_stop_on_any_sm_disable_f();
		} else {
			reg_mask |= gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_m();
			reg_val |= gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_off_f();
		}

		ops[i].and_n_mask_lo = reg_mask;
		ops[i].value_lo = reg_val;
		i++;
	}

	err = gr_gk20a_exec_ctx_ops(tsg, ops, i, i, 0, &flags);
	if (err != 0) {
		nvgpu_err(g, "Failed to access register");
	}
	nvgpu_kfree(g, ops);
	return err;
}

/*
 * gr_gk20a_suspend_context()
 * This API should be called with dbg_session lock held
 * and ctxsw disabled
 * Returns bool value indicating if context was resident
 * or not
 */
bool gr_gk20a_suspend_context(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	bool ctx_resident = false;

	if (gk20a_is_channel_ctx_resident(ch)) {
		g->ops.gr.suspend_all_sms(g, 0, false);
		ctx_resident = true;
	} else {
		if (nvgpu_channel_disable_tsg(g, ch) != 0) {
			/* ch might not be bound to tsg anymore */
			nvgpu_err(g, "failed to disable channel/TSG");
		}
	}

	return ctx_resident;
}

bool gr_gk20a_resume_context(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	bool ctx_resident = false;

	if (gk20a_is_channel_ctx_resident(ch)) {
		g->ops.gr.resume_all_sms(g);
		ctx_resident = true;
	} else {
		if (nvgpu_channel_enable_tsg(g, ch) != 0) {
			/* ch might not be bound to tsg anymore */
			nvgpu_err(g, "failed to enable channel/TSG");
		}
	}

	return ctx_resident;
}

int gr_gk20a_suspend_contexts(struct gk20a *g,
			      struct dbg_session_gk20a *dbg_s,
			      int *ctx_resident_ch_fd)
{
	int local_ctx_resident_ch_fd = -1;
	bool ctx_resident;
	struct nvgpu_channel *ch;
	struct dbg_session_channel_data *ch_data;
	int err = 0;

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	err = nvgpu_gr_disable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "unable to stop gr ctxsw");
		goto clean_up;
	}

	nvgpu_mutex_acquire(&dbg_s->ch_list_lock);

	nvgpu_list_for_each_entry(ch_data, &dbg_s->ch_list,
			dbg_session_channel_data, ch_entry) {
		ch = g->fifo.channel + ch_data->chid;

		ctx_resident = gr_gk20a_suspend_context(ch);
		if (ctx_resident) {
			local_ctx_resident_ch_fd = ch_data->channel_fd;
		}
	}

	nvgpu_mutex_release(&dbg_s->ch_list_lock);

	err = nvgpu_gr_enable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "unable to restart ctxsw!");
	}

	*ctx_resident_ch_fd = local_ctx_resident_ch_fd;

clean_up:
	nvgpu_mutex_release(&g->dbg_sessions_lock);

	return err;
}

int gr_gk20a_resume_contexts(struct gk20a *g,
			      struct dbg_session_gk20a *dbg_s,
			      int *ctx_resident_ch_fd)
{
	int local_ctx_resident_ch_fd = -1;
	bool ctx_resident;
	struct nvgpu_channel *ch;
	int err = 0;
	struct dbg_session_channel_data *ch_data;

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	err = nvgpu_gr_disable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "unable to stop gr ctxsw");
		goto clean_up;
	}

	nvgpu_list_for_each_entry(ch_data, &dbg_s->ch_list,
			dbg_session_channel_data, ch_entry) {
		ch = g->fifo.channel + ch_data->chid;

		ctx_resident = gr_gk20a_resume_context(ch);
		if (ctx_resident) {
			local_ctx_resident_ch_fd = ch_data->channel_fd;
		}
	}

	err = nvgpu_gr_enable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "unable to restart ctxsw!");
	}

	*ctx_resident_ch_fd = local_ctx_resident_ch_fd;

clean_up:
	nvgpu_mutex_release(&g->dbg_sessions_lock);

	return err;
}

int gr_gk20a_trigger_suspend(struct gk20a *g)
{
	int err = 0;
	u32 dbgr_control0;

	if (!g->ops.gr.sm_debugger_attached(g)) {
		nvgpu_err(g,
			"SM debugger not attached, do not trigger suspend!");
		return -EINVAL;
	}

	/* assert stop trigger. uniformity assumption: all SMs will have
	 * the same state in dbg_control0. */
	dbgr_control0 =
		gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r());
	dbgr_control0 |= gr_gpcs_tpcs_sm_dbgr_control0_stop_trigger_enable_f();

	/* broadcast write */
	gk20a_writel(g,
		gr_gpcs_tpcs_sm_dbgr_control0_r(), dbgr_control0);

	return err;
}

int gr_gk20a_wait_for_pause(struct gk20a *g, struct nvgpu_warpstate *w_state)
{
	int err = 0;
	u32 gpc, tpc, sm, sm_id;
	u32 global_mask;
	u32 no_of_sm;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	if ((g->ops.gr.intr.get_sm_no_lock_down_hww_global_esr_mask == NULL) ||
			(g->ops.gr.lock_down_sm == NULL) ||
			(g->ops.gr.bpt_reg_info == NULL) ||
			(g->ops.gr.sm_debugger_attached == NULL)) {
		return -EINVAL;
	}

	no_of_sm = nvgpu_gr_config_get_no_of_sm(gr->config);

	if (!g->ops.gr.sm_debugger_attached(g)) {
		nvgpu_err(g,
			"SM debugger not attached, do not wait for pause!");
		return -EINVAL;
	}

	/* Wait for the SMs to reach full stop. This condition is:
	 * 1) All SMs with valid warps must be in the trap handler (SM_IN_TRAP_MODE)
	 * 2) All SMs in the trap handler must have equivalent VALID and PAUSED warp
	 *    masks.
	*/
	global_mask = g->ops.gr.intr.get_sm_no_lock_down_hww_global_esr_mask(g);

	/* Lock down all SMs */
	for (sm_id = 0; sm_id < no_of_sm; sm_id++) {
		struct nvgpu_sm_info *sm_info =
			nvgpu_gr_config_get_sm_info(gr->config, sm_id);
		gpc = nvgpu_gr_config_get_sm_info_gpc_index(sm_info);
		tpc = nvgpu_gr_config_get_sm_info_tpc_index(sm_info);
		sm = nvgpu_gr_config_get_sm_info_sm_index(sm_info);

		err = g->ops.gr.lock_down_sm(g, gpc, tpc, sm,
				global_mask, false);
		if (err != 0) {
			nvgpu_err(g, "sm did not lock down!");
			return err;
		}
	}

	/* Read the warp status */
	g->ops.gr.bpt_reg_info(g, w_state);

	return 0;
}

int gr_gk20a_resume_from_pause(struct gk20a *g)
{
	int err = 0;

	if (!g->ops.gr.sm_debugger_attached(g)) {
		nvgpu_err(g,
			"SM debugger not attached, do not resume for pause!");
		return -EINVAL;
	}

	/* Clear the pause mask to tell the GPU we want to resume everyone */
	gk20a_writel(g,
		gr_gpcs_tpcs_sm_dbgr_bpt_pause_mask_r(), 0);

	/* explicitly re-enable forwarding of SM interrupts upon any resume */
	g->ops.gr.intr.tpc_exception_sm_enable(g);

	/* Now resume all sms, write a 0 to the stop trigger
	 * then a 1 to the run trigger */
	g->ops.gr.resume_all_sms(g);

	return err;
}

int gr_gk20a_clear_sm_errors(struct gk20a *g)
{
	int ret = 0;
	u32 gpc, tpc, sm;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 global_esr;
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);

	if ((g->ops.gr.intr.get_sm_hww_global_esr == NULL) ||
			(g->ops.gr.intr.clear_sm_hww == NULL)) {
		return -EINVAL;
	}

	for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(gr->config); gpc++) {

		/* check if any tpc has an exception */
		for (tpc = 0;
		     tpc < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc);
		     tpc++) {

			for (sm = 0; sm < sm_per_tpc; sm++) {
				global_esr = g->ops.gr.intr.get_sm_hww_global_esr(g,
							 gpc, tpc, sm);

				/* clearing hwws, also causes tpc and gpc
				 * exceptions to be cleared
				 */
				g->ops.gr.intr.clear_sm_hww(g,
					gpc, tpc, sm, global_esr);
			}
		}
	}

	return ret;
}

