/*
 * Copyright (c) 2011-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FIFO_MMU_FAULT_GK20A_H
#define NVGPU_FIFO_MMU_FAULT_GK20A_H

#include <nvgpu/types.h>

struct gk20a;
struct mmu_fault_info;

void gk20a_fifo_get_mmu_fault_desc(struct mmu_fault_info *mmufault);
void gk20a_fifo_get_mmu_fault_client_desc(struct mmu_fault_info *mmufault);
void gk20a_fifo_get_mmu_fault_gpc_desc(struct mmu_fault_info *mmufault);
void gk20a_fifo_get_mmu_fault_info(struct gk20a *g, u32 mmu_fault_id,
	struct mmu_fault_info *mmufault);

void gk20a_fifo_mmu_fault_info_dump(struct gk20a *g, u32 engine_id,
	u32 mmu_fault_id, bool fake_fault, struct mmu_fault_info *mmufault);

void gk20a_fifo_handle_dropped_mmu_fault(struct gk20a *g);

void gk20a_fifo_handle_mmu_fault(struct gk20a *g, u32 mmu_fault_engines,
	u32 hw_id, bool id_is_tsg);

void gk20a_fifo_handle_mmu_fault_locked(struct gk20a *g, u32 mmu_fault_engines,
	u32 hw_id, bool id_is_tsg);

#endif /* NVGPU_FIFO_MMU_FAULT_GK20A_H */
