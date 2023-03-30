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

#include <nvgpu/io.h>
#include <nvgpu/atomic.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/fifo.h>
#include <nvgpu/vm.h>
#include <nvgpu/tsg.h>
#include <nvgpu/engines.h>
#include <nvgpu/preempt.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>
#include <nvgpu/hw/gv11b/hw_gmmu_gv11b.h>
#include <nvgpu/posix/dma.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include "os/posix/os_posix.h"

#include "hal/init/hal_gv11b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include "hal/fifo/channel_gk20a.h"
#include "hal/fifo/channel_gv11b.h"
#include "hal/fifo/preempt_gv11b.h"
#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramin_gv11b.h"
#include "hal/mm/cache/flush_gk20a.h"
#include "hal/mm/gmmu/gmmu_gp10b.h"
#include "hal/mm/gmmu/gmmu_gv11b.h"
#include "hal/mm/mm_gp10b.h"
#include "hal/mm/mm_gv11b.h"
#include "hal/cic/mon/cic_ga10b.h"

#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"
#include "mmu-fault-gv11b-fusa.h"

static u32 global_count;
static u32 count;

/*
 * Write callback (for all nvgpu_writel calls).
 */
static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
		nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

/*
 * Read callback, similar to the write callback above.
 */
static void readl_access_reg_fn(struct gk20a *g,
			    struct nvgpu_reg_access *access)
{
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}

/*
 * Define all the callbacks to be used during the test. Typically all
 * write operations use the same callback, likewise for all read operations.
 */
static struct nvgpu_posix_io_callbacks mmu_faults_callbacks = {
	/* Write APIs all can use the same accessor. */
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,

	/* Likewise for the read APIs. */
	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,
};

static void init_platform(struct unit_module *m, struct gk20a *g, bool is_iGPU)
{
	if (is_iGPU) {
		nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, true);
	} else {
		nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, false);
	}
}

static u32 stub_channel_count(struct gk20a *g)
{
	return 32;
}

static int stub_mm_l2_flush(struct gk20a *g, bool invalidate)
{
	return 0;
}

static int init_mm(struct unit_module *m, struct gk20a *g)
{
	u64 low_hole, aperture_size;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct mm_gk20a *mm = &g->mm;
	int err;

	p->mm_is_iommuable = true;

	/* Minimum HALs for page_table */
	g->ops.mm.gmmu.get_default_big_page_size =
					nvgpu_gmmu_default_big_page_size;
	g->ops.mm.init_inst_block = gv11b_mm_init_inst_block;
	g->ops.mm.gmmu.get_mmu_levels = gp10b_mm_get_mmu_levels;
	g->ops.ramin.init_pdb = gv11b_ramin_init_pdb;
	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;
	g->ops.mm.setup_hw = nvgpu_mm_setup_hw;
	g->ops.fb.init_hw = gv11b_fb_init_hw;
	g->ops.fb.intr.enable = gv11b_fb_intr_enable;
	g->ops.mm.cache.fb_flush = gk20a_mm_fb_flush;
	g->ops.channel.count = stub_channel_count;
	g->ops.mm.gmmu.map = nvgpu_gmmu_map_locked;
	g->ops.mm.gmmu.unmap = nvgpu_gmmu_unmap_locked;
	g->ops.mm.gmmu.get_iommu_bit = gp10b_mm_get_iommu_bit;
	g->ops.mm.gmmu.gpu_phys_addr = gv11b_gpu_phys_addr;
	g->ops.mm.cache.l2_flush = stub_mm_l2_flush;
	g->ops.fb.tlb_invalidate = gm20b_fb_tlb_invalidate;
	g->ops.mm.gmmu.get_max_page_table_levels =
						gp10b_get_max_page_table_levels;
	g->ops.mm.mmu_fault.info_mem_destroy =
					gv11b_mm_mmu_fault_info_mem_destroy;
	g->ops.mm.mmu_fault.parse_mmu_fault_info =
					gv11b_mm_mmu_fault_parse_mmu_fault_info;

	nvgpu_posix_register_io(g, &mmu_faults_callbacks);

	/* Register space: FB_MMU */
	if (nvgpu_posix_io_add_reg_space(g, fb_mmu_ctrl_r(), 0x800) != 0) {
		unit_return_fail(m, "nvgpu_posix_io_add_reg_space failed\n");
	}

	/*
	 * Initialize VM space for system memory to be used throughout this
	 * unit module.
	 * Values below are similar to those used in nvgpu_init_system_vm()
	 */
	low_hole = SZ_4K * 16UL;
	aperture_size = GK20A_PMU_VA_SIZE;
	mm->pmu.aperture_size = GK20A_PMU_VA_SIZE;

	mm->pmu.vm = nvgpu_vm_init(g,
				   g->ops.mm.gmmu.get_default_big_page_size(),
				   low_hole,
				   0ULL,
				   nvgpu_safe_sub_u64(aperture_size, low_hole),
				   0ULL,
				   true,
				   false,
				   false,
				   "system");
	if (mm->pmu.vm == NULL) {
		unit_return_fail(m, "'system' nvgpu_vm_init failed\n");
	}

	/* BAR2 memory space */
	mm->bar2.aperture_size = U32(32) << 20U;
	mm->bar2.vm = nvgpu_vm_init(g,
		g->ops.mm.gmmu.get_default_big_page_size(),
		SZ_4K, 0ULL, nvgpu_safe_sub_u64(mm->bar2.aperture_size, SZ_4K),
		0ULL, false, false, false, "bar2");
	if (mm->bar2.vm == NULL) {
		unit_return_fail(m, "'bar2' nvgpu_vm_init failed\n");
	}

	err = nvgpu_pd_cache_init(g);
	if (err != 0) {
		unit_return_fail(m, "PD cache init failed\n");
	}

	/*
	 * This initialization will make sure that correct aperture mask
	 * is returned */
	g->mm.mmu_wr_mem.aperture = APERTURE_SYSMEM;
	g->mm.mmu_rd_mem.aperture = APERTURE_SYSMEM;

	return UNIT_SUCCESS;
}

int test_env_init_mm_mmu_fault_gv11b_fusa(struct unit_module *m,
						struct gk20a *g, void *args)
{
	g->log_mask = 0;

	init_platform(m, g, true);

	if (init_mm(m, g) != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed\n");
	}

	g->ops.cic_mon.init = ga10b_cic_mon_init;

	if (nvgpu_cic_mon_setup(g) != 0) {
		unit_return_fail(m, "Failed to initialize CIC\n");
	}

	if (nvgpu_cic_mon_init_lut(g) != 0) {
		unit_return_fail(m, "Failed to initialize CIC LUT\n");
	}

	return UNIT_SUCCESS;
}

#define F_MMU_FAULT_SETUP_SW_FAULT_BUF_ALLOC_FAIL	0
#define F_MMU_FAULT_SETUP_SW_DEFAULT			1

static const char *f_mmu_fault_setup_sw[] = {
	"mmu_fault_setup_sw_alloc_fail",
	"mmu_fault_setup_sw_default",
};

int test_gv11b_mm_mmu_fault_setup_sw(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	int err;
	struct nvgpu_posix_fault_inj *l_dma_fi;
	u64 branch = (u64)args;

	l_dma_fi = nvgpu_dma_alloc_get_fault_injection();

	nvgpu_posix_enable_fault_injection(l_dma_fi,
		branch == F_MMU_FAULT_SETUP_SW_FAULT_BUF_ALLOC_FAIL ?
			true : false, 0);

	err = gv11b_mm_mmu_fault_setup_sw(g);
	unit_assert(err == 0, goto done);

	if (branch == F_MMU_FAULT_SETUP_SW_FAULT_BUF_ALLOC_FAIL) {
		unit_assert(
		g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX].aperture
						== APERTURE_INVALID, goto done);
	} else {
		unit_assert(
		g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX].aperture
						== APERTURE_SYSMEM, goto done);
		unit_assert(
		g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX].gpu_va
						!= 0ULL, goto done);
	}
	gv11b_mm_mmu_fault_info_mem_destroy(g);

	unit_assert(g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX].aperture
						== APERTURE_INVALID, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: %s failed\n", __func__,
						f_mmu_fault_setup_sw[branch]);
	}
	nvgpu_posix_enable_fault_injection(l_dma_fi, false, 0);
	return ret;
}

static void stub_fb_fault_buf_configure_hw(struct gk20a *g, u32 index)
{
	count = global_count;
}

int test_gv11b_mm_mmu_fault_setup_hw(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	int err;
	enum nvgpu_aperture fb_aperture_orig = APERTURE_INVALID;

	global_count = 0U;
	count = 1U;

	g->ops.fb.fault_buf_configure_hw = stub_fb_fault_buf_configure_hw;

	err = gv11b_mm_mmu_fault_setup_sw(g);
	unit_assert(err == 0, goto done);

	gv11b_mm_mmu_fault_setup_hw(g);
	unit_assert(count == global_count, goto done);
	global_count++;

	fb_aperture_orig =
		g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX].aperture;
	g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX].aperture =
		APERTURE_INVALID;

	gv11b_mm_mmu_fault_setup_hw(g);
	unit_assert(count != global_count, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}
	g->mm.hw_fault_buf[NVGPU_MMU_FAULT_NONREPLAY_INDX].aperture =
		fb_aperture_orig;
	gv11b_mm_mmu_fault_info_mem_destroy(g);
	return ret;
}

#define F_MMU_FAULT_DISABLE_HW_FALSE	0
#define F_MMU_FAULT_DISABLE_HW_TRUE	1

static const char *f_mmu_fault_disable[] = {
	"mmu_fault_disable_hw_false",
	"mmu_fault_disable_hw_true",
};

static bool fault_buf_enabled;

static bool stub_fb_is_fault_buf_enabled(struct gk20a *g, u32 index)
{
	count = global_count;
	return fault_buf_enabled;
}

static void stub_fb_fault_buf_set_state_hw(struct gk20a *g, u32 index, u32 state)
{
	global_count += 2U;
}

int test_gv11b_mm_mmu_fault_disable_hw(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	int err = 0U;
	u64 branch = (u64)args;
	struct gpu_ops gops = g->ops;

	global_count = 10U;
	count = 0U;

	err = gv11b_mm_mmu_fault_setup_sw(g);
	unit_assert(err == 0, goto done);

	g->ops.fb.is_fault_buf_enabled = stub_fb_is_fault_buf_enabled;
	g->ops.fb.fault_buf_set_state_hw = stub_fb_fault_buf_set_state_hw;
	fault_buf_enabled = branch == F_MMU_FAULT_DISABLE_HW_FALSE ?
				false : true;

	gv11b_mm_mmu_fault_disable_hw(g);
	unit_assert(count == 10U, goto done);
	unit_assert(global_count == (10U + (2U * fault_buf_enabled)), goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: %s failed\n", __func__,
						f_mmu_fault_disable[branch]);
	}
	gv11b_mm_mmu_fault_info_mem_destroy(g);
	g->ops = gops;
	return ret;
}

#define F_MMU_FAULT_ENG_ID_INVALID	0
#define F_MMU_FAULT_ENG_ID_BAR2		1
#define F_MMU_FAULT_ENG_ID_PHYSICAL	2

static const char *f_mmu_fault_notify[] = {
	"mmu_fault_notify_eng_id_invalid",
	"mmu_fault_notify_eng_id_bar2",
	"mmu_fault_notify_eng_id_physical",
};

static int stub_bus_bar2_bind(struct gk20a *g, struct nvgpu_mem *bar2_inst)
{
	return 0;
}

static u32 stub_fifo_mmu_fault_id_to_pbdma_id(struct gk20a *g, u32 mmu_fault_id)
{
	return INVAL_ID;
}

int test_gv11b_mm_mmu_fault_handle_other_fault_notify(struct unit_module *m,
					struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u64 branch = (u64)args;
	int err;
	struct gpu_ops gops = g->ops;
	u32 reg_val;

	g->ops.fb.read_mmu_fault_inst_lo_hi =
					gv11b_fb_read_mmu_fault_inst_lo_hi;
	g->ops.fb.read_mmu_fault_addr_lo_hi =
					gv11b_fb_read_mmu_fault_addr_lo_hi;
	g->ops.fb.read_mmu_fault_info = gv11b_fb_read_mmu_fault_info;
	g->ops.fb.write_mmu_fault_status = gv11b_fb_write_mmu_fault_status;
	g->ops.bus.bar2_bind = stub_bus_bar2_bind;
	g->ops.fifo.mmu_fault_id_to_pbdma_id =
					stub_fifo_mmu_fault_id_to_pbdma_id;

	reg_val = branch == F_MMU_FAULT_ENG_ID_BAR2 ?
			gmmu_fault_mmu_eng_id_bar2_v() :
			branch == F_MMU_FAULT_ENG_ID_PHYSICAL ?
				gmmu_fault_mmu_eng_id_physical_v() : 0U;
	nvgpu_writel(g, fb_mmu_fault_inst_lo_r(), reg_val);

	err = gv11b_mm_mmu_fault_setup_sw(g);
	unit_assert(err == 0, goto done);

	gv11b_mm_mmu_fault_handle_other_fault_notify(g,
					fb_mmu_fault_status_valid_set_f());

	if (branch == F_MMU_FAULT_ENG_ID_BAR2) {
		unit_assert(g->mm.fault_info[
			NVGPU_MMU_FAULT_NONREPLAY_INDX].mmu_engine_id ==
			gmmu_fault_mmu_eng_id_bar2_v(), goto done);
	} else if (branch == F_MMU_FAULT_ENG_ID_PHYSICAL) {
		unit_assert(g->mm.fault_info[
			NVGPU_MMU_FAULT_NONREPLAY_INDX].mmu_engine_id ==
			gmmu_fault_mmu_eng_id_physical_v(), goto done);
	} else {
		unit_assert(g->mm.fault_info[
			NVGPU_MMU_FAULT_NONREPLAY_INDX].mmu_engine_id ==
			0U, goto done);
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: %s failed\n", __func__,
						f_mmu_fault_notify[branch]);
	}
	gv11b_mm_mmu_fault_info_mem_destroy(g);
	g->ops = gops;
	return ret;
}

#define F_MMU_FAULT_INFO_FAULT_TYPE_INVALID		0x01ULL
#define F_MMU_FAULT_INFO_CLIENT_TYPE_INVALID		0x02ULL
#define F_MMU_FAULT_INFO_CLIENT_TYPE_HUB		0x04ULL
#define F_MMU_FAULT_INFO_CLIENT_TYPE_GPC		0x08ULL
#define F_MMU_FAULT_INFO_CLIENT_ID_INVALID		0x10ULL

#define F_MMU_FAULT_PARSE_DEFAULT			0x00ULL
/* F_MMU_FAULT_INFO_FAULT_TYPE_INVALID */
#define F_MMU_FAULT_PARSE_FAULT_TYPE_INVALID		0x01ULL
/* F_MMU_FAULT_INFO_CLIENT_TYPE_INVALID */
#define F_MMU_FAULT_PARSE_CLIENT_TYPE_INVALID		0x02ULL
/* F_MMU_FAULT_INFO_CLIENT_TYPE_HUB */
#define F_MMU_FAULT_PARSE_CLIENT_TYPE_HUB		0x04ULL
/* F_MMU_FAULT_INFO_CLIENT_TYPE_HUB + F_MMU_FAULT_INFO_CLIENT_ID_INVALID */
#define F_MMU_FAULT_PARSE_CLIENT_HUB_ID_INVALID		0x14ULL
/* F_MMU_FAULT_INFO_CLIENT_TYPE_GPC */
#define F_MMU_FAULT_PARSE_CLIENT_TYPE_GPC		0x08ULL
/* F_MMU_FAULT_INFO_CLIENT_TYPE_GPC + F_MMU_FAULT_INFO_CLIENT_ID_INVALID */
#define F_MMU_FAULT_PARSE_CLIENT_GPC_ID_INVALID		0x18ULL


int test_gv11b_mm_mmu_fault_parse_mmu_fault_info(struct unit_module *m,
					struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u64 branch = (u64)args;
	struct mmu_fault_info *mmufault =
			&g->mm.fault_info[NVGPU_MMU_FAULT_NONREPLAY_INDX];

	mmufault->fault_type = branch & F_MMU_FAULT_INFO_FAULT_TYPE_INVALID ?
				1000U : 0U;
	mmufault->client_type = branch & F_MMU_FAULT_INFO_CLIENT_TYPE_INVALID ?
		1000U :
		branch & F_MMU_FAULT_INFO_CLIENT_TYPE_HUB ?
			gmmu_fault_client_type_hub_v() :
			branch & F_MMU_FAULT_INFO_CLIENT_TYPE_GPC ?
				gmmu_fault_client_type_gpc_v() : 0U;

	mmufault->client_id = branch & F_MMU_FAULT_INFO_CLIENT_ID_INVALID ?
				1000U : 0U;

	EXPECT_BUG(gv11b_mm_mmu_fault_parse_mmu_fault_info(mmufault));

	if (!(branch & F_MMU_FAULT_PARSE_FAULT_TYPE_INVALID)) {
		unit_assert(strcmp(mmufault->fault_type_desc, "invalid pde") == 0, goto done);
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: failed\n", __func__);
	}
	return ret;
}

static u32 ret_num_lce;

static u32 stub_top_get_num_lce(struct gk20a *g)
{
	return ret_num_lce;
}

static int stub_runlist_update(struct gk20a *g,
			       struct nvgpu_runlist *rl,
			       struct nvgpu_channel *ch,
			       bool add, bool wait_for_finish)
{
	return 0;
}

static void stub_set_err_notifier_if_empty(struct nvgpu_channel *ch, u32 error)
{
}

static u32 stub_gr_init_get_no_of_sm(struct gk20a *g)
{
	return 8;
}

#define F_MMU_FAULT_VALID			0x01ULL
#define F_NVGPU_POWERED_ON			0x02ULL
#define F_MMU_FAULT_ENG_ID_CE0			0x04ULL
#define F_NUM_LCE_0				0x08ULL
#define F_MMU_FAULT_NON_REPLAYABLE		0x10ULL
#define F_MMU_FAULT_TYPE_INST_BLOCK		0x20ULL
#define F_MMU_FAULT_REFCH			0x40ULL
#define F_FAULTED_ENGINE_INVALID		0x80ULL
#define F_MMU_NACK_HANDLED			0x100ULL
#define F_TSG_VALID				0x200ULL

/* !F_MMU_FAULT_VALID */
#define F_MMU_HANDLER_FAULT_INVALID			0x00ULL
/* F_MMU_FAULT_VALID + !F_NVGPU_POWERED_ON */
#define F_MMU_HANDLER_NVGPU_POWERED_OFF			0x01ULL
/* F_MMU_FAULT_VALID + F_NVGPU_POWERED_ON + F_MMU_FAULT_ENG_ID_CE0 */
#define F_MMU_HANDLER_CE_DEFAULT			0x07ULL
/* F_MMU_FAULT_VALID + F_NVGPU_POWERED_ON + F_MMU_FAULT_ENG_ID_CE0 +
	F_NUM_LCE_0 */
#define F_MMU_HANDLER_CE_LCE_0				0x0FULL
/* F_MMU_FAULT_VALID + F_NVGPU_POWERED_ON + F_MMU_FAULT_ENG_ID_CE0 +
	F_MMU_FAULT_REFCH */
#define F_MMU_HANDLER_CE_REFCH				0x47ULL
/* F_MMU_FAULT_VALID + F_NVGPU_POWERED_ON + F_MMU_FAULT_NON_REPLAYABLE */
#define F_MMU_HANDLER_NON_REPLAYABLE_DEFAULT		0x13ULL
/* F_MMU_FAULT_VALID + F_NVGPU_POWERED_ON + F_MMU_FAULT_NON_REPLAYABLE +
 F_MMU_FAULT_TYPE_INST_BLOCK */
#define F_MMU_HANDLER_NON_REPLAYABLE_INST_BLOCK		0x33ULL
/* F_MMU_FAULT_VALID + F_NVGPU_POWERED_ON + F_MMU_FAULT_NON_REPLAYABLE +
	F_MMU_FAULT_REFCH */
#define F_MMU_HANDLER_NON_REPLAYABLE_REFCH		0x53ULL
/* F_MMU_FAULT_VALID + F_NVGPU_POWERED_ON + F_MMU_FAULT_NON_REPLAYABLE +
	F_MMU_FAULT_REFCH + F_MMU_NACK_HANDLED*/
#define F_MMU_HANDLER_NON_REPLAYABLE_REFCH_NACK_HNDLD	0x153ULL
/* F_MMU_FAULT_VALID + F_NVGPU_POWERED_ON + F_MMU_FAULT_NON_REPLAYABLE +
	F_FAULTED_ENGINE_INVALID */
#define F_MMU_HANDLER_NON_REPLAYABLE_FAULTED_INVALID	0x93ULL
/* F_MMU_FAULT_VALID + F_NVGPU_POWERED_ON + F_MMU_FAULT_NON_REPLAYABLE +
	F_NUM_LCE_0 + F_TSG_VALID */
#define F_MMU_HANDLER_NON_REPLAYABLE_TSG		0x29BULL

static const char *f_mmu_handler[] = {
	[F_MMU_HANDLER_FAULT_INVALID] = "mmu_handler_fault_invalid",
	[F_MMU_HANDLER_NVGPU_POWERED_OFF] = "mmu_handler_nvgpu_powered_off",
	[F_MMU_HANDLER_CE_DEFAULT] = "mmu_handler_ce_default",
	[F_MMU_HANDLER_CE_LCE_0] = "mmu_handler_ce_with_lce_0",
	[F_MMU_HANDLER_CE_REFCH] = "mmu_handler_ce_refch_valid",
	[F_MMU_HANDLER_NON_REPLAYABLE_DEFAULT] =
				"mmu_handler_non-replayable_default",
	[F_MMU_HANDLER_NON_REPLAYABLE_INST_BLOCK] =
				"mmu_handler_non-replayable_inst_block",
	[F_MMU_HANDLER_NON_REPLAYABLE_REFCH] =
				"mmu_handler_non-replayable_refch_valid",
	[F_MMU_HANDLER_NON_REPLAYABLE_REFCH_NACK_HNDLD] =
				"mmu_handler_non-replayable_refch_nack_handled",
	[F_MMU_HANDLER_NON_REPLAYABLE_FAULTED_INVALID] =
			"mmu_handler_non-replayable_faulted_engine_invalid",
	[F_MMU_HANDLER_NON_REPLAYABLE_TSG] =
				"mmu_handler_non-replayable_tsg_valid",
};

int test_handle_mmu_fault_common(struct unit_module *m,
						struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	u64 branch = (u64)args;
	int err;
	u32 invalidate_replay_val;
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel chA = {0};
	struct nvgpu_channel *chB = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct mmu_fault_info *mmufault =
			&g->mm.fault_info[NVGPU_MMU_FAULT_NONREPLAY_INDX];

	g->ops.top.get_num_lce = stub_top_get_num_lce;
	g->sw_quiesce_pending = true;

	err = gv11b_mm_mmu_fault_setup_sw(g);
	unit_assert(err == 0, goto done);

	mmufault->valid = branch & F_MMU_FAULT_VALID ? true : false;
	nvgpu_set_power_state(g, branch & F_NVGPU_POWERED_ON ?
			NVGPU_STATE_POWERED_ON : NVGPU_STATE_POWERED_OFF);
	mmufault->mmu_engine_id = branch & F_MMU_FAULT_ENG_ID_CE0 ?
				gmmu_fault_mmu_eng_id_ce0_v() :
				gmmu_fault_mmu_eng_id_ce0_v() - 1U;
	ret_num_lce = branch & F_NUM_LCE_0 ? 0U : 5U;
	mmufault->replayable_fault = branch & F_MMU_FAULT_NON_REPLAYABLE ?
					false : true;
	mmufault->fault_type = branch & F_MMU_FAULT_TYPE_INST_BLOCK ?
			gmmu_fault_type_unbound_inst_block_v() : 0U;
	mmufault->faulted_engine = branch & F_FAULTED_ENGINE_INVALID ?
						NVGPU_INVALID_ENG_ID : 0U;

	if (branch & F_MMU_FAULT_REFCH) {
		/* Init chA */
		chA.g = g;
		chA.tsgid = NVGPU_INVALID_TSG_ID;
		nvgpu_atomic_set(&chA.ref_count, 2);
		chA.mmu_nack_handled = branch & F_MMU_NACK_HANDLED ?
								true : false;
		mmufault->refch = &chA;
	} else if (branch & F_TSG_VALID) {
		/* Init TSG and chB */
		g->ops.gr.init.get_no_of_sm = stub_gr_init_get_no_of_sm;
		g->ops.runlist.update =	stub_runlist_update;
		g->ops.tsg.default_timeslice_us =
						nvgpu_tsg_default_timeslice_us;
		g->ops.channel.alloc_inst = nvgpu_channel_alloc_inst;
		g->ops.channel.set_error_notifier =
						stub_set_err_notifier_if_empty;
		g->ops.channel.disable = gk20a_channel_disable;
		g->ops.channel.unbind = gv11b_channel_unbind;
		g->ops.channel.free_inst = nvgpu_channel_free_inst;
		g->ops.tsg.disable = nvgpu_tsg_disable;
		g->ops.fifo.preempt_tsg = nvgpu_fifo_preempt_tsg;
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		g->aggressive_sync_destroy_thresh = 0U;
#endif

		g->fifo.g = g;

		err = nvgpu_channel_setup_sw(g);
		unit_assert(err == 0, goto done);

		err = nvgpu_tsg_setup_sw(g);
		unit_assert(err == 0, goto done);

		tsg = nvgpu_tsg_open(g, getpid());
		unit_assert(tsg != NULL, goto done);

		chB = nvgpu_channel_open_new(g, U32_MAX, false,
							getpid(), getpid());
		unit_assert(chB != NULL, goto done);

		err = nvgpu_tsg_bind_channel(tsg, chB);
		unit_assert(err == 0, goto done);

		mmufault->refch = chB;

	} else {
		mmufault->refch = NULL;
	}

	gv11b_mm_mmu_fault_handle_mmu_fault_common(g, mmufault,
							&invalidate_replay_val);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: %s failed\n", __func__, f_mmu_handler[branch]);
	}

	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_OFF);
	gv11b_mm_mmu_fault_info_mem_destroy(g);
	if (chB != NULL) {
		nvgpu_atomic_set(&chB->ref_count, 1);
		nvgpu_channel_close(chB);
	}
	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}
	g->ops = gops;
	return ret;
}

#define F_BUF_EMPTY	0x01ULL
#define F_VALID_ENTRY	0x02ULL
#define F_VALID_CH	0x04ULL

#define F_HANDLE_NON_RPLYBLE_BUF_EMPTY		0x01ULL
#define F_HANDLE_NON_RPLYBLE_INVALID_BUF_ENTRY	0x00ULL
#define F_HANDLE_NON_RPLYBLE_VALID_BUF_ENTRY	0x02ULL
#define F_HANDLE_NON_RPLYBLE_VALID_BUF_CH	0x06ULL

static const char *f_mmu_fault_nonreplay[] = {
	[F_HANDLE_NON_RPLYBLE_BUF_EMPTY] = "fault_buf_empty",
	[F_HANDLE_NON_RPLYBLE_INVALID_BUF_ENTRY] = "buf_entry_invalid",
	[F_HANDLE_NON_RPLYBLE_VALID_BUF_ENTRY] = "buf_entry_valid",
	[F_HANDLE_NON_RPLYBLE_VALID_BUF_CH] = "validbuf_entry_and_refch",
};

static u32 get_idx, put_idx;

static u32 stub_fb_read_mmu_fault_buffer_get(struct gk20a *g, u32 index)
{
	return get_idx;
}

static u32 stub_fb_read_mmu_fault_buffer_put(struct gk20a *g, u32 index)
{
	return put_idx;
}

static u32 stub_fb_read_mmu_fault_buffer_size(struct gk20a *g, u32 index)
{
	return 32U;
}

static void stub_fb_write_mmu_fault_buffer_get(struct gk20a *g, u32 index,
								u32 reg_val)
{
}

int test_handle_nonreplay_replay_fault(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	int err;
	u64 branch = (u64)args;
	u32 *data;
	struct nvgpu_channel ch = {0};
	struct gpu_ops gops = g->ops;

	g->ops.fb.read_mmu_fault_buffer_get =
					stub_fb_read_mmu_fault_buffer_get;
	g->ops.fb.read_mmu_fault_buffer_put =
					stub_fb_read_mmu_fault_buffer_put;
	g->ops.fb.read_mmu_fault_buffer_size =
					stub_fb_read_mmu_fault_buffer_size;
	g->ops.fb.write_mmu_fault_buffer_get =
					stub_fb_write_mmu_fault_buffer_get;
	g->ops.fifo.mmu_fault_id_to_pbdma_id =
					stub_fifo_mmu_fault_id_to_pbdma_id;

	err = gv11b_mm_mmu_fault_setup_sw(g);
	unit_assert(err == 0, goto done);

	get_idx = 0;
	put_idx = (branch & F_BUF_EMPTY) ? get_idx : 1U;

	data = g->mm.hw_fault_buf[0].cpu_va;

	data[gmmu_fault_buf_entry_valid_w()] = branch & F_VALID_ENTRY ?
				gmmu_fault_buf_entry_valid_m() : 0U;

	if (branch & F_VALID_CH) {
		g->fifo.channel = &ch;
		g->fifo.num_channels = 1;
		ch.referenceable = true;
	}

	gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(g, 0U, 0U);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: %s failed\n", __func__,
						f_mmu_fault_nonreplay[branch]);
	}
	gv11b_mm_mmu_fault_info_mem_destroy(g);
	g->ops = gops;
	return ret;
}

int test_env_clean_mm_mmu_fault_gv11b_fusa(struct unit_module *m,
						struct gk20a *g, void *args)
{
	g->log_mask = 0;

	nvgpu_vm_put(g->mm.pmu.vm);
	nvgpu_vm_put(g->mm.bar2.vm);

	return UNIT_SUCCESS;
}

struct unit_module_test mm_mmu_fault_gv11b_fusa_tests[] = {
	UNIT_TEST(env_init, test_env_init_mm_mmu_fault_gv11b_fusa, NULL, 0),
	UNIT_TEST(setup_sw_s0, test_gv11b_mm_mmu_fault_setup_sw, (void *)F_MMU_FAULT_SETUP_SW_FAULT_BUF_ALLOC_FAIL, 0),
	UNIT_TEST(setup_sw_s1, test_gv11b_mm_mmu_fault_setup_sw, (void *)F_MMU_FAULT_SETUP_SW_DEFAULT, 0),
	UNIT_TEST(setup_hw, test_gv11b_mm_mmu_fault_setup_hw, NULL, 0),
	UNIT_TEST(disable_hw_s0, test_gv11b_mm_mmu_fault_disable_hw, (void *)F_MMU_FAULT_DISABLE_HW_FALSE, 0),
	UNIT_TEST(disable_hw_s1, test_gv11b_mm_mmu_fault_disable_hw, (void *)F_MMU_FAULT_DISABLE_HW_TRUE, 0),
	UNIT_TEST(fault_notify_s0, test_gv11b_mm_mmu_fault_handle_other_fault_notify, (void *)F_MMU_FAULT_ENG_ID_INVALID, 0),
	UNIT_TEST(fault_notify_s1, test_gv11b_mm_mmu_fault_handle_other_fault_notify, (void *)F_MMU_FAULT_ENG_ID_BAR2, 0),
	UNIT_TEST(fault_notify_s2, test_gv11b_mm_mmu_fault_handle_other_fault_notify, (void *)F_MMU_FAULT_ENG_ID_PHYSICAL, 0),
	UNIT_TEST(parse_info_s0, test_gv11b_mm_mmu_fault_parse_mmu_fault_info, (void *)F_MMU_FAULT_PARSE_DEFAULT, 0),
	UNIT_TEST(parse_info_s1, test_gv11b_mm_mmu_fault_parse_mmu_fault_info, (void *)F_MMU_FAULT_PARSE_FAULT_TYPE_INVALID, 0),
	UNIT_TEST(parse_info_s2, test_gv11b_mm_mmu_fault_parse_mmu_fault_info, (void *)F_MMU_FAULT_PARSE_CLIENT_TYPE_INVALID, 0),
	UNIT_TEST(parse_info_s3, test_gv11b_mm_mmu_fault_parse_mmu_fault_info, (void *)F_MMU_FAULT_PARSE_CLIENT_TYPE_HUB, 0),
	UNIT_TEST(parse_info_s4, test_gv11b_mm_mmu_fault_parse_mmu_fault_info, (void *)F_MMU_FAULT_PARSE_CLIENT_HUB_ID_INVALID, 0),
	UNIT_TEST(parse_info_s5, test_gv11b_mm_mmu_fault_parse_mmu_fault_info, (void *)F_MMU_FAULT_PARSE_CLIENT_TYPE_GPC, 0),
	UNIT_TEST(parse_info_s6, test_gv11b_mm_mmu_fault_parse_mmu_fault_info, (void *)F_MMU_FAULT_PARSE_CLIENT_GPC_ID_INVALID, 0),
	UNIT_TEST(handle_mmu_common_s0, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_FAULT_INVALID, 0),
	UNIT_TEST(handle_mmu_common_s1, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_NVGPU_POWERED_OFF, 0),
	UNIT_TEST(handle_mmu_common_s2, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_CE_DEFAULT, 0),
	UNIT_TEST(handle_mmu_common_s3, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_CE_LCE_0, 0),
	UNIT_TEST(handle_mmu_common_s4, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_CE_REFCH, 0),
	UNIT_TEST(handle_mmu_common_s5, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_NON_REPLAYABLE_DEFAULT, 0),
	UNIT_TEST(handle_mmu_common_s6, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_NON_REPLAYABLE_INST_BLOCK, 0),
	UNIT_TEST(handle_mmu_common_s7, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_NON_REPLAYABLE_REFCH, 0),
	UNIT_TEST(handle_mmu_common_s8, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_NON_REPLAYABLE_REFCH_NACK_HNDLD, 0),
	UNIT_TEST(handle_mmu_common_s9, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_NON_REPLAYABLE_FAULTED_INVALID, 0),
	UNIT_TEST(handle_mmu_common_s10, test_handle_mmu_fault_common, (void *)F_MMU_HANDLER_NON_REPLAYABLE_TSG, 2),
	UNIT_TEST(handle_nonreplay_s0, test_handle_nonreplay_replay_fault, (void *)F_HANDLE_NON_RPLYBLE_BUF_EMPTY, 0),
	UNIT_TEST(handle_nonreplay_s1, test_handle_nonreplay_replay_fault, (void *)F_HANDLE_NON_RPLYBLE_INVALID_BUF_ENTRY, 0),
	UNIT_TEST(handle_nonreplay_s2, test_handle_nonreplay_replay_fault, (void *)F_HANDLE_NON_RPLYBLE_VALID_BUF_ENTRY, 0),
	UNIT_TEST(handle_nonreplay_s3, test_handle_nonreplay_replay_fault, (void *)F_HANDLE_NON_RPLYBLE_VALID_BUF_CH, 0),
	UNIT_TEST(env_clean, test_env_clean_mm_mmu_fault_gv11b_fusa, NULL, 0),
};

UNIT_MODULE(mmu_fault_gv11b_fusa, mm_mmu_fault_gv11b_fusa_tests, UNIT_PRIO_NVGPU_TEST);
