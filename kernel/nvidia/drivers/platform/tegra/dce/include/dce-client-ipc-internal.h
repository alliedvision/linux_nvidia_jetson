/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_CLIENT_IPC_INTERNAL_H
#define DCE_CLIENT_IPC_INTERNAL_H

#include <linux/platform/tegra/dce/dce-client-ipc.h>

/**
 * struct tegra_dce_client_ipc - Data Structure to hold client specific ipc
 *				data pertaining to IPC type
 *
 * @valid : Tells if the client ipc data held by data structure is valid
 * @data : Pointer to any specific data passed by client during registration
 *         for corresponding IPC type
 * @type : Corresponding IPC type as defined in CPU driver
 * @handle : Corresponding handle allocated for client during registration
 * @int_type : IPC interface type for above IPC type as defined in CPU driver
 * @d : pointer to OS agnostic dce struct. Stores all runtime info for dce
 *      cluster elements
 * @recv_wait : condition variable used for IPC synchronization
 * @complete : atomic variable used for IPC synchronization
 * @callback_fn : function pointer to the callback function passed by the
 *                client during registration
 */
struct tegra_dce_client_ipc {
	bool valid;
	void *data;
	uint32_t type;
	uint32_t handle;
	uint32_t int_type;
	struct tegra_dce *d;
	struct dce_cond recv_wait;
	atomic_t complete;
	tegra_dce_client_ipc_callback_t callback_fn;
};

#define DCE_MAX_ASYNC_WORK	8
struct dce_async_work {
	struct tegra_dce *d;
	struct work_struct async_event_work;
	atomic_t in_use;
};

/**
 * @async_event_wq - Workqueue to process async events from DCE
 */
struct tegra_dce_async_ipc_info {
	struct workqueue_struct *async_event_wq;
	struct dce_async_work work[DCE_MAX_ASYNC_WORK];
};

void dce_client_ipc_wakeup(struct tegra_dce *d,	u32 ch_type);

int dce_client_ipc_wait(struct tegra_dce *d, u32 ch_type);

int dce_client_init(struct tegra_dce *d);

void dce_client_deinit(struct tegra_dce *d);

#endif
