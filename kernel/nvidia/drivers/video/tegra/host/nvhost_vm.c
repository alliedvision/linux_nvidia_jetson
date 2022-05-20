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

#include <trace/events/nvhost.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/iommu.h>

#include "chip_support.h"
#include "nvhost_vm.h"
#include "dev.h"

int nvhost_vm_init(struct platform_device *pdev)
{
	return 0;
}

int nvhost_vm_init_device(struct platform_device *pdev)
{
	trace_nvhost_vm_init_device(pdev->name);

	if (!vm_op().init_device)
		return 0;

	return vm_op().init_device(pdev);
}

int nvhost_vm_get_id(struct nvhost_vm *vm)
{
	int id;

	if (!vm_op().get_id)
		return -ENOSYS;

	id = vm_op().get_id(vm);
	trace_nvhost_vm_get_id(vm, id);

	return id;
}

static void nvhost_vm_deinit(struct kref *kref)
{
	struct nvhost_vm *vm = container_of(kref, struct nvhost_vm, kref);
	struct nvhost_master *host = nvhost_get_prim_host();

	trace_nvhost_vm_deinit(vm);

	/* remove this vm from the vms list */
	mutex_lock(&host->vm_mutex);
	list_del(&vm->vm_list);
	mutex_unlock(&host->vm_mutex);

	if (vm_op().deinit && vm->enable_hw)
		vm_op().deinit(vm);

	kfree(vm);
	vm = NULL;
}

void nvhost_vm_put(struct nvhost_vm *vm)
{
	trace_nvhost_vm_put(vm);
	kref_put(&vm->kref, nvhost_vm_deinit);
}

void nvhost_vm_get(struct nvhost_vm *vm)
{
	trace_nvhost_vm_get(vm);
	kref_get(&vm->kref);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
static struct device *dev_get_iommu(struct device *dev)
{
	return dev->iommu->iommu_dev->dev;
}

static bool iommu_match(struct device *a, struct device *b)
{
	return dev_get_iommu(a) == dev_get_iommu(b);
}
#else
static bool iommu_match(struct device *a, struct device *b)
{
	return true;
}
#endif

static inline bool nvhost_vm_can_be_reused(
	struct platform_device *pdev,
	struct nvhost_vm *vm,
	void *identifier)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	bool pdev_iommu = (iommu_get_domain_for_dev(&pdev->dev) != NULL);
	bool vm_iommu = (iommu_get_domain_for_dev(&vm->pdev->dev) != NULL);

	if (!pdata->isolate_contexts)
		return vm->pdev == pdev;

	if (pdev_iommu != vm_iommu)
		return false;

	if (pdev_iommu && !iommu_match(&pdev->dev, &vm->pdev->dev))
		return false;

	return vm->identifier == identifier &&
		vm->enable_hw == pdata->isolate_contexts;
}

struct nvhost_vm *nvhost_vm_allocate(struct platform_device *pdev,
				     void *identifier)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_master *host = nvhost_get_prim_host();
	struct nvhost_vm *vm;
	int err;

	trace_nvhost_vm_allocate(pdev->name, identifier);

	mutex_lock(&host->vm_alloc_mutex);
	mutex_lock(&host->vm_mutex);

	if (identifier) {
		list_for_each_entry(vm, &host->vm_list, vm_list) {
			if (nvhost_vm_can_be_reused(pdev, vm, identifier)) {
				/* skip entries that are going to be removed */
				if (!kref_get_unless_zero(&vm->kref))
					continue;
				mutex_unlock(&host->vm_mutex);
				mutex_unlock(&host->vm_alloc_mutex);

				trace_nvhost_vm_allocate_reuse(pdev->name,
					identifier, vm, vm->pdev->name);

				return vm;
			}
		}
	}

	/* get room to keep vm */
	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm) {
		nvhost_err(&pdev->dev, "failed to allocate vm");
		mutex_unlock(&host->vm_mutex);
		goto err_alloc_vm;
	}

	kref_init(&vm->kref);
	INIT_LIST_HEAD(&vm->vm_list);
	vm->pdev = pdev;
	vm->enable_hw = pdata->isolate_contexts;
	vm->identifier = identifier;

	/* add this vm into list of vms */
	list_add_tail(&vm->vm_list, &host->vm_list);

	/* release the vm mutex */
	mutex_unlock(&host->vm_mutex);

	if (vm_op().init && vm->enable_hw) {
		err = vm_op().init(vm, identifier, &pdev->dev);
		if (err) {
			nvhost_debug_dump(host);
			goto err_init;
		}
	}

	mutex_unlock(&host->vm_alloc_mutex);

	trace_nvhost_vm_allocate_done(pdev->name, identifier, vm,
				      vm->pdev->name);

	return vm;

err_init:
	mutex_lock(&host->vm_mutex);
	list_del(&vm->vm_list);
	mutex_unlock(&host->vm_mutex);
	kfree(vm);
err_alloc_vm:
	mutex_unlock(&host->vm_alloc_mutex);
	return NULL;
}
