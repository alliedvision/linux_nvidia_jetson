/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2011-2020, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef __TEGRA_SOCTHERM_H
#define __TEGRA_SOCTHERM_H

void tegra_soctherm_gpu_tsens_invalidate(bool control);
void tegra_soctherm_cpu_tsens_invalidate(bool control);

#endif /* __TEGRA_SOCTHERM_H */
