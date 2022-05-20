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
#include <nvgpu/dma.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/hw/gv11b/hw_ram_gv11b.h>

#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramin_gv11b.h"

#include "../../nvgpu-fifo-common.h"
#include "ramin-gv11b-fusa.h"

#ifdef RAMIN_GV11B_UNIT_DEBUG
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

int test_gv11b_ramin_set_gr_ptr(struct unit_module *m, struct gk20a *g,
								void *args)
{
	struct nvgpu_mem inst_block;
	int ret = UNIT_FAIL;
	int err;
	u32 addr_lo = 1U;
	u32 addr_hi = 2U;
	u64 addr = ((u64)addr_hi << 32U) | (addr_lo << ram_in_base_shift_v());
	u32 data_lo = ram_in_engine_cs_wfi_v() |
		ram_in_engine_wfi_mode_f(ram_in_engine_wfi_mode_virtual_v()) |
		ram_in_engine_wfi_ptr_lo_f(addr_lo);
	u32 data_hi = ram_in_engine_wfi_ptr_hi_f(addr_hi);
	u32 *data_ptr;

	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;

	err = nvgpu_alloc_inst_block(g, &inst_block);
	unit_assert(err == 0, goto done);
	data_ptr = inst_block.cpu_va;

	gv11b_ramin_set_gr_ptr(g, &inst_block, addr);
	unit_assert(data_ptr[ram_in_engine_wfi_target_w()] == data_lo,
			goto done);
	unit_assert(data_ptr[ram_in_engine_wfi_ptr_hi_w()] == data_hi,
			goto done);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	nvgpu_free_inst_block(g, &inst_block);
	return ret;
}

#define F_INIT_SUBCTX_PDB_REPLAYABLE			BIT(0)
#define F_INIT_SUBCTX_PDB_LAST				BIT(1)

static const char *f_init_subctx_pdb[] = {
	"init_subctx_pdb",
};

int test_gv11b_ramin_init_subctx_pdb(struct unit_module *m, struct gk20a *g,
								void *args)
{
	struct nvgpu_mem inst_block;
	struct nvgpu_mem pdb_mem;
	int ret = UNIT_FAIL;
	u32 branches = 0U;
	int err;
	bool replayable;
	u32 subctx_id, format_data;
	u32 addr_lo, addr_hi;
	u32 pdb_addr_lo, pdb_addr_hi;
	u64 pdb_addr;
	u32 max_subctx_count = ram_in_sc_page_dir_base_target__size_1_v();
	u32 aperture = ram_in_sc_page_dir_base_target_sys_mem_ncoh_v();

	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;

	/* Aperture should be fixed = SYSMEM */
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, true);
	err = nvgpu_alloc_inst_block(g, &inst_block);
	unit_assert(err == 0, goto done);

	err = nvgpu_dma_alloc(g, g->ops.ramin.alloc_size(), &pdb_mem);
	unit_assert(err == 0, goto done);

	pdb_addr = nvgpu_mem_get_addr(g, &pdb_mem);
	pdb_addr_lo = u64_lo32(pdb_addr >> ram_in_base_shift_v());
	pdb_addr_hi = u64_hi32(pdb_addr);

	format_data = ram_in_sc_page_dir_base_target_f(aperture, 0U) |
		ram_in_sc_page_dir_base_vol_f(
		ram_in_sc_page_dir_base_vol_true_v(), 0U) |
		ram_in_sc_use_ver2_pt_format_f(1U, 0U) |
		ram_in_sc_big_page_size_f(1U, 0U) |
		ram_in_sc_page_dir_base_lo_0_f(pdb_addr_lo);

	for (branches = 0U; branches < F_INIT_SUBCTX_PDB_LAST; branches++) {
		unit_verbose(m, "%s branches=%s\n",
			__func__, branches_str(branches, f_init_subctx_pdb));

		replayable = branches & F_INIT_SUBCTX_PDB_REPLAYABLE ?
				true : false;

		if (replayable) {
			format_data |=
				ram_in_sc_page_dir_base_fault_replay_tex_f(
					1U, 0U) |
				ram_in_sc_page_dir_base_fault_replay_gcc_f(
					1U, 0U);
		}

		gv11b_ramin_init_subctx_pdb(g, &inst_block, &pdb_mem,
								replayable, 64);

		for (subctx_id = 0; subctx_id < max_subctx_count; subctx_id++) {
			addr_lo = ram_in_sc_page_dir_base_vol_w(subctx_id);
			addr_hi = ram_in_sc_page_dir_base_hi_w(subctx_id);
			unit_assert(nvgpu_mem_rd32(g, &inst_block, addr_lo) ==
							format_data, goto done);
			unit_assert(nvgpu_mem_rd32(g, &inst_block, addr_hi) ==
							pdb_addr_hi, goto done);
		}

		for (subctx_id = 0; subctx_id < ram_in_sc_pdb_valid__size_1_v();
							subctx_id += 32U) {
			unit_assert(nvgpu_mem_rd32(g, &inst_block,
				ram_in_sc_pdb_valid_long_w(subctx_id)) ==
							U32_MAX, goto done);
		}

	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s branches=%s\n", __func__,
				branches_str(branches, f_init_subctx_pdb));
	}

	nvgpu_dma_free(g, &pdb_mem);
	nvgpu_free_inst_block(g, &inst_block);
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, false);
	return ret;
}

int test_gv11b_ramin_set_eng_method_buffer(struct unit_module *m,
						struct gk20a *g, void *args)
{
	struct nvgpu_mem inst_block;
	int ret = UNIT_FAIL;
	int err;
	u32 addr_lo = 1U;
	u32 addr_hi = 2U;
	u64 addr = ((u64)addr_hi << 32U) | (addr_lo);
	u32 *data_ptr;

	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;

	err = nvgpu_alloc_inst_block(g, &inst_block);
	unit_assert(err == 0, goto done);
	data_ptr = inst_block.cpu_va;

	gv11b_ramin_set_eng_method_buffer(g, &inst_block, addr);
	unit_assert(data_ptr[ram_in_eng_method_buffer_addr_lo_w()] == addr_lo,
			goto done);
	unit_assert(data_ptr[ram_in_eng_method_buffer_addr_hi_w()] == addr_hi,
			goto done);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	nvgpu_free_inst_block(g, &inst_block);
	return ret;
}

int test_gv11b_ramin_init_pdb(struct unit_module *m, struct gk20a *g,
								void *args)
{
	struct nvgpu_mem inst_block;
	struct nvgpu_mem pdb_mem;
	int ret = UNIT_FAIL;
	int err;
	u32 data;
	u32 pdb_addr_lo, pdb_addr_hi;
	u64 pdb_addr;
	u32 aperture;

	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;

	/* Aperture should be fixed = SYSMEM */
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, true);
	err = nvgpu_alloc_inst_block(g, &inst_block);
	unit_assert(err == 0, goto done);

	err = nvgpu_dma_alloc(g, g->ops.ramin.alloc_size(), &pdb_mem);
	unit_assert(err == 0, goto done);

	pdb_addr = nvgpu_mem_get_addr(g, &pdb_mem);
	pdb_addr_lo = u64_lo32(pdb_addr >> ram_in_base_shift_v());
	pdb_addr_hi = u64_hi32(pdb_addr);

	aperture = ram_in_sc_page_dir_base_target_sys_mem_ncoh_v();

	data = aperture | ram_in_page_dir_base_vol_true_f() |
		ram_in_big_page_size_64kb_f() |
		ram_in_page_dir_base_lo_f(pdb_addr_lo) |
		ram_in_use_ver2_pt_format_true_f();

	gv11b_ramin_init_pdb(g, &inst_block, pdb_addr, &pdb_mem);

	unit_assert(nvgpu_mem_rd32(g, &inst_block,
			ram_in_page_dir_base_lo_w()) ==	data, goto done);
	unit_assert(nvgpu_mem_rd32(g, &inst_block,
			ram_in_page_dir_base_hi_w()) ==
			ram_in_page_dir_base_hi_f(pdb_addr_hi), goto done);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}

	nvgpu_dma_free(g, &pdb_mem);
	nvgpu_free_inst_block(g, &inst_block);
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, false);
	return ret;
}

struct unit_module_test ramin_gv11b_fusa_tests[] = {
	UNIT_TEST(set_gr_ptr, test_gv11b_ramin_set_gr_ptr, NULL, 0),
	UNIT_TEST(init_subctx_pdb, test_gv11b_ramin_init_subctx_pdb, NULL, 2),
	UNIT_TEST(set_eng_method_buf, test_gv11b_ramin_set_eng_method_buffer, NULL, 0),
	UNIT_TEST(init_pdb, test_gv11b_ramin_init_pdb, NULL, 0),
};

UNIT_MODULE(ramin_gv11b_fusa, ramin_gv11b_fusa_tests, UNIT_PRIO_NVGPU_TEST);
