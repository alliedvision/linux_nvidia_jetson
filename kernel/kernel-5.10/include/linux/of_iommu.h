/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __OF_IOMMU_H
#define __OF_IOMMU_H

#include <linux/device.h>
#include <linux/iommu.h>
#include <linux/of.h>

#ifdef CONFIG_OF_IOMMU

extern int of_get_dma_window(struct device_node *dn, const char *prefix,
			     int index, unsigned long *busno, dma_addr_t *addr,
			     size_t *size);

extern const struct iommu_ops *of_iommu_configure(struct device *dev,
					struct device_node *master_np,
					const u32 *id);

extern int of_iommu_msi_get_resv_regions(struct device *dev,
		struct list_head *head);

#else

static inline int of_get_dma_window(struct device_node *dn, const char *prefix,
			    int index, unsigned long *busno, dma_addr_t *addr,
			    size_t *size)
{
	return -EINVAL;
}

static inline const struct iommu_ops *of_iommu_configure(struct device *dev,
					 struct device_node *master_np,
					 const u32 *id)
{
	return NULL;
}

int of_iommu_msi_get_resv_regions(struct device *dev, struct list_head *head)
{
	return NULL;
}

#endif	/* CONFIG_OF_IOMMU */

void of_get_iommu_resv_regions(struct device *dev, struct list_head *head);
void of_get_iommu_direct_regions(struct device *dev, struct list_head *head);

#endif /* __OF_IOMMU_H */
