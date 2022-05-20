/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __CDI_DEV_H__
#define __CDI_DEV_H__

#include <uapi/media/cdi-dev.h>
#include <linux/regmap.h>

#define MAX_CDI_NAME_LENGTH	32

struct cdi_dev_platform_data {
	struct device *pdev; /* parent device of cdi_dev */
	struct device_node *np;
	int reg_bits;
	int val_bits;
	char drv_name[MAX_CDI_NAME_LENGTH];
};

#endif  /* __CDI_DEV_H__ */
