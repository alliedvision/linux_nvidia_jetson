/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_WORKQUEUE_H
#define DCE_WORKQUEUE_H

#include <linux/workqueue.h>

struct dce_work_struct {
	struct tegra_dce *d;
	struct work_struct work;
	void (*dce_work_fn)(struct tegra_dce *d);
};

int dce_init_work(struct tegra_dce *d,
		   struct dce_work_struct *work,
		   void (*work_fn)(struct tegra_dce *d));
void dce_schedule_work(struct dce_work_struct *work);

#endif
