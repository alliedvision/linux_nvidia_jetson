/*
 * Copyright (C) 2022, NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef DCE_PM_H
#define DCE_PM_H

#include <dce.h>

struct dce_sc7_state {
	uint32_t hsp_ie;
};

int dce_pm_handle_sc7_enter_requested_event(struct tegra_dce *d, void *params);
int dce_pm_handle_sc7_enter_received_event(struct tegra_dce *d, void *params);
int dce_pm_handle_sc7_exit_received_event(struct tegra_dce *d, void *params);

#endif
