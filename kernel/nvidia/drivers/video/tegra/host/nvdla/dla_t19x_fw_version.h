/*
 * NVDLA OS Interface
 *
 * Copyright (c) 2016-2020, NVIDIA Corporation.  All rights reserved.
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
 */

#ifndef DLA_T19X_FW_VERSION_H
#define DLA_T19X_FW_VERSION_H

#define FIRMWARE_T19X_VERSION_MAJOR		0x1UL
#define FIRMWARE_T19X_VERSION_MINOR		0x2UL
#define FIRMWARE_T19X_VERSION_SUBMINOR		0x0UL

static inline uint32_t dla_t19x_fw_version(void)
{
	return (((FIRMWARE_T19X_VERSION_MAJOR & 0xffU) << 16) |
			((FIRMWARE_T19X_VERSION_MINOR & 0xffU) << 8) |
			((FIRMWARE_T19X_VERSION_SUBMINOR & 0xffU)));
}

#endif /* End of DLA_T19X_FW_VERSION_H */
