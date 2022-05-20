/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_NVLINK_BIOS_H
#define NVGPU_NVLINK_BIOS_H

#include <nvgpu/types.h>
#include <nvgpu/utils.h>

#define NVLINK_CONFIG_DATA_HDR_VER_10           0x1U
#define NVLINK_CONFIG_DATA_HDR_10_SIZE          16U
#define NVLINK_CONFIG_DATA_HDR_11_SIZE          17U
#define NVLINK_CONFIG_DATA_HDR_12_SIZE          21U

struct nvlink_config_data_hdr_v1 {
	u8 version;
	u8 hdr_size;
	u16 rsvd0;
	u32 link_disable_mask;
	u32 link_mode_mask;
	u32 link_refclk_mask;
	u8 train_at_boot;
	u32 ac_coupling_mask;
} __attribute__((packed));

struct gk20a;

int nvgpu_bios_get_nvlink_config_data(struct gk20a *g);

#endif
