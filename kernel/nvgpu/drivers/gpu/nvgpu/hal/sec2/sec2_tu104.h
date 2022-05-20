/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_SEC2_TU104_H
#define NVGPU_SEC2_TU104_H

struct gk20a;
struct nvgpu_sec2;

int tu104_sec2_reset(struct gk20a *g);
int tu104_sec2_flcn_copy_to_emem(struct gk20a *g,
	u32 dst, u8 *src, u32 size, u8 port);
int tu104_sec2_flcn_copy_from_emem(struct gk20a *g,
	u32 src, u8 *dst, u32 size, u8 port);
void tu104_sec2_flcn_setup_boot_config(struct gk20a *g);

int tu104_sec2_queue_head(struct gk20a *g, u32 queue_id, u32 queue_index,
	u32 *head, bool set);
int tu104_sec2_queue_tail(struct gk20a *g, u32 queue_id, u32 queue_index,
	u32 *tail, bool set);
void tu104_sec2_msgq_tail(struct gk20a *g, struct nvgpu_sec2 *sec2,
	u32 *tail, bool set);

bool tu104_sec2_is_interrupted(struct nvgpu_sec2 *sec2);
u32 tu104_sec2_get_intr(struct gk20a *g);
bool tu104_sec2_msg_intr_received(struct gk20a *g);
void tu104_sec2_set_msg_intr(struct gk20a *g);
void tu104_sec2_clr_intr(struct gk20a *g, u32 intr);
void tu104_sec2_process_intr(struct gk20a *g, struct nvgpu_sec2 *sec2);
void tu104_sec2_enable_irq(struct nvgpu_sec2 *sec2, bool enable);
void tu104_start_sec2_secure(struct gk20a *g);
u32 tu104_sec2_falcon_base_addr(void);

#endif /* NVGPU_SEC2_TU104_H */
