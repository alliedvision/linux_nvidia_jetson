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

#include <dce.h>

#define CCPLEX_HSP_IE 1U /* TODO : Have an api to read from platform data */

static void dce_pm_save_state(struct tegra_dce *d)
{
	d->sc7_state.hsp_ie = dce_hsp_ie_read(d, CCPLEX_HSP_IE);
}

static void dce_pm_restore_state(struct tegra_dce *d)
{
	uint32_t val = d->sc7_state.hsp_ie;

	dce_hsp_ie_write(d, val, CCPLEX_HSP_IE);
}

/**
 * dce_resume_work_fn : execute resume and bootstrap flow
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : void
 */
void dce_resume_work_fn(struct tegra_dce *d)
{
	int ret = 0;

	if (d == NULL) {
		dce_err(d, "tegra_dce struct is NULL");
		return;
	}

	ret = dce_fsm_post_event(d, EVENT_ID_DCE_BOOT_COMPLETE_REQUESTED, NULL);
	if (ret) {
		dce_err(d, "Error while posting DCE_BOOT_COMPLETE_REQUESTED event");
		return;
	}

	ret = dce_start_boot_flow(d);
	if (ret) {
		dce_err(d, "DCE bootstrapping failed\n");
		return;
	}
}

/**
 * dce_handle_sc7_enter_requested_event - callback handler function for event
 *					 EVENT_ID_DCE_SC7_ENTER_REQUESTED
 *
 * @d : Pointer to tegra_dce struct.
 * @params : callback params
 *
 * Return : 0 if successful else error code
 */
int dce_pm_handle_sc7_enter_requested_event(struct tegra_dce *d, void *params)
{
	int ret = 0;
	struct dce_ipc_message *msg = NULL;

	msg = dce_admin_allocate_message(d);
	if (!msg) {
		dce_err(d, "IPC msg allocation failed");
		goto out;
	}

	ret = dce_admin_send_enter_sc7(d, msg);
	if (ret) {
		dce_err(d, "Enter SC7 failed [%d]", ret);
		goto out;
	}

	dce_set_boot_complete(d, false);
	d->boot_status |= DCE_FW_SUSPENDED;

out:
	dce_admin_free_message(d, msg);
	return ret;
}

/**
 * dce_handle_sc7_enter_received_event - callback handler function for event
 *					 EVENT_ID_DCE_SC7_ENTER_RECEIVED
 *
 * @d : Pointer to tegra_dce struct.
 * @params : callback params
 *
 * Return : 0 if successful else error code
 */
int dce_pm_handle_sc7_enter_received_event(struct tegra_dce *d, void *params)
{
	dce_wakeup_interruptible(d, DCE_WAIT_SC7_ENTER);
	return 0;
}

/**
 * dce_handle_sc7_exit_received_event - callback handler function for event
 *					 EVENT_ID_DCE_SC7_EXIT_RECEIVED
 *
 * @d : Pointer to tegra_dce struct.
 * @params : callback params
 *
 * Return : 0 if successful else error code
 */
int dce_pm_handle_sc7_exit_received_event(struct tegra_dce *d, void *params)
{
	dce_schedule_work(&d->dce_resume_work);
	return 0;
}

int dce_pm_enter_sc7(struct tegra_dce *d)
{
	int ret = 0;
	struct dce_ipc_message *msg = NULL;

	/*
	 * If Bootstrap is not yet done. Nothing to do during SC7 Enter
	 * Return success immediately.
	 */
	if (!dce_is_bootstrap_done(d)) {
		dce_debug(d, "Bootstrap not done, Succeed SC7 enter\n");
		goto out;
	}

	msg = dce_admin_allocate_message(d);
	if (!msg) {
		dce_err(d, "IPC msg allocation failed");
		ret = -1;
		goto out;
	}

	dce_pm_save_state(d);

	ret = dce_admin_send_prepare_sc7(d, msg);
	if (ret) {
		dce_err(d, "Prepare SC7 failed [%d]", ret);
		ret = -1;
		goto out;
	}

	ret = dce_fsm_post_event(d, EVENT_ID_DCE_SC7_ENTER_REQUESTED, NULL);
	if (ret) {
		dce_err(d, "Error while posting SC7_ENTER event [%d]", ret);
		ret = -1;
		goto out;
	}

out:
	dce_admin_free_message(d, msg);
	return ret;
}

int dce_pm_exit_sc7(struct tegra_dce *d)
{
	int ret = 0;

	dce_pm_restore_state(d);

	ret = dce_fsm_post_event(d, EVENT_ID_DCE_SC7_EXIT_RECEIVED, NULL);
	if (ret) {
		dce_err(d, "Error while posting SC7_EXIT event [%d]", ret);
		goto out;
	}
out:
	return ret;
}
