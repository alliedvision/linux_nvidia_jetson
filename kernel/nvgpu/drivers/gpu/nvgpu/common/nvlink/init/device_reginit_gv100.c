/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/nvlink.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include "device_reginit_gv100.h"

#ifdef CONFIG_NVGPU_NVLINK
struct nvlink_reginit {
	u32 addr;
	u32 value;
};

static const struct nvlink_reginit  nvlink_reginit_per_link_tegra[] = {
	/* NVTLC when connected to Tegra */
	{ 0x300U, 0x00800040U },
	{ 0x304U, 0x00000000U },
	{ 0x308U, 0x00000000U },
	{ 0x30CU, 0x00000000U },
	{ 0x310U, 0x00000000U },
	{ 0x314U, 0x00800040U },
	{ 0x318U, 0x00000000U },
	{ 0x31CU, 0x00000000U },
	{ 0x200U, 0x007F003FU },
	{ 0x204U, 0x007F003FU },
	{ 0x208U, 0x007F003FU },
	{ 0x20CU, 0x007F003FU },
	{ 0x210U, 0x007F003FU },
	{ 0x214U, 0x00FF007FU },
	{ 0x218U, 0x00FF007FU },
	{ 0x21CU, 0x00FF007FU },
	{ 0xB00U, 0x010000C0U },
	{ 0xB04U, 0x00000000U },
	{ 0xB08U, 0x00000000U },
	{ 0xB0CU, 0x00000000U },
	{ 0xB10U, 0x00000000U },
	{ 0xB14U, 0x010000C0U },
	{ 0xB18U, 0x00000000U },
	{ 0xB1CU, 0x00000000U },
	{ 0xA00U, 0x00FF00BFU },
	{ 0xA04U, 0x00FF00BFU },
	{ 0xA08U, 0x00FF00BFU },
	{ 0xA0CU, 0x00FF00BFU },
	{ 0xA10U, 0x00FF00BFU },
	{ 0xA14U, 0x01FF017FU },
	{ 0xA18U, 0x01FF017FU },
	{ 0xA1CU, 0x01FF017FU },
	{ 0x400U, 0x00000001U },
	{ 0xC00U, 0x00000001U },
};

static const struct nvlink_reginit nvlink_reginit_per_link_gpu[] = {
	/* NVTLC for PEER GPU */
	{ 0x300U, 0x00800040U },
	{ 0x304U, 0x00000000U },
	{ 0x308U, 0x00000000U },
	{ 0x30CU, 0x00000000U },
	{ 0x310U, 0x00000000U },
	{ 0x314U, 0x00800040U },
	{ 0x318U, 0x00000000U },
	{ 0x31CU, 0x00000000U },
	{ 0x200U, 0x007F003FU },
	{ 0x204U, 0x007F003FU },
	{ 0x208U, 0x007F003FU },
	{ 0x20CU, 0x007F003FU },
	{ 0x210U, 0x007F003FU },
	{ 0x214U, 0x00FF007FU },
	{ 0x218U, 0x00FF007FU },
	{ 0x21CU, 0x00FF007FU },
	{ 0xB00U, 0x010000C0U },
	{ 0xB04U, 0x00000000U },
	{ 0xB08U, 0x00000000U },
	{ 0xB0CU, 0x00000000U },
	{ 0xB10U, 0x00000000U },
	{ 0xB14U, 0x010000C0U },
	{ 0xB18U, 0x00000000U },
	{ 0xB1CU, 0x00000000U },
	{ 0xA00U, 0x00FF00BFU },
	{ 0xA04U, 0x00FF00BFU },
	{ 0xA08U, 0x00FF00BFU },
	{ 0xA0CU, 0x00FF00BFU },
	{ 0xA10U, 0x00FF00BFU },
	{ 0xA14U, 0x01FF017FU },
	{ 0xA18U, 0x01FF017FU },
	{ 0xA1CU, 0x01FF017FU },
	{ 0xF04U, 0x00FFFFFFU },
	{ 0xF0CU, 0x00FFFFFFU },
	{ 0xF1CU, 0x003FFFFFU },
	{ 0xF24U, 0x003FFFFFU },
	{ 0x704U, 0x003FFFFFU },
	{ 0x70CU, 0x003FFFFFU },
	{ 0x400U, 0x00000001U },
	{ 0xC00U, 0x00000001U },
};

static int gv100_nvlink_get_tlc_reginit(enum nvgpu_nvlink_endp endp,
		const struct nvlink_reginit **reg, u32 *count)
{
	int ret = 0;

	switch(endp) {
	case nvgpu_nvlink_endp_tegra:
		*reg = (const struct nvlink_reginit *)
			nvlink_reginit_per_link_tegra;
		*count = (u32)ARRAY_SIZE(nvlink_reginit_per_link_tegra);
		break;
	case nvgpu_nvlink_endp_gpu:
		*reg = (const struct nvlink_reginit *)
			nvlink_reginit_per_link_gpu;
		*count = (u32)ARRAY_SIZE(nvlink_reginit_per_link_gpu);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int gv100_nvlink_reg_init(struct gk20a *g)
{
	u32 i = 0;
	u32 count = 0;
	const struct nvlink_reginit *reg;
	enum nvgpu_nvlink_endp endp;
	int err;
	u32 link_id;
	unsigned long mask = g->nvlink.enabled_links;
	struct nvgpu_nvlink_link *link;
	unsigned long bit;

	/* Apply automated reg init flow for PROD settings */
	for_each_set_bit(bit, &mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		link = &g->nvlink.links[link_id];
		if (!link->remote_info.is_connected) {
			continue;
		}

		endp = link->remote_info.device_type;
		err = gv100_nvlink_get_tlc_reginit(endp, &reg, &count);
		if (err != 0) {
			nvgpu_err(g, "no reginit for endp=%u", endp);
			continue;
		}

		for (i = 0; i < count; i++) {
			TLC_REG_WR32(g, link_id, reg->addr, reg->value);
			reg++;
		}
	}
	return 0;
}
#endif /* CONFIG_NVGPU_NVLINK */
