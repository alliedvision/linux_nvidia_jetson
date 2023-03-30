// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe driver to enumerate PCIe virtual functions in VM.
 *
 * Copyright (C) 2021-2022, NVIDIA Corporation. All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pci-ecam.h>
#include <linux/pci.h>
#include <linux/platform_device.h>

static void pci_tegra_vf_ecam_free(void *ptr)
{
	pci_ecam_free((struct pci_config_window *)ptr);
}

static struct pci_config_window *pci_tegra_vf_ecam_init(struct device *dev,
		struct pci_host_bridge *bridge, const struct pci_ecam_ops *ops)
{
	struct resource cfgres;
	struct pci_config_window *cfg;
	static struct resource busn_res = {
		.start = 0,
		.end = 255,
		.flags = IORESOURCE_BUS,
	};
	int err;

	err = of_address_to_resource(dev->of_node, 0, &cfgres);
	if (err) {
		dev_err(dev, "missing \"reg\" property\n");
		return ERR_PTR(err);
	}

	cfg = pci_ecam_create(dev, &cfgres, &busn_res, ops);
	if (IS_ERR(cfg)) {
		dev_err(dev, "pci_ecam_create() failed\n");
		return cfg;
	}

	err = devm_add_action_or_reset(dev, pci_tegra_vf_ecam_free, cfg);
	if (err) {
		dev_err(dev, "devm_add_action_or_reset() failed\n");
		return ERR_PTR(err);
	}

	return cfg;
}

static int pci_tegra_vf_claim_resource(struct pci_dev *dev, void *data)
{
	int i;
	struct resource *res;

	for (i = 0; i < PCI_NUM_RESOURCES; i++) {
		res = &dev->resource[i];

		if (!res->parent && res->start && res->flags)
			if (pci_claim_resource(dev, i))
				dev_err(&dev->dev, "can't claim resource %pR\n",
					res);
	}

	return 0;
}

static int pci_tegra_vf_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pci_host_bridge *bridge;
	struct pci_config_window *cfg;
	struct pci_bus *bus;
	LIST_HEAD(resources);
	static struct resource busn_res = {
		.start = 0,
		.end = 255,
		.flags = IORESOURCE_BUS,
	};

	bridge = pci_alloc_host_bridge(0);
	if (!bridge) {
		dev_err(dev, "pci_alloc_host_bridge() failed\n");
		return -ENOMEM;
	}

	/* Parse and map our Configuration Space windows */
	cfg = pci_tegra_vf_ecam_init(dev, bridge, &pci_generic_ecam_ops);
	if (IS_ERR(cfg)) {
		dev_err(dev, "pci_tegra_vf_ecam_init() failed\n");
		return PTR_ERR(cfg);
	}

	bridge->sysdata = cfg;
	bridge->ops = (struct pci_ops *)&pci_generic_ecam_ops.pci_ops;

	platform_set_drvdata(pdev, bridge);

	pci_add_resource(&resources, &ioport_resource);
	pci_add_resource(&resources, &iomem_resource);
	pci_add_resource(&resources, &busn_res);

	pci_lock_rescan_remove();

	bus = pci_scan_root_bus(dev, 0, bridge->ops, cfg, &resources);
	if (!bus) {
		dev_err(dev, "pci_scan_root_bus() failed\n");
		pci_unlock_rescan_remove();
		pci_free_resource_list(&resources);
		return -ENOMEM;
	}

	pci_walk_bus(bus, pci_tegra_vf_claim_resource, pdev);

	pci_bus_add_devices(bus);

	pci_unlock_rescan_remove();

	return 0;
}

static int pci_tegra_vf_remove(struct platform_device *pdev)
{
	struct pci_host_bridge *bridge = platform_get_drvdata(pdev);

	pci_lock_rescan_remove();
	pci_stop_root_bus(bridge->bus);
	pci_remove_root_bus(bridge->bus);
	pci_unlock_rescan_remove();

	return 0;
}

static const struct of_device_id pci_tegra_vf_of_match[] = {
	{ .compatible = "pcie-tegra-vf", },
	{ },
};
MODULE_DEVICE_TABLE(of, pci_tegra_vf_of_match);

static struct platform_driver pci_tegra_vf_driver = {
	.driver = {
		.name = "pcie-tegra-vf",
		.of_match_table = pci_tegra_vf_of_match,
	},
	.probe = pci_tegra_vf_probe,
	.remove = pci_tegra_vf_remove,
};
module_platform_driver(pci_tegra_vf_driver);

MODULE_AUTHOR("Manikanta Maddireddy <mmaddireddy@nvidia.com>");
MODULE_LICENSE("GPL v2");
