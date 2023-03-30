/*
 * Engine side synchronization support
 *
 * Copyright (c) 2016-2022, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/nvhost.h>
#include <linux/errno.h>
#include <linux/iommu.h>
#include <linux/io.h>
#include <linux/nvmap_t19x.h>
#include <linux/nvhost_t194.h>
#include <linux/slab.h>

#include "bus_client_t194.h"
#include "nvhost_syncpt_unit_interface.h"

/**
 * nvhost_syncpt_unit_interface_get_aperture() - Get syncpoint MSS aperture
 *
 * @host_pdev:	Host1x pdev
 * @base:	Pointer for storing the MMIO base address
 * @size:	Pointer for storing the aperture size
 *
 * Return:	0 on success, a negative error code otherwise
 *
 * This function returns the start and size of the MSS syncpoint aperture.
 * The function can be used in cases where the device is not an nvhost
 * device (e.g. GPU).
 */
int nvhost_syncpt_unit_interface_get_aperture(
					struct platform_device *host_pdev,
					phys_addr_t *base,
					size_t *size)
{
	struct resource *res;

	if (host_pdev == NULL || base == NULL || size == NULL) {
		nvhost_err(NULL, "need nonNULL parameters to return output");
		return -ENOSYS;
	}

	res = platform_get_resource_byname(host_pdev, IORESOURCE_MEM,
					   "sem-syncpt-shim");
	if (!res) {
		dev_err(&host_pdev->dev, "failed to get syncpt aperture info");
		return -ENXIO;
	}

	*base = (phys_addr_t)res->start;
	*size = (size_t)res->end - (size_t)res->start + 1;

	return 0;
}
EXPORT_SYMBOL(nvhost_syncpt_unit_interface_get_aperture);

/**
 * nvhost_syncpt_unit_interface_get_byte_offset() - Get syncpoint offset
 *
 * @syncpt_id:	Syncpoint id
 *
 * Return:	Offset to the syncpoint address within the shim
 *
 * This function returns the offset to the syncpoint address from
 * the syncpoint mss aperture base.
 */
u32 nvhost_syncpt_unit_interface_get_byte_offset(u32 syncpt_id)
{
	struct nvhost_master *host = nvhost_get_prim_host();

	return syncpt_id * host->info.syncpt_page_size;
}
EXPORT_SYMBOL(nvhost_syncpt_unit_interface_get_byte_offset);

/**
 * nvhost_syncpt_address() - Get syncpoint IOVA for a device
 *
 * @engine_pdev:	Engine platform device pointer
 * @syncpt_id:		Syncpoint id
 *
 * Return:		IOVA address to the syncpoint
 *
 * This function returns the IOVA to a syncpoint. It is assumed that the
 * pdev uses nvhost and nvhost_syncpt_unit_interface_init_engine() has been
 * called.
 */
dma_addr_t nvhost_syncpt_address(struct platform_device *engine_pdev, u32 id)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(engine_pdev);
	struct nvhost_syncpt_unit_interface *syncpt_unit_interface =
			pdata->syncpt_unit_interface;

	return syncpt_unit_interface->start +
	       syncpt_unit_interface->syncpt_page_size * id;
}

/**
 * nvhost_syncpt_unit_interface() - Initialize engine-side synchronization
 *
 * @engine_pdev:	Engine platform device pointer
 *
 * Return:		0 on success, a negative error code otherwise
 *
 * This function prepares engine to perform synchronization without
 * utilizing Host1x channels to perform syncpoint waits. This includes
 * initialization of the syncpoint<->MSS interface and mapping the GoS
 * for the device if needed.
 */
int nvhost_syncpt_unit_interface_init(struct platform_device *engine_pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(engine_pdev);
	struct nvhost_master *host = nvhost_get_host(engine_pdev);
	struct nvhost_syncpt_unit_interface *syncpt_unit_interface;
	struct platform_device *host_pdev;
	dma_addr_t range_start;
	struct resource *res;
	unsigned int range_size;
	int err = 0;

	/* Get aperture and initialize range variables assuming physical
	 * addressing
	 */
	host_pdev = to_platform_device(engine_pdev->dev.parent);
	res = platform_get_resource_byname(host_pdev, IORESOURCE_MEM,
					"sem-syncpt-shim");
	range_start = (dma_addr_t)res->start;
	range_size = (unsigned int)res->end - (unsigned int)res->start + 1;

	/* Allocate space for storing the interface configuration */
	syncpt_unit_interface = devm_kzalloc(&engine_pdev->dev,
					     sizeof(*syncpt_unit_interface),
					     GFP_KERNEL);
	if (syncpt_unit_interface == NULL) {
		nvhost_err(&engine_pdev->dev,
			   "failed to allocate syncpt_unit_interface");
		return -ENOMEM;
	}

	/* If IOMMU is enabled, map it into the device memory */
	if (iommu_get_domain_for_dev(&engine_pdev->dev)) {
		/* Initialize the scatterlist to cover the whole range */
		sg_init_table(&syncpt_unit_interface->sg, 1);
		sg_set_page(&syncpt_unit_interface->sg,
			    phys_to_page(res->start), range_size, 0);

		err = dma_map_sg_attrs(&engine_pdev->dev,
				       &syncpt_unit_interface->sg, 1,
				       DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);

		/* dma_map_sg_attrs returns 0 on errors */
		if (err == 0) {
			err = -ENOMEM;
			return err;
		}

		range_start = sg_dma_address(&syncpt_unit_interface->sg);
		err = 0;
	}

	syncpt_unit_interface->start = range_start;
	syncpt_unit_interface->syncpt_page_size = host->info.syncpt_page_size;
	pdata->syncpt_unit_interface = syncpt_unit_interface;

	nvhost_dbg_info("%s: unit interface initialized to range %llu-%llu",
			dev_name(&engine_pdev->dev),
			(u64)range_start, (u64)range_start + (u64)range_size);

	return err;
}

void nvhost_syncpt_unit_interface_deinit(struct platform_device *pdev)
{
	struct nvhost_syncpt_unit_interface *syncpt_unit_interface;
	struct nvhost_device_data *pdata;

	if (iommu_get_domain_for_dev(&pdev->dev)) {
		pdata = platform_get_drvdata(pdev);
		syncpt_unit_interface = pdata->syncpt_unit_interface;

		dma_unmap_sg_attrs(&pdev->dev, &syncpt_unit_interface->sg, 1,
				   DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
	}
}
