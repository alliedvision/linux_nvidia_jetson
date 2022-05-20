/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef __SOC_TEGRA_AHB_H__
#define __SOC_TEGRA_AHB_H__

extern int tegra_ahb_enable_smmu(struct device_node *ahb);

int tegra_ahb_get_master_id(struct device *dev);

bool tegra_ahb_is_mem_wrque_busy(u32 mst_id);

#endif /* __SOC_TEGRA_AHB_H__ */
