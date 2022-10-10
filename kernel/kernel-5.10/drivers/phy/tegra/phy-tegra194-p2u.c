// SPDX-License-Identifier: GPL-2.0+
/*
 * P2U (PIPE to UPHY) driver for Tegra T194 SoC
 *
 * Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
 *
 * Author: Vidya Sagar <vidyas@nvidia.com>
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <soc/tegra/bpmp.h>
#include <soc/tegra/bpmp-abi.h>

#define P2U_CONTROL_CMN			0x74
#define P2U_CONTROL_CMN_ENABLE_L2_EXIT_RATE_CHANGE	BIT(13)
#define P2U_CONTROL_CMN_SKP_SIZE_PROTECTION_EN		BIT(20)

#define P2U_CONTROL_GEN1	0x78U
#define P2U_CONTROL_GEN1_ENABLE_RXIDLE_ENTRY_ON_LINK_STATUS	BIT(2)
#define P2U_CONTROL_GEN1_ENABLE_RXIDLE_ENTRY_ON_EIOS		BIT(3)

#define P2U_PERIODIC_EQ_CTRL_GEN3	0xc0
#define P2U_PERIODIC_EQ_CTRL_GEN3_PERIODIC_EQ_EN		BIT(0)
#define P2U_PERIODIC_EQ_CTRL_GEN3_INIT_PRESET_EQ_TRAIN_EN	BIT(1)
#define P2U_PERIODIC_EQ_CTRL_GEN4	0xc4
#define P2U_PERIODIC_EQ_CTRL_GEN4_INIT_PRESET_EQ_TRAIN_EN	BIT(1)

#define P2U_RX_DEBOUNCE_TIME				0xa4
#define P2U_RX_DEBOUNCE_TIME_DEBOUNCE_TIMER_MASK	0xffff
#define P2U_RX_DEBOUNCE_TIME_DEBOUNCE_TIMER_VAL		160

#define P2U_DIR_SEARCH_CTRL				0xd4
#define P2U_DIR_SEARCH_CTRL_GEN4_FINE_GRAIN_SEARCH_TWICE	BIT(18)

#define P2U_RX_MARGIN_SW_INT_EN		0xe0
#define P2U_RX_MARGIN_SW_INT_EN_READINESS		BIT(0)
#define P2U_RX_MARGIN_SW_INT_EN_MARGIN_START		BIT(1)
#define P2U_RX_MARGIN_SW_INT_EN_MARGIN_CHANGE		BIT(2)
#define P2U_RX_MARGIN_SW_INT_EN_MARGIN_STOP		BIT(3)

#define P2U_RX_MARGIN_SW_INT		0xe4
#define P2U_RX_MARGIN_SW_INT_MASK			0xf
#define P2U_RX_MARGIN_SW_INT_READINESS			BIT(0)
#define P2U_RX_MARGIN_SW_INT_MARGIN_START		BIT(1)
#define P2U_RX_MARGIN_SW_INT_MARGIN_CHANGE		BIT(2)
#define P2U_RX_MARGIN_SW_INT_MARGIN_STOP		BIT(3)

#define P2U_RX_MARGIN_SW_STATUS		0xe8
#define P2U_RX_MARGIN_SW_STATUS_MARGIN_SW_READY		BIT(0)
#define P2U_RX_MARGIN_SW_STATUS_MARGIN_READY		BIT(1)
#define P2U_RX_MARGIN_SW_STATUS_PHY_MARGIN_STATUS	BIT(2)
#define P2U_RX_MARGIN_SW_STATUS_PHY_MARGIN_ERROR_STATUS	BIT(3)

#define P2U_RX_MARGIN_CTRL		0xec
#define P2U_RX_MARGIN_CTRL_EN				BIT(0)
#define P2U_RX_MARGIN_CTRL_N_BLKS_MASK			0x7f8000
#define P2U_RX_MARGIN_CTRL_N_BLKS_SHIFT			15

/* Any value between {0x80, 0xFF}, randomly selected 0x81 */
#define N_BLKS_COUNT	0x81

#define P2U_RX_MARGIN_STATUS		0xf0
#define P2U_RX_MARGIN_STATUS_ERRORS_MASK		0xffff

#define P2U_RX_MARGIN_CONTROL		0xf4
#define P2U_RX_MARGIN_CONTROL_START			BIT(0)

#define P2U_RX_MARGIN_CYA_CTRL		0xf8
#define P2U_RX_MARGIN_CYA_CTRL_IND_X			BIT(0)
#define P2U_RX_MARGIN_CYA_CTRL_IND_Y			BIT(1)

#define RX_MARGIN_START_CHANGE	1
#define RX_MARGIN_STOP		2
#define RX_MARGIN_GET_MARGIN	3

struct tegra_p2u_of_data {
	bool one_dir_search;
	bool lane_margin;
	bool eios_override;
};

struct tegra_p2u {
	void __iomem *base;
	bool skip_sz_protection_en;	/* Needed to support two retimers */
	struct tegra_p2u_of_data *of_data;
	struct device *dev;
	struct tegra_bpmp *bpmp;
	u32 id;
	struct work_struct rx_margin_work;
	u32 next_state;
	spinlock_t next_state_lock;	/* lock for next_state */
};

struct margin_ctrl {
	u32 en:1;
	u32 clr:1;
	u32 x:7;
	u32 y:6;
	u32 n_blks:8;
};

static inline void p2u_writel(struct tegra_p2u *phy, const u32 value,
			      const u32 reg)
{
	writel_relaxed(value, phy->base + reg);
}

static inline u32 p2u_readl(struct tegra_p2u *phy, const u32 reg)
{
	return readl_relaxed(phy->base + reg);
}

static int tegra_p2u_power_on(struct phy *x)
{
	struct tegra_p2u *phy = phy_get_drvdata(x);
	u32 val;

	if (phy->skip_sz_protection_en) {
		val = p2u_readl(phy, P2U_CONTROL_CMN);
		val |= P2U_CONTROL_CMN_SKP_SIZE_PROTECTION_EN;
		p2u_writel(phy, val, P2U_CONTROL_CMN);
	}

	val = p2u_readl(phy, P2U_CONTROL_GEN1);

	if (phy->of_data->eios_override)
		val &= ~P2U_CONTROL_GEN1_ENABLE_RXIDLE_ENTRY_ON_EIOS;

	val |= (P2U_CONTROL_GEN1_ENABLE_RXIDLE_ENTRY_ON_LINK_STATUS);
	p2u_writel(phy, val, P2U_CONTROL_GEN1);

	val = p2u_readl(phy, P2U_PERIODIC_EQ_CTRL_GEN3);
	val &= ~P2U_PERIODIC_EQ_CTRL_GEN3_PERIODIC_EQ_EN;
	val |= P2U_PERIODIC_EQ_CTRL_GEN3_INIT_PRESET_EQ_TRAIN_EN;
	p2u_writel(phy, val, P2U_PERIODIC_EQ_CTRL_GEN3);

	val = p2u_readl(phy, P2U_PERIODIC_EQ_CTRL_GEN4);
	val |= P2U_PERIODIC_EQ_CTRL_GEN4_INIT_PRESET_EQ_TRAIN_EN;
	p2u_writel(phy, val, P2U_PERIODIC_EQ_CTRL_GEN4);

	val = p2u_readl(phy, P2U_RX_DEBOUNCE_TIME);
	val &= ~P2U_RX_DEBOUNCE_TIME_DEBOUNCE_TIMER_MASK;
	val |= P2U_RX_DEBOUNCE_TIME_DEBOUNCE_TIMER_VAL;
	p2u_writel(phy, val, P2U_RX_DEBOUNCE_TIME);

	if (phy->of_data->one_dir_search) {
		val = p2u_readl(phy, P2U_DIR_SEARCH_CTRL);
		val &= ~P2U_DIR_SEARCH_CTRL_GEN4_FINE_GRAIN_SEARCH_TWICE;
		p2u_writel(phy, val, P2U_DIR_SEARCH_CTRL);
	}

	if (phy->of_data->lane_margin) {
		val = P2U_RX_MARGIN_SW_INT_EN_READINESS |
		      P2U_RX_MARGIN_SW_INT_EN_MARGIN_START |
		      P2U_RX_MARGIN_SW_INT_EN_MARGIN_CHANGE |
		      P2U_RX_MARGIN_SW_INT_EN_MARGIN_STOP;
		writel(val, phy->base + P2U_RX_MARGIN_SW_INT_EN);

		val = readl(phy->base + P2U_RX_MARGIN_CYA_CTRL);
		val |= P2U_RX_MARGIN_CYA_CTRL_IND_X;
		val |= P2U_RX_MARGIN_CYA_CTRL_IND_Y;
		writel(val, phy->base + P2U_RX_MARGIN_CYA_CTRL);
	}

	return 0;
}

int tegra_p2u_calibrate(struct phy *x)
{
	struct tegra_p2u *phy = phy_get_drvdata(x);
	u32 val;

	val = p2u_readl(phy, P2U_CONTROL_CMN);
	val |= P2U_CONTROL_CMN_ENABLE_L2_EXIT_RATE_CHANGE;
	p2u_writel(phy, val, P2U_CONTROL_CMN);

	return 0;
}

static const struct phy_ops ops = {
	.power_on = tegra_p2u_power_on,
	.calibrate = tegra_p2u_calibrate,
	.owner = THIS_MODULE,
};

static int set_margin_control(struct tegra_p2u *phy, u32 ctrl_data)
{
	struct tegra_bpmp_message msg;
	struct mrq_uphy_response resp;
	struct mrq_uphy_request req;
	struct margin_ctrl ctrl;
	int err;

	memcpy(&ctrl, &ctrl_data, sizeof(ctrl_data));

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.lane = phy->id;
	req.cmd = CMD_UPHY_PCIE_LANE_MARGIN_CONTROL;
	req.uphy_set_margin_control.en = ctrl.en;
	req.uphy_set_margin_control.clr = ctrl.clr;
	req.uphy_set_margin_control.x = ctrl.x;
	req.uphy_set_margin_control.y = ctrl.y;
	req.uphy_set_margin_control.nblks = ctrl.n_blks;

	memset(&msg, 0, sizeof(msg));
	msg.mrq = MRQ_UPHY;
	msg.tx.data = &req;
	msg.tx.size = sizeof(req);
	msg.rx.data = &resp;
	msg.rx.size = sizeof(resp);

	err = tegra_bpmp_transfer(phy->bpmp, &msg);
	if (err)
		return err;
	if (msg.rx.ret)
		return -EINVAL;

	return 0;
}

static int get_margin_status(struct tegra_p2u *phy, u32 *val)
{
	struct tegra_bpmp_message msg;
	struct mrq_uphy_response resp;
	struct mrq_uphy_request req;
	int rc;

	req.lane = phy->id;
	req.cmd = CMD_UPHY_PCIE_LANE_MARGIN_STATUS;

	memset(&msg, 0, sizeof(msg));
	msg.mrq = MRQ_UPHY;
	msg.tx.data = &req;
	msg.tx.size = sizeof(req);
	msg.rx.data = &resp;
	msg.rx.size = sizeof(resp);

	rc = tegra_bpmp_transfer(phy->bpmp, &msg);
	if (rc)
		return rc;
	if (msg.rx.ret)
		return -EINVAL;

	*val = resp.uphy_get_margin_status.status;

	return 0;
}

static void rx_margin_work_fn(struct work_struct *work)
{
	struct tegra_p2u *phy = container_of(work, struct tegra_p2u,
					     rx_margin_work);
	struct device *dev = phy->dev;
	unsigned long flags;
	u32 val;
	int ret;
	u8 state;

	do {
		spin_lock_irqsave(&phy->next_state_lock, flags);
		state = phy->next_state;
		spin_unlock_irqrestore(&phy->next_state_lock, flags);
		switch (state) {
		case RX_MARGIN_START_CHANGE:
		case RX_MARGIN_STOP:
			val = readl(phy->base + P2U_RX_MARGIN_CTRL);
			ret = set_margin_control(phy, val);
			if (ret) {
				dev_err(dev, "MARGIN_SET err: %d\n", ret);
				break;
			}
			val = readl(phy->base + P2U_RX_MARGIN_SW_STATUS);
			val |= P2U_RX_MARGIN_SW_STATUS_MARGIN_SW_READY;
			val |= P2U_RX_MARGIN_SW_STATUS_MARGIN_READY;
			val |= P2U_RX_MARGIN_SW_STATUS_PHY_MARGIN_STATUS;
			val |= P2U_RX_MARGIN_SW_STATUS_PHY_MARGIN_ERROR_STATUS;
			writel(val, phy->base + P2U_RX_MARGIN_SW_STATUS);

			usleep_range(10, 11);

			if (state != RX_MARGIN_STOP) {
				spin_lock_irqsave(&phy->next_state_lock, flags);
				phy->next_state = RX_MARGIN_GET_MARGIN;
				spin_unlock_irqrestore(&phy->next_state_lock,
						       flags);
				continue;
			}
			fallthrough;

		case RX_MARGIN_GET_MARGIN:
			if (state != RX_MARGIN_STOP) {
				ret = get_margin_status(phy, &val);
				if (ret) {
					dev_err(dev, "MARGIN_GET err: %d\n",
						ret);
					break;
				}
				writel(val & P2U_RX_MARGIN_STATUS_ERRORS_MASK,
				       phy->base + P2U_RX_MARGIN_STATUS);
			}
			val = readl(phy->base + P2U_RX_MARGIN_SW_STATUS);
			val |= P2U_RX_MARGIN_SW_STATUS_MARGIN_SW_READY;
			val |= P2U_RX_MARGIN_SW_STATUS_MARGIN_READY;
			val &= ~P2U_RX_MARGIN_SW_STATUS_PHY_MARGIN_STATUS;
			val |= P2U_RX_MARGIN_SW_STATUS_PHY_MARGIN_ERROR_STATUS;
			writel(val, phy->base + P2U_RX_MARGIN_SW_STATUS);

			if (state != RX_MARGIN_STOP) {
				msleep(20);
				continue;
			} else {
				return;
			}
			break;

		default:
			dev_err(dev, "Invalid margin state\n");
			return;
		};
	} while (1);
}

static irqreturn_t tegra_p2u_irq_handler(int irq, void *arg)
{
	struct tegra_p2u *phy = (struct tegra_p2u *)arg;
	unsigned long flags;
	u32 val = 0;

	val = readl(phy->base + P2U_RX_MARGIN_SW_INT);
	writel(val, phy->base + P2U_RX_MARGIN_SW_INT);
	switch (val & P2U_RX_MARGIN_SW_INT_MASK) {
	case P2U_RX_MARGIN_SW_INT_READINESS:
		dev_dbg(phy->dev, "Rx_Margin_intr : READINESS\n");
		val = readl(phy->base + P2U_RX_MARGIN_SW_STATUS);
		val |= P2U_RX_MARGIN_SW_STATUS_MARGIN_SW_READY;
		val |= P2U_RX_MARGIN_SW_STATUS_MARGIN_READY;
		writel(val, phy->base + P2U_RX_MARGIN_SW_STATUS);
		/* Write N_BLKS with any value between {0x80, 0xFF} */
		val = readl(phy->base + P2U_RX_MARGIN_CTRL);
		val &= P2U_RX_MARGIN_CTRL_N_BLKS_MASK;
		val |= (N_BLKS_COUNT << P2U_RX_MARGIN_CTRL_N_BLKS_SHIFT);
		writel(val, phy->base + P2U_RX_MARGIN_CTRL);
		break;

	case P2U_RX_MARGIN_SW_INT_MARGIN_STOP:
		dev_dbg(phy->dev, "Rx_Margin_intr : MARGIN_STOP\n");
		spin_lock_irqsave(&phy->next_state_lock, flags);
		phy->next_state = RX_MARGIN_STOP;
		spin_unlock_irqrestore(&phy->next_state_lock, flags);
		break;

	case P2U_RX_MARGIN_SW_INT_MARGIN_CHANGE:
	case (P2U_RX_MARGIN_SW_INT_MARGIN_CHANGE |
	      P2U_RX_MARGIN_SW_INT_MARGIN_START):
		dev_dbg(phy->dev, "Rx_Margin_intr : MARGIN_CHANGE\n");
		spin_lock_irqsave(&phy->next_state_lock, flags);
		phy->next_state = RX_MARGIN_START_CHANGE;
		spin_unlock_irqrestore(&phy->next_state_lock, flags);
		fallthrough;

	case P2U_RX_MARGIN_SW_INT_MARGIN_START:
		dev_dbg(phy->dev, "Rx_Margin_intr : MARGIN_START\n");
		schedule_work(&phy->rx_margin_work);
		break;

	default:
		dev_err(phy->dev, "INVALID Rx_Margin_intr : 0x%x\n", val);
		break;
	}

	return IRQ_HANDLED;
}

static int tegra_p2u_probe(struct platform_device *pdev)
{
	struct phy_provider *phy_provider;
	struct device *dev = &pdev->dev;
	struct phy *generic_phy;
	struct tegra_p2u *phy;
	struct resource *res;
	int ret;
	u32 irq;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	phy->of_data =
		(struct tegra_p2u_of_data *)of_device_get_match_data(dev);
	if (!phy->of_data)
		return -EINVAL;

	phy->dev = dev;
	platform_set_drvdata(pdev, phy);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ctl");
	phy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->base))
		return PTR_ERR(phy->base);

	phy->skip_sz_protection_en =
		of_property_read_bool(dev->of_node,
				      "nvidia,skip-sz-protect-en");

	platform_set_drvdata(pdev, phy);

	generic_phy = devm_phy_create(dev, NULL, &ops);
	if (IS_ERR(generic_phy))
		return PTR_ERR(generic_phy);

	phy_set_drvdata(generic_phy, phy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	if (phy->of_data->lane_margin) {
		spin_lock_init(&phy->next_state_lock);
		INIT_WORK(&phy->rx_margin_work, rx_margin_work_fn);

		irq = platform_get_irq_byname(pdev, "intr");
		if (!irq) {
			dev_err(dev, "failed to get intr interrupt\n");
			return irq;
		}

		ret = devm_request_irq(&pdev->dev, irq, tegra_p2u_irq_handler,
				       0, "tegra-p2u-intr", phy);
		if (ret) {
			dev_err(dev, "failed to request \"intr\" irq\n");
			return ret;
		}

		ret = of_property_read_u32_index(dev->of_node, "nvidia,bpmp",
						 1, &phy->id);
		if (ret) {
			dev_err(dev, "Failed to read P2U id: %d\n", ret);
			return ret;
		}

		phy->bpmp = tegra_bpmp_get(dev);
		if (IS_ERR(phy->bpmp))
			return PTR_ERR(phy->bpmp);
	}

	return 0;
}

static int tegra_p2u_remove(struct platform_device *pdev)
{
	struct tegra_p2u *phy = platform_get_drvdata(pdev);

	if (phy->of_data->lane_margin)
		tegra_bpmp_put(phy->bpmp);

	return 0;
}

static const struct tegra_p2u_of_data tegra_p2u_of_data_t194 = {
	.one_dir_search = false,
	.lane_margin = false,
	.eios_override = true,
};

static const struct tegra_p2u_of_data tegra_p2u_of_data_t234 = {
	.one_dir_search = true,
	.lane_margin = true,
	.eios_override = false,
};

static const struct of_device_id tegra_p2u_id_table[] = {
	{
		.compatible = "nvidia,tegra194-p2u",
		.data = &tegra_p2u_of_data_t194,
	},
	{
		.compatible = "nvidia,tegra234-p2u",
		.data = &tegra_p2u_of_data_t234,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_p2u_id_table);

static struct platform_driver tegra_p2u_driver = {
	.probe = tegra_p2u_probe,
	.remove = tegra_p2u_remove,
	.driver = {
		.name = "tegra194-p2u",
		.of_match_table = tegra_p2u_id_table,
	},
};
module_platform_driver(tegra_p2u_driver);

MODULE_AUTHOR("Vidya Sagar <vidyas@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra194 PIPE2UPHY PHY driver");
MODULE_LICENSE("GPL v2");
