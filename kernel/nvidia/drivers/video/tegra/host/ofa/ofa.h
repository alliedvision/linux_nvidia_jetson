/*
 * drivers/video/tegra/host/ofa/ofa.h
 *
 * Tegra OFA Module Support
 *
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_OFA_H__
#define __NVHOST_OFA_H__

#include <linux/types.h>
#include <linux/platform_device.h>

static inline u32 ofa_safety_ram_init_req_r(void)
{
	/* NV_POFA_SAFETY_RAM_INIT_REQ */
	return 0x00003320;
}

static inline u32 ofa_safety_ram_init_done_r(void)
{
	/* NV_POFA_SAFETY_RAM_INIT_DONE */
	return 0x00003324;
}

int ofa_safety_ram_init(struct platform_device *pdev);

#endif
