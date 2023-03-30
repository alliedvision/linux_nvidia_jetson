/*
 * IVC based Library for I2C services.
 *
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/tegra-ivc.h>
#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include "i2c-tegra-hv-common.h"

#ifdef I2C_DEBUG
void print_msg(struct i2c_msg *msg)
{
	int len = msg->len;
	int i;
	uint8_t *temp_buf = (uint8_t *)&(msg->buf);

	for (i = 0; i < len; i++) {
		pr_err("address:0x%x:flags:0x%x:len:0x%x:buffer:0x%x\n",
				msg->addr, msg->flags, msg->len, *(temp_buf++));

	}
}

void print_frame(struct i2c_ivc_frame *i2c_ivc_frame, int num)
{
	int i;
	struct i2c_msg *msg_ptr = (struct i2c_msg
			*)&(i2c_ivc_frame->virt_msg_array);
	pr_err("smarker:0x%x::\nemarker:0x%x\n", i2c_ivc_frame->hdr.s_marker,
			i2c_ivc_frame->hdr.e_marker);
	for (i = 0; i < num; i++) {
		print_msg(msg_ptr);
		msg_ptr = (struct i2c_msg *)((char *)msg_ptr + offsetof(struct
					i2c_virt_msg, buf) +
			msg_ptr->len);
	}
}
#endif

static int hv_i2c_ivc_send(struct tegra_hv_i2c_comm_chan *comm_chan,
			struct i2c_ivc_frame *i2c_ivc_frame)
{
	struct tegra_hv_i2c_comm_dev *comm_dev = comm_chan->hv_comm_dev;
	unsigned long flags = 0;
	int ret = 0;

	if (!comm_chan->ivck || !i2c_ivc_frame)
		return -EINVAL;

	while (tegra_hv_ivc_channel_notified(comm_chan->ivck))
	/* Waiting for the channel to be ready */;

	spin_lock_irqsave(&comm_dev->ivck_tx_lock, flags);

	if (!tegra_hv_ivc_can_write(comm_chan->ivck)) {
		ret = -EBUSY;
		goto fail;
	}

	ret = tegra_hv_ivc_write(comm_chan->ivck, i2c_ivc_frame,
			comm_chan->ivck->frame_size);
	if (ret != comm_chan->ivck->frame_size) {
		ret = -EIO;
		goto fail;
	}

fail:
	spin_unlock_irqrestore(&comm_dev->ivck_tx_lock, flags);
	return ret;
}

void hv_i2c_comm_chan_cleanup(struct tegra_hv_i2c_comm_chan *comm_chan)
{
	unsigned long flags;

	spin_lock_irqsave(&comm_chan->lock, flags);
	/* Free this channel up for further use */
	comm_chan->rcvd_data = NULL;
	comm_chan->rx_state = I2C_RX_INIT;
	spin_unlock_irqrestore(&comm_chan->lock, flags);
}

static void hv_i2c_prep_msg_generic(int comm_chan_id, phys_addr_t base,
		struct i2c_ivc_frame *frame, int32_t count)
{
	i2c_ivc_error_field(frame) = 0;
	i2c_ivc_count_field(frame) = count;
	i2c_ivc_start_marker(frame) = 0xf005ba11;
	i2c_ivc_end_marker(frame) = 0x11ab500f;
	i2c_ivc_chan_id(frame) = comm_chan_id;
	i2c_ivc_controller_instance(frame) = (u32)base;

	/* Since controller instance is 32 bit, this is to make sure we don't
	 * send the base more than 4GB */
	BUG_ON(base >= SZ_4G);

}

static int hv_i2c_send_msg(struct device *dev,
			struct tegra_hv_i2c_comm_chan *comm_chan,
			struct i2c_ivc_frame *i2c_ivc_frame)
{
	unsigned long flags;
	int rv;

	spin_lock_irqsave(&comm_chan->lock, flags);

	/* comm chan state check and set */
	if (comm_chan->rx_state != I2C_RX_INIT) {
		dev_err(dev, "can only handle 1 frame per adapter at a time\n");
		rv = -EBUSY;
		goto fail;
	}

	/* Update comm chan context */
	comm_chan->rx_state = I2C_RX_PENDING;
	comm_chan->rcvd_data = i2c_ivc_frame;

	rv = hv_i2c_ivc_send(comm_chan, i2c_ivc_frame);
	if (rv < 0) {
		dev_err(dev, "ivc_send failed err %d\n", rv);
		/* restore channel state because no message was sent */
		comm_chan->rx_state = I2C_RX_INIT;
		goto fail; /* Redundant but safe */
	}
fail:
	spin_unlock_irqrestore(&comm_chan->lock, flags);
	return rv;
}

int hv_i2c_comm_chan_transfer_size(struct tegra_hv_i2c_comm_chan *comm_chan)
{
	return comm_chan->ivck->frame_size;
}

/*
 * hv_i2c_transfer
 * Send a message to the i2c server, caller is expected to wait for response
 * and handle possible timeout
 *
 * Create the necessary structure to transfer the messages
 */
int hv_i2c_transfer(struct i2c_ivc_frame *p_ivc_frame, struct tegra_hv_i2c_comm_chan
		*comm_chan, phys_addr_t base, struct i2c_msg msgs[],
		int count)
{
	int j;
	int err;
	int frame_len = 0;
	struct device *dev = comm_chan->dev;
	struct i2c_virt_msg *p_virt_msg = NULL;

	/* prepare the generic header */
	hv_i2c_prep_msg_generic(comm_chan->id, base, p_ivc_frame, count);

	p_virt_msg = I2C_IVC_FRAME_GET_FIRST_MSG_PTR(p_ivc_frame);

	frame_len += I2C_IVC_COMMON_HEADER_LEN;

	for (j = 0; j < count; j++) {
		frame_len += offsetof(struct i2c_virt_msg, buf) + msgs[j].len;
		if (frame_len >= comm_chan->ivck->frame_size) {
			dev_err(comm_chan->dev, "Data exceeded IVC frame size\n");
			return -ENOMEM;
		}

		/* copy addr, flags and len */
		p_virt_msg->addr = msgs[j].addr;
		p_virt_msg->flags = msgs[j].flags;
		p_virt_msg->len = msgs[j].len;

		/* memcpy the buf */
		memcpy(&(p_virt_msg->buf), msgs[j].buf, msgs[j].len);

		p_virt_msg = I2C_IVC_FRAME_GET_NEXT_MSG_PTR(p_virt_msg);
	}

	err = hv_i2c_send_msg(dev, comm_chan, p_ivc_frame);

	return err;
}

static void *_hv_i2c_comm_chan_alloc(i2c_isr_handler handler, void *data,
		struct device *dev, struct tegra_hv_i2c_comm_dev *comm_dev)
{
	struct tegra_hv_i2c_comm_chan *comm_chan = NULL;
	unsigned long flags;
	void *err = NULL;
	int chan_id;

	comm_chan = devm_kzalloc(dev, sizeof(*comm_chan),
						GFP_KERNEL);
	if (!comm_chan) {
		err = ERR_PTR(-ENOMEM);
		goto out;
	}

	comm_chan->dev = dev;
	comm_chan->rx_state = I2C_RX_INIT;
	comm_chan->hv_comm_dev = comm_dev;
	spin_lock_init(&comm_chan->lock);
	comm_chan->handler = handler;
	comm_chan->data = data;
	comm_chan->ivck = comm_dev->ivck;

	spin_lock_irqsave(&comm_dev->lock, flags);
	/* Find a free channel number */
	for (chan_id = 0; chan_id < MAX_COMM_CHANS; chan_id++) {
		if (comm_dev->hv_comm_chan[chan_id] == NULL)
			break;
	}

	if (chan_id >= MAX_COMM_CHANS) {
		/* No free channel available */
		err = ERR_PTR(-ENOMEM);
		goto fail_cleanup;
	}
	comm_chan->id = chan_id;

	comm_dev->hv_comm_chan[comm_chan->id] = comm_chan;
	spin_unlock_irqrestore(&comm_dev->lock, flags);
	return comm_chan;
fail_cleanup:
	spin_unlock_irqrestore(&comm_dev->lock, flags);
	devm_kfree(dev, comm_chan);
out:
	return err;
}

void hv_i2c_comm_chan_free(struct tegra_hv_i2c_comm_chan *comm_chan)
{
	unsigned long flags;
	struct tegra_hv_i2c_comm_dev *comm_dev = comm_chan->hv_comm_dev;
	struct device *dev = comm_chan->dev;

	spin_lock_irqsave(&comm_dev->lock, flags);
	comm_dev->hv_comm_chan[comm_chan->id] = NULL;
	spin_unlock_irqrestore(&comm_dev->lock, flags);

	devm_kfree(dev, comm_chan);

}

void hv_i2c_comm_suspend(struct tegra_hv_i2c_comm_chan *comm_chan)
{
	disable_irq(comm_chan->ivck->irq);
	cancel_work_sync(&comm_chan->hv_comm_dev->work);
}

void hv_i2c_comm_resume(struct tegra_hv_i2c_comm_chan *comm_chan)
{
	enable_irq(comm_chan->ivck->irq);
	schedule_work(&comm_chan->hv_comm_dev->work);
}

static irqreturn_t hv_i2c_isr(int irq, void *dev_id)
{
	struct tegra_hv_i2c_comm_dev *comm_dev =
		(struct tegra_hv_i2c_comm_dev *)dev_id;

	schedule_work(&comm_dev->work);

	return IRQ_HANDLED;
}

static void hv_i2c_work(struct work_struct *work)
{
	/* In theory it is possible that the comm_chan referred to in the
	 * received message might not have been allocated yet on this side
	 * (although that is unlikely given that the server responds to
	 * messages from the client only)
	 */
	struct tegra_hv_i2c_comm_dev *comm_dev = container_of(work, struct
			tegra_hv_i2c_comm_dev, work);
	struct tegra_hv_ivc_cookie *ivck = comm_dev->ivck;
	struct i2c_ivc_msg_common rx_msg_hdr;
	struct i2c_ivc_frame *fake_rx_frame = (struct i2c_ivc_frame *)&rx_msg_hdr;
	struct i2c_ivc_frame *frame;
	/* fake in the sense that this ptr does not represent the whole message,
	 * DO NOT use it to access anything except common header fields
	 */
	struct tegra_hv_i2c_comm_chan *comm_chan = NULL;
	int comm_chan_id;
	int ret;
	u32 len = 0;

	if (tegra_hv_ivc_channel_notified(ivck)) {
		pr_warn("%s: Skipping work since queue is not ready\n",
				__func__);
		return;
	}
	while (tegra_hv_ivc_can_read(ivck)) {
		/* Message available
		 * Initialize local variables to safe values
		 */
		comm_chan = NULL;
		comm_chan_id = -1;
		len = 0;
		memset(&rx_msg_hdr, 0, I2C_IVC_COMMON_HEADER_LEN);

		/* Read from IVC queue offset 0 a size of header len
		 * and copy it to rx_msg_hdr */
		len = tegra_hv_ivc_read_peek(ivck, &rx_msg_hdr, 0,
				I2C_IVC_COMMON_HEADER_LEN);
		if (len != I2C_IVC_COMMON_HEADER_LEN) {
			pr_err("%s: IVC read failure (msg size error)\n",
					__func__);
			tegra_hv_ivc_read_advance(ivck);
			continue;
		}

		if ((i2c_ivc_start_marker(fake_rx_frame) != 0xf005ba11) ||
				(i2c_ivc_end_marker(fake_rx_frame) != 0x11ab500f)) {
			pr_err("%s: IVC read failure (invalid markers)\n",
					__func__);
			tegra_hv_ivc_read_advance(ivck);
			continue;
		}

		comm_chan_id = i2c_ivc_chan_id(fake_rx_frame);

		if (!((comm_chan_id >= 0) && (comm_chan_id < MAX_COMM_CHANS))) {
			pr_err("%s: IVC read failure (invalid comm chan id)\n",
					__func__);
			tegra_hv_ivc_read_advance(ivck);
			continue;
		}

		comm_chan = comm_dev->hv_comm_chan[comm_chan_id];
		if (!comm_chan || comm_chan->id != comm_chan_id) {
			pr_err("%s: Invalid channel from server %d\n", __func__,
					comm_chan_id);
			tegra_hv_ivc_read_advance(ivck);
			continue;
		}

		if (comm_chan->rx_state == I2C_RX_INIT) {
			dev_err(comm_chan->dev,
					"Spurious message from server (channel %d)\n",
					comm_chan_id);
			WARN_ON(comm_chan->rcvd_data != NULL);
			tegra_hv_ivc_read_advance(ivck);
			continue;
		}


		frame = comm_chan->rcvd_data;
		if (comm_chan->rx_state == I2C_RX_PENDING) {
			/* Copy the message to consumer*/
			ret = tegra_hv_ivc_read(ivck, comm_chan->rcvd_data, comm_chan->ivck->frame_size);
			if ( ret != comm_chan->ivck->frame_size ) {
				dev_err(comm_chan->dev, "IVC read failed for channel ID : %d\n", comm_chan_id);
			}
			WARN_ON(comm_chan->rcvd_data == NULL);
			hv_i2c_comm_chan_cleanup(comm_chan);
			comm_chan->handler(comm_chan->data);
		} else {
			WARN(1, "Bad channel state\n");
		}
	}

	return;
}

void tegra_hv_i2c_poll_cleanup(struct tegra_hv_i2c_comm_chan *comm_chan)
{
	struct tegra_hv_i2c_comm_dev *comm_dev = comm_chan->hv_comm_dev;
	unsigned long ms = 0;

	while (comm_chan->rx_state != I2C_RX_INIT) {
		msleep(20);
		ms += 20;
		dev_err(comm_chan->dev, "Polling for response (Total %lu ms)\n",
				ms);
		hv_i2c_work(&comm_dev->work);
	}
}

static struct tegra_hv_i2c_comm_dev *_hv_i2c_get_comm_dev(struct device *dev,
		struct device_node *hv_dn, uint32_t ivc_queue)
{
	static HLIST_HEAD(ivc_comm_devs);
	struct tegra_hv_i2c_comm_dev *comm_dev = NULL;
	struct tegra_hv_ivc_cookie *ivck = NULL;
	int err;

	hlist_for_each_entry(comm_dev, &ivc_comm_devs, list) {
		if (comm_dev->queue_id == ivc_queue)
			goto end;
	}

	/* could not find a previously created comm_dev for this ivc
	 * queue, create one.
	 */

	ivck = tegra_hv_ivc_reserve(hv_dn, ivc_queue, NULL);
	if (IS_ERR_OR_NULL(ivck)) {
		dev_err(dev, "Failed to reserve ivc queue %d\n",
				ivc_queue);
		comm_dev = ERR_PTR(-EINVAL);
		goto end;
	}

	comm_dev = devm_kzalloc(dev, sizeof(*comm_dev),
			GFP_KERNEL);
	if (!comm_dev) {
		/* Unreserve the queue here because other controllers
		 * will probably try to reserve it again until one
		 * succeeds or all of them fail
		 */
		tegra_hv_ivc_unreserve(ivck);
		comm_dev = ERR_PTR(-ENOMEM);
		goto end;
	}

	comm_dev->ivck = ivck;
	comm_dev->queue_id = ivc_queue;

	spin_lock_init(&comm_dev->ivck_tx_lock);
	spin_lock_init(&comm_dev->lock);

	INIT_HLIST_NODE(&comm_dev->list);
	hlist_add_head(&comm_dev->list, &ivc_comm_devs);
	INIT_WORK(&comm_dev->work, hv_i2c_work);

	/* Our comm_dev is ready, so enable irq here. But channels are
	 * not yet allocated, we need to take care of that in the
	 * handler
	 */
	err = request_threaded_irq(ivck->irq, hv_i2c_isr, NULL, 0,
			dev_name(dev), comm_dev);
	if (err) {
		hlist_del(&comm_dev->list);
		devm_kfree(dev, comm_dev);
		tegra_hv_ivc_unreserve(ivck);
		comm_dev = ERR_PTR(-ENOMEM);
		goto end;
	}

	/* set ivc channel to invalid state */
	tegra_hv_ivc_channel_reset(ivck);

end:
	return comm_dev;
}

void *hv_i2c_comm_init(struct device *dev, i2c_isr_handler handler,
		void *data)
{
	int err;
	uint32_t ivc_queue;
	struct device_node *dn, *hv_dn;
	struct tegra_hv_i2c_comm_dev *comm_dev = NULL;

	dn = dev->of_node;
	if (dn == NULL) {
		dev_err(dev, "No OF data\n");
		return ERR_PTR(-EINVAL);
	}

	hv_dn = of_parse_phandle(dn, "ivc_queue", 0);
	if (hv_dn == NULL) {
		dev_err(dev, "Failed to parse phandle of ivc prop\n");
		return ERR_PTR(-EINVAL);
	}

	err = of_property_read_u32_index(dn, "ivc_queue", 1,
			&ivc_queue);
	if (err != 0) {
		dev_err(dev, "Failed to read IVC property ID\n");
		of_node_put(hv_dn);
		return ERR_PTR(-EINVAL);
	}

	comm_dev = _hv_i2c_get_comm_dev(dev, hv_dn, ivc_queue);

	if (IS_ERR_OR_NULL(comm_dev))
		return comm_dev;

	return _hv_i2c_comm_chan_alloc(handler, data, dev, comm_dev);
}
