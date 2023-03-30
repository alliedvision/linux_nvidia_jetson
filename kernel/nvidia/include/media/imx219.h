/**
 * Copyright (c) 2014-2022, NVIDIA Corporation.  All rights reserved.
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

#ifndef __IMX219_H__
#define __IMX219_H__

#include <uapi/media/imx219.h>

#define IMX219_FUSE_ID_SIZE		6
#define IMX219_FUSE_ID_STR_SIZE		(IMX219_FUSE_ID_SIZE * 2)

struct imx219_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
	struct regulator *vdd_af;
};

struct imx219_platform_data {
	struct imx219_flash_control flash_cap;
	const char *mclk_name; /* NULL for default default_mclk */
	int (*power_on)(struct imx219_power_rail *pw);
	int (*power_off)(struct imx219_power_rail *pw);
};

#endif  /* __IMX219_H__ */
