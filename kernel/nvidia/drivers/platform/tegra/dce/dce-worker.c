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

#include <dce.h>
#include <dce-cond.h>
#include <dce-lock.h>
#include <dce-worker.h>
#include <dce-util-common.h>
#include <interface/dce-admin-cmds.h>

/*
 * dce_wait_interruptible : Wait for a given condition
 *
 * @d : Pointer to tegra_dce struct.
 * @msg_id : index of wait condition
 *
 * Return : 0 if successful else error code
 */
int dce_wait_interruptible(struct tegra_dce *d, u32 msg_id)
{
	struct dce_wait_cond *wait;

	if (msg_id >= DCE_MAX_WAIT) {
		dce_err(d, "Invalid wait requested %u", msg_id);
		return -EINVAL;
	}

	wait = &d->ipc_waits[msg_id];
	atomic_set(&wait->complete, 0);

	DCE_COND_WAIT_INTERRUPTIBLE(&wait->cond_wait,
			atomic_read(&wait->complete) == 1,
			0);

	if (atomic_read(&wait->complete) != 1)
		return -EINTR;

	atomic_set(&wait->complete, 0);
	return 0;
}

/*
 * dce_wakeup_interruptible : Wakeup waiting task on given condition
 *
 * @d : Pointer to tegra_dce struct.
 * @msg_id : index of wait condition
 *
 * Return : void
 */
void dce_wakeup_interruptible(struct tegra_dce *d, u32 msg_id)
{
	struct dce_wait_cond *wait;

	if (msg_id >= DCE_MAX_WAIT) {
		dce_err(d, "Invalid wait requested %u", msg_id);
		return;
	}

	wait = &d->ipc_waits[msg_id];

	atomic_set(&wait->complete, 1);
	dce_cond_signal_interruptible(&wait->cond_wait);
}

/*
 * dce_start_boot_flow : Start dce bootstrap flow
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : 0 if successful else error code
 */
static int
dce_start_boot_flow(struct tegra_dce *d)
{
	int ret = 0;

	ret = dce_start_bootstrap_flow(d);
	if (ret) {
		dce_warn(d, "DCE_BOOT_FAILED: Bootstrap flow didn't complete");
		goto exit;
	}

	dce_admin_ivc_channel_reset(d);

	ret = dce_start_admin_seq(d);
	if (ret) {
		dce_warn(d, "DCE_BOOT_FAILED: Admin flow didn't complete");
	} else {
		d->boot_status |= DCE_FW_BOOT_DONE;
		dce_info(d, "DCE_BOOT_DONE");
	}

exit:
	if (ret)
		d->boot_status |= DCE_STATUS_FAILED;

	return ret;
}

/**
 * dce_fsm_bootstrap_work_fn : execute fsm start and bootstrap flow
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : void
 */
void dce_fsm_bootstrap_work_fn(struct tegra_dce *d)
{
	int ret = 0;

	if (d == NULL) {
		dce_err(d, "tegra_dce struct is NULL");
		return;
	}

	ret = dce_fsm_post_event(d, EVENT_ID_DCE_FSM_START, NULL);
	if (ret) {
		dce_err(d, "FSM start failed\n");
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
 * dce_fsm_resource_init : Init dce workques related resources
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : 0 if successful else error code
 */
int dce_fsm_resource_init(struct tegra_dce *d)
{
	int ret = 0;
	int i;

	ret = dce_init_work(d, &d->dce_fsm_bootstrap_work, dce_fsm_bootstrap_work_fn);
	if (ret) {
		dce_err(d, "fsm_start work init failed");
		goto exit;
	}

	ret = dce_init_work(d, &d->dce_resume_work, dce_resume_work_fn);
	if (ret) {
		dce_err(d, "resume work init failed");
		goto exit;
	}

	for (i = 0; i < DCE_MAX_WAIT; i++) {
		struct dce_wait_cond *wait = &d->ipc_waits[i];

		if (dce_cond_init(&wait->cond_wait)) {
			dce_err(d, "dce wait condition %d init failed", i);
			ret = -1;
			goto init_error;
		}

		atomic_set(&wait->complete, 0);
	}

	return 0;
init_error:
	while (i >= 0) {
		struct dce_wait_cond *wait = &d->ipc_waits[i];

		dce_cond_destroy(&wait->cond_wait);
		i--;
	}
exit:
	return ret;
}

/**
 * dce_fsm_resource_init : de-init dce workques related resources
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : void
 */
void dce_fsm_resource_deinit(struct tegra_dce *d)
{
	int i;

	for (i = 0; i < DCE_MAX_WAIT; i++) {
		struct dce_wait_cond *wait = &d->ipc_waits[i];

		dce_cond_destroy(&wait->cond_wait);
		atomic_set(&wait->complete, 0);
	}
}
