/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/version.h>

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/tegra-hsp.h>

#include <aon.h>
#include <aon-hsp-combo.h>

#define TX_BLOCK_PERIOD 20

struct aon_hsp {
	struct tegra_hsp_sm_rx *rx;
	struct tegra_hsp_sm_tx *tx;
	struct device dev;
	void (*full_notify)(void *data, u32 value);
	void *pdata;
};

static void aon_hsp_rx_full_notify(void *data, u32 msg)
{
	struct aon_hsp *aonhsp = data;

	aonhsp->full_notify(aonhsp->pdata, msg);
}

static int aon_hsp_probe(struct aon_hsp *aonhsp)
{
	struct device_node *np = aonhsp->dev.parent->of_node;
	int err = -ENODEV;

	np = of_get_compatible_child(np, "nvidia,tegra-aon-hsp");
	if (!of_device_is_available(np)) {
		of_node_put(np);
		dev_err(&aonhsp->dev, "no hsp protocol \"%s\"\n",
				"nvidia,tegra-aon-hsp");
		return -ENODEV;
	}

	aonhsp->dev.of_node = np;

	/* Fetch the shared mailbox associated with IVC rx */
	aonhsp->rx = of_tegra_hsp_sm_rx_by_name(np, "ivc-rx",
						aon_hsp_rx_full_notify, aonhsp);
	if (IS_ERR(aonhsp->rx)) {
		err = PTR_ERR(aonhsp->rx);
		if (err != -EPROBE_DEFER)
			dev_err(&aonhsp->dev, "failed to fetch rx sm : %d\n",
				err);
		goto fail;
	}

	/* Fetch the shared mailbox associated with IVC tx */
	aonhsp->tx = of_tegra_hsp_sm_tx_by_name(np, "ivc-tx", NULL, aonhsp);
	if (IS_ERR(aonhsp->tx)) {
		err = PTR_ERR(aonhsp->tx);
		if (err != -EPROBE_DEFER)
			dev_err(&aonhsp->dev, "failed to fetch tx sm : %d\n",
				err);
		goto fail;
	}

	dev_set_name(&aonhsp->dev, "%s:%s", dev_name(aonhsp->dev.parent),
				 aonhsp->dev.of_node->name);
	dev_dbg(&aonhsp->dev, "probed\n");

	return 0;

fail:
	if (err != -EPROBE_DEFER) {
		dev_err(&aonhsp->dev, "%s: failed to obtain : %d\n",
			np->name, err);
	}
	of_node_put(np);
	return err;
}

static const struct device_type aon_hsp_combo_dev_type = {
	.name	= "aon-hsp-protocol",
};

static void aon_hsp_combo_dev_release(struct device *dev)
{
	struct aon_hsp *aonhsp = container_of(dev, struct aon_hsp, dev);

	if (!IS_ERR_OR_NULL(aonhsp->rx))
		tegra_hsp_sm_rx_free(aonhsp->rx);
	if (!IS_ERR_OR_NULL(aonhsp->tx))
		tegra_hsp_sm_tx_free(aonhsp->tx);

	of_node_put(dev->of_node);
	kfree(aonhsp);
}

static void aon_hsp_free(struct aon_hsp *aonhsp)
{
	if (IS_ERR_OR_NULL(aonhsp))
		return;

	if (dev_get_drvdata(&aonhsp->dev) != NULL)
		device_unregister(&aonhsp->dev);
	else
		put_device(&aonhsp->dev);
}

static struct aon_hsp *aon_hsp_create(struct device *dev,
				void (*full_notify)(void *data, u32 value),
				void *pdata)
{
	struct aon_hsp *aonhsp;
	int ret = -EINVAL;

	aonhsp = kzalloc(sizeof(*aonhsp), GFP_KERNEL);
	if (aonhsp == NULL)
		return ERR_PTR(-ENOMEM);

	aonhsp->dev.parent = dev;
	aonhsp->full_notify = full_notify;
	aonhsp->pdata = pdata;

	aonhsp->dev.type = &aon_hsp_combo_dev_type;
	aonhsp->dev.release = aon_hsp_combo_dev_release;
	device_initialize(&aonhsp->dev);

	dev_set_name(&aonhsp->dev, "%s:%s", dev_name(dev), "hsp");

	ret = aon_hsp_probe(aonhsp);
	if (ret < 0)
		goto fail;

	ret = device_add(&aonhsp->dev);
	if (ret < 0)
		goto fail;

	dev_set_drvdata(&aonhsp->dev, aonhsp);

	return aonhsp;

fail:
	aon_hsp_free(aonhsp);
	return ERR_PTR(ret);
}

bool tegra_aon_hsp_sm_tx_is_empty(struct tegra_aon *aon)
{
	struct aon_hsp *aonhsp = aon->hsp;

	return tegra_hsp_sm_tx_is_empty(aonhsp->tx);
}
EXPORT_SYMBOL(tegra_aon_hsp_sm_tx_is_empty);

int tegra_aon_hsp_sm_tx_write(struct tegra_aon *aon, u32 value)
{
	struct aon_hsp *aonhsp = aon->hsp;

	tegra_hsp_sm_tx_write(aonhsp->tx, value);

	return 0;
}
EXPORT_SYMBOL(tegra_aon_hsp_sm_tx_write);

int tegra_aon_hsp_sm_pair_request(struct tegra_aon *aon,
				  void (*full_notify)(void *data, u32 value),
				  void *pdata)
{
	struct device_node *hsp_node;
	struct device *dev = aon->dev;
	struct device_node *dn = dev->of_node;

	hsp_node = of_get_child_by_name(dn, "hsp");
	if (hsp_node == NULL) {
		dev_err(dev, "No hsp child node for AON\n");
		return -ENODEV;
	}

	aon->hsp = aon_hsp_create(dev, full_notify, pdata);
	if (IS_ERR(aon->hsp)) {
		aon->hsp = NULL;
		return PTR_ERR(aon->hsp);
	}

	return 0;
}
EXPORT_SYMBOL(tegra_aon_hsp_sm_pair_request);

void tegra_aon_hsp_sm_pair_free(struct tegra_aon *aon)
{
	struct aon_hsp *aonhsp = aon->hsp;

	if (aonhsp) {
		tegra_hsp_sm_rx_free(aonhsp->rx);
		tegra_hsp_sm_tx_free(aonhsp->tx);
		aon_hsp_free(aonhsp);
		aon->hsp = NULL;
	}
}
EXPORT_SYMBOL(tegra_aon_hsp_sm_pair_free);
