/*
 * Tegra Graphics Host Virtual Memory
 *
 * Copyright (c) 2014-2020, NVIDIA Corporation. All rights reserved.
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

#ifndef NVHOST_VM_H
#define NVHOST_VM_H

#include <linux/kref.h>
#include <linux/iommu.h>
#include <linux/version.h>

#ifdef CONFIG_NV_TEGRA_MC
#include <linux/platform/tegra/tegra-mc-sid.h>
#endif

struct platform_device;
struct nvhost_vm_pin;
struct dma_buf;
struct dma_buf_attachment;
struct sg_table;

struct nvhost_vm {
	struct platform_device *pdev;

	/* reference to this VM */
	struct kref kref;

	/* used by hardware layer */
	void *private_data;

	/* used for combining different users with same identifier */
	void *identifier;

	/* to track all vms in the system */
	struct list_head vm_list;

	/* marks if hardware isolation is enabled */
	bool enable_hw;
};

/**
 * nvhost_vm_init - initialize vm
 *	@pdev: Pointer to host1x platform device
 *
 * This function initializes vm operations for host1x.
 */
int nvhost_vm_init(struct platform_device *pdev);

/**
 * nvhost_vm_init_device - initialize device vm
 *	@pdev: Pointer to platform device
 *
 * This function initializes device VM operations during boot. The
 * call is routed to hardware specific function that is responsible
 * for performing hardware initialization.
 */
int nvhost_vm_init_device(struct platform_device *pdev);

/**
 * nvhost_vm_get_id - get hw identifier of this vm
 *	@vm: Pointer to nvhost_vm structure
 *
 * This function returns hardware identifier of the given vm.
 */
int nvhost_vm_get_id(struct nvhost_vm *vm);

/**
 * nvhost_vm_put - Drop reference to vm
 *	@vm: Pointer to nvhost_vm structure
 *
 * Drops reference to vm. When refcount goes to 0, vm resources are released.
 *
 * No return value
 */
void nvhost_vm_put(struct nvhost_vm *vm);

/**
 * nvhost_vm_get - Get reference to vm
 *	@vm: Pointer to nvhost_vm structure
 *
 * No return value
 */
void nvhost_vm_get(struct nvhost_vm *vm);

/**
 * nvhost_vm_allocate - Allocate vm to hold buffers
 *	@pdev: pointer to the host1x client device
 *	@identifier: used for combining different users
 *
 * This function allocates IOMMU domain to hold buffers and makes
 * initializations to lists, mutexes, bitmaps, etc. to keep track of mappings.
 *
 * Returns pointer to nvhost_vm on success, 0 otherwise.
 */
struct nvhost_vm *nvhost_vm_allocate(struct platform_device *pdev,
				     void *identifier);

static inline int nvhost_vm_get_hwid(struct platform_device *pdev,
				     unsigned int id)
{
	struct device *dev = &pdev->dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
#else
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
#endif

	if (!fwspec)
		return -EINVAL;

	if (id >= fwspec->num_ids)
		return -EINVAL;

	return fwspec->ids[id] & 0xffff;
}

static inline int nvhost_vm_get_bypass_hwid(void)
{
	return 0x7f;
}

#endif
