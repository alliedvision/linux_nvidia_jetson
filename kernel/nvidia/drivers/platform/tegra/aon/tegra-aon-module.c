/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/sizes.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/tegra-cache.h>
#include <linux/io.h>

#include <aon.h>

#define FW_CARVEOUT_SIZE	SZ_1M
#define FW_CARVEOUT_VA		0x70000000

#define AON_CARVEOUT		8
#define AON_STREAMID		0x1
#define PHYS_STREAMID		0x7F

#define IPCBUF_SIZE		2097152

static const struct aon_platform_data t234_aon_platform_data = {
	.aon_stream_id	  = AON_STREAMID,
	.phys_stream_id	  = PHYS_STREAMID,
	.fw_carveout_id	  = AON_CARVEOUT,
	.fw_vmindex	  = 0,
	.fw_name	  = "spe_t234.bin",
	.fw_carveout_va	  = FW_CARVEOUT_VA,
	.fw_carveout_size = FW_CARVEOUT_SIZE,
	.fw_info_valid	  = true,
	.use_physical_id  = false,
};

static const struct of_device_id tegra_aon_of_match[] = {
	{
		.compatible = NV("tegra234-aon"),
		.data = (struct aon_platform_data *)&t234_aon_platform_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_aon_of_match);

static inline void tegra_aon_set_pdata(struct platform_device *pdev,
				       struct tegra_aon *aon)
{
	struct aon_platform_data *pdata = NULL;

	pdata = dev_get_drvdata(&pdev->dev);
	pdata->d = aon;
}

static inline struct tegra_aon *tegra_aon_get_pdata(struct platform_device *pd)
{
	struct aon_platform_data *pdata = NULL;

	pdata = dev_get_drvdata(&pd->dev);

	return pdata->d;
}

static int tegra_aon_init_dev_data(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tegra_aon *aon = NULL;
	struct device_node *dn;
	int ret = 0;

	aon = devm_kzalloc(dev, sizeof(struct tegra_aon), GFP_KERNEL);
	if (!aon) {
		ret = -ENOMEM;
		goto exit;
	}

	if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32))) {
		dev_err(&pdev->dev, "setting DMA MASK failed!\n");
	}

	tegra_aon_set_pdata(pdev, aon);
	aon->dev = dev;

	dn = aon->dev->of_node;
	aon->regs = of_iomap(dn, 0);
	if (!aon->regs) {
		dev_err(&pdev->dev, "Cannot map AON register space\n");
		ret = -ENOMEM;
		goto exit;
	}

exit:
	return ret;
}

static int  tegra_aon_setup_fw_carveout(struct tegra_aon *aon)
{
	struct device_node *dn = aon->dev->of_node;
	int ret = 0;

	aon->fw = devm_kzalloc(aon->dev,
			       sizeof(struct aon_firmware),
			       GFP_KERNEL);
	if (!aon->fw) {
		ret = -ENOMEM;
		goto exit;
	}

	aon->fw->data = dma_alloc_coherent(aon->dev,
					   FW_CARVEOUT_SIZE,
					   (dma_addr_t *)&aon->fw->dma_handle,
					   GFP_KERNEL);
	if (!aon->fw->data) {
		dev_err(aon->dev, "Couldn't alloc FW carveout\n");
		ret = -ENOMEM;
		goto exit;
	}

	memset(aon->fw->data, 0, FW_CARVEOUT_SIZE);
	tegra_flush_cache_all();

	ret = of_property_read_u32(dn,
				   NV("ivc-carveout-base-ss"),
				   &aon->ivc_carveout_base_ss);
	if (ret) {
		dev_err(aon->dev,
			"missing <%s> property\n",
			NV("ivc-carveout-base-ss"));
		goto exit;
	}

	ret = of_property_read_u32(dn,
				   NV("ivc-carveout-size-ss"),
				   &aon->ivc_carveout_size_ss);
	if (ret) {
		dev_err(aon->dev,
			"missing <%s> property\n",
			NV("ivc-carveout-size-ss"));
		goto exit;
	}

exit:
	return ret;
}

static int  tegra_aon_setup_ipc_carveout(struct tegra_aon *aon)
{
	struct device_node *dn = aon->dev->of_node;
	int ret = 0;

	aon->ipcbuf = dmam_alloc_coherent(aon->dev,
					  IPCBUF_SIZE,
					  &aon->ipcbuf_dma,
					  GFP_KERNEL | __GFP_ZERO);
	if (!aon->ipcbuf) {
		dev_err(aon->dev, "failed to allocate IPC memory\n");
		ret = -ENOMEM;
		goto exit;

	}
	aon->ipcbuf_size = IPCBUF_SIZE;

	ret = of_property_read_u32(dn, NV("ivc-rx-ss"), &aon->ivc_rx_ss);
	if (ret) {
		dev_err(aon->dev, "missing <%s> property\n", NV("ivc-rx-ss"));
		goto exit;
	}

	ret = of_property_read_u32(dn, NV("ivc-tx-ss"), &aon->ivc_tx_ss);
	if (ret) {
		dev_err(aon->dev, "missing <%s> property\n", NV("ivc-tx-ss"));
		goto exit;
	}

exit:
	return ret;
}

static int tegra_aon_probe(struct platform_device *pdev)
{
	struct tegra_aon *aon = NULL;
	struct device *dev = &pdev->dev;
	struct aon_platform_data *pdata = NULL;
	const struct of_device_id *match = NULL;
	int ret = 0;

	match = of_match_device(tegra_aon_of_match, dev);
	if (match == NULL) {
		dev_info(dev, "no matching of node\n");
		ret = -ENODATA;
		goto exit;
	}

	pdata = (struct aon_platform_data *)match->data;
	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(dev, "no platform data\n");
		ret = -ENODATA;
		goto exit;
	}
	dev_set_drvdata(dev, pdata);

	ret = tegra_aon_init_dev_data(pdev);
	if (ret) {
		dev_err(dev, "failed to init device data err = %d\n", ret);
		goto exit;
	}
	aon = pdata->d;

	ret = tegra_aon_setup_fw_carveout(aon);
	if (ret) {
		dev_err(dev, "failed to setup fw carveout err = %d\n", ret);
		goto exit;
	}

	ret = tegra_aon_setup_ipc_carveout(aon);
	if (ret) {
		dev_err(dev, "failed to setup ipc carveout err = %d\n", ret);
		goto exit;
	}

	ret = tegra_aon_mail_init(aon);
	if (ret) {
		dev_err(dev, "failed to init mail err = %d\n", ret);
		goto exit;
	}

	ret = tegra_aon_debugfs_create(aon);
	if (ret) {
		dev_err(dev, "failed to create debugfs err = %d\n", ret);
		goto exit;
	}

	ret = tegra_aon_ipc_init(aon);
	if (ret) {
		dev_err(dev, "failed to init ipc err = %d\n", ret);
		goto exit;
	}

	dev_info(aon->dev, "init done\n");

exit:
	return ret;
}

static int tegra_aon_remove(struct platform_device *pdev)
{
	struct tegra_aon *aon = tegra_aon_get_pdata(pdev);

	tegra_aon_debugfs_remove(aon);
	tegra_aon_mail_deinit(aon);

	return 0;
}

static struct platform_driver tegra234_aon_driver = {
	.driver = {
		.name	= "tegra234-aon",
		.of_match_table = tegra_aon_of_match,
	},
	.probe = tegra_aon_probe,
	.remove = tegra_aon_remove,
};
module_platform_driver(tegra234_aon_driver);

MODULE_DESCRIPTION("Tegra SPE driver");
MODULE_AUTHOR("akhumbum@nvidia.com");
MODULE_LICENSE("GPL v2");
