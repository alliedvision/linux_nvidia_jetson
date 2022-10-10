/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
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

#ifndef NVGPU_REGOPS_ALLOWLIST_H
#define NVGPU_REGOPS_ALLOWLIST_H

#include <nvgpu/types.h>

struct nvgpu_pm_resource_register_range {
	u32 start;
	u32 end;
};

enum nvgpu_pm_resource_hwpm_register_type {
	NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMON,
	NVGPU_HWPM_REGISTER_TYPE_HWPM_ROUTER,
	NVGPU_HWPM_REGISTER_TYPE_HWPM_PMA_TRIGGER,
	NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMUX,
	NVGPU_HWPM_REGISTER_TYPE_SMPC,
	NVGPU_HWPM_REGISTER_TYPE_CAU,
	NVGPU_HWPM_REGISTER_TYPE_HWPM_PMA_CHANNEL,
	NVGPU_HWPM_REGISTER_TYPE_PC_SAMPLER,
	NVGPU_HWPM_REGISTER_TYPE_TEST,
	NVGPU_HWPM_REGISTER_TYPE_COUNT,
};

struct nvgpu_pm_resource_register_range_map {
	u32 start;
	u32 end;
	enum nvgpu_pm_resource_hwpm_register_type type;
};

#endif /* NVGPU_REGOPS_ALLOWLIST_H */
