// SPDX-License-Identifier: GPL-2.0-only
/*
 * OF helpers for IOMMU
 *
 * Copyright (c) 2012-2020, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/export.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/fsl/mc.h>

#define NO_IOMMU	1

/*
 * Parses &prop_name from the DT node &resv_node, then
 * creates and adds a resv regions with a &type and &prot
 * status.
 *
 * The DT property at &prop_name must be in start, size pairs
 * of u64 values.
*/
static void parse_resv_regions(struct device_node *resv_node,
					struct list_head *head,
					char *prop_name,
					int prot,
					enum iommu_resv_type type)
{
	int total_values, i, ret;

	total_values = of_property_count_elems_of_size(resv_node,
			prop_name,
			sizeof(u64));
	if (total_values % 2 != 0) {
		pr_warn("iommu-region props must be pairs of <start size>\n");
		return;
	}

	for (i = 0; i < total_values; i += 2) {
		u64 size, start;
		struct iommu_resv_region *resv;

		ret = of_property_read_u64_index(resv_node, prop_name,
				i, &start);
		if (ret)
			return;

		ret = of_property_read_u64_index(resv_node, prop_name,
				i + 1, &size);
		if (ret)
			return;

		if (start == 0 && size == 0)
			continue;

		/* If there is overflow, replace size with max possible size */
		if (start + size < start) {
			size = (~0x0) - start;
		}
		resv = iommu_alloc_resv_region(start, size,
				prot,
				type);
		if (!resv)
			continue;

		list_add_tail(&resv->list, head);
	}
}

void of_get_iommu_resv_regions(struct device *dev, struct list_head *head)
{
	parse_resv_regions(dev->of_node, head, "iommu-resv-regions", 0,
					IOMMU_RESV_RESERVED);
}

static int of_iommu_alloc_resv_msi_region(struct device_node *np,
		struct list_head *head)
{
	int prot = IOMMU_WRITE | IOMMU_NOEXEC | IOMMU_MMIO;
	struct iommu_resv_region *region;
	struct resource res;
	int i = 0;

	while (of_address_to_resource(np, i++, &res) == 0) {
		region = iommu_alloc_resv_region(res.start, resource_size(&res),
				prot, IOMMU_RESV_MSI);
		if (!region)
			return -ENOMEM;

		list_add_tail(&region->list, head);
	}

	return 0;
}

static int __get_pci_rid(struct pci_dev *pdev, u16 alias, void *data)
{
	u32 *rid = data;

	*rid = alias;
	return 0;
}

static int of_pci_msi_get_resv_regions(struct device *dev,
		struct list_head *head)
{
	struct device_node *msi_np;
	struct device *pdev;
	u32 rid;
	int err, resv = 0;

	pci_for_each_dma_alias(to_pci_dev(dev), __get_pci_rid, &rid);

	for_each_node_with_property(msi_np, "msi-controller") {
		for (pdev = dev; pdev; pdev = pdev->parent) {
			if (!pdev->of_node)
				continue;

			if (!of_map_id(pdev->of_node, rid, "msi-map",
					"msi-map-mask", &msi_np, NULL)) {
				err = of_iommu_alloc_resv_msi_region(msi_np,
									head);
				if (err)
					return err;
				resv++;
			}
		}
	}

	return resv;
}

static int of_platform_msi_get_resv_regions(struct device *dev,
		struct list_head *head)
{
	struct of_phandle_args args;
	int err, resv = 0;

	while (!of_parse_phandle_with_args(dev->of_node, "msi-parent",
				"#msi-cells", resv, &args)) {
		err = of_iommu_alloc_resv_msi_region(args.np, head);
		of_node_put(args.np);
		if (err)
			return err;

		resv++;
	}

	return resv;
}

void of_get_iommu_direct_regions(struct device *dev, struct list_head *head)
{
	struct device_node *dn = dev->of_node;
	struct device_node *dm_node;
	int phandle_index = 0;

	dm_node = of_parse_phandle(dn, "iommu-direct-regions", phandle_index++);
	while (dm_node != NULL) {
		parse_resv_regions(dm_node, head, "reg",
						IOMMU_READ | IOMMU_WRITE,
						IOMMU_RESV_DIRECT);
		dm_node = of_parse_phandle(dn, "iommu-direct-regions",
							phandle_index++);
	}

	return;
}

/**
 * of_get_dma_window - Parse *dma-window property and returns 0 if found.
 *
 * @dn: device node
 * @prefix: prefix for property name if any
 * @index: index to start to parse
 * @busno: Returns busno if supported. Otherwise pass NULL
 * @addr: Returns address that DMA starts
 * @size: Returns the range that DMA can handle
 *
 * This supports different formats flexibly. "prefix" can be
 * configured if any. "busno" and "index" are optionally
 * specified. Set 0(or NULL) if not used.
 */
int of_get_dma_window(struct device_node *dn, const char *prefix, int index,
		      unsigned long *busno, dma_addr_t *addr, size_t *size)
{
	const __be32 *dma_window, *end;
	int bytes, cur_index = 0;
	char propname[NAME_MAX], addrname[NAME_MAX], sizename[NAME_MAX];

	if (!dn || !addr || !size)
		return -EINVAL;

	if (!prefix)
		prefix = "";

	snprintf(propname, sizeof(propname), "%sdma-window", prefix);
	snprintf(addrname, sizeof(addrname), "%s#dma-address-cells", prefix);
	snprintf(sizename, sizeof(sizename), "%s#dma-size-cells", prefix);

	dma_window = of_get_property(dn, propname, &bytes);
	if (!dma_window)
		return -ENODEV;
	end = dma_window + bytes / sizeof(*dma_window);

	while (dma_window < end) {
		u32 cells;
		const void *prop;

		/* busno is one cell if supported */
		if (busno)
			*busno = be32_to_cpup(dma_window++);

		prop = of_get_property(dn, addrname, NULL);
		if (!prop)
			prop = of_get_property(dn, "#address-cells", NULL);

		cells = prop ? be32_to_cpup(prop) : of_n_addr_cells(dn);
		if (!cells)
			return -EINVAL;
		*addr = of_read_number(dma_window, cells);
		dma_window += cells;

		prop = of_get_property(dn, sizename, NULL);
		cells = prop ? be32_to_cpup(prop) : of_n_size_cells(dn);
		if (!cells)
			return -EINVAL;
		*size = of_read_number(dma_window, cells);
		dma_window += cells;

		if (cur_index++ == index)
			break;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(of_get_dma_window);

static int of_iommu_xlate(struct device *dev,
			  struct of_phandle_args *iommu_spec)
{
	const struct iommu_ops *ops;
	struct fwnode_handle *fwnode = &iommu_spec->np->fwnode;
	int ret;

	ops = iommu_ops_from_fwnode(fwnode);
	if ((ops && !ops->of_xlate) ||
	    !of_device_is_available(iommu_spec->np))
		return NO_IOMMU;

	ret = iommu_fwspec_init(dev, &iommu_spec->np->fwnode, ops);
	if (ret)
		return ret;
	/*
	 * The otherwise-empty fwspec handily serves to indicate the specific
	 * IOMMU device we're waiting for, which will be useful if we ever get
	 * a proper probe-ordering dependency mechanism in future.
	 */
	if (!ops)
		return driver_deferred_probe_check_state(dev);

	if (!try_module_get(ops->owner))
		return -ENODEV;

	ret = ops->of_xlate(dev, iommu_spec);
	module_put(ops->owner);
	return ret;
}

static int of_iommu_configure_dev_id(struct device_node *master_np,
				     struct device *dev,
				     const u32 *id)
{
	struct of_phandle_args iommu_spec = { .args_count = 1 };
	int err;

	err = of_map_id(master_np, *id, "iommu-map",
			 "iommu-map-mask", &iommu_spec.np,
			 iommu_spec.args);
	if (err)
		return err == -ENODEV ? NO_IOMMU : err;

	err = of_iommu_xlate(dev, &iommu_spec);
	of_node_put(iommu_spec.np);
	return err;
}

static int of_iommu_configure_dev(struct device_node *master_np,
				  struct device *dev)
{
	struct of_phandle_args iommu_spec;
	int err = NO_IOMMU, idx = 0;

	while (!of_parse_phandle_with_args(master_np, "iommus",
					   "#iommu-cells",
					   idx, &iommu_spec)) {
		err = of_iommu_xlate(dev, &iommu_spec);
		of_node_put(iommu_spec.np);
		idx++;
		if (err)
			break;
	}

	return err;
}

struct of_pci_iommu_alias_info {
	struct device *dev;
	struct device_node *np;
};

static int of_pci_iommu_init(struct pci_dev *pdev, u16 alias, void *data)
{
	struct of_pci_iommu_alias_info *info = data;
	u32 input_id = alias;

	return of_iommu_configure_dev_id(info->np, info->dev, &input_id);
}

static int of_iommu_configure_device(struct device_node *master_np,
				     struct device *dev, const u32 *id)
{
	return (id) ? of_iommu_configure_dev_id(master_np, dev, id) :
		      of_iommu_configure_dev(master_np, dev);
}

const struct iommu_ops *of_iommu_configure(struct device *dev,
					   struct device_node *master_np,
					   const u32 *id)
{
	const struct iommu_ops *ops = NULL;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	int err = NO_IOMMU;

	if (!master_np)
		return NULL;

	if (fwspec) {
		if (fwspec->ops)
			return fwspec->ops;

		/* In the deferred case, start again from scratch */
		iommu_fwspec_free(dev);
	}

	/*
	 * We don't currently walk up the tree looking for a parent IOMMU.
	 * See the `Notes:' section of
	 * Documentation/devicetree/bindings/iommu/iommu.txt
	 */
	if (dev_is_pci(dev)) {
		struct of_pci_iommu_alias_info info = {
			.dev = dev,
			.np = master_np,
		};

		pci_request_acs();
		err = pci_for_each_dma_alias(to_pci_dev(dev),
					     of_pci_iommu_init, &info);
	} else {
		err = of_iommu_configure_device(master_np, dev, id);

		fwspec = dev_iommu_fwspec_get(dev);
		if (!err && fwspec)
			of_property_read_u32(master_np, "pasid-num-bits",
					     &fwspec->num_pasid_bits);
	}

	/*
	 * Two success conditions can be represented by non-negative err here:
	 * >0 : there is no IOMMU, or one was unavailable for non-fatal reasons
	 *  0 : we found an IOMMU, and dev->fwspec is initialised appropriately
	 * <0 : any actual error
	 */
	if (!err) {
		/* The fwspec pointer changed, read it again */
		fwspec = dev_iommu_fwspec_get(dev);
		ops    = fwspec->ops;
	}
	/*
	 * If we have reason to believe the IOMMU driver missed the initial
	 * probe for dev, replay it to get things in order.
	 */
	if (!err && dev->bus && !device_iommu_mapped(dev))
		err = iommu_probe_device(dev);

	/* Ignore all other errors apart from EPROBE_DEFER */
	if (err == -EPROBE_DEFER) {
		ops = ERR_PTR(err);
	} else if (err < 0) {
		dev_dbg(dev, "Adding to IOMMU failed: %d\n", err);
		ops = NULL;
	}

	return ops;
}

/*
 * of_iommu_msi_get_resv_regions - Reserved region driver helper
 * @dev: Device from iommu_get_resv_regions()
 * @head: Reserved region list from iommu_get_resv_regions()
 *
 * Returns: Number of reserved regions on success (0 if no associated
 * msi parent), appropriate error value otherwise.
 */
int of_iommu_msi_get_resv_regions(struct device *dev, struct list_head *head)
{

	if (dev_is_pci(dev))
		return of_pci_msi_get_resv_regions(dev, head);
	else if (dev->of_node)
		return of_platform_msi_get_resv_regions(dev, head);

	return 0;
}
