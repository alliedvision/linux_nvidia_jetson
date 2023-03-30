/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <dt-bindings/interconnect/tegra_icc_id.h>
#include <linux/interconnect.h>
#include <linux/interconnect-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "tegra_icc.h"
#if KERNEL_VERSION(4, 15, 0) < LINUX_VERSION_CODE
#include <soc/tegra/fuse.h>
#endif

DEFINE_TNODE(icc_primary, TEGRA_ICC_PRIMARY, TEGRA_ICC_NONE);
DEFINE_TNODE(debug, TEGRA_ICC_DEBUG, TEGRA_ICC_NISO);
DEFINE_TNODE(display, TEGRA_ICC_DISPLAY, TEGRA_ICC_ISO_DISPLAY);
DEFINE_TNODE(vi, TEGRA_ICC_VI, TEGRA_ICC_ISO_VI);
DEFINE_TNODE(eqos, TEGRA_ICC_EQOS, TEGRA_ICC_NISO);
DEFINE_TNODE(cpu_cluster0, TEGRA_ICC_CPU_CLUSTER0, TEGRA_ICC_NISO);
DEFINE_TNODE(cpu_cluster1, TEGRA_ICC_CPU_CLUSTER1, TEGRA_ICC_NISO);
DEFINE_TNODE(cpu_cluster2, TEGRA_ICC_CPU_CLUSTER2, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_0, TEGRA_ICC_PCIE_0, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_1, TEGRA_ICC_PCIE_1, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_2, TEGRA_ICC_PCIE_2, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_3, TEGRA_ICC_PCIE_3, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_4, TEGRA_ICC_PCIE_4, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_5, TEGRA_ICC_PCIE_5, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_6, TEGRA_ICC_PCIE_6, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_7, TEGRA_ICC_PCIE_7, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_8, TEGRA_ICC_PCIE_8, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_9, TEGRA_ICC_PCIE_9, TEGRA_ICC_NISO);
DEFINE_TNODE(pcie_10, TEGRA_ICC_PCIE_10, TEGRA_ICC_NISO);
DEFINE_TNODE(dla_0, TEGRA_ICC_DLA_0, TEGRA_ICC_NISO);
DEFINE_TNODE(dla_1, TEGRA_ICC_DLA_1, TEGRA_ICC_NISO);
DEFINE_TNODE(sdmmc_1, TEGRA_ICC_SDMMC_1, TEGRA_ICC_NISO);
DEFINE_TNODE(sdmmc_2, TEGRA_ICC_SDMMC_2, TEGRA_ICC_NISO);
DEFINE_TNODE(sdmmc_3, TEGRA_ICC_SDMMC_3, TEGRA_ICC_NISO);
DEFINE_TNODE(sdmmc_4, TEGRA_ICC_SDMMC_4, TEGRA_ICC_NISO);
DEFINE_TNODE(nvdec, TEGRA_ICC_NVDEC, TEGRA_ICC_NISO);
DEFINE_TNODE(nvenc, TEGRA_ICC_NVENC, TEGRA_ICC_NISO);
DEFINE_TNODE(nvjpg_0, TEGRA_ICC_NVJPG_0, TEGRA_ICC_NISO);
DEFINE_TNODE(nvjpg_1, TEGRA_ICC_NVJPG_1, TEGRA_ICC_NISO);
DEFINE_TNODE(ofaa, TEGRA_ICC_OFAA, TEGRA_ICC_NISO);
DEFINE_TNODE(xusb_host, TEGRA_ICC_XUSB_HOST, TEGRA_ICC_NISO);
DEFINE_TNODE(xusb_dev, TEGRA_ICC_XUSB_DEV, TEGRA_ICC_NISO);
DEFINE_TNODE(tsec, TEGRA_ICC_TSEC, TEGRA_ICC_NISO);
DEFINE_TNODE(vic, TEGRA_ICC_VIC, TEGRA_ICC_NISO);
DEFINE_TNODE(ape, TEGRA_ICC_APE, TEGRA_ICC_ISO_AUDIO);
DEFINE_TNODE(apedma, TEGRA_ICC_APEDMA, TEGRA_ICC_ISO_AUDIO);
DEFINE_TNODE(se, TEGRA_ICC_SE, TEGRA_ICC_NISO);
DEFINE_TNODE(gpu, TEGRA_ICC_GPU, TEGRA_ICC_NISO);
DEFINE_TNODE(cactmon, TEGRA_ICC_CACTMON, TEGRA_ICC_NISO);
DEFINE_TNODE(isp, TEGRA_ICC_ISP, TEGRA_ICC_NISO); /* non ISO camera */
DEFINE_TNODE(hda, TEGRA_ICC_HDA, TEGRA_ICC_ISO_AUDIO);
DEFINE_TNODE(vifal, TEGRA_ICC_VIFAL, TEGRA_ICC_ISO_VIFAL);
DEFINE_TNODE(vi2fal, TEGRA_ICC_VI2FAL, TEGRA_ICC_ISO_VIFAL);
DEFINE_TNODE(vi2, TEGRA_ICC_VI2, TEGRA_ICC_ISO_VI);
DEFINE_TNODE(rce, TEGRA_ICC_RCE, TEGRA_ICC_NISO);
DEFINE_TNODE(pva, TEGRA_ICC_PVA, TEGRA_ICC_NISO);
DEFINE_TNODE(nvpmodel, TEGRA_ICC_NVPMODEL, TEGRA_ICC_NONE);

static struct tegra_icc_node *tegra_icc_nodes[] = {
	[TEGRA_ICC_PRIMARY] = &icc_primary,
	[TEGRA_ICC_DEBUG] = &debug,
	[TEGRA_ICC_DISPLAY] = &display,
	[TEGRA_ICC_VI] = &vi,
	[TEGRA_ICC_EQOS] = &eqos,
	[TEGRA_ICC_CPU_CLUSTER0] = &cpu_cluster0,
	[TEGRA_ICC_CPU_CLUSTER1] = &cpu_cluster1,
	[TEGRA_ICC_CPU_CLUSTER2] = &cpu_cluster2,
	[TEGRA_ICC_PCIE_0] = &pcie_0,
	[TEGRA_ICC_PCIE_1] = &pcie_1,
	[TEGRA_ICC_PCIE_2] = &pcie_2,
	[TEGRA_ICC_PCIE_3] = &pcie_3,
	[TEGRA_ICC_PCIE_4] = &pcie_4,
	[TEGRA_ICC_PCIE_5] = &pcie_5,
	[TEGRA_ICC_PCIE_6] = &pcie_6,
	[TEGRA_ICC_PCIE_7] = &pcie_7,
	[TEGRA_ICC_PCIE_8] = &pcie_8,
	[TEGRA_ICC_PCIE_9] = &pcie_9,
	[TEGRA_ICC_PCIE_10] = &pcie_10,
	[TEGRA_ICC_DLA_0] = &dla_0,
	[TEGRA_ICC_DLA_1] = &dla_1,
	[TEGRA_ICC_SDMMC_1] = &sdmmc_1,
	[TEGRA_ICC_SDMMC_2] = &sdmmc_2,
	[TEGRA_ICC_SDMMC_3] = &sdmmc_3,
	[TEGRA_ICC_SDMMC_4] = &sdmmc_4,
	[TEGRA_ICC_NVDEC] = &nvdec,
	[TEGRA_ICC_NVENC] = &nvenc,
	[TEGRA_ICC_NVJPG_0] = &nvjpg_0,
	[TEGRA_ICC_NVJPG_1] = &nvjpg_1,
	[TEGRA_ICC_OFAA] = &ofaa,
	[TEGRA_ICC_XUSB_HOST] = &xusb_host,
	[TEGRA_ICC_XUSB_DEV] = &xusb_dev,
	[TEGRA_ICC_TSEC] = &tsec,
	[TEGRA_ICC_VIC] = &vic,
	[TEGRA_ICC_APE] = &ape,
	[TEGRA_ICC_APEDMA] = &apedma,
	[TEGRA_ICC_SE] = &se,
	[TEGRA_ICC_GPU] = &gpu,
	[TEGRA_ICC_CACTMON] = &cactmon,
	[TEGRA_ICC_ISP] = &isp,
	[TEGRA_ICC_HDA] = &hda,
	[TEGRA_ICC_VIFAL] = &vifal,
	[TEGRA_ICC_VI2FAL] = &vi2fal,
	[TEGRA_ICC_VI2] = &vi2,
	[TEGRA_ICC_RCE] = &rce,
	[TEGRA_ICC_PVA] = &pva,
	[TEGRA_ICC_NVPMODEL] = &nvpmodel,
};

static int tegra_icc_probe(struct platform_device *pdev)
{
	const struct tegra_icc_ops *ops;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct tegra_icc_node **tnodes;
	struct tegra_icc_provider *tp;
	struct icc_node *node;
	size_t num_nodes, i;
	long rate;
	int ret;

	ops = of_device_get_match_data(&pdev->dev);
	if (!ops)
		return -EINVAL;

	tnodes = tegra_icc_nodes;
	num_nodes = ARRAY_SIZE(tegra_icc_nodes);

	tp = devm_kzalloc(&pdev->dev, sizeof(*tp), GFP_KERNEL);
	if (!tp)
		return -ENOMEM;

	data = devm_kcalloc(&pdev->dev, num_nodes, sizeof(*node), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	provider = &tp->provider;
	provider->dev = &pdev->dev;
	provider->set = ops->plat_icc_set;
	provider->aggregate = ops->plat_icc_aggregate;
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 15, 0)
	provider->get_bw = ops->plat_icc_get_bw;
#endif
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;

	tp->dev = &pdev->dev;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 15, 0)
	tp->bpmp_dev = tegra_bpmp_get(tp->dev);
	if (IS_ERR(tp->bpmp_dev)) {
		dev_err(&pdev->dev, "bpmp_get failed\n");
		return -EPROBE_DEFER;
	}
#endif

	tp->dram_clk = of_clk_get_by_name(pdev->dev.of_node, "emc");
	if (IS_ERR_OR_NULL(tp->dram_clk)) {
		dev_err(&pdev->dev, "couldn't find emc clock\n");
		ret = PTR_ERR(tp->dram_clk);
		tp->dram_clk = NULL;
		goto err_bpmp;
	}
	clk_prepare_enable(tp->dram_clk);

	if (tegra_platform_is_silicon()) {
		rate = clk_round_rate(tp->dram_clk, ULONG_MAX);
		if (rate < 0) {
			dev_err(&pdev->dev, "couldn't get emc clk max rate\n");
			ret = rate;
			goto err_bpmp;
		} else {
			tp->max_rate = rate;
			tp->cap_rate = tp->max_rate;
		}

		rate = clk_round_rate(tp->dram_clk, 0);
		if (rate < 0) {
			dev_err(&pdev->dev, "couldn't get emc clk min rate\n");
			ret = rate;
			goto err_bpmp;
		} else
			tp->min_rate = rate;
	}

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(&pdev->dev, "error adding interconnect provider\n");
		goto err_bpmp;
	}

	for (i = 0; i < num_nodes; i++) {
		node = icc_node_create(tnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}

		node->name = tnodes[i]->name;
		node->data = tnodes[i];
		icc_node_add(node, provider);

		dev_dbg(&pdev->dev, "registered node %p %s %d\n", node,
			tnodes[i]->name, node->id);

		icc_link_create(node, TEGRA_ICC_PRIMARY);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	platform_set_drvdata(pdev, tp);

	dev_dbg(&pdev->dev, "Registered TEGRA ICC\n");

	return ret;
err:
	list_for_each_entry(node, &provider->nodes, node_list) {
		icc_node_del(node);
		icc_node_destroy(node->id);
	}

	icc_provider_del(provider);
err_bpmp:
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 15, 0)
	tegra_bpmp_put(tp->bpmp_dev);
#endif
	return ret;
}

static int tegra_icc_remove(struct platform_device *pdev)
{
	struct tegra_icc_provider *tp = platform_get_drvdata(pdev);
	struct icc_provider *provider = &tp->provider;
	struct icc_node *n;

	list_for_each_entry(n, &provider->nodes, node_list) {
		icc_node_del(n);
		icc_node_destroy(n->id);
	}
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 15, 0)
	tegra_bpmp_put(tp->bpmp_dev);
#endif

	return icc_provider_del(provider);
}

static const struct of_device_id tegra_icc_of_match[] = {
	{ .compatible = "nvidia,tegra23x-icc", .data = &tegra23x_icc_ops },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_icc_of_match);

static struct platform_driver tegra_icc_driver = {
	.probe = tegra_icc_probe,
	.remove = tegra_icc_remove,
	.driver = {
		.name = "tegra-icc",
		.of_match_table = tegra_icc_of_match,
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 15, 0)
		.sync_state = icc_sync_state,
#endif
	},
};
module_platform_driver(tegra_icc_driver);

MODULE_AUTHOR("Sanjay Chandrashekara <sanjayc@nvidia.com>");
MODULE_DESCRIPTION("Tegra ICC driver");
MODULE_LICENSE("GPL v2");
