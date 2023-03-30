/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, NVIDIA Corporation.
 */

#ifndef DRM_TEGRA_UTIL_H
#define DRM_TEGRA_UTIL_H

void tegra_drm_program_iommu_regs(struct device *dev, void __iomem *regs, u32 transcfg_offset);

#endif
