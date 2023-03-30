/*
 * Copyright (C) 2017-2023, NVIDIA Corporation. All rights reserved.
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
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/reset.h>
#if IS_ENABLED(CONFIG_INTERCONNECT)
#include <linux/interconnect.h>
#include <dt-bindings/interconnect/tegra_icc_id.h>
#endif
#include <linux/platform/tegra/emc_bwmgr.h>
#include <linux/platform/tegra/actmon_common.h>

#include <linux/platform/tegra/mc_utils.h>

/************ START OF REG DEFINITION **************/
/* Actmon common registers */
#define ACTMON_GLB_CTRL				0x00
#define ACTMON_GLB_INT_EN			0x04
#define ACTMON_GLB_INT_STATUS			0x08

/* Actmon device registers */
/* ACTMON_*_CTRL_0 offset */
#define ACTMON_DEV_CTRL				0x00
/* ACTMON_*_CTRL_0 bit definitions */
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_SHIFT	26
#define ACTMON_DEV_CTRL_UP_WMARK_NUM_MASK	(0x7 << 26)
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_SHIFT	21
#define ACTMON_DEV_CTRL_DOWN_WMARK_NUM_MASK	(0x7 << 21)
#define ACTMON_DEV_CTRL_PERIODIC_ENB		(0x1 << 13)
#define ACTMON_DEV_CTRL_K_VAL_SHIFT		10
#define ACTMON_DEV_CTRL_K_VAL_MASK		(0x7 << 10)

/* ACTMON_*_INTR_ENABLE_0 */
#define ACTMON_DEV_INTR_ENB			0x04
/* ACTMON_*_INTR_ENABLE_0 bit definitions */
#define ACTMON_DEV_INTR_UP_WMARK_ENB		(0x1 << 31)
#define ACTMON_DEV_INTR_DOWN_WMARK_ENB		(0x1 << 30)
#define ACTMON_DEV_INTR_AVG_UP_WMARK_ENB	(0x1 << 29)
#define ACTMON_DEV_INTR_AVG_DOWN_WMARK_ENB	(0x1 << 28)

/* ACTMON_*_INTR_STAUS_0 */
#define ACTMON_DEV_INTR_STATUS			0x08
/* ACTMON_*_UPPER_WMARK_0 */
#define ACTMON_DEV_UP_WMARK			0x0c
/* ACTMON_*_LOWER_WMARK_0 */
#define ACTMON_DEV_DOWN_WMARK			0x10
/* ACTMON_*_AVG_UPPER_WMARK_0 */
#define ACTMON_DEV_AVG_UP_WMARK			0x14
/* ACTMON_*_AVG_LOWER_WMARK_0 */
#define ACTMON_DEV_AVG_DOWN_WMARK		0x18
/* ACTMON_*_AVG_INIT_AVG_0 */
#define ACTMON_DEV_INIT_AVG			0x1c
/* ACTMON_*_COUNT_0 */
#define ACTMON_DEV_COUNT			0x20
/* ACTMON_*_AVG_COUNT_0 */
#define ACTMON_DEV_AVG_COUNT			0x24
/* ACTMON_*_AVG_COUNT_WEIGHT_0 */
#define ACTMON_DEV_COUNT_WEGHT			0x28
/* ACTMON_*_CUMULATIVE_COUNT_0 */
#define ACTMON_DEV_CUMULATIVE_COUNT		0x2c
/************ END OF REG DEFINITION **************/

/******** start of actmon register operations **********/
static void set_prd(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_GLB_CTRL);
}

static u32 get_glb_ctrl(void __iomem *base)
{
	return __raw_readl(base + ACTMON_GLB_CTRL);
}

static void set_glb_intr(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_GLB_INT_EN);
}

static u32 get_glb_intr(void __iomem *base)
{
	return __raw_readl(base + ACTMON_GLB_INT_EN);
}

static u32 get_glb_intr_st(void __iomem *base)
{
	return __raw_readl(base + ACTMON_GLB_INT_STATUS);
}
/******** end of actmon register operations **********/

/*********start of actmon device register operations***********/
static void set_init_avg(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_INIT_AVG);
}

static void set_avg_up_wm(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_AVG_UP_WMARK);
}

static u32 get_avg_up_wm(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_AVG_UP_WMARK);
}

static void set_avg_dn_wm(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_AVG_DOWN_WMARK);
}

static u32 get_avg_dn_wm(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_AVG_DOWN_WMARK);
}

static void set_dev_up_wm(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_UP_WMARK);
}

static u32 get_dev_up_wm(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_UP_WMARK);
}

static void set_dev_dn_wm(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_DOWN_WMARK);
}

static u32 get_dev_dn_wm(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_DOWN_WMARK);
}

static void set_cnt_wt(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_COUNT_WEGHT);
}

static void set_intr_st(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_INTR_STATUS);
}

static u32 get_intr_st(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_INTR_STATUS);
}

static void set_dev_ctrl(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_CTRL);
}

static u32 get_dev_ctrl(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_CTRL);
}

static void init_dev_cntrl(struct actmon_dev *dev, void __iomem *base)
{
	u32 val = 0;

	val |= ACTMON_DEV_CTRL_PERIODIC_ENB;
	val |= (((dev->avg_window_log2 - 1) << ACTMON_DEV_CTRL_K_VAL_SHIFT)
		& ACTMON_DEV_CTRL_K_VAL_MASK);
	val |= (((dev->down_wmark_window - 1) <<
		ACTMON_DEV_CTRL_DOWN_WMARK_NUM_SHIFT) &
		ACTMON_DEV_CTRL_DOWN_WMARK_NUM_MASK);
	val |=  (((dev->up_wmark_window - 1) <<
		ACTMON_DEV_CTRL_UP_WMARK_NUM_SHIFT) &
		ACTMON_DEV_CTRL_UP_WMARK_NUM_MASK);

	__raw_writel(val, base + ACTMON_DEV_CTRL);
}

static void enb_dev_intr_all(void __iomem *base)
{
	u32 val = 0;

	val |= (ACTMON_DEV_INTR_UP_WMARK_ENB |
			ACTMON_DEV_INTR_DOWN_WMARK_ENB |
			ACTMON_DEV_INTR_AVG_UP_WMARK_ENB |
			ACTMON_DEV_INTR_AVG_DOWN_WMARK_ENB);

	__raw_writel(val, base + ACTMON_DEV_INTR_ENB);
}

static void enb_dev_intr(u32 val, void __iomem *base)
{
	__raw_writel(val, base + ACTMON_DEV_INTR_ENB);
}

static u32 get_dev_intr(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_INTR_ENB);
}

static u32 get_avg_cnt(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_AVG_COUNT);
}

static u32 get_raw_cnt(void __iomem *base)
{
	return __raw_readl(base + ACTMON_DEV_COUNT);
}

static void enb_dev_wm(u32 *val)
{
	*val |= (ACTMON_DEV_INTR_UP_WMARK_ENB |
			ACTMON_DEV_INTR_DOWN_WMARK_ENB);
}

static void disb_dev_up_wm(u32 *val)
{
	*val &= ~ACTMON_DEV_INTR_UP_WMARK_ENB;
}

static void disb_dev_dn_wm(u32 *val)
{
	*val &= ~ACTMON_DEV_INTR_DOWN_WMARK_ENB;
}

/*********end of actmon device register operations***********/

static void actmon_dev_reg_ops_init(struct actmon_dev *adev)
{
	adev->ops.set_init_avg = set_init_avg;
	adev->ops.set_avg_up_wm = set_avg_up_wm;
	adev->ops.get_avg_up_wm = get_avg_up_wm;
	adev->ops.set_avg_dn_wm = set_avg_dn_wm;
	adev->ops.get_avg_dn_wm = get_avg_dn_wm;
	adev->ops.set_dev_up_wm = set_dev_up_wm;
	adev->ops.get_dev_up_wm = get_dev_up_wm;
	adev->ops.set_dev_dn_wm = set_dev_dn_wm;
	adev->ops.get_dev_dn_wm = get_dev_dn_wm;
	adev->ops.set_cnt_wt = set_cnt_wt;
	adev->ops.set_intr_st = set_intr_st;
	adev->ops.get_intr_st = get_intr_st;
	adev->ops.set_dev_ctrl = set_dev_ctrl;
	adev->ops.get_dev_ctrl = get_dev_ctrl;
	adev->ops.init_dev_cntrl = init_dev_cntrl;
	adev->ops.enb_dev_intr_all = enb_dev_intr_all;
	adev->ops.enb_dev_intr = enb_dev_intr;
	adev->ops.get_dev_intr_enb = get_dev_intr;
	adev->ops.get_avg_cnt = get_avg_cnt;
	adev->ops.get_raw_cnt = get_raw_cnt;
	adev->ops.enb_dev_wm = enb_dev_wm;
	adev->ops.disb_dev_up_wm = disb_dev_up_wm;
	adev->ops.disb_dev_dn_wm = disb_dev_dn_wm;
}

static unsigned long actmon_dev_get_max_rate(struct actmon_dev *adev)
{
	unsigned long rate = 0;

	if (!adev->bwmgr_disable)
		return tegra_bwmgr_get_max_emc_rate();

	if (adev->dram_clk_handle) {
		rate = clk_round_rate(adev->dram_clk_handle, ULONG_MAX);
		return rate;
	}

	return 0;
}

static unsigned long actmon_dev_get_rate(struct actmon_dev *adev)
{
	unsigned long rate = 0;

	if (!adev->bwmgr_disable)
		return tegra_bwmgr_get_emc_rate();

	if (adev->dram_clk_handle) {
		rate = clk_get_rate(adev->dram_clk_handle);
		return rate;
	}

	return 0;
}

static unsigned long actmon_dev_post_change_rate(
	struct actmon_dev *adev, void *cclk)
{
	struct clk_notifier_data *clk_data = cclk;

	return clk_data->new_rate;
}

static void icc_set_rate(struct actmon_dev *adev, unsigned long freq)
{
#if IS_ENABLED(CONFIG_INTERCONNECT)
	struct icc_path *icc_path_handle = NULL;
	u32 floor_bw_kbps = 0;

	icc_path_handle = (struct icc_path *)adev->clnt;
	floor_bw_kbps = emc_freq_to_bw(freq);

	icc_set_bw(icc_path_handle, 0, floor_bw_kbps);
#endif
}

static void actmon_dev_set_rate(struct actmon_dev *adev,
		unsigned long freq)
{
	struct tegra_bwmgr_client *bwclnt = NULL;

	if (!adev->bwmgr_disable) {
		bwclnt = (struct tegra_bwmgr_client *)adev->clnt;

		tegra_bwmgr_set_emc(bwclnt, freq * 1000,
			TEGRA_BWMGR_SET_EMC_FLOOR);
	} else {
		icc_set_rate(adev, freq);
	}
}

static int cactmon_bwmgr_register(
	struct actmon_dev *adev, struct platform_device *pdev)
{
	struct tegra_bwmgr_client *bwclnt = (struct tegra_bwmgr_client *)
		adev->clnt;
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	bwclnt = tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_MON);
	if (IS_ERR_OR_NULL(bwclnt)) {
		ret = -ENODEV;
		dev_err(mon_dev, "emc bw manager registration failed for %s\n",
			adev->dn->name);
		return ret;
	}

	adev->clnt = bwclnt;

	return ret;
}


static void cactmon_bwmgr_unregister(
	struct actmon_dev *adev, struct platform_device *pdev)
{
	struct tegra_bwmgr_client *bwclnt = (struct tegra_bwmgr_client *)
			adev->clnt;
	struct device *mon_dev = &pdev->dev;

	if (bwclnt) {
		dev_dbg(mon_dev, "unregistering BW manager for %s\n",
			adev->dn->name);
		tegra_bwmgr_unregister(bwclnt);
		adev->clnt = NULL;
	}
}

static int cactmon_icc_register(
	struct actmon_dev *adev, struct platform_device *pdev)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_INTERCONNECT)
	struct icc_path *icc_path_handle = NULL;
	struct device *mon_dev = &pdev->dev;

	icc_path_handle = icc_get(mon_dev, TEGRA_ICC_CACTMON, TEGRA_ICC_PRIMARY);
	if (IS_ERR_OR_NULL(icc_path_handle)) {
		ret = -ENODEV;
		dev_err(mon_dev, "icc registration failed for %s\n",
			adev->dn->name);
		return ret;
	}

	adev->clnt = icc_path_handle;
#endif

	return ret;
}

static void cactmon_icc_unregister(
	struct actmon_dev *adev, struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_INTERCONNECT)
	struct icc_path *icc_path_handle = (struct icc_path *)
		adev->clnt;
	struct device *mon_dev = &pdev->dev;

	if (icc_path_handle) {
		dev_dbg(mon_dev, "unregistering icc for %s\n",
			adev->dn->name);
		icc_put(icc_path_handle);
		adev->clnt = NULL;
	}
#endif
}

static int cactmon_register_bw(
	struct actmon_dev *adev, struct platform_device *pdev)
{
	int ret = 0;

	if (adev->bwmgr_disable)
		ret = cactmon_icc_register(adev, pdev);
	else
		ret = cactmon_bwmgr_register(adev, pdev);

	return ret;
}

static void cactmon_unregister_bw(
	struct actmon_dev *adev, struct platform_device *pdev)
{
	if (adev->bwmgr_disable)
		cactmon_icc_unregister(adev, pdev);
	else
		cactmon_bwmgr_unregister(adev, pdev);
}

static int actmon_dev_platform_init(struct actmon_dev *adev,
		struct platform_device *pdev)
{
	struct tegra_bwmgr_client *bwclnt;
	int ret = 0;

	ret = cactmon_register_bw(adev, pdev);
	if (ret)
		goto end;

	adev->dev_name = adev->dn->name;
	adev->max_freq = actmon_dev_get_max_rate(adev);

	if (!adev->bwmgr_disable) {
		bwclnt = (struct tegra_bwmgr_client *) adev->clnt;
		tegra_bwmgr_set_emc(bwclnt, adev->max_freq,
			TEGRA_BWMGR_SET_EMC_FLOOR);
	} else {
		icc_set_rate(adev, adev->max_freq);
	}

	adev->max_freq /= 1000;
	actmon_dev_reg_ops_init(adev);
	adev->actmon_dev_set_rate = actmon_dev_set_rate;
	adev->actmon_dev_get_rate = actmon_dev_get_rate;
	if (adev->rate_change_nb.notifier_call) {
		if (!adev->bwmgr_disable)
			ret = tegra_bwmgr_notifier_register(&adev->rate_change_nb);
		else
			ret = clk_notifier_register(adev->dram_clk_handle, &adev->rate_change_nb);

		if (ret) {
			pr_err("Failed to register bw manager rate change notifier for %s\n",
				adev->dev_name);
			return ret;
		}
	}
	adev->actmon_dev_post_change_rate = actmon_dev_post_change_rate;
end:
	return ret;
}

static void cactmon_free_resource(
	struct actmon_dev *adev, struct platform_device *pdev)
{
	int ret = 0;

	if (adev->rate_change_nb.notifier_call) {
		if (!adev->bwmgr_disable)
			ret = tegra_bwmgr_notifier_unregister(&adev->rate_change_nb);
		else
			ret = clk_notifier_unregister(adev->dram_clk_handle, &adev->rate_change_nb);

		if (ret) {
			pr_err("Failed to register bw manager rate change notifier for %s\n",
			adev->dev_name);
		}
	}
	cactmon_unregister_bw(adev, pdev);
}

static int cactmon_reset_dinit(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = -EINVAL;

	if (actmon->actmon_rst) {
		ret = reset_control_assert(actmon->actmon_rst);
		if (ret)
			dev_err(mon_dev, "failed to assert actmon\n");
	}

	return ret;
}

static int cactmon_reset_init(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	actmon->actmon_rst = devm_reset_control_get(mon_dev, "actmon_rst");
	if (IS_ERR(actmon->actmon_rst)) {
		ret = PTR_ERR(actmon->actmon_rst);
		dev_err(mon_dev, "can not get actmon reset%d\n", ret);
	}

	/* Make actmon out of reset */
	ret = reset_control_deassert(actmon->actmon_rst);
	if (ret)
		dev_err(mon_dev, "failed to deassert actmon\n");

	return ret;
}


static int cactmon_clk_disable(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	if (actmon->actmon_clk) {
		clk_disable_unprepare(actmon->actmon_clk);
		devm_clk_put(mon_dev, actmon->actmon_clk);
		actmon->actmon_clk = NULL;
		dev_dbg(mon_dev, "actmon clocks disabled\n");
	}

	return ret;
}

static int cactmon_clk_enable(struct platform_device *pdev)
{
	struct actmon_drv_data *actmon = platform_get_drvdata(pdev);
	struct device *mon_dev = &pdev->dev;
	int ret = 0;

	actmon->actmon_clk = devm_clk_get(mon_dev, "actmon");
	if (IS_ERR_OR_NULL(actmon->actmon_clk)) {
		dev_err(mon_dev, "unable to find actmon clock\n");
		ret = PTR_ERR(actmon->actmon_clk);
		return ret;
	}

	ret = clk_prepare_enable(actmon->actmon_clk);
	if (ret) {
		dev_err(mon_dev, "unable to enable actmon clock\n");
		goto end;
	}

	actmon->freq = clk_get_rate(actmon->actmon_clk) / 1000;

	return 0;
end:
	devm_clk_put(mon_dev, actmon->actmon_clk);
	return ret;
}

static struct actmon_drv_data actmon_data =
{
	.clock_init = cactmon_clk_enable,
	.clock_deinit = cactmon_clk_disable,
	.reset_init = cactmon_reset_init,
	.reset_deinit = cactmon_reset_dinit,
	.dev_free_resource = cactmon_free_resource,
	.actmon_dev_platform_init = actmon_dev_platform_init,
	.ops.set_sample_prd = set_prd,
	.ops.set_glb_intr = set_glb_intr,
	.ops.get_glb_intr = get_glb_intr,
	.ops.get_glb_ctrl = get_glb_ctrl,
	.ops.get_glb_intr_st = get_glb_intr_st,
};

static const struct of_device_id tegra_actmon_of_match[] = {
	{ .compatible = "nvidia,tegra194-cactmon", .data = &actmon_data, },
	{ .compatible = "nvidia,tegra186-cactmon", .data = &actmon_data, },
	{ .compatible = "nvidia,tegra234-cactmon", .data = &actmon_data, },
	{},
};

static int tegra_actmon_resume(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct actmon_drv_data *actmon = (struct actmon_drv_data *) platform_get_drvdata(pdev);
	struct actmon_dev *adev;

	for (i = 0; i < MAX_DEVICES; i++) {
		if (actmon->devices[i].state != ACTMON_ON) {
			continue;
		}
		adev = &actmon->devices[i];

		/* dev_intr_enb */
		adev->ops.enb_dev_intr_all(offs(adev->reg_offs));
		/* init_avg */
		adev->ops.set_init_avg(adev->avg_count, offs(adev->reg_offs));
		/* cnt_wt */
		adev->ops.set_cnt_wt(adev->count_weight, offs(adev->reg_offs));

		adev->ops.set_avg_up_wm(adev->reg_ctx.dev_avg_up_wm, offs(adev->reg_offs));
		adev->ops.set_avg_dn_wm(adev->reg_ctx.dev_avg_dn_wm, offs(adev->reg_offs));
		adev->ops.set_dev_up_wm(adev->reg_ctx.dev_up_wm, offs(adev->reg_offs));
		adev->ops.set_dev_dn_wm(adev->reg_ctx.dev_dn_wm, offs(adev->reg_offs));
		adev->ops.set_dev_ctrl(adev->reg_ctx.dev_ctrl, offs(adev->reg_offs));
	}

	/* clear interrupts */
	actmon->ops.set_glb_intr(0xff, actmon->base);

	/* glb enables */
	actmon->ops.set_sample_prd(actmon->reg_ctx.glb_ctrl, actmon->base);
	actmon->ops.set_glb_intr(actmon->reg_ctx.glb_intr_en, actmon->base);

	return ret;
}

static int tegra_actmon_suspend(struct platform_device *pdev, struct pm_message pm)
{
	int ret = 0;
	int i;
	struct actmon_drv_data *actmon = (struct actmon_drv_data *) platform_get_drvdata(pdev);
	struct actmon_dev *adev;

	for (i = 0; i < MAX_DEVICES; i++) {
		if (actmon->devices[i].state != ACTMON_ON) {
			continue;
		}
		adev = &actmon->devices[i];

		adev->reg_ctx.dev_up_wm = adev->ops.get_avg_up_wm(offs(adev->reg_offs));
		adev->reg_ctx.dev_dn_wm = adev->ops.get_avg_dn_wm(offs(adev->reg_offs));
		adev->reg_ctx.dev_avg_up_wm = adev->ops.get_dev_up_wm(offs(adev->reg_offs));
		adev->reg_ctx.dev_avg_dn_wm = adev->ops.get_dev_dn_wm(offs(adev->reg_offs));
		adev->reg_ctx.dev_ctrl = adev->ops.get_dev_ctrl(offs(adev->reg_offs));
	}

	actmon->reg_ctx.glb_ctrl = actmon->ops.get_glb_ctrl(actmon->base);
	actmon->reg_ctx.glb_intr_en = actmon->ops.get_glb_intr(actmon->base);

	return ret;
}

static int __init tegra_actmon_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct of_device_id *of_id;
	struct actmon_drv_data *actmon;

	of_id = of_match_node(tegra_actmon_of_match, pdev->dev.of_node);
	if (of_id == NULL) {
		pr_err("No matching of node\n");
		ret = -ENODATA;
		return ret;
	}
	actmon = (struct actmon_drv_data *)of_id->data;
	platform_set_drvdata(pdev, actmon);
	actmon->pdev = pdev;
	ret = tegra_actmon_register(actmon);
	return ret;
}

static struct platform_driver tegra19x_actmon_driver __refdata = {
	.probe		= tegra_actmon_probe,
	.remove		= tegra_actmon_remove,
	.resume		= tegra_actmon_resume,
	.suspend	= tegra_actmon_suspend,
	.driver	= {
		.name	= "tegra_actmon",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_actmon_of_match),
	},
};

static int __init cactmon_init(void)
{
	return platform_driver_register(&tegra19x_actmon_driver);
}

static void __exit cactmon_exit(void)
{
	platform_driver_unregister(&tegra19x_actmon_driver);
}

late_initcall(cactmon_init);
module_exit(cactmon_exit);
