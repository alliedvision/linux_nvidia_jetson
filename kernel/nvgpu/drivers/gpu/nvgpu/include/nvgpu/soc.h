/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_SOC_H
#define NVGPU_SOC_H

#include <nvgpu/types.h>

struct gk20a;

#ifdef CONFIG_NVGPU_TEGRA_FUSE
/**
 * @brief Check whether running on silicon or not.
 *
 * @param g [in]	GPU super structure.
 *
 * - Extract out platform info by calling NvTegraSysGetPlatform().
 * - If info is NULL return false.
 * - If info is not "silicon" then return false. Otherwise return true.
 *
 * @return Returns true if it's silicon else return false.
 */
bool nvgpu_platform_is_silicon(struct gk20a *g);

/**
 * @brief Check whether running simulation or not.
 *
 * @param g [in]	GPU super structure.
 *
 * - Read CPU type by calling NvTegraSysGetCpuType().
 * - Read platform info by calling NvTegraSysGetPlatform().
 * - If any of above two or both are NULL return false.
 * - True if any/all of the below condition is true.
 *   - CPU type is "vdk" or
 *   - CPU type is "dsim" or
 *   - CPU type is "asim" or
 *   - platform info is "quickturn".
 * @return Returns true if it's simulation else returns false.
 */
bool nvgpu_platform_is_simulation(struct gk20a *g);

/**
 * @brief Check whether running fpga or not.
 *
 * @param g [in]	GPU super structure.
 *
 * - Get tegra Platform info using NvTegraSysGetPlatform().
 * - Return false if return info is NULL.
 * - Return true if any/all of the below condition is true.
 *   - Info is  "system_fpga" or
 *   - Info is  "unit_fpga".
 * @return Returns true if it's fpga else returns false.
 */
bool nvgpu_platform_is_fpga(struct gk20a *g);

/**
 * @brief Check whether running in virtualized environment.
 *
 * @param g [in]	GPU super structure.
 *
 * - Return true if NvHvCheckOsNative() is successful.
 *
 * @return Returns true if it's virtualized environment else returns false.
 */
bool nvgpu_is_hypervisor_mode(struct gk20a *g);

/**
 * @brief Check whether soc is t194 and revision a01.
 *
 * @param g [in]	GPU super structure.
 *
 * - Return true only if NvTegraSysGetChipId() is equal to TEGRA_CHIPID_TEGRA19
 *   and NvTegraSysGetChipRevision() is equal to TEGRA_REVISION_A01.
 * @return Returns true if soc is t194-a01 else returns false.
 */
bool nvgpu_is_soc_t194_a01(struct gk20a *g);

/**
 * @brief Do soc related init
 *
 * @param g [in]	GPU super structure.
 *
 * - Set VMID_UNINITIALIZED to r->gid.
 * - Check if nvgpu_is_hypervisor_mode is enabled if yes then
 *   - If module is not virtual then set nvgpu_hv_ipa_pa to desc->phys_addr.
 *
 * @return 0 in case of success, <0 in case of call to NvHvGetOsVmId() gets fail.
 * @retval ENODEV Get VM id fails to open device node.
 * @retval EINVAL Invalid argument
 * @retval EFAULT devctl call to nvhv device node returned failure
 */
int nvgpu_init_soc_vars(struct gk20a *g);

#else /* CONFIG_NVGPU_TEGRA_FUSE */

static inline bool nvgpu_platform_is_silicon(struct gk20a *g)
{
	return true;
}

static inline bool nvgpu_platform_is_simulation(struct gk20a *g)
{
	return false;
}

static inline bool nvgpu_platform_is_fpga(struct gk20a *g)
{
	return false;
}

static inline bool nvgpu_is_hypervisor_mode(struct gk20a *g)
{
	return false;
}

static inline bool nvgpu_is_soc_t194_a01(struct gk20a *g)
{
	return false;
}

static inline int nvgpu_init_soc_vars(struct gk20a *g)
{
	return 0;
}
#endif /* CONFIG_NVGPU_TEGRA_FUSE */

/**
 * @brief Get the physical address from the given intermediate physical address.
 *
 * @param[in]  g	Pointer to GPU structure.
 * @param[in]  ipa	Intermediate physical address.
 *
 * @return translated physical address.
 */
u64 nvgpu_get_pa_from_ipa(struct gk20a *g, u64 ipa);

#endif /* NVGPU_SOC_H */
