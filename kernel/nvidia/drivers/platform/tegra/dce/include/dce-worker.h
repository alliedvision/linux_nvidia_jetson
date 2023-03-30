/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
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

#ifndef DCE_WORKER_H
#define DCE_WORKER_H

#include <dce-cond.h>
#include <dce-lock.h>
#include <dce-thread.h>

struct tegra_dce;

#define DCE_WAIT_BOOT_COMPLETE		0
#define DCE_WAIT_MBOX_IPC		1
#define DCE_WAIT_ADMIN_IPC		2
#define DCE_WAIT_SC7_ENTER		3
#define DCE_WAIT_LOG			4
#define DCE_MAX_WAIT			5

struct dce_wait_cond {
	atomic_t complete;
	struct dce_cond cond_wait;
};

int dce_work_cond_sw_resource_init(struct tegra_dce *d);
void dce_work_cond_sw_resource_deinit(struct tegra_dce *d);
void dce_schedule_boot_complete_wait_worker(struct tegra_dce *d);
int dce_wait_interruptible(struct tegra_dce *d, u32 msg_id);
void dce_wakeup_interruptible(struct tegra_dce *d, u32 msg_id);

#endif
