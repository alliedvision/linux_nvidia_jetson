/*
 * include/linux/nvhost_ioctl_t194.h
 *
 * Tegra graphics host driver
 *
 * Copyright (c) 2017, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __LINUX_NVHOST_IOCTL_T194_H
#define __LINUX_NVHOST_IOCTL_T194_H

enum nvhost_module_id_t194 {
	NVHOST_MODULE_NVENC1 = 12,
	NVHOST_MODULE_NVDEC1 = 13,
};

#define NVHOST_RELOC_TYPE_NVLINK 3

#endif
