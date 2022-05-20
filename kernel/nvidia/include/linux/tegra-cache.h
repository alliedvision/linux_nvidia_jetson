/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LINUX_TEGRA_CACHE_H
#define _LINUX_TEGRA_CACHE_H

/* Tegra cache functions */
int tegra_flush_cache_all(void);
int tegra_flush_dcache_all(void *__maybe_unused unused);
int tegra_clean_dcache_all(void *__maybe_unused unused);

struct tegra_cache_ops {
	int (*flush_cache_all)(void);
	int (*flush_dcache_all)(void *__maybe_unused unused);
	int (*clean_dcache_all)(void *__maybe_unused unused);
};

void tegra_cache_set_ops(struct tegra_cache_ops *);
#endif /* _LINUX_TEGRA_CACHE_H */
