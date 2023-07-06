/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
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
#include <dce-mailbox.h>
#include <dce-util-common.h>
#include <dce-client-ipc-internal.h>
#include <interface/dce-core-interface-errors.h>
#include <interface/dce-interface.h>
#include <interface/dce-admin-cmds.h>

/**
 * dce_admin_ipc_wait - Waits for message from DCE.
 *
 * @d :  Pointer to tegra_dce struct.
 * @w_type : Requested wait type.
 *
 * Return : 0 if successful
 */
int dce_admin_ipc_wait(struct tegra_dce *d, u32 w_type)
{
	int ret = 0;

	ret = dce_wait_interruptible(d, DCE_WAIT_ADMIN_IPC);
	if (ret) {
		/**
		 * TODO: Add error handling for abort and retry
		 */
		dce_err(d, "Admin IPC wait was interrupted with err:%d", ret);
		goto out;
	}

out:
	return ret;
}

/**
 * dce_admin_wakeup_ipc - wakeup process, waiting for Admin RPC
 *
 * @d :  Pointer to tegra_de struct.
 *
 * Return : Void
 */
static void dce_admin_wakeup_ipc(struct tegra_dce *d)
{
	int ret;

	ret = dce_fsm_post_event(d, EVENT_ID_DCE_ADMIN_IPC_MSG_RECEIVED, NULL);
	if (ret)
		dce_err(d, "Error while posting ADMIN_IPC_MSG_RECEIVED event");
}

/**
 * dce_admin_ipc_handle_signal - Isr for the CCPLEX<->DCE admin interface
 *
 * @d :  Pointer to tegra_de struct.
 *
 * Return : Void
 */
void dce_admin_ipc_handle_signal(struct tegra_dce *d, u32 ch_type)
{
	bool wakeup_needed = false;

	if (!dce_ipc_channel_is_synced(d, ch_type)) {
		/**
		 * The ivc channel is not ready yet. Exit
		 * and wait for another signal from target.
		 */
		return;
	}

	/**
	 * Channel already in sync with remote. Check if data
	 * is available to read.
	 */
	wakeup_needed = dce_ipc_is_data_available(d, ch_type);

	if (!wakeup_needed) {
		dce_info(d, "Spurious signal on channel: [%d]. Ignored...",
			 ch_type);
		return;
	}

	if (ch_type == DCE_IPC_CH_KMD_TYPE_ADMIN) {
		dce_admin_wakeup_ipc(d);
	} else {
		dce_client_ipc_wakeup(d, ch_type);
	}
}

/**
 * dce_admin_ivc_channel_reset - Resets the admin ivc channel
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Void.
 */
void dce_admin_ivc_channel_reset(struct tegra_dce *d)
{
	dce_ipc_channel_reset(d, DCE_IPC_CH_KMD_TYPE_ADMIN);
}

/**
 * dce_admin_channel_deinit - Cleans up the channel resources.
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_admin_channel_deinit(struct tegra_dce *d)
{
	u32 loop_cnt;

	for (loop_cnt = 0; loop_cnt < DCE_IPC_CH_KMD_TYPE_MAX; loop_cnt++)
		dce_ipc_channel_deinit(d, loop_cnt);
}


/**
 * dce_admin_channel_init - Initializes the admin ivc interface
 *
 * @d : Pointer to tegra_dce.
 *
 * Return : 0 if successful.
 */
static int dce_admin_channel_init(struct tegra_dce *d)
{
	int ret = 0;
	u32 loop_cnt;

	for (loop_cnt = 0; loop_cnt < DCE_IPC_CH_KMD_TYPE_MAX; loop_cnt++) {
		ret = dce_ipc_channel_init(d, loop_cnt);
		if (ret) {
			dce_err(d, "Channel init failed for type : [%d]",
				loop_cnt);
			goto out;
		}
	}

out:
	if (ret)
		dce_admin_channel_deinit(d);
	return ret;
}

/**
 * dce_admin_init - Sets up resources managed by admin.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : 0 if successful
 */
int dce_admin_init(struct tegra_dce *d)
{
	int ret = 0;

	d->boot_status |= DCE_EARLY_INIT_START;
	ret = dce_ipc_allocate_region(d);
	if (ret) {
		dce_err(d, "IPC region allocation failed");
		goto err_ipc_reg_alloc;
	}

	ret = dce_admin_channel_init(d);
	if (ret) {
		dce_err(d, "Channel Initialization Failed");
		goto err_channel_init;
	}

	d->boot_status |= DCE_EARLY_INIT_DONE;
	return 0;

err_channel_init:
	dce_ipc_free_region(d);
err_ipc_reg_alloc:
	d->boot_status |= DCE_EARLY_INIT_FAILED;
	return ret;
}

/**
 * dce_admin_deinit - Releases the resources
 *					associated with admin interface.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Void
 */
void dce_admin_deinit(struct tegra_dce *d)
{
	dce_admin_channel_deinit(d);

	dce_ipc_free_region(d);

	dce_mailbox_deinit_interface(d,
			DCE_MAILBOX_ADMIN_INTERFACE);
}

/**
 * dce_admin_allocate_message - Allocates memory for a message
 *						on admin interface.
 * @d : Pointer tegra_dce struct.
 *
 * Return : Allocated msg if successful.
 */
struct dce_ipc_message *dce_admin_allocate_message(struct tegra_dce *d)
{
	struct dce_ipc_message *msg;

	msg = dce_kzalloc(d, sizeof(*msg), false);
	if (!msg) {
		dce_err(d, "Insufficient memory for admin msg");
		goto err_alloc_msg;
	}

	msg->tx.data = dce_kzalloc(d, DCE_ADMIN_CMD_SIZE, false);
	if (!msg->tx.data) {
		dce_err(d, "Insufficient memory for admin msg");
		goto err_alloc_tx;
	}

	msg->rx.data = dce_kzalloc(d, DCE_ADMIN_RESP_SIZE, false);
	if (!msg->rx.data) {
		dce_err(d, "Insufficient memory for admin msg");
		goto err_alloc_rx;
	}

	msg->tx.size = DCE_ADMIN_CMD_SIZE;
	msg->rx.size = DCE_ADMIN_RESP_SIZE;

	return msg;

err_alloc_rx:
	dce_kfree(d, msg->tx.data);
err_alloc_tx:
	dce_kfree(d, msg);
err_alloc_msg:
	return NULL;
}

/**
 * dce_admin_free_message - Frees memory allocated for a message
 *						on admin interface.
 *
 * @d : Pointer to tegra_dce struct.
 * @msg : Pointer to allocated message.
 *
 * Return : Void.
 */
void dce_admin_free_message(struct tegra_dce *d,
				struct dce_ipc_message *msg)
{
	if (!msg || !msg->tx.data || !msg->rx.data)
		return;

	dce_kfree(d, msg->tx.data);
	dce_kfree(d, msg->rx.data);
	dce_kfree(d, msg);
}

/**
 * dce_admin_send_msg - Sends messages on Admin Channel
 *				synchronously and waits for an ack.
 *
 * @d : Pointer to tegra_dce struct.
 * @msg : Pointer to allocated message.
 *
 * Return : 0 if successful
 */
int dce_admin_send_msg(struct tegra_dce *d, struct dce_ipc_message *msg)
{
	int ret = 0;
	struct dce_admin_send_msg_params params;

	params.msg = msg;

	ret = dce_fsm_post_event(d,
				 EVENT_ID_DCE_ADMIN_IPC_MSG_REQUESTED,
				 (void *)&params);
	if (ret)
		dce_err(d, "Unable to send msg invalid FSM state");

	return ret;
}

/**
 * dce_admin_send_msg - Sends messages on Admin Channel
 *				synchronously and waits for an ack.
 *
 * @d : Pointer to tegra_dce struct.
 * @msg : Pointer to allocated message.
 *
 * Return : 0 if successful
 */
int dce_admin_handle_ipc_requested_event(struct tegra_dce *d, void *params)
{
	int ret = 0;
	struct dce_ipc_message *msg;
	struct dce_admin_send_msg_params *admin_params =
			(struct dce_admin_send_msg_params *)params;

	/*
	 * Do not handle admin IPC if boot commands are not completed
	 */
	if (!dce_is_bootcmds_done(d)) {
		dce_err(d, "Boot commands are not yet completed\n");
		ret = -EINVAL;
		goto out;
	}

	/* Error check on msg */
	msg = admin_params->msg;

	ret = dce_ipc_send_message_sync(d, DCE_IPC_CHANNEL_TYPE_ADMIN, msg);
	if (ret)
		dce_err(d, "Error sending admin message on admin interface");
out:
	return ret;
}

int dce_admin_handle_ipc_received_event(struct tegra_dce *d, void *params)
{
	dce_wakeup_interruptible(d, DCE_WAIT_ADMIN_IPC);
	return 0;
}

/**
 * dce_admin_get_ipc_channel_info - Provides channel's
 *                                      buff details
 *
 * @d - Pointer to tegra_dce struct.
 * @q_info : Pointer to struct dce_ipc_queue_info
 *
 * Return - 0 if successful
 */
int dce_admin_get_ipc_channel_info(struct tegra_dce *d,
					struct dce_ipc_queue_info *q_info)
{
	int ret;
	u8 channel_id = DCE_IPC_CHANNEL_TYPE_ADMIN;

	ret = dce_ipc_get_channel_info(d, q_info, channel_id);

	return ret;
}

/**
 * dce_admin_send_cmd_echo - Sends DCE_ADMIN_CMD_ECHO cmd.
 *
 * @d - Pointer to tegra_dce struct.
 * @msg - Pointer to dce_ipc_msg struct.
 *
 * Return - 0 if successful
 */
int dce_admin_send_cmd_echo(struct tegra_dce *d,
			    struct dce_ipc_message *msg)
{
	int ret = -1;
	struct dce_admin_ipc_cmd *req_msg;
	struct dce_admin_ipc_resp *resp_msg;

	if (!msg || !msg->tx.data || !msg->rx.data)
		goto out;

	/* return if dce bootstrap not completed */
	if (!dce_is_bootstrap_done(d)) {
		dce_err(d, "Admin Bootstrap not yet done");
		goto out;
	}

	req_msg = (struct dce_admin_ipc_cmd *)(msg->tx.data);
	resp_msg = (struct dce_admin_ipc_resp *) (msg->rx.data);

	req_msg->cmd = (uint32_t)DCE_ADMIN_CMD_ECHO;

	ret = dce_admin_send_msg(d, msg);
	if ((ret) || (resp_msg->error != DCE_ERR_CORE_SUCCESS)) {
		dce_err(d, "Error sending echo msg : [%d]", ret);
		ret = ret ? ret : resp_msg->error;
		goto out;
	}

out:
	return ret;
}

/**
 * dce_admin_send_cmd_ver - Sends DCE_ADMIN_CMD_VERSION cmd.
 *
 * @d - Pointer to tegra_dce struct.
 * @msg - Pointer to dce_ipc_msg struct.
 *
 * Return - 0 if successful
 */
static int dce_admin_send_cmd_ver(struct tegra_dce *d,
		struct dce_ipc_message *msg)
{
	int ret = 0;
	struct dce_admin_ipc_cmd *req_msg;
	struct dce_admin_ipc_resp *resp_msg;
	struct dce_admin_version_info *ver_info;

	req_msg = (struct dce_admin_ipc_cmd *)(msg->tx.data);
	resp_msg = (struct dce_admin_ipc_resp *) (msg->rx.data);

	req_msg->cmd = (uint32_t)DCE_ADMIN_CMD_VERSION;
	ver_info = (struct dce_admin_version_info *)(&resp_msg->args.version);

	ret = dce_admin_send_msg(d, msg);
	if (ret) {
		dce_err(d, "Error sending get version info : [%d]", ret);
		goto out;
	}
	dce_info(d, "version : [0x%x] err : [0x%x]", ver_info->version,
						     resp_msg->error);

out:
	/**
	 * TODO : Add more error handling here
	 */
	return ret;
}

/**
 * dce_admin_send_prepare_sc7 - Sends DCE_ADMIN_CMD_PREPARE_SC7 cmd.
 *
 * @d - Pointer to tegra_dce struct.
 * @msg - Pointer to dce_ipc_msg struct.
 *
 * Return - 0 if successful
 */
int dce_admin_send_prepare_sc7(struct tegra_dce *d,
			    struct dce_ipc_message *msg)
{
	int ret = -1;
	struct dce_admin_ipc_cmd *req_msg;
	struct dce_admin_ipc_resp *resp_msg;

	if (!msg || !msg->tx.data || !msg->rx.data)
		goto out;

	req_msg = (struct dce_admin_ipc_cmd *)(msg->tx.data);
	resp_msg = (struct dce_admin_ipc_resp *) (msg->rx.data);

	req_msg->cmd = (uint32_t)DCE_ADMIN_CMD_PREPARE_SC7;

	ret = dce_admin_send_msg(d, msg);
	if (ret) {
		dce_err(d, "Error sending prepare sc7 command [%d]", ret);
		goto out;
	}

out:
	return ret;
}

/**
 * dce_admin_send_enter_sc7 - Sends DCE_ADMIN_CMD_ENTER_SC7 cmd.
 *
 * @d - Pointer to tegra_dce struct.
 * @msg - Pointer to dce_ipc_msg struct.
 *
 * Return - 0 if successful
 */
int dce_admin_send_enter_sc7(struct tegra_dce *d,
			     struct dce_ipc_message *msg)
{
	int ret = -1;
	struct dce_admin_ipc_cmd *req_msg;
	struct dce_admin_ipc_resp *resp_msg;

	if (!msg || !msg->tx.data || !msg->rx.data)
		goto out;

	req_msg = (struct dce_admin_ipc_cmd *)(msg->tx.data);
	resp_msg = (struct dce_admin_ipc_resp *) (msg->rx.data);

	req_msg->cmd = (uint32_t)DCE_ADMIN_CMD_ENTER_SC7;

	ret = dce_ipc_send_message(d, DCE_IPC_CHANNEL_TYPE_ADMIN, msg->tx.data, msg->tx.size);
	if (ret) {
		dce_err(d, "Error sending enter sc7 command [%d]", ret);
		goto out;
	}

	/* Wait for SC7 Enter done */
	ret = dce_wait_interruptible(d, DCE_WAIT_SC7_ENTER);
	if (ret) {
		dce_err(d, "SC7 Enter wait was interrupted with err:%d", ret);
		goto out;
	}

out:
	return ret;
}

static int dce_admin_setup_clients_ipc(struct tegra_dce *d,
		struct dce_ipc_message *msg)
{
	uint32_t i;
	int ret = 0;
	struct dce_admin_ipc_cmd *req_msg;
	struct dce_admin_ipc_resp *resp_msg;
	struct dce_ipc_queue_info q_info;
	struct dce_admin_ipc_create_args *ipc_info;

	req_msg = (struct dce_admin_ipc_cmd *)(msg->tx.data);
	resp_msg = (struct dce_admin_ipc_resp *) (msg->rx.data);

	req_msg->cmd = (uint32_t)DCE_ADMIN_CMD_IPC_CREATE;
	ipc_info = (struct dce_admin_ipc_create_args *)
					(&req_msg->args.ipc_create);

	for (i = 0; i < DCE_IPC_CH_KMD_TYPE_MAX; i++) {
		if (i == DCE_IPC_CH_KMD_TYPE_ADMIN)
			continue;
		ret = dce_ipc_get_channel_info(d, &q_info, i);
		if (ret) {
			dce_info(d, "Get queue info failed for [%u]", i);
			ret = 0;
			continue;
		}

		ipc_info->type = dce_ipc_get_ipc_type(d, i);
		ipc_info->rd_iova = q_info.tx_iova;
		ipc_info->wr_iova = q_info.rx_iova;
		ipc_info->fsize = q_info.frame_sz;
		ipc_info->n_frames = q_info.nframes;

		ret = dce_admin_send_msg(d, msg);
		if (ret) {
			dce_err(d, "Error sending IPC create msg for type [%u]",
				i);
			goto out;
		}

		if (resp_msg->error != DCE_ERR_CORE_SUCCESS) {
			dce_err(d, "IPC create for type [%u] failed", i);
			goto out;
		}

		dce_ipc_channel_reset(d, i);
		dce_info(d, "Channel Reset Complete for Type [%u] ...", i);
	}

out:
	/**
	 * TODO : Add more error handling here
	 */
	return ret;
}

static int dce_admin_send_rm_bootstrap(struct tegra_dce *d,
		struct dce_ipc_message *msg)
{
	int ret = 0;
	struct dce_admin_ipc_cmd *req_msg;
	struct dce_admin_ipc_resp *resp_msg;
	struct dce_admin_version_info *ver_info;

	req_msg = (struct dce_admin_ipc_cmd *)(msg->tx.data);
	resp_msg = (struct dce_admin_ipc_resp *) (msg->rx.data);

	req_msg->cmd = (uint32_t)DCE_ADMIN_CMD_RM_BOOTSTRAP;
	ver_info = (struct dce_admin_version_info *)(&resp_msg->args.version);

	ret = dce_admin_send_msg(d, msg);
	if (ret) {
		dce_err(d, "Error sending rm bootstrap cmd: [%d]", ret);
		goto out;
	}

	if (resp_msg->error != DCE_ERR_CORE_SUCCESS) {
		dce_err(d, "Error in handling rm bootstrap cmd on dce: [0x%x]",
			resp_msg->error);
		ret = -EINVAL;
	}

out:
	/**
	 * TODO : Add more error handling here
	 */
	return ret;
}

int dce_start_admin_seq(struct tegra_dce *d)
{
	int ret = 0;
	struct dce_ipc_message *msg;

	msg = dce_admin_allocate_message(d);
	if (!msg)
		return -1;

	d->boot_status |= DCE_FW_ADMIN_SEQ_START;
	ret = dce_admin_send_cmd_ver(d, msg);
	if (ret) {
		dce_err(d, "RPC failed for DCE_ADMIN_CMD_VERSION");
		goto out;
	}

	ret = dce_admin_setup_clients_ipc(d, msg);
	if (ret) {
		dce_err(d, "RPC failed for DCE_ADMIN_CMD_IPC_CREATE");
		goto out;
	}

	ret = dce_admin_send_rm_bootstrap(d, msg);
	if (ret) {
		dce_err(d, "RPC failed for DCE_ADMIN_CMD_RM_BOOTSTRAP");
		goto out;
	}
	d->boot_status |= DCE_FW_ADMIN_SEQ_DONE;
out:
	dce_admin_free_message(d, msg);
	if (ret)
		d->boot_status |= DCE_FW_ADMIN_SEQ_FAILED;
	return ret;
}
