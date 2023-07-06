// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, NVIDIA Corporation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/dma-fence.h>
#include <linux/dma-mapping.h>
#include <linux/host1x-next.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/nvhost.h>
#include <linux/nvhost_t194.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "falcon.h"

#define TEGRA194_SYNCPT_PAGE_SIZE 0x1000
#define TEGRA194_SYNCPT_SHIM_BASE 0x60000000
#define TEGRA194_SYNCPT_SHIM_SIZE 0x00400000
#define TEGRA234_SYNCPT_PAGE_SIZE 0x10000
#define TEGRA234_SYNCPT_SHIM_BASE 0x60000000
#define TEGRA234_SYNCPT_SHIM_SIZE 0x04000000

#define THI_STREAMID0	0x00000030
#define THI_STREAMID1	0x00000034

#define NVHOST_NUM_CDEV 1

struct nvhost_syncpt_interface {
	dma_addr_t base;
	size_t size;
	uint32_t page_size;
};

u32 host1x_readl(struct platform_device *pdev, u32 r)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	void __iomem *addr = pdata->aperture[0] + r;

	return readl(addr);
}
EXPORT_SYMBOL(host1x_readl);

void host1x_writel(struct platform_device *pdev, u32 r, u32 v)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	void __iomem *addr = pdata->aperture[0] + r;

	writel(v, addr);
}
EXPORT_SYMBOL(host1x_writel);

static const struct of_device_id host1x_match[] = {
	{ .compatible = "nvidia,tegra194-host1x", },
	{ .compatible = "nvidia,tegra234-host1x", },
	{},
};

struct platform_device *nvhost_get_default_device(void)
{
	struct platform_device *host1x_pdev;
	struct device_node *np;

	np = of_find_matching_node(NULL, host1x_match);
	if (!np)
		return NULL;

	host1x_pdev = of_find_device_by_node(np);
	if (!host1x_pdev)
		return NULL;

	return host1x_pdev;
}
EXPORT_SYMBOL(nvhost_get_default_device);

struct host1x *nvhost_get_host1x(struct platform_device *pdev)
{
	struct platform_device *host1x_pdev;
	struct host1x *host1x;

	host1x_pdev = nvhost_get_default_device();
	if (!host1x_pdev) {
		dev_dbg(&pdev->dev, "host1x device not available\n");
		return NULL;
	}

	host1x = platform_get_drvdata(host1x_pdev);
	if (!host1x) {
		dev_warn(&pdev->dev, "No platform data for host1x!\n");
		return NULL;
	}

	return host1x;
}
EXPORT_SYMBOL(nvhost_get_host1x);

static struct device *nvhost_client_device_create(struct platform_device *pdev,
						  struct cdev *cdev,
						  const char *cdev_name,
						  dev_t devno,
						  const struct file_operations *ops)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct device *dev;
	int err;

	pdata->nvhost_class = class_create(THIS_MODULE, pdev->dev.of_node->name);
	if (IS_ERR(pdata->nvhost_class)) {
		dev_err(&pdev->dev, "failed to create class\n");
		return ERR_CAST(pdata->nvhost_class);
	}

	cdev_init(cdev, ops);
	cdev->owner = THIS_MODULE;

	err = cdev_add(cdev, devno, 1);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to add cdev\n");
		class_destroy(pdata->nvhost_class);
		return ERR_PTR(err);
	}

	dev = device_create(pdata->nvhost_class, &pdev->dev, devno, NULL,
			    (pdev->id <= 0) ? "nvhost-%s%s" : "nvhost-%s%s.%d",
			    cdev_name, pdev->dev.of_node->name, pdev->id);

	if (IS_ERR(dev)) {
		dev_err(&pdev->dev, "failed to create %s device\n", cdev_name);
		class_destroy(pdata->nvhost_class);
		cdev_del(cdev);
	}

	return dev;
}

int nvhost_client_device_get_resources(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	int err;
	u32 i;

	pdata->host1x = nvhost_get_host1x(pdev);
	if (!pdata->host1x) {
		dev_warn(&pdev->dev, "No platform data for host1x!\n");
		return -ENODEV;
	}

	for (i = 0; i < pdev->num_resources; i++) {
		void __iomem *regs = NULL;
		struct resource *r;

		r = platform_get_resource(pdev, IORESOURCE_MEM, i);
		/* We've run out of mem resources */
		if (!r)
			break;

		regs = devm_ioremap_resource(&pdev->dev, r);
		if (IS_ERR(regs)) {
			err = PTR_ERR(regs);
			goto fail;
		}

		pdata->aperture[i] = regs;
	}

	return 0;

fail:
	dev_err(&pdev->dev, "failed to get register memory\n");

	return err;

}
EXPORT_SYMBOL(nvhost_client_device_get_resources);

int nvhost_client_device_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	dev_t devno;
	int err;

	err = alloc_chrdev_region(&devno, 0, NVHOST_NUM_CDEV, "nvhost");
	if (err < 0) {
		dev_err(&pdev->dev, "failed to reserve chrdev region\n");
		return err;
	}

	pdata->ctrl_node = nvhost_client_device_create(pdev, &pdata->ctrl_cdev,
						       "ctrl-", devno,
						       pdata->ctrl_ops);
	if (IS_ERR(pdata->ctrl_node))
		return PTR_ERR(pdata->ctrl_node);

	pdata->cdev_region = devno;

	return 0;
}
EXPORT_SYMBOL(nvhost_client_device_init);

int nvhost_client_device_release(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);

	if (!IS_ERR_OR_NULL(pdata->ctrl_node)) {
		device_destroy(pdata->nvhost_class, pdata->ctrl_cdev.dev);
		cdev_del(&pdata->ctrl_cdev);
		class_destroy(pdata->nvhost_class);
	}

	unregister_chrdev_region(pdata->cdev_region, NVHOST_NUM_CDEV);

	return 0;
}
EXPORT_SYMBOL(nvhost_client_device_release);

u32 nvhost_get_syncpt_host_managed(struct platform_device *pdev,
				   u32 param, const char *syncpt_name)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_alloc(pdata->host1x, 0, syncpt_name ? syncpt_name :
				 dev_name(&pdev->dev));
	if (!sp)
		return 0;

	return host1x_syncpt_id(sp);
}
EXPORT_SYMBOL(nvhost_get_syncpt_host_managed);

u32 nvhost_get_syncpt_client_managed(struct platform_device *pdev,
				     const char *syncpt_name)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_alloc(pdata->host1x, HOST1X_SYNCPT_CLIENT_MANAGED,
				 syncpt_name ? syncpt_name :
						     dev_name(&pdev->dev));
	if (!sp)
		return 0;

	return host1x_syncpt_id(sp);
}
EXPORT_SYMBOL_GPL(nvhost_get_syncpt_client_managed);

void nvhost_syncpt_put_ref_ext(struct platform_device *pdev, u32 id)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_get_by_id_noref(pdata->host1x, id);
	if (WARN_ON(!sp))
		return;

	host1x_syncpt_put(sp);
}
EXPORT_SYMBOL(nvhost_syncpt_put_ref_ext);

bool nvhost_syncpt_is_valid_pt_ext(struct platform_device *pdev, u32 id)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;

	if (!pdata || !pdata->host1x)
		return -ENODEV;

	sp = host1x_syncpt_get_by_id_noref(pdata->host1x, id);

	return sp ? true : false;
}
EXPORT_SYMBOL(nvhost_syncpt_is_valid_pt_ext);

int nvhost_syncpt_is_expired_ext(struct platform_device *pdev, u32 id,
				 u32 thresh)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_get_by_id_noref(pdata->host1x, id);
	if (WARN_ON(!sp))
		return true;

	if (host1x_syncpt_wait(sp, thresh, 0, NULL))
		return false;

	return true;
}
EXPORT_SYMBOL(nvhost_syncpt_is_expired_ext);

void nvhost_syncpt_set_minval(struct platform_device *pdev, u32 id, u32 val)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;
	u32 cur;

	sp = host1x_syncpt_get_by_id_noref(pdata->host1x, id);
	if (WARN_ON(!sp))
		return;

	cur = host1x_syncpt_read(sp);

	while (cur++ != val)
		host1x_syncpt_incr(sp);
}
EXPORT_SYMBOL(nvhost_syncpt_set_minval);

void nvhost_syncpt_set_min_update(struct platform_device *pdev, u32 id, u32 val)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;
	u32 cur;

	sp = host1x_syncpt_get_by_id_noref(pdata->host1x, id);
	if (WARN_ON(!sp))
		return;

	cur = host1x_syncpt_read(sp);

	while (cur++ != val)
		host1x_syncpt_incr(sp);

	host1x_syncpt_read(sp);
}
EXPORT_SYMBOL(nvhost_syncpt_set_min_update);

int nvhost_syncpt_read_ext_check(struct platform_device *pdev, u32 id, u32 *val)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_get_by_id_noref(pdata->host1x, id);
	if (!sp)
		return -EINVAL;

	*val = host1x_syncpt_read(sp);
	return 0;
}
EXPORT_SYMBOL(nvhost_syncpt_read_ext_check);

u32 nvhost_syncpt_read_maxval(struct platform_device *pdev, u32 id)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_get_by_id_noref(pdata->host1x, id);
	if (WARN_ON(!sp))
		return 0;

	return host1x_syncpt_read_max(sp);
}
EXPORT_SYMBOL(nvhost_syncpt_read_maxval);

u32 nvhost_syncpt_incr_max_ext(struct platform_device *pdev, u32 id, u32 incrs)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host1x_syncpt *sp;

	sp = host1x_syncpt_get_by_id_noref(pdata->host1x, id);
	if (WARN_ON(!sp))
		return 0;

	return host1x_syncpt_incr_max(sp, incrs);
}
EXPORT_SYMBOL(nvhost_syncpt_incr_max_ext);

static int nvhost_syncpt_get_aperture(struct device_node *np, u64 *base,
				      size_t *size)
{
	if (of_device_is_compatible(np, "nvidia,tegra194-host1x")) {
		*base = TEGRA194_SYNCPT_SHIM_BASE;
		*size = TEGRA194_SYNCPT_SHIM_SIZE;
		return 0;
	}

	if (of_device_is_compatible(np, "nvidia,tegra234-host1x")) {
		*base = TEGRA234_SYNCPT_SHIM_BASE;
		*size = TEGRA234_SYNCPT_SHIM_SIZE;
		return 0;
	}

	return -ENODEV;
}

static int nvhost_syncpt_get_page_size(struct device_node *np, uint32_t *size)
{
	if (of_device_is_compatible(np, "nvidia,tegra194-host1x")) {
		*size = TEGRA194_SYNCPT_PAGE_SIZE;
		return 0;
	}

	if (of_device_is_compatible(np, "nvidia,tegra234-host1x")) {
		*size = TEGRA234_SYNCPT_PAGE_SIZE;
		return 0;
	}

	return -ENODEV;
}

u32 nvhost_syncpt_unit_interface_get_byte_offset_ext(struct platform_device *pdev,
						     u32 syncpt_id)
{
	uint32_t size;
	int err;

	err = nvhost_syncpt_get_page_size(pdev->dev.of_node, &size);
	if (WARN_ON(err < 0))
		return 0;

	return syncpt_id * size;
}
EXPORT_SYMBOL(nvhost_syncpt_unit_interface_get_byte_offset_ext);

u32 nvhost_syncpt_unit_interface_get_byte_offset(u32 syncpt_id)
{
	struct platform_device *host1x_pdev;

	host1x_pdev = nvhost_get_default_device();
	if (WARN_ON(!host1x_pdev))
		return 0;

	return nvhost_syncpt_unit_interface_get_byte_offset_ext(host1x_pdev,
								syncpt_id);
}
EXPORT_SYMBOL(nvhost_syncpt_unit_interface_get_byte_offset);

int nvhost_syncpt_unit_interface_get_aperture(struct platform_device *pdev,
					      u64 *base, size_t *size)
{
	return nvhost_syncpt_get_aperture(pdev->dev.of_node, base, size);
}
EXPORT_SYMBOL(nvhost_syncpt_unit_interface_get_aperture);

int nvhost_syncpt_unit_interface_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_syncpt_interface *syncpt_if;
	u64 base;
	int err;

	syncpt_if = devm_kzalloc(&pdev->dev, sizeof(*syncpt_if), GFP_KERNEL);
	if (!syncpt_if)
		return -ENOMEM;

	err = nvhost_syncpt_get_aperture(pdev->dev.parent->of_node, &base,
					 &syncpt_if->size);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to get syncpt aperture\n");
		return err;
	}

	err = nvhost_syncpt_get_page_size(pdev->dev.parent->of_node,
					  &syncpt_if->page_size);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to get syncpt page size\n");
		return err;
	}

	/* If IOMMU is enabled, map it into the device memory */
	if (iommu_get_domain_for_dev(&pdev->dev)) {
		syncpt_if->base = dma_map_resource(&pdev->dev, base,
						   syncpt_if->size,
						   DMA_BIDIRECTIONAL,
						   DMA_ATTR_SKIP_CPU_SYNC);
		if (dma_mapping_error(&pdev->dev, syncpt_if->base))
			return -ENOMEM;
	} else {
		syncpt_if->base = base;
	}

	pdata->syncpt_unit_interface = syncpt_if;

	dev_info(&pdev->dev,
		 "syncpt_unit_base %llx syncpt_unit_size %zx size %x\n",
		 base, syncpt_if->size, syncpt_if->page_size);

	return 0;
}
EXPORT_SYMBOL(nvhost_syncpt_unit_interface_init);

void nvhost_syncpt_unit_interface_deinit(struct platform_device *pdev)
{
	struct nvhost_syncpt_interface *syncpt_if;
	struct nvhost_device_data *pdata;

	if (iommu_get_domain_for_dev(&pdev->dev)) {
		pdata = platform_get_drvdata(pdev);
		syncpt_if = pdata->syncpt_unit_interface;

		dma_unmap_resource(&pdev->dev, syncpt_if->base, syncpt_if->size,
				   DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
	}
}
EXPORT_SYMBOL(nvhost_syncpt_unit_interface_deinit);

dma_addr_t nvhost_syncpt_address(struct platform_device *pdev, u32 id)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_syncpt_interface *syncpt_if = pdata->syncpt_unit_interface;

	return syncpt_if->base + syncpt_if->page_size * id;
}
EXPORT_SYMBOL(nvhost_syncpt_address);

static irqreturn_t flcn_isr(int irq, void *dev_id)
{
	struct platform_device *pdev = (struct platform_device *)(dev_id);
	struct nvhost_device_data *pdata = nvhost_get_devdata(pdev);

	if (pdata->flcn_isr)
		pdata->flcn_isr(pdev);

	return IRQ_HANDLED;
}

int flcn_intr_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = nvhost_get_devdata(pdev);
	int ret = 0;

	pdata->irq = platform_get_irq(pdev, 0);
	if (pdata->irq < 0) {
		dev_err(&pdev->dev, "failed to get IRQ\n");
		return -ENXIO;
	}

	spin_lock_init(&pdata->mirq_lock);
	ret = devm_request_irq(&pdev->dev, pdata->irq, flcn_isr, 0,
			       dev_name(&pdev->dev), pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq. err %d\n", ret);
		return ret;
	}

	/* keep irq disabled */
	disable_irq(pdata->irq);

	return 0;
}
EXPORT_SYMBOL(flcn_intr_init);

int flcn_reload_fw(struct platform_device *pdev)
{
	/* TODO: Used by debugfs */
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(flcn_reload_fw);

static int nvhost_flcn_init(struct platform_device *pdev,
			    struct nvhost_device_data *pdata)
{
	struct falcon *falcon;

	falcon = devm_kzalloc(&pdev->dev, sizeof(*falcon), GFP_KERNEL);
	if (!falcon)
		return -ENOMEM;

	falcon->dev = &pdev->dev;
	falcon->regs = pdata->aperture[0];

	falcon_init(falcon);

	pdata->falcon_data = falcon;

	return 0;
}

int nvhost_flcn_prepare_poweroff(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);

	if (pdata->flcn_isr)
		disable_irq(pdata->irq);

	return 0;
}
EXPORT_SYMBOL(nvhost_flcn_prepare_poweroff);

static int nvhost_flcn_load_firmware(struct platform_device *pdev,
				     struct falcon *falcon,
				     char *firmware_name)
{
	dma_addr_t iova;
	size_t size;
	void *virt;
	int err;

	if (falcon->firmware.virt)
		return 0;

	err = falcon_read_firmware(falcon, firmware_name);
	if (err < 0)
		return err;

	size = falcon->firmware.size;
	virt = dma_alloc_coherent(&pdev->dev, size, &iova, GFP_KERNEL);
	if (!virt)
		return -ENOMEM;

	falcon->firmware.virt = virt;
	falcon->firmware.iova = iova;

	err = falcon_load_firmware(falcon);
	if (err < 0)
		goto cleanup;

	return 0;

cleanup:
	dma_free_coherent(&pdev->dev, size, virt, iova);

	return err;
}

int nvhost_flcn_finalize_poweron(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
#ifdef CONFIG_IOMMU_API
	struct iommu_fwspec *spec = dev_iommu_fwspec_get(&pdev->dev);
#endif
	struct falcon *falcon;
	int err;
	u32 value;

	if (!pdata->falcon_data) {
		err = nvhost_flcn_init(pdev, pdata);
		if (err < 0)
			return -ENOMEM;
	}

	falcon = pdata->falcon_data;

	err = nvhost_flcn_load_firmware(pdev, falcon, pdata->firmware_name);
	if (err < 0)
		return err;

#ifdef CONFIG_IOMMU_API
	if (spec) {
		host1x_writel(pdev, pdata->transcfg_addr, pdata->transcfg_val);

		if (spec->num_ids > 0) {
			value = spec->ids[0] & 0xffff;
			host1x_writel(pdev, THI_STREAMID0, value);
			host1x_writel(pdev, THI_STREAMID1, value);
		}
	}
#endif

	err = falcon_boot(falcon);
	if (err < 0)
		return err;

	err = falcon_wait_idle(falcon);
	if (err < 0) {
		dev_err(&pdev->dev, "falcon boot timed out\n");
		return err;
	}

	return 0;
}
EXPORT_SYMBOL(nvhost_flcn_finalize_poweron);

struct nvhost_host1x_cb {
	struct dma_fence_cb cb;
	struct work_struct work;
	void (*notifier)(void *data, int unused);
	void *notifier_data;
};

static void nvhost_host1x_cb_func(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct nvhost_host1x_cb *host1x_cb;

	host1x_cb = container_of(cb, struct nvhost_host1x_cb, cb);
	schedule_work(&host1x_cb->work);
	dma_fence_put(f);
}

static void nvhost_intr_do_work(struct work_struct *work)
{
	struct nvhost_host1x_cb *host1x_cb;

	host1x_cb = container_of(work, struct nvhost_host1x_cb, work);
	host1x_cb->notifier(host1x_cb->notifier_data, 0);
	kfree_rcu(host1x_cb);
}

int nvhost_intr_register_notifier(struct platform_device *pdev,
				  u32 id, u32 thresh,
				  void (*callback)(void *data, int unused),
				  void *private_data)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct dma_fence *fence;
	struct nvhost_host1x_cb *cb;
	struct host1x_syncpt *sp;
	int err;

	sp = host1x_syncpt_get_by_id_noref(pdata->host1x, id);
	if (!sp)
		return -EINVAL;

	fence = host1x_fence_create(sp, thresh, true);
	if (IS_ERR(fence)) {
		pr_err("error %d during construction of fence!",
			(int)PTR_ERR(fence));
		return PTR_ERR(fence);
	}

	cb = kzalloc(sizeof(*cb), GFP_KERNEL);
	if (!cb) {
		dma_fence_put(fence);
		return -ENOMEM;
	}

	INIT_WORK(&cb->work, nvhost_intr_do_work);
	cb->notifier = callback;
	cb->notifier_data = private_data;

	err = dma_fence_add_callback(fence, &cb->cb, nvhost_host1x_cb_func);
	if (err < 0) {
		dma_fence_put(fence);
		kfree(cb);
	}

	return err;
}
EXPORT_SYMBOL(nvhost_intr_register_notifier);

void nvhost_module_deinit(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct falcon *falcon = pdata->falcon_data;

	pm_runtime_disable(&pdev->dev);

	if (falcon) {
		dma_free_coherent(&pdev->dev, falcon->firmware.size,
			  falcon->firmware.virt, falcon->firmware.iova);
		falcon_exit(falcon);
	}

	debugfs_remove_recursive(pdata->debugfs);
}
EXPORT_SYMBOL(nvhost_module_deinit);

int nvhost_module_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	unsigned int i;
	int err;

	err = devm_clk_bulk_get_all(&pdev->dev, &pdata->clks);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to get clocks %d\n", err);
		return err;
	}
	pdata->num_clks = err;

	for (i = 0; i < pdata->num_clks; i++) {
		err = clk_set_rate(pdata->clks[i].clk, ULONG_MAX);
		if (err < 0) {
			dev_err(&pdev->dev, "failed to set clock rate!\n");
			return err;
		}
	}

	pdata->reset_control = devm_reset_control_get_exclusive_released(
					&pdev->dev, NULL);
	if (IS_ERR(pdata->reset_control)) {
		dev_err(&pdev->dev, "failed to get reset\n");
		return PTR_ERR(pdata->reset_control);
	}

	reset_control_acquire(pdata->reset_control);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to acquire reset: %d\n", err);
		return err;
	}

	err = clk_bulk_prepare_enable(pdata->num_clks, pdata->clks);
	if (err < 0) {
		reset_control_release(pdata->reset_control);
		dev_err(&pdev->dev, "failed to enabled clocks: %d\n", err);
		return err;
	}

	reset_control_reset(pdata->reset_control);
	clk_bulk_disable_unprepare(pdata->num_clks, pdata->clks);
	reset_control_release(pdata->reset_control);

	if (pdata->autosuspend_delay) {
		pm_runtime_set_autosuspend_delay(&pdev->dev,
			pdata->autosuspend_delay);
		pm_runtime_use_autosuspend(&pdev->dev);
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		return -EOPNOTSUPP;

	pdata->debugfs = debugfs_create_dir(pdev->dev.of_node->name,
					    NULL);

	return 0;
}
EXPORT_SYMBOL(nvhost_module_init);

static void nvhost_module_load_regs(struct platform_device *pdev, bool prod)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_gating_register *regs = pdata->engine_cg_regs;

	if (!regs)
		return;

	while (regs->addr) {
		if (prod)
			host1x_writel(pdev, regs->addr, regs->prod);
		else
			host1x_writel(pdev, regs->addr, regs->disable);
		regs++;
	}
}

void nvhost_module_reset(struct platform_device *pdev, bool reboot)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	int err;

	if (reboot)
		if (pdata->prepare_poweroff)
			pdata->prepare_poweroff(pdev);

	mutex_lock(&pdata->lock);
	err = reset_control_acquire(pdata->reset_control);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to acquire reset: %d\n", err);
	} else {
		reset_control_reset(pdata->reset_control);
		reset_control_release(pdata->reset_control);
	}
	mutex_unlock(&pdata->lock);

	if (reboot) {
		/* Load clockgating registers */
		nvhost_module_load_regs(pdev, pdata->engine_can_cg);

		/* ..and execute engine specific operations (i.e. boot) */
		if (pdata->finalize_poweron)
			pdata->finalize_poweron(pdev);
	}
}
EXPORT_SYMBOL(nvhost_module_reset);

int nvhost_module_busy(struct platform_device *dev)
{
	int err;

	err = pm_runtime_get_sync(&dev->dev);
	if (err < 0) {
		pm_runtime_put_noidle(&dev->dev);
		return err;
	}

	return 0;
}
EXPORT_SYMBOL(nvhost_module_busy);

void nvhost_module_idle_mult(struct platform_device *pdev, int refs)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);

	while (refs--) {
		pm_runtime_mark_last_busy(&pdev->dev);
		if (pdata->autosuspend_delay)
			pm_runtime_put_autosuspend(&pdev->dev);
		else
			pm_runtime_put(&pdev->dev);
	}
}
EXPORT_SYMBOL(nvhost_module_idle_mult);

inline void nvhost_module_idle(struct platform_device *pdev)
{
	nvhost_module_idle_mult(pdev, 1);
}
EXPORT_SYMBOL(nvhost_module_idle);

static int nvhost_module_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	int err;

	err = clk_bulk_prepare_enable(pdata->num_clks, pdata->clks);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to enabled clocks: %d\n", err);
		return err;
	}

	if (pdata->poweron_reset)
		nvhost_module_reset(pdev, false);

	/* Load clockgating registers */
	nvhost_module_load_regs(pdev, pdata->engine_can_cg);

	if (pdata->flcn_isr)
		enable_irq(pdata->irq);

	if (pdata->finalize_poweron)
		err = pdata->finalize_poweron(pdev);

	return err;
}

static int nvhost_module_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	int err;

	if (pdata->prepare_poweroff) {
		err = pdata->prepare_poweroff(pdev);
		if (err)
			return err;
	}

	clk_bulk_disable_unprepare(pdata->num_clks, pdata->clks);

	return 0;
}

const struct dev_pm_ops nvhost_module_pm_ops = {
	SET_RUNTIME_PM_OPS(nvhost_module_runtime_suspend,
			   nvhost_module_runtime_resume, NULL)
};
EXPORT_SYMBOL(nvhost_module_pm_ops);

static struct platform_driver nvhost_driver = {
	.driver = {
		.name = "host1x-nvhost",
	},
};

module_platform_driver(nvhost_driver);
MODULE_LICENSE("GPL v2");
