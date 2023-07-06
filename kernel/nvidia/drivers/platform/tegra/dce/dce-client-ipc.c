/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
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
#include <dce-ipc.h>
#include <dce-util-common.h>
#include <dce-client-ipc-internal.h>

#define DCE_IPC_HANDLES_MAX 6U
#define DCE_CLIENT_IPC_HANDLE_INVALID 0U
#define DCE_CLIENT_IPC_HANDLE_VALID ((u32)BIT(31))

struct tegra_dce_client_ipc client_handles[DCE_CLIENT_IPC_TYPE_MAX];

static uint32_t dce_interface_type_map[DCE_CLIENT_IPC_TYPE_MAX] = {
	[DCE_CLIENT_IPC_TYPE_CPU_RM] = DCE_IPC_TYPE_DISPRM,
	[DCE_CLIENT_IPC_TYPE_HDCP_KMD] = DCE_IPC_TYPE_HDCP,
	[DCE_CLIENT_IPC_TYPE_RM_EVENT] = DCE_IPC_TYPE_RM_NOTIFY,
};

static void dce_client_process_event_ipc(struct tegra_dce *d,
					 struct tegra_dce_client_ipc *cl);

static inline uint32_t dce_client_get_type(uint32_t int_type)
{
	uint32_t lc = 0;

	for (lc = 0; lc < DCE_CLIENT_IPC_TYPE_MAX; lc++)
		if (dce_interface_type_map[lc] == int_type)
			break;

	return lc;
}

static inline u32 client_handle_to_index(u32 handle)
{
	return (u32)(handle & ~DCE_CLIENT_IPC_HANDLE_VALID);
}

static inline bool is_client_handle_valid(u32 handle)
{
	bool valid = false;

	if ((handle & DCE_CLIENT_IPC_HANDLE_VALID) == 0U)
		goto out;

	if (client_handle_to_index(handle) >= DCE_CLIENT_IPC_TYPE_MAX)
		goto out;

	valid = true;

out:
	return valid;
}

struct tegra_dce_client_ipc *dce_client_ipc_lookup_handle(u32 handle)
{
	struct tegra_dce_client_ipc *cl = NULL;

	if (!is_client_handle_valid(handle))
		goto out;

	cl = &client_handles[client_handle_to_index(handle)];

out:
	return cl;
}


static int dce_client_ipc_handle_alloc(u32 *handle)
{
	u32 index;
	int ret = -1;

	if (handle == NULL)
		return ret;

	for (index = 0; index < DCE_CLIENT_IPC_TYPE_MAX; index++) {
		if (client_handles[index].valid == false) {
			client_handles[index].valid = true;
			*handle = (u32)(index | DCE_CLIENT_IPC_HANDLE_VALID);
			ret = 0;
			break;
		}
	}

	return ret;
}

static int dce_client_ipc_handle_free(u32 handle)
{
	struct tegra_dce *d;
	struct tegra_dce_client_ipc *cl;

	if (!is_client_handle_valid(handle))
		return -EINVAL;

	cl = &client_handles[client_handle_to_index(handle)];

	if (cl->valid == false)
		return -EINVAL;

	d = cl->d;
	d->d_clients[cl->type] = NULL;
	memset(cl, 0, sizeof(struct tegra_dce_client_ipc));

	return 0;
}

static void dce_client_async_event_work(struct work_struct *data)
{
	struct tegra_dce_client_ipc *cl;
	struct dce_async_work *work = container_of(data, struct dce_async_work,
						   async_event_work);
	struct tegra_dce *d = work->d;

	cl = d->d_clients[DCE_CLIENT_IPC_TYPE_RM_EVENT];

	dce_client_process_event_ipc(d, cl);
	atomic_set(&work->in_use, 0);
}

int tegra_dce_register_ipc_client(u32 type,
		tegra_dce_client_ipc_callback_t callback_fn,
		void *data, u32 *handlep)
{
	int ret;
	uint32_t int_type;
	struct tegra_dce *d = NULL;
	struct tegra_dce_client_ipc *cl;
	u32 handle = DCE_CLIENT_IPC_HANDLE_INVALID;

	if (handlep == NULL) {
		dce_err(d, "Invalid handle pointer");
		ret = -EINVAL;
		goto end;
	}

	if (type >= DCE_CLIENT_IPC_TYPE_MAX) {
		dce_err(d, "Failed to retrieve client info for type: [%u]", type);
		ret = -EINVAL;
		goto end;
	}

	int_type = dce_interface_type_map[type];

	d = dce_ipc_get_dce_from_ch(int_type);
	if (d == NULL) {
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Wait for bootstrapping to complete before client IPC registration
	 */
#define DCE_IPC_REGISTER_BOOT_WAIT	(30U * 1000)
	ret = DCE_COND_WAIT_INTERRUPTIBLE_TIMEOUT(&d->dce_bootstrap_done,
						  dce_is_bootstrap_done(d),
						  DCE_IPC_REGISTER_BOOT_WAIT);
	if (ret) {
		dce_info(d, "dce boot wait failed (%d)\n", ret);
		goto out;
	}

	ret = dce_client_ipc_handle_alloc(&handle);
	if (ret)
		goto out;

	cl = &client_handles[client_handle_to_index(handle)];

	cl->d = d;
	cl->type = type;
	cl->data = data;
	cl->handle = handle;
	cl->int_type = int_type;
	cl->callback_fn = callback_fn;
	atomic_set(&cl->complete, 0);

	ret = dce_cond_init(&cl->recv_wait);
	if (ret) {
		dce_err(d, "dce condition initialization failed for int_type: [%u]",
			int_type);
		goto out;
	}

	d->d_clients[type] = cl;

out:
	if (ret != 0) {
		dce_client_ipc_handle_free(handle);
		handle = DCE_CLIENT_IPC_HANDLE_INVALID;
	}

	*handlep = handle;
end:
	return ret;
}
EXPORT_SYMBOL(tegra_dce_register_ipc_client);

int tegra_dce_unregister_ipc_client(u32 handle)
{
	struct tegra_dce_client_ipc *cl;

	cl = &client_handles[client_handle_to_index(handle)];
	dce_cond_destroy(&cl->recv_wait);

	return dce_client_ipc_handle_free(handle);
}
EXPORT_SYMBOL(tegra_dce_unregister_ipc_client);

int tegra_dce_client_ipc_send_recv(u32 handle, struct dce_ipc_message *msg)
{
	int ret;
	struct tegra_dce_client_ipc *cl;

	if (msg == NULL) {
		ret = -1;
		goto out;
	}

	cl = dce_client_ipc_lookup_handle(handle);
	if (cl == NULL) {
		ret = -1;
		goto out;
	}

	ret = dce_ipc_send_message_sync(cl->d, cl->int_type, msg);

out:
	return ret;
}
EXPORT_SYMBOL(tegra_dce_client_ipc_send_recv);

int dce_client_init(struct tegra_dce *d)
{
	int ret = 0;
	uint8_t i;
	struct tegra_dce_async_ipc_info *d_aipc = &d->d_async_ipc;

	d_aipc->async_event_wq =
		create_singlethread_workqueue("dce-async-ipc-wq");

	for (i = 0; i < DCE_MAX_ASYNC_WORK; i++) {
		struct dce_async_work *d_work = &d_aipc->work[i];

		INIT_WORK(&d_work->async_event_work,
			  dce_client_async_event_work);
		d_work->d = d;
		atomic_set(&d_work->in_use, 0);
	}

	return ret;
}

void dce_client_deinit(struct tegra_dce *d)
{
	struct tegra_dce_async_ipc_info *d_aipc = &d->d_async_ipc;

	flush_workqueue(d_aipc->async_event_wq);
	destroy_workqueue(d_aipc->async_event_wq);
}

int dce_client_ipc_wait(struct tegra_dce *d, u32 int_type)
{
	uint32_t type;
	struct tegra_dce_client_ipc *cl;

	type = dce_client_get_type(int_type);
	if (type >= DCE_CLIENT_IPC_TYPE_MAX) {
		dce_err(d, "Failed to retrieve client info for int_type: [%d]",
			int_type);
		return -EINVAL;
	}

	cl = d->d_clients[type];
	if ((cl == NULL) || (cl->int_type != int_type)) {
		dce_err(d, "Failed to retrieve client info for int_type: [%d]",
			int_type);
		return -EINVAL;
	}

retry_wait:
	DCE_COND_WAIT_INTERRUPTIBLE(&cl->recv_wait,
			atomic_read(&cl->complete) == 1);
	if (atomic_read(&cl->complete) != 1)
		goto retry_wait;

	atomic_set(&cl->complete, 0);

	return 0;
}

static void dce_client_process_event_ipc(struct tegra_dce *d,
					 struct tegra_dce_client_ipc *cl)
{
	void *msg_data = NULL;
	u32 msg_length;
	int ret = 0;

	if ((cl == NULL) || (cl->callback_fn == NULL)) {
		dce_err(d, "Invalid arg tegra_dce_client_ipc");
		return;
	}

	if (cl->type != DCE_CLIENT_IPC_TYPE_RM_EVENT) {
		dce_err(d, "Invalid arg for DCE_CLIENT_IPC_TYPE_RM_EVENT type:[%u]", cl->type);
		return;
	}

	msg_data = dce_kzalloc(d, DCE_CLIENT_MAX_IPC_MSG_SIZE, false);
	if (msg_data == NULL) {
		dce_err(d, "Could not allocate msg read buffer");
		goto done;
	}
	msg_length = DCE_CLIENT_MAX_IPC_MSG_SIZE;

	while (dce_ipc_is_data_available(d, cl->int_type)) {
		ret = dce_ipc_read_message(d, cl->int_type, msg_data, msg_length);
		if (ret) {
			dce_info(d, "Error in reading DCE msg for ch_type [%d]",
				cl->int_type);
			goto done;
		}

		cl->callback_fn(cl->handle, cl->type, msg_length, msg_data, cl->data);
	}

done:
	if (msg_data)
		dce_kfree(d, msg_data);
}

static void dce_client_schedule_event_work(struct tegra_dce *d)
{
	struct tegra_dce_async_ipc_info *async_work_info = &d->d_async_ipc;
	uint8_t i;

	for (i = 0; i < DCE_MAX_ASYNC_WORK; i++) {
		struct dce_async_work *d_work = &async_work_info->work[i];

		if (atomic_add_unless(&d_work->in_use, 1, 1) > 0) {
			queue_work(async_work_info->async_event_wq,
				   &d_work->async_event_work);
			break;
		}
	}

	if (i == DCE_MAX_ASYNC_WORK)
		dce_err(d, "Failed to schedule Async event Queue Full!");
}

void dce_client_ipc_wakeup(struct tegra_dce *d, u32 ch_type)
{
	uint32_t type;
	struct tegra_dce_client_ipc *cl;

	type = dce_client_get_type(ch_type);
	if (type == DCE_CLIENT_IPC_TYPE_MAX) {
		dce_err(d, "Failed to retrieve client info for ch_type: [%d]",
			ch_type);
		return;
	}

	cl = d->d_clients[type];
	if ((cl == NULL) || (cl->int_type != ch_type)) {
		dce_err(d, "Failed to retrieve client info for ch_type: [%d]",
			ch_type);
		return;
	}

	if (type == DCE_CLIENT_IPC_TYPE_RM_EVENT)
		return dce_client_schedule_event_work(d);

	atomic_set(&cl->complete, 1);
	dce_cond_signal_interruptible(&cl->recv_wait);
}
