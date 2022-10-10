/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/mailbox_controller.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-instance.h>
#include <linux/tegra-aon.h>
#include <linux/cache.h>

#include <aon-hsp-combo.h>
#include <aon.h>

#define IVC_INIT_TIMEOUT_US (200000)

struct tegra_aon_ivc {
	struct mbox_controller mbox;
};

struct tegra_aon_ivc_chan {
	struct ivc ivc;
	char *name;
	int chan_id;
	struct tegra_aon *aon;
	bool last_tx_done;
};

static struct tegra_aon_ivc aon_ivc;

/* This has to be a multiple of the cache line size */
static inline int ivc_min_frame_size(void)
{
	return cache_line_size();
}

int tegra_aon_ipc_init(struct tegra_aon *aon)
{
	ktime_t tstart;
	int ret = 0;

	tegra_aon_hsp_ss_set(aon, aon->ivc_carveout_base_ss,
					(u32)aon->ipcbuf_dma);
	tegra_aon_hsp_ss_set(aon, aon->ivc_carveout_size_ss,
					(u32)aon->ipcbuf_size);
	ret = tegra_aon_hsp_sm_tx_write(aon, SMBOX_IVC_READY_MSG);
	if (ret) {
		dev_err(aon->dev, "aon hsp sm tx write failed: %d\n", ret);
		return ret;
	}

	tstart = ktime_get();
	while (!tegra_aon_hsp_sm_tx_is_empty(aon)) {
		if (ktime_us_delta(ktime_get(), tstart) > IVC_INIT_TIMEOUT_US) {
			tegra_aon_hsp_sm_pair_free(aon);
			ret = -ETIMEDOUT;
			break;
		}
	}

	return ret;
}

static int tegra_aon_mbox_get_max_txsize(struct mbox_chan *mbox_chan)
{
	struct tegra_aon_ivc_chan *ivc_chan;

	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;

	return ivc_chan->ivc.frame_size;
}

static int tegra_aon_mbox_send_data(struct mbox_chan *mbox_chan, void *data)
{
	struct tegra_aon_ivc_chan *ivc_chan;
	struct tegra_aon_mbox_msg *msg;
	int bytes;
	int ret;

	msg = (struct tegra_aon_mbox_msg *)data;
	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;
	bytes = tegra_ivc_write(&ivc_chan->ivc, msg->data, msg->length);
	ret = (bytes != msg->length) ? -EBUSY : 0;
	if (bytes < 0) {
		pr_err("%s mbox send failed with error %d\n", __func__, bytes);
		ret = bytes;
	}
	ivc_chan->last_tx_done = (ret == 0);

	return ret;
}

static int tegra_aon_mbox_startup(struct mbox_chan *mbox_chan)
{
	return 0;
}

static void tegra_aon_mbox_shutdown(struct mbox_chan *mbox_chan)
{
	struct tegra_aon_ivc_chan *ivc_chan;

	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;
	ivc_chan->chan_id = -1;
}

static bool tegra_aon_mbox_last_tx_done(struct mbox_chan *mbox_chan)
{
	struct tegra_aon_ivc_chan *ivc_chan;

	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;

	return ivc_chan->last_tx_done;
}

static struct mbox_chan_ops tegra_aon_mbox_chan_ops = {
	.get_max_txsize = tegra_aon_mbox_get_max_txsize,
	.send_data = tegra_aon_mbox_send_data,
	.startup = tegra_aon_mbox_startup,
	.shutdown = tegra_aon_mbox_shutdown,
	.last_tx_done = tegra_aon_mbox_last_tx_done,
};

static void tegra_aon_notify_remote(struct ivc *ivc)
{
	struct tegra_aon_ivc_chan *ivc_chan;

	ivc_chan = container_of(ivc, struct tegra_aon_ivc_chan, ivc);
	tegra_aon_hsp_ss_set(ivc_chan->aon, ivc_chan->aon->ivc_tx_ss,
				BIT(ivc_chan->chan_id));
	tegra_aon_hsp_sm_tx_write(ivc_chan->aon, SMBOX_IVC_NOTIFY);
}

static void tegra_aon_rx_handler(u32 ivc_chans)
{
	struct mbox_chan *mbox_chan;
	struct ivc *ivc;
	struct tegra_aon_ivc_chan *ivc_chan;
	struct tegra_aon_mbox_msg msg;
	int i;

	ivc_chans &= BIT(aon_ivc.mbox.num_chans) - 1;
	while (ivc_chans) {
		i = __builtin_ctz(ivc_chans);
		ivc_chans &= ~BIT(i);
		mbox_chan = &aon_ivc.mbox.chans[i];
		ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;
		/* check if mailbox client exists */
		if (ivc_chan->chan_id == -1)
			continue;
		ivc = &ivc_chan->ivc;
		while (tegra_ivc_can_read(ivc)) {
			msg.data = tegra_ivc_read_get_next_frame(ivc);
			msg.length = ivc->frame_size;
			mbox_chan_received_data(mbox_chan, &msg);
			tegra_ivc_read_advance(ivc);
		}
	}
}

static void tegra_aon_hsp_sm_full_notify(void *data, u32 value)
{
	struct tegra_aon *aon = data;
	u32 ss_val;

	if (value != SMBOX_IVC_NOTIFY) {
		dev_err(aon->dev, "Invalid IVC notification\n");
		return;
	}

	ss_val = tegra_aon_hsp_ss_status(aon, aon->ivc_rx_ss);
	tegra_aon_hsp_ss_clr(aon, aon->ivc_rx_ss, ss_val);
	tegra_aon_rx_handler(ss_val);
}

static int tegra_aon_parse_channel(struct tegra_aon *aon,
				struct mbox_chan *mbox_chan,
				struct device_node *ch_node,
				int chan_id)
{
	struct device *dev;
	struct tegra_aon_ivc_chan *ivc_chan;
	struct {
		u32 rx, tx;
	} start, end;
	u32 nframes, frame_size;
	int ret = 0;

	/* Sanity check */
	if (!mbox_chan || !ch_node || !aon)
		return -EINVAL;

	dev = aon->dev;

	ret = of_property_read_u32_array(ch_node, "reg", &start.rx, 2);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", "reg");
		return ret;
	}
	ret = of_property_read_u32(ch_node, NV("frame-count"), &nframes);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV("frame-count"));
		return ret;
	}
	ret = of_property_read_u32(ch_node, NV("frame-size"), &frame_size);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV("frame-size"));
		return ret;
	}

	if (!nframes) {
		dev_err(dev, "Invalid <nframes> property\n");
		return -EINVAL;
	}

	if (frame_size < ivc_min_frame_size()) {
		dev_err(dev, "Invalid <frame-size> property\n");
		return -EINVAL;
	}

	end.rx = start.rx + tegra_ivc_total_queue_size(nframes * frame_size);
	end.tx = start.tx + tegra_ivc_total_queue_size(nframes * frame_size);

	if (end.rx > aon->ipcbuf_size) {
		dev_err(dev, "%s buffer exceeds ivc size\n", "rx");
		return -EINVAL;
	}
	if (end.tx > aon->ipcbuf_size) {
		dev_err(dev, "%s buffer exceeds ivc size\n", "tx");
		return -EINVAL;
	}

	if (start.tx < start.rx ? end.tx > start.rx : end.rx > start.tx) {
		dev_err(dev, "rx and tx buffers overlap on channel %s\n",
			ch_node->name);
		return -EINVAL;
	}

	ivc_chan = devm_kzalloc(dev, sizeof(*ivc_chan), GFP_KERNEL);
	if (!ivc_chan)
		return -ENOMEM;

	ivc_chan->name = devm_kstrdup(dev, ch_node->name, GFP_KERNEL);
	if (!ivc_chan->name)
		return -ENOMEM;

	ivc_chan->chan_id = chan_id;

	/* Allocate the IVC links */
	ret = tegra_ivc_init_with_dma_handle(&ivc_chan->ivc,
				 (unsigned long)aon->ipcbuf + start.rx,
				 (u64)aon->ipcbuf_dma + start.rx,
				 (unsigned long)aon->ipcbuf + start.tx,
				 (u64)aon->ipcbuf_dma + start.tx,
				 nframes, frame_size, dev,
				 tegra_aon_notify_remote);
	if (ret) {
		dev_err(dev, "failed to instantiate IVC.\n");
		return ret;
	}

	ivc_chan->aon = aon;
	mbox_chan->con_priv = ivc_chan;

	dev_dbg(dev, "%s: RX: 0x%x-0x%x TX: 0x%x-0x%x\n",
		ivc_chan->name, start.rx, end.rx, start.tx, end.tx);

	return ret;
}

static int tegra_aon_check_channels_overlap(struct device *dev,
			struct tegra_aon_ivc_chan *ch0,
			struct tegra_aon_ivc_chan *ch1)
{
	unsigned int s0, s1;
	unsigned long tx0, rx0, tx1, rx1;

	if (ch0 == NULL || ch1 == NULL)
		return -EINVAL;

	tx0 = (unsigned long)ch0->ivc.tx_channel;
	rx0 = (unsigned long)ch0->ivc.rx_channel;
	s0 = ch0->ivc.nframes * ch0->ivc.frame_size;
	s0 = tegra_ivc_total_queue_size(s0);

	tx1 = (unsigned long)ch1->ivc.tx_channel;
	rx1 = (unsigned long)ch1->ivc.rx_channel;
	s1 = ch1->ivc.nframes * ch1->ivc.frame_size;
	s1 = tegra_ivc_total_queue_size(s1);

	if ((tx0 < tx1 ? tx0 + s0 > tx1 : tx1 + s1 > tx0) ||
		(rx0 < tx1 ? rx0 + s0 > tx1 : tx1 + s1 > rx0) ||
		(rx0 < rx1 ? rx0 + s0 > rx1 : rx1 + s1 > rx0) ||
		(tx0 < rx1 ? tx0 + s0 > rx1 : rx1 + s1 > tx0)) {
		dev_err(dev, "ivc buffers overlap on channels %s and %s\n",
			ch0->name, ch1->name);
		return -EINVAL;
	}

	return 0;
}

static int tegra_aon_validate_channels(struct device *dev)
{
	struct tegra_aon *aon;
	struct tegra_aon_ivc_chan *i_chan, *j_chan;
	int i, j;
	int ret;

	aon = dev_get_drvdata(dev);

	for (i = 0; i < aon_ivc.mbox.num_chans; i++) {
		i_chan = aon_ivc.mbox.chans[i].con_priv;

		for (j = i + 1; j < aon_ivc.mbox.num_chans; j++) {
			j_chan = aon_ivc.mbox.chans[j].con_priv;

			ret = tegra_aon_check_channels_overlap(aon->dev,
							i_chan, j_chan);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int tegra_aon_parse_channels(struct tegra_aon *aon)
{
	struct tegra_aon_ivc *aonivc = &aon_ivc;
	struct device *dev = aon->dev;
	struct device_node *reg_node, *ch_node;
	int ret, i;

	i = 0;

	for_each_child_of_node(dev->of_node, reg_node) {
		if (strcmp(reg_node->name, "ivc-channels"))
			continue;

		for_each_child_of_node(reg_node, ch_node) {
			ret = tegra_aon_parse_channel(aon,
						&aonivc->mbox.chans[i],
						ch_node, i);
			i++;
			if (ret) {
				dev_err(dev, "failed to parse a channel\n");
				return ret;
			}
		}
		break;
	}

	return tegra_aon_validate_channels(dev);
}

static int tegra_aon_count_ivc_channels(struct device_node *dev_node)
{
	int num = 0;
	struct device_node *child_node;

	for_each_child_of_node(dev_node, child_node) {
		if (strcmp(child_node->name, "ivc-channels"))
			continue;
		num = of_get_child_count(child_node);
		break;
	}

	return num;
}

int tegra_aon_mail_init(struct tegra_aon *aon)
{
	struct tegra_aon_ivc *aonivc = &aon_ivc;
	struct device *dev = aon->dev;
	struct device_node *dn = dev->of_node;
	int num_chans;
	int ret;

	num_chans = tegra_aon_count_ivc_channels(dn);
	if (num_chans <= 0) {
		dev_err(dev, "no ivc channels\n");
		ret = -EINVAL;
		goto exit;
	}

	aonivc->mbox.dev = aon->dev;
	aonivc->mbox.chans = devm_kzalloc(aon->dev,
					num_chans * sizeof(*aonivc->mbox.chans),
					GFP_KERNEL);
	if (!aonivc->mbox.chans) {
		ret = -ENOMEM;
		goto exit;
	}

	aonivc->mbox.num_chans = num_chans;
	aonivc->mbox.ops = &tegra_aon_mbox_chan_ops;
	aonivc->mbox.txdone_poll = true;
	aonivc->mbox.txpoll_period = 1;

	/* Parse out all channels from DT */
	ret = tegra_aon_parse_channels(aon);
	if (ret) {
		dev_err(dev, "ivc-channels set up failed: %d\n", ret);
		goto exit;
	}

	/* Fetch the shared mailbox pair associated with IVC tx and rx */
	ret = tegra_aon_hsp_sm_pair_request(aon, tegra_aon_hsp_sm_full_notify,
					    aon);
	if (ret) {
		dev_err(dev, "aon hsp sm pair request failed: %d\n", ret);
		goto exit;
	}

	ret = mbox_controller_register(&aonivc->mbox);
	if (ret) {
		dev_err(dev, "failed to register mailbox: %d\n", ret);
		tegra_aon_hsp_sm_pair_free(aon);
		goto exit;
	}

exit:
	return ret;
}

void tegra_aon_mail_deinit(struct tegra_aon *aon)
{
	mbox_controller_unregister(&aon_ivc.mbox);
	tegra_aon_hsp_sm_pair_free(aon);
}
