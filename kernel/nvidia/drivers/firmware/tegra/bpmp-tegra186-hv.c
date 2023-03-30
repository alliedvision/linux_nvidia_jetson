// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
 */
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/tegra-ivc.h>
#include <soc/tegra/bpmp.h>

#include "bpmp-private.h"

#define MAX_POSSIBLE_RX_CHANNEL 1
#define TX_CHANNEL_EXACT_COUNT  1

static struct tegra186_hv_bpmp {
	struct tegra_bpmp *parent;
} tegra186_hv_bpmp;

static struct tegra_hv_ivc_cookie **hv_bpmp_ivc_cookies;
static struct device_node *hv_of_node;

static irqreturn_t tegra186_hv_bpmp_rx_handler(int irq, void *ivck)
{
	tegra_bpmp_handle_rx(tegra186_hv_bpmp.parent);
	return IRQ_HANDLED;
}

static int tegra186_hv_bpmp_channel_init(struct tegra_bpmp_channel *channel,
				      struct tegra_bpmp *bpmp,
				      unsigned int queue_id, bool threaded)
{
	static int cookie_idx;
	int err;

	channel->hv_ivc = tegra_hv_ivc_reserve(hv_of_node, queue_id, NULL);

	if (IS_ERR_OR_NULL(channel->hv_ivc)) {
		pr_err("%s: Failed to reserve ivc queue @index %d\n",
				__func__, queue_id);
		goto request_cleanup;
	}

	if (channel->hv_ivc->frame_size < MSG_DATA_MIN_SZ) {
		pr_err("%s: Frame size is too small\n", __func__);
		goto request_cleanup;
	}

	hv_bpmp_ivc_cookies[cookie_idx++] = channel->hv_ivc;

	/* init completion */
	init_completion(&channel->completion);
	channel->bpmp = bpmp;

	if (threaded) {
		err = request_threaded_irq(
				channel->hv_ivc->irq,
				tegra186_hv_bpmp_rx_handler, NULL,
				IRQF_NO_SUSPEND,
				"bpmp_irq_handler", &channel->hv_ivc);
	} else {
		err = 0;
	}

	if (err) {
		pr_err("%s: Failed to request irq %d for index %d\n",
				__func__, channel->hv_ivc->irq, queue_id);
		goto request_cleanup;
	}

	return 0;

request_cleanup:
	return -1;
}

static bool tegra186_bpmp_hv_is_message_ready(struct tegra_bpmp_channel *channel)
{
	void *frame;

	frame = tegra_hv_ivc_read_get_next_frame(channel->hv_ivc);
	if (IS_ERR(frame)) {
		channel->ib = NULL;
		return false;
	}

	channel->ib = frame;

	return true;
}

static int tegra186_bpmp_hv_ack_message(struct tegra_bpmp_channel *channel)
{
	return tegra_hv_ivc_read_advance(channel->hv_ivc);
}

static bool tegra186_hv_bpmp_is_channel_free(struct tegra_bpmp_channel *channel)
{
	void *frame;

	frame = tegra_hv_ivc_write_get_next_frame(channel->hv_ivc);
	if (IS_ERR(frame)) {
		channel->ob = NULL;
		return false;
	}

	channel->ob = frame;

	return true;
}

static int tegra186_hv_bpmp_post_message(struct tegra_bpmp_channel *channel)
{
	return tegra_hv_ivc_write_advance(channel->hv_ivc);
}

static void tegra186_hv_bpmp_channel_reset(struct tegra_bpmp_channel *channel)
{
	/* reset the channel state */
	tegra_hv_ivc_channel_reset(channel->hv_ivc);

	/* sync the channel state with BPMP */
	while (tegra_hv_ivc_channel_notified(channel->hv_ivc))
		;
}

static int tegra186_hv_bpmp_resume(struct tegra_bpmp *bpmp)
{
	unsigned int i;

	/* reset message channels */
	if (bpmp->soc->channels.cpu_tx.count == TX_CHANNEL_EXACT_COUNT) {
		tegra186_hv_bpmp_channel_reset(bpmp->tx_channel);
	} else {
		pr_err("%s: Error: driver should have single tx channel mandatory\n", __func__);
		return -1;
	}

	if (bpmp->soc->channels.cpu_rx.count == MAX_POSSIBLE_RX_CHANNEL)
		tegra186_hv_bpmp_channel_reset(bpmp->rx_channel);

	for (i = 0; i < bpmp->threaded.count; i++)
		tegra186_hv_bpmp_channel_reset(&bpmp->threaded_channels[i]);

	return 0;
}

static int tegra186_hv_ivc_notify(struct tegra_bpmp *bpmp)
{
	tegra_hv_ivc_notify(bpmp->tx_channel->hv_ivc);
	return 0;
}

static int tegra186_hv_bpmp_init(struct tegra_bpmp *bpmp)
{
	struct tegra186_hv_bpmp *priv;
	struct device_node *of_node = bpmp->dev->of_node;
	int err, index;
	uint32_t first_ivc_queue, num_ivc_queues;

	priv = devm_kzalloc(bpmp->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	bpmp->priv = priv;
	priv->parent = bpmp;
	tegra186_hv_bpmp.parent = bpmp;

	/* get starting ivc queue id & ivc queue count */
	hv_of_node = of_parse_phandle(of_node, "ivc_queue", 0);
	if (!hv_of_node) {
		pr_err("%s: Unable to find hypervisor node\n", __func__);
		return -EINVAL;
	}

	err = of_property_read_u32_index(of_node, "ivc_queue", 1,
			&first_ivc_queue);
	if (err != 0) {
		pr_err("%s: Failed to read start IVC queue\n",
				__func__);
		of_node_put(hv_of_node);
		return -EINVAL;
	}

	err = of_property_read_u32_index(of_node, "ivc_queue", 2,
			&num_ivc_queues);
	if (err != 0) {
		pr_err("%s: Failed to read range of IVC queues\n",
				__func__);
		of_node_put(hv_of_node);
		return -EINVAL;
	}

	/* verify total queue count meets expectations */
	if (num_ivc_queues < (bpmp->soc->channels.thread.count +
			bpmp->soc->channels.cpu_tx.count + bpmp->soc->channels.cpu_rx.count)) {
		pr_err("%s: no of ivc queues in DT < required no of channels \n",
				__func__);
		of_node_put(hv_of_node);
		return -EINVAL;
	}

	hv_bpmp_ivc_cookies = kzalloc(sizeof(struct tegra_hv_ivc_cookie *) *
					num_ivc_queues, GFP_KERNEL);

	if (!hv_bpmp_ivc_cookies) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		of_node_put(hv_of_node);
		return -ENOMEM;
	}

	/* init tx channel */
	if (bpmp->soc->channels.cpu_tx.count == TX_CHANNEL_EXACT_COUNT) {
		err = tegra186_hv_bpmp_channel_init(bpmp->tx_channel, bpmp,
					 bpmp->soc->channels.cpu_tx.offset + first_ivc_queue, false);
		if (err < 0) {
			pr_err("%s: Failed initialize tx channel\n", __func__);
			goto cleanup;
		}
	} else {
		pr_err("%s: Error: driver should have single tx channel mandatory\n", __func__);
		goto cleanup;
	}

	/* init rx channel */
	if (bpmp->soc->channels.cpu_rx.count == MAX_POSSIBLE_RX_CHANNEL) {
		err = tegra186_hv_bpmp_channel_init(bpmp->rx_channel, bpmp,
					 bpmp->soc->channels.cpu_rx.offset + first_ivc_queue, true);
		if (err < 0) {
			pr_err("%s: Failed initialize rx channel\n", __func__);
			goto cleanup;
		}
	}

	for (index = 0; index < bpmp->threaded.count; index++) {
		unsigned int idx = bpmp->soc->channels.thread.offset + index;

		err = tegra186_hv_bpmp_channel_init(&bpmp->threaded_channels[index],
								 bpmp, idx + first_ivc_queue, true);
		if (err < 0) {
			pr_err("%s: Failed initialize tx channel\n", __func__);
			goto cleanup;
		}
	}

	tegra186_hv_bpmp_resume(bpmp);
	of_node_put(hv_of_node);

	return 0;

cleanup:
	for (index = 0; index < num_ivc_queues; index++) {
		if (hv_bpmp_ivc_cookies[index]) {
			tegra_hv_ivc_unreserve(
					hv_bpmp_ivc_cookies[index]);
			hv_bpmp_ivc_cookies[index] = NULL;
		}
	}
	kfree(hv_bpmp_ivc_cookies);
	of_node_put(hv_of_node);

	return -ENOMEM;
}

static void tegra186_hv_bpmp_channel_cleanup(struct tegra_bpmp_channel *channel)
{
	free_irq(channel->hv_ivc->irq, &channel->hv_ivc);
	tegra_hv_ivc_unreserve(channel->hv_ivc);
	kfree(channel->hv_ivc);
}

static void tegra186_hv_bpmp_deinit(struct tegra_bpmp *bpmp)
{
	unsigned int i;

	tegra186_hv_bpmp_channel_cleanup(bpmp->tx_channel);
	tegra186_hv_bpmp_channel_cleanup(bpmp->rx_channel);

	for (i = 0; i < bpmp->threaded.count; i++) {
		tegra186_hv_bpmp_channel_cleanup(&bpmp->threaded_channels[i]);
	}
}

const struct tegra_bpmp_ops tegra186_bpmp_hv_ops = {
	.init = tegra186_hv_bpmp_init,
	.deinit = tegra186_hv_bpmp_deinit,
	.is_response_ready = tegra186_bpmp_hv_is_message_ready,
	.is_request_ready = tegra186_bpmp_hv_is_message_ready,
	.ack_response = tegra186_bpmp_hv_ack_message,
	.ack_request = tegra186_bpmp_hv_ack_message,
	.is_response_channel_free = tegra186_hv_bpmp_is_channel_free,
	.is_request_channel_free = tegra186_hv_bpmp_is_channel_free,
	.post_response = tegra186_hv_bpmp_post_message,
	.post_request = tegra186_hv_bpmp_post_message,
	.ring_doorbell = tegra186_hv_ivc_notify,
	.resume = tegra186_hv_bpmp_resume,
};
