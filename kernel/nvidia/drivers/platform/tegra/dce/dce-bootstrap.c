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
#include <dce.h>
#include <dce-util-common.h>
#include <interface/dce-interface.h>
#include <interface/dce-boot-cmds.h>
#include <interface/dce-interface.h>

/**
 * dce_boot_complete - Checks if dce has complelted boot.
 *
 * @d - Pointer to tegra_dce struct.
 *
 * Return : True if boot is complete
 */
static inline bool dce_boot_complete(struct tegra_dce *d)
{
	return !!(dce_ss_get_state(d, DCE_BOOT_SEMA)
					& DCE_BOOT_COMPLETE);
}

/**
 * dce_boot_poll_boot_complete - Poll until dce boot is complete.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : 0 if successful
 */
static int dce_boot_poll_boot_complete(struct tegra_dce *d)
{
	int ret = 0;

	while (!dce_boot_complete(d)) {
		dce_worker_thread_wait(d,
			EVENT_ID_DCE_BOOT_COMPLETE_IRQ_REQ_SET);
	}

	if (dce_worker_get_state(d) == STATE_DCE_WORKER_ABORTED)
		ret = -1;

	return ret;
}

/**
 * dce_req_boot_irq_sync - Requests DCE to raise an
 *				interrupt on boot completion.
 *
 * @d - Pointer to tegra_dce struct.
 *
 * Return : 0 if sucessful.
 */
static int dce_req_boot_irq_sync(struct tegra_dce *d)
{
	int ret = 0;

#define DCE_BOOT_INIT_BPOS 31U
	dce_ss_set(d, DCE_BOOT_INIT_BPOS, DCE_BOOT_SEMA);
#undef DCE_BOOT_INIT_BPOS

	dce_info(d, "Waiting on dce fw to boot...");

	ret = dce_boot_poll_boot_complete(d);
	if (ret)
		dce_err(d, "DCE Boot Complete Poll Interrupted");

	return ret;
}

/**
 * dce_wait_boot_complete - Wait for the DCE to boot and be ready to receive
 *				commands from CCPLEX driver.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : 0 if successful
 */
int dce_wait_boot_complete(struct tegra_dce *d)
{
	int ret = 0;

	if (dce_boot_complete(d))
		goto boot_done;

	ret = dce_req_boot_irq_sync(d);

boot_done:
	if (!ret) {
		dce_set_boot_complete(d, true);
		d->boot_status |= DCE_FW_EARLY_BOOT_DONE;
		dce_info(d, "dce is ready to receive bootstrap commands");
	} else {
		d->boot_status |= DCE_FW_EARLY_BOOT_FAILED;
	}
	return ret;
}

/**
 * dce_handle_irq_status - Handles irq status from DCE
 *
 * @d : Pointer to struct tegra_dce
 * @status : Status received from DCE
 *
 * Return : Void
 */
void dce_handle_irq_status(struct tegra_dce *d, u32 status)
{
	enum dce_worker_event_id_type event;

	if (status & DCE_IRQ_LOG_OVERFLOW)
		dce_info(d, "DCE trace log overflow error received");

	if (status & DCE_IRQ_LOG_READY)
		dce_info(d, "DCE trace log buffers available");

	if (status & DCE_IRQ_CRASH_LOG)
		dce_info(d, "DCE crash log available");

	if (status & DCE_IRQ_ABORT)
		dce_err(d, "DCE ucode abort occurred");

	if (status & DCE_IRQ_SC7_ENTERED)
		dce_info(d, "DCE can be safely powered-off now");

	event = EVENT_ID_DCE_INTERFACE_ERROR_RECEIVED;

	if (status & DCE_IRQ_READY) {
		dce_info(d, "DCE IRQ Ready Received");
		event = EVENT_ID_DCE_BOOT_COMPLETE_IRQ_RECEIVED;
	}

	dce_worker_thread_wakeup(d, event);
}


/**
 * dce_bootstrap_handle_boot_status- Handles boot status from DCE
 *
 * @d : Pointer to struct tegra_dce
 * @status : Status received from DCE
 *
 * Return : Void
 */
void dce_bootstrap_handle_boot_status(struct tegra_dce *d, u32 status)
{
	enum dce_worker_event_id_type event;

	event = EVENT_ID_DCE_IPC_SIGNAL_RECEIVED;

	dce_mailbox_store_interface_status(d, status,
					   DCE_MAILBOX_BOOT_INTERFACE);

	dce_worker_thread_wakeup(d, event);
}


/**
 * dce_boot_interface_isr - Isr for the CCPLEX<->DCE boot interface.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Void
 */
static void dce_boot_interface_isr(struct tegra_dce *d, void *data)
{
	u32 status;
	u8 interface_id = DCE_MAILBOX_BOOT_INTERFACE;

	status = dce_mailbox_get_interface_status(d, interface_id);
	if (status == 0xffffffff)
		return;

	switch (DCE_IRQ_GET_STATUS_TYPE(status)) {
	case DCE_IRQ_STATUS_TYPE_IRQ:
		dce_handle_irq_status(d, status);
		break;
	case DCE_IRQ_STATUS_TYPE_BOOT_CMD:
		dce_bootstrap_handle_boot_status(d, status);
		break;
	default:
		dce_info(d, "Invalid Status Received from DCE. Status: [%x]",
			 status);
		break;
	}
}

/**
 * dce_parse_boot_status_err - Parses the error sent by DCE
 *
 * @d : Pointer to struct tegra_dce
 * @status : Status read from mailbox
 *
 * Return : Void
 */
static void dce_parse_boot_status_err(struct tegra_dce *d, u32 status)
{
#define DCE_BOOT_ERR_MASK 0x7FFFFF
	status &= DCE_BOOT_ERR_MASK;
#undef DCE_BOOT_ERR_MASK

	switch (status) {
	case DCE_BOOT_CMD_ERR_BAD_COMMAND:
		dce_info(d, "Boot Status Error : DCE_BOOT_CMD_ERR_BAD_COMMAND");
		break;
	case DCE_BOOT_CMD_ERR_UNIMPLEMENTED:
		dce_info(d, "Boot Status Error : DCE_BOOT_CMD_ERR_UNIMPLEMENTED");
		break;
	case DCE_BOOT_CMD_ERR_IPC_SETUP:
		dce_info(d, "Boot Status Error : DCE_BOOT_CMD_ERR_IPC_SETUP");
		break;
	case DCE_BOOT_CMD_ERR_INVALID_NFRAMES:
		dce_info(d, "Boot Status Error : DCE_BOOT_CMD_ERR_INVALID_NFRAMES");
		break;
	case DCE_BOOT_CMD_ERR_IPC_CREATE:
		dce_info(d, "Boot Status Error : DCE_BOOT_CMD_ERR_IPC_CREATE");
		break;
	case DCE_BOOT_CMD_ERR_LOCKED:
		dce_info(d, "Boot Status Error : DCE_BOOT_CMD_ERR_LOCKED");
		break;
	default:
		dce_info(d, "Invalid Error Status Rcvd. Status: [%x]", status);
		break;
	}
}

/**
 * dce_mailbox_wait_boot_interface - Waits for mailbox messages.
 *
 * @d : Pointer to tegra_dce
 *
 * Return : 0 if successful
 */
static int dce_mailbox_wait_boot_interface(struct tegra_dce *d)
{
	u32 status;
	enum dce_worker_event_id_type event;

	event = EVENT_ID_DCE_IPC_MESSAGE_SENT;

	dce_worker_thread_wait(d, event);

	status = dce_mailbox_get_interface_status(d,
				DCE_MAILBOX_BOOT_INTERFACE);

	if (dce_worker_get_state(d) == STATE_DCE_WORKER_ABORTED)
		return -EINTR;

	if (status & DCE_BOOT_CMD_ERR_FLAG) {
		dce_parse_boot_status_err(d, status);
		dce_err(d, "Error code received on boot interface : 0x%x",
			status);
		return -EBADE;
	}

	return 0;
}

/**
 * dce_boot_interface_init - Initializes the dce boot interface
 *			and the associated resources.
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : 0 if successful
 */
int dce_boot_interface_init(struct tegra_dce *d)
{
	int ret = 0;
	u8 mailbox_id = DCE_MAILBOX_BOOT_INTERFACE;

	ret = dce_mailbox_init_interface(d, mailbox_id,
					DCE_MBOX_BOOT_CMD, DCE_MBOX_IRQ,
					dce_mailbox_wait_boot_interface,
					NULL, dce_boot_interface_isr);
	if (ret) {
		dce_err(d, "Boot Mailbox Interface Init Failed");
		goto err_init;
	}

err_init:
	return ret;
}

/**
 * dce_boot_interface_deinit - Releases the resources
 *			associated with dce boot.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Void
 */
void dce_boot_interface_deinit(struct tegra_dce *d)
{
	dce_mailbox_deinit_interface(d,
			DCE_MAILBOX_BOOT_INTERFACE);
}

/**
 * dce_send_version_cmd - Sends the "VERSION" command to dce fw
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : 0 if successful
 */
static int dce_send_version_cmd(struct tegra_dce *d)
{
	u32 val;
	int ret = 0;

	val = DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_VERSION);

	ret = dce_mailbox_send_cmd_sync(d, val, DCE_MAILBOX_BOOT_INTERFACE);

	return ret;
}

/**
 * dce_send_set_sid_cmd - Sends the "SET_SID" command to dce fw
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : 0 if successful
 */
static int dce_send_set_sid_cmd(struct tegra_dce *d)
{
	u32 val;
	int ret = 0;

	val = DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_SET_SID) |
			DCE_BOOT_CMD_PARM_SET(0, dce_get_dce_stream_id(d));

	ret = dce_mailbox_send_cmd_sync(d, val, DCE_MAILBOX_BOOT_INTERFACE);

	return ret;
}

/**
 * dce_send_channel_int_cmd - Sends the "CHANNEL_INIT" command to dce fw
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : 0 if successful
 */
static int dce_send_channel_int_cmd(struct tegra_dce *d)
{
	u32 val;
	int ret = 0;

	val = DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_CHANNEL_INIT);

	ret = dce_mailbox_send_cmd_sync(d, val, DCE_MAILBOX_BOOT_INTERFACE);

	return ret;
}

/**
 * dce_send_set_addr_read_cmd_hi - Sends addr_hi cmd to dce fw.
 *
 * @d : Pointer to tegra_dce struct.
 * @addr : IOVA addr to be sent.
 * @rd_wr : Tells if the addr to be sent is for read or write
 * interface.
 *
 * Return : 0 if successful
 */
static int dce_send_set_addr_cmd_hi(struct tegra_dce *d, u32 addr, u8 rd_wr)
{
	u32 val;
	int ret = 0;

	val = DCE_BOOT_CMD_SET_HILO(0U, 1U) |
		DCE_BOOT_CMD_SET_RDWR(0U, rd_wr) |
		DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_SET_ADDR) |
		DCE_BOOT_CMD_PARM_SET(0U, addr);

	ret = dce_mailbox_send_cmd_sync(d, val, DCE_MAILBOX_BOOT_INTERFACE);

	return ret;
}

/**
 * dce_send_set_addr_read_cmd_lo - Sends addr_lo cmd to dce fw.
 *
 * @d : Pointer to tegra_dce struct.
 * @addr : IOVA addr to be sent.
 * @rd_wr : Tells if the addr to be sent is for read or write
 * interface.
 *
 * Return : 0 if successful
 */
static int dce_send_set_addr_cmd_lo(struct tegra_dce *d, u32 addr, u8 rd_wr)
{
	u32 val;
	int ret = 0;

	val = DCE_BOOT_CMD_SET_HILO(0U, 0U) |
		DCE_BOOT_CMD_SET_RDWR(0U, rd_wr) |
		DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_SET_ADDR) |
		DCE_BOOT_CMD_PARM_SET(0U, addr);

	ret = dce_mailbox_send_cmd_sync(d, val, DCE_MAILBOX_BOOT_INTERFACE);

	return ret;
}

/**
 * dce_send_set_addr_read_cmd - Sends the addresses for admin
 *					read interface to dce fw.
 *
 * @d : Pointer to tegra_dce struct
 * @rd_buff : Read address
 *
 * Return : 0 if successful
 */
static int dce_send_set_addr_read_cmd(struct tegra_dce *d, const u64 rd_buff)
{
	int ret = 0;

#define DCE_DATA_NBITS_SHIFT 20
	ret = dce_send_set_addr_cmd_hi(d, rd_buff >> DCE_DATA_NBITS_SHIFT, 0);
	if (ret) {
		dce_err(d, "Sending of SEND_ADDR for READ IOVA HI failed");
		goto err_sending;
	}

	ret = dce_send_set_addr_cmd_lo(d, rd_buff, 0);
	if (ret) {
		dce_err(d, "Sending of SEND_ADDR for READ IOVA LO failed");
		goto err_sending;
	}
#undef DCE_DATA_NBITS_SHIFT

err_sending:
	return ret;
}

/**
 * dce_send_set_addr_write_cmd - Sends the addresses for admin
 *					write interface to dce fw.
 *
 * @d : Pointer to tegra_dce struct
 * @wr_buff : Write address
 *
 * Return : 0 if successful
 */
static int dce_send_set_addr_write_cmd(struct tegra_dce *d, const u64 wr_buff)
{
	int ret = 0;

#define DCE_DATA_NBITS_SHIFT 20
	ret = dce_send_set_addr_cmd_hi(d, wr_buff >> DCE_DATA_NBITS_SHIFT, 1);
	if (ret) {
		dce_err(d, "Sending of SEND_ADDR for READ IOVA HI failed");
		goto err_sending;
	}

	ret = dce_send_set_addr_cmd_lo(d, wr_buff, 1);
	if (ret) {
		dce_err(d, "Sending of SEND_ADDR for READ IOVA LO failed");
		goto err_sending;
	}
#undef DCE_DATA_NBITS_SHIFT

err_sending:
	return ret;
}

/**
 * dce_send_get_fsize_cmd - Sends the "GET_FSIZE" command to dce fw
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : 0 if successful
 */
static int dce_send_get_fsize_cmd(struct tegra_dce *d)
{
	u32 val;
	int ret = 0;

	val = DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_GET_FSIZE);

	ret = dce_mailbox_send_cmd_sync(d, val, DCE_MAILBOX_BOOT_INTERFACE);

	return ret;
}

/**
 * dce_send_set_nframes_cmd - Sends the "SET_NFRAMES" command to dce fw.
 *
 * @d : Pointer to tegra_dce struct
 * @nframes : No. of frames
 *
 * Return : 0 if successful
 */
static int dce_send_set_nframes_cmd(struct tegra_dce *d, const u8 nframes)
{
	u32 val;
	int ret = 0;

	val = DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_SET_NFRAMES)
		| DCE_BOOT_CMD_PARM_SET(0U, nframes);

	ret = dce_mailbox_send_cmd_sync(d, val, DCE_MAILBOX_BOOT_INTERFACE);

	return ret;
}

/**
 * dce_send_set_frames_cmd - Sends the "SET_NFRAMES" command to dce fw.
 *
 * @d : Pointer to tegra_dce struct
 * @nframes : No. of frames
 *
 * Retrun : 0 if successful
 */
static int dce_send_set_fsize_cmd(struct tegra_dce *d, const u32 fsize)
{
	u32 val;
	int ret = 0;

	val = DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_SET_FSIZE)
		| DCE_BOOT_CMD_PARM_SET(0U, fsize);

	ret = dce_mailbox_send_cmd_sync(d, val, DCE_MAILBOX_BOOT_INTERFACE);

	return ret;
}

/**
 * dce_send_channel_int_cmd - Sends the "CHANNEL_INIT" command to dce fw
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : 0 if successful
 */
static int dce_send_lock_cmd(struct tegra_dce *d)
{
	u32 val;
	int ret = 0;

	val = DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_LOCK);

	ret = dce_mailbox_send_cmd_sync(d, val, DCE_MAILBOX_BOOT_INTERFACE);

	return ret;
}

/**
 * dce_bootstrap_send_ast_iova_info - Sends the iova info for AST
 * channel.
 *
 * @d - Pointer to struct tegra_dce.
 *
 * Return : O if successful
 */
static int dce_bootstrap_send_ast_iova_info(struct tegra_dce *d)
{
	u64 iova;
	u32 size;
	int ret = 0;

	ret = dce_ipc_get_region_iova_info(d, &iova, &size);
	if (ret) {
		dce_err(d, "Failed to get the iova info needed for ast config");
		goto err_sending;
	}

#define DCE_DATA_NBITS_SHIFT 20
	ret = dce_mailbox_send_cmd_sync(d,
			DCE_BOOT_CMD_SET_HILO(0U, 1U) |
			DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_SET_AST_LENGTH) |
			DCE_BOOT_CMD_PARM_SET(0U, size >> DCE_DATA_NBITS_SHIFT),
			DCE_MAILBOX_BOOT_INTERFACE);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd SET_AST_LENGTH(HI) failed");
		goto err_sending;
	}

	ret = dce_mailbox_send_cmd_sync(d,
			DCE_BOOT_CMD_SET_HILO(0U, 0U) |
			DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_SET_AST_LENGTH) |
			DCE_BOOT_CMD_PARM_SET(0U, size),
			DCE_MAILBOX_BOOT_INTERFACE);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd SET_AST_LENGTH(LO) failed");
		goto err_sending;
	}

	ret = dce_mailbox_send_cmd_sync(d,
			DCE_BOOT_CMD_SET_HILO(0U, 1U) |
			DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_SET_AST_IOVA) |
			DCE_BOOT_CMD_PARM_SET(0U, iova >> DCE_DATA_NBITS_SHIFT),
			DCE_MAILBOX_BOOT_INTERFACE);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd SET_AST_IOVA(HI) failed");
		goto err_sending;
	}
#undef DCE_DATA_NBITS_SHIFT

	ret = dce_mailbox_send_cmd_sync(d,
			DCE_BOOT_CMD_SET_HILO(0U, 0U) |
			DCE_BOOT_CMD_SET(0U, DCE_BOOT_CMD_SET_AST_IOVA) |
			DCE_BOOT_CMD_PARM_SET(0U, iova),
			DCE_MAILBOX_BOOT_INTERFACE);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd SET_AST_IOVA(LO) failed");
		goto err_sending;
	}

err_sending:
	return ret;
}

/**
 * dce_bootstrap_send_admin_ivc_info - Sends the ivc related info for admin
 * channel.
 *
 * @d - Pointer to struct tegra_dce.
 *
 * Return : O if successful
 */
static int dce_bootstrap_send_admin_ivc_info(struct tegra_dce *d)
{
	int ret = 0;
	u32 val = 0;

	struct dce_ipc_queue_info q_info;

	ret = dce_admin_get_ipc_channel_info(d, &q_info);
	if (ret) {
		dce_err(d, "Failed to get the admin ivc channel info");
		goto err_sending;
	}

	ret = dce_send_set_addr_read_cmd(d, (u64)(q_info.tx_iova));
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd set_addr_read failed");
		goto err_sending;
	}

	ret = dce_send_set_addr_write_cmd(d, (u64)(q_info.rx_iova));
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd set_addr_write failed");
		goto err_sending;
	}

	ret = dce_send_get_fsize_cmd(d);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd get_fsize failed");
		goto err_sending;
	}

	/**
	 * It's assummed here that no other command is sent in between.
	 */
	val = dce_mailbox_get_interface_status(d,
				DCE_MAILBOX_BOOT_INTERFACE);

	ret = dce_send_set_nframes_cmd(d, q_info.nframes);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd set_nframes failed");
		goto err_sending;
	}

	ret = dce_send_set_fsize_cmd(d, q_info.frame_sz);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd set_fsize failed");
		goto err_sending;
	}

err_sending:
	return ret;
}

/**
 * dce_start_bootstrap_flow - Starts sending the boostrap cmds to
 *					dce fw in the required sequence.
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : 0 if successful
 */
int dce_start_bootstrap_flow(struct tegra_dce *d)
{
	u32 val;
	int ret = 0;

	d->boot_status |= DCE_FW_BOOTSTRAP_START;
	ret = dce_send_version_cmd(d);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd VERSION failed");
		goto err_sending;
	}
	/**
	 * It's assummed here that no other command is sent in between.
	 */
	val = dce_mailbox_get_interface_status(d,
				DCE_MAILBOX_BOOT_INTERFACE);

	ret = dce_send_set_sid_cmd(d);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd set_sid failed");
		goto err_sending;
	}

	ret = dce_bootstrap_send_ast_iova_info(d);
	if (ret) {
		dce_err(d, "Sending of iova info failed");
		goto err_sending;
	}

	ret = dce_bootstrap_send_admin_ivc_info(d);
	if (ret) {
		dce_err(d, "Sending of ivc channel info failedbootstrap cmd set_sid failed");
		goto err_sending;
	}

	ret = dce_send_channel_int_cmd(d);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd channel_int failed");
		goto err_sending;
	}

	ret = dce_send_lock_cmd(d);
	if (ret) {
		dce_err(d, "Sending of bootstrap cmd lock failed");
		goto err_sending;
	}

	d->boot_status |= DCE_FW_BOOTSTRAP_DONE;
	return 0;

err_sending:
	dce_err(d, "Bootstrap process failed");
	d->boot_status |= DCE_FW_BOOTSTRAP_FAILED;
	return ret;
}
