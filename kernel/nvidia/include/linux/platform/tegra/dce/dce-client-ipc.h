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

#ifndef TEGRA_DCE_CLIENT_IPC_H
#define TEGRA_DCE_CLIENT_IPC_H

#define DCE_CLIENT_IPC_TYPE_CPU_RM		0U
#define DCE_CLIENT_IPC_TYPE_HDCP_KMD		1U
#define DCE_CLIENT_IPC_TYPE_RM_EVENT		2U
#define DCE_CLIENT_IPC_TYPE_MAX			3U

#define DCE_CLIENT_MAX_IPC_MSG_SIZE		4096

/**
 * struct dce_ipc_message - Contains necessary info for an ipc msg.
 *
 * @tx : transmit message info
 * @rx : receive message info
 */
struct dce_ipc_message {
	struct {
		void *data;
		size_t size;
	} tx;
	struct {
		void *data;
		size_t size;
	} rx;
};

/*
 * tegra_dce_client_ipc_callback_t - callback function to notify the
 * client when the CPU Driver receives an IPC from DCE for the client.
 *
 * @client_id: id that represents the corresponding client.
 * @interface_type: Interface on which the IPC was received.
 * @msg_length: Length of the message.
 * @msg_data: Message data.
 * @usr_ctx: Any user context if present.
 */
typedef void (*tegra_dce_client_ipc_callback_t)(u32 handle,
	      u32 interface_type, u32 msg_length,
	      void *msg_data, void *usr_ctx);

/*
 * tegra_dce_register_ipc_client() - used by clients to register with dce driver
 * @interface_type: Interface for which this client is expected to send rpcs and
 * receive callbacks.
 * @callback_fn: callback function to be called by DCE driver on receiving IPCs
 * from DCE on this interface.
 * @usr_ctx: Any user context if present.
 *
 * Return: valid client_id if no errors else corresponding error value.
 */
int tegra_dce_register_ipc_client(u32 interface_type,
		tegra_dce_client_ipc_callback_t callback_fn,
		void *usr_ctx, u32 *handlep);

/*
 * tegra_dce_unregister_client() - used by clients to unregister to dce driver
 * @client_id : ointer to client's data
 *
 * Return: 0 if no errors else corresponding error value.
 */
int tegra_dce_unregister_ipc_client(u32 handle);

/*
 * tegra_dce_client_ipc_send_recv() - used by clients to send rpcs to dce
 * @client_id : client_id registered with dce driver
 * @msg : message to be sent and received
 *
 * Return: 0 if no errors else corresponding error value.
 */
int tegra_dce_client_ipc_send_recv(u32 handle, struct dce_ipc_message *msg);

#endif
