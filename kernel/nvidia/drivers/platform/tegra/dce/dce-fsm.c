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

#include <dce.h>
#include <dce-util-common.h>

struct dce_event_process_struct {
	enum dce_fsm_event_id_type event;
	int (*fsm_event_handle)(struct tegra_dce *d, void *params);
};

/*
 * Please update FSM design Document, whenever updating below event table
 */
static struct dce_event_process_struct event_process_table[] = {
	{
		.event			= EVENT_ID_DCE_FSM_START,
		.fsm_event_handle	= dce_handle_fsm_start_event,
	},
	{
		.event			= EVENT_ID_DCE_BOOT_COMPLETE_REQUESTED,
		.fsm_event_handle	= dce_handle_boot_complete_requested_event,
	},
	{
		.event			= EVENT_ID_DCE_BOOT_COMPLETE_RECEIVED,
		.fsm_event_handle	= dce_handle_boot_complete_received_event,
	},
	{
		.event			= EVENT_ID_DCE_MBOX_IPC_MSG_REQUESTED,
		.fsm_event_handle	= dce_handle_mbox_ipc_requested_event,
	},
	{
		.event			= EVENT_ID_DCE_MBOX_IPC_MSG_RECEIVED,
		.fsm_event_handle	= dce_handle_mbox_ipc_received_event,
	},
	{
		.event			= EVENT_ID_DCE_ADMIN_IPC_MSG_REQUESTED,
		.fsm_event_handle	= dce_admin_handle_ipc_requested_event,
	},
	{
		.event			= EVENT_ID_DCE_ADMIN_IPC_MSG_RECEIVED,
		.fsm_event_handle	= dce_admin_handle_ipc_received_event,
	},
	{
		.event			= EVENT_ID_DCE_SC7_ENTER_REQUESTED,
		.fsm_event_handle	= dce_pm_handle_sc7_enter_requested_event,
	},
	{
		.event			= EVENT_ID_DCE_SC7_ENTERED_RECEIVED,
		.fsm_event_handle	= dce_pm_handle_sc7_enter_received_event,
	},
	{
		.event			= EVENT_ID_DCE_SC7_EXIT_RECEIVED,
		.fsm_event_handle	= dce_pm_handle_sc7_exit_received_event,
	},
	{
		.event			= EVENT_ID_DCE_LOG_REQUESTED,
		.fsm_event_handle	= dce_handle_event_stub,
	},
	{
		.event			= EVENT_ID_DCE_LOG_READY_RECEIVED,
		.fsm_event_handle	= dce_handle_event_stub,
	},
	{
		.event			= EVENT_ID_DCE_ABORT_RECEIVED,
		.fsm_event_handle	= dce_handle_event_stub,
	},
	{
		.event			= EVENT_ID_DCE_CRASH_LOG_RECEIVED,
		.fsm_event_handle	= dce_handle_event_stub,
	},
	{
		.event			= EVENT_ID_DCE_LOG_OVERFLOW_RECEIVED,
		.fsm_event_handle	= dce_handle_event_stub,
	},
	{
		.event			= EVENT_ID_DCE_FSM_STOP,
		.fsm_event_handle	= dce_handle_event_stub,
	},
};

#define DCE_MAX_EVENTS_IDS	(sizeof(event_process_table) /		\
				 sizeof(struct dce_event_process_struct))

/**
 * dce_handle_fsm_start_event - Callback handler function for FSM_START event.
 *
 * @d : Pointer to tegra_dce struct.
 * @params : callback params
 *
 * Return : 0 if successful else error code
 */
int dce_handle_fsm_start_event(struct tegra_dce *d, void *params)
{
	return 0;
}

/**
 * dce_handle_event_stub - Stub callback handler function.
 *
 * @d : Pointer to tegra_dce struct.
 * @params : callback params
 *
 * Return : 0 if successful
 */
int dce_handle_event_stub(struct tegra_dce *d, void *params)
{
	return 0;
}

/**
 * dce_fsm_set_state - Set the FSM state based on event
 *
 * This Function is called with Mutex held.
 *
 * @d : Pointer to tegra_dce struct.
 * @event : Event for which FSM state need to be set
 *
 * Return : void
 *
 * Please update FSM design Document, whenever updating below event states
 */
static void
dce_fsm_set_state(struct tegra_dce *d,
		     enum dce_fsm_event_id_type event)
{
	struct dce_fsm_info *fsm = &d->fsm_info;

	switch (event) {
	case EVENT_ID_DCE_FSM_START:
		fsm->c_state = STATE_DCE_FSM_IDLE;
		fsm->requested_ipcs = 0;
		break;

	case EVENT_ID_DCE_BOOT_COMPLETE_REQUESTED:
		fsm->c_state = STATE_DCE_BOOT_WAIT;
		fsm->requested_ipcs |= DCE_BIT(DCE_WAIT_BOOT_COMPLETE);
		break;

	case EVENT_ID_DCE_BOOT_COMPLETE_RECEIVED:
		fsm->c_state = STATE_DCE_FSM_IDLE;
		fsm->requested_ipcs &= ~DCE_BIT(DCE_WAIT_BOOT_COMPLETE);
		break;

	case EVENT_ID_DCE_MBOX_IPC_MSG_REQUESTED:
		fsm->c_state = STATE_DCE_MBOX_WFI;
		fsm->requested_ipcs |= DCE_BIT(DCE_WAIT_MBOX_IPC);
		break;

	case EVENT_ID_DCE_MBOX_IPC_MSG_RECEIVED:
		fsm->c_state = STATE_DCE_FSM_IDLE;
		fsm->requested_ipcs &= ~DCE_BIT(DCE_WAIT_MBOX_IPC);
		break;

	case EVENT_ID_DCE_ADMIN_IPC_MSG_REQUESTED:
		fsm->c_state = STATE_DCE_ADMIN_WFI;
		fsm->requested_ipcs |= DCE_BIT(DCE_WAIT_ADMIN_IPC);
		break;

	case EVENT_ID_DCE_ADMIN_IPC_MSG_RECEIVED:
		fsm->c_state = STATE_DCE_FSM_IDLE;
		fsm->requested_ipcs &= ~DCE_BIT(DCE_WAIT_ADMIN_IPC);
		break;

	case EVENT_ID_DCE_SC7_ENTER_REQUESTED:
		fsm->c_state = STATE_DCE_SC7_ENTER_WFI;
		fsm->requested_ipcs |= DCE_BIT(DCE_WAIT_SC7_ENTER);
		break;

	case EVENT_ID_DCE_SC7_ENTERED_RECEIVED:
		fsm->c_state = STATE_DCE_SC7_ENTERED;
		fsm->requested_ipcs &= ~DCE_BIT(DCE_WAIT_SC7_ENTER);
		break;

	case EVENT_ID_DCE_SC7_EXIT_RECEIVED:
		fsm->c_state = STATE_DCE_FSM_IDLE;
		fsm->requested_ipcs = 0;
		break;

	case EVENT_ID_DCE_LOG_REQUESTED:
		fsm->c_state = STATE_DCE_LOG_READY_WFI;
		fsm->requested_ipcs |= DCE_BIT(DCE_WAIT_LOG);
		break;

	case EVENT_ID_DCE_LOG_READY_RECEIVED:
		fsm->c_state = STATE_DCE_FSM_IDLE;
		fsm->requested_ipcs &= ~DCE_BIT(DCE_WAIT_LOG);
		break;

	case EVENT_ID_DCE_FSM_STOP:
		fsm->c_state = STATE_DCE_INVALID;
		break;

	case EVENT_ID_DCE_ABORT_RECEIVED:
	case EVENT_ID_DCE_CRASH_LOG_RECEIVED:
	case EVENT_ID_DCE_LOG_OVERFLOW_RECEIVED:
		dce_debug(d, "DCE Abort received");
		fsm->c_state = STATE_DCE_ABORT;
		/*
		 * TODO: error handling
		 * wake-up any waiting process avoid kernel watchdog
		 * trigger and crash
		 */
		break;
	default:
		dce_err(d, "INVALID EVENT [%d]", event);
	}

}

/*
 * dce_fsm_validate_event - Validate event agaist current FSM state
 *
 * @d : Pointer to tegra_dce struct
 * @event : Posted FSM event
 *
 * @return : ESUCCESS if event valid else error code
 *
 * Please update FSM design Document, whenever updating below event validation
 */
static int
dce_fsm_validate_event(struct tegra_dce *d,
		       enum dce_fsm_event_id_type event)
{
	int ret = 0;
	struct dce_fsm_info *fsm = &d->fsm_info;
	enum dce_fsm_state curr_state;

	if (event > EVENT_ID_DCE_FSM_STOP) {
		dce_err(d, "Invalid event received [%d]\n", event);
		return -EINVAL;
	}

	// Should we check This??
	if (fsm->initialized == false) {
		dce_err(d, "FSM is not initialized yet\n");
		return -EINVAL;
	}

	curr_state = fsm->c_state;
	dce_debug(d, "Called for event [%d], curr_state:[%d]", event, curr_state);
	switch (curr_state) {
	case STATE_DCE_INVALID:
		switch (event) {
		case EVENT_ID_DCE_FSM_START:
			ret = 0;
			break;
		default:
			dce_err(d, "Invalid event received [%d] state:[%d]\n",
				event, curr_state);
			ret = -EINVAL;
			break;
			}
		break;
	case STATE_DCE_FSM_IDLE:
		switch (event) {
		case EVENT_ID_DCE_BOOT_COMPLETE_RECEIVED:
		case EVENT_ID_DCE_MBOX_IPC_MSG_RECEIVED:
		case EVENT_ID_DCE_ADMIN_IPC_MSG_RECEIVED:
		case EVENT_ID_DCE_SC7_ENTERED_RECEIVED:
		case EVENT_ID_DCE_LOG_READY_RECEIVED:
		case EVENT_ID_DCE_FSM_START:
			dce_err(d, "Invalid event received [%d] state:[%d]\n",
				event, curr_state);
			ret = -EINVAL;
			break;
		default:
			ret = 0;
			break;
		}
		break;
	case STATE_DCE_BOOT_WAIT:
		switch (event) {
		case EVENT_ID_DCE_BOOT_COMPLETE_RECEIVED:
		case EVENT_ID_DCE_ABORT_RECEIVED:
		case EVENT_ID_DCE_CRASH_LOG_RECEIVED:
		case EVENT_ID_DCE_SC7_ENTER_REQUESTED: // TODO: Do we need this here
		case EVENT_ID_DCE_LOG_OVERFLOW_RECEIVED:
		case EVENT_ID_DCE_FSM_STOP:
			ret = 0;
			break;
		default:
			dce_err(d, "Invalid event received [%d] state:[%d]\n",
				event, curr_state);
			ret = -EINVAL;
			break;
		}
		break;
	case STATE_DCE_MBOX_WFI:
		switch (event) {
		case EVENT_ID_DCE_MBOX_IPC_MSG_RECEIVED:
		case EVENT_ID_DCE_ABORT_RECEIVED:
		case EVENT_ID_DCE_CRASH_LOG_RECEIVED:
		case EVENT_ID_DCE_SC7_ENTER_REQUESTED:
		case EVENT_ID_DCE_LOG_OVERFLOW_RECEIVED:
		case EVENT_ID_DCE_FSM_STOP:
			ret = 0;
			break;
		default:
			dce_err(d, "Invalid event received [%d] state:[%d]\n",
				event, curr_state);
			ret = -EINVAL;
			break;
		}
		break;
	case STATE_DCE_ADMIN_WFI:
		switch (event) {
		case EVENT_ID_DCE_ADMIN_IPC_MSG_RECEIVED:
		case EVENT_ID_DCE_ABORT_RECEIVED:
		case EVENT_ID_DCE_CRASH_LOG_RECEIVED:
		case EVENT_ID_DCE_SC7_ENTER_REQUESTED:
		case EVENT_ID_DCE_LOG_OVERFLOW_RECEIVED:
		case EVENT_ID_DCE_FSM_STOP:
			ret = 0;
			break;
		default:
			dce_err(d, "Invalid event received [%d] state:[%d]\n",
				event, curr_state);
			ret = -EINVAL;
			break;
		}
		break;
	case STATE_DCE_SC7_ENTER_WFI:
		switch (event) {
		case EVENT_ID_DCE_SC7_ENTERED_RECEIVED:
		case EVENT_ID_DCE_ABORT_RECEIVED:
		case EVENT_ID_DCE_CRASH_LOG_RECEIVED:
		case EVENT_ID_DCE_LOG_OVERFLOW_RECEIVED:
		case EVENT_ID_DCE_FSM_STOP:
			ret = 0;
			break;
		default:
			dce_err(d, "Invalid event received [%d] state:[%d]\n",
				event, curr_state);
			ret = -EINVAL;
			break;
		}
		break;
	case STATE_DCE_LOG_READY_WFI:
		switch (event) {
		case EVENT_ID_DCE_LOG_READY_RECEIVED:
		case EVENT_ID_DCE_ABORT_RECEIVED:
		case EVENT_ID_DCE_CRASH_LOG_RECEIVED:
		case EVENT_ID_DCE_SC7_ENTER_REQUESTED:
		case EVENT_ID_DCE_LOG_OVERFLOW_RECEIVED:
		case EVENT_ID_DCE_FSM_STOP:
			ret = 0;
			break;
		default:
			dce_err(d, "Invalid event received [%d] state:[%d]\n",
				event, curr_state);
			ret = -EINVAL;
			break;
		}
		break;
	case STATE_DCE_SC7_ENTERED:
		switch (event) {
		case EVENT_ID_DCE_SC7_EXIT_RECEIVED:
			ret = 0;
			break;
		default:
			dce_err(d, "Invalid event received [%d] state:[%d]\n",
				event, curr_state);
			ret = -EINVAL;
			break;
		}
		break;
	default:
		dce_err(d, "Invalid state:[%d] event received [%d]\n", curr_state,
			event);

	}

	if (ret) {
		dce_err(d, "dce event handling failed for event[%d] curr state [%d]",
			event, curr_state);
		goto done;
	}

done:
	return ret;
}

/*
 * dce_fsm_get_event_index : Get index of event in event_process_table
 *
 * @d : Pointer to tegra_dce struct.
 * @event : event for which index is requested
 * @return : index of event in event_process_table or DCE_MAX_EVENTS_IDS
 */
static u32
dce_fsm_get_event_index(struct tegra_dce *d,
			enum dce_fsm_event_id_type event)
{
	u32 id;

	for (id = 0; id < DCE_MAX_EVENTS_IDS; id++) {
		struct dce_event_process_struct *e =
			&event_process_table[id];

		if (e->event == event)
			break;
	}

	return id;
}

/**
 * dce_fsm_post - Post event to FSM
 *
 * @d : Pointer to tegra_dce struct.
 * @event : Event for which FSM state need to be set.
 * @data : Event specific data to pass to the callback function.
 *
 * Return : 0 if successful else error code.
 */
int dce_fsm_post_event(struct tegra_dce *d,
		       enum dce_fsm_event_id_type event,
		       void *data)
{
	u32 id;
	int ret = 0;
	enum dce_fsm_state prev_state;
	struct dce_fsm_info *fsm = &d->fsm_info;

	dce_mutex_lock(&fsm->lock);

	ret = dce_fsm_validate_event(d, event);
	if (ret) {
		dce_mutex_unlock(&fsm->lock);
		goto out;
	}

	prev_state = fsm->c_state;
	dce_fsm_set_state(d, event);
	dce_mutex_unlock(&fsm->lock);

	/*
	 * call callback function with mutex unlocked
	 */
	id = dce_fsm_get_event_index(d, event);
	if ((id < DCE_MAX_EVENTS_IDS) &&
	    (event_process_table[id].fsm_event_handle != NULL)) {
		ret = event_process_table[id].fsm_event_handle(d, data);
		if (ret) {
			dce_mutex_lock(&fsm->lock);
			dce_err(d, "Callback failed: Resetting state old:new [%d:%d]",
				prev_state, fsm->c_state);
			fsm->c_state = prev_state;
			dce_mutex_unlock(&fsm->lock);
		}
	}
out:
	return ret;
}

/**
 * dce_fsm_start - Schedule a work to start the FSM
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : void.
 */
void dce_fsm_start(struct tegra_dce *d)
{
	dce_schedule_work(&d->dce_fsm_bootstrap_work);
}

/**
 * dce_fsm_init - Init the FSM
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : 0 if successful else error code.
 */
int dce_fsm_init(struct tegra_dce *d)
{
	int ret = 0;
	struct dce_fsm_info *fsm = &d->fsm_info;

	fsm->c_state = STATE_DCE_INVALID;
	ret = dce_mutex_init(&fsm->lock);
	if (ret) {
		dce_err(d, "dce mutex initialization failed for FSM");
		return ret;
	}

	fsm->d = d;
	fsm->initialized = true;

	return 0;
}

/**
 * dce_fsm_deinit - DeInit the FSM
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : void
 */
void dce_fsm_deinit(struct tegra_dce *d)
{
	struct dce_fsm_info *fsm = &d->fsm_info;

	dce_fsm_post_event(d, EVENT_ID_DCE_FSM_STOP, NULL);
	dce_mutex_destroy(&fsm->lock);

	fsm->initialized = false;
}
