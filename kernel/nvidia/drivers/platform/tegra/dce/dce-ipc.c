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

#include <linux/version.h>

#include <dce.h>
#include <dce-ipc.h>
#include <dce-util-common.h>
#include <interface/dce-interface.h>
#include <interface/dce-ipc-header.h>

#define CREATE_TRACE_POINTS
#include <trace/events/dce_events.h>

struct dce_ipc_channel ivc_channels[DCE_IPC_CH_KMD_TYPE_MAX] = {
	[DCE_IPC_CH_KMD_TYPE_ADMIN] = {
		.flags = DCE_IPC_CHANNEL_VALID
			 | DCE_IPC_CHANNEL_MSG_HEADER,
		.ch_type = DCE_IPC_CH_KMD_TYPE_ADMIN,
		.ipc_type = DCE_IPC_TYPE_ADMIN,
		.signal = {
			.to_d = {
				.type = DCE_IPC_SIGNAL_MAILBOX,
				.sema_num = DCE_NUM_SEMA_REGS,
				.sema_bit = 0U,
				.form = {
					.mbox = {
						.mb_type = DCE_MAILBOX_ADMIN_INTERFACE,
						.mb_num = DCE_MBOX_TO_DCE_ADMIN,
					},
				},
				.signal = NULL,
				.next = NULL,
			},
			.from_d = {
				.type = DCE_IPC_SIGNAL_MAILBOX,
				.sema_num = DCE_NUM_SEMA_REGS,
				.sema_bit = 0U,
				.form = {
					.mbox = {
						.mb_type = DCE_MAILBOX_ADMIN_INTERFACE,
						.mb_num = DCE_MBOX_FROM_DCE_ADMIN,
					},
				},
				.signal = NULL,
				.next = NULL,
			},
		},
		.q_info = {
			.nframes = DCE_ADMIN_CMD_MAX_NFRAMES,
			.frame_sz = DCE_ADMIN_CMD_MAX_FSIZE,
		},
	},
	[DCE_IPC_CH_KMD_TYPE_RM] = {
		.flags = DCE_IPC_CHANNEL_VALID
			 | DCE_IPC_CHANNEL_MSG_HEADER,
		.ch_type = DCE_IPC_CH_KMD_TYPE_RM,
		.ipc_type = DCE_IPC_TYPE_DISPRM,
		.signal = {
			.to_d = {
				.type = DCE_IPC_SIGNAL_MAILBOX,
				.sema_num = DCE_NUM_SEMA_REGS,
				.sema_bit = 0U,
				.form = {
					.mbox = {
						.mb_type = DCE_MAILBOX_DISPRM_INTERFACE,
						.mb_num = DCE_MBOX_TO_DCE_RM,
					},
				},
				.signal = NULL,
				.next = NULL,
			},
			.from_d = {
				.type = DCE_IPC_SIGNAL_MAILBOX,
				.sema_num = DCE_NUM_SEMA_REGS,
				.sema_bit = 0U,
				.form = {
					.mbox = {
						.mb_type = DCE_MAILBOX_DISPRM_INTERFACE,
						.mb_num = DCE_MBOX_FROM_DCE_RM,
					},
				},
				.signal = NULL,
				.next = NULL,
			},
		},
		.q_info = {
			.nframes = DCE_DISPRM_CMD_MAX_NFRAMES,
			.frame_sz = DCE_DISPRM_CMD_MAX_FSIZE,
		},
	},
	[DCE_IPC_CH_KMD_TYPE_RM_NOTIFY] = {
		.flags = DCE_IPC_CHANNEL_VALID
			 | DCE_IPC_CHANNEL_MSG_HEADER,
		.ch_type = DCE_IPC_CH_KMD_TYPE_RM_NOTIFY,
		.ipc_type = DCE_IPC_TYPE_RM_NOTIFY,
		.signal = {
			.to_d = {
				.type = DCE_IPC_SIGNAL_MAILBOX,
				.sema_num = DCE_NUM_SEMA_REGS,
				.sema_bit = 0U,
				.form = {
					.mbox = {
						.mb_type = DCE_MAILBOX_DISPRM_NOTIFY_INTERFACE,
						.mb_num = DCE_MBOX_FROM_DCE_RM_EVENT_NOTIFY,
					},
				},
				.signal = NULL,
				.next = NULL,
			},
			.from_d = {
				.type = DCE_IPC_SIGNAL_MAILBOX,
				.sema_num = DCE_NUM_SEMA_REGS,
				.sema_bit = 0U,
				.form = {
					.mbox = {
						.mb_type = DCE_MAILBOX_DISPRM_NOTIFY_INTERFACE,
						.mb_num = DCE_MBOX_TO_DCE_RM_EVENT_NOTIFY,
					},
				},
				.signal = NULL,
				.next = NULL,
			},
		},
		.q_info = {
			.nframes = DCE_DISPRM_EVENT_NOTIFY_CMD_MAX_NFRAMES,
			.frame_sz = DCE_DISPRM_EVENT_NOTIFY_CMD_MAX_FSIZE,
		},
	},
};

/**
 * dce_ipc_allocate_region - Allocates IPC region for IVC
 *
 * @ch : Pointer to the pertinent dce_ipc_channel.
 * @q_size : IVC queue size.
 *
 * Return : 0 if successful
 */
int dce_ipc_allocate_region(struct tegra_dce *d)
{
	unsigned long tot_q_sz;
	unsigned long tot_ivc_q_sz;
	struct device *dev;
	struct dce_ipc_region *region;

	dev = dev_from_dce(d);
	region = &d->d_ipc.region;

	tot_q_sz = ((DCE_ADMIN_CMD_MAX_NFRAMES *
		     tegra_ivc_align(DCE_ADMIN_CMD_MAX_FSIZE) * 2) +
		    (DCE_DISPRM_CMD_MAX_NFRAMES	*
		     tegra_ivc_align(DCE_DISPRM_CMD_MAX_FSIZE) * 2) +
		    (DCE_ADMIN_CMD_MAX_NFRAMES *
		     tegra_ivc_align(DCE_ADMIN_CMD_CHAN_FSIZE) * 2) +
		    (DCE_DISPRM_EVENT_NOTIFY_CMD_MAX_NFRAMES *
		     tegra_ivc_align(DCE_DISPRM_EVENT_NOTIFY_CMD_MAX_FSIZE) * 2)
		   );

	tot_ivc_q_sz = tegra_ivc_total_queue_size(tot_q_sz);
	region->size = dce_get_nxt_pow_of_2(&tot_ivc_q_sz, 32);
	region->base = dma_alloc_coherent(dev, region->size,
			&region->iova, GFP_KERNEL | __GFP_ZERO);
	if (!region->base)
		return -ENOMEM;

	region->s_offset = 0;

	return 0;
}

/**
 * dce_ipc_free_region - Frees up the IPC region for IVC
 *
 * @d : Pointer to the tegra_dce struct.
 *
 * Return : Void
 */
void dce_ipc_free_region(struct tegra_dce *d)
{
	struct device *dev;
	struct dce_ipc_region *region;

	dev = dev_from_dce(d);
	region = &d->d_ipc.region;

	dma_free_coherent(dev, region->size,
		(void *)region->base, region->iova);

	region->s_offset = 0;
}

/**
 * dce_ipc_signal_target - Generic function to signal target.
 *
 * @d_ivc : Pointer to struct tegra_ivc.
 *
 * Do not take a channel lock here.
 *
 * Return : Void.
 */
static void dce_ipc_signal_target(struct tegra_ivc *ivc, void *data)
{
}

static int _dce_ipc_wait(struct tegra_dce *d, u32 w_type, u32 ch_type)
{
	int ret = 0;
	struct dce_ipc_channel *ch;

	if (ch_type >= DCE_IPC_CH_KMD_TYPE_MAX) {
		dce_err(d, "Invalid Channel Type : [%d]", ch_type);
		return -EINVAL;
	}

	ch = d->d_ipc.ch[ch_type];
	if (ch == NULL) {
		dce_err(d, "Invalid Channel Data for type : [%d]", ch_type);
		ret = -EINVAL;
		goto out;
	}

	ch->w_type = w_type;

	dce_mutex_unlock(&ch->lock);

	if (ch_type == DCE_IPC_TYPE_ADMIN)
		ret = dce_admin_ipc_wait(d, w_type);
	else
		ret = dce_client_ipc_wait(d, ch_type);

	dce_mutex_lock(&ch->lock);

	ch->w_type = DCE_IPC_WAIT_TYPE_INVALID;

out:
	return ret;
}

u32 dce_ipc_get_cur_wait_type(struct tegra_dce *d, u32 ch_type)
{
	uint32_t w_type;
	struct dce_ipc_channel *ch;

	if (ch_type >= DCE_IPC_CH_KMD_TYPE_MAX) {
		dce_err(d, "Invalid Channel Type : [%d]", ch_type);
		return -EINVAL;
	}

	ch = d->d_ipc.ch[ch_type];
	if (ch == NULL) {
		dce_err(d, "Invalid Channel Data for type : [%d]", ch_type);
		return -EINVAL;
	}

	dce_mutex_lock(&ch->lock);

	w_type = ch->w_type;

	dce_mutex_unlock(&ch->lock);

	return w_type;
}

/**
 * dce_ipc_channel_init - Initializes the underlying IPC channel to
 *				be used for all bi-directional messaging.
 * @d : Pointer to struct tegra_dce.
 * @type : Type of interface for which this channel is needed.
 *
 * Return : 0 if successful.
 */
int dce_ipc_channel_init(struct tegra_dce *d, u32 ch_type)
{
	u32 q_sz;
	u32 msg_sz;
	int ret = 0;
	struct device *dev;
	struct dce_ipc_region *r;
	struct dce_ipc_channel *ch;
	struct dce_ipc_queue_info *q_info;
#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
	struct iosys_map rx, tx;
#endif

	if (ch_type >= DCE_IPC_CH_KMD_TYPE_MAX) {
		dce_err(d, "Invalid ivc channel ch_type : [%d]", ch_type);
		ret = -EINVAL;
		goto out;
	}

	ch = &ivc_channels[ch_type];
	if (!ch) {
		dce_err(d, "Invalid ivc channel for this ch_type : [%d]",
			ch_type);
		ret = -ENOMEM;
		goto out;
	}

	ret = dce_mutex_init(&ch->lock);
	if (ret) {
		dce_err(d, "dce lock initialization failed for mailbox");
		goto out;
	}

	dce_mutex_lock(&ch->lock);

	if ((ch->flags & DCE_IPC_CHANNEL_VALID) == 0U) {
		dce_info(d, "Invalid Channel State [0x%x] for ch_type [%d]",
		ch->flags, ch_type);
		goto out_lock_destroy;
	}

	ch->d = d;

	ret = dce_ipc_init_signaling(d, ch);
	if (ret) {
		dce_err(d, "Signaling init failed");
		goto out_lock_destroy;
	}

	q_info = &ch->q_info;
	msg_sz = tegra_ivc_align(q_info->frame_sz);
	q_sz = tegra_ivc_total_queue_size(msg_sz * q_info->nframes);

	r = &d->d_ipc.region;
	if (!r->base) {
		ret = -ENOMEM;
		goto out_lock_destroy;
	}

	dev = dev_from_dce(d);

#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
	iosys_map_set_vaddr_iomem(&rx, r->base + r->s_offset);
	iosys_map_set_vaddr_iomem(&tx, r->base + r->s_offset + q_sz);

	ret = tegra_ivc_init(&ch->d_ivc, NULL, &rx, r->iova + r->s_offset, &tx,
			r->iova + r->s_offset + q_sz, q_info->nframes, msg_sz,
			dce_ipc_signal_target, NULL);
#else
	ret = tegra_ivc_init(&ch->d_ivc, NULL, r->base + r->s_offset,
			r->iova + r->s_offset, r->base + r->s_offset + q_sz,
			r->iova + r->s_offset + q_sz, q_info->nframes, msg_sz,
			dce_ipc_signal_target, NULL);
#endif
	if (ret) {
		dce_err(d, "IVC creation failed");
		goto out_lock_destroy;
	}

	ch->flags |= DCE_IPC_CHANNEL_INITIALIZED;

	q_info->rx_iova = r->iova + r->s_offset;
	q_info->tx_iova = r->iova + r->s_offset + q_sz;

	trace_ivc_channel_init_complete(d, ch);

	d->d_ipc.ch[ch_type] = ch;
	r->s_offset += (2 * q_sz);

out_lock_destroy:
	dce_mutex_unlock(&ch->lock);
	if (ret)
		dce_mutex_destroy(&ch->lock);
out:
	return ret;
}

/**
 * dce_ivc_channel_deinit - Releases resources for a ivc channel
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Channel Id.
 */
void dce_ipc_channel_deinit(struct tegra_dce *d, u32 ch_type)
{
	struct dce_ipc_channel *ch = d->d_ipc.ch[ch_type];

	if (ch == NULL || (ch->flags & DCE_IPC_CHANNEL_INITIALIZED) == 0U) {
		dce_info(d, "Invalid IVC Channel [%d]", ch_type);
		return;
	}

	dce_mutex_lock(&ch->lock);

	dce_ipc_deinit_signaling(d, ch);

	ch->flags &= ~DCE_IPC_CHANNEL_INITIALIZED;
	ch->flags &= ~DCE_IPC_CHANNEL_SYNCED;

	d->d_ipc.ch[ch_type] = NULL;

	dce_mutex_unlock(&ch->lock);

	dce_mutex_destroy(&ch->lock);

}

struct tegra_dce *dce_ipc_get_dce_from_ch(u32 ch_type)
{
	struct tegra_dce *d = NULL;
	struct dce_ipc_channel *ch = NULL;

	if (ch_type >= DCE_IPC_CH_KMD_TYPE_MAX)
		goto out;

	ch = &ivc_channels[ch_type];

	dce_mutex_lock(&ch->lock);

	d = ch->d;

	dce_mutex_unlock(&ch->lock);

out:
	return d;
}

/**
 * dce_ipc_channel_ready - Checks if channel is ready to use
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Channel Id.
 *
 * Return : true if channel ready to use.
 */
bool dce_ipc_channel_is_ready(struct tegra_dce *d, u32 ch_type)
{
	bool is_est;

	struct dce_ipc_channel *ch = d->d_ipc.ch[ch_type];

	dce_mutex_lock(&ch->lock);

	is_est = (tegra_ivc_notified(&ch->d_ivc) ? false : true);

	ch->signal.notify(d, &ch->signal.to_d);

	dce_mutex_unlock(&ch->lock);

	return is_est;
}

/**
 * dce_ipc_channel_synced - Checks if channel is in synced state
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Channel Id.
 *
 * Return : true if channel is in synced state.
 */
bool dce_ipc_channel_is_synced(struct tegra_dce *d, u32 ch_type)
{
	bool ret;

	struct dce_ipc_channel *ch = d->d_ipc.ch[ch_type];

	dce_mutex_lock(&ch->lock);

	ret = (ch->flags & DCE_IPC_CHANNEL_SYNCED) ? true : false;

	dce_mutex_unlock(&ch->lock);

	return ret;
}

/**
 * dce_ipc_channel_reset - Resets the channel and completes
 *				the handshake with the remote.
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Channel Id.
 *
 * Return : void
 */
void dce_ipc_channel_reset(struct tegra_dce *d, u32 ch_type)
{
	struct dce_ipc_channel *ch = d->d_ipc.ch[ch_type];

	dce_mutex_lock(&ch->lock);

	tegra_ivc_reset(&ch->d_ivc);

	trace_ivc_channel_reset_triggered(d, ch);

	ch->flags &= ~DCE_IPC_CHANNEL_SYNCED;

	ch->signal.notify(d, &ch->signal.to_d);

	dce_mutex_unlock(&ch->lock);

	do {
		if (dce_ipc_channel_is_ready(d, ch_type) == true)
			break;

	} while (true);

	dce_mutex_lock(&ch->lock);

	ch->flags |= DCE_IPC_CHANNEL_SYNCED;

	trace_ivc_channel_reset_complete(d, ch);

	dce_mutex_unlock(&ch->lock);
}

/**
 * dce_ipc_get_next_write_buff - waits for the next write frame.
 *
 * @ch : Pointer to the pertinent channel.
 *
 * Return : 0 if successful
 */
static int _dce_ipc_get_next_write_buff(struct dce_ipc_channel *ch)
{
#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
	int err;

	err = tegra_ivc_write_get_next_frame(&ch->d_ivc, &ch->obuff);
	if (err) {
		iosys_map_clear(&ch->obuff);
		return err;
	}
#else
	void *frame = NULL;

	frame = tegra_ivc_write_get_next_frame(&ch->d_ivc);

	if (IS_ERR(frame)) {
		ch->obuff = NULL;
		return -ENOMEM;
	}

	ch->obuff = frame;
#endif
	return 0;
}

/**
 * dce_ipc_write_channel - Writes to an ivc channel.
 *
 * @ch : Pointer to the pertinent channel.
 * @data : Pointer to the data to be written.
 * @size : Size of the data to be written.
 *
 * Return : 0 if successful.
 */
static int _dce_ipc_write_channel(struct dce_ipc_channel *ch,
		const void *data, size_t size)
{
	struct dce_ipc_header *hdr;

	/**
	 * Add actual length information to the top
	 * of the IVC frame
	 */

#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
	if ((ch->flags & DCE_IPC_CHANNEL_MSG_HEADER) != 0U) {
		iosys_map_wr_field(&ch->obuff, 0, struct dce_ipc_header, length,
				   size);
		iosys_map_incr(&ch->obuff, sizeof(*hdr));
	}

	if (data && size > 0)
		iosys_map_memcpy_to(&ch->obuff, 0, data, size);
#else
	if ((ch->flags & DCE_IPC_CHANNEL_MSG_HEADER) != 0U) {
		hdr = (struct dce_ipc_header *)ch->obuff;
		hdr->length = (uint32_t)size;
		ch->obuff = (void *)(hdr + 1U);
	}

	if (data && size > 0)
		memcpy(ch->obuff, data, size);
#endif

	return tegra_ivc_write_advance(&ch->d_ivc);
}

/**
 * dce_ipc_send_message - Sends messages over ipc.
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Channel Id.
 * @data : Pointer to the data to be written.
 * @size : Size of the data to be written.
 *
 * Return : 0 if successful.
 */
int dce_ipc_send_message(struct tegra_dce *d, u32 ch_type,
		const void *data, size_t size)
{
	int ret = 0;
	struct dce_ipc_channel *ch
		= d->d_ipc.ch[ch_type];

	dce_mutex_lock(&ch->lock);

	trace_ivc_send_req_received(d, ch);

	ret = _dce_ipc_get_next_write_buff(ch);
	if (ret) {
		dce_err(ch->d, "Error getting next free buf to write");
		goto out;
	}

	ret = _dce_ipc_write_channel(ch, data, size);
	if (ret) {
		dce_err(ch->d, "Error writing to channel");
		goto out;
	}

	ch->signal.notify(d, &ch->signal.to_d);

	trace_ivc_send_complete(d, ch);

out:
	dce_mutex_unlock(&ch->lock);

	return ret;
}

/**
 * dce_ipc_get_next_read_buff - waits for the next write frame.
 *
 * @ch : Pointer to the pertinent channel.
 *
 * Return : 0 if successful
 */
static int _dce_ipc_get_next_read_buff(struct dce_ipc_channel *ch)
{
#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
	int err;

	err = tegra_ivc_read_get_next_frame(&ch->d_ivc, &ch->ibuff);
	if (err) {
		iosys_map_clear(&ch->ibuff);
		return err;
	}
#else
	void *frame = NULL;

	frame = tegra_ivc_read_get_next_frame(&ch->d_ivc);

	if (IS_ERR(frame)) {
		ch->ibuff = NULL;
		return -ENOMEM;
	}

	ch->ibuff = frame;
#endif
	return 0;
}

/**
 * dce_ipc_read_channel - Writes to an ivc channel.
 *
 * @ch : Pointer to the pertinent channel.
 * @data : Pointer to the data to be read.
 * @size : Size of the data to be read.
 *
 * Return : 0 if successful.
 */
static int _dce_ipc_read_channel(struct dce_ipc_channel *ch,
		void *data, size_t size)
{
	struct dce_ipc_header *hdr;

	/**
	 * Get actual length information from the top
	 * of the IVC frame
	 */
#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
	if ((ch->flags & DCE_IPC_CHANNEL_MSG_HEADER) != 0U) {
		iosys_map_wr_field(&ch->ibuff, 0, struct dce_ipc_header, length,
				   size);
		iosys_map_incr(&ch->ibuff, sizeof(*hdr));
	}

	if (data && size > 0)
		iosys_map_memcpy_from(data, &ch->ibuff, 0, size);
#else
	if ((ch->flags & DCE_IPC_CHANNEL_MSG_HEADER) != 0U) {
		hdr = (struct dce_ipc_header *)ch->ibuff;
		size = (size_t)(hdr->length);
		ch->ibuff = (void *)(hdr + 1U);
	}

	if (data && size > 0)
		memcpy(data, ch->ibuff, size);
#endif

	return tegra_ivc_read_advance(&ch->d_ivc);
}

/**
 * dce_ipc_read_message - Reads messages over ipc.
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Channel Id.
 * @data : Pointer to the data to be read.
 * @size : Size of the data to be read.
 *
 * Return : 0 if successful.
 */
int dce_ipc_read_message(struct tegra_dce *d, u32 ch_type,
		void *data, size_t size)
{
	int ret = 0;
	struct dce_ipc_channel *ch = d->d_ipc.ch[ch_type];

	dce_mutex_lock(&ch->lock);

	trace_ivc_receive_req_received(d, ch);

	ret = _dce_ipc_get_next_read_buff(ch);
	if (ret) {
		dce_debug(ch->d, "No Msg to read");
		goto out;
	}

	ret = _dce_ipc_read_channel(ch, data, size);
	if (ret) {
		dce_err(ch->d, "Error reading from channel");
		goto out;
	}

	trace_ivc_receive_req_complete(d, ch);

out:
	dce_mutex_unlock(&ch->lock);
	return ret;
}

/**
 * dce_ipc_send_message_sync - Sends messages on a channel
 *				synchronously and waits for an ack.
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Channel Id.
 * @msg : Pointer to the message to be sent/received.
 *
 * Return : 0 if successful
 */
int dce_ipc_send_message_sync(struct tegra_dce *d, u32 ch_type,
				struct dce_ipc_message *msg)
{
	int ret = 0;
	struct dce_ipc_channel *ch = d->d_ipc.ch[ch_type];

	ret = dce_ipc_send_message(d, ch_type, msg->tx.data, msg->tx.size);
	if (ret) {
		dce_err(ch->d, "Error in sending message to DCE");
		goto done;
	}

	dce_mutex_lock(&ch->lock);
	ret = _dce_ipc_wait(ch->d, DCE_IPC_WAIT_TYPE_RPC, ch_type);
	dce_mutex_unlock(&ch->lock);
	if (ret) {
		dce_err(ch->d, "Error in waiting for ack");
		goto done;
	}

	trace_ivc_wait_complete(d, ch);

	ret = dce_ipc_read_message(d, ch_type, msg->rx.data, msg->rx.size);
	if (ret) {
		dce_err(ch->d, "Error in reading DCE msg for ch_type [%d]",
			ch_type);
		goto done;
	}
done:
	return ret;
}

/**
 * dce_ipc_get_channel_info - Provides information about frames details
 *
 * @d : Pointer to tegra_dce struct.
 * @ch_index : Channel Index.
 * @q_info : Pointer to struct dce_ipc_queue_info
 *
 * Return : 0 if successful
 */
int dce_ipc_get_channel_info(struct tegra_dce *d,
		struct dce_ipc_queue_info *q_info, u32 ch_index)
{
	struct dce_ipc_channel *ch = d->d_ipc.ch[ch_index];

	if (ch == NULL)
		return -ENOMEM;

	dce_mutex_lock(&ch->lock);

	memcpy(q_info, &ch->q_info, sizeof(ch->q_info));

	dce_mutex_unlock(&ch->lock);

	return 0;
}

/**
 * dce_ipc_get_region_iova_info - Provides iova details for ipc region
 *
 * @d : Pointer to tegra_dce struct.
 * @iova : Iova start address.
 * @size : Iova size
 *
 * Return : 0 if successful
 */
int dce_ipc_get_region_iova_info(struct tegra_dce *d, u64 *iova, u32 *size)
{
	struct dce_ipc_region *r = &d->d_ipc.region;

	if (!r->base)
		return -ENOMEM;

	*iova = r->iova;
	*size = r->size;

	return 0;
}

/*
 * dce_ipc_handle_notification - Handles the notification from remote
 *
 * @d : Pointer to tegra_dce struct
 * @id : Channel Index
 *
 * Return : True if the worker thread needs to wake up
 */
bool dce_ipc_is_data_available(struct tegra_dce *d, u32 ch_type)
{
	bool ret = false;
#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
	struct iosys_map map;
#else
	void *frame;
#endif
	struct dce_ipc_channel *ch = d->d_ipc.ch[ch_type];

	dce_mutex_lock(&ch->lock);

#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
	if (!tegra_ivc_read_get_next_frame(&ch->d_ivc, &map))
		ret = true;
#else
	frame = tegra_ivc_read_get_next_frame(&ch->d_ivc);
	if (!IS_ERR(frame))
		ret = true;
#endif

	dce_mutex_unlock(&ch->lock);

	return ret;
}

/*
 * dce_ipc_get_ipc_type - Returns the ipc_type for the channel.
 *
 * @d : Pointer to tegra_dce struct
 * @id : Channel Index
 *
 * Return : True if the worker thread needs to wake up
 */
uint32_t dce_ipc_get_ipc_type(struct tegra_dce *d, u32 ch_type)
{
	uint32_t ipc_type;
	struct dce_ipc_channel *ch = d->d_ipc.ch[ch_type];

	dce_mutex_lock(&ch->lock);

	ipc_type = ch->ipc_type;

	dce_mutex_unlock(&ch->lock);

	return ipc_type;
}
