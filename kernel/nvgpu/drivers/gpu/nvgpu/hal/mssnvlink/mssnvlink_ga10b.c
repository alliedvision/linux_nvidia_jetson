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

#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/soc.h>
#include <nvgpu/kmem.h>
#include <nvgpu/string.h>

#include "mssnvlink_ga10b.h"

#define MSS_NVLINK_INTERNAL_NUM                 8U
#define MSS_NVLINK_GLOBAL_CREDIT_CONTROL_0      0x00000010
#define MSS_NVLINK_MCF_MEMORY_TYPE_CONTROL_0    0x00000040

#define MSS_NVLINK_SIZE                         0x00001000
#define MSS_NVLINK_1_BASE                       0x01f20000
#define MSS_NVLINK_2_BASE                       0x01f40000
#define MSS_NVLINK_3_BASE                       0x01f60000
#define MSS_NVLINK_4_BASE                       0x01f80000
#define MSS_NVLINK_5_BASE                       0x01fa0000
#define MSS_NVLINK_6_BASE                       0x01fc0000
#define MSS_NVLINK_7_BASE                       0x01fe0000
#define MSS_NVLINK_8_BASE                       0x01e00000
#define MSS_NVLINK_INIT_CREDITS                 0x00000001U
#define MSS_NVLINK_FORCE_COH_SNP		0x3U

u32 ga10b_mssnvlink_get_links(struct gk20a *g, u32 **links)
{
	u32 nvlink_base[MSS_NVLINK_INTERNAL_NUM] = {
		MSS_NVLINK_1_BASE, MSS_NVLINK_2_BASE, MSS_NVLINK_3_BASE,
		MSS_NVLINK_4_BASE, MSS_NVLINK_5_BASE, MSS_NVLINK_6_BASE,
		MSS_NVLINK_7_BASE, MSS_NVLINK_8_BASE
	};

	*links = nvgpu_kzalloc(g, sizeof(nvlink_base));
	if (*links == NULL) {
		return 0;
	}
	nvgpu_memcpy((u8 *)*links, (u8 *)nvlink_base, sizeof(nvlink_base));

	return MSS_NVLINK_INTERNAL_NUM;
}

void ga10b_mssnvlink_init_soc_credits(struct gk20a *g)
{
	u32 i = 0U;
	u32 val = MSS_NVLINK_INIT_CREDITS;
	u32 *nvlink_base;
	u32 num_links;

	uintptr_t mssnvlink_control[MSS_NVLINK_INTERNAL_NUM];

	if (nvgpu_platform_is_simulation(g)) {
		nvgpu_log(g, gpu_dbg_info, "simulation platform: "
			"nvlink soc credits not required");
		return;
	}

	if (nvgpu_platform_is_silicon(g)) {
		nvgpu_log(g, gpu_dbg_info,
			"nvlink soc credits init done by bpmp on silicon");
		return;
	}

	num_links = g->ops.mssnvlink.get_links(g, &nvlink_base);
	if (num_links == 0) {
		nvgpu_err(g, "num_links = %d, skipping", num_links);
		return;
	}

	for (i = 0U; i < num_links; i++) {
		mssnvlink_control[i] = nvgpu_io_map(g, nvlink_base[i],
				MSS_NVLINK_SIZE);
	}

	/* init nvlink soc credits */
	nvgpu_log(g, gpu_dbg_info, "init nvlink soc credits");

	for (i = 0U; i < num_links; i++) {
		nvgpu_os_writel(val, (*(mssnvlink_control + i) +
				 MSS_NVLINK_GLOBAL_CREDIT_CONTROL_0));
	}

	/*
	 * Set force snoop, always snoop all nvlink memory transactions
	 * (both coherent and non-coherent)
	 */
	nvgpu_log(g, gpu_dbg_info, "set force snoop");

	for (i = 0U; i < num_links; i++) {
		val = nvgpu_os_readl((*(mssnvlink_control + i) +
			MSS_NVLINK_MCF_MEMORY_TYPE_CONTROL_0));
		val &= ~(MSS_NVLINK_FORCE_COH_SNP);
		val |= MSS_NVLINK_FORCE_COH_SNP;
		nvgpu_os_writel(val, *(mssnvlink_control + i) +
			MSS_NVLINK_MCF_MEMORY_TYPE_CONTROL_0);
	}

	for (i = 0U; i < num_links; i++) {
		nvgpu_io_unmap(g, mssnvlink_control[i], MSS_NVLINK_SIZE);
	}
	nvgpu_kfree(g, nvlink_base);
}
