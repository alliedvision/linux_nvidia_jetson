/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PBDMA_GA10B_H
#define NVGPU_PBDMA_GA10B_H

#include <nvgpu/types.h>

#define HW_PBDMA_STRIDE			2048U
#define HW_PBDMA_BASE			0x040000U

#define PBDMA_PRI_BASE_INVALID		U32_MAX
#define PBDMA_ID_INVALID		U32_MAX

#define INTR_SIZE			0U
#define INTR_SET_SIZE			1U
#define INTR_CLEAR_SIZE			2U

struct gk20a;
struct nvgpu_debug_context;
struct nvgpu_pbdma_status_info;
struct nvgpu_device;

void ga10b_pbdma_intr_enable(struct gk20a *g, bool enable);
void ga10b_pbdma_handle_intr(struct gk20a *g, u32 pbdma_id, bool recover);
bool ga10b_pbdma_handle_intr_0(struct gk20a *g, u32 pbdma_id, u32 pbdma_intr_0,
			u32 *error_notifier);
bool ga10b_pbdma_handle_intr_1(struct gk20a *g, u32 pbdma_id, u32 pbdma_intr_1,
			u32 *error_notifier);

u32 ga10b_pbdma_read_data(struct gk20a *g, u32 pbdma_id);
void ga10b_pbdma_reset_header(struct gk20a *g, u32 pbdma_id);
void ga10b_pbdma_reset_method(struct gk20a *g, u32 pbdma_id,
			u32 pbdma_method_index);

void ga10b_pbdma_clear_all_intr(struct gk20a *g, u32 pbdma_id);
void ga10b_pbdma_disable_and_clear_all_intr(struct gk20a *g);
u32 ga10b_pbdma_channel_fatal_0_intr_descs(void);
u32 ga10b_pbdma_device_fatal_0_intr_descs(void);

u32 ga10b_pbdma_set_channel_info_chid(u32 chid);
u32 ga10b_pbdma_set_intr_notify(u32 eng_intr_vector);
u32 ga10b_pbdma_set_clear_intr_offsets(struct gk20a *g,
			u32 set_clear_size);

u32 ga10b_pbdma_get_fc_target(const struct nvgpu_device *dev);

void ga10b_pbdma_dump_status(struct gk20a *g, struct nvgpu_debug_context *o);

u32 ga10b_pbdma_get_mmu_fault_id(struct gk20a *g, u32 pbdma_id);
u32 ga10b_pbdma_get_num_of_pbdmas(void);

#endif /* NVGPU_PBDMA_GA10B_H */
