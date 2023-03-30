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
#ifndef UNIT_FIFO_RAMIN_GV11B_FUSA_H
#define UNIT_FIFO_RAMIN_GV11B_FUSA_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-ramin-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/ramin/gv11b
 */

/**
 * Test specification for: test_gv11b_ramin_set_gr_ptr
 *
 * Description: Test GR address set in instance block
 *
 * Test Type: Feature
 *
 * Targets: gops_ramin.set_gr_ptr, gv11b_ramin_set_gr_ptr,
 * nvgpu_free_inst_block
 *
 * Input: None
 *
 * Steps:
 * - Store GPU_VA of GR engine context state in channel instance block.
 * - Check that stored value is correct.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_ramin_set_gr_ptr(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gv11b_ramin_init_subctx_pdb
 *
 * Description: Test page directory buffer configure for sub-contexts of
 *              instance block
 *
 * Test Type: Feature
 *
 * Targets: gops_ramin.init_subctx_pdb, gv11b_ramin_init_subctx_pdb,
 *          gv11b_subctx_commit_pdb, gv11b_subctx_commit_valid_mask
 *
 * Input: None
 *
 * Steps:
 * - Build PDB entry with PT version, big page size, volatile attribute and
 *   pdb_mem aperture mask. If errors are replayable, set replayable attribute
 *   for TEX and GCC faults. Set lo and hi 32-bits to point to pdb_mem and store
 *   this related entry in instance block.
 * - Check that the stored entry value is correct.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_ramin_init_subctx_pdb(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gv11b_ramin_set_eng_method_buffer
 *
 * Description: Test engine method buffer set
 *
 * Test Type: Feature
 *
 * Targets: gops_ramin.set_eng_method_buffer, gv11b_ramin_set_eng_method_buffer
 *
 * Input: None
 *
 * Steps:
 * - Save engine method buffer gpu_va to instance block data.
 * - Check address stored at specific offset is equal to given address.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_ramin_set_eng_method_buffer(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_ramin_init_pdb
 *
 * Description: Initialize instance block's PDB
 *
 * Test Type: Feature
 *
 * Targets: gops_ramin.init_pdb, gv11b_ramin_init_pdb
 *
 * Input: None
 *
 * Steps:
 * - Configure PDB aperture, big page size, pdb address, PT format and default
 *   attribute.
 * - Check page directory base values stored in instance block are correct.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_ramin_init_pdb(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * @}
 */

#endif /* UNIT_FIFO_RAMIN_GV11B_FUSA_H */
