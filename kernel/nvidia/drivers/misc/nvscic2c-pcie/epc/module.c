// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#define pr_fmt(fmt)	"nvscic2c-pcie: epc: " fmt

#include <linux/dma-iommu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pci.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/tegra-pcie-edma.h>
#include <linux/types.h>

#include "comm-channel.h"
#include "common.h"
#include "endpoint.h"
#include "module.h"
#include "pci-client.h"
#include "vmap.h"

/*
 * Following Ids are reserved in pci_ids.h, refer:
 * PCI_DEVICE_ID_NVIDIA_C2C_1, PCI_DEVICE_ID_NVIDIA_C2C_2 and
 * PCI_DEVICE_ID_NVIDIA_C2C_3
 */
static const struct pci_device_id nvscic2c_pcie_epc_tbl[] = {
	{ PCI_DEVICE(0x10DE, 0x22CB) },
	{ PCI_DEVICE(0x10DE, 0x22CC) },
	{ PCI_DEVICE(0x10DE, 0x22CD) },
	{},
};

/* wrapper over tegra-pcie-edma init api. */
static int
edma_module_init(struct driver_ctx_t *drv_ctx)
{
	u8 i = 0;
	int ret = 0;
	struct tegra_pcie_edma_init_info info = {0};

	if (WARN_ON(!drv_ctx || !drv_ctx->drv_param.edma_np))
		return -EINVAL;

	memset(&info, 0x0, sizeof(info));
	info.np = drv_ctx->drv_param.edma_np;
	info.edma_remote = NULL;
	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		info.tx[i].ch_type = EDMA_CHAN_XFER_ASYNC;
		info.tx[i].num_descriptors = NUM_EDMA_DESC;
	}
	/*No use-case for RD channels.*/

	drv_ctx->edma_h = tegra_pcie_edma_initialize(&info);
	if (!drv_ctx->edma_h)
		ret = -ENODEV;

	return ret;
}

/* should not have any ongoing eDMA transfers.*/
static void
edma_module_deinit(struct driver_ctx_t *drv_ctx)
{
	if (!drv_ctx || !drv_ctx->edma_h)
		return;

	tegra_pcie_edma_deinit(drv_ctx->edma_h);
	drv_ctx->edma_h = NULL;
}

static void
free_inbound_area(struct pci_dev *pdev, struct dma_buff_t *self_mem)
{
	if (!pdev || !self_mem || !self_mem->dma_handle)
		return;

	iommu_dma_free_iova(&pdev->dev, self_mem->dma_handle, self_mem->size);
	self_mem->dma_handle = 0x0;
}

/*
 * Allocate area visible to PCIe EP/DRV_MODE_EPF. To have symmetry between the
 * two modules, even PCIe RP/DRV_MODE_EPC allocates an empty area for all writes
 * from PCIe EP/DRV_MODE_EPF to land into. Also, all CPU access from PCIe EP/
 * DRV_MODE_EPF need be for one continguous region.
 */
static int
allocate_inbound_area(struct pci_dev *pdev, size_t win_size,
		      struct dma_buff_t *self_mem)
{
	int ret = 0;

	/* allocate same area size as that of exported by PCIe EP.*/
	self_mem->size = win_size;
	self_mem->dma_handle =
		 iommu_dma_alloc_iova(&pdev->dev, self_mem->size,
				      (&pdev->dev)->coherent_dma_mask);
	if (!self_mem->dma_handle) {
		ret = -ENOMEM;
		pr_err("iommu_dma_alloc_iova() failed for size:(0x%lx)\n",
		       self_mem->size);
	}

	return ret;
}

static void
free_outbound_area(struct pci_dev *pdev, struct pci_aper_t *peer_mem)
{
	if (!pdev || !peer_mem)
		return;

	peer_mem->aper = 0x0;
	peer_mem->size = 0;
}

/* Assign outbound pcie aperture for CPU/eDMA access towards PCIe EP. */
static int
assign_outbound_area(struct pci_dev *pdev, size_t win_size,
		     struct pci_aper_t *peer_mem)
{
	int ret = 0;

	peer_mem->size = win_size;
	peer_mem->aper = pci_resource_start(pdev, 0);

	return ret;
}

/* Handle link message from @DRV_MODE_EPC. */
static void
link_msg_cb(void *data, void *ctx)
{
	struct comm_msg *msg = (struct comm_msg *)data;
	struct driver_ctx_t *drv_ctx = (struct driver_ctx_t *)ctx;

	if (WARN_ON(!msg || !drv_ctx))
		return;

	/* inidicate link status to application.*/
	pci_client_change_link_status(drv_ctx->pci_client_h,
				      msg->u.link.status);
}

static void
nvscic2c_pcie_epc_remove(struct pci_dev *pdev)
{
	struct driver_ctx_t *drv_ctx = NULL;

	if (!pdev)
		return;

	drv_ctx = pci_get_drvdata(pdev);
	if (!drv_ctx)
		return;

	pci_client_change_link_status(drv_ctx->pci_client_h,
				      NVSCIC2C_PCIE_LINK_DOWN);
	endpoints_core_deinit(drv_ctx->endpoints_h);
	edma_module_deinit(drv_ctx);
	endpoints_release(&drv_ctx->endpoints_h);
	vmap_deinit(&drv_ctx->vmap_h);
	comm_channel_deinit(&drv_ctx->comm_channel_h);
	pci_client_deinit(&drv_ctx->pci_client_h);
	free_outbound_area(pdev, &drv_ctx->peer_mem);
	free_inbound_area(pdev, &drv_ctx->self_mem);

	pci_release_region(pdev, 0);
	pci_clear_master(pdev);
	pci_disable_device(pdev);

	dt_release(&drv_ctx->drv_param);

	pci_set_drvdata(pdev, NULL);
	kfree_const(drv_ctx->drv_name);
	kfree(drv_ctx);
}

static int
nvscic2c_pcie_epc_probe(struct pci_dev *pdev,
			const struct pci_device_id *id)
{
	int ret = 0;
	char *name = NULL;
	size_t win_size = 0;
	struct comm_msg msg = {0};
	struct callback_ops cb_ops = {0};
	struct driver_ctx_t *drv_ctx = NULL;
	struct pci_client_params params = {0};

	/* allocate module context.*/
	drv_ctx = kzalloc(sizeof(*drv_ctx), GFP_KERNEL);
	if (WARN_ON(!drv_ctx))
		return -ENOMEM;

	name = kasprintf(GFP_KERNEL, "%s-%x", DRIVER_NAME_EPC, id->device);
	if (WARN_ON(!name)) {
		kfree(drv_ctx);
		return -ENOMEM;
	}

	drv_ctx->drv_mode = DRV_MODE_EPC;
	drv_ctx->drv_name = name;
	pci_set_drvdata(pdev, drv_ctx);

	/* check for the device tree node against this Id, must be only one.*/
	ret = dt_parse(id->device, drv_ctx->drv_mode, &drv_ctx->drv_param);
	if (ret)
		goto err_dt_parse;

	ret = pcim_enable_device(pdev);
	if (ret)
		goto err_enable_device;
	pci_set_master(pdev);
	ret = pci_request_region(pdev, 0, MODULE_NAME);
	if (ret)
		goto err_request_region;

	win_size = pci_resource_len(pdev, 0);
	ret = allocate_inbound_area(pdev, win_size, &drv_ctx->self_mem);
	if (ret)
		goto err_alloc_inbound;
	ret = assign_outbound_area(pdev, win_size, &drv_ctx->peer_mem);
	if (ret)
		goto err_assign_outbound;

	params.dev = &pdev->dev;
	params.self_mem = &drv_ctx->self_mem;
	params.peer_mem = &drv_ctx->peer_mem;
	ret = pci_client_init(&params, &drv_ctx->pci_client_h);
	if (ret) {
		pr_err("pci_client_init() failed\n");
		goto err_pci_client;
	}
	pci_client_save_driver_ctx(drv_ctx->pci_client_h, drv_ctx);
	pci_client_save_peer_cpu(drv_ctx->pci_client_h, NVCPU_ORIN);

	ret = comm_channel_init(drv_ctx, &drv_ctx->comm_channel_h);
	if (ret) {
		pr_err("Failed to initialize comm-channel\n");
		goto err_comm_init;
	}

	ret = vmap_init(drv_ctx, &drv_ctx->vmap_h);
	if (ret) {
		pr_err("Failed to initialize vmap\n");
		goto err_vmap_init;
	}

	ret = edma_module_init(drv_ctx);
	if (ret) {
		pr_err("Failed to initialize edma module\n");
		goto err_edma_init;
	}

	ret = endpoints_setup(drv_ctx, &drv_ctx->endpoints_h);
	if (ret) {
		pr_err("Failed to initialize endpoints\n");
		goto err_endpoints_init;
	}

	/* register for link status message from @DRV_MODE_EPF (PCIe EP).*/
	cb_ops.callback = link_msg_cb;
	cb_ops.ctx = (void *)drv_ctx;
	ret = comm_channel_register_msg_cb(drv_ctx->comm_channel_h,
					   COMM_MSG_TYPE_LINK, &cb_ops);
	if (ret) {
		pr_err("Failed to register for link message from EP\n");
		goto err_register_msg;
	}

	/*
	 * share iova with @DRV_MODE_EPF for it's outbound translation.
	 * This must be send only after comm-channel, endpoint memory backing
	 * is created and mapped to self_mem. @DRV_MODE_EPF on seeing this
	 * message shall send link-up message over comm-channel and possibly
	 * applications can also start endpoint negotiation, therefore.
	 */
	msg.type = COMM_MSG_TYPE_BOOTSTRAP;
	msg.u.bootstrap.iova = drv_ctx->self_mem.dma_handle;
	msg.u.bootstrap.peer_cpu = NVCPU_ORIN;
	ret = comm_channel_bootstrap_msg_send(drv_ctx->comm_channel_h, &msg);
	if (ret) {
		pr_err("Failed to send comm bootstrap message\n");
		goto err_msg_send;
	}

	pci_set_drvdata(pdev, drv_ctx);
	return ret;

err_msg_send:
err_register_msg:
	endpoints_release(&drv_ctx->endpoints_h);

err_endpoints_init:
	edma_module_deinit(drv_ctx);

err_edma_init:
	vmap_deinit(&drv_ctx->vmap_h);

err_vmap_init:
	comm_channel_deinit(&drv_ctx->comm_channel_h);

err_comm_init:
	pci_client_deinit(&drv_ctx->pci_client_h);

err_pci_client:
	free_outbound_area(pdev, &drv_ctx->peer_mem);

err_assign_outbound:
	free_inbound_area(pdev, &drv_ctx->self_mem);

err_alloc_inbound:
	pci_release_region(pdev, 0);

err_request_region:
	pci_clear_master(pdev);
	pci_disable_device(pdev);

err_enable_device:
	dt_release(&drv_ctx->drv_param);

err_dt_parse:
	pci_set_drvdata(pdev, NULL);
	kfree_const(drv_ctx->drv_name);
	kfree(drv_ctx);
	return ret;
}

MODULE_DEVICE_TABLE(pci, nvscic2c_pcie_epc_tbl);
static struct pci_driver nvscic2c_pcie_epc_driver = {
	.name		= DRIVER_NAME_EPC,
	.id_table	= nvscic2c_pcie_epc_tbl,
	.probe		= nvscic2c_pcie_epc_probe,
	.remove		= nvscic2c_pcie_epc_remove,
};
module_pci_driver(nvscic2c_pcie_epc_driver);

#define DRIVER_LICENSE		"GPL v2"
#define DRIVER_DESCRIPTION	"NVIDIA Chip-to-Chip transfer module for PCIeRP"
#define DRIVER_AUTHOR		"Nvidia Corporation"
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE(DRIVER_LICENSE);
MODULE_AUTHOR(DRIVER_AUTHOR);
