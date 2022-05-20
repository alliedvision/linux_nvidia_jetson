/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/nvlink_bios.h>
#include <nvgpu/string.h>
#include <nvgpu/bios.h>

int nvgpu_bios_get_nvlink_config_data(struct gk20a *g)
{
	int ret = 0;
	struct nvlink_config_data_hdr_v1 config;

	if (g->bios->nvlink_config_data_offset == 0U) {
		return -EINVAL;
	}

	nvgpu_memcpy((u8 *)&config,
		&g->bios->data[g->bios->nvlink_config_data_offset],
		sizeof(config));

	if (config.version != NVLINK_CONFIG_DATA_HDR_VER_10) {
		nvgpu_err(g, "unsupported nvlink bios version: 0x%x",
				config.version);
		return -EINVAL;
	}

	switch (config.hdr_size) {
	case NVLINK_CONFIG_DATA_HDR_12_SIZE:
		g->nvlink.ac_coupling_mask = config.ac_coupling_mask;
		g->nvlink.train_at_boot = config.train_at_boot;
		g->nvlink.link_disable_mask = config.link_disable_mask;
		g->nvlink.link_mode_mask = config.link_mode_mask;
		g->nvlink.link_refclk_mask = config.link_refclk_mask;
		break;
	case NVLINK_CONFIG_DATA_HDR_11_SIZE:
		g->nvlink.train_at_boot = config.train_at_boot;
		g->nvlink.link_disable_mask = config.link_disable_mask;
		g->nvlink.link_mode_mask = config.link_mode_mask;
		g->nvlink.link_refclk_mask = config.link_refclk_mask;
		break;
	case NVLINK_CONFIG_DATA_HDR_10_SIZE:
		g->nvlink.link_disable_mask = config.link_disable_mask;
		g->nvlink.link_mode_mask = config.link_mode_mask;
		g->nvlink.link_refclk_mask = config.link_refclk_mask;
		break;
	default:
		nvgpu_err(g, "invalid nvlink bios config size");
		ret = -EINVAL;
		break;
	}

	return ret;
}
