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

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/mm.h>
#include <nvgpu/channel.h>
#include <nvgpu/dma.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/hw/gv11b/hw_ram_gv11b.h>

#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramfc_gv11b.h"

#include "../../nvgpu-fifo-common.h"
#include "nvgpu-ramfc-gv11b.h"

#ifdef RAMFC_GV11B_UNIT_DEBUG
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

static u32 global_count;

static u32 stub_pbdma_acquire_val(u64 timeout)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_get_gp_base(u64 gpfifo_base)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_get_gp_base_hi(u64 gpfifo_base, u32 gpfifo_entry)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_get_signature(struct gk20a *g)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_get_fc_pb_header(void)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_get_fc_subdevice(void)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_get_fc_target(const struct nvgpu_device *dev)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_get_fc_runlist_timeslice(void)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_set_channel_info_veid(u32 subctx_id)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_get_config_auth_level_privileged(void)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_get_ctrl_hce_priv_mode_yes(void)
{
	global_count++;
	return 0U;
}

static u32 stub_pbdma_config_userd_writeback_enable(u32 v)
{
	global_count++;
	return 5U;
}

static int stub_ramfc_commit_userd(struct nvgpu_channel *ch)
{
	global_count++;
	return 0;
}

static void stub_ramin_init_subctx_pdb(struct gk20a *g,
			struct nvgpu_mem *inst_block, struct nvgpu_mem *pdb_mem,
			bool replayable, u32 max_subctx_count)
{
	global_count++;
}

#define F_RAMFC_SETUP_PRIVILEDGED_CH			BIT(0)
#define F_RAMFC_SETUP_LAST				BIT(1)

static const char *f_ramfc_setup[] = {
	"priviledged_ch_true",
};

int test_gv11b_ramfc_setup(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_channel ch;
	struct vm_gk20a vm = {0};
	int ret = UNIT_FAIL;
	u32 branches = 0U;
	int err;

	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;
	g->ops.pbdma.acquire_val = stub_pbdma_acquire_val;
	g->ops.ramin.init_subctx_pdb = stub_ramin_init_subctx_pdb;
	g->ops.pbdma.get_gp_base = stub_pbdma_get_gp_base;
	g->ops.pbdma.get_gp_base_hi = stub_pbdma_get_gp_base_hi;
	g->ops.pbdma.get_signature = stub_pbdma_get_signature;
	g->ops.pbdma.get_fc_pb_header = stub_pbdma_get_fc_pb_header;
	g->ops.pbdma.get_fc_subdevice = stub_pbdma_get_fc_subdevice;
	g->ops.pbdma.get_fc_target = stub_pbdma_get_fc_target;
	g->ops.pbdma.get_fc_runlist_timeslice =
					stub_pbdma_get_fc_runlist_timeslice;
	g->ops.pbdma.set_channel_info_veid = stub_pbdma_set_channel_info_veid;
	g->ops.pbdma.get_config_auth_level_privileged =
				stub_pbdma_get_config_auth_level_privileged;
	g->ops.pbdma.get_ctrl_hce_priv_mode_yes =
					stub_pbdma_get_ctrl_hce_priv_mode_yes;
	g->ops.pbdma.config_userd_writeback_enable =
				stub_pbdma_config_userd_writeback_enable;
	g->ops.ramfc.commit_userd = stub_ramfc_commit_userd;

	/* Aperture should be fixed = SYSMEM */
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, true);
	err = nvgpu_alloc_inst_block(g, &ch.inst_block);
	unit_assert(err == 0, goto done);

	ch.g = g;
	ch.subctx_id = 1;
	ch.vm = &vm;
	memset(&ch.vm->pdb, 0, sizeof(struct nvgpu_gmmu_pd));

	for (branches = 0U; branches < F_RAMFC_SETUP_LAST; branches++) {
		unit_verbose(m, "%s branches=%s\n",
			__func__, branches_str(branches, f_ramfc_setup));

		global_count = 0U;
		ch.is_privileged_channel =
				branches & F_RAMFC_SETUP_PRIVILEDGED_CH ?
				true : false;

		err = gv11b_ramfc_setup(&ch, 0U, 0U, 0ULL, 0U);
		unit_assert(err == 0, goto done);
		unit_assert(nvgpu_mem_rd32(g, &ch.inst_block,
				ram_fc_config_w()) == 5U, goto done);

		if (branches & F_RAMFC_SETUP_PRIVILEDGED_CH) {
			unit_assert(global_count == 15U, goto done);
		} else {
			unit_assert(global_count == 13U, goto done);
		}
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
				branches_str(branches, f_ramfc_setup));
	}

	nvgpu_free_inst_block(g, &ch.inst_block);
	g->ops = gops;
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, false);
	return ret;
}

int test_gv11b_ramfc_capture_ram_dump(struct unit_module *m,
						struct gk20a *g, void *args)
{
	struct nvgpu_channel ch;
	struct nvgpu_channel_dump_info info;
	int ret = UNIT_FAIL;
	int err;

	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;

	err = nvgpu_alloc_inst_block(g, &ch.inst_block);
	unit_assert(err == 0, goto done);

	nvgpu_memset(g, &ch.inst_block, 0U, 0xa5U, 256U);

	gv11b_ramfc_capture_ram_dump(g, &ch, &info);
	unit_assert(info.inst.pb_top_level_get == 0xa5a5a5a5a5a5a5a5,
			goto done);
	unit_assert(info.inst.pb_count == 0xa5a5a5a5, goto done);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	nvgpu_free_inst_block(g, &ch.inst_block);
	return ret;
}

struct unit_module_test nvgpu_ramfc_gv11b_tests[] = {
	UNIT_TEST(ramfc_setup, test_gv11b_ramfc_setup, NULL, 0),
	UNIT_TEST(capture_ram_dump, test_gv11b_ramfc_capture_ram_dump, NULL, 0),
};

UNIT_MODULE(nvgpu_ramfc_gv11b, nvgpu_ramfc_gv11b_tests, UNIT_PRIO_NVGPU_TEST);
