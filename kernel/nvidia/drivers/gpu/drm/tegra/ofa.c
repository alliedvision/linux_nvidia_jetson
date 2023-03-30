// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022, NVIDIA Corporation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/host1x-next.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include <soc/tegra/pmc.h>

#include "drm.h"
#include "falcon.h"
#include "util.h"
#include "vic.h"

#define OFA_TFBIF_TRANSCFG		0x1444
#define OFA_SAFETY_RAM_INIT_REQ		0x3320
#define OFA_SAFETY_RAM_INIT_DONE	0x3324

struct ofa_config {
	const char *firmware;
	unsigned int version;
	bool has_safety_ram;
};

struct ofa {
	struct falcon falcon;

	void __iomem *regs;
	struct tegra_drm_client client;
	struct host1x_channel *channel;
	struct device *dev;
	struct clk *clk;

	/* Platform configuration */
	const struct ofa_config *config;
};

static inline struct ofa *to_ofa(struct tegra_drm_client *client)
{
	return container_of(client, struct ofa, client);
}

static inline void ofa_writel(struct ofa *ofa, u32 value, unsigned int offset)
{
	writel(value, ofa->regs + offset);
}

static int ofa_boot(struct ofa *ofa)
{
	int err;
	u32 val;

	ofa_writel(ofa, 0x1, OFA_SAFETY_RAM_INIT_REQ);
	err = readl_poll_timeout(ofa->regs + OFA_SAFETY_RAM_INIT_DONE, val, (val == 1), 100000, 10);
	if (err < 0) {
		dev_err(ofa->dev, "timeout while initializing safety RAM\n");
		return err;
	}

	tegra_drm_program_iommu_regs(ofa->dev, ofa->regs, OFA_TFBIF_TRANSCFG);

	err = falcon_boot(&ofa->falcon);
	if (err < 0)
		return err;

	err = falcon_wait_idle(&ofa->falcon);
	if (err < 0) {
		dev_err(ofa->dev, "falcon boot timed out\n");
		return err;
	}

	return 0;
}

static int ofa_init(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->host);
	struct tegra_drm *tegra = dev->dev_private;
	struct ofa *ofa = to_ofa(drm);
	int err;

	ofa->channel = host1x_channel_request(client);
	if (!ofa->channel) {
		err = -ENOMEM;
		goto detach;
	}

	client->syncpts[0] = host1x_syncpt_request(client, 0);
	if (!client->syncpts[0]) {
		err = -ENOMEM;
		goto free_channel;
	}

	pm_runtime_enable(client->dev);
	pm_runtime_use_autosuspend(client->dev);
	pm_runtime_set_autosuspend_delay(client->dev, 500);

	err = tegra_drm_register_client(tegra, drm);
	if (err < 0)
		goto disable_rpm;

	/*
	 * Inherit the DMA parameters (such as maximum segment size) from the
	 * parent host1x device.
	 */
	client->dev->dma_parms = client->host->dma_parms;

	return 0;

disable_rpm:
	pm_runtime_dont_use_autosuspend(client->dev);
	pm_runtime_force_suspend(client->dev);
	host1x_syncpt_put(client->syncpts[0]);
free_channel:
	host1x_channel_put(ofa->channel);
detach:
	host1x_client_iommu_detach(client);

	return err;
}

static int ofa_exit(struct host1x_client *client)
{
	struct tegra_drm_client *drm = host1x_to_drm_client(client);
	struct drm_device *dev = dev_get_drvdata(client->host);
	struct tegra_drm *tegra = dev->dev_private;
	struct ofa *ofa = to_ofa(drm);
	int err;

	/* avoid a dangling pointer just in case this disappears */
	client->dev->dma_parms = NULL;

	err = tegra_drm_unregister_client(tegra, drm);
	if (err < 0)
		return err;

	pm_runtime_dont_use_autosuspend(client->dev);
	pm_runtime_force_suspend(client->dev);

	host1x_syncpt_put(client->syncpts[0]);
	host1x_channel_put(ofa->channel);

	ofa->channel = NULL;

	dma_free_coherent(ofa->dev, ofa->falcon.firmware.size,
			  ofa->falcon.firmware.virt,
			  ofa->falcon.firmware.iova);

	return 0;
}

static const struct host1x_client_ops ofa_client_ops = {
	.init = ofa_init,
	.exit = ofa_exit,
};

static int ofa_load_firmware(struct ofa *ofa)
{
	dma_addr_t iova;
	size_t size;
	void *virt;
	int err;

	if (ofa->falcon.firmware.virt)
		return 0;

	err = falcon_read_firmware(&ofa->falcon, ofa->config->firmware);
	if (err < 0)
		return err;

	size = ofa->falcon.firmware.size;

	virt = dma_alloc_coherent(ofa->dev, size, &iova, GFP_KERNEL);

	err = dma_mapping_error(ofa->dev, iova);
	if (err < 0)
		return err;

	ofa->falcon.firmware.virt = virt;
	ofa->falcon.firmware.iova = iova;

	err = falcon_load_firmware(&ofa->falcon);
	if (err < 0)
		goto cleanup;

	return 0;

cleanup:
	dma_free_coherent(ofa->dev, size, virt, iova);

	return err;
}


static __maybe_unused int ofa_runtime_resume(struct device *dev)
{
	struct ofa *ofa = dev_get_drvdata(dev);
	int err;

	err = clk_prepare_enable(ofa->clk);
	if (err < 0)
		return err;

	usleep_range(10, 20);

	err = ofa_load_firmware(ofa);
	if (err < 0)
		goto disable;

	err = ofa_boot(ofa);
	if (err < 0)
		goto disable;

	return 0;

disable:
	clk_disable_unprepare(ofa->clk);
	return err;
}

static __maybe_unused int ofa_runtime_suspend(struct device *dev)
{
	struct ofa *ofa = dev_get_drvdata(dev);

	host1x_channel_stop(ofa->channel);

	clk_disable_unprepare(ofa->clk);

	return 0;
}

static int ofa_open_channel(struct tegra_drm_client *client,
			    struct tegra_drm_context *context)
{
	struct ofa *ofa = to_ofa(client);
	int err;

	err = pm_runtime_get_sync(ofa->dev);
	if (err < 0) {
		pm_runtime_put(ofa->dev);
		return err;
	}

	context->channel = host1x_channel_get(ofa->channel);
	if (!context->channel) {
		pm_runtime_put(ofa->dev);
		return -ENOMEM;
	}

	return 0;
}

static void ofa_close_channel(struct tegra_drm_context *context)
{
	struct ofa *ofa = to_ofa(context->client);

	host1x_channel_put(context->channel);
	pm_runtime_put(ofa->dev);
}

static int ofa_can_use_memory_ctx(struct tegra_drm_client *client, bool *supported)
{
	*supported = true;

	return 0;
}

static const struct tegra_drm_client_ops ofa_ops = {
	.open_channel = ofa_open_channel,
	.close_channel = ofa_close_channel,
	.submit = tegra_drm_submit,
	.get_streamid_offset = tegra_drm_get_streamid_offset_thi,
	.can_use_memory_ctx = ofa_can_use_memory_ctx,
};

#define NVIDIA_TEGRA_234_OFA_FIRMWARE "nvidia/tegra234/ofa.bin"

static const struct ofa_config ofa_t234_config = {
	.firmware = NVIDIA_TEGRA_234_OFA_FIRMWARE,
	.version = 0x23,
	.has_safety_ram = true,
};

static const struct of_device_id tegra_ofa_of_match[] = {
	{ .compatible = "nvidia,tegra234-ofa", .data = &ofa_t234_config },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_ofa_of_match);

static int ofa_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct host1x_syncpt **syncpts;
	struct ofa *ofa;
	int err;

	/* inherit DMA mask from host1x parent */
	err = dma_coerce_mask_and_coherent(dev, *dev->parent->dma_mask);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to set DMA mask: %d\n", err);
		return err;
	}

	ofa = devm_kzalloc(dev, sizeof(*ofa), GFP_KERNEL);
	if (!ofa)
		return -ENOMEM;

	ofa->config = of_device_get_match_data(dev);

	syncpts = devm_kzalloc(dev, sizeof(*syncpts), GFP_KERNEL);
	if (!syncpts)
		return -ENOMEM;

	ofa->regs = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(ofa->regs))
		return PTR_ERR(ofa->regs);

	ofa->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ofa->clk)) {
		dev_err(&pdev->dev, "failed to get clock\n");
		return PTR_ERR(ofa->clk);
	}

	err = clk_set_rate(ofa->clk, ULONG_MAX);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to set clock rate\n");
		return err;
	}

	ofa->falcon.dev = dev;
	ofa->falcon.regs = ofa->regs;

	err = falcon_init(&ofa->falcon);
	if (err < 0)
		return err;

	platform_set_drvdata(pdev, ofa);

	INIT_LIST_HEAD(&ofa->client.base.list);
	ofa->client.base.ops = &ofa_client_ops;
	ofa->client.base.dev = dev;
	ofa->client.base.class = 0xf8;
	ofa->client.base.syncpts = syncpts;
	ofa->client.base.num_syncpts = 1;
	ofa->dev = dev;

	INIT_LIST_HEAD(&ofa->client.list);
	ofa->client.version = ofa->config->version;
	ofa->client.ops = &ofa_ops;

	err = host1x_client_register(&ofa->client.base);
	if (err < 0) {
		dev_err(dev, "failed to register host1x client: %d\n", err);
		goto exit_falcon;
	}

	return 0;

exit_falcon:
	falcon_exit(&ofa->falcon);

	return err;
}

static int ofa_remove(struct platform_device *pdev)
{
	struct ofa *ofa = platform_get_drvdata(pdev);
	int err;

	err = host1x_client_unregister(&ofa->client.base);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to unregister host1x client: %d\n",
			err);
		return err;
	}

	falcon_exit(&ofa->falcon);

	return 0;
}

static const struct dev_pm_ops ofa_pm_ops = {
	SET_RUNTIME_PM_OPS(ofa_runtime_suspend, ofa_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

struct platform_driver tegra_ofa_driver = {
	.driver = {
		.name = "tegra-ofa",
		.of_match_table = tegra_ofa_of_match,
		.pm = &ofa_pm_ops
	},
	.probe = ofa_probe,
	.remove = ofa_remove,
};

#if IS_ENABLED(CONFIG_ARCH_TEGRA_234_SOC)
MODULE_FIRMWARE(NVIDIA_TEGRA_234_OFA_FIRMWARE);
#endif
