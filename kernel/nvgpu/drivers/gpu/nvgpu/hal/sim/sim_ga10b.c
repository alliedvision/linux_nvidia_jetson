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
#include <nvgpu/log.h>
#include <nvgpu/bitops.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/dma.h>
#include <nvgpu/io.h>
#include <nvgpu/hw_sim.h>
#include <nvgpu/sim.h>
#include <nvgpu/utils.h>
#include <nvgpu/bug.h>
#include <nvgpu/string.h>

#include "sim_ga10b.h"

#ifdef CONFIG_NVGPU_SIM
static void nvgpu_sim_esc_readl_ga10b(struct gk20a *g,
		const char *path, u32 index, u32 *data)
{
	int err;
	u32 data_offset;

	sim_write_hdr(g, sim_msg_function_sim_escape_read_v(),
		      sim_escape_read_hdr_size());
	*sim_msg_param(g, 0) = index;
	*sim_msg_param(g, 4) = sizeof(u32);
	data_offset = (u32)round_up(
		nvgpu_safe_add_u64(strlen(path), 1ULL), sizeof(u32));
	*sim_msg_param(g, 8) = data_offset;
	strcpy((char *)sim_msg_param(g, sim_escape_read_hdr_size()), path);

	err = issue_rpc_and_wait(g);

	if (err == 0) {
		nvgpu_memcpy((u8 *)data, (u8 *)sim_msg_param(g,
			nvgpu_safe_add_u32(data_offset,
				sim_escape_read_hdr_size())),
			sizeof(u32));
	} else {
		*data = 0xffffffff;
		WARN(1, "issue_rpc_and_wait failed err=%d", err);
	}
}

void nvgpu_init_sim_support_ga10b(struct gk20a *g)
{
	if (g->sim) {
		g->sim->esc_readl = nvgpu_sim_esc_readl_ga10b;
	}
}
#endif /* CONFIG_NVGPU_SIM */
