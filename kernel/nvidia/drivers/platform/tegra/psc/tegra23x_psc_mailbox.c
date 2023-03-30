/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>

/* from drivers/mailbox/mailbox.h */
#include "mailbox.h"

#include "tegra23x_psc.h"

#define MBOX_NUM	8
#define MBOX_REG_OFFSET	0x10000
/* 16 32-bit registers for MBOX_CHAN_IN/OUT */
#define MBOX_MSG_SIZE	16

#define MBOX_CHAN_ID	0x0

#define MBOX_CHAN_EXT_CTRL	0x4
#define MBOX_CHAN_PSC_CTRL	0x8
/* bit to indicate remote that IN parameters are ready. */
#define MBOX_IN_VALID	BIT(0)
/* bit to indicate remote that OUT parameters are read out */
#define MBOX_OUT_DONE	BIT(4)
#define LIC_INTR_EN	BIT(8)
#define MBOX_OUT_VALID	BIT(0)

#define MBOX_CHAN_TX	0x800
#define MBOX_CHAN_RX	0x1000

struct psc_mbox;

struct mbox_vm_chan {
	unsigned int irq;
	void __iomem *base;
	struct psc_mbox *parent;
};

struct psc_mbox {
	struct device *dev;
	void __iomem *vm_chan_base;
	struct mbox_chan chan[MBOX_NUM];
	struct mbox_controller mbox;
	struct mbox_vm_chan vm_chan[MBOX_NUM];
};

static irqreturn_t  psc_mbox_rx_interrupt(int irq, void *p)
{
	u32 data[MBOX_MSG_SIZE];
	struct mbox_chan *chan = p;
	struct mbox_vm_chan *vm_chan = chan->con_priv;
	struct device *dev = vm_chan->parent->dev;
	u32 ext_ctrl;
	u32 psc_ctrl;
	int i;

	psc_ctrl = readl(vm_chan->base + MBOX_CHAN_PSC_CTRL);
	/* not a valid case but it does happen. */
	if ((psc_ctrl & MBOX_OUT_VALID) == 0) {
		ext_ctrl = readl(vm_chan->base + MBOX_CHAN_EXT_CTRL);
		dev_err_once(dev, "invalid interrupt, psc_ctrl: 0x%08x ext_ctrl: 0x%08x\n",
			psc_ctrl, ext_ctrl);
		return IRQ_HANDLED;
	}

	for (i = 0; i < MBOX_MSG_SIZE; i++)
		data[i] = readl(vm_chan->base + MBOX_CHAN_RX + i * 4);

	mbox_chan_received_data(chan, data);
	/* finish read */
	ext_ctrl = readl(vm_chan->base + MBOX_CHAN_EXT_CTRL);
	ext_ctrl |= MBOX_OUT_DONE;
	writel(ext_ctrl, vm_chan->base + MBOX_CHAN_EXT_CTRL);

	return IRQ_HANDLED;
}

static int psc_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct mbox_vm_chan  *vm_chan = chan->con_priv;
	struct device *dev = vm_chan->parent->dev;
	u32 *buf = data;
	int i;
	u32 ext_ctrl;

	ext_ctrl = readl(vm_chan->base + MBOX_CHAN_EXT_CTRL);

	if ((ext_ctrl & MBOX_IN_VALID) != 0) {
		dev_err(dev, "%s:pending write.\n", __func__);
		return -EBUSY;
	}
	for (i = 0; i < MBOX_MSG_SIZE; i++)
		writel(buf[i], vm_chan->base + MBOX_CHAN_TX + i * 4);

	ext_ctrl |= MBOX_IN_VALID;
	writel(ext_ctrl, vm_chan->base + MBOX_CHAN_EXT_CTRL);
	return 0;
}

static int psc_mbox_startup(struct mbox_chan *chan)
{
	struct mbox_vm_chan  *vm_chan = chan->con_priv;
	u32 ext_ctrl = LIC_INTR_EN;

	writel(ext_ctrl, vm_chan->base + MBOX_CHAN_EXT_CTRL);
	chan->txdone_method = TXDONE_BY_ACK;
	return 0;
}

static void psc_mbox_shutdown(struct mbox_chan *chan)
{
	struct mbox_vm_chan  *vm_chan = chan->con_priv;
	struct device *dev = vm_chan->parent->dev;

	dev_dbg(dev, "%s\n", __func__);
	writel(0, vm_chan->base + MBOX_CHAN_EXT_CTRL);
}

static const struct mbox_chan_ops psc_mbox_ops = {
	.send_data = psc_mbox_send_data,
	.startup = psc_mbox_startup,
	.shutdown = psc_mbox_shutdown,
};

static int tegra234_psc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct psc_mbox *psc;
	void __iomem *base;
	int i;
	int ret;

	dev_dbg(dev, "psc driver init\n");

	psc = devm_kzalloc(dev, sizeof(*psc), GFP_KERNEL);
	if (!psc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(dev, "ioremap failed\n");
		return PTR_ERR(base);
	}

	psc->vm_chan_base = base;
	psc->dev = dev;

	for (i = 0; i < MBOX_NUM; i++) {
		int irq;

		irq = platform_get_irq(pdev, i);
		if (irq < 0) {
			dev_err(dev, "Unable to get IRQ %d\n", irq);
			return irq;
		}
		ret = devm_request_irq(dev, irq, psc_mbox_rx_interrupt,
				IRQF_ONESHOT, dev_name(dev), &psc->chan[i]);
		if (ret) {
			dev_err(dev, "Unable to acquire IRQ %d\n", irq);
			return ret;
		}
		psc->chan[i].con_priv = &psc->vm_chan[i];
		psc->vm_chan[i].parent = psc;
		psc->vm_chan[i].irq = irq;
		psc->vm_chan[i].base = base + (MBOX_REG_OFFSET * i);
		dev_dbg(dev, "vm_chan[%d].base:%p, chan_id:0x%x, irq:%d\n",
			i, psc->vm_chan[i].base,
			readl(psc->vm_chan[i].base), irq);
	}
	psc->mbox.dev = dev;
	psc->mbox.chans = &psc->chan[0];
	psc->mbox.num_chans = MBOX_NUM;
	psc->mbox.ops = &psc_mbox_ops;
	/* drive txdone by mailbox client ACK with tx_block set to false */
	psc->mbox.txdone_irq = false;
	psc->mbox.txdone_poll = false;

	platform_set_drvdata(pdev, psc);

	ret = mbox_controller_register(&psc->mbox);
	if (ret) {
		dev_err(dev, "Failed to register mailboxes %d\n", ret);
		return ret;
	}

	psc_debugfs_create(pdev);
	dev_info(dev, "init done\n");

	return 0;
}

static int tegra234_psc_remove(struct platform_device *pdev)
{
	struct psc_mbox *psc = platform_get_drvdata(pdev);

	dev_err(&pdev->dev, "%s:%d\n", __func__, __LINE__);

	psc_debugfs_remove(pdev);

	mbox_controller_unregister(&psc->mbox);

	return 0;
}

static const struct of_device_id tegra234_psc_match[] = {
	{ .compatible = "nvidia,tegra234-psc", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, tegra234_psc_match);

static struct platform_driver tegra234_psc_driver = {
	.probe          = tegra234_psc_probe,
	.remove         = tegra234_psc_remove,
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "tegra23x-psc",
		.of_match_table = of_match_ptr(tegra234_psc_match),
	},
};

module_platform_driver(tegra234_psc_driver);
MODULE_DESCRIPTION("Tegra PSC driver");
MODULE_AUTHOR("dpu@nvidia.com");
MODULE_LICENSE("GPL v2");
