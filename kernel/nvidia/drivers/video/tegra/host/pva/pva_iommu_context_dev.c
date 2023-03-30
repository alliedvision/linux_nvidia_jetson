/*
 * PVA Application Specific Virtual Memory
 *
 * Copyright (c) 2022, NVIDIA Corporation.  All rights reserved.
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

#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/version.h>
#include <linux/dma-buf.h>

#include "pva_iommu_context_dev.h"
#include "pva.h"

static u32 cntxt_dev_count;

static const struct of_device_id pva_iommu_context_dev_of_match[] = {
	{.compatible = "nvidia,pva-tegra186-iommu-context"},
	{},
};

struct pva_iommu_ctx {
	struct platform_device		*pdev;
	struct list_head		list;
	struct device_dma_parameters	dma_parms;
	bool				allocated;
};

static LIST_HEAD(pva_iommu_ctx_list);
static DEFINE_MUTEX(pva_iommu_ctx_list_mutex);

bool is_cntxt_initialized(void)
{
	return (cntxt_dev_count == 8);
}

int nvpva_iommu_context_dev_get_sids(int *hwids, int *count, int max_cnt)
{
	struct pva_iommu_ctx *ctx;
	int err = 0;

	*count = 0;
	mutex_lock(&pva_iommu_ctx_list_mutex);

	list_for_each_entry(ctx, &pva_iommu_ctx_list, list) {
		hwids[*count] = nvpva_get_device_hwid(ctx->pdev, 0);
		if (hwids[*count] < 0) {
			err = hwids[*count];
			break;
		}
		++(*count);
		if (*count >= max_cnt)
			break;
	}

	mutex_unlock(&pva_iommu_ctx_list_mutex);

	return err;
}

struct platform_device
*nvpva_iommu_context_dev_allocate(void)
{
	struct pva_iommu_ctx *ctx;
	struct pva_iommu_ctx *ctx_new = NULL;

	mutex_lock(&pva_iommu_ctx_list_mutex);

	list_for_each_entry(ctx, &pva_iommu_ctx_list, list)
		if (!ctx->allocated && !ctx_new)
			ctx_new = ctx;


	if (ctx_new) {
#ifdef CONFIG_NVMAP
		/*
		 * Ensure that all stashed mappings are removed from this context device
		 * before this context device gets reassigned to some other process
		 */
		dma_buf_release_stash(&ctx_new->pdev->dev);
#endif
		ctx_new->allocated = true;
		mutex_unlock(&pva_iommu_ctx_list_mutex);
		return ctx_new->pdev;
	}

	mutex_unlock(&pva_iommu_ctx_list_mutex);

	return NULL;
}

void nvpva_iommu_context_dev_release(struct platform_device *pdev)
{
	struct pva_iommu_ctx *ctx;

	if (pdev == NULL)
		return;

	ctx = platform_get_drvdata(pdev);
	mutex_lock(&pva_iommu_ctx_list_mutex);
	ctx->allocated = false;
	mutex_unlock(&pva_iommu_ctx_list_mutex);
}

static int pva_iommu_context_dev_probe(struct platform_device *pdev)
{
	struct pva_iommu_ctx *ctx;

	if (!iommu_get_domain_for_dev(&pdev->dev)) {
		dev_err(&pdev->dev,
			"iommu is not enabled for context device. aborting.");
		return -ENOSYS;
	}

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		dev_err(&pdev->dev,
			   "%s: could not allocate iommu ctx\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&ctx->list);
	ctx->pdev = pdev;

	mutex_lock(&pva_iommu_ctx_list_mutex);
	list_add_tail(&ctx->list, &pva_iommu_ctx_list);
	cntxt_dev_count++;
	mutex_unlock(&pva_iommu_ctx_list_mutex);

	platform_set_drvdata(pdev, ctx);

	pdev->dev.dma_parms = &ctx->dma_parms;
	dma_set_max_seg_size(&pdev->dev, UINT_MAX);

#ifdef CONFIG_NVMAP
	/* flag required to handle stashings in context devices */
	pdev->dev.context_dev = true;
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 0, 0)
	dev_info(&pdev->dev, "initialized (streamid=%d, iommu=%s)",
		 nvpva_get_device_hwid(pdev, 0),
		 dev_name(pdev->dev.iommu->iommu_dev->dev));
#else
	dev_info(&pdev->dev, "initialized (streamid=%d)",
		 nvpva_get_device_hwid(pdev, 0));
#endif
	return 0;
}

static int __exit pva_iommu_context_dev_remove(struct platform_device *pdev)
{
	struct pva_iommu_ctx *ctx = platform_get_drvdata(pdev);

	mutex_lock(&pva_iommu_ctx_list_mutex);
	list_del(&ctx->list);
	mutex_unlock(&pva_iommu_ctx_list_mutex);

	return 0;
}

struct platform_driver nvpva_iommu_context_dev_driver = {
	.probe = pva_iommu_context_dev_probe,
	.remove = __exit_p(pva_iommu_context_dev_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "pva_iommu_context_dev",
#ifdef CONFIG_OF
		.of_match_table = pva_iommu_context_dev_of_match,
#endif
	},
};

