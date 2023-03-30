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

#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/barrier.h>
#include <nvgpu/bug.h>
#include <nvgpu/soc.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/timers.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/ltc.h>
#include <nvgpu/rc.h>
#include <nvgpu/mmu_fault.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/string.h>

#include <nvgpu/hw/gv11b/hw_gmmu_gv11b.h>

#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
static int gv11b_fb_fix_page_fault(struct gk20a *g,
		 struct mmu_fault_info *mmufault);
static void gv11b_mm_mmu_fault_handle_replayable(struct gk20a *g,
		struct mmu_fault_info *mmufault, u32 *invalidate_replay_val);
#endif

static const char mmufault_invalid_str[] = "invalid";

static const char *const gv11b_fault_type_descs[] = {
	"invalid pde",
	"invalid pde size",
	"invalid pte",
	"limit violation",
	"unbound inst block",
	"priv violation",
	"write",
	"read",
	"pitch mask violation",
	"work creation",
	"unsupported aperture",
	"compression failure",
	"unsupported kind",
	"region violation",
	"poison",
	"atomic"
};

static const char *const gv11b_fault_client_type_descs[] = {
	"gpc",
	"hub",
};

static const char *const gv11b_hub_client_descs[] = {
	"vip", "ce0", "ce1", "dniso", "fe", "fecs", "host", "host cpu",
	"host cpu nb", "iso", "mmu", "nvdec", "nvenc1", "nvenc2",
	"niso", "p2p", "pd", "perf", "pmu", "raster twod", "scc",
	"scc nb", "sec", "ssync", "gr copy", "xv", "mmu nb",
	"nvenc", "d falcon", "sked", "a falcon", "hsce0", "hsce1",
	"hsce2", "hsce3", "hsce4", "hsce5", "hsce6", "hsce7", "hsce8",
	"hsce9", "hshub", "ptp x0", "ptp x1", "ptp x2", "ptp x3",
	"ptp x4", "ptp x5", "ptp x6", "ptp x7", "vpr scrubber0",
	"vpr scrubber1", "dwbif", "fbfalcon", "ce shim", "gsp",
	"dont care"
};

static const char *const gv11b_gpc_client_descs[] = {
	"t1 0", "t1 1", "t1 2", "t1 3",
	"t1 4", "t1 5", "t1 6", "t1 7",
	"pe 0", "pe 1", "pe 2", "pe 3",
	"pe 4", "pe 5", "pe 6", "pe 7",
	"rast", "gcc", "gpccs",
	"prop 0", "prop 1", "prop 2", "prop 3",
	"gpm",
	"ltp utlb 0", "ltp utlb 1", "ltp utlb 2", "ltp utlb 3",
	"ltp utlb 4", "ltp utlb 5", "ltp utlb 6", "ltp utlb 7",
	"utlb",
	"t1 8", "t1 9", "t1 10", "t1 11",
	"t1 12", "t1 13", "t1 14", "t1 15",
	"tpccs 0", "tpccs 1", "tpccs 2", "tpccs 3",
	"tpccs 4", "tpccs 5", "tpccs 6", "tpccs 7",
	"pe 8", "pe 9", "tpccs 8", "tpccs 9",
	"t1 16", "t1 17", "t1 18", "t1 19",
	"pe 10", "pe 11", "tpccs 10", "tpccs 11",
	"t1 20", "t1 21", "t1 22", "t1 23",
	"pe 12", "pe 13", "tpccs 12", "tpccs 13",
	"t1 24", "t1 25", "t1 26", "t1 27",
	"pe 14", "pe 15", "tpccs 14", "tpccs 15",
	"t1 28", "t1 29", "t1 30", "t1 31",
	"pe 16", "pe 17", "tpccs 16", "tpccs 17",
	"t1 32", "t1 33", "t1 34", "t1 35",
	"pe 18", "pe 19", "tpccs 18", "tpccs 19",
	"t1 36", "t1 37", "t1 38", "t1 39",
};

void gv11b_mm_mmu_fault_parse_mmu_fault_info(struct mmu_fault_info *mmufault)
{
	if (mmufault->mmu_engine_id == gmmu_fault_mmu_eng_id_bar2_v()) {
		mmufault->mmu_engine_id_type = NVGPU_MMU_ENGINE_ID_TYPE_BAR2;

	} else if (mmufault->mmu_engine_id ==
			gmmu_fault_mmu_eng_id_physical_v()) {
		mmufault->mmu_engine_id_type =
				NVGPU_MMU_ENGINE_ID_TYPE_PHYSICAL;
	} else {
		mmufault->mmu_engine_id_type = NVGPU_MMU_ENGINE_ID_TYPE_OTHER;
	}
	if (mmufault->fault_type >= ARRAY_SIZE(gv11b_fault_type_descs)) {
		nvgpu_do_assert();
		mmufault->fault_type_desc = mmufault_invalid_str;
	} else {
		mmufault->fault_type_desc =
			 gv11b_fault_type_descs[mmufault->fault_type];
	}

	if (mmufault->client_type >=
			ARRAY_SIZE(gv11b_fault_client_type_descs)) {
		nvgpu_do_assert();
		mmufault->client_type_desc = mmufault_invalid_str;
	} else {
		mmufault->client_type_desc =
			 gv11b_fault_client_type_descs[mmufault->client_type];
	}

	mmufault->client_id_desc = mmufault_invalid_str;
	if (mmufault->client_type == gmmu_fault_client_type_hub_v()) {
		if (!(mmufault->client_id >=
				 ARRAY_SIZE(gv11b_hub_client_descs))) {
			mmufault->client_id_desc =
				 gv11b_hub_client_descs[mmufault->client_id];
		} else {
			nvgpu_do_assert();
		}
	} else if (mmufault->client_type ==
			gmmu_fault_client_type_gpc_v()) {
		if (!(mmufault->client_id >=
				 ARRAY_SIZE(gv11b_gpc_client_descs))) {
			mmufault->client_id_desc =
				 gv11b_gpc_client_descs[mmufault->client_id];
		} else {
			nvgpu_do_assert();
		}
	} else {
		/* Nothing to do here */
	}
}

/*
 *Fault buffer format
 *
 * 31    28     24 23           16 15            8 7     4       0
 *.-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-.
 *|              inst_lo                  |0 0|apr|0 0 0 0 0 0 0 0|
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                             inst_hi                           |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|              addr_31_12               |                   |AP |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                            addr_63_32                         |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                          timestamp_lo                         |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                          timestamp_hi                         |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                           (reserved)        |    engine_id    |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|V|R|P|  gpc_id |0 0 0|t|0|acctp|0|   client    |RF0 0|faulttype|
 */

static void gv11b_fb_copy_from_hw_fault_buf(struct gk20a *g,
	 struct nvgpu_mem *mem, u32 offset, struct mmu_fault_info *mmufault)
{
	u32 rd32_val;
	u32 addr_lo, addr_hi;
	u64 inst_ptr;
	u32 chid = NVGPU_INVALID_CHANNEL_ID;
	struct nvgpu_channel *refch;

	(void) memset(mmufault, 0, sizeof(*mmufault));

	rd32_val = nvgpu_mem_rd32(g, mem, nvgpu_safe_add_u32(offset,
			 gmmu_fault_buf_entry_inst_lo_w()));
	addr_lo = gmmu_fault_buf_entry_inst_lo_v(rd32_val);
	addr_lo = addr_lo << gmmu_fault_buf_entry_inst_lo_b();

	addr_hi = nvgpu_mem_rd32(g, mem, nvgpu_safe_add_u32(offset,
				 gmmu_fault_buf_entry_inst_hi_w()));
	addr_hi = gmmu_fault_buf_entry_inst_hi_v(addr_hi);

	inst_ptr = hi32_lo32_to_u64(addr_hi, addr_lo);

	/* refch will be put back after fault is handled */
	refch = nvgpu_channel_refch_from_inst_ptr(g, inst_ptr);
	if (refch != NULL) {
		chid = refch->chid;
	}

	/* it is ok to continue even if refch is NULL */
	mmufault->refch = refch;
	mmufault->chid = chid;
	mmufault->inst_ptr = inst_ptr;
	mmufault->inst_aperture =
		gmmu_fault_buf_entry_inst_aperture_v(rd32_val);

	rd32_val = nvgpu_mem_rd32(g, mem, nvgpu_safe_add_u32(offset,
			 gmmu_fault_buf_entry_addr_lo_w()));

	mmufault->fault_addr_aperture =
		gmmu_fault_buf_entry_addr_phys_aperture_v(rd32_val);
	addr_lo = gmmu_fault_buf_entry_addr_lo_v(rd32_val);
	addr_lo = addr_lo << gmmu_fault_buf_entry_addr_lo_b();

	rd32_val = nvgpu_mem_rd32(g, mem, nvgpu_safe_add_u32(offset,
			 gmmu_fault_buf_entry_addr_hi_w()));
	addr_hi = gmmu_fault_buf_entry_addr_hi_v(rd32_val);
	mmufault->fault_addr = hi32_lo32_to_u64(addr_hi, addr_lo);

	rd32_val = nvgpu_mem_rd32(g, mem, nvgpu_safe_add_u32(offset,
			 gmmu_fault_buf_entry_timestamp_lo_w()));
	mmufault->timestamp_lo =
		 gmmu_fault_buf_entry_timestamp_lo_v(rd32_val);

	rd32_val = nvgpu_mem_rd32(g, mem, nvgpu_safe_add_u32(offset,
			 gmmu_fault_buf_entry_timestamp_hi_w()));
	mmufault->timestamp_hi =
		 gmmu_fault_buf_entry_timestamp_hi_v(rd32_val);

	rd32_val = nvgpu_mem_rd32(g, mem, nvgpu_safe_add_u32(offset,
			 gmmu_fault_buf_entry_engine_id_w()));

	mmufault->mmu_engine_id =
		 gmmu_fault_buf_entry_engine_id_v(rd32_val);
	nvgpu_engine_mmu_fault_id_to_eng_ve_pbdma_id(g, mmufault->mmu_engine_id,
		 &mmufault->faulted_engine, &mmufault->faulted_subid,
		 &mmufault->faulted_pbdma);

	rd32_val = nvgpu_mem_rd32(g, mem, nvgpu_safe_add_u32(offset,
			gmmu_fault_buf_entry_fault_type_w()));
	mmufault->client_id =
		 gmmu_fault_buf_entry_client_v(rd32_val);
	mmufault->replayable_fault =
		(gmmu_fault_buf_entry_replayable_fault_v(rd32_val) ==
			gmmu_fault_buf_entry_replayable_fault_true_v());

	mmufault->fault_type =
		 gmmu_fault_buf_entry_fault_type_v(rd32_val);
	mmufault->access_type =
		 gmmu_fault_buf_entry_access_type_v(rd32_val);

	mmufault->client_type =
		gmmu_fault_buf_entry_mmu_client_type_v(rd32_val);

	mmufault->gpc_id =
		 gmmu_fault_buf_entry_gpc_id_v(rd32_val);
	mmufault->protected_mode =
		gmmu_fault_buf_entry_protected_mode_v(rd32_val);

	mmufault->replay_fault_en =
		gmmu_fault_buf_entry_replayable_fault_en_v(rd32_val);

	mmufault->valid = (gmmu_fault_buf_entry_valid_v(rd32_val) ==
				gmmu_fault_buf_entry_valid_true_v());

	rd32_val = nvgpu_mem_rd32(g, mem, nvgpu_safe_add_u32(offset,
			gmmu_fault_buf_entry_fault_type_w()));
	rd32_val &= ~(gmmu_fault_buf_entry_valid_m());
	nvgpu_mem_wr32(g, mem, nvgpu_safe_add_u32(offset,
						gmmu_fault_buf_entry_valid_w()),
		       rd32_val);

	g->ops.mm.mmu_fault.parse_mmu_fault_info(mmufault);
}

static bool gv11b_mm_mmu_fault_handle_mmu_fault_ce(struct gk20a *g,
		struct mmu_fault_info *mmufault, u32 *invalidate_replay_val)
{
	struct nvgpu_tsg *tsg = NULL;
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	int err;
#endif

	/* CE page faults are not reported as replayable */
	nvgpu_log(g, gpu_dbg_intr, "CE Faulted");
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	err = gv11b_fb_fix_page_fault(g, mmufault);
#endif

	if (mmufault->refch != NULL) {
		tsg = nvgpu_tsg_from_ch(mmufault->refch);
		nvgpu_tsg_reset_faulted_eng_pbdma(g, tsg, true, true);
	}

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if (err == 0) {
		*invalidate_replay_val = 0;
		nvgpu_log(g, gpu_dbg_intr, "CE Page Fault Fixed");

		if (mmufault->refch != NULL) {
			nvgpu_channel_put(mmufault->refch);
			mmufault->refch = NULL;
		}
		return true;
	}
#else
	(void)invalidate_replay_val;
#endif
	/* Do recovery */
	nvgpu_log(g, gpu_dbg_intr, "CE Page Fault Not Fixed");

	return false;
}

static bool gv11b_mm_mmu_fault_handle_mmu_fault_refch(struct gk20a *g,
		struct mmu_fault_info *mmufault, u32 *id_ptr,
		unsigned int *id_type_ptr, unsigned int *rc_type_ptr)
{
	struct nvgpu_tsg *tsg = NULL;

	if (mmufault->client_type == gmmu_fault_client_type_gpc_v()) {
		if (mmufault->refch->mmu_nack_handled) {
			/*
			 * We have already recovered for the same
			 * context, skip doing another recovery.
			 */
			mmufault->refch->mmu_nack_handled = false;
			/*
			 * Recovery path can be entered twice for the
			 * same error in case of mmu nack. If mmu
			 * nack interrupt is handled before mmu fault
			 * then channel reference is increased to avoid
			 * closing the channel by userspace. Decrement
			 * channel reference.
			 */
			nvgpu_channel_put(mmufault->refch);
			/*
			 * refch in mmufault is assigned at the time
			 * of copying fault info from snap reg or bar2
			 * fault buf.
			 */
			nvgpu_channel_put(mmufault->refch);
			return true;
		} else {
			/*
			 * Indicate recovery is handled if mmu fault is
			 * a result of mmu nack.
			 */
			mmufault->refch->mmu_nack_handled = true;
		}
	}

	tsg = nvgpu_tsg_from_ch(mmufault->refch);
	if (tsg != NULL) {
		*id_ptr = mmufault->refch->tsgid;
		*id_type_ptr = ID_TYPE_TSG;
		*rc_type_ptr = RC_TYPE_MMU_FAULT;
	} else {
		nvgpu_err(g, "chid: %d is referenceable but "
				"not bound to tsg",
				mmufault->refch->chid);
		*id_ptr = mmufault->refch->chid;
		*id_type_ptr = ID_TYPE_CHANNEL;
		*rc_type_ptr = RC_TYPE_NO_RC;
	}
	return false;
}

static bool gv11b_mm_mmu_fault_handle_non_replayable(struct gk20a *g,
					struct mmu_fault_info *mmufault)
{
	unsigned int id_type = ID_TYPE_UNKNOWN;
	u32 act_eng_bitmask = 0U;
	u32 id = NVGPU_INVALID_TSG_ID;
	unsigned int rc_type = RC_TYPE_NO_RC;
	bool ret = false;

	if (mmufault->fault_type ==
			gmmu_fault_type_unbound_inst_block_v()) {
		/*
		 * Bug 1847172: When an engine faults due to an unbound
		 * instance block, the fault cannot be isolated to a
		 * single context so we need to reset the entire runlist
		 */
		rc_type = RC_TYPE_MMU_FAULT;

	} else if (mmufault->refch != NULL) {
		ret = gv11b_mm_mmu_fault_handle_mmu_fault_refch(g, mmufault,
				&id, &id_type, &rc_type);
		if (ret) {
			return ret;
		}
	} else {
		/* Nothing to do here */
	}

	/* engine is faulted */
	if (mmufault->faulted_engine != NVGPU_INVALID_ENG_ID) {
		act_eng_bitmask = BIT32(mmufault->faulted_engine);
		rc_type = RC_TYPE_MMU_FAULT;
	}

	/*
	 * refch in mmufault is assigned at the time of copying
	 * fault info from snap reg or bar2 fault buf
	 */
	if (mmufault->refch != NULL) {
		nvgpu_channel_put(mmufault->refch);
		mmufault->refch = NULL;
	}

	if (rc_type != RC_TYPE_NO_RC) {
		nvgpu_rc_mmu_fault(g, act_eng_bitmask,
			id, id_type, rc_type, mmufault);
	}
	return ret;
}


void gv11b_mm_mmu_fault_handle_mmu_fault_common(struct gk20a *g,
		 struct mmu_fault_info *mmufault, u32 *invalidate_replay_val)
{
	u32 num_lce;
	bool ret = false;

	if (!mmufault->valid) {
		return;
	}

	gv11b_fb_mmu_fault_info_dump(g, mmufault);

	/**
	 * If nvgpu power-on is yet to complete, don't attempt further fault
	 * handling. Access to fault buffers is synchronized as nvgpu driver
	 * reads the fault buffer registers before proceeding with fault
	 * handling.
	 * However, MMU fault handling needs to be synchronized with GR/FIFO/
	 * quiesce/recovery related setup through nvgpu power-on state.
	 */
	if (!nvgpu_is_powered_on(g)) {
		return;
	}

	num_lce = g->ops.top.get_num_lce(g);
	if ((mmufault->mmu_engine_id >= gmmu_fault_mmu_eng_id_ce0_v()) &&
	    (mmufault->mmu_engine_id <
		nvgpu_safe_add_u32(gmmu_fault_mmu_eng_id_ce0_v(), num_lce))) {
		ret = gv11b_mm_mmu_fault_handle_mmu_fault_ce(g, mmufault,
			invalidate_replay_val);
		if (ret) {
			return;
		}
	}

	if (!mmufault->replayable_fault) {
		ret = gv11b_mm_mmu_fault_handle_non_replayable(g, mmufault);
		if (ret) {
			return;
		}
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	} else {
		gv11b_mm_mmu_fault_handle_replayable(g, mmufault,
						invalidate_replay_val);
#endif
	}
}

static void gv11b_mm_mmu_fault_handle_buf_valid_entry(struct gk20a *g,
		struct nvgpu_mem *mem, struct mmu_fault_info *mmufault,
		u32 *invalidate_replay_val_ptr, u32 rd32_val, u32 fault_status,
		u32 index, u32 get_indx, u32 offset, u32 entries)
{
	u32 err_type =  0U;
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	u64 prev_fault_addr =  0ULL;
	u64 next_fault_addr =  0ULL;
#endif

	while ((rd32_val & gmmu_fault_buf_entry_valid_m()) != 0U) {

		nvgpu_log(g, gpu_dbg_intr, "entry valid = 0x%x", rd32_val);

		gv11b_fb_copy_from_hw_fault_buf(g, mem, offset, mmufault);

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
		if (index == NVGPU_MMU_FAULT_REPLAY_REG_INDX) {
			err_type = GPU_HUBMMU_PAGE_FAULT_REPLAYABLE_FAULT_NOTIFY_ERROR;
		} else {
#endif
			err_type = GPU_HUBMMU_PAGE_FAULT_NONREPLAYABLE_FAULT_NOTIFY_ERROR;
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
		}
#endif

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HUBMMU, err_type);
		nvgpu_err(g, "page fault error: err_type = 0x%x, "
				"fault_status = 0x%x", err_type, fault_status);

		nvgpu_assert(get_indx < U32_MAX);
		nvgpu_assert(entries != 0U);
		get_indx = (get_indx + 1U) % entries;
		nvgpu_log(g, gpu_dbg_intr, "new get index = %d", get_indx);

		gv11b_fb_fault_buffer_get_ptr_update(g, index, get_indx);

		offset = nvgpu_safe_mult_u32(get_indx, gmmu_fault_buf_size_v())
			 / U32(sizeof(u32));
		nvgpu_log(g, gpu_dbg_intr, "next word offset = 0x%x", offset);

		rd32_val = nvgpu_mem_rd32(g, mem,
				nvgpu_safe_add_u32(offset,
					gmmu_fault_buf_entry_valid_w()));

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
		if (index == NVGPU_MMU_FAULT_REPLAY_REG_INDX &&
		    mmufault->fault_addr != 0ULL) {
			/*
			 * fault_addr "0" is not supposed to be fixed ever.
			 * For the first time when prev = 0, next = 0 and
			 * fault addr is also 0 then handle_mmu_fault_common
			 * will not be called. Fix by checking fault_addr not
			 * equal to 0
			 */
			prev_fault_addr = next_fault_addr;
			next_fault_addr = mmufault->fault_addr;
			if (prev_fault_addr == next_fault_addr) {
				nvgpu_log(g, gpu_dbg_intr,
					"pte already scanned");
				if (mmufault->refch != NULL) {
					nvgpu_channel_put(mmufault->refch);
					mmufault->refch = NULL;
				}
				continue;
			}
		}
#endif

		gv11b_mm_mmu_fault_handle_mmu_fault_common(g, mmufault,
				invalidate_replay_val_ptr);

	}
}

void gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(struct gk20a *g,
		 u32 fault_status, u32 index)
{
	u32 get_indx, offset, rd32_val, entries;
	struct nvgpu_mem *mem;
	struct mmu_fault_info *mmufault;
	u32 invalidate_replay_val = 0U;
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	int err;
#endif

	if (gv11b_fb_is_fault_buffer_empty(g, index, &get_indx)) {
		nvgpu_log(g, gpu_dbg_intr,
			"SPURIOUS mmu fault: reg index:%d", index);
		return;
	}
	nvgpu_log(g, gpu_dbg_intr, "%s MMU FAULT",
		(index == NVGPU_MMU_FAULT_REPLAY_REG_INDX) ?
		"REPLAY" : "NON-REPLAY");

	nvgpu_log(g, gpu_dbg_intr, "get ptr = %d", get_indx);

	mem = &g->mm.hw_fault_buf[index];
	mmufault = &g->mm.fault_info[index];

	entries = gv11b_fb_fault_buffer_size_val(g, index);
	nvgpu_log(g, gpu_dbg_intr, "buffer num entries = %d", entries);

	offset = nvgpu_safe_mult_u32(get_indx, gmmu_fault_buf_size_v()) /
			U32(sizeof(u32));
	nvgpu_log(g, gpu_dbg_intr, "starting word offset = 0x%x", offset);

	rd32_val = nvgpu_mem_rd32(g, mem,
		 nvgpu_safe_add_u32(offset, gmmu_fault_buf_entry_valid_w()));
	nvgpu_log(g, gpu_dbg_intr, "entry valid offset val = 0x%x", rd32_val);

	gv11b_mm_mmu_fault_handle_buf_valid_entry(g, mem, mmufault,
				&invalidate_replay_val, rd32_val, fault_status,
				index, get_indx, offset, entries);
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if (index == NVGPU_MMU_FAULT_REPLAY_REG_INDX &&
	    invalidate_replay_val != 0U) {
		err = gv11b_fb_replay_or_cancel_faults(g,
							invalidate_replay_val);
		if (err != 0) {
			nvgpu_err(g, "gv11b_fb replay or cancel"
							" faults failed");
		}
	}
#endif
}

void gv11b_mm_mmu_fault_handle_other_fault_notify(struct gk20a *g,
			 u32 fault_status)
{
	struct mmu_fault_info *mmufault;
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	u32 invalidate_replay_val = 0U;
	int err;
#endif

	mmufault = &g->mm.fault_info[NVGPU_MMU_FAULT_NONREPLAY_INDX];

	gv11b_mm_copy_from_fault_snap_reg(g, fault_status, mmufault);

	/* BAR2/Physical faults will not be snapped in hw fault buf */
	if (mmufault->mmu_engine_id_type == NVGPU_MMU_ENGINE_ID_TYPE_BAR2) {
		nvgpu_err(g, "BAR2 MMU FAULT");
		gv11b_fb_handle_bar2_fault(g, mmufault, fault_status);

	} else if (mmufault->mmu_engine_id_type ==
			NVGPU_MMU_ENGINE_ID_TYPE_PHYSICAL) {
		/* usually means VPR or out of bounds physical accesses */
		nvgpu_err(g, "PHYSICAL MMU FAULT");
	} else {
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
		gv11b_mm_mmu_fault_handle_mmu_fault_common(g, mmufault,
				 &invalidate_replay_val);

		if (invalidate_replay_val != 0U) {
			err = gv11b_fb_replay_or_cancel_faults(g,
					invalidate_replay_val);
			if (err != 0) {
				nvgpu_err(g, "gv11b_fb replay or cancel"
							" faults failed");
			}
		}
#endif
	}
}


void gv11b_mm_mmu_fault_disable_hw(struct gk20a *g)
{
	nvgpu_mutex_acquire(&g->mm.hub_isr_mutex);

	if ((g->ops.fb.is_fault_buf_enabled(g,
			NVGPU_MMU_FAULT_NONREPLAY_REG_INDX))) {
		g->ops.fb.fault_buf_set_state_hw(g,
				NVGPU_MMU_FAULT_NONREPLAY_REG_INDX,
				NVGPU_MMU_FAULT_BUF_DISABLED);
	}

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if ((g->ops.fb.is_fault_buf_enabled(g,
			NVGPU_MMU_FAULT_REPLAY_REG_INDX))) {
		g->ops.fb.fault_buf_set_state_hw(g,
				NVGPU_MMU_FAULT_REPLAY_REG_INDX,
				NVGPU_MMU_FAULT_BUF_DISABLED);
	}
#endif

	nvgpu_mutex_release(&g->mm.hub_isr_mutex);
}

void gv11b_mm_mmu_fault_info_mem_destroy(struct gk20a *g)
{
	struct vm_gk20a *vm = g->mm.bar2.vm;

	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->mm.hub_isr_mutex);

	if (nvgpu_mem_is_valid(
		    &g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX])) {
		nvgpu_dma_unmap_free(vm,
			 &g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX]);
	}
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if (nvgpu_mem_is_valid(
			&g->mm.hw_fault_buf[NVGPU_MMU_FAULT_REPLAY_INDX])) {
		nvgpu_dma_unmap_free(vm,
			 &g->mm.hw_fault_buf[NVGPU_MMU_FAULT_REPLAY_INDX]);
	}
#endif

	nvgpu_mutex_release(&g->mm.hub_isr_mutex);
	nvgpu_mutex_destroy(&g->mm.hub_isr_mutex);
}

static int gv11b_mm_mmu_fault_info_buf_init(struct gk20a *g)
{
	(void)g;
	return 0;
}

static void gv11b_mm_mmu_hw_fault_buf_init(struct gk20a *g)
{
	struct vm_gk20a *vm = g->mm.bar2.vm;
	int err = 0;
	size_t fb_size;

	/* Max entries take care of 1 entry used for full detection */
	fb_size = nvgpu_safe_add_u64((size_t)g->ops.channel.count(g),
				     (size_t)1);
	fb_size = nvgpu_safe_mult_u64(fb_size,
				      (size_t)gmmu_fault_buf_size_v());

	if (!nvgpu_mem_is_valid(
		&g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX])) {

		err = nvgpu_dma_alloc_map_sys(vm, fb_size,
			&g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX]);
		if (err != 0) {
			nvgpu_err(g,
			"Error in hw mmu fault buf [0] alloc in bar2 vm ");
			/* Fault will be snapped in pri reg but not in buffer */
			return;
		}
	}

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if (!nvgpu_mem_is_valid(
		&g->mm.hw_fault_buf[NVGPU_MMU_FAULT_REPLAY_INDX])) {
		err = nvgpu_dma_alloc_map_sys(vm, fb_size,
				&g->mm.hw_fault_buf[NVGPU_MMU_FAULT_REPLAY_INDX]);
		if (err != 0) {
			nvgpu_err(g,
			"Error in hw mmu fault buf [1] alloc in bar2 vm ");
			/* Fault will be snapped in pri reg but not in buffer */
			return;
		}
	}
#endif
}

void gv11b_mm_mmu_fault_setup_hw(struct gk20a *g)
{
	if (nvgpu_mem_is_valid(
			&g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX])) {
		g->ops.fb.fault_buf_configure_hw(g,
				NVGPU_MMU_FAULT_NONREPLAY_REG_INDX);
	}
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if (nvgpu_mem_is_valid(
			&g->mm.hw_fault_buf[NVGPU_MMU_FAULT_REPLAY_INDX])) {
		g->ops.fb.fault_buf_configure_hw(g,
				NVGPU_MMU_FAULT_REPLAY_REG_INDX);
	}
#endif
}

int gv11b_mm_mmu_fault_setup_sw(struct gk20a *g)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	nvgpu_mutex_init(&g->mm.hub_isr_mutex);

	err = gv11b_mm_mmu_fault_info_buf_init(g);

	if (err == 0) {
		gv11b_mm_mmu_hw_fault_buf_init(g);
	}

	return err;
}

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
static void gv11b_mm_mmu_fault_handle_replayable(struct gk20a *g,
		struct mmu_fault_info *mmufault, u32 *invalidate_replay_val)
{
	int err = 0;

	if (mmufault->fault_type == gmmu_fault_type_pte_v()) {
		nvgpu_log(g, gpu_dbg_intr, "invalid pte! try to fix");
		err = gv11b_fb_fix_page_fault(g, mmufault);
		if (err != 0) {
			*invalidate_replay_val |=
				gv11b_fb_get_replay_cancel_global_val();
		} else {
			*invalidate_replay_val |=
				gv11b_fb_get_replay_start_ack_all();
		}
	} else {
		/* cancel faults other than invalid pte */
		*invalidate_replay_val |=
			gv11b_fb_get_replay_cancel_global_val();
	}
	/*
	 * refch in mmufault is assigned at the time of copying
	 * fault info from snap reg or bar2 fault buf
	 */
	if (mmufault->refch != NULL) {
		nvgpu_channel_put(mmufault->refch);
		mmufault->refch = NULL;
	}
}

static int gv11b_fb_fix_page_fault(struct gk20a *g,
			 struct mmu_fault_info *mmufault)
{
	int err = 0;
	u32 pte[2];

	if (mmufault->refch == NULL) {
		nvgpu_log(g, gpu_dbg_intr, "refch from mmu_fault_info is NULL");
		return -EINVAL;
	}

	err = nvgpu_get_pte(g,
			mmufault->refch->vm, mmufault->fault_addr, &pte[0]);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_pte, "pte not found");
		return err;
	}
	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_pte,
			"pte: %#08x %#08x", pte[1], pte[0]);

	if (pte[0] == 0x0U && pte[1] == 0x0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_pte,
				"pte all zeros, do not set valid");
		return -1;
	}
	if ((pte[0] & gmmu_new_pte_valid_true_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_pte,
				"pte valid already set");
		return -1;
	}

	pte[0] |= gmmu_new_pte_valid_true_f();
	if ((pte[0] & gmmu_new_pte_read_only_true_f()) != 0U) {
		pte[0] &= ~(gmmu_new_pte_read_only_true_f());
	}
	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_pte,
			"new pte: %#08x %#08x", pte[1], pte[0]);

	err = nvgpu_set_pte(g,
			mmufault->refch->vm, mmufault->fault_addr, &pte[0]);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_pte, "pte not fixed");
		return err;
	}
	/* invalidate tlb so that GMMU does not use old cached translation */
	err = nvgpu_pg_elpg_ms_protected_call(g,
			g->ops.fb.tlb_invalidate(g, mmufault->refch->vm->pdb.mem));
	if (err != 0) {
		nvgpu_err(g, "tlb invalidate failed");
		return err;
	}

	err = nvgpu_get_pte(g,
			mmufault->refch->vm, mmufault->fault_addr, &pte[0]);
	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_pte,
			"pte after tlb invalidate: %#08x %#08x",
			pte[1], pte[0]);
	return err;
}
#endif
