/*
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/channel.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>

#include "hal/fifo/ramin_gm20b.h"

#include <nvgpu/hw/gm20b/hw_ram_gm20b.h>

void gm20b_ramin_set_big_page_size(struct gk20a *g,
		struct nvgpu_mem *mem, u32 size)
{
	u32 val;

	nvgpu_log_fn(g, " ");

	nvgpu_log_info(g, "big page size %u", size);
	val = nvgpu_mem_rd32(g, mem, ram_in_big_page_size_w());
	val &= ~ram_in_big_page_size_m();

	if (size == SZ_64K) {
		val |= ram_in_big_page_size_64kb_f();
	} else {
#ifndef CONFIG_NVGPU_HAL_NON_FUSA
		nvgpu_err(g, "only SZ_64K is allowed");
		return;
#else
		val |= ram_in_big_page_size_128kb_f();
#endif
	}

	nvgpu_mem_wr32(g, mem, ram_in_big_page_size_w(), val);
	nvgpu_log_fn(g, "done");
}
