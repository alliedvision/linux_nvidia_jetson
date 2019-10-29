/*
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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

#ifndef _NVGPU_PLATFORM_SYSFS_H_
#define _NVGPU_PLATFORM_SYSFS_H_

#include "gp10b/gr_gp10b.h"

#define ECC_STAT_NAME_MAX_SIZE	100

int nvgpu_gr_ecc_stat_create(struct device *dev,
			     int is_l2, char *ecc_stat_name,
			     struct gk20a_ecc_stat *ecc_stat);
int nvgpu_ecc_stat_create(struct device *dev,
			  int num_hw_units, int num_subunits,
			  char *ecc_unit_name, char *ecc_subunit_name,
			  char *ecc_stat_name,
			  struct gk20a_ecc_stat *ecc_stat);
void nvgpu_gr_ecc_stat_remove(struct device *dev,
			      int is_l2, struct gk20a_ecc_stat *ecc_stat);
void nvgpu_ecc_stat_remove(struct device *dev,
			   int num_hw_units, int num_subunits,
			   struct gk20a_ecc_stat *ecc_stat);
#endif
