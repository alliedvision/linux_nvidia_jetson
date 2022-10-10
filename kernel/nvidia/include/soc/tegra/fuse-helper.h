/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 */

#include <soc/tegra/fuse.h>

#define FUSE_SKU_INFO     0x10

static inline u32 tegra_get_sku_id(void)
{
	if (!tegra_sku_info.sku_id)
		tegra_fuse_readl(FUSE_SKU_INFO, &tegra_sku_info.sku_id);

	return tegra_sku_info.sku_id;
}

#ifdef CONFIG_TEGRA_FUSE_UPSTREAM

/*
 * For upstream the following functions to determine if the
 * platform is silicon and simulator are not supported and
 * so for now, always assume that we are silicon.
 */
static inline bool tegra_platform_is_silicon(void)
{
	return true;
}

static inline bool tegra_platform_is_sim(void)
{
	return false;
}

#endif /* CONFIG_TEGRA_FUSE_UPSTREAM */
