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
 *
 * This file is autogenerated.  Do not edit.
 */

#ifndef NVGPU_GA10B_REGOPS_ALLOWLIST_H
#define NVGPU_GA10B_REGOPS_ALLOWLIST_H

#include <nvgpu/types.h>

struct nvgpu_pm_resource_register_range;

u32 ga10b_get_hwpm_perfmon_register_stride(void);
u32 ga10b_get_hwpm_router_register_stride(void);
u32 ga10b_get_hwpm_pma_channel_register_stride(void);
u32 ga10b_get_hwpm_pma_trigger_register_stride(void);
u32 ga10b_get_smpc_register_stride(void);
u32 ga10b_get_cau_register_stride(void);

const u32 *ga10b_get_hwpm_perfmon_register_offset_allowlist(u32 *count);
const u32 *ga10b_get_hwpm_router_register_offset_allowlist(u32 *count);
const u32 *ga10b_get_hwpm_pma_channel_register_offset_allowlist(u32 *count);
const u32 *ga10b_get_hwpm_pma_trigger_register_offset_allowlist(u32 *count);
const u32 *ga10b_get_smpc_register_offset_allowlist(u32 *count);
const u32 *ga10b_get_cau_register_offset_allowlist(u32 *count);

const struct nvgpu_pm_resource_register_range
	*ga10b_get_hwpm_perfmon_register_ranges(u32 *count);
const struct nvgpu_pm_resource_register_range
	*ga10b_get_hwpm_router_register_ranges(u32 *count);
const struct nvgpu_pm_resource_register_range
	*ga10b_get_hwpm_pma_channel_register_ranges(u32 *count);
const struct nvgpu_pm_resource_register_range
	*ga10b_get_hwpm_pma_trigger_register_ranges(u32 *count);
const struct nvgpu_pm_resource_register_range
	*ga10b_get_smpc_register_ranges(u32 *count);
const struct nvgpu_pm_resource_register_range
	*ga10b_get_hwpm_perfmux_register_ranges(u32 *count);
const struct nvgpu_pm_resource_register_range
	*ga10b_get_cau_register_ranges(u32 *count);

#endif /* NVGPU_GA10B_REGOPS_ALLOWLIST_H */
