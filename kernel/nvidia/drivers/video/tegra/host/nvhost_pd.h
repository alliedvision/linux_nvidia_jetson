/*
 * drivers/video/tegra/host/nvhost_pd.h
 *
 * Tegra Graphics Host Legacy Power Domain Provider
 *
 * Copyright (c) 2019, NVIDIA Corporation.  All rights reserved.
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

#ifndef __NVHOST_PD_H
#define __NVHOST_PD_H

#include <linux/version.h>

#ifdef CONFIG_TEGRA_GRHOST_LEGACY_PD
int nvhost_domain_init(struct of_device_id *matches);
#else
static inline int nvhost_domain_init(struct of_device_id *matches)
{ return 0; }
#endif

#endif
