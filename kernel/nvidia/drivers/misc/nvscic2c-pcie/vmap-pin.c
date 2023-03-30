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

#define pr_fmt(fmt)	"nvscic2c-pcie: vmap-pin: " fmt

#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/dma-buf.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/nvhost.h>
#include <linux/nvhost_t194.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/types.h>

#include "common.h"
#include "module.h"
#include "pci-client.h"
#include "vmap.h"
#include "vmap-internal.h"

void
memobj_devmngd_unpin(struct vmap_ctx_t *vmap_ctx,
		     struct memobj_pin_t *pin)
{
	if (!vmap_ctx)
		return;

	if (!pin)
		return;

	if (pin->sgt) {
		dma_buf_unmap_attachment(pin->attach, pin->sgt,	pin->dir);
		pin->sgt = NULL;
	}

	if (pin->attach) {
		pci_client_dmabuf_detach(vmap_ctx->pci_client_h,
					 pin->dmabuf, pin->attach);
		pin->attach = NULL;
	}
}

int
memobj_devmngd_pin(struct vmap_ctx_t *vmap_ctx,
		   struct memobj_pin_t *pin)
{
	int ret = 0;
	u32 sg_index = 0;
	struct scatterlist *sg = NULL;

	if (pin->prot == VMAP_OBJ_PROT_WRITE)
		pin->dir = DMA_FROM_DEVICE;
	else
		pin->dir = DMA_TO_DEVICE;

	pin->attach = pci_client_dmabuf_attach(vmap_ctx->pci_client_h,
					       pin->dmabuf);
	if (IS_ERR_OR_NULL(pin->attach)) {
		ret = PTR_ERR(pin->attach);
		goto err;
	}
	pin->sgt = dma_buf_map_attachment(pin->attach, pin->dir);
	if (IS_ERR_OR_NULL(pin->sgt)) {
		ret = PTR_ERR(pin->sgt);
		goto err;
	}

	/* dma address (for all nents) are deemed contiguous for smmu=on.*/
	pin->attrib.iova = sg_dma_address(pin->sgt->sgl);
	for_each_sg(pin->sgt->sgl, sg, pin->sgt->nents, sg_index) {
		pin->attrib.size += sg->length;
	}

	/*
	 * dev mngd used in local mem or remote mem (when exporting from
	 * Tegra PCIe RP), in both cases, offsetof is not needed.
	 */
	pin->attrib.offsetof = 0;

	return ret;
err:
	memobj_devmngd_unpin(vmap_ctx, pin);
	return ret;
}

void
memobj_clientmngd_unpin(struct vmap_ctx_t *vmap_ctx,
			struct memobj_pin_t *pin)
{
	u32 i = 0;

	if (!vmap_ctx)
		return;

	if (!pin)
		return;

	if (pin->nents) {
		for (i = 0; i < pin->nr_nents; i++) {
			if (pin->nents[i].mapped_iova) {
				pci_client_unmap_addr(vmap_ctx->pci_client_h,
						      pin->nents[i].iova,
						      pin->nents[i].len);
				pin->nents[i].mapped_iova = false;
			}
		}
		kfree(pin->nents);
		pin->nents = NULL;
	}

	if (pin->iova_block_h) {
		pci_client_free_iova(vmap_ctx->pci_client_h,
				     &pin->iova_block_h);
		pin->iova_block_h = NULL;
	}

	if (pin->sgt) {
		dma_buf_unmap_attachment(pin->attach, pin->sgt, pin->dir);
		pin->sgt = NULL;
	}

	if (pin->attach) {
		dma_buf_detach(pin->dmabuf, pin->attach);
		pin->attach = NULL;
	}
}

int
memobj_clientmngd_pin(struct vmap_ctx_t *vmap_ctx,
		      struct memobj_pin_t *pin)
{
	int ret = 0;
	u64 iova = 0;
	u32 sg_index = 0;
	int prot = IOMMU_WRITE;
	struct scatterlist *sg = NULL;

	if (pin->prot == VMAP_OBJ_PROT_WRITE) {
		prot = IOMMU_WRITE;
		pin->dir = DMA_FROM_DEVICE;
	} else {
		prot = IOMMU_READ;
		pin->dir = DMA_TO_DEVICE;
	}

	/*
	 * pin to dummy device (which has smmu disabled) to get scatter-list
	 * of phys addr.
	 */
	pin->attach = dma_buf_attach(pin->dmabuf, &vmap_ctx->dummy_pdev->dev);
	if (IS_ERR_OR_NULL(pin->attach)) {
		ret = PTR_ERR(pin->attach);
		pr_err("client_mngd dma_buf_attach failed\n");
		goto err;
	}
	pin->sgt = dma_buf_map_attachment(pin->attach, pin->dir);
	if (IS_ERR_OR_NULL(pin->sgt)) {
		ret = PTR_ERR(pin->sgt);
		pr_err("client_mngd dma_buf_attachment failed\n");
		goto err;
	}

	for_each_sg(pin->sgt->sgl, sg, pin->sgt->nents, sg_index)
		pin->attrib.size += sg->length;

	/* get one contiguous iova.*/
	ret = pci_client_alloc_iova(vmap_ctx->pci_client_h, pin->attrib.size,
				    &pin->attrib.iova, &pin->attrib.offsetof,
				    &pin->iova_block_h);
	if (ret) {
		pr_err("Failed to reserve iova block of size: (%lu)\n",
		       pin->attrib.size);
		goto err;
	}

	/* pin the scatter list to contiguous iova.*/
	pin->nr_nents = pin->sgt->nents;
	pin->nents = kzalloc((sizeof(*pin->nents) * pin->nr_nents), GFP_KERNEL);
	if (WARN_ON(!pin->nents)) {
		ret = -ENOMEM;
		goto err;
	}

	iova = pin->attrib.iova;
	for_each_sg(pin->sgt->sgl, sg, pin->sgt->nents, sg_index) {
		phys_addr_t paddr = (phys_addr_t)(sg_phys(sg));

		pin->nents[sg_index].iova = iova;
		pin->nents[sg_index].len = sg->length;
		ret = pci_client_map_addr(vmap_ctx->pci_client_h,
					pin->nents[sg_index].iova, paddr,
					pin->nents[sg_index].len,
					(IOMMU_CACHE | prot));
		if (ret < 0) {
			pr_err("Failed: to iommu_map sg_nent: (%u), size: (%u)\n",
					sg_index, sg->length);
			goto err;
		}
		pin->nents[sg_index].mapped_iova = true;

		/* store information for unmap.*/
		iova += sg->length;
	}

	return ret;
err:
	memobj_clientmngd_unpin(vmap_ctx, pin);
	return ret;
}

void
memobj_unpin(struct vmap_ctx_t *vmap_ctx,
	     struct memobj_pin_t *pin)
{
	if (!vmap_ctx)
		return;

	if (!pin)
		return;

	if (pin->mngd == VMAP_MNGD_CLIENT)
		memobj_clientmngd_unpin(vmap_ctx, pin);
	else
		memobj_devmngd_unpin(vmap_ctx, pin);

	dma_buf_put(pin->dmabuf); // get_dma_buf();
}

int
memobj_pin(struct vmap_ctx_t *vmap_ctx,
	   struct memobj_pin_t *pin)
{
	int ret = 0;

	/* ref count till we unmap. */
	get_dma_buf(pin->dmabuf);

	if (pin->mngd == VMAP_MNGD_CLIENT)
		ret = memobj_clientmngd_pin(vmap_ctx, pin);
	else
		ret = memobj_devmngd_pin(vmap_ctx, pin);

	if (ret)
		memobj_unpin(vmap_ctx, pin);

	return ret;
}

void
syncobj_clientmngd_unpin(struct vmap_ctx_t *vmap_ctx,
			 struct syncobj_pin_t *pin)
{
	if (!vmap_ctx)
		return;

	if (!pin)
		return;

	if (pin->mapped_iova) {
		pci_client_unmap_addr(vmap_ctx->pci_client_h,
				      pin->attrib.iova, pin->attrib.size);
		pin->mapped_iova = false;
	}

	if (pin->iova_block_h) {
		pci_client_free_iova(vmap_ctx->pci_client_h,
				     &pin->iova_block_h);
		pin->iova_block_h = NULL;
	}
}

static int
syncobj_clientmngd_pin(struct vmap_ctx_t *vmap_ctx,
		       struct syncobj_pin_t *pin)
{
	int ret = 0;

	if (pin->prot != VMAP_OBJ_PROT_WRITE) {
		pr_err("Pinning syncobj with read access not supported\n");
		return -EOPNOTSUPP;
	}

	ret = pci_client_alloc_iova(vmap_ctx->pci_client_h, pin->attrib.size,
				    &pin->attrib.iova, &pin->attrib.offsetof,
				    &pin->iova_block_h);
	if (ret) {
		pr_err("Failed to reserve iova block of size: (%lu)\n",
		       pin->attrib.size);
		goto err;
	}

	ret = pci_client_map_addr(vmap_ctx->pci_client_h, pin->attrib.iova,
				  pin->phy_addr, pin->attrib.size,
				  (IOMMU_CACHE | IOMMU_WRITE));
	if (ret) {
		pr_err("Failed to pin syncpoint physical addr to client iova\n");
		goto err;
	}
	pin->mapped_iova = true;

	return ret;
err:
	syncobj_clientmngd_unpin(vmap_ctx, pin);
	return ret;
}

void
syncobj_unpin(struct vmap_ctx_t *vmap_ctx,
	      struct syncobj_pin_t *pin)
{
	if (!vmap_ctx)
		return;

	if (!pin)
		return;

	if (pin->pin_reqd) {
		if (pin->mngd == VMAP_MNGD_DEV)
			return;

		syncobj_clientmngd_unpin(vmap_ctx, pin);
	}

	// against nvhost_syncpt_get_ref_ext()
	nvhost_syncpt_put_ref_ext(vmap_ctx->host1x_pdev, pin->syncpt_id);
}

int
syncobj_pin(struct vmap_ctx_t *vmap_ctx,
	    struct syncobj_pin_t *pin)
{
	int ret = 0;
	size_t size = 0;

	/* take a reference until we unmap.*/
	nvhost_syncpt_get_ref_ext(vmap_ctx->host1x_pdev, pin->syncpt_id);

	ret = nvhost_syncpt_unit_interface_get_aperture(vmap_ctx->host1x_pdev,
							&pin->phy_addr, &size);
	if (ret) {
		pr_err("Failed to get syncpt aper for syncpt id:(%u)\n",
		       pin->syncpt_id);
		goto err;
	}
	pin->phy_addr +=
		nvhost_syncpt_unit_interface_get_byte_offset(pin->syncpt_id);

	pin->attrib.syncpt_id = pin->syncpt_id;
	pin->attrib.size = SP_SIZE;

	if (pin->pin_reqd) {
		/*
		 * remote/export sync obj are mapped to an iova of client
		 * choice always and we should not come here for local sync objs
		 */
		if (pin->mngd == VMAP_MNGD_DEV) {
			ret = -EOPNOTSUPP;
			goto err;
		}

		ret = syncobj_clientmngd_pin(vmap_ctx, pin);
	}
err:
	if (ret)
		syncobj_unpin(vmap_ctx, pin);

	return ret;
}
