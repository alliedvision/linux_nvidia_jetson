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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA_CDI_MGR_H__
#define __TEGRA_CDI_MGR_H__

#include <uapi/media/cdi-mgr.h>

#define MAX_CDI_GPIOS	8

struct cdi_mgr_client {
	struct mutex mutex;
	struct list_head list;
	struct i2c_client *client;
	struct cdi_mgr_new_dev cfg;
	struct cdi_dev_platform_data pdata;
	int id;
};

struct cdi_mgr_platform_data {
	int bus;
	int num_pwr_gpios;
	u32 pwr_gpios[MAX_CDI_GPIOS];
	u32 pwr_flags[MAX_CDI_GPIOS];
	int num_pwr_map;
	u32 pwr_mapping[MAX_CDI_GPIOS];
	int num_mcdi_gpios;
	u32 mcdi_gpios[MAX_CDI_GPIOS];
	u32 mcdi_flags[MAX_CDI_GPIOS];
	int csi_port;
	bool default_pwr_on;
	bool runtime_pwrctrl_off;
	char *drv_name;
	u8 ext_pwr_ctrl; /* bit 0 - des, bit 1 - sensor */
	bool max20087_pwrctl;
};

int cdi_delete_lst(struct device *dev, struct i2c_client *client);

#endif  /* __TEGRA_CDI_MGR_H__ */
