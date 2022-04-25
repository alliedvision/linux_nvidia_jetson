/*
 *
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_OS_ECC_LINUX_H
#define NVGPU_OS_ECC_LINUX_H

#ifdef CONFIG_NVGPU_SUPPORT_LINUX_ECC_ERROR_REPORTING

#include <linux/tegra_l1ss_kernel_interface.h>
#include <linux/tegra_l1ss_ioctl.h>
#include <linux/tegra_nv_guard_service_id.h>
#include <linux/tegra_nv_guard_group_id.h>

#include <nvgpu/nvgpu_err.h>

struct nvgpu_ecc_reporting_linux {
    struct nvgpu_ecc_reporting common;
    client_param_t priv;
};

static inline struct nvgpu_ecc_reporting_linux *get_ecc_reporting_linux(
    struct nvgpu_ecc_reporting *ecc_report)
{
	return container_of(ecc_report, struct nvgpu_ecc_reporting_linux, common);
}

#endif /* CONFIG_NVGPU_SUPPORT_LINUX_ECC_ERROR_REPORTING */

#endif