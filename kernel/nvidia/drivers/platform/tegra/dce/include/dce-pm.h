/*
 * Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
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

#ifndef DCE_PM_H
#define DCE_PM_H

#include <dce.h>

struct dce_sc7_state {
	uint32_t hsp_ie;
};

int dce_pm_enter_sc7(struct tegra_dce *d);
int dce_pm_exit_sc7(struct tegra_dce *d);
void dce_resume_work_fn(struct tegra_dce *d);
int dce_pm_handle_sc7_enter_requested_event(struct tegra_dce *d, void *params);
int dce_pm_handle_sc7_enter_received_event(struct tegra_dce *d, void *params);
int dce_pm_handle_sc7_exit_received_event(struct tegra_dce *d, void *params);

#endif
