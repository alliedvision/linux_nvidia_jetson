/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <soc/tegra/chip-id.h>
#endif
#include <soc/tegra/fuse.h>
#ifdef CONFIG_TEGRA_HV_MANAGER
#include <soc/tegra/virt/syscalls.h>
#include <nvgpu/ipa_pa_cache.h>
#endif

#include <nvgpu/soc.h>
#include "os_linux.h"
#include "platform_gk20a.h"

bool nvgpu_platform_is_silicon(struct gk20a *g)
{
	return tegra_platform_is_silicon();
}

bool nvgpu_platform_is_simulation(struct gk20a *g)
{
	return tegra_platform_is_vdk();
}

bool nvgpu_platform_is_fpga(struct gk20a *g)
{
	return tegra_platform_is_fpga();
}

bool nvgpu_is_hypervisor_mode(struct gk20a *g)
{
	return is_tegra_hypervisor_mode();
}

bool nvgpu_is_soc_t194_a01(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	return ((platform->platform_chip_id == TEGRA_194 &&
			tegra_chip_get_revision() == TEGRA_REVISION_A01) ?
		true : false);
}

#ifdef CONFIG_TEGRA_HV_MANAGER
/* When nvlink is enabled on dGPU, we need to use physical memory addresses.
 * There is no SMMU translation. However, the device initially enumerates as a
 * PCIe device. As such, when allocation memory for this PCIe device, the DMA
 * framework ends up allocating memory using SMMU (if enabled in device tree).
 * As a result, when we switch to nvlink, we need to use underlying physical
 * addresses, even if memory mappings exist in SMMU.
 * In addition, when stage-2 SMMU translation is enabled (for instance when HV
 * is enabled), the addresses we get from dma_alloc are IPAs. We need to
 * convert them to PA.
 */
static u64 nvgpu_tegra_hv_ipa_pa(struct gk20a *g, u64 ipa, u64 *pa_len)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct hyp_ipa_pa_info info;
	struct nvgpu_hyp_ipa_pa_info nvgpu_ipapainfo;
	int err;
	u64 pa = 0ULL;

	pa = nvgpu_ipa_to_pa_cache_lookup_locked(g, ipa, pa_len);
	if (pa != 0UL) {
		return pa;
	}

	err = hyp_read_ipa_pa_info(&info, platform->vmid, ipa);
	if (err < 0) {
		nvgpu_err(g, "ipa=%llx translation failed vmid=%u err=%d",
			ipa, platform->vmid, err);
	} else {
		pa = info.base + info.offset;
		if (pa_len != NULL) {
			/*
			 * Update the size of physical memory chunk after the
			 * specified offset.
			 */
			*pa_len = info.size - info.offset;
		}
		nvgpu_log(g, gpu_dbg_map_v,
				"ipa=%llx vmid=%d -> pa=%llx "
				"base=%llx offset=%llx size=%llx\n",
				ipa, platform->vmid, pa, info.base,
				info.offset, info.size);
	}

	if (pa != 0U) {
		nvgpu_ipapainfo.base = info.base;
		nvgpu_ipapainfo.offset = info.offset;
		nvgpu_ipapainfo.size = info.size;
		nvgpu_ipa_to_pa_add_to_cache(g, ipa, pa,
				&nvgpu_ipapainfo);
	}

	return pa;
}
#endif

int nvgpu_init_soc_vars(struct gk20a *g)
{
#ifdef CONFIG_TEGRA_HV_MANAGER
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	int err;

	if (nvgpu_is_hypervisor_mode(g)) {
		err = hyp_read_gid(&platform->vmid);
		if (err) {
			nvgpu_err(g, "failed to read vmid");
			return err;
		}
		platform->phys_addr = nvgpu_tegra_hv_ipa_pa;
	}
#endif
	return 0;
}

u64 nvgpu_get_pa_from_ipa(struct gk20a *g, u64 ipa)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	u64 pa_len = 0U;

	if (platform->phys_addr) {
		return platform->phys_addr(g, ipa, &pa_len);
	}
	return ipa;
}
