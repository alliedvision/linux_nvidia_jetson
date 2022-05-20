/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_SEC2_H
#define NVGPU_GOPS_SEC2_H

#include <nvgpu/types.h>

struct gk20a;

struct gops_sec2 {
	int (*init_sec2_setup_sw)(struct gk20a *g);
	int (*init_sec2_support)(struct gk20a *g);
	int (*sec2_destroy)(struct gk20a *g);
	void (*secured_sec2_start)(struct gk20a *g);
	void (*enable_irq)(struct nvgpu_sec2 *sec2, bool enable);
	bool (*is_interrupted)(struct nvgpu_sec2 *sec2);
	u32 (*get_intr)(struct gk20a *g);
	bool (*msg_intr_received)(struct gk20a *g);
	void (*set_msg_intr)(struct gk20a *g);
	void (*clr_intr)(struct gk20a *g, u32 intr);
	void (*process_intr)(struct gk20a *g, struct nvgpu_sec2 *sec2);
	void (*msgq_tail)(struct gk20a *g, struct nvgpu_sec2 *sec2,
		u32 *tail, bool set);
	u32 (*falcon_base_addr)(void);
	int (*sec2_reset)(struct gk20a *g);
	int (*sec2_copy_to_emem)(struct gk20a *g, u32 dst,
				 u8 *src, u32 size, u8 port);
	int (*sec2_copy_from_emem)(struct gk20a *g,
				   u32 src, u8 *dst, u32 size, u8 port);
	int (*sec2_queue_head)(struct gk20a *g,
			       u32 queue_id, u32 queue_index,
			       u32 *head, bool set);
	int (*sec2_queue_tail)(struct gk20a *g,
			       u32 queue_id, u32 queue_index,
			       u32 *tail, bool set);
	void (*flcn_setup_boot_config)(struct gk20a *g);
};

#endif /* NVGPU_GOPS_SEC2_H */
