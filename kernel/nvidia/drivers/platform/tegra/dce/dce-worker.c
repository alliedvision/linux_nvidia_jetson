/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <dce-cond.h>
#include <dce-lock.h>
#include <dce-worker.h>
#include <dce-util-common.h>
#include <interface/dce-admin-cmds.h>

/**
 * dce_worker_wakeup_cond - Generic wake up condition for
 *						dce_worker thread.
 *
 * @d : Pointer to struct tegra_dce.
 *
 * Return : Boolean
 */
static bool dce_worker_wakeup_cond(struct tegra_dce *d)
{
	struct dce_worker_info *w = &d->wrk_info;

	return (w->state_changed == true ||
			dce_thread_should_stop(&w->wrk_thread));
}

/**
 * dce_worker_thread_wait - Will wait in a given state.
 *
 * @d : Pointer to tegra_dce struct.
 * @event : Event that will trigger the wait.
 *
 * Will change state and wait based on the event that
 * triggers the wait.
 *
 * Return : Void
 */
void dce_worker_thread_wait(struct tegra_dce *d,
			enum dce_worker_event_id_type event)
{
	int ret;
	u32 timeout_val_ms = 0;
	enum dce_worker_state new_state;
	struct dce_worker_info *w = &d->wrk_info;

	dce_mutex_lock(&w->lock);

	if (w->state_changed == true) {
		w->state_changed = false;
		dce_warn(d, "Unexpected state_changed value");
		dce_mutex_unlock(&w->lock);
		return;
	}

	switch (event) {
	case EVENT_ID_DCE_BOOT_COMPLETE_IRQ_REQ_SET:
		if ((w->c_state != STATE_DCE_WORKER_IDLE) &&
			(w->c_state != STATE_DCE_WORKER_BOOT_WAIT)) {
			dce_warn(d, "Unexpected wait event [%d] rcvd in state [%d]",
					event, w->c_state);
			return;
		}
		new_state = STATE_DCE_WORKER_BOOT_WAIT;
		break;
	case EVENT_ID_DCE_IPC_SYNC_TRIGGERED:
	case EVENT_ID_DCE_IPC_MESSAGE_SENT:
		new_state = STATE_DCE_WORKER_WFI;
		break;
	case EVENT_ID_DCE_BOOT_COMPLETE:
		new_state = STATE_DCE_WORKER_IDLE;
		break;
	default:
		dce_warn(d, "Invalid wait event [%d] rcvd in state [%d]",
				event, w->c_state);
		return;
	}

	w->c_state = new_state;
	dce_mutex_unlock(&w->lock);

	if (new_state == STATE_DCE_WORKER_BOOT_WAIT)
		timeout_val_ms = 1000;

	ret = DCE_COND_WAIT_INTERRUPTIBLE(&w->cond,
				dce_worker_wakeup_cond(d),
				timeout_val_ms);

	dce_mutex_lock(&w->lock);

	w->state_changed = false;

	if (ret)
		w->c_state = STATE_DCE_WORKER_IDLE;

	dce_mutex_unlock(&w->lock);
}

/**
 * dce_worker_thread_wakeup - Wakeup the dce worker thread
 *
 * @d : Pointer to tegra_dce struct.
 * @event : Event that will trigger the wakeup.
 *
 * Will change state and wakeup the worker thread based on
 * the event that triggers it.
 *
 * Return : Void
 */
void dce_worker_thread_wakeup(struct tegra_dce *d,
			enum dce_worker_event_id_type event)
{
	struct dce_worker_info *w = &d->wrk_info;
	enum dce_worker_state new_state = w->c_state;

	dce_mutex_lock(&w->lock);

	if (w->state_changed == true)
		dce_warn(d, "Unexpected state_changed value");

	switch (event) {
	case EVENT_ID_DCE_IPC_SIGNAL_RECEIVED:
		if (w->c_state != STATE_DCE_WORKER_WFI) {
			dce_warn(d, "Unexpected wakeup event rcvd: [%d]. Cur State: [%d]",
					event, w->c_state);
		}
		new_state = STATE_DCE_WORKER_IDLE;
		break;
	case EVENT_ID_DCE_BOOT_COMPLETE_IRQ_RECEIVED:
		if (w->c_state != STATE_DCE_WORKER_BOOT_WAIT) {
			dce_warn(d, "Unexpected wakeup event rcvd: [%d]. Cur State: [%d]",
					event, w->c_state);
		}
		new_state = STATE_DCE_WORKER_IDLE;
		break;
	case EVENT_ID_DCE_INTERFACE_ERROR_RECEIVED:
		new_state = STATE_DCE_WORKER_HANDLE_DCE_ERROR;
		dce_warn(d, "Error Event Rcvd: [%d]. Cur State: [%d]",
				event, w->c_state);
		break;
	case EVENT_ID_DCE_THREAD_ABORT_REQ_RECEIVED:
		if (dce_thread_should_stop(&w->wrk_thread))
			new_state = STATE_DCE_WORKER_ABORTED;
		else
			dce_warn(d, "Unexpected wakeup event rcvd: [%d]. Cur State: [%d]",
					event, w->c_state);
		break;
	default:
		dce_warn(d, "Unexpected wakeup event rcvd: [%d]. Cur State: [%d]",
				event, w->c_state);
		return;
	}

	w->c_state = new_state;
	w->state_changed = true;
	dce_mutex_unlock(&w->lock);

	dce_cond_signal_interruptible(&w->cond);
}

static void dce_handle_dce_error(struct tegra_dce *d)
{
	/**
	 * TODO : Handle error messages from DCE
	 */
}

/**
 * dce_worker - dce worker main function to manage dce thread states.
 *
 * @arg : Void pointer to be typecast to tegra_dce struct before use.
 *
 * Return : 0 on success.
 */
static int dce_worker(void *arg)
{
	int ret = 0;
	struct tegra_dce *d = (struct tegra_dce *)arg;
	struct dce_worker_info *w = &d->wrk_info;

	ret = dce_wait_boot_complete(d);
	if (ret) {
		dce_warn(d, "DCE_BOOT_FAILED: Boot didn't complete");
		goto worker_exit;
	}

	ret = dce_start_bootstrap_flow(d);
	if (ret) {
		dce_warn(d, "DCE_BOOT_FAILED: Bootstrap flow didn't complete");
		goto worker_exit;
	}

	dce_admin_ivc_channel_reset(d);

	ret = dce_start_admin_seq(d);
	if (ret) {
		dce_warn(d, "DCE_BOOT_FAILED: Admin flow didn't complete");
	} else {
		d->boot_status |= DCE_FW_BOOT_DONE;
		dce_info(d, "DCE_BOOT_DONE");
	}

	do {
		dce_worker_thread_wait(d, EVENT_ID_DCE_BOOT_COMPLETE);

		if (w->c_state == STATE_DCE_WORKER_HANDLE_DCE_ERROR) {
			dce_handle_dce_error(d);
			d->boot_status |= DCE_STATUS_FAILED;
		}
	} while ((w->c_state != STATE_DCE_WORKER_ABORTED) ||
		(!dce_thread_should_stop(&w->wrk_thread)));

worker_exit:
	if (w->c_state == STATE_DCE_WORKER_ABORTED)
		dce_warn(d, "Exiting Dce Worker Thread");
	if (ret)
		d->boot_status |= DCE_STATUS_FAILED;
	return 0;
}

/**
 * dce_worker_get_state - Gets the current state of dce_worker
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Current state
 */
enum dce_worker_state dce_worker_get_state(struct tegra_dce *d)
{
	return d->wrk_info.c_state;
}

/**
 * dce_worker_thread_init - Initializes the worker thread and
 *					its corresponding resources.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : 0 if success.
 */
int dce_worker_thread_init(struct tegra_dce *d)
{
	int ret = 0;
	struct dce_worker_info *w
			= &d->wrk_info;

	ret = dce_cond_init(&w->cond);
	if (ret) {
		dce_err(d, "dce condition initialization failed for worker");
		goto err_cond_init;
	}

	ret = dce_mutex_init(&w->lock);
	if (ret) {
		dce_err(d, "dce condition initialization failed for worker");
		goto err_lock_init;
	}

	dce_mutex_lock(&w->lock);

	w->state_changed = false;

	w->c_state = STATE_DCE_WORKER_IDLE;

	ret = dce_thread_create(&w->wrk_thread, d,
				dce_worker, "dce_worker_thread");
	if (ret) {
		dce_err(d, "Dce Worker Thread creation failed");
		dce_mutex_unlock(&w->lock);
		goto err_thread_create;
	}

	dce_mutex_unlock(&w->lock);

	return ret;

err_thread_create:
	dce_mutex_destroy(&w->lock);
err_lock_init:
	dce_cond_destroy(&w->cond);
err_cond_init:
	return ret;
}

/**
 * dce_worker_thread_deinit - Kill the dce worker thread and
 *					its corresponding resources.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return :Void
 */
void dce_worker_thread_deinit(struct tegra_dce *d)
{
	struct dce_worker_info *w = &d->wrk_info;

	if (dce_thread_is_running(&w->wrk_thread))
		dce_thread_stop(&w->wrk_thread);

	dce_thread_join(&w->wrk_thread);

	dce_mutex_destroy(&w->lock);

	dce_cond_destroy(&w->cond);
}
