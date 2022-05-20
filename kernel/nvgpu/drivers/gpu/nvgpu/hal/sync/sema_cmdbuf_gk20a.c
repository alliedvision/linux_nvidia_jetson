/*
 * GK20A sema cmdbuf
 *
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log.h>
#include <nvgpu/semaphore.h>
#include <nvgpu/priv_cmdbuf.h>

#include "sema_cmdbuf_gk20a.h"

u32 gk20a_sema_get_wait_cmd_size(void)
{
	return 8U;
}

u32 gk20a_sema_get_incr_cmd_size(void)
{
	return 10U;
}

static void gk20a_sema_add_header(struct gk20a *g,
		struct priv_cmd_entry *cmd, u64 sema_va)
{
	u32 data[] = {
		/* semaphore_a */
		0x20010004U,
		/* offset_upper */
		(u32)(sema_va >> 32) & 0xffU,
		/* semaphore_b */
		0x20010005U,
		/* offset */
		(u32)sema_va & 0xffffffff,
	};

	nvgpu_priv_cmdbuf_append(g, cmd, data, ARRAY_SIZE(data));
}

void gk20a_sema_add_wait_cmd(struct gk20a *g,
		struct priv_cmd_entry *cmd,
		struct nvgpu_semaphore *s, u64 sema_va)
{
	u32 data[] = {
		/* semaphore_c */
		0x20010006U,
		/* payload */
		nvgpu_semaphore_get_value(s),
		/* semaphore_d */
		0x20010007U,
		/* operation: acq_geq, switch_en */
		0x4U | BIT32(12),
	};

	nvgpu_log_fn(g, " ");

	gk20a_sema_add_header(g, cmd, sema_va);
	nvgpu_priv_cmdbuf_append(g, cmd, data, ARRAY_SIZE(data));
}

void gk20a_sema_add_incr_cmd(struct gk20a *g,
		struct priv_cmd_entry *cmd,
		struct nvgpu_semaphore *s, u64 sema_va,
		bool wfi)
{
	u32 data[] = {
		/* semaphore_c */
		0x20010006U,
		/* payload */
		nvgpu_semaphore_get_value(s),
		/* semaphore_d */
		0x20010007U,
		/* operation: release, wfi */
		0x2UL | ((wfi ? 0x0UL : 0x1UL) << 20),
		/* non_stall_int */
		0x20010008U,
		/* ignored */
		0U,
	};

	nvgpu_log_fn(g, " ");

	gk20a_sema_add_header(g, cmd, sema_va);
	nvgpu_priv_cmdbuf_append(g, cmd, data, ARRAY_SIZE(data));
}
