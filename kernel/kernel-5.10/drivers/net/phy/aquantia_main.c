// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Aquantia PHY
 *
 * Author: Shaohui Xie <Shaohui.Xie@freescale.com>
 *
 * Copyright 2015 Freescale Semiconductor, Inc.
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/phy.h>
#include <soc/tegra/fuse.h>
#include <linux/netdevice.h>
#include <linux/of.h>

#include "aquantia.h"

#define PHY_ID_AQ1202	0x03a1b445
#define PHY_ID_AQ2104	0x03a1b460
#define PHY_ID_AQR105	0x03a1b4a2
#define PHY_ID_AQR106	0x03a1b4d0
#define PHY_ID_AQR107	0x03a1b4e0
#define PHY_ID_AQCS109	0x03a1b5c2
#define PHY_ID_AQR405	0x03a1b4b0
#define PHY_ID_AQR113C	0x31c31c12

#define MDIO_PHYXS_VEND_IF_STATUS		0xe812
#define MDIO_PHYXS_VEND_IF_STATUS_TYPE_MASK	GENMASK(7, 3)
#define MDIO_PHYXS_VEND_IF_STATUS_TYPE_KR	0
#define MDIO_PHYXS_VEND_IF_STATUS_TYPE_XFI	2
#define MDIO_PHYXS_VEND_IF_STATUS_TYPE_USXGMII	3
#define MDIO_PHYXS_VEND_IF_STATUS_TYPE_SGMII	6
#define MDIO_PHYXS_VEND_IF_STATUS_TYPE_OCSGMII	10
#define MDIO_PHYXS_VEND_IF_STATUS_TX_READY	BIT(12)

#define MDIO_AN_RSVD_VEND_STATUS3		0xc812

#define MDIO_AN_VEND_PROV			0xc400
#define MDIO_AN_VEND_PROV_1000BASET_FULL	BIT(15)
#define MDIO_AN_VEND_PROV_1000BASET_HALF	BIT(14)
#define MDIO_AN_VEND_PROV_AQRATE_DWN_SHFT_CAP	BIT(12)
#define MDIO_AN_VEND_PROV_DOWNSHIFT_EN		BIT(4)
#define MDIO_AN_VEND_PROV_DOWNSHIFT_MASK	GENMASK(3, 0)
#define MDIO_AN_VEND_PROV_DOWNSHIFT_DFLT	4

#define MDIO_AN_RSVD_VEND_PROV1			0xc410
#define MDIO_AN_TX_VEND_STATUS1			0xc800
#define MDIO_AN_TX_VEND_STATUS1_RATE_MASK	GENMASK(3, 1)
#define MDIO_AN_TX_VEND_STATUS1_10BASET		0
#define MDIO_AN_TX_VEND_STATUS1_100BASETX	1
#define MDIO_AN_TX_VEND_STATUS1_1000BASET	2
#define MDIO_AN_TX_VEND_STATUS1_10GBASET	3
#define MDIO_AN_TX_VEND_STATUS1_2500BASET	4
#define MDIO_AN_TX_VEND_STATUS1_5000BASET	5
#define MDIO_AN_TX_VEND_STATUS1_FULL_DUPLEX	BIT(0)

#define MDIO_AN_TX_VEND_INT_STATUS1		0xcc00
#define MDIO_AN_TX_VEND_INT_STATUS1_DOWNSHIFT	BIT(1)

#define MDIO_AN_TX_VEND_INT_STATUS2		0xcc01

#define MDIO_AN_TX_VEND_INT_MASK2		0xd401
#define MDIO_AN_TX_VEND_INT_MASK2_LINK		BIT(0)

#define MDIO_AN_RX_LP_STAT1			0xe820
#define MDIO_AN_RX_LP_STAT1_1000BASET_FULL	BIT(15)
#define MDIO_AN_RX_LP_STAT1_1000BASET_HALF	BIT(14)
#define MDIO_AN_RX_LP_STAT1_SHORT_REACH		BIT(13)
#define MDIO_AN_RX_LP_STAT1_AQRATE_DOWNSHIFT	BIT(12)
#define MDIO_AN_RX_LP_STAT1_AQ_PHY		BIT(2)

#define MDIO_AN_RX_LP_STAT4			0xe823
#define MDIO_AN_RX_LP_STAT4_FW_MAJOR		GENMASK(15, 8)
#define MDIO_AN_RX_LP_STAT4_FW_MINOR		GENMASK(7, 0)

#define MDIO_AN_RX_VEND_STAT3			0xe832
#define MDIO_AN_RX_VEND_STAT3_AFR		BIT(0)

#define MDIO_AN_VEND_PROV1		0xC440
#define MDIO_AN_VEND_PROV1_5G		BIT(11)
#define MDIO_AN_VEND_PROV1_2_5G		BIT(10)
#define MDIO_AN_10GBT_CTRL_5GBASET	BIT(8)
#define MDIO_AN_10GBT_CTRL_10GBASET	BIT(12)
#define MDIO_AN_10GBT_CTRL_2_5GBASET	BIT(7)

#define MDIO_AN_PAUSE			BIT(10)
#define MDIO_AN_ASYM_PAUSE		BIT(11)

#define MDIO_AN_LD_LOOP_TIMING_ABILITY	BIT(0)
#define MDIO_MMD_AN_WOL_ENABLE		BIT(6)
#define MDIO_AN_VEND_MASK		0xF0FF

/* MDIO_MMD_C22EXT */
#define MDIO_C22EXT_MAGIC_PKT_PATTREN_0_2_15		0xc339
#define MDIO_C22EXT_MAGIC_PKT_PATTREN_16_2_31		0xc33a
#define MDIO_C22EXT_MAGIC_PKT_PATTREN_32_2_47		0xc33b
#define MDIO_C22EXT_GBE_PHY_RSI1_CTRL6			0xc355
#define MDIO_C22EXT_GBE_PHY_RSI1_CTRL7			0xc356
#define MDIO_C22EXT_GBE_PHY_RSI1_CTRL8			0xc357
#define MDIO_C22EXT_GBE_PHY_SGMII_TX_INT_MASK1		0xf420
#define MDIO_C22EXT_STAT_SGMII_RX_GOOD_FRAMES		0xd292
#define MDIO_C22EXT_STAT_SGMII_RX_BAD_FRAMES		0xd294
#define MDIO_C22EXT_STAT_SGMII_RX_FALSE_CARRIER		0xd297
#define MDIO_C22EXT_STAT_SGMII_TX_GOOD_FRAMES		0xd313
#define MDIO_C22EXT_STAT_SGMII_TX_BAD_FRAMES		0xd315
#define MDIO_C22EXT_STAT_SGMII_TX_FALSE_CARRIER		0xd317
#define MDIO_C22EXT_STAT_SGMII_TX_COLLISIONS		0xd318
#define MDIO_C22EXT_STAT_SGMII_TX_LINE_COLLISIONS	0xd319
#define MDIO_C22EXT_STAT_SGMII_TX_FRAME_ALIGN_ERR	0xd31a
#define MDIO_C22EXT_STAT_SGMII_TX_RUNT_FRAMES		0xd31b
#define MDIO_C22EXT_GBE_PHY_SGMII_TX_ALARM1		0xec20

#define	MDIO_C22EXT_RSI_WAKE_UP_FRAME_DETECTION		BIT(0)
#define	MDIO_C22EXT_RSI_MAGIC_PKT_FRAME_DETECTION	BIT(0)
#define MDIO_C22EXT_RSI_WOL_FCS_MONITOR_MODE		BIT(15)
#define MDIO_C22EXT_SGMII0_WAKE_UP_FRAME_MASK		BIT(4)
#define MDIO_C22EXT_SGMII0_MAGIC_PKT_FRAME_MASK		BIT(5)

/* Vendor specific 1, MDIO_MMD_VEND1 */
#define VEND1_GLOBAL_FW_ID			0x0020
#define VEND1_GLOBAL_FW_ID_MAJOR		GENMASK(15, 8)
#define VEND1_GLOBAL_FW_ID_MINOR		GENMASK(7, 0)

#define VEND1_GLOBAL_RSVD_STAT1			0xc885
#define VEND1_GLOBAL_RSVD_STAT1_FW_BUILD_ID	GENMASK(7, 4)
#define VEND1_GLOBAL_RSVD_STAT1_PROV_ID		GENMASK(3, 0)

#define VEND1_GLOBAL_RSVD_STAT9			0xc88d
#define VEND1_GLOBAL_RSVD_STAT9_MODE		GENMASK(7, 0)
#define VEND1_GLOBAL_RSVD_STAT9_1000BT2		0x23

#define VEND1_GLOBAL_INT_STD_STATUS		0xfc00
#define VEND1_GLOBAL_INT_VEND_STATUS		0xfc01

#define VEND1_GLOBAL_INT_STD_MASK		0xff00
#define VEND1_GLOBAL_INT_STD_MASK_PMA1		BIT(15)
#define VEND1_GLOBAL_INT_STD_MASK_PMA2		BIT(14)
#define VEND1_GLOBAL_INT_STD_MASK_PCS1		BIT(13)
#define VEND1_GLOBAL_INT_STD_MASK_PCS2		BIT(12)
#define VEND1_GLOBAL_INT_STD_MASK_PCS3		BIT(11)
#define VEND1_GLOBAL_INT_STD_MASK_PHY_XS1	BIT(10)
#define VEND1_GLOBAL_INT_STD_MASK_PHY_XS2	BIT(9)
#define VEND1_GLOBAL_INT_STD_MASK_AN1		BIT(8)
#define VEND1_GLOBAL_INT_STD_MASK_AN2		BIT(7)
#define VEND1_GLOBAL_INT_STD_MASK_GBE		BIT(6)
#define VEND1_GLOBAL_INT_STD_MASK_ALL		BIT(0)

#define VEND1_GLOBAL_INT_VEND_MASK		0xff01
#define VEND1_GLOBAL_INT_VEND_MASK_PMA		BIT(15)
#define VEND1_GLOBAL_INT_VEND_MASK_PCS		BIT(14)
#define VEND1_GLOBAL_INT_VEND_MASK_PHY_XS	BIT(13)
#define VEND1_GLOBAL_INT_VEND_MASK_AN		BIT(12)
#define VEND1_GLOBAL_INT_VEND_MASK_GBE		BIT(11)
#define VEND1_GLOBAL_INT_VEND_MASK_GLOBAL1	BIT(2)
#define VEND1_GLOBAL_INT_VEND_MASK_GLOBAL2	BIT(1)
#define VEND1_GLOBAL_INT_VEND_MASK_GLOBAL3	BIT(0)

#define VEND1_GLOBAL_MDIO_CTRL1			0x0
#define VEND1_GLOBAL_MDIO_CTRL1_SOFT_RST	BIT(15)

#define VEND1_GLOBAL_MDIO_PHYXS_PROV2		0xC441
#define VEND1_GLOBAL_MDIO_PHYXS_PROV2_USX_AN	BIT(3)

#define VEND1_SEC_INGRESS_CNTRL_REG1		0x7001
#define VEND1_GLOBAL_SYS_CONFIG_100M		0x31b
#define VEND1_GLOBAL_SYS_CONFIG_1G		0x31c

#define VEND1_GLOBAL_SYS_CONFIG_SGMII		(BIT(0) | BIT(1) | BIT(3))
#define VEND1_GLOBAL_SYS_CONFIG_XFI		BIT(8)

#define VEND1_GLOBAL_CFG_2_5G			0x031D
#define VEND1_GLOBAL_CFG_5G			0x031E
#define VEND1_GLOBAL_CFG_10G			0x031F

#define BIT_SHIFT_8 8
#define MAC_ADDRESS_BYTE_0 0
#define MAC_ADDRESS_BYTE_1 1
#define MAC_ADDRESS_BYTE_2 2
#define MAC_ADDRESS_BYTE_3 3
#define MAC_ADDRESS_BYTE_4 4
#define MAC_ADDRESS_BYTE_5 5

struct aqr107_hw_stat {
	const char *name;
	int reg;
	int size;
};

#define SGMII_STAT(n, r, s) { n, MDIO_C22EXT_STAT_SGMII_ ## r, s }
static const struct aqr107_hw_stat aqr107_hw_stats[] = {
	SGMII_STAT("sgmii_rx_good_frames",	    RX_GOOD_FRAMES,	26),
	SGMII_STAT("sgmii_rx_bad_frames",	    RX_BAD_FRAMES,	26),
	SGMII_STAT("sgmii_rx_false_carrier_events", RX_FALSE_CARRIER,	 8),
	SGMII_STAT("sgmii_tx_good_frames",	    TX_GOOD_FRAMES,	26),
	SGMII_STAT("sgmii_tx_bad_frames",	    TX_BAD_FRAMES,	26),
	SGMII_STAT("sgmii_tx_false_carrier_events", TX_FALSE_CARRIER,	 8),
	SGMII_STAT("sgmii_tx_collisions",	    TX_COLLISIONS,	 8),
	SGMII_STAT("sgmii_tx_line_collisions",	    TX_LINE_COLLISIONS,	 8),
	SGMII_STAT("sgmii_tx_frame_alignment_err",  TX_FRAME_ALIGN_ERR,	16),
	SGMII_STAT("sgmii_tx_runt_frames",	    TX_RUNT_FRAMES,	22),
};
#define AQR107_SGMII_STAT_SZ ARRAY_SIZE(aqr107_hw_stats)

struct aqr107_priv {
	u64 sgmii_stats[AQR107_SGMII_STAT_SZ];
	int wol_status;
};

static int aqr107_get_sset_count(struct phy_device *phydev)
{
	return AQR107_SGMII_STAT_SZ;
}

static void aqr107_get_strings(struct phy_device *phydev, u8 *data)
{
	int i;

	for (i = 0; i < AQR107_SGMII_STAT_SZ; i++)
		strscpy(data + i * ETH_GSTRING_LEN, aqr107_hw_stats[i].name,
			ETH_GSTRING_LEN);
}

static u64 aqr107_get_stat(struct phy_device *phydev, int index)
{
	const struct aqr107_hw_stat *stat = aqr107_hw_stats + index;
	int len_l = min(stat->size, 16);
	int len_h = stat->size - len_l;
	u64 ret;
	int val;

	val = phy_read_mmd(phydev, MDIO_MMD_C22EXT, stat->reg);
	if (val < 0)
		return U64_MAX;

	ret = val & GENMASK(len_l - 1, 0);
	if (len_h) {
		val = phy_read_mmd(phydev, MDIO_MMD_C22EXT, stat->reg + 1);
		if (val < 0)
			return U64_MAX;

		ret += (val & GENMASK(len_h - 1, 0)) << 16;
	}

	return ret;
}

static void aqr107_get_stats(struct phy_device *phydev,
			     struct ethtool_stats *stats, u64 *data)
{
	struct aqr107_priv *priv = phydev->priv;
	u64 val;
	int i;

	for (i = 0; i < AQR107_SGMII_STAT_SZ; i++) {
		val = aqr107_get_stat(phydev, i);
		if (val == U64_MAX)
			phydev_err(phydev, "Reading HW Statistics failed for %s\n",
				   aqr107_hw_stats[i].name);
		else
			priv->sgmii_stats[i] += val;

		data[i] = priv->sgmii_stats[i];
	}
}

static int aqr_config_aneg(struct phy_device *phydev)
{
	struct device_node *node = phydev->mdio.dev.of_node;
	bool changed = false;
	u16 reg;
	int ret, err;
	int phy_mode;

	if (phydev->autoneg == AUTONEG_DISABLE)
		return genphy_c45_pma_setup_forced(phydev);

	ret = genphy_c45_an_config_aneg(phydev);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	/* Clause 45 has no standardized support for 1000BaseT, therefore
	 * use vendor registers for this mode.
	 */
	reg = 0;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
			      phydev->advertising))
		reg |= MDIO_AN_VEND_PROV_1000BASET_FULL;

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
			      phydev->advertising))
		reg |= MDIO_AN_VEND_PROV_1000BASET_HALF;

	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_AN, MDIO_AN_VEND_PROV,
				     MDIO_AN_VEND_PROV_1000BASET_HALF |
				     MDIO_AN_VEND_PROV_1000BASET_FULL, reg);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	err = of_property_read_u32(node, "aquantia,phy_mode", &phy_mode);
	if (!err) {
		if (phy_mode == 1) {
			phydev_info(phydev, "Configuring AQR PHY to 5G Mode\n");
			phy_write_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_CFG_2_5G, 0x0106);
			phy_write_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_CFG_5G, 0x0106);
			phy_write_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_CFG_10G, 0x0000);
			/* Disable 10G advertizement and restart autoneg */
			phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL, 0x01E1);
			/* restart auto-negotiation */
			genphy_c45_restart_aneg(phydev);
			phy_write_mmd(phydev, MDIO_MMD_PHYXS, VEND1_GLOBAL_MDIO_PHYXS_PROV2, 0x8);
		}
	} else {
		phydev_info(phydev, "No AQR phy_mode setting in DT\n");
	}

	return genphy_c45_check_and_restart_aneg(phydev, changed);
}

static int aqr_config_intr(struct phy_device *phydev)
{
	bool en = phydev->interrupts == PHY_INTERRUPT_ENABLED;
	int err;

	err = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_TX_VEND_INT_MASK2,
			    en ? MDIO_AN_TX_VEND_INT_MASK2_LINK : 0);
	if (err < 0)
		return err;

	err = phy_write_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_INT_STD_MASK,
			    en ? VEND1_GLOBAL_INT_STD_MASK_ALL : 0);
	if (err < 0)
		return err;

	return phy_write_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_INT_VEND_MASK,
			     en ? VEND1_GLOBAL_INT_VEND_MASK_GLOBAL3 |
			     VEND1_GLOBAL_INT_VEND_MASK_AN : 0);
}

static int aqr_ack_interrupt(struct phy_device *phydev)
{
	int reg;
	int val, ret;
	struct aqr107_priv *priv = phydev->priv;
	reg = phy_read_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_GBE_PHY_SGMII_TX_ALARM1);
	if ((reg & MDIO_C22EXT_SGMII0_MAGIC_PKT_FRAME_MASK) ==
	    MDIO_C22EXT_SGMII0_MAGIC_PKT_FRAME_MASK) {
		/* Disable the WoL enable bit */
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RSVD_VEND_PROV1);
		val &= ~MDIO_MMD_AN_WOL_ENABLE;
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RSVD_VEND_PROV1, val);
		if (ret < 0)
			return ret;

		/* Restore the SERDES/System Interface back to the XFI mode */
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				    VEND1_GLOBAL_SYS_CONFIG_100M, VEND1_GLOBAL_SYS_CONFIG_XFI);
		if (ret < 0)
			return ret;
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				    VEND1_GLOBAL_SYS_CONFIG_1G, VEND1_GLOBAL_SYS_CONFIG_XFI);
		if (ret < 0)
			return ret;
		/* restart auto-negotiation */
		priv->wol_status = 0;
		return genphy_c45_restart_aneg(phydev);
	}
	reg = phy_read_mmd(phydev, MDIO_MMD_AN,
			   MDIO_AN_TX_VEND_INT_STATUS2);

	return (reg < 0) ? reg : 0;
}

static int aqr_read_status(struct phy_device *phydev)
{
	int val;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RX_LP_STAT1);
		if (val < 0)
			return val;

		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
				 phydev->lp_advertising,
				 val & MDIO_AN_RX_LP_STAT1_1000BASET_FULL);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
				 phydev->lp_advertising,
				 val & MDIO_AN_RX_LP_STAT1_1000BASET_HALF);
	}

	return genphy_c45_read_status(phydev);
}

static int aqr107_read_rate(struct phy_device *phydev)
{
	int val;

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_TX_VEND_STATUS1);
	if (val < 0)
		return val;

	switch (FIELD_GET(MDIO_AN_TX_VEND_STATUS1_RATE_MASK, val)) {
	case MDIO_AN_TX_VEND_STATUS1_10BASET:
		phydev->speed = SPEED_10;
		break;
	case MDIO_AN_TX_VEND_STATUS1_100BASETX:
		phydev->speed = SPEED_100;
		break;
	case MDIO_AN_TX_VEND_STATUS1_1000BASET:
		phydev->speed = SPEED_1000;
		break;
	case MDIO_AN_TX_VEND_STATUS1_2500BASET:
		phydev->speed = SPEED_2500;
		break;
	case MDIO_AN_TX_VEND_STATUS1_5000BASET:
		phydev->speed = SPEED_5000;
		break;
	case MDIO_AN_TX_VEND_STATUS1_10GBASET:
		phydev->speed = SPEED_10000;
		break;
	default:
		phydev->speed = SPEED_UNKNOWN;
		break;
	}

	if (val & MDIO_AN_TX_VEND_STATUS1_FULL_DUPLEX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	return 0;
}

static int aqr107_read_status(struct phy_device *phydev)
{
	int val, ret;
	struct aqr107_priv *priv = phydev->priv;

	ret = aqr_read_status(phydev);
	if (ret)
		return ret;

	if (!phydev->link || phydev->autoneg == AUTONEG_DISABLE)
		return 0;

	val = phy_read_mmd(phydev, MDIO_MMD_PHYXS, MDIO_PHYXS_VEND_IF_STATUS);
	if (val < 0)
		return val;

	switch (FIELD_GET(MDIO_PHYXS_VEND_IF_STATUS_TYPE_MASK, val)) {
	case MDIO_PHYXS_VEND_IF_STATUS_TYPE_KR:
		phydev->interface = PHY_INTERFACE_MODE_10GKR;
		break;
	case MDIO_PHYXS_VEND_IF_STATUS_TYPE_XFI:
		phydev->interface = PHY_INTERFACE_MODE_10GBASER;
		break;
	case MDIO_PHYXS_VEND_IF_STATUS_TYPE_USXGMII:
		phydev->interface = PHY_INTERFACE_MODE_USXGMII;
		break;
	case MDIO_PHYXS_VEND_IF_STATUS_TYPE_SGMII:
		phydev->interface = PHY_INTERFACE_MODE_SGMII;
		break;
	case MDIO_PHYXS_VEND_IF_STATUS_TYPE_OCSGMII:
		phydev->interface = PHY_INTERFACE_MODE_2500BASEX;
		break;
	default:
		phydev->interface = PHY_INTERFACE_MODE_NA;
		break;
	}

	if (!priv->wol_status)
		ret = phy_read_mmd_poll_timeout(phydev, MDIO_MMD_PHYXS, MDIO_PHYXS_VEND_IF_STATUS,
						val, (val & MDIO_PHYXS_VEND_IF_STATUS_TX_READY),
						20000, 2000000, false);
	if (ret) {
		phydev_err(phydev, "PHY system interface is not yet ready\n");
		return ret;
	}
	/* Read possibly downshifted rate from vendor register */
	return aqr107_read_rate(phydev);
}

static int aqr107_get_downshift(struct phy_device *phydev, u8 *data)
{
	int val, cnt, enable;

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_VEND_PROV);
	if (val < 0)
		return val;

	enable = FIELD_GET(MDIO_AN_VEND_PROV_DOWNSHIFT_EN, val);
	cnt = FIELD_GET(MDIO_AN_VEND_PROV_DOWNSHIFT_MASK, val);

	*data = enable && cnt ? cnt : DOWNSHIFT_DEV_DISABLE;

	return 0;
}

static int aqr107_set_downshift(struct phy_device *phydev, u8 cnt)
{
	int val = 0;

	if (!FIELD_FIT(MDIO_AN_VEND_PROV_DOWNSHIFT_MASK, cnt))
		return -E2BIG;

	if (cnt != DOWNSHIFT_DEV_DISABLE) {
		val = MDIO_AN_VEND_PROV_DOWNSHIFT_EN;
		val |= FIELD_PREP(MDIO_AN_VEND_PROV_DOWNSHIFT_MASK, cnt);
	}

	return phy_modify_mmd(phydev, MDIO_MMD_AN, MDIO_AN_VEND_PROV,
			      MDIO_AN_VEND_PROV_DOWNSHIFT_EN |
			      MDIO_AN_VEND_PROV_DOWNSHIFT_MASK, val);
}

static int aqr107_get_tunable(struct phy_device *phydev,
			      struct ethtool_tunable *tuna, void *data)
{
	switch (tuna->id) {
	case ETHTOOL_PHY_DOWNSHIFT:
		return aqr107_get_downshift(phydev, data);
	default:
		return -EOPNOTSUPP;
	}
}

static int aqr107_set_tunable(struct phy_device *phydev,
			      struct ethtool_tunable *tuna, const void *data)
{
	switch (tuna->id) {
	case ETHTOOL_PHY_DOWNSHIFT:
		return aqr107_set_downshift(phydev, *(const u8 *)data);
	default:
		return -EOPNOTSUPP;
	}
}

/* If we configure settings whilst firmware is still initializing the chip,
 * then these settings may be overwritten. Therefore make sure chip
 * initialization has completed. Use presence of the firmware ID as
 * indicator for initialization having completed.
 * The chip also provides a "reset completed" bit, but it's cleared after
 * read. Therefore function would time out if called again.
 */
static int aqr107_wait_reset_complete(struct phy_device *phydev)
{
	int val;

	return phy_read_mmd_poll_timeout(phydev, MDIO_MMD_VEND1,
					 VEND1_GLOBAL_FW_ID, val, val != 0,
					 20000, 2000000, false);
}

static inline int wait_for_reset_complete(struct phy_device *phydev)
{
	unsigned int retries = 1000;
	int ret = 0;

	do {
		ret = phy_read_mmd(phydev, MDIO_MMD_VEND1,
				   VEND1_GLOBAL_MDIO_CTRL1);
		if (ret < 0)
			return ret;
		msleep(1);
	} while ((ret & VEND1_GLOBAL_MDIO_CTRL1_SOFT_RST) && --retries);

	if (ret & VEND1_GLOBAL_MDIO_CTRL1_SOFT_RST)
		return -ETIMEDOUT;

	return 0;
}

static void aqr107_chip_info(struct phy_device *phydev)
{
	u8 fw_major, fw_minor, build_id, prov_id;
	int val;

	val = phy_read_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_FW_ID);
	if (val < 0)
		return;

	fw_major = FIELD_GET(VEND1_GLOBAL_FW_ID_MAJOR, val);
	fw_minor = FIELD_GET(VEND1_GLOBAL_FW_ID_MINOR, val);

	val = phy_read_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_RSVD_STAT1);
	if (val < 0)
		return;

	build_id = FIELD_GET(VEND1_GLOBAL_RSVD_STAT1_FW_BUILD_ID, val);
	prov_id = FIELD_GET(VEND1_GLOBAL_RSVD_STAT1_PROV_ID, val);

	phydev_dbg(phydev, "FW %u.%u, Build %u, Provisioning %u\n",
		   fw_major, fw_minor, build_id, prov_id);
}

static int aqr107_config_init(struct phy_device *phydev)
{
	int ret, err;

	/* Check that the PHY interface type is compatible */
	if (phydev->interface != PHY_INTERFACE_MODE_SGMII &&
	    phydev->interface != PHY_INTERFACE_MODE_2500BASEX &&
	    phydev->interface != PHY_INTERFACE_MODE_XGMII &&
	    phydev->interface != PHY_INTERFACE_MODE_USXGMII &&
	    phydev->interface != PHY_INTERFACE_MODE_10GKR &&
	    phydev->interface != PHY_INTERFACE_MODE_10GBASER)
		return -ENODEV;

	WARN(phydev->interface == PHY_INTERFACE_MODE_XGMII,
	     "Your devicetree is out of date, please update it. The AQR107 family doesn't support XGMII, maybe you mean USXGMII.\n");

	ret = aqr107_wait_reset_complete(phydev);
	if (!ret)
		aqr107_chip_info(phydev);

	/* Advertize flow control */
	linkmode_set_bit(ETHTOOL_LINK_MODE_Pause_BIT, phydev->supported);
	linkmode_set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, phydev->supported);
	linkmode_copy(phydev->advertising, phydev->supported);

	/* Configure flow control */
	ret = phy_read_mmd(phydev, MDIO_MMD_AN, 0x10);
	if (ret < 0)
		return ret;
	ret |= MDIO_AN_PAUSE | MDIO_AN_ASYM_PAUSE;
	err = phy_write_mmd(phydev, MDIO_MMD_AN, 0x10, ret);
	if (err < 0)
		return err;

	/* Enable MAC Controlled EEE */
	err = phy_write_mmd(phydev, MDIO_MMD_VEND1,
			    VEND1_SEC_INGRESS_CNTRL_REG1, 0x1100);
	if (err < 0)
		return err;

	/* Configure Magic packet frame pattern (MAC address) */
	err = phy_write_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_MAGIC_PKT_PATTREN_0_2_15,
			    phydev->attached_dev->dev_addr[MAC_ADDRESS_BYTE_0] |
			    (phydev->attached_dev->dev_addr[MAC_ADDRESS_BYTE_1] << BIT_SHIFT_8));
	if (err < 0) {
		phydev_err(phydev, "Error setting magic packet frame of 0/1st byte\n");
		return err;
	}

	err = phy_write_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_MAGIC_PKT_PATTREN_16_2_31,
			    phydev->attached_dev->dev_addr[MAC_ADDRESS_BYTE_2] |
			    (phydev->attached_dev->dev_addr[MAC_ADDRESS_BYTE_3] << BIT_SHIFT_8));
	if (err < 0) {
		phydev_err(phydev, "Error setting magic packet frame of 2/3rd byte\n");
		return err;
	}

	err = phy_write_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_MAGIC_PKT_PATTREN_32_2_47,
			    phydev->attached_dev->dev_addr[MAC_ADDRESS_BYTE_4] |
			    (phydev->attached_dev->dev_addr[MAC_ADDRESS_BYTE_5] << BIT_SHIFT_8));
	if (err < 0) {
		phydev_err(phydev, "Error setting magic packet frame of 4/5th byte\n");
		return err;
	}

	return aqr107_set_downshift(phydev, MDIO_AN_VEND_PROV_DOWNSHIFT_DFLT);
}

static int aqcs109_config_init(struct phy_device *phydev)
{
	int ret;

	/* Check that the PHY interface type is compatible */
	if (phydev->interface != PHY_INTERFACE_MODE_SGMII &&
	    phydev->interface != PHY_INTERFACE_MODE_2500BASEX)
		return -ENODEV;

	ret = aqr107_wait_reset_complete(phydev);
	if (!ret)
		aqr107_chip_info(phydev);

	/* AQCS109 belongs to a chip family partially supporting 10G and 5G.
	 * PMA speed ability bits are the same for all members of the family,
	 * AQCS109 however supports speeds up to 2.5G only.
	 */
	ret = phy_set_max_speed(phydev, SPEED_2500);
	if (ret)
		return ret;

	return aqr107_set_downshift(phydev, MDIO_AN_VEND_PROV_DOWNSHIFT_DFLT);
}

static void aqr107_link_change_notify(struct phy_device *phydev)
{
	u8 fw_major, fw_minor;
	bool downshift, short_reach, afr;
	int mode, val;

	if (phydev->state != PHY_RUNNING || phydev->autoneg == AUTONEG_DISABLE)
		return;

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RX_LP_STAT1);
	/* call failed or link partner is no Aquantia PHY */
	if (val < 0 || !(val & MDIO_AN_RX_LP_STAT1_AQ_PHY))
		return;

	short_reach = val & MDIO_AN_RX_LP_STAT1_SHORT_REACH;
	downshift = val & MDIO_AN_RX_LP_STAT1_AQRATE_DOWNSHIFT;

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RX_LP_STAT4);
	if (val < 0)
		return;

	fw_major = FIELD_GET(MDIO_AN_RX_LP_STAT4_FW_MAJOR, val);
	fw_minor = FIELD_GET(MDIO_AN_RX_LP_STAT4_FW_MINOR, val);

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RX_VEND_STAT3);
	if (val < 0)
		return;

	afr = val & MDIO_AN_RX_VEND_STAT3_AFR;

	phydev_dbg(phydev, "Link partner is Aquantia PHY, FW %u.%u%s%s%s\n",
		   fw_major, fw_minor,
		   short_reach ? ", short reach mode" : "",
		   downshift ? ", fast-retrain downshift advertised" : "",
		   afr ? ", fast reframe advertised" : "");

	val = phy_read_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_RSVD_STAT9);
	if (val < 0)
		return;

	mode = FIELD_GET(VEND1_GLOBAL_RSVD_STAT9_MODE, val);
	if (mode == VEND1_GLOBAL_RSVD_STAT9_1000BT2)
		phydev_info(phydev, "Aquantia 1000Base-T2 mode active\n");
}

static int aqr107_suspend(struct phy_device *phydev)
{
	return phy_set_bits_mmd(phydev, MDIO_MMD_VEND1, MDIO_CTRL1,
				MDIO_CTRL1_LPOWER);
}

static int aqr107_resume(struct phy_device *phydev)
{
	return phy_clear_bits_mmd(phydev, MDIO_MMD_VEND1, MDIO_CTRL1,
				  MDIO_CTRL1_LPOWER);
}

static int aqr107_probe(struct phy_device *phydev)
{
	phydev->priv = devm_kzalloc(&phydev->mdio.dev,
				    sizeof(struct aqr107_priv), GFP_KERNEL);
	if (!phydev->priv)
		return -ENOMEM;

	return aqr_hwmon_probe(phydev);
}

static int aqr113c_wol_settings(struct phy_device *phydev, bool enable)
{
	u16 val;
	int ret = 0;
	struct aqr107_priv *priv = phydev->priv;

	if (enable) {
		/* Disables all advertised speeds except for the WoL
		 * speed (100BASE-TX FD or 1000BASE-T)
		 * This is set as per the APP note from Marvel
		 */
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL);
		val |= MDIO_AN_LD_LOOP_TIMING_ABILITY;
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL, val);
		if (ret < 0)
			return ret;
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_VEND_PROV);
		val = (val & MDIO_AN_VEND_MASK) |
		      (MDIO_AN_VEND_PROV_AQRATE_DWN_SHFT_CAP | MDIO_AN_VEND_PROV_1000BASET_FULL);
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_VEND_PROV, val);
		if (ret < 0)
			return ret;

		/* Enable the magic frame and wake up frame detection for the PHY */
		val = phy_read_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_GBE_PHY_RSI1_CTRL6);
		val |= MDIO_C22EXT_RSI_WAKE_UP_FRAME_DETECTION;
		ret = phy_write_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_GBE_PHY_RSI1_CTRL6, val);
		if (ret < 0)
			return ret;
		val = phy_read_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_GBE_PHY_RSI1_CTRL7);
		val |= MDIO_C22EXT_RSI_MAGIC_PKT_FRAME_DETECTION;
		ret = phy_write_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_GBE_PHY_RSI1_CTRL7, val);
		if (ret < 0)
			return ret;

		/* Set the WoL enable bit */
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RSVD_VEND_PROV1);
		val |= MDIO_MMD_AN_WOL_ENABLE;
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RSVD_VEND_PROV1, val);
		if (ret < 0)
			return ret;

		/* Set the WoL INT_N trigger bit */
		val = phy_read_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_GBE_PHY_RSI1_CTRL8);
		val |= MDIO_C22EXT_RSI_WOL_FCS_MONITOR_MODE;
		ret = phy_write_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_GBE_PHY_RSI1_CTRL8, val);
		if (ret < 0)
			return ret;

		/* Enable Interrupt INT_N Generation at pin level */
		val = phy_read_mmd(phydev, MDIO_MMD_C22EXT, MDIO_C22EXT_GBE_PHY_SGMII_TX_INT_MASK1);
		val |= MDIO_C22EXT_SGMII0_WAKE_UP_FRAME_MASK |
		       MDIO_C22EXT_SGMII0_MAGIC_PKT_FRAME_MASK;
		ret = phy_write_mmd(phydev, MDIO_MMD_C22EXT,
				    MDIO_C22EXT_GBE_PHY_SGMII_TX_INT_MASK1, val);
		if (ret < 0)
			return ret;
		val = phy_read_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_INT_STD_MASK);
		val |= VEND1_GLOBAL_INT_STD_MASK_ALL;
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_INT_STD_MASK, val);
		if (ret < 0)
			return ret;
		val = phy_read_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_INT_VEND_MASK);
		val |= VEND1_GLOBAL_INT_VEND_MASK_GBE;
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1, VEND1_GLOBAL_INT_VEND_MASK, val);
		if (ret < 0)
			return ret;

		/* Set the system interface to SGMII */
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				    VEND1_GLOBAL_SYS_CONFIG_100M, VEND1_GLOBAL_SYS_CONFIG_SGMII);
		if (ret < 0)
			return ret;
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				    VEND1_GLOBAL_SYS_CONFIG_1G, VEND1_GLOBAL_SYS_CONFIG_SGMII);
		if (ret < 0)
			return ret;

		/* restart auto-negotiation */
		genphy_c45_restart_aneg(phydev);
		priv->wol_status = 1;
	} else {
		/* Disable the WoL enable bit */
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RSVD_VEND_PROV1);
		val &= ~MDIO_MMD_AN_WOL_ENABLE;
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RSVD_VEND_PROV1, val);
		if (ret < 0)
			return ret;

		/* Restore the SERDES/System Interface back to the XFI mode */
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				    VEND1_GLOBAL_SYS_CONFIG_100M, VEND1_GLOBAL_SYS_CONFIG_XFI);
		if (ret < 0)
			return ret;
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND1,
				    VEND1_GLOBAL_SYS_CONFIG_1G, VEND1_GLOBAL_SYS_CONFIG_XFI);
		if (ret < 0)
			return ret;

		/* restart auto-negotiation */
		genphy_c45_restart_aneg(phydev);
		priv->wol_status = 0;
	}
	return ret;
}

static void aqr113c_get_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	u16 val;

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_RSVD_VEND_STATUS3);
	if (val < 0)
		return;

	wol->supported = WAKE_MAGIC;
	if (val & 0x1) {
		wol->wolopts = WAKE_MAGIC;
	}
}

static int aqr113c_set_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	if (wol->wolopts & WAKE_MAGIC)
		return aqr113c_wol_settings(phydev, true);
	else
		return aqr113c_wol_settings(phydev, false);
	return 0;
}

static struct phy_driver aqr_driver[] = {
{
	PHY_ID_MATCH_MODEL(PHY_ID_AQ1202),
	.name		= "Aquantia AQ1202",
	.config_aneg    = aqr_config_aneg,
	.config_intr	= aqr_config_intr,
	.ack_interrupt	= aqr_ack_interrupt,
	.read_status	= aqr_read_status,
},
{
	PHY_ID_MATCH_MODEL(PHY_ID_AQ2104),
	.name		= "Aquantia AQ2104",
	.config_aneg    = aqr_config_aneg,
	.config_intr	= aqr_config_intr,
	.ack_interrupt	= aqr_ack_interrupt,
	.read_status	= aqr_read_status,
},
{
	PHY_ID_MATCH_MODEL(PHY_ID_AQR105),
	.name		= "Aquantia AQR105",
	.config_aneg    = aqr_config_aneg,
	.config_intr	= aqr_config_intr,
	.ack_interrupt	= aqr_ack_interrupt,
	.read_status	= aqr_read_status,
	.suspend	= aqr107_suspend,
	.resume		= aqr107_resume,
},
{
	PHY_ID_MATCH_MODEL(PHY_ID_AQR106),
	.name		= "Aquantia AQR106",
	.config_aneg    = aqr_config_aneg,
	.config_intr	= aqr_config_intr,
	.ack_interrupt	= aqr_ack_interrupt,
	.read_status	= aqr_read_status,
},
{
	PHY_ID_MATCH_MODEL(PHY_ID_AQR107),
	.name		= "Aquantia AQR107",
	.probe		= aqr107_probe,
	.config_init	= aqr107_config_init,
	.config_aneg    = aqr_config_aneg,
	.config_intr	= aqr_config_intr,
	.ack_interrupt	= aqr_ack_interrupt,
	.read_status	= aqr107_read_status,
	.get_tunable    = aqr107_get_tunable,
	.set_tunable    = aqr107_set_tunable,
	.suspend	= aqr107_suspend,
	.resume		= aqr107_resume,
	.get_sset_count	= aqr107_get_sset_count,
	.get_strings	= aqr107_get_strings,
	.get_stats	= aqr107_get_stats,
	.link_change_notify = aqr107_link_change_notify,
},
{
	PHY_ID_MATCH_MODEL(PHY_ID_AQCS109),
	.name		= "Aquantia AQCS109",
	.probe		= aqr107_probe,
	.config_init	= aqcs109_config_init,
	.config_aneg    = aqr_config_aneg,
	.config_intr	= aqr_config_intr,
	.ack_interrupt	= aqr_ack_interrupt,
	.read_status	= aqr107_read_status,
	.get_tunable    = aqr107_get_tunable,
	.set_tunable    = aqr107_set_tunable,
	.suspend	= aqr107_suspend,
	.resume		= aqr107_resume,
	.get_sset_count	= aqr107_get_sset_count,
	.get_strings	= aqr107_get_strings,
	.get_stats	= aqr107_get_stats,
	.link_change_notify = aqr107_link_change_notify,
},
{
	PHY_ID_MATCH_MODEL(PHY_ID_AQR113C),
	.name		= "Aquantia AQR113C",
	.probe		= aqr107_probe,
	.config_init	= aqr107_config_init,
	.config_aneg    = aqr_config_aneg,
	.config_intr	= aqr_config_intr,
	.ack_interrupt	= aqr_ack_interrupt,
	.read_status	= aqr107_read_status,
	.get_tunable    = aqr107_get_tunable,
	.set_tunable    = aqr107_set_tunable,
	.suspend	= aqr107_suspend,
	.resume		= aqr107_resume,
	.get_sset_count	= aqr107_get_sset_count,
	.get_strings	= aqr107_get_strings,
	.get_stats	= aqr107_get_stats,
	.link_change_notify = aqr107_link_change_notify,
	.get_wol	= &aqr113c_get_wol,
	.set_wol	= &aqr113c_set_wol,
},
{
	PHY_ID_MATCH_MODEL(PHY_ID_AQR405),
	.name		= "Aquantia AQR405",
	.config_aneg    = aqr_config_aneg,
	.config_intr	= aqr_config_intr,
	.ack_interrupt	= aqr_ack_interrupt,
	.read_status	= aqr_read_status,
},
};

module_phy_driver(aqr_driver);

static struct mdio_device_id __maybe_unused aqr_tbl[] = {
	{ PHY_ID_MATCH_MODEL(PHY_ID_AQ1202) },
	{ PHY_ID_MATCH_MODEL(PHY_ID_AQ2104) },
	{ PHY_ID_MATCH_MODEL(PHY_ID_AQR105) },
	{ PHY_ID_MATCH_MODEL(PHY_ID_AQR106) },
	{ PHY_ID_MATCH_MODEL(PHY_ID_AQR107) },
	{ PHY_ID_MATCH_MODEL(PHY_ID_AQCS109) },
	{ PHY_ID_MATCH_MODEL(PHY_ID_AQR405) },
	{ PHY_ID_MATCH_MODEL(PHY_ID_AQR113C) },
	{ }
};

MODULE_DEVICE_TABLE(mdio, aqr_tbl);

MODULE_DESCRIPTION("Aquantia PHY driver");
MODULE_AUTHOR("Shaohui Xie <Shaohui.Xie@freescale.com>");
MODULE_LICENSE("GPL v2");
