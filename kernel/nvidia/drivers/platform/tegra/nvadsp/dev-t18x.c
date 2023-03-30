/*
 * Copyright (c) 2015-2021, NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <linux/platform_device.h>
#include <linux/tegra_nvadsp.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include <linux/delay.h>
#include <linux/tegra_nvadsp.h>

#ifdef CONFIG_TEGRA_VIRT_AUDIO_IVC
#include "tegra_virt_alt_ivc_common.h"
#include "tegra_virt_alt_ivc.h"
#endif

#include "dev.h"
#include "dev-t18x.h"

#ifdef CONFIG_PM
static int nvadsp_t18x_clocks_disable(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	/* APE and APB2APE clocks which are required by NVADSP are controlled
	 * from parent ACONNECT bus driver
	 */
	if (drv_data->adsp_clk) {
		clk_disable_unprepare(drv_data->adsp_clk);
		dev_dbg(dev, "adsp clocks disabled\n");
		drv_data->adsp_clk = NULL;
	}

	if (drv_data->aclk_clk) {
		clk_disable_unprepare(drv_data->aclk_clk);
		dev_dbg(dev, "aclk clock disabled\n");
		drv_data->aclk_clk = NULL;
	}

	if (drv_data->adsp_neon_clk) {
		clk_disable_unprepare(drv_data->adsp_neon_clk);
		dev_dbg(dev, "adsp_neon clocks disabled\n");
		drv_data->adsp_neon_clk = NULL;
	}

	return 0;
}

static int nvadsp_t18x_clocks_enable(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int ret = 0;
	/* APE and APB2APE clocks which are required by NVADSP are controlled
	 * from parent ACONNECT bus driver
	 */
	drv_data->adsp_clk = devm_clk_get(dev, "adsp");
	if (IS_ERR_OR_NULL(drv_data->adsp_clk)) {
		dev_err(dev, "unable to find adsp clock\n");
		ret = PTR_ERR(drv_data->adsp_clk);
		goto end;
	}
	ret = clk_prepare_enable(drv_data->adsp_clk);
	if (ret) {
		dev_err(dev, "unable to enable adsp clock\n");
		goto end;
	}

	drv_data->aclk_clk = devm_clk_get(dev, "aclk");
	if (IS_ERR_OR_NULL(drv_data->aclk_clk)) {
		dev_err(dev, "unable to find aclk clock\n");
		ret = PTR_ERR(drv_data->aclk_clk);
		goto end;
	}
	ret = clk_prepare_enable(drv_data->aclk_clk);
	if (ret) {
		dev_err(dev, "unable to enable aclk clock\n");
		goto end;
	}

	drv_data->adsp_neon_clk = devm_clk_get(dev, "adspneon");
	if (IS_ERR_OR_NULL(drv_data->adsp_neon_clk)) {
		dev_err(dev, "unable to find adsp neon clock\n");
		ret = PTR_ERR(drv_data->adsp_neon_clk);
		goto end;
	}
	ret = clk_prepare_enable(drv_data->adsp_neon_clk);
	if (ret) {
		dev_err(dev, "unable to enable adsp neon clock\n");
		goto end;
	}
	dev_dbg(dev, "adsp neon clock enabled\n");

	dev_dbg(dev, "all clocks enabled\n");
	return 0;
 end:
	nvadsp_t18x_clocks_disable(pdev);
	return ret;
}

static int __nvadsp_t18x_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int ret;

	dev_dbg(dev, "at %s:%d\n", __func__, __LINE__);

	ret = nvadsp_t18x_clocks_enable(pdev);
	if (ret) {
		dev_dbg(dev, "failed in nvadsp_t18x_clocks_enable\n");
		return ret;
	}

	return ret;
}

static int __nvadsp_t18x_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	dev_dbg(dev, "at %s:%d\n", __func__, __LINE__);

	return nvadsp_t18x_clocks_disable(pdev);
}

static int __nvadsp_t18x_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "at %s:%d\n", __func__, __LINE__);
	return 0;
}

int nvadsp_pm_t18x_init(struct platform_device *pdev)
{
	struct nvadsp_drv_data *d = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "at %s:%d\n", __func__, __LINE__);

	d->runtime_suspend = __nvadsp_t18x_runtime_suspend;
	d->runtime_resume = __nvadsp_t18x_runtime_resume;
	d->runtime_idle = __nvadsp_t18x_runtime_idle;

	return 0;
}
#endif /* CONFIG_PM */

static int __assert_t18x_adsp(struct nvadsp_drv_data *d)
{
	struct platform_device *pdev = d->pdev;
	struct device *dev = &pdev->dev;
	int ret = 0;

	/*
	 * The ADSP_ALL reset in BPMP-FW is overloaded to assert
	 * all 7 resets i.e. ADSP, ADSPINTF, ADSPDBG, ADSPNEON,
	 * ADSPPERIPH, ADSPSCU and ADSPWDT resets. So resetting
	 * only ADSP reset is sufficient to reset all ADSP sub-modules.
	 */
	ret = reset_control_assert(d->adspall_rst);
	if (ret) {
		dev_err(dev, "failed to assert adsp\n");
		goto end;
	}

	/* APE_TKE reset */
	if (d->ape_tke_rst) {
		ret = reset_control_assert(d->ape_tke_rst);
		if (ret)
			dev_err(dev, "failed to assert ape_tke\n");
	}

end:
	return ret;
}

static int __deassert_t18x_adsp(struct nvadsp_drv_data *d)
{
	struct platform_device *pdev = d->pdev;
	struct device *dev = &pdev->dev;
	int ret = 0;

	/* APE_TKE reset */
	if (d->ape_tke_rst) {
		ret = reset_control_deassert(d->ape_tke_rst);
		if (ret) {
			 dev_err(dev, "failed to deassert ape_tke\n");
			 goto end;
		}
	}

	/*
	 * The ADSP_ALL reset in BPMP-FW is overloaded to de-assert
	 * all 7 resets i.e. ADSP, ADSPINTF, ADSPDBG, ADSPNEON, ADSPPERIPH,
	 * ADSPSCU and ADSPWDT resets. The BPMP-FW also takes care
	 * of specific de-assert sequence and delays between them.
	 * So de-resetting only ADSP reset is sufficient to de-reset
	 * all ADSP sub-modules.
	 */
	ret = reset_control_deassert(d->adspall_rst);
	if (ret)
		dev_err(dev, "failed to deassert adsp\n");

end:
	return ret;
}

#ifdef CONFIG_TEGRA_VIRT_AUDIO_IVC
static int __virt_assert_t18x_adsp(struct nvadsp_drv_data *d)
{
	int err;
	struct nvaudio_ivc_msg	msg;
	struct nvaudio_ivc_ctxt *hivc_client = nvaudio_get_ivc_alloc_ctxt();

	if (!hivc_client) {
		pr_err("%s: Failed to allocate IVC context\n", __func__);
		return -ENODEV;
	}

	memset(&msg, 0, sizeof(struct nvaudio_ivc_msg));
	msg.cmd = NVAUDIO_ADSP_RESET;
	msg.params.adsp_reset_info.reset_req = ASSERT;
	msg.ack_required = true;

	err = nvaudio_ivc_send_receive(hivc_client,
			&msg,
			sizeof(struct nvaudio_ivc_msg));
	if (err < 0)
		pr_err("%s: error on ivc_send_receive\n", __func__);

	return 0;
}

static int __virt_deassert_t18x_adsp(struct nvadsp_drv_data *d)
{
	int err;
	struct nvaudio_ivc_msg	msg;
	struct nvaudio_ivc_ctxt *hivc_client = nvaudio_get_ivc_alloc_ctxt();

	if (!hivc_client) {
		pr_err("%s: Failed to allocate IVC context\n", __func__);
		return -ENODEV;
	}

	memset(&msg, 0, sizeof(struct nvaudio_ivc_msg));
	msg.cmd = NVAUDIO_ADSP_RESET;
	msg.params.adsp_reset_info.reset_req = DEASSERT;
	msg.ack_required = true;

	err = nvaudio_ivc_send_receive(hivc_client,
			&msg,
			sizeof(struct nvaudio_ivc_msg));
	if (err < 0)
		pr_err("%s: error on ivc_send_receive\n", __func__);

	return 0;
}
#endif

int nvadsp_reset_t18x_init(struct platform_device *pdev)
{
	struct nvadsp_drv_data *d = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int ret = 0;

#ifdef CONFIG_TEGRA_VIRT_AUDIO_IVC

	if (is_tegra_hypervisor_mode()) {
		d->assert_adsp = __virt_assert_t18x_adsp;
		d->deassert_adsp = __virt_deassert_t18x_adsp;
		d->adspall_rst = NULL;
		return 0;
	}
#endif

	d->assert_adsp = __assert_t18x_adsp;
	d->deassert_adsp = __deassert_t18x_adsp;
	d->adspall_rst = devm_reset_control_get(dev, "adspall");
	if (IS_ERR(d->adspall_rst)) {
		dev_err(dev, "can not get adspall reset\n");
		ret = PTR_ERR(d->adspall_rst);
		goto end;
	}

	d->ape_tke_rst = devm_reset_control_get(dev, "ape_tke");
	if (IS_ERR(d->ape_tke_rst))
		d->ape_tke_rst = NULL;

end:
	return ret;
}
