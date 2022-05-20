/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

/**
 * enum dce_worker_event_id_type - IDs to be used to convey various
 * events thoughout the life cycle of dce worker thread.
 */
enum dce_worker_event_id_type {
	EVENT_ID_DCE_BOOT_COMPLETE_IRQ_REQ_SET = 0,
	EVENT_ID_DCE_BOOT_COMPLETE_IRQ_RECEIVED = 1,
	EVENT_ID_DCE_IPC_SYNC_TRIGGERED = 2,
	EVENT_ID_DCE_IPC_MESSAGE_SENT = 3,
	EVENT_ID_DCE_IPC_SIGNAL_RECEIVED = 4,
	EVENT_ID_DCE_THREAD_ABORT_REQ_RECEIVED = 5,
	EVENT_ID_DCE_INTERFACE_ERROR_RECEIVED = 6,
	EVENT_ID_DCE_BOOT_COMPLETE = 7,
};

/**
 * dce_worker_state - Different states of dce worker thread.
 */
enum dce_worker_state {
	STATE_DCE_WORKER_IDLE = 0,
	STATE_DCE_WORKER_BOOT_WAIT = 1,
	STATE_DCE_WORKER_WFI = 2,
	STATE_DCE_WORKER_ABORTED = 3,
	STATE_DCE_WORKER_HANDLE_DCE_ERROR = 4,
};

/**
 * struct dce_worker_info - Contains information to control and manage
 *				dce worker thread throughout its lifecycle.
 * @wrk_thread : Event driven dce thread to manage initilaization and
 *				release workflow.
 * @state_changed : Boolean to convey state changes.
 * @c_state : Stores the current state of dce worker thread. State changes
 *				are triggered by vrious events.
 * @lock : Used for exclusive state modifications from thread and
 *				interrupt context.
 * @cond : dce_cond to manage various thread states.
 */
struct dce_worker_info {
	struct dce_thread wrk_thread;
	bool state_changed;
	enum dce_worker_state c_state;
	struct dce_mutex lock;
	struct dce_cond cond;
};

void dce_worker_thread_wait(struct tegra_dce *d,
			enum dce_worker_event_id_type event);

void dce_worker_thread_wakeup(struct tegra_dce *d,
			enum dce_worker_event_id_type event);

enum dce_worker_state dce_worker_get_state(struct tegra_dce *d);

int dce_worker_thread_init(struct tegra_dce *d);

void dce_worker_thread_deinit(struct tegra_dce *d);

#endif
