/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef HAL_MM_MMU_FAULT_MMU_FAULT_GV11B_H
#define HAL_MM_MMU_FAULT_MMU_FAULT_GV11B_H

struct gk20a;
struct mmu_fault_info;

void gv11b_mm_mmu_fault_info_mem_destroy(struct gk20a *g);
void gv11b_mm_mmu_fault_disable_hw(struct gk20a *g);

void gv11b_mm_mmu_fault_setup_hw(struct gk20a *g);
int  gv11b_mm_mmu_fault_setup_sw(struct gk20a *g);

void gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(struct gk20a *g,
		 u32 fault_status, u32 index);
void gv11b_mm_mmu_fault_handle_mmu_fault_common(struct gk20a *g,
		 struct mmu_fault_info *mmufault, u32 *invalidate_replay_val);
void gv11b_mm_mmu_fault_handle_other_fault_notify(struct gk20a *g,
						  u32 fault_status);

void gv11b_mm_mmu_fault_parse_mmu_fault_info(struct mmu_fault_info *mmufault);

#endif
