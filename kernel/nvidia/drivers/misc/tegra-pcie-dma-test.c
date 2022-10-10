// SPDX-License-Identifier: GPL-2.0+
/*
 * PCIe DMA test framework for Tegra PCIe
 *
 * Copyright (C) 2021-2022 NVIDIA Corporation. All rights reserved.
 */

#include <linux/aer.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/pcie_dma.h>
#include <linux/random.h>
#include <linux/types.h>
#include <linux/tegra-pcie-edma-test-common.h>

#define MODULENAME "pcie_dma_host"

#define EDMA_LIB_TEST 1

struct ep_pvt {
	struct pci_dev *pdev;
	void __iomem *bar0_virt;
	void __iomem *dma_base;
	u32 dma_size;
	void *dma_virt;
	dma_addr_t dma_phy;
	dma_addr_t bar0_phy;
	struct dentry *debugfs;
	void *cookie;
	u32 stress_count;
	u32 edma_ch;
	u32 prev_edma_ch;
	u32 msi_irq;
	u64 msi_addr;
	u16 msi_data;
	phys_addr_t dma_phy_base;
	u32 dma_phy_size;
	u64 tsz;
	ktime_t edma_start_time[DMA_WR_CHNL_NUM];
	struct edmalib_common edma;
};

static irqreturn_t ep_isr(int irq, void *arg)
{
#ifndef EDMA_LIB_TEST
	struct ep_pvt *ep = (struct ep_pvt *)arg;
	struct pcie_epf_bar0 *epf_bar0 = ep->bar0_virt;
	int bit = 0;
	u32 val;
	unsigned long wr = DMA_WR_CHNL_MASK, rd = DMA_RD_CHNL_MASK;

	val = dma_common_rd(ep->dma_base, DMA_WRITE_INT_STATUS_OFF);
	for_each_set_bit(bit, &wr, DMA_WR_CHNL_NUM) {
		if (BIT(bit) & val) {
			dma_common_wr(ep->dma_base, BIT(bit),
				      DMA_WRITE_INT_CLEAR_OFF);
			epf_bar0->wr_data[bit].crc =
				crc32_le(~0,
					 ep->dma_virt + BAR0_DMA_BUF_OFFSET,
					 epf_bar0->wr_data[bit].size);
			/* Trigger interrupt to endpoint */
			writel(epf_bar0->msi_data[bit],
			       ep->bar0_virt + BAR0_MSI_OFFSET);
		}
	}

	val = dma_common_rd(ep->dma_base, DMA_READ_INT_STATUS_OFF);
	for_each_set_bit(bit, &rd, DMA_RD_CHNL_NUM) {
		if (BIT(bit) & val) {
			dma_common_wr(ep->dma_base, BIT(bit),
				      DMA_READ_INT_CLEAR_OFF);
			epf_bar0->rd_data[bit].crc =
				crc32_le(~0,
					 ep->dma_virt + BAR0_DMA_BUF_OFFSET,
					 epf_bar0->rd_data[bit].size);
			/* Trigger interrupt to endpoint */
			writel(epf_bar0->msi_data[DMA_WR_CHNL_NUM + bit],
			       ep->bar0_virt + BAR0_MSI_OFFSET);
		}
	}
#endif

	return IRQ_HANDLED;
}

extern struct device *naga_pci[];
struct edma_desc {
	dma_addr_t src;
	dma_addr_t dst;
	size_t sz;
};

static struct device *tegra_pci_dma_get_host_bridge_device(struct pci_dev *dev)
{
	struct pci_bus *bus = dev->bus;
	struct device *bridge;

	while (bus->parent)
		bus = bus->parent;

	bridge = bus->bridge;
	kobject_get(&bridge->kobj);

	return bridge;
}

static void  tegra_pci_dma_put_host_bridge_device(struct device *dev)
{
	kobject_put(&dev->kobj);
}

/* debugfs to perform eDMA lib transfers */
static int edmalib_test(struct seq_file *s, void *data)
{
	struct ep_pvt *ep = (struct ep_pvt *)dev_get_drvdata(s->private);
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)ep->bar0_virt;
	 /* RP uses 128M(used by EP) + 1M(reserved) offset for source and dest data transfers */
	dma_addr_t ep_dma_addr = epf_bar0->ep_phy_addr + SZ_128M + SZ_1M;
	dma_addr_t bar0_dma_addr = ep->bar0_phy + SZ_128M + SZ_1M;
	dma_addr_t rp_dma_addr = ep->dma_phy + SZ_128M + SZ_1M;
	struct pci_dev *pdev = ep->pdev;
	struct device *bridge, *rdev;
	struct edmalib_common *edma = &ep->edma;

	ep->edma.src_dma_addr = rp_dma_addr;
	ep->edma.src_virt = ep->dma_virt + SZ_128M + SZ_1M;
	ep->edma.fdev = &ep->pdev->dev;
	ep->edma.bar0_virt = ep->bar0_virt;
	ep->edma.bar0_phy = ep->bar0_phy;
	ep->edma.dma_base = ep->dma_base;

	if (REMOTE_EDMA_TEST_EN) {
		ep->edma.dst_dma_addr = ep_dma_addr;
		ep->edma.edma_remote.msi_addr = ep->msi_addr;
		ep->edma.edma_remote.msi_data = ep->msi_data;
		ep->edma.edma_remote.msi_irq = ep->msi_irq;
		ep->edma.edma_remote.dma_phy_base = ep->dma_phy_base;
		ep->edma.edma_remote.dma_size = ep->dma_phy_size;
		ep->edma.edma_remote.dev = &pdev->dev;
	} else {
		bridge = tegra_pci_dma_get_host_bridge_device(pdev);
		rdev = bridge->parent;
		tegra_pci_dma_put_host_bridge_device(bridge);
		ep->edma.of_node = rdev->of_node;
		ep->edma.dst_dma_addr = bar0_dma_addr;
	}

	return edmalib_common_test(&ep->edma);
}

static void init_debugfs(struct ep_pvt *ep)
{
	debugfs_create_devm_seqfile(&ep->pdev->dev, "edmalib_test", ep->debugfs, edmalib_test);

	debugfs_create_u32("edma_ch", 0644, ep->debugfs, &ep->edma.edma_ch);
	/* Enable remote dma ASYNC for ch 0 as default */
	ep->edma.edma_ch = 0x80000011;
	ep->edma.st_as_ch = -1;

	debugfs_create_u32("stress_count", 0644, ep->debugfs, &ep->edma.stress_count);
	ep->edma.stress_count = 10;

	debugfs_create_u32("dma_size", 0644, ep->debugfs, &ep->edma.dma_size);
	ep->edma.dma_size = SZ_1M;

	debugfs_create_u32("nents", 0644, ep->debugfs, &ep->edma.nents);
	/* Set DMA_LL_DEFAULT_SIZE as default nents, Max NUM_EDMA_DESC */
	ep->edma.nents = DMA_LL_DEFAULT_SIZE;
}

static int ep_test_dma_probe(struct pci_dev *pdev,
			     const struct pci_device_id *id)
{
	struct ep_pvt *ep;
	struct pcie_epf_bar0 *epf_bar0;
	int ret = 0;
	u32 val, i;
	u16 val_16;
	char *name;

	ep = devm_kzalloc(&pdev->dev, sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return -ENOMEM;

	ep->edma.ll_desc = devm_kzalloc(&pdev->dev, sizeof(*ep->edma.ll_desc) * NUM_EDMA_DESC,
					GFP_KERNEL);
	if (!ep->edma.ll_desc)
		return -ENOMEM;

	ep->pdev = pdev;
	pci_set_drvdata(pdev, ep);

	ret = pci_enable_device(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable PCI device\n");
		return ret;
	}

	pci_enable_pcie_error_reporting(pdev);

	pci_set_master(pdev);

	ret = pci_request_regions(pdev, MODULENAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request PCI regions\n");
		goto fail_region_request;
	}

	ep->bar0_phy = pci_resource_start(pdev, 0);
	ep->bar0_virt = devm_ioremap(&pdev->dev, ep->bar0_phy, pci_resource_len(pdev, 0));
	if (!ep->bar0_virt) {
		dev_err(&pdev->dev, "Failed to IO remap BAR0\n");
		ret = -ENOMEM;
		goto fail_region_remap;
	}

	ep->dma_base = devm_ioremap(&pdev->dev, pci_resource_start(pdev, 4),
				    pci_resource_len(pdev, 4));
	if (!ep->dma_base) {
		dev_err(&pdev->dev, "Failed to IO remap BAR4\n");
		ret = -ENOMEM;
		goto fail_region_remap;
	}

	if (pci_enable_msi(pdev) < 0) {
		dev_err(&pdev->dev, "Failed to enable MSI interrupt\n");
		ret = -ENODEV;
		goto fail_region_remap;
	}
	ret = request_irq(pdev->irq, (irq_handler_t)ep_isr, IRQF_SHARED,
			  "pcie_ep_isr", ep);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register isr\n");
		goto fail_isr;
	}

	/* Set MSI address and data in DMA registers */
#ifndef EDMA_LIB_TEST
	pci_read_config_dword(pdev, pdev->msi_cap + PCI_MSI_ADDRESS_LO, &val);
	dma_common_wr(ep->dma_base, val, DMA_WRITE_DONE_IMWR_LOW_OFF);
	dma_common_wr(ep->dma_base, val, DMA_WRITE_ABORT_IMWR_LOW_OFF);
	dma_common_wr(ep->dma_base, val, DMA_READ_DONE_IMWR_LOW_OFF);
	dma_common_wr(ep->dma_base, val, DMA_READ_ABORT_IMWR_LOW_OFF);

	pci_read_config_word(pdev, pdev->msi_cap + PCI_MSI_FLAGS, &val_16);
	if (val_16 & PCI_MSI_FLAGS_64BIT) {
		pci_read_config_dword(pdev, pdev->msi_cap + PCI_MSI_ADDRESS_HI,
				      &val);
		dma_common_wr(ep->dma_base, val, DMA_WRITE_DONE_IMWR_HIGH_OFF);
		dma_common_wr(ep->dma_base, val, DMA_WRITE_ABORT_IMWR_HIGH_OFF);
		dma_common_wr(ep->dma_base, val, DMA_READ_DONE_IMWR_HIGH_OFF);
		dma_common_wr(ep->dma_base, val, DMA_READ_ABORT_IMWR_HIGH_OFF);

		pci_read_config_word(pdev, pdev->msi_cap + PCI_MSI_DATA_64,
				     &val_16);
		val = val_16;
		val = (val << 16) | val_16;
		dma_common_wr(ep->dma_base, val, DMA_WRITE_IMWR_DATA_OFF_BASE);
		dma_common_wr(ep->dma_base, val,
			      DMA_WRITE_IMWR_DATA_OFF_BASE + 0x4);
		dma_common_wr(ep->dma_base, val, DMA_READ_IMWR_DATA_OFF_BASE);
	} else {
		pci_read_config_word(pdev, pdev->msi_cap + PCI_MSI_DATA_32,
				     &val_16);
		val = val_16;
		val = (val << 16) | val_16;
		dma_common_wr(ep->dma_base, val, DMA_WRITE_IMWR_DATA_OFF_BASE);
		dma_common_wr(ep->dma_base, val,
			      DMA_WRITE_IMWR_DATA_OFF_BASE + 0x4);
		dma_common_wr(ep->dma_base, val, DMA_READ_IMWR_DATA_OFF_BASE);
	}
#endif

	ep->dma_virt = dma_alloc_coherent(&pdev->dev, BAR0_SIZE, &ep->dma_phy,
					  GFP_KERNEL);
	if (!ep->dma_virt) {
		dev_err(&pdev->dev, "Failed to allocate DMA memory\n");
		ret = -ENOMEM;
		goto fail_dma_alloc;
	}
	get_random_bytes(ep->dma_virt, BAR0_SIZE);

	/* Update RP DMA system memory base address in BAR0 */
	epf_bar0 = (struct pcie_epf_bar0 *)ep->bar0_virt;
	epf_bar0->rp_phy_addr = ep->dma_phy;
	dev_info(&pdev->dev, "DMA mem, IOVA: 0x%llx size: %d\n", ep->dma_phy, BAR0_SIZE);

	pci_read_config_word(pdev, pdev->msi_cap + PCI_MSI_FLAGS, &val_16);
	if (val_16 & PCI_MSI_FLAGS_64BIT) {
		pci_read_config_dword(pdev, pdev->msi_cap + PCI_MSI_ADDRESS_HI, &val);
		ep->msi_addr = val;

		pci_read_config_word(pdev, pdev->msi_cap + PCI_MSI_DATA_64, &val_16);
		ep->msi_data = val_16;
	} else {
		pci_read_config_word(pdev, pdev->msi_cap + PCI_MSI_DATA_32, &val_16);
		ep->msi_data = val_16;
	}
	pci_read_config_dword(pdev, pdev->msi_cap + PCI_MSI_ADDRESS_LO, &val);
	ep->msi_addr = (ep->msi_addr << 32) | val;
	ep->msi_irq = pdev->irq;
	ep->dma_phy_base = pci_resource_start(pdev, 4);
	ep->dma_phy_size = pci_resource_len(pdev, 4);

	name = devm_kasprintf(&ep->pdev->dev, GFP_KERNEL, "%s_pcie_dma_test", dev_name(&pdev->dev));
	if (!name) {
		dev_err(&pdev->dev, "%s: Fail to set debugfs name\n", __func__);
		ret = -ENOMEM;
		goto fail_name;
	}

	for (i = 0; i < DMA_WR_CHNL_NUM; i++)
		init_waitqueue_head(&ep->edma.wr_wq[i]);

	for (i = 0; i < DMA_RD_CHNL_NUM; i++)
		init_waitqueue_head(&ep->edma.rd_wq[i]);

	ep->debugfs = debugfs_create_dir(name, NULL);
	init_debugfs(ep);

	return ret;

fail_name:
	dma_free_coherent(&pdev->dev, BAR0_SIZE, ep->dma_virt, ep->dma_phy);
fail_dma_alloc:
	free_irq(pdev->irq, ep);
fail_isr:
	pci_disable_msi(pdev);
fail_region_remap:
	pci_release_regions(pdev);
fail_region_request:
	pci_clear_master(pdev);

	return ret;
}

static void ep_test_dma_remove(struct pci_dev *pdev)
{
	struct ep_pvt *ep = pci_get_drvdata(pdev);

	debugfs_remove_recursive(ep->debugfs);
	tegra_pcie_edma_deinit(ep->cookie);
	dma_free_coherent(&pdev->dev, BAR0_SIZE, ep->dma_virt, ep->dma_phy);
	free_irq(pdev->irq, ep);
	pci_disable_msi(pdev);
	pci_release_regions(pdev);
	pci_clear_master(pdev);
}

static const struct pci_device_id ep_pci_tbl[] = {
	{ PCI_DEVICE(0x10DE, 0x229a)},
	{ PCI_DEVICE(0x10DE, 0x229c)},
	{},
};

MODULE_DEVICE_TABLE(pci, ep_pci_tbl);

static struct pci_driver ep_pci_driver = {
	.name		= MODULENAME,
	.id_table	= ep_pci_tbl,
	.probe		= ep_test_dma_probe,
	.remove		= ep_test_dma_remove,
};

module_pci_driver(ep_pci_driver);

MODULE_DESCRIPTION("Tegra PCIe client driver for endpoint DMA test func");
MODULE_LICENSE("GPL");
