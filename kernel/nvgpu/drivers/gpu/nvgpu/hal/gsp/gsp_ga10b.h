/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef GSP_GA10B_H
#define GSP_GA10B_H

u32 ga10b_gsp_falcon_base_addr(void);
u32 ga10b_gsp_falcon2_base_addr(void);
int ga10b_gsp_engine_reset(struct gk20a *g);
bool ga10b_gsp_validate_mem_integrity(struct gk20a *g);
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
void ga10b_gsp_flcn_setup_boot_config(struct gk20a *g);

/* queue */
u32 ga10b_gsp_queue_head_r(u32 i);
u32 ga10b_gsp_queue_head__size_1_v(void);
u32 ga10b_gsp_queue_tail_r(u32 i);
u32 ga10b_gsp_queue_tail__size_1_v(void);
int ga10b_gsp_queue_head(struct gk20a *g, u32 queue_id, u32 queue_index,
	u32 *head, bool set);
int ga10b_gsp_queue_tail(struct gk20a *g, u32 queue_id, u32 queue_index,
	u32 *tail, bool set);
void ga10b_gsp_msgq_tail(struct gk20a *g, struct nvgpu_gsp *gsp,
	u32 *tail, bool set);
int ga10b_gsp_flcn_copy_to_emem(struct gk20a *g,
	u32 dst, u8 *src, u32 size, u8 port);
int ga10b_gsp_flcn_copy_from_emem(struct gk20a *g,
	u32 src, u8 *dst, u32 size, u8 port);

/* interrupt */
void ga10b_gsp_enable_irq(struct gk20a *g, bool enable);
void ga10b_gsp_isr(struct gk20a *g, struct nvgpu_gsp *gsp);
void ga10b_gsp_set_msg_intr(struct gk20a *g);
#endif /* CONFIG_NVGPU_GSP_SCHEDULER */
#endif /* GSP_GA10B_H */
