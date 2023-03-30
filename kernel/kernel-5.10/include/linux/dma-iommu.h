/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2014-2015 ARM Ltd.
 */
#ifndef __DMA_IOMMU_H
#define __DMA_IOMMU_H

#include <linux/errno.h>
#include <linux/types.h>

#ifdef CONFIG_IOMMU_DMA
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/msi.h>

/* Domain management interface for IOMMU drivers */
int iommu_get_dma_cookie(struct iommu_domain *domain);
int iommu_get_msi_cookie(struct iommu_domain *domain, dma_addr_t base);
void iommu_put_dma_cookie(struct iommu_domain *domain);
dma_addr_t iommu_dma_alloc_iova(struct device *dev, size_t size,
				u64 dma_limit);
void iommu_dma_free_iova(struct device *dev, dma_addr_t iova, size_t size);

/* Setup call for arch DMA mapping code */
void iommu_setup_dma_ops(struct device *dev, u64 dma_base, u64 size);

/* The DMA API isn't _quite_ the whole story, though... */
/*
 * iommu_dma_prepare_msi() - Map the MSI page in the IOMMU device
 *
 * The MSI page will be stored in @desc.
 *
 * Return: 0 on success otherwise an error describing the failure.
 */
int iommu_dma_prepare_msi(struct msi_desc *desc, phys_addr_t msi_addr);

/* Update the MSI message if required. */
void iommu_dma_compose_msi_msg(struct msi_desc *desc,
			       struct msi_msg *msg);

void iommu_dma_map_msi_pages(struct device *dev, phys_addr_t msi_base,
		int irq, unsigned int nr_irqs);
void iommu_dma_unmap_msi_pages(struct device *dev, phys_addr_t msi_base,
		int irq, unsigned int nr_irqs);
void iommu_dma_get_resv_regions(struct device *dev, struct list_head *list);

#else /* CONFIG_IOMMU_DMA */

struct iommu_domain;
struct msi_desc;
struct msi_msg;
struct device;

static inline void iommu_setup_dma_ops(struct device *dev, u64 dma_base,
		u64 size)
{
}

static inline int iommu_get_dma_cookie(struct iommu_domain *domain)
{
	return -ENODEV;
}

static inline int iommu_get_msi_cookie(struct iommu_domain *domain, dma_addr_t base)
{
	return -ENODEV;
}

static inline void iommu_put_dma_cookie(struct iommu_domain *domain)
{
}

static dma_addr_t iommu_dma_alloc_iova(struct device *dev, size_t size,
				       u64 dma_limit)
{
	return 0;
}
void iommu_dma_free_iova(struct device *dev, dma_addr_t iova, size_t size)
{
}

static inline int iommu_dma_prepare_msi(struct msi_desc *desc,
					phys_addr_t msi_addr)
{
	return 0;
}

static inline void iommu_dma_compose_msi_msg(struct msi_desc *desc,
					     struct msi_msg *msg)
{
}

static void iommu_dma_map_msi_pages(struct device *dev, phys_addr_t msi_base,
		int irq, unsigned int nr_irqs)
{
}

static void iommu_dma_unmap_msi_pages(struct device *dev, phys_addr_t msi_base,
		int irq, unsigned int nr_irqs)
{
}

static inline void iommu_dma_get_resv_regions(struct device *dev, struct list_head *list)
{
}

#endif	/* CONFIG_IOMMU_DMA */
#endif	/* __DMA_IOMMU_H */
