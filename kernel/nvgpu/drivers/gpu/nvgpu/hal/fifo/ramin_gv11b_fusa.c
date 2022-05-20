/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>

#include <nvgpu/hw/gv11b/hw_ram_gv11b.h>

#include "hal/fifo/ramin_gv11b.h"

void gv11b_ramin_set_gr_ptr(struct gk20a *g,
		struct nvgpu_mem *inst_block, u64 gpu_va)
{
	u32 addr_lo = u64_lo32(gpu_va) >> ram_in_base_shift_v();
	u32 addr_hi = u64_hi32(gpu_va);

	/* point this address to engine_wfi_ptr */
	nvgpu_mem_wr32(g, inst_block, ram_in_engine_wfi_target_w(),
		ram_in_engine_cs_wfi_v() |
		ram_in_engine_wfi_mode_f(ram_in_engine_wfi_mode_virtual_v()) |
		ram_in_engine_wfi_ptr_lo_f(addr_lo));

	nvgpu_mem_wr32(g, inst_block, ram_in_engine_wfi_ptr_hi_w(),
		ram_in_engine_wfi_ptr_hi_f(addr_hi));
}

static void gv11b_subctx_commit_valid_mask(struct gk20a *g,
		struct nvgpu_mem *inst_block, u32 max_subctx_count)
{
	u32 id;
	u32 subctx_count = max_subctx_count;

	for (id = 0U; id < max_subctx_count; id += 32U) {
		u32 subctx_mask_max_bit = ((subctx_count < 32U) ?
			(subctx_count % 32U) : 0U);
		u32 subctx_mask = U32_MAX;

		if (subctx_mask_max_bit != 0U) {
			subctx_mask = nvgpu_safe_sub_u32(
				BIT32(subctx_mask_max_bit), 1U);
		}

		nvgpu_mem_wr32(g, inst_block,
				ram_in_sc_pdb_valid_long_w(id), subctx_mask);

		nvgpu_log(g, gpu_dbg_info | gpu_dbg_mig,
			"id[%d] max_subctx_count[%u] subctx_mask_max_bit[%u] "
				"subctx_count[%u] subctx_mask[%x] ",
			id, max_subctx_count, subctx_mask_max_bit,
			subctx_count, subctx_mask);

		if (subctx_count > 32U) {
			subctx_count = nvgpu_safe_sub_u32(subctx_count, 32U);
		}
	}
}

static void gv11b_subctx_commit_pdb(struct gk20a *g,
		struct nvgpu_mem *inst_block, struct nvgpu_mem *pdb_mem,
		bool replayable, u32 max_subctx_count)
{
	u32 lo, hi;
	u32 subctx_id = 0;
	u32 format_word;
	u32 pdb_addr_lo, pdb_addr_hi;
	u64 pdb_addr;
	u32 aperture = nvgpu_aperture_mask(g, pdb_mem,
				ram_in_sc_page_dir_base_target_sys_mem_ncoh_v(),
				ram_in_sc_page_dir_base_target_sys_mem_coh_v(),
				ram_in_sc_page_dir_base_target_vid_mem_v());

	pdb_addr = nvgpu_mem_get_addr(g, pdb_mem);
	pdb_addr_lo = u64_lo32(pdb_addr >> ram_in_base_shift_v());
	pdb_addr_hi = u64_hi32(pdb_addr);
	format_word = ram_in_sc_page_dir_base_target_f(aperture, 0U) |
		ram_in_sc_page_dir_base_vol_f(
		ram_in_sc_page_dir_base_vol_true_v(), 0U) |
		ram_in_sc_use_ver2_pt_format_f(1U, 0U) |
		ram_in_sc_big_page_size_f(1U, 0U) |
		ram_in_sc_page_dir_base_lo_0_f(pdb_addr_lo);

	if (replayable) {
		format_word |=
			ram_in_sc_page_dir_base_fault_replay_tex_f(1U, 0U) |
			ram_in_sc_page_dir_base_fault_replay_gcc_f(1U, 0U);
	}

	nvgpu_log(g, gpu_dbg_info, " pdb info lo %x hi %x",
					format_word, pdb_addr_hi);
	for (subctx_id = 0U; subctx_id < max_subctx_count; subctx_id++) {
		lo = ram_in_sc_page_dir_base_vol_w(subctx_id);
		hi = ram_in_sc_page_dir_base_hi_w(subctx_id);
		nvgpu_mem_wr32(g, inst_block, lo, format_word);
		nvgpu_mem_wr32(g, inst_block, hi, pdb_addr_hi);
	}
}

void gv11b_ramin_init_subctx_pdb(struct gk20a *g,
		struct nvgpu_mem *inst_block, struct nvgpu_mem *pdb_mem,
		bool replayable, u32 max_subctx_count)
{
	gv11b_subctx_commit_pdb(g, inst_block, pdb_mem, replayable,
		max_subctx_count);
	gv11b_subctx_commit_valid_mask(g, inst_block, max_subctx_count);

}

void gv11b_ramin_set_eng_method_buffer(struct gk20a *g,
		struct nvgpu_mem *inst_block, u64 gpu_va)
{
	u32 addr_lo = u64_lo32(gpu_va);
	u32 addr_hi = u64_hi32(gpu_va);

	nvgpu_mem_wr32(g, inst_block, ram_in_eng_method_buffer_addr_lo_w(),
			addr_lo);
	nvgpu_mem_wr32(g, inst_block, ram_in_eng_method_buffer_addr_hi_w(),
			addr_hi);
}

void gv11b_ramin_init_pdb(struct gk20a *g, struct nvgpu_mem *inst_block,
		u64 pdb_addr, struct nvgpu_mem *pdb_mem)
{
	u32 pdb_addr_lo = u64_lo32(pdb_addr >> ram_in_base_shift_v());
	u32 pdb_addr_hi = u64_hi32(pdb_addr);

	nvgpu_log_info(g, "pde pa=0x%llx", pdb_addr);

	nvgpu_mem_wr32(g, inst_block, ram_in_page_dir_base_lo_w(),
		nvgpu_aperture_mask(g, pdb_mem,
				ram_in_page_dir_base_target_sys_mem_ncoh_f(),
				ram_in_page_dir_base_target_sys_mem_coh_f(),
				ram_in_page_dir_base_target_vid_mem_f()) |
		ram_in_page_dir_base_vol_true_f() |
		ram_in_big_page_size_64kb_f() |
		ram_in_page_dir_base_lo_f(pdb_addr_lo) |
		ram_in_use_ver2_pt_format_true_f());

	nvgpu_mem_wr32(g, inst_block, ram_in_page_dir_base_hi_w(),
		ram_in_page_dir_base_hi_f(pdb_addr_hi));
}
