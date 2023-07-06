/*
 * Copyright (c) 2022-2023, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_FSM_H
#define DCE_FSM_H

#include <dce-cond.h>
#include <dce-lock.h>

/**
 * enum dce_fsm_event_id_type - IDs to be used to convey various
 * events throughout the life cycle of dce.
 */
enum dce_fsm_event_id_type {
	EVENT_ID_DCE_INVALID = -1,
	EVENT_ID_DCE_FSM_START = 0,
	EVENT_ID_DCE_BOOT_COMPLETE_REQUESTED,
	EVENT_ID_DCE_BOOT_COMPLETE_RECEIVED,
	EVENT_ID_DCE_MBOX_IPC_MSG_REQUESTED,
	EVENT_ID_DCE_MBOX_IPC_MSG_RECEIVED,
	EVENT_ID_DCE_ADMIN_IPC_MSG_REQUESTED,
	EVENT_ID_DCE_ADMIN_IPC_MSG_RECEIVED,
	EVENT_ID_DCE_SC7_ENTER_REQUESTED,
	EVENT_ID_DCE_SC7_ENTERED_RECEIVED,
	EVENT_ID_DCE_SC7_EXIT_RECEIVED,
	EVENT_ID_DCE_LOG_REQUESTED,
	EVENT_ID_DCE_LOG_READY_RECEIVED,
	EVENT_ID_DCE_ABORT_RECEIVED,
	EVENT_ID_DCE_CRASH_LOG_RECEIVED,
	EVENT_ID_DCE_LOG_OVERFLOW_RECEIVED,
	EVENT_ID_DCE_FSM_STOP,
};

/**
 * dce_fsm_state - Different states of dce.
 */
enum dce_fsm_state {
	STATE_DCE_INVALID = -1,
	STATE_DCE_FSM_IDLE = 0,
	STATE_DCE_BOOT_WAIT,
	STATE_DCE_MBOX_WFI,
	STATE_DCE_ADMIN_WFI,
	STATE_DCE_SC7_ENTER_WFI,
	STATE_DCE_SC7_ENTERED,
	STATE_DCE_LOG_READY_WFI,
	STATE_DCE_ABORT,
};

struct dce_fsm_info {
	struct tegra_dce *d;
	bool initialized;
	enum dce_fsm_state c_state;
	struct dce_mutex lock;
	u32 requested_ipcs;
};

/**
 * struct dce_mailbox_send_cmd_params - contains params for callback function
 * @cmd : u32 command to be send
 * @interface : interface id on which command need to be sent
 */
struct dce_mailbox_send_cmd_params {
	u32 cmd;
	u32 interface;
};

/**
 * struct dce_admin_send_msg_params - contains params for callback function
 * @msg : Pointer to dce_ipc_msg
 */
struct dce_admin_send_msg_params {
	struct dce_ipc_message *msg;
};

int dce_fsm_init(struct tegra_dce *d);
void dce_fsm_start(struct tegra_dce *d);
void dce_fsm_deinit(struct tegra_dce *d);
int dce_fsm_post_event(struct tegra_dce *d,
		       enum dce_fsm_event_id_type event,
		       void *data);

int dce_handle_fsm_start_event(struct tegra_dce *d, void *params);
int dce_handle_event_stub(struct tegra_dce *d, void *params);
#endif
