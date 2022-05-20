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

#ifndef __TEGRA19X_CACHE_H
#define __TEGRA19X_CACHE_H

/* Tegra19x cache functions */
int tegra19x_flush_cache_all(void);
int tegra19x_flush_dcache_all(void);
int tegra19x_clean_dcache_all(void);

#endif /* __TEGRA19x_CACHE_H */
