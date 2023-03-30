/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/module.h>

#include <asm/smp_plat.h>
#include <linux/version.h>
#include <linux/tegra-cache.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <asm/cacheflush.h>

static struct tegra_cache_ops *cache_ops;

void tegra_cache_set_ops(struct tegra_cache_ops *tegra_cache_plat_ops)
{
	cache_ops = tegra_cache_plat_ops;
}

/*
 * Tegra cache functions
 *
 * Return 0 on success.
 * 	  Error code or -ENOTSUPP on failure.
 *
 */
int tegra_flush_cache_all(void)
{
	int ret = -ENOTSUPP;

	if (cache_ops && cache_ops->flush_cache_all)
		ret = cache_ops->flush_cache_all();

	return ret;
}
EXPORT_SYMBOL(tegra_flush_cache_all);

int tegra_flush_dcache_all(void *__maybe_unused unused)
{
	int ret = -ENOTSUPP;

	if (cache_ops && cache_ops->flush_dcache_all)
		ret = cache_ops->flush_dcache_all(unused);

	return ret;
}
EXPORT_SYMBOL(tegra_flush_dcache_all);

int tegra_clean_dcache_all(void *__maybe_unused unused)
{
	int ret = -ENOTSUPP;

	if (cache_ops && cache_ops->clean_dcache_all)
		ret = cache_ops->clean_dcache_all(unused);

	return ret;
}
EXPORT_SYMBOL(tegra_clean_dcache_all);

