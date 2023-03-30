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
#define pr_fmt(fmt)	"nvscic2c-pcie: pci-client: " fmt

#include <linux/dma-buf.h>
#include <linux/dma-iommu.h>
#include <linux/dma-map-ops.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/pci-epc.h>
#include <linux/pci-epf.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <asm/cacheflush.h>

#include <uapi/misc/nvscic2c-pcie-ioctl.h>
#include <linux/tegra-pcie-edma.h>

#include "common.h"
#include "iova-mngr.h"
#include "pci-client.h"

/* Anticipate as many users as endpoints in worst-case. */
#define MAX_LINK_EVENT_USERS	(MAX_ENDPOINTS)

/*
 * BAR0 base + Offset:64K, Size=64K is not usable. We cannot leave a hole so
 * mark all first 128K as to be skipped for use.
 */
#define SKIP_BAR_AREA		(SZ_128K)

/* Internal private data-structure as PCI client. */
struct pci_client_t {
	struct device *dev;
	struct iommu_domain *domain;

	/* Recv area. Peer's write reflect here. */
	struct dma_buff_t *self_mem;

	/* Send area. PCIe aperture area. Self's Write reach Peer via this.*/
	struct pci_aper_t *peer_mem;

	/* PCI link status memory. mmap() to user-space.*/
	atomic_t link_status;
	struct cpu_buff_t link_status_mem;

	/*
	 * Lock to guard users getting un/registered and link status change
	 * invocation at the same time. Also, to protect table from concurrent
	 * access.
	 */
	struct mutex event_tbl_lock;

	/* Table of users registered for change in PCI link status. */
	struct event_t {
		/* is taken.*/
		atomic_t in_use;

		/* callback to invoke when change in status is seen.*/
		struct callback_ops cb_ops;
	} event_tbl[MAX_LINK_EVENT_USERS];

	/*
	 * Skip reserved iova for use. This area in BAR0 aperture is reserved for
	 * GIC SPI interrupt mechanism. As the allocation, fragmentration
	 * of iova must be identical on both @DRV_MODE_EPF and @DRV_MODE_EPC
	 * skip this area for use in @DRV_MODE_EPC also. We skip by reserving
	 * the iova region and thereby marking it as unusable.
	 */
	dma_addr_t edma_ch_desc_iova;
	void *skip_iova;
	void *skip_meta;
	void *edma_ch_desc_iova_h;
	/*
	 * iova-mngr instance for managing the reserved iova region.
	 * application allocated objs and endpoints allocated physical memory
	 * are pinned to this address.
	 */
	void *mem_mngr_h;

	/*
	 * the context of DRV_MODE_EPC/DRV_MODE_EPF
	 */
	struct driver_ctx_t *drv_ctx;

};

static void
free_link_status_mem(struct pci_client_t *ctx)
{
	if (!ctx || !ctx->link_status_mem.pva)
		return;

	kfree(ctx->link_status_mem.pva);
	ctx->link_status_mem.pva = NULL;
}

static int
allocate_link_status_mem(struct pci_client_t *ctx)
{
	int ret = 0;
	struct cpu_buff_t *mem = &ctx->link_status_mem;

	mem->size = PAGE_ALIGN(sizeof(enum nvscic2c_pcie_link));
	mem->pva  = kzalloc(mem->size, GFP_KERNEL);
	if (WARN_ON(!mem->pva))
		return -ENOMEM;

	atomic_set(&ctx->link_status, NVSCIC2C_PCIE_LINK_DOWN);
	*((enum nvscic2c_pcie_link *)mem->pva) = NVSCIC2C_PCIE_LINK_DOWN;

	/* physical address to be mmap() in user-space.*/
	mem->phys_addr = virt_to_phys(mem->pva);

	return ret;
}

/* Allocate desc_iova and mapping to bar0 for remote edma, x86-orin c2c only */
static int
pci_client_allocate_edma_rx_desc_iova(void *pci_client_h)
{
	int ret = 0;
	int prot = 0;
	void *pva;
	u64 phys_addr;
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;
	if (WARN_ON(!ctx))
		return -EINVAL;

	/*
	 *bar0 first 128K [-------128k-------]
	 *                [-4k-][-60k-][-64k-]
	 *first 4K reserved for meta data communication
	 *next 60k for x86/peer edma rx descriptor
	 *next 64K resered for sys-sw
	 */
	ret = iova_mngr_block_reserve(ctx->mem_mngr_h, SZ_4K,
				NULL, NULL, &ctx->skip_meta);
	if (ret) {
		pr_err("Failed to skip the 4K reserved iova region\n");
		return ret;
	}
	pva = alloc_pages_exact(60*SZ_1K, (GFP_KERNEL | __GFP_ZERO));
	if (!pva) {
		pr_err("Failed to allocate a page with size of 60K\n");
		return -ENOMEM;
	}
	phys_addr = page_to_phys(virt_to_page(pva));
	ret = pci_client_alloc_iova(ctx, 60*SZ_1K,
				&ctx->edma_ch_desc_iova, NULL,
				&ctx->edma_ch_desc_iova_h);
	if (ret) {
		pr_err("pci client failed to allocate iova with size of 60k\n");
		return ret;
	}
	prot = (IOMMU_CACHE | IOMMU_READ | IOMMU_WRITE);
	ret = pci_client_map_addr(ctx, ctx->edma_ch_desc_iova,
				phys_addr, 60*SZ_1K, prot);
	if (ret) {
		pr_err("ci client failed to map iova to 60K physical backing\n");
		return ret;
	}
	/* bar0+64K - bar0+126K  reserved for sys-sw  */
	ret = iova_mngr_block_reserve(ctx->mem_mngr_h, SZ_64K,
				NULL, NULL, &ctx->skip_iova);
	if (ret) {
		pr_err("Failed to skip the 64K reserved iova region\n");
		return ret;
	}
	return ret;
}

int
pci_client_init(struct pci_client_params *params, void **pci_client_h)
{
	u32 i = 0;
	int ret = 0;
	struct pci_client_t *ctx = NULL;

	/* should not be an already instantiated pci client context. */
	if (WARN_ON(!pci_client_h || *pci_client_h
		    || !params || !params->self_mem || !params->peer_mem
		    || !params->dev))
		return -EINVAL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (WARN_ON(!ctx))
		return -ENOMEM;
	ctx->dev = params->dev;
	ctx->self_mem = params->self_mem;
	ctx->peer_mem = params->peer_mem;
	mutex_init(&ctx->event_tbl_lock);

	/* for link event notifications. */
	for (i = 0; i < MAX_LINK_EVENT_USERS; i++)
		atomic_set(&ctx->event_tbl[i].in_use, 0);

	ret = allocate_link_status_mem(ctx);
	if (ret)
		goto err;

	/*
	 * for mapping application objs and endpoint physical memory to remote
	 * visible area.
	 */
	ctx->domain = iommu_get_domain_for_dev(ctx->dev);
	if (WARN_ON(!ctx->domain)) {
		ret = -ENODEV;
		pr_err("iommu not available for the pci device\n");
		goto err;
	}

	/* assumption : PCIe to be fully IO Coherent. Validate the assumption.*/
	if (WARN_ON(!dev_is_dma_coherent(ctx->dev))) {
		ret = -ENODEV;
		pr_err("io-coherency not enabled for the pci device\n");
		goto err;
	}

	/*
	 * configure iova manager for inbound/self_mem. Application
	 * supplied objs shall be pinned to this area.
	 */
	ret = iova_mngr_init("self_mem",
			     ctx->self_mem->dma_handle, ctx->self_mem->size,
			     &ctx->mem_mngr_h);
	if (ret) {
		pr_err("Failed to initialize iova memory manager\n");
		goto err;
	}

	/*
	 * Skip reserved iova for any use. This area in BAR0 is reserved for
	 * GIC SPI interrupt mechanism. As the allocation, fragmentration
	 * of iova must be identical on both @DRV_MODE_EPF and @DRV_MODE_EPC
	 * skip this area for use in @DRV_MODE_EPC also. We skip by reserving
	 * the iova region and thereby marking it as unusable for others.
	 */
	/* remote edma on x86 */
	ret = pci_client_allocate_edma_rx_desc_iova(ctx);
	if (ret) {
		pr_err("Failed to skip the reserved iova region\n");
		goto err;
	}

	*pci_client_h = ctx;
	return ret;

err:
	pci_client_deinit((void **)&ctx);
	return ret;
}

void
pci_client_deinit(void **pci_client_h)
{
	struct pci_client_t *ctx = (struct pci_client_t *)(*pci_client_h);

	if (!ctx)
		return;

	if (ctx->skip_iova) {
		iova_mngr_block_release(ctx->mem_mngr_h, &ctx->skip_iova);
		ctx->skip_iova = NULL;
	}

	if (ctx->skip_meta) {
		iova_mngr_block_release(ctx->mem_mngr_h, &ctx->skip_meta);
		ctx->skip_meta = NULL;
	}

	if (ctx->edma_ch_desc_iova) {
		iova_mngr_block_release(ctx->mem_mngr_h, &ctx->edma_ch_desc_iova_h);
		ctx->edma_ch_desc_iova_h = NULL;
	}

	if (ctx->mem_mngr_h) {
		iova_mngr_deinit(&ctx->mem_mngr_h);
		ctx->mem_mngr_h = NULL;
	}

	free_link_status_mem(ctx);
	mutex_destroy(&ctx->event_tbl_lock);
	kfree(ctx);

	*pci_client_h = NULL;
}

int
pci_client_alloc_iova(void *pci_client_h, size_t size, u64 *iova,
		      size_t *offset, void **block_h)
{
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!ctx))
		return -EINVAL;

	return iova_mngr_block_reserve(ctx->mem_mngr_h, size,
				       iova, offset, block_h);
}

int
pci_client_free_iova(void *pci_client_h, void **block_h)
{
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (!ctx || !block_h)
		return -EINVAL;

	return iova_mngr_block_release(ctx->mem_mngr_h, block_h);
}

int
pci_client_map_addr(void *pci_client_h, u64 to_iova, phys_addr_t paddr,
		    size_t size, int prot)
{
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!ctx || !to_iova || !paddr || !size))
		return -EINVAL;

	return iommu_map(ctx->domain, to_iova, paddr, size, prot);
}

size_t
pci_client_unmap_addr(void *pci_client_h, u64 from_iova, size_t size)
{
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (!ctx || !ctx->domain)
		return 0;

	return iommu_unmap(ctx->domain, from_iova, size);
}

int
pci_client_get_peer_aper(void *pci_client_h, size_t offsetof, size_t size,
			 phys_addr_t *phys_addr)
{
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!ctx || !size || !phys_addr))
		return -EINVAL;

	if (ctx->peer_mem->size < (offsetof + size))
		return -ENOMEM;

	*phys_addr = ctx->peer_mem->aper + offsetof;

	return 0;
}

struct dma_buf_attachment *
pci_client_dmabuf_attach(void *pci_client_h, struct dma_buf *dmabuff)
{
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!ctx || !dmabuff))
		return ERR_PTR(-EINVAL);

	return dma_buf_attach(dmabuff, ctx->dev);
}

void
pci_client_dmabuf_detach(void *pci_client_h, struct dma_buf *dmabuff,
			 struct dma_buf_attachment *attach)
{
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (!ctx || !dmabuff || !attach)
		return;

	return dma_buf_detach(dmabuff, attach);
}

/* Helper function to mmap the PCI link status memory to user-space.*/
int
pci_client_mmap_link_mem(void *pci_client_h, struct vm_area_struct *vma)
{
	int ret = 0;
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!vma))
		return -EINVAL;

	if (WARN_ON(!ctx || !ctx->link_status_mem.pva))
		return -EINVAL;

	if ((vma->vm_end - vma->vm_start) != ctx->link_status_mem.size)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = remap_pfn_range(vma,
			      vma->vm_start,
			      PFN_DOWN(ctx->link_status_mem.phys_addr),
			      ctx->link_status_mem.size,
			      vma->vm_page_prot);
	if (ret)
		pr_err("remap_pfn_range returns error: (%d) for Link mem\n", ret);

	return ret;
}

/* Query PCI link status. */
enum nvscic2c_pcie_link
pci_client_query_link_status(void *pci_client_h)
{
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!ctx))
		return NVSCIC2C_PCIE_LINK_DOWN;

	return atomic_read(&ctx->link_status);
}

/*
 * Users/Units can register for PCI link events as received by
 * @@DRV_MODE_EPF or @DRV_MODE_EPC module sbstraction.
 */
int
pci_client_register_for_link_event(void *pci_client_h,
				   struct callback_ops *ops, u32 *id)
{
	u32 i = 0;
	int ret = 0;
	struct event_t *event = NULL;
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!ctx))
		return -EINVAL;

	if (WARN_ON(!id || !ops || !ops->callback))
		return -EINVAL;

	mutex_lock(&ctx->event_tbl_lock);
	for (i = 0; i < MAX_LINK_EVENT_USERS; i++) {
		event = &ctx->event_tbl[i];
		if (!atomic_read(&event->in_use)) {
			event->cb_ops.callback = ops->callback;
			event->cb_ops.ctx = ops->ctx;
			atomic_set(&event->in_use, 1);
			*id = i;
			break;
		}
	}
	if (i == MAX_LINK_EVENT_USERS) {
		ret = -ENOMEM;
		pr_err("PCI link event registration full\n");
	}
	mutex_unlock(&ctx->event_tbl_lock);

	return ret;
}

/* Unregister for PCI link events. - teardown only. */
int
pci_client_unregister_for_link_event(void *pci_client_h, u32 id)
{
	int ret = 0;
	struct event_t *event = NULL;
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!ctx))
		return -EINVAL;

	if (WARN_ON(id >= MAX_LINK_EVENT_USERS))
		return -EINVAL;

	mutex_lock(&ctx->event_tbl_lock);
	event = &ctx->event_tbl[id];
	if (atomic_read(&event->in_use)) {
		atomic_set(&event->in_use, 0);
		event->cb_ops.callback = NULL;
		event->cb_ops.ctx = NULL;
	}
	mutex_unlock(&ctx->event_tbl_lock);

	return ret;
}

/*
 * Update the PCI link status as received in @DRV_MODE_EPF or @DRV_MODE_EPC
 * module abstraction. Propagate the link status to registered users.
 */
int
pci_client_change_link_status(void *pci_client_h,
			      enum nvscic2c_pcie_link status)
{
	u32 i = 0;
	int ret = 0;
	struct event_t *event = NULL;
	struct callback_ops *ops = NULL;
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!ctx))
		return -EINVAL;

	if (WARN_ON(status < NVSCIC2C_PCIE_LINK_DOWN ||
		    status > NVSCIC2C_PCIE_LINK_UP))
		return -EINVAL;

	/*
	 * Reflect the status for user-space to see.
	 * For consistent view of same phys_addr by user-space, flush the update
	 * Call is arm64 specific.
	 */
	atomic_set(&ctx->link_status, status);
	*((enum nvscic2c_pcie_link *)ctx->link_status_mem.pva) = status;
	__flush_dcache_area(ctx->link_status_mem.pva,
			    ctx->link_status_mem.size);

	/* interrupt registered users. */
	mutex_lock(&ctx->event_tbl_lock);
	for (i = 0; i < MAX_LINK_EVENT_USERS; i++) {
		event = &ctx->event_tbl[i];
		if (atomic_read(&event->in_use)) {
			ops = &event->cb_ops;
			ops->callback(NULL, ops->ctx);
		}
	}
	mutex_unlock(&ctx->event_tbl_lock);

	return ret;
}

/*
 * Helper functions to set and get driver context from  pci_client t
 *
 */
/*Set driver context of DRV_MODE_EPF or DRV_MODE_EPC */
int
pci_client_save_driver_ctx(void *pci_client_h, struct driver_ctx_t *drv_ctx)
{
	int ret = 0;
	struct pci_client_t *pci_client_ctx = (struct pci_client_t *)pci_client_h;

	if (WARN_ON(!pci_client_ctx))
		return -EINVAL;
	if (WARN_ON(!drv_ctx))
		return -EINVAL;
	pci_client_ctx->drv_ctx = drv_ctx;
	return ret;
}

/*Get the driver context of DRV_MODE_EPF or DRV_MODE_EPC */
struct driver_ctx_t *
pci_client_get_driver_ctx(void *pci_client_h)
{
	struct pci_client_t *pci_client_ctx = (struct pci_client_t *)pci_client_h;
	struct driver_ctx_t *drv_ctx = NULL;

	if (WARN_ON(!pci_client_ctx))
		return NULL;
	drv_ctx = pci_client_ctx->drv_ctx;
	if (WARN_ON(!drv_ctx))
		return NULL;
	return drv_ctx;
}

/* get drv mode  */
enum drv_mode_t
pci_client_get_drv_mode(void *pci_client_h)
{
	struct pci_client_t *pci_client_ctx = (struct pci_client_t *)pci_client_h;
	struct driver_ctx_t *drv_ctx = NULL;

	if (WARN_ON(!pci_client_ctx))
		return DRV_MODE_INVALID;
	drv_ctx = pci_client_ctx->drv_ctx;
	if (WARN_ON(!drv_ctx))
		return NVCPU_MAXIMUM;
	return drv_ctx->drv_mode;
}
/* save the peer cup orin/x86_64 */
int
pci_client_save_peer_cpu(void *pci_client_h, enum peer_cpu_t peer_cpu)
{
	int ret = 0;
	struct pci_client_t *pci_client_ctx = (struct pci_client_t *)pci_client_h;
	struct driver_ctx_t *drv_ctx = NULL;

	if (WARN_ON(!pci_client_ctx))
		return -EINVAL;
	drv_ctx = pci_client_ctx->drv_ctx;
	if (WARN_ON(!drv_ctx))
		return -EINVAL;
	drv_ctx->peer_cpu = peer_cpu;
	return ret;
}


/* get the peer cup orin/x86_64 */
enum peer_cpu_t
pci_client_get_peer_cpu(void *pci_client_h)
{
	struct pci_client_t *pci_client_ctx = (struct pci_client_t *)pci_client_h;
	struct driver_ctx_t *drv_ctx = NULL;

	if (WARN_ON(!pci_client_ctx))
		return NVCPU_MAXIMUM;
	drv_ctx = pci_client_ctx->drv_ctx;
	if (WARN_ON(!drv_ctx))
		return NVCPU_MAXIMUM;
	return drv_ctx->peer_cpu;
}

/* get  the iova allocated for x86 peer tegra-pcie-emda rx descriptor   */
dma_addr_t
pci_client_get_edma_rx_desc_iova(void *pci_client_h)
{
	struct pci_client_t *ctx = (struct pci_client_t *)pci_client_h;

	if (ctx)
		return ctx->edma_ch_desc_iova;
	else
		return 0;
}

int
pci_client_raise_irq(void *pci_client_h, enum pci_epc_irq_type type, u16 num)
{
	int ret = 0;
	struct pci_client_t *pci_client_ctx = (struct pci_client_t *)pci_client_h;
	struct driver_ctx_t *drv_ctx = NULL;
	struct epf_context_t *epf_ctx = NULL;

	if (WARN_ON(!pci_client_ctx))
		return -EINVAL;

	drv_ctx = pci_client_ctx->drv_ctx;
	if (WARN_ON(!drv_ctx))
		return -EINVAL;
	epf_ctx = (struct epf_context_t *)drv_ctx->epf_ctx;
	if (WARN_ON(!epf_ctx))
		return -EINVAL;
	if (WARN_ON(!epf_ctx->epf))
		return -EINVAL;
	if (WARN_ON(drv_ctx->drv_mode != DRV_MODE_EPF))
		return -EINVAL;
	ret = pci_epc_raise_irq(epf_ctx->epf->epc, epf_ctx->epf->func_no, type, num);
	return ret;
}

