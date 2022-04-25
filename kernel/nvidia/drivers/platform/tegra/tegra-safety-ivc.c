/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/tegra-hsp.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-instance.h>
#include <linux/tegra_l1ss_kernel_interface.h>
#include <linux/wait.h>

#include <dt-bindings/memory/tegra-swgroup.h>
#include <linux/tegra-safety-ivc.h>
#include "tegra_l1ss.h"

#define NV(p) "nvidia," #p

int ivc_chan_count;
struct tegra_safety_ivc_chan *tegra_safety_get_ivc_chan_from_str(
		struct tegra_safety_ivc *safety_ivc,
		char *ch_name)
{
	int i;

	for (i = 0; i < ivc_chan_count; i++) {
		if (strcmp(safety_ivc->ivc_chan[i]->name, ch_name) == 0)
			return safety_ivc->ivc_chan[i];
	}

	return NULL;
}

static void tegra_safety_decode_cmd_resp(struct tegra_safety_ivc *safety_ivc)
{
	struct tegra_safety_ivc_chan *ivc_ch;
	struct ivc *ivc;
	static uint8_t cmd_resp[CMDRESP_CMD_FRAME_EX_SIZE] = {0};
	const void *ivc_frame = NULL;

	ivc_ch = tegra_safety_get_ivc_chan_from_str(safety_ivc, "cmdresp");
	if (!ivc_ch) {
		pr_err("%s: Failed to get CMD RESP IVC channel\n", __func__);
		return;
	}

	ivc = &ivc_ch->ivc;
	mutex_lock(&safety_ivc->rlock);
	if (tegra_ivc_can_read(ivc))
		ivc_frame = tegra_ivc_read_get_next_frame(ivc);
	mutex_unlock(&safety_ivc->rlock);

	if (ivc_frame == NULL) {
		pr_err("%s: No IVC frame to read\n", __func__);
		return;
	}

	memcpy(cmd_resp, ivc_frame, sizeof(cmdresp_frame_ex_t));

	tegra_ivc_read_advance(ivc);
	tegra_safety_handle_cmd((cmdresp_frame_ex_t *)&cmd_resp,
				safety_ivc->ldata);
}

static void tegra_safety_cmdresp_work_func(struct work_struct *work)
{
	struct tegra_safety_ivc *safety_ivc = container_of(work,
			struct tegra_safety_ivc, work);

	tegra_safety_decode_cmd_resp(safety_ivc);
}

/* wake up cmd-resp threads */
static u32 tegra_safety_ivc_full_notify(void *data, u32 response)
{
	struct tegra_safety_ivc *safety_ivc = (struct tegra_safety_ivc *)data;

	tegra_safety_dev_notify();

	if (response & SAFETY_CONF_IVC_L2SS_READY) {
		atomic_set(&safety_ivc->ivc_ready, 1);
		l1ss_set_ivc_ready();
		wake_up(&safety_ivc->cmd.response_waitq);
		l1ss_notify_client(L1SS_READY);
	} else if (response == TEGRA_SAFETY_SM_CMDRESP_CH) {
		queue_work(safety_ivc->wq, &safety_ivc->work);
	} else
		pr_err("%s: Invalid response %d received", __func__, response);

	return 0;
}

static int tegra_safety_ivc_setup_ready(struct device *dev)
{
	struct tegra_safety_ivc *safety_ivc = dev_get_drvdata(dev);
	struct safety_ast_region *region = &safety_ivc->region;
	u32 command;
	int ret;
	long timeout;

	command = SAFETY_CONF(IVC_READY, (region->dma >> 8));

	tegra_hsp_sm_pair_write(safety_ivc->ivc_pair, command);

	timeout = wait_event_interruptible_timeout(
			safety_ivc->cmd.response_waitq,
			(atomic_read(&safety_ivc->ivc_ready) == 1),
			TEGRA_SAFETY_IVC_READ_TIMEOUT * 2);
	if (timeout <= 0) {
		dev_err(dev, "Timed out waiting for SCE Ready");
		ret = -ETIMEDOUT;
	}

	return 0;
}

static void tegra_ivc_channel_ring(struct ivc *ivc)
{
	struct tegra_safety_ivc_chan *ivc_chan =
		container_of(ivc, struct tegra_safety_ivc_chan, ivc);
	struct tegra_safety_ivc *safety_ivc = ivc_chan->safety_ivc;

	tegra_hsp_sm_pair_write(safety_ivc->ivc_pair,
				TEGRA_SAFETY_SM_CMDRESP_CH);
}

static int tegra_ivc_channel_create(
		struct device *dev, struct device_node *ch_node,
		struct safety_ast_region *region)
{
	struct tegra_safety_ivc *safety_ivc = dev_get_drvdata(dev);
	void *base;
	dma_addr_t dma_handle;
	size_t size;
	union {
		u32 tab[2];
		struct {
			u32 rx;
			u32 tx;
		};
	} start, end;
	u32 nframes, frame_size;
	struct tegra_safety_ivc_chan *ivc_chan;
	int ret;

	ret = of_property_read_u32_array(ch_node, "reg", start.tab,
			ARRAY_SIZE(start.tab));
	if (ret) {
		dev_err(dev, "missing <%s> property\n", "reg");
		goto error;
	}

	ret = of_property_read_u32(ch_node, NV(frame-count), &nframes);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV(frame-count));
		goto error;
	}

	ret = of_property_read_u32(ch_node, NV(frame-size), &frame_size);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV(frame-size));
		goto error;
	}

	base = region->base;
	size = region->size;
	dma_handle = region->dma;
	ret = -EINVAL;
	end.rx = start.rx + tegra_ivc_total_queue_size(nframes * frame_size);
	end.tx = start.tx + tegra_ivc_total_queue_size(nframes * frame_size);

	if (end.rx > size) {
		dev_err(dev, "%s buffer exceeds IVC size\n", "RX");
		goto error;
	}

	if (end.tx > size) {
		dev_err(dev, "%s buffer exceeds IVC size\n", "TX");
		goto error;
	}

	if (start.tx < start.rx ? end.tx > start.rx : end.rx > start.tx) {
		dev_err(dev, "RX and TX buffers overlap\n");
		goto error;
	}

	ivc_chan = devm_kzalloc(dev, sizeof(*ivc_chan), GFP_KERNEL);
	if (!ivc_chan) {
		dev_err(dev, "Failed to allocate safety ivc channel\n");
		return -ENOMEM;
	}

	ivc_chan->name = devm_kstrdup(dev, ch_node->name, GFP_KERNEL);
	if (!ivc_chan->name)
		return -ENOMEM;


	/* Init IVC */
	ret = tegra_ivc_init_with_dma_handle(&ivc_chan->ivc,
			(uintptr_t)base + start.rx, dma_handle + start.rx,
			(uintptr_t)base + start.tx, dma_handle + start.tx,
			nframes, frame_size, dev,
			tegra_ivc_channel_ring);
	if (ret) {
		dev_err(dev, "IVC initialization error: %d\n", ret);
		goto error;
	}

	ivc_chan->safety_ivc = dev_get_drvdata(dev);
	safety_ivc->ivc_chan[ivc_chan_count] = ivc_chan;
	ivc_chan_count++;

	dev_info(dev, "%s: RX: 0x%x-0x%x TX: 0x%x-0x%x\n",
			ivc_chan->name, start.rx, end.rx, start.tx, end.tx);

	return 0;
error:
	return ret;
}

static int tegra_safety_ivc_parse_channel(struct device *dev)
{
	struct tegra_safety_ivc *safety_ivc = dev_get_drvdata(dev);
	struct of_phandle_args reg_spec;
	struct device_node *ch_node;
	int ret;

	ret = of_parse_phandle_with_fixed_args(dev->of_node, NV(ivc-channels),
			3, 0, &reg_spec);
	if (ret) {
		dev_err(dev, "failed to parse DT\n");
		return ret;
	}

	for_each_child_of_node(reg_spec.np, ch_node) {

		ret = tegra_ivc_channel_create(dev, ch_node,
						&safety_ivc->region);
		if (ret) {
			dev_err(dev, "failed to create a channel\n");
			return ret;
		}
	}

	return 0;
}

static int tegra_safety_ivc_parse_ast_region(struct device *dev)
{
	struct tegra_safety_ivc *safety_ivc = dev_get_drvdata(dev);
	struct safety_ast_region *region = &safety_ivc->region;
	struct of_phandle_args reg_spec;
	int ret;

	ret = of_parse_phandle_with_fixed_args(dev->of_node, NV(ivc-channels),
			3, 0, &reg_spec);
	if (ret) {
		dev_err(dev, "failed to parse AST info\n");
		return ret;
	}

	if (reg_spec.args_count < 3)
		return -EINVAL;

	region->ast_id = reg_spec.args[0];
	region->slave_base = reg_spec.args[1];
	region->size = reg_spec.args[2];

	/* Allocate RAM for IVC */
	region->base = dma_alloc_coherent(dev, region->size, &region->dma,
					  GFP_KERNEL | __GFP_ZERO);
	if (region->base == NULL) {
		dev_err(dev, "dma_alloc_coherent failed\n");
		return -ENOMEM;
	}

	dev_info(dev, "dma address = 0x%x\n", (unsigned int)region->dma);
	return 0;
}

static void tegra_safety_ast_region_free(struct device *dev)
{
	struct tegra_safety_ivc *safety_ivc = dev_get_drvdata(dev);
	struct safety_ast_region *region = &safety_ivc->region;

	dma_free_coherent(dev, region->size, region->base, region->dma);
}

static int tegra_safety_ivc_parse_hsp(struct device *dev)
{
	struct tegra_safety_ivc *safety_ivc = dev_get_drvdata(dev);
	struct device_node *hsp_node;
	int ret;

	hsp_node = of_get_child_by_name(dev->of_node, "hsp");
	safety_ivc->ivc_pair = of_tegra_hsp_sm_pair_by_name(hsp_node,
			"ivc-pair", tegra_safety_ivc_full_notify,
			NULL, safety_ivc);
	of_node_put(hsp_node);
	if (IS_ERR(safety_ivc->ivc_pair)) {
		ret = PTR_ERR(safety_ivc->ivc_pair);
		dev_err(dev, "failed to obtain ivc pair: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id tegra_safety_ivc_of_match[] = {
	{ .compatible = NV(tegra186-safety-ivc)},
	{ .compatible = NV(tegra194-safety-ivc)},
	{ }
};
MODULE_DEVICE_TABLE(of, tegra_safety_ivc_of_match);

static int tegra_safety_ivc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tegra_safety_ivc *safety_ivc = dev_get_drvdata(dev);
	int i;

	if (!safety_ivc)
		return 0;

	l1ss_exit(safety_ivc);

	for (i = 0; i < ivc_chan_count; i++)
		tegra_safety_dev_exit(dev, i);

	tegra_safety_ast_region_free(dev);
	tegra_hsp_sm_pair_free(safety_ivc->ivc_pair);
	destroy_workqueue(safety_ivc->wq);
	ivc_chan_count = 0;

	return 0;
}

static int tegra_safety_ivc_probe(struct platform_device *pdev)
{
	struct tegra_safety_ivc *safety_ivc;
	struct device *dev = &pdev->dev;
	int ret, i;
	nv_guard_request_t req;

	dev_info(dev, "Probing sce safety driver\n");

	safety_ivc = devm_kzalloc(dev, sizeof(*safety_ivc), GFP_KERNEL);
	if (!safety_ivc)
		return -ENOMEM;

	dev_set_drvdata(dev, safety_ivc);

	init_waitqueue_head(&safety_ivc->cmd.response_waitq);
	init_waitqueue_head(&safety_ivc->cmd.empty_waitq);
	safety_ivc->wq = alloc_workqueue("safety_cmdresp", WQ_HIGHPRI, 0);
	INIT_WORK(&safety_ivc->work, tegra_safety_cmdresp_work_func);
	mutex_init(&safety_ivc->rlock);
	mutex_init(&safety_ivc->wlock);

	ret = tegra_safety_ivc_parse_hsp(dev);
	if (ret) {
		dev_err(dev, "failed to get hsp: %d\n", ret);
		goto fail;
	}

	ret = tegra_safety_ivc_parse_ast_region(dev);
	if (ret) {
		dev_err(dev, "failed to get ast region: %d\n", ret);
		goto fail;
	}

	ret = tegra_safety_ivc_parse_channel(dev);
	if (ret) {
		dev_err(dev, "failed to get ivc channel info: %d\n", ret);
		goto fail;
	}

	ret = l1ss_init(safety_ivc);
	if (ret) {
		dev_err(dev, "failed to setup l1ss: %d\n", ret);
		goto fail;
	}
	/* inform sce that IVC setup is complete */
	ret = tegra_safety_ivc_setup_ready(dev);
	if (ret) {
		dev_err(dev, "failed to setup ivc: %d\n", ret);
		goto fail;
	}

	/* create user space safety cdevs */
	for (i = 0; i < ivc_chan_count; i++) {
		ret = tegra_safety_dev_init(dev, i);
		if (ret) {
			dev_err(dev, "failed to setup cdev %d\n", ret);
			goto fail;
		}
	}

	req.srv_id_cmd = NVGUARD_PHASE_NOTIFICATION;
	req.phase = NVGUARD_TEGRA_PHASE_INITDONE;
	ret = l1ss_submit_rq(&req, false);
	if (ret) {
		dev_err(dev, "failed to submit phase init done: %d\n", ret);
		goto fail;
	}
	dev_info(dev, "Successfully probed safety ivc driver\n");

	return 0;

fail:
	tegra_safety_ivc_remove(pdev);
	return ret;
}

static struct platform_driver tegra_safety_ivc_driver = {
	.driver = {
		.name	= "tegra186-safety-ivc",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_safety_ivc_of_match),
	},
	.probe = tegra_safety_ivc_probe,
	.remove = tegra_safety_ivc_remove,
};
module_platform_driver(tegra_safety_ivc_driver);

MODULE_DESCRIPTION("Safety L1-L2 communication driver");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL v2");
