/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_GSP_H
#define NVGPU_GOPS_GSP_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_gsp;

#define GSP_WAIT_TIME_MS 10000U

struct gops_gsp {
	u32 (*falcon_base_addr)(void);
	u32 (*falcon2_base_addr)(void);
	void (*falcon_setup_boot_config)(struct gk20a *g);
	int (*gsp_reset)(struct gk20a *g);
	bool (*validate_mem_integrity)(struct gk20a *g);
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
	u32 (*gsp_get_queue_head)(u32 i);
	u32 (*gsp_get_queue_head_size)(void);
	u32 (*gsp_get_queue_tail_size)(void);
	u32 (*gsp_get_queue_tail)(u32 i);
	int (*gsp_copy_to_emem)(struct gk20a *g, u32 dst,
			u8 *src, u32 size, u8 port);
	int (*gsp_copy_from_emem)(struct gk20a *g,
			u32 src, u8 *dst, u32 size, u8 port);
	int (*gsp_queue_head)(struct gk20a *g,
			u32 queue_id, u32 queue_index,
			u32 *head, bool set);
	int (*gsp_queue_tail)(struct gk20a *g,
			u32 queue_id, u32 queue_index,
			u32 *tail, bool set);
	void (*msgq_tail)(struct gk20a *g, struct nvgpu_gsp *gsp,
			u32 *tail, bool set);
	void (*enable_irq)(struct gk20a *g, bool enable);
	void (*gsp_isr)(struct gk20a *g, struct nvgpu_gsp *gsp);
	void (*set_msg_intr)(struct gk20a *g);
#endif /* CONFIG_NVGPU_GSP_SCHEDULER */
};

#endif /* NVGPU_GOPS_GSP_H */
