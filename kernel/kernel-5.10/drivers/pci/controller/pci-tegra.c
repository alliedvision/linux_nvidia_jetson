// SPDX-License-Identifier: GPL-2.0+
/*
 * PCIe host controller driver for Tegra SoCs
 *
 * Copyright (c) 2010, CompuLab, Ltd.
 * Author: Mike Rapoport <mike@compulab.co.il>
 *
 * Based on NVIDIA PCIe driver
 * Copyright (c) 2008-2020, NVIDIA Corporation.
 *
 * Bits taken from arch/arm/mach-dove/pcie.c
 *
 * Author: Thierry Reding <treding@nvidia.com>
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
#include <linux/platform/tegra/emc_bwmgr.h>
#endif
#include <linux/reset.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/regulator/consumer.h>

#include <soc/tegra/cpuidle.h>
#include <soc/tegra/pmc.h>

#include "../pci.h"

#define INT_PCI_MSI_NR (8 * 32)

#define PCI_CFG_SPACE_SIZE	256
#define PCI_EXT_CFG_SPACE_SIZE	4096

/* register definitions */

#define AFI_AXI_BAR0_SZ	0x00
#define AFI_AXI_BAR1_SZ	0x04
#define AFI_AXI_BAR2_SZ	0x08
#define AFI_AXI_BAR3_SZ	0x0c
#define AFI_AXI_BAR4_SZ	0x10
#define AFI_AXI_BAR5_SZ	0x14

#define AFI_AXI_BAR0_START	0x18
#define AFI_AXI_BAR1_START	0x1c
#define AFI_AXI_BAR2_START	0x20
#define AFI_AXI_BAR3_START	0x24
#define AFI_AXI_BAR4_START	0x28
#define AFI_AXI_BAR5_START	0x2c

#define AFI_FPCI_BAR0	0x30
#define AFI_FPCI_BAR1	0x34
#define AFI_FPCI_BAR2	0x38
#define AFI_FPCI_BAR3	0x3c
#define AFI_FPCI_BAR4	0x40
#define AFI_FPCI_BAR5	0x44

#define AFI_CACHE_BAR0_SZ	0x48
#define AFI_CACHE_BAR0_ST	0x4c
#define AFI_CACHE_BAR1_SZ	0x50
#define AFI_CACHE_BAR1_ST	0x54

#define AFI_MSI_BAR_SZ		0x60
#define AFI_MSI_FPCI_BAR_ST	0x64
#define AFI_MSI_AXI_BAR_ST	0x68

#define AFI_MSI_VEC0		0x6c
#define AFI_MSI_VEC1		0x70
#define AFI_MSI_VEC2		0x74
#define AFI_MSI_VEC3		0x78
#define AFI_MSI_VEC4		0x7c
#define AFI_MSI_VEC5		0x80
#define AFI_MSI_VEC6		0x84
#define AFI_MSI_VEC7		0x88

#define AFI_MSI_EN_VEC0		0x8c
#define AFI_MSI_EN_VEC1		0x90
#define AFI_MSI_EN_VEC2		0x94
#define AFI_MSI_EN_VEC3		0x98
#define AFI_MSI_EN_VEC4		0x9c
#define AFI_MSI_EN_VEC5		0xa0
#define AFI_MSI_EN_VEC6		0xa4
#define AFI_MSI_EN_VEC7		0xa8

#define AFI_CONFIGURATION		0xac
#define  AFI_CONFIGURATION_EN_FPCI		(1 << 0)
#define  AFI_CONFIGURATION_CLKEN_OVERRIDE	(1 << 31)

#define AFI_FPCI_ERROR_MASKS	0xb0

#define AFI_INTR_MASK		0xb4
#define  AFI_INTR_MASK_INT_MASK	(1 << 0)
#define  AFI_INTR_MASK_MSI_MASK	(1 << 8)

#define AFI_INTR_CODE			0xb8
#define  AFI_INTR_CODE_MASK		0xf
#define  AFI_INTR_INI_SLAVE_ERROR	1
#define  AFI_INTR_INI_DECODE_ERROR	2
#define  AFI_INTR_TARGET_ABORT		3
#define  AFI_INTR_MASTER_ABORT		4
#define  AFI_INTR_INVALID_WRITE		5
#define  AFI_INTR_LEGACY		6
#define  AFI_INTR_FPCI_DECODE_ERROR	7
#define  AFI_INTR_AXI_DECODE_ERROR	8
#define  AFI_INTR_FPCI_TIMEOUT		9
#define  AFI_INTR_PE_PRSNT_SENSE	10
#define  AFI_INTR_PE_CLKREQ_SENSE	11
#define  AFI_INTR_CLKCLAMP_SENSE	12
#define  AFI_INTR_RDY4PD_SENSE		13
#define  AFI_INTR_P2P_ERROR		14

#define AFI_INTR_SIGNATURE	0xbc
#define AFI_UPPER_FPCI_ADDRESS	0xc0
#define AFI_SM_INTR_ENABLE	0xc4
#define  AFI_SM_INTR_INTA_ASSERT	(1 << 0)
#define  AFI_SM_INTR_INTB_ASSERT	(1 << 1)
#define  AFI_SM_INTR_INTC_ASSERT	(1 << 2)
#define  AFI_SM_INTR_INTD_ASSERT	(1 << 3)
#define  AFI_SM_INTR_INTA_DEASSERT	(1 << 4)
#define  AFI_SM_INTR_INTB_DEASSERT	(1 << 5)
#define  AFI_SM_INTR_INTC_DEASSERT	(1 << 6)
#define  AFI_SM_INTR_INTD_DEASSERT	(1 << 7)

#define AFI_AFI_INTR_ENABLE		0xc8
#define  AFI_INTR_EN_INI_SLVERR		(1 << 0)
#define  AFI_INTR_EN_INI_DECERR		(1 << 1)
#define  AFI_INTR_EN_TGT_SLVERR		(1 << 2)
#define  AFI_INTR_EN_TGT_DECERR		(1 << 3)
#define  AFI_INTR_EN_TGT_WRERR		(1 << 4)
#define  AFI_INTR_EN_DFPCI_DECERR	(1 << 5)
#define  AFI_INTR_EN_AXI_DECERR		(1 << 6)
#define  AFI_INTR_EN_FPCI_TIMEOUT	(1 << 7)
#define  AFI_INTR_EN_PRSNT_SENSE	(1 << 8)

#define AFI_PCIE_PME		0xf0

#define AFI_PCIE_CONFIG					0x0f8
#define  AFI_PCIE_CONFIG_PCIE_DISABLE(x)		(1 << ((x) + 1))
#define  AFI_PCIE_CONFIG_PCIE_DISABLE_ALL		0xe
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK	(0xf << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_SINGLE	(0x0 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_420	(0x0 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X2_X1	(0x0 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_401	(0x0 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_DUAL	(0x1 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_222	(0x1 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X4_X1	(0x1 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_211	(0x1 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_411	(0x2 << 20)
#define  AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_111	(0x2 << 20)
#define  AFI_PCIE_CONFIG_PCIE_CLKREQ_GPIO(x)		(1 << ((x) + 29))
#define  AFI_PCIE_CONFIG_PCIE_CLKREQ_GPIO_ALL		(0x7 << 29)

#define AFI_FUSE			0x104
#define  AFI_FUSE_PCIE_T0_GEN2_DIS	(1 << 2)

#define AFI_PEX0_CTRL			0x110
#define AFI_PEX1_CTRL			0x118
#define  AFI_PEX_CTRL_RST		(1 << 0)
#define  AFI_PEX_CTRL_CLKREQ_EN		(1 << 1)
#define  AFI_PEX_CTRL_REFCLK_EN		(1 << 3)
#define  AFI_PEX_CTRL_OVERRIDE_EN	(1 << 4)

#define AFI_PLLE_CONTROL		0x160
#define  AFI_PLLE_CONTROL_BYPASS_PADS2PLLE_CONTROL (1 << 9)
#define  AFI_PLLE_CONTROL_BYPASS_PCIE2PLLE_CONTROL (1 << 8)
#define  AFI_PLLE_CONTROL_PADS2PLLE_CONTROL_EN (1 << 1)
#define  AFI_PLLE_CONTROL_PCIE2PLLE_CONTROL_EN (1 << 0)

#define AFI_PEXBIAS_CTRL_0		0x168

#define RP_INTR_BCR	0x3c
#define  RP_INTR_BCR_INTR_LINE		(0xff << 0)
#define  RP_INTR_BCR_SB_RESET		(0x1 << 22)

#define RP_L1_PM_SUBSTATES_CTL		0xc00
#define  RP_L1_PM_SUBSTATES_CTL_PCI_PM_L1_2		(0x1 << 0)
#define  RP_L1_PM_SUBSTATES_CTL_PCI_PM_L1_1		(0x1 << 1)
#define  RP_L1_PM_SUBSTATES_CTL_ASPM_L1_2		(0x1 << 2)
#define  RP_L1_PM_SUBSTATES_CTL_ASPM_L1_1		(0x1 << 3)
#define  RP_L1_PM_SUBSTATES_CTL_CM_RTIME_MASK		(0xff << 8)
#define  RP_L1_PM_SUBSTATES_CTL_CM_RTIME_SHIFT		8
#define  RP_L1_PM_SUBSTATES_CTL_T_PWRN_SCL_MASK		(0x3 << 16)
#define  RP_L1_PM_SUBSTATES_CTL_T_PWRN_SCL_SHIFT	16
#define  RP_L1_PM_SUBSTATES_CTL_T_PWRN_VAL_MASK		(0x1f << 19)
#define  RP_L1_PM_SUBSTATES_CTL_T_PWRN_VAL_SHIFT	19
#define  RP_L1_PM_SUBSTATES_CTL_HIDE_CAP		(0x1 << 24)

#define RP_L1_PM_SUBSTATES_1_CTL	0xc04
#define  RP_L1_PM_SUBSTATES_1_CTL_PWR_OFF_DLY_MASK	0x1fff
#define  RP_L1_PM_SUBSTATES_1_CTL_PWR_OFF_DLY		0x26
#define  RP_L1SS_1_CTL_CLKREQ_ASSERTED_DLY_MASK		(0x1ff << 13)
#define  RP_L1SS_1_CTL_CLKREQ_ASSERTED_DLY		(0x27 << 13)

#define RP_L1_PM_SUBSTATES_2_CTL	0xc08
#define  RP_L1_PM_SUBSTATES_2_CTL_T_L1_2_DLY_MASK	0x1fff
#define  RP_L1_PM_SUBSTATES_2_CTL_T_L1_2_DLY		0x4d
#define  RP_L1_PM_SUBSTATES_2_CTL_MICROSECOND_MASK	(0xff << 13)
#define  RP_L1_PM_SUBSTATES_2_CTL_MICROSECOND		(0x13 << 13)
#define  RP_L1_PM_SUBSTATES_2_CTL_MICROSECOND_COMP_MASK	(0xf << 21)
#define  RP_L1_PM_SUBSTATES_2_CTL_MICROSECOND_COMP	(0x2 << 21)

#define RP_LTR_REP_VAL			0xc10

#define RP_L1_1_ENTRY_COUNT		0xc14
#define  RP_L1_1_ENTRY_COUNT_RESET	(1 << 31)

#define RP_L1_2_ENTRY_COUNT		0xc18
#define  RP_L1_2_ENTRY_COUNT_RESET	(1 << 31)

#define RP_TIMEOUT0		0xe24
#define  RP_TIMEOUT0_PAD_PWRUP_MASK		0xff
#define  RP_TIMEOUT0_PAD_PWRUP			0xa
#define  RP_TIMEOUT0_PAD_PWRUP_CM_MASK		0xffff00
#define  RP_TIMEOUT0_PAD_PWRUP_CM		(0x180 << 8)
#define  RP_TIMEOUT0_PAD_SPDCHNG_GEN2_MASK	(0xff << 24)
#define  RP_TIMEOUT0_PAD_SPDCHNG_GEN2		(0xa << 24)

#define RP_TIMEOUT1		0xe28
#define  RP_TIMEOUT1_RCVRY_SPD_SUCCESS_EIDLE_MASK	(0xff << 16)
#define  RP_TIMEOUT1_RCVRY_SPD_SUCCESS_EIDLE		(0x10 << 16)
#define  RP_TIMEOUT1_RCVRY_SPD_UNSUCCESS_EIDLE_MASK	(0xff << 24)
#define  RP_TIMEOUT1_RCVRY_SPD_UNSUCCESS_EIDLE		(0x74 << 24)

#define RP_PRBS			0xe34
#define  RP_PRBS_LOCKED			(1 << 16)

#define RP_LANE_PRBS_ERR_COUNT	0xe38

#define RP_LTSSM_DBGREG		0xe44
#define  RP_LTSSM_DBGREG_LINKFSM16	(1 << 16)

#define RP_LTSSM_TRACE_CONTROL	0xe50
#define  LTSSM_TRACE_CONTROL_CLEAR_STORE_EN			(1 << 0)
#define  LTSSM_TRACE_CONTROL_CLEAR_RAM				(1 << 2)
#define  LTSSM_TRACE_CONTROL_TRIG_ON_EVENT			(1 << 3)
#define  LTSSM_TRACE_CONTROL_TRIG_LTSSM_MAJOR_OFFSET		4
#define  LTSSM_TRACE_CONTROL_TRIG_PTX_LTSSM_MINOR_OFFSET	8
#define  LTSSM_TRACE_CONTROL_TRIG_PRX_LTSSM_MAJOR_OFFSET	11

#define RP_LTSSM_TRACE_STATUS	0xe54
#define  LTSSM_TRACE_STATUS_PRX_MINOR(reg)		(((reg) >> 19) & 0x7)
#define  LTSSM_TRACE_STATUS_PTX_MINOR(reg)		(((reg) >> 16) & 0x7)
#define  LTSSM_TRACE_STATUS_MAJOR(reg)			(((reg) >> 12) & 0xf)
#define  LTSSM_TRACE_STATUS_READ_DATA_VALID(reg)	(((reg) >> 11) & 0x1)
#define  LTSSM_TRACE_STATUS_READ_ADDR(reg)		((reg) << 6)
#define  LTSSM_TRACE_STATUS_WRITE_POINTER(reg)		(((reg) >> 1) & 0x1f)
#define  LTSSM_TRACE_STATUS_RAM_FULL(reg)		(reg & 0x1)

#define RP_ECTL_1_R1	0x00000e80
#define  RP_ECTL_1_R1_TX_DRV_AMP_1C_MASK	0x3f

#define RP_ECTL_2_R1	0x00000e84
#define  RP_ECTL_2_R1_RX_CTLE_1C_MASK		0xffff

#define RP_ECTL_4_R1	0x00000e8c
#define  RP_ECTL_4_R1_RX_CDR_CTRL_1C_MASK	(0xffff << 16)
#define  RP_ECTL_4_R1_RX_CDR_CTRL_1C_SHIFT	16

#define RP_ECTL_5_R1	0x00000e90
#define  RP_ECTL_5_R1_RX_EQ_CTRL_L_1C_MASK	0xffffffff

#define RP_ECTL_6_R1	0x00000e94
#define  RP_ECTL_6_R1_RX_EQ_CTRL_H_1C_MASK	0xffffffff

#define RP_ECTL_1_R2	0x00000ea0
#define  RP_ECTL_1_R2_TX_DRV_AMP_1C_MASK	0x3f

#define RP_ECTL_2_R2	0x00000ea4
#define  RP_ECTL_2_R2_RX_CTLE_1C_MASK	0xffff

#define RP_ECTL_4_R2	0x00000eac
#define  RP_ECTL_4_R2_RX_CDR_CTRL_1C_MASK	(0xffff << 16)
#define  RP_ECTL_4_R2_RX_CDR_CTRL_1C_SHIFT	16

#define RP_ECTL_5_R2	0x00000eb0
#define  RP_ECTL_5_R2_RX_EQ_CTRL_L_1C_MASK	0xffffffff

#define RP_ECTL_6_R2	0x00000eb4
#define  RP_ECTL_6_R2_RX_EQ_CTRL_H_1C_MASK	0xffffffff

#define RP_VEND_XP	0x00000f00
#define  RP_VEND_XP_DL_UP			(1 << 30)
#define  RP_VEND_XP_OPPORTUNISTIC_ACK		(1 << 27)
#define  RP_VEND_XP_OPPORTUNISTIC_UPDATEFC	(1 << 28)
#define  RP_VEND_XP_UPDATE_FC_THRESHOLD_MASK	(0xff << 18)
#define  RP_VEND_XP_PRBS_STAT			(0xffff << 2)
#define  RP_VEND_XP_PRBS_EN			(1 << 1)

#define RP_VEND_XP1	0xf04
#define  RP_VEND_XP1_LINK_PVT_CTL_IGNORE_L0S		(1 << 23)
#define  RP_VEND_XP1_LINK_PVT_CTL_L1_ASPM_SUPPORT	(1 << 21)
#define  RP_VEND_XP1_RNCTRL_MAXWIDTH_MASK		(0x3f << 0)
#define  RP_VEND_XP1_RNCTRL_EN				(1 << 7)

#define RP_XP_REF	0xf30
#define  RP_XP_REF_MICROSECOND_LIMIT_MASK	0xff
#define  RP_XP_REF_MICROSECOND_LIMIT		0x14
#define  RP_XP_REF_MICROSECOND_ENABLE		(1 << 8)
#define  RP_XP_REF_CPL_TO_OVERRIDE		(1 << 13)
#define  RP_XP_REF_CPL_TO_CUSTOM_VALUE_MASK	(0x1ffff << 14)
#define  RP_XP_REF_CPL_TO_CUSTOM_VALUE		(0x1770 << 14)

#define RP_VEND_CTL0	0x00000f44
#define  RP_VEND_CTL0_DSK_RST_PULSE_WIDTH_MASK	(0xf << 12)
#define  RP_VEND_CTL0_DSK_RST_PULSE_WIDTH	(0x9 << 12)

#define RP_VEND_CTL1	0x00000f48
#define  RP_VEND_CTL1_ERPT	(1 << 13)

#define RP_VEND_XP_BIST	0x00000f4c
#define  RP_VEND_XP_BIST_GOTO_L1_L2_AFTER_DLLP_DONE	(1 << 28)

#define RP_VEND_CTL2 0x00000fa8
#define  RP_VEND_CTL2_PCA_ENABLE (1 << 7)

#define RP_PRIV_XP_CONFIG	0xfac
#define  RP_PRIV_XP_CONFIG_LOW_PWR_DURATION_MASK	0x3
#define  RP_PRIV_XP_DURATION_IN_LOW_PWR_100NS		0xfb0

#define RP_PRIV_MISC	0x00000fe0
#define  RP_PRIV_MISC_PRSNT_MAP_EP_PRSNT		(0xe << 0)
#define  RP_PRIV_MISC_PRSNT_MAP_EP_ABSNT		(0xf << 0)
#define  RP_PRIV_MISC_CTLR_CLK_CLAMP_THRESHOLD_MASK	(0x7f << 16)
#define  RP_PRIV_MISC_CTLR_CLK_CLAMP_THRESHOLD		(0xf << 16)
#define  RP_PRIV_MISC_CTLR_CLK_CLAMP_ENABLE		(1 << 23)
#define  RP_PRIV_MISC_TMS_CLK_CLAMP_THRESHOLD_MASK	(0x7f << 24)
#define  RP_PRIV_MISC_TMS_CLK_CLAMP_THRESHOLD		(0xf << 24)
#define  RP_PRIV_MISC_TMS_CLK_CLAMP_ENABLE		(1 << 31)

#define RP_XP_CTL_1	0xfec
#define  RP_XP_CTL_1_OLD_IOBIST_EN	(1 << 25)

#define RP_VEND_XP_PAD_PWRDN	0x00000f50
#define  RP_VEND_XP_PAD_PWRDN_L1_EN			(1 << 0)
#define  RP_VEND_XP_PAD_PWRDN_DYNAMIC_EN		(1 << 1)
#define  RP_VEND_XP_PAD_PWRDN_DISABLED_EN		(1 << 2)
#define  RP_VEND_XP_PAD_PWRDN_L1_CLKREQ_EN		(1 << 15)
#define  RP_VEND_XP_PAD_PWRDN_SLEEP_MODE_DYNAMIC_L1PP	(3 << 5)
#define  RP_VEND_XP_PAD_PWRDN_SLEEP_MODE_L1_L1PP	(3 << 3)
#define  RP_VEND_XP_PAD_PWRDN_SLEEP_MODE_L1_CLKREQ_L1PP	(3 << 16)

#define RP_PRIV_XP_RX_L0S_ENTRY_COUNT	0xf8C
#define RP_PRIV_XP_TX_L0S_ENTRY_COUNT	0xf90
#define RP_PRIV_XP_TX_L1_ENTRY_COUNT	0xf94

#define RP_LINK_CONTROL_STATUS			0x00000090
#define  RP_LINK_CONTROL_STATUS_DL_LINK_ACTIVE	0x20000000
#define  RP_LINK_CONTROL_STATUS_LINKSTAT_MASK	0x3fff0000
#define  RP_LINK_CONTROL_STATUS_NEG_LINK_WIDTH	(0x3f << 20)
#define  RP_LINK_CONTROL_STATUS_LINK_SPEED	(0xf << 16)
#define  RP_LINK_CONTROL_STATUS_L1_ENABLED	(1 << 1)
#define  RP_LINK_CONTROL_STATUS_L0s_ENABLED	(1 << 0)

#define RP_LINK_CONTROL_STATUS_2		0x000000b0

#define RP_L1_PM_SUBSTATES_CAP	0x144

#define RP_L1_PM_SS_CONTROL	0x148
#define  RP_L1_PM_SS_CONTROL_ASPM_L11_ENABLE	0x8
#define  RP_L1_PM_SS_CONTROL_ASPM_L12_ENABLE	0x4

#define PADS_CTL_SEL		0x0000009c

#define PADS_CTL		0x000000a0
#define  PADS_CTL_IDDQ_1L	(1 << 0)
#define  PADS_CTL_TX_DATA_EN_1L	(1 << 6)
#define  PADS_CTL_RX_DATA_EN_1L	(1 << 10)

#define PADS_PLL_CTL_TEGRA20			0x000000b8
#define PADS_PLL_CTL_TEGRA30			0x000000b4
#define  PADS_PLL_CTL_RST_B4SM			(1 << 1)
#define  PADS_PLL_CTL_LOCKDET			(1 << 8)
#define  PADS_PLL_CTL_REFCLK_MASK		(0x3 << 16)
#define  PADS_PLL_CTL_REFCLK_INTERNAL_CML	(0 << 16)
#define  PADS_PLL_CTL_REFCLK_INTERNAL_CMOS	(1 << 16)
#define  PADS_PLL_CTL_REFCLK_EXTERNAL		(2 << 16)
#define  PADS_PLL_CTL_TXCLKREF_MASK		(0x1 << 20)
#define  PADS_PLL_CTL_TXCLKREF_DIV10		(0 << 20)
#define  PADS_PLL_CTL_TXCLKREF_DIV5		(1 << 20)
#define  PADS_PLL_CTL_TXCLKREF_BUF_EN		(1 << 22)

#define PADS_REFCLK_CFG0			0x000000c8
#define PADS_REFCLK_CFG1			0x000000cc
#define PADS_REFCLK_BIAS			0x000000d0

/*
 * Fields in PADS_REFCLK_CFG*. Those registers form an array of 16-bit
 * entries, one entry per PCIe port. These field definitions and desired
 * values aren't in the TRM, but do come from NVIDIA.
 */
#define PADS_REFCLK_CFG_TERM_SHIFT		2  /* 6:2 */
#define PADS_REFCLK_CFG_E_TERM_SHIFT		7
#define PADS_REFCLK_CFG_PREDI_SHIFT		8  /* 11:8 */
#define PADS_REFCLK_CFG_DRVI_SHIFT		12 /* 15:12 */

#define PME_ACK_TIMEOUT 10000
#define LINK_RETRAIN_TIMEOUT 100000 /* in usec */

struct tegra_msi {
	struct msi_controller chip;
	DECLARE_BITMAP(used, INT_PCI_MSI_NR);
	struct irq_domain *domain;
	struct mutex lock;
	void *virt;
	dma_addr_t phys;
	int irq;
};

/* used to differentiate between Tegra SoC generations */
struct tegra_pcie_port_soc {
	struct {
		u8 turnoff_bit;
		u8 ack_bit;
	} pme;
};

struct pcie_dvfs {
	u32 afi_clk;
	u32 emc_clk;
};

struct tegra_pcie_soc {
	unsigned int num_ports;
	const struct tegra_pcie_port_soc *ports;
	unsigned int msi_base_shift;
	unsigned long afi_pex2_ctrl;
	u32 pads_pll_ctl;
	u32 tx_ref_sel;
	u32 pads_refclk_cfg0;
	u32 pads_refclk_cfg1;
	u32 update_fc_threshold;
	bool has_pex_clkreq_en;
	bool has_pex_bias_ctrl;
	bool has_intr_prsnt_sense;
	bool has_cml_clk;
	bool has_gen2;
	bool force_pca_enable;
	bool program_uphy;
	bool update_clamp_threshold;
	bool program_deskew_time;
	bool update_fc_timer;
	bool has_cache_bars;
	bool enable_wrap;
	bool has_aspm_l1;
	bool has_aspm_l1ss;
	bool l1ss_rp_wake_fixup;
	bool dvfs_mselect;
	bool dvfs_afi;
	struct pcie_dvfs dfs_tbl[10][2];
	struct {
		struct {
			u32 rp_ectl_1_r1;
			u32 rp_ectl_2_r1;
			u32 rp_ectl_4_r1;
			u32 rp_ectl_5_r1;
			u32 rp_ectl_6_r1;
			u32 rp_ectl_1_r2;
			u32 rp_ectl_2_r2;
			u32 rp_ectl_4_r2;
			u32 rp_ectl_5_r2;
			u32 rp_ectl_6_r2;
		} regs;
		bool enable;
	} ectl;
};

static inline struct tegra_msi *to_tegra_msi(struct msi_controller *chip)
{
	return container_of(chip, struct tegra_msi, chip);
}

struct tegra_pcie {
	struct device *dev;

	void __iomem *pads;
	void __iomem *afi;
	void __iomem *cfg;
	int irq;

	struct resource cs;

	struct clk *pex_clk;
	struct clk *afi_clk;
	struct clk *pll_e;
	struct clk *cml_clk;

	struct reset_control *pex_rst;
	struct reset_control *afi_rst;
	struct reset_control *pcie_xrst;

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	struct tegra_bwmgr_client *emc_bwmgr;
#endif

	bool legacy_phy;
	struct phy *phy;

	struct tegra_msi msi;

	struct list_head ports;
	u32 xbar_config;

	struct regulator_bulk_data *supplies;
	unsigned int num_supplies;

	int pex_wake;

	const struct tegra_pcie_soc *soc;
	struct dentry *debugfs;
};

struct tegra_pcie_port {
	struct tegra_pcie *pcie;
	struct device_node *np;
	struct list_head list;
	struct resource regs;
	void __iomem *base;
	unsigned int index;
	unsigned int lanes;
	unsigned int loopback_stat;
	unsigned int aspm_state;
	bool supports_clkreq;

	int n_gpios;
	int *gpios;
	bool has_mxm_port;
	int pwr_gd_gpio;

	struct phy **phys;

	struct gpio_desc *reset_gpio;
	struct dentry *port_debugfs;
};

struct tegra_pcie_bus {
	struct list_head list;
	unsigned int nr;
};

static bool is_gen2_speed;
static u16 bdf;
static u16 config_offset;
static u32 config_val;
static u16 config_aspm_state;

static inline void afi_writel(struct tegra_pcie *pcie, u32 value,
			      unsigned long offset)
{
	writel(value, pcie->afi + offset);
}

static inline u32 afi_readl(struct tegra_pcie *pcie, unsigned long offset)
{
	return readl(pcie->afi + offset);
}

static inline void pads_writel(struct tegra_pcie *pcie, u32 value,
			       unsigned long offset)
{
	writel(value, pcie->pads + offset);
}

static inline u32 pads_readl(struct tegra_pcie *pcie, unsigned long offset)
{
	return readl(pcie->pads + offset);
}

static bool tegra_pcie_link_up(struct tegra_pcie_port *port)
{
	u32 value;

	value = readl(port->base + RP_LINK_CONTROL_STATUS);
	return !!(value & RP_LINK_CONTROL_STATUS_DL_LINK_ACTIVE);
}

/*
 * The configuration space mapping on Tegra is somewhat similar to the ECAM
 * defined by PCIe. However it deviates a bit in how the 4 bits for extended
 * register accesses are mapped:
 *
 *    [27:24] extended register number
 *    [23:16] bus number
 *    [15:11] device number
 *    [10: 8] function number
 *    [ 7: 0] register number
 *
 * Mapping the whole extended configuration space would require 256 MiB of
 * virtual address space, only a small part of which will actually be used.
 *
 * To work around this, a 4 KiB region is used to generate the required
 * configuration transaction with relevant B:D:F and register offset values.
 * This is achieved by dynamically programming base address and size of
 * AFI_AXI_BAR used for end point config space mapping to make sure that the
 * address (access to which generates correct config transaction) falls in
 * this 4 KiB region.
 */
static unsigned int tegra_pcie_conf_offset(u8 bus, unsigned int devfn,
					   unsigned int where)
{
	return ((where & 0xf00) << 16) | (bus << 16) | (PCI_SLOT(devfn) << 11) |
	       (PCI_FUNC(devfn) << 8) | (where & 0xff);
}

static void __iomem *tegra_pcie_map_bus(struct pci_bus *bus,
					unsigned int devfn,
					int where)
{
	struct tegra_pcie *pcie = bus->sysdata;
	void __iomem *addr = NULL;

	if (bus->number == 0) {
		unsigned int slot = PCI_SLOT(devfn);
		struct tegra_pcie_port *port;

		list_for_each_entry(port, &pcie->ports, list) {
			if (port->index + 1 == slot) {
				addr = port->base + (where & ~3);
				break;
			}
		}
	} else {
		unsigned int offset;
		u32 base;

		offset = tegra_pcie_conf_offset(bus->number, devfn, where);

		/* move 4 KiB window to offset within the FPCI region */
		base = 0xfe100000 + ((offset & ~(SZ_4K - 1)) >> 8);
		afi_writel(pcie, base, AFI_FPCI_BAR0);

		/* move to correct offset within the 4 KiB page */
		addr = pcie->cfg + (offset & (SZ_4K - 1));
	}

	return addr;
}

static int tegra_pcie_config_read(struct pci_bus *bus, unsigned int devfn,
				  int where, int size, u32 *value)
{
	struct tegra_pcie *pcie = bus->sysdata;
	struct pci_dev *bridge;
	struct tegra_pcie_port *port;

	if (bus->number == 0)
		return pci_generic_config_read32(bus, devfn, where, size,
						 value);

	bridge = pcie_find_root_port(bus->self);

	list_for_each_entry(port, &pcie->ports, list)
		if (port->index + 1 == PCI_SLOT(bridge->devfn))
			break;

	/* If there is no link, then there is no device */
	if (!tegra_pcie_link_up(port)) {
		*value = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	return pci_generic_config_read(bus, devfn, where, size, value);
}

static int tegra_pcie_config_write(struct pci_bus *bus, unsigned int devfn,
				   int where, int size, u32 value)
{
	struct tegra_pcie *pcie = bus->sysdata;
	struct tegra_pcie_port *port;
	struct pci_dev *bridge;

	if (bus->number == 0)
		return pci_generic_config_write32(bus, devfn, where, size,
						  value);

	bridge = pcie_find_root_port(bus->self);

	list_for_each_entry(port, &pcie->ports, list)
		if (port->index + 1 == PCI_SLOT(bridge->devfn))
			break;

	/* If there is no link, then there is no device */
	if (!tegra_pcie_link_up(port))
		return PCIBIOS_DEVICE_NOT_FOUND;

	return pci_generic_config_write(bus, devfn, where, size, value);
}

static struct pci_ops tegra_pcie_ops = {
	.map_bus = tegra_pcie_map_bus,
	.read = tegra_pcie_config_read,
	.write = tegra_pcie_config_write,
};

static unsigned long tegra_pcie_port_get_pex_ctrl(struct tegra_pcie_port *port)
{
	const struct tegra_pcie_soc *soc = port->pcie->soc;
	unsigned long ret = 0;

	switch (port->index) {
	case 0:
		ret = AFI_PEX0_CTRL;
		break;

	case 1:
		ret = AFI_PEX1_CTRL;
		break;

	case 2:
		ret = soc->afi_pex2_ctrl;
		break;
	}

	return ret;
}

static void tegra_pcie_port_reset(struct tegra_pcie_port *port)
{
	unsigned long ctrl = tegra_pcie_port_get_pex_ctrl(port);
	unsigned long value;

	/* pulse reset signal */
	if (port->reset_gpio) {
		gpiod_set_value(port->reset_gpio, 1);
	} else {
		value = afi_readl(port->pcie, ctrl);
		value &= ~AFI_PEX_CTRL_RST;
		afi_writel(port->pcie, value, ctrl);
	}

	usleep_range(1000, 2000);

	if (port->reset_gpio) {
		gpiod_set_value(port->reset_gpio, 0);
	} else {
		value = afi_readl(port->pcie, ctrl);
		value |= AFI_PEX_CTRL_RST;
		afi_writel(port->pcie, value, ctrl);
	}
}

static void disable_aspm_l0s(struct tegra_pcie_port *port)
{
	u32 val = 0;

	val = readl(port->base + RP_VEND_XP1);
	val |= RP_VEND_XP1_LINK_PVT_CTL_IGNORE_L0S;
	writel(val, port->base + RP_VEND_XP1);
}

static void disable_aspm_l10(struct tegra_pcie_port *port)
{
	u32 val = 0;

	val = readl(port->base + RP_VEND_XP1);
	val &= ~RP_VEND_XP1_LINK_PVT_CTL_L1_ASPM_SUPPORT;
	writel(val, port->base + RP_VEND_XP1);
}

static void disable_aspm_l11(struct tegra_pcie_port *port)
{
	u32 val = 0;

	val = readl(port->base + RP_L1_PM_SUBSTATES_CTL);
	val &= ~RP_L1_PM_SUBSTATES_CTL_PCI_PM_L1_1;
	val &= ~RP_L1_PM_SUBSTATES_CTL_ASPM_L1_1;
	writel(val, port->base + RP_L1_PM_SUBSTATES_CTL);
}

static void disable_aspm_l12(struct tegra_pcie_port *port)
{
	u32 val = 0;

	val = readl(port->base + RP_L1_PM_SUBSTATES_CTL);
	val &= ~RP_L1_PM_SUBSTATES_CTL_PCI_PM_L1_2;
	val &= ~RP_L1_PM_SUBSTATES_CTL_ASPM_L1_2;
	writel(val, port->base + RP_L1_PM_SUBSTATES_CTL);
}

static void tegra_pcie_enable_rp_features(struct tegra_pcie_port *port)
{
	const struct tegra_pcie_soc *soc = port->pcie->soc;
	u32 value;

	/* Enable AER capability */
	value = readl(port->base + RP_VEND_CTL1);
	value |= RP_VEND_CTL1_ERPT;
	writel(value, port->base + RP_VEND_CTL1);

	/* Optimal settings to enhance bandwidth */
	value = readl(port->base + RP_VEND_XP);
	value |= RP_VEND_XP_OPPORTUNISTIC_ACK;
	value |= RP_VEND_XP_OPPORTUNISTIC_UPDATEFC;
	writel(value, port->base + RP_VEND_XP);

	/*
	 * LTSSM will wait for DLLP to finish before entering L1 or L2,
	 * to avoid truncation of PM messages which results in receiver errors
	 */
	value = readl(port->base + RP_VEND_XP_BIST);
	value |= RP_VEND_XP_BIST_GOTO_L1_L2_AFTER_DLLP_DONE;
	writel(value, port->base + RP_VEND_XP_BIST);

	value = readl(port->base + RP_PRIV_MISC);
	value |= RP_PRIV_MISC_CTLR_CLK_CLAMP_ENABLE;
	value |= RP_PRIV_MISC_TMS_CLK_CLAMP_ENABLE;

	if (soc->update_clamp_threshold) {
		value &= ~(RP_PRIV_MISC_CTLR_CLK_CLAMP_THRESHOLD_MASK |
				RP_PRIV_MISC_TMS_CLK_CLAMP_THRESHOLD_MASK);
		value |= RP_PRIV_MISC_CTLR_CLK_CLAMP_THRESHOLD |
			RP_PRIV_MISC_TMS_CLK_CLAMP_THRESHOLD;
	}

	writel(value, port->base + RP_PRIV_MISC);

	if (soc->has_aspm_l1) {
		/* Advertise ASPM-L1 state capability*/
		value = readl(port->base + RP_VEND_XP1);
		value |= RP_VEND_XP1_LINK_PVT_CTL_L1_ASPM_SUPPORT;
		writel(value, port->base + RP_VEND_XP1);

		/* Power saving configuration for L1 sleep/idle */
		value = readl(port->base + RP_VEND_XP_PAD_PWRDN);
		value |= RP_VEND_XP_PAD_PWRDN_DISABLED_EN;
		value |= RP_VEND_XP_PAD_PWRDN_DYNAMIC_EN;
		value |= RP_VEND_XP_PAD_PWRDN_L1_EN;
		value |= RP_VEND_XP_PAD_PWRDN_L1_CLKREQ_EN;
		value |= RP_VEND_XP_PAD_PWRDN_SLEEP_MODE_DYNAMIC_L1PP;
		value |= RP_VEND_XP_PAD_PWRDN_SLEEP_MODE_L1_L1PP;
		value |= RP_VEND_XP_PAD_PWRDN_SLEEP_MODE_L1_CLKREQ_L1PP;
		writel(value, port->base + RP_VEND_XP_PAD_PWRDN);

		if (port->aspm_state & 0x1)
			disable_aspm_l0s(port);
		if (port->aspm_state & 0x2)
			disable_aspm_l10(port);
	}

	if (soc->has_aspm_l1ss) {
		if (port->aspm_state & 0x2) {
			disable_aspm_l11(port);
			disable_aspm_l12(port);
		}
		if (port->aspm_state & 0x4)
			disable_aspm_l11(port);
		if (port->aspm_state & 0x8)
			disable_aspm_l12(port);

		/* Disable L1SS capability if CLKREQ# is not present */
		if (!port->supports_clkreq) {
			value = readl(port->base + RP_L1_PM_SUBSTATES_CTL);
			value |= RP_L1_PM_SUBSTATES_CTL_HIDE_CAP;
			writel(value, port->base + RP_L1_PM_SUBSTATES_CTL);
		}
	}
}

static void tegra_pcie_program_ectl_settings(struct tegra_pcie_port *port)
{
	const struct tegra_pcie_soc *soc = port->pcie->soc;
	u32 value;

	value = readl(port->base + RP_ECTL_1_R1);
	value &= ~RP_ECTL_1_R1_TX_DRV_AMP_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_1_r1;
	writel(value, port->base + RP_ECTL_1_R1);

	value = readl(port->base + RP_ECTL_2_R1);
	value &= ~RP_ECTL_2_R1_RX_CTLE_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_2_r1;
	writel(value, port->base + RP_ECTL_2_R1);

	value = readl(port->base + RP_ECTL_4_R1);
	value &= ~RP_ECTL_4_R1_RX_CDR_CTRL_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_4_r1 <<
				RP_ECTL_4_R1_RX_CDR_CTRL_1C_SHIFT;
	writel(value, port->base + RP_ECTL_4_R1);

	value = readl(port->base + RP_ECTL_5_R1);
	value &= ~RP_ECTL_5_R1_RX_EQ_CTRL_L_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_5_r1;
	writel(value, port->base + RP_ECTL_5_R1);

	value = readl(port->base + RP_ECTL_6_R1);
	value &= ~RP_ECTL_6_R1_RX_EQ_CTRL_H_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_6_r1;
	writel(value, port->base + RP_ECTL_6_R1);

	value = readl(port->base + RP_ECTL_1_R2);
	value &= ~RP_ECTL_1_R2_TX_DRV_AMP_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_1_r2;
	writel(value, port->base + RP_ECTL_1_R2);

	value = readl(port->base + RP_ECTL_2_R2);
	value &= ~RP_ECTL_2_R2_RX_CTLE_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_2_r2;
	writel(value, port->base + RP_ECTL_2_R2);

	value = readl(port->base + RP_ECTL_4_R2);
	value &= ~RP_ECTL_4_R2_RX_CDR_CTRL_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_4_r2 <<
				RP_ECTL_4_R2_RX_CDR_CTRL_1C_SHIFT;
	writel(value, port->base + RP_ECTL_4_R2);

	value = readl(port->base + RP_ECTL_5_R2);
	value &= ~RP_ECTL_5_R2_RX_EQ_CTRL_L_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_5_r2;
	writel(value, port->base + RP_ECTL_5_R2);

	value = readl(port->base + RP_ECTL_6_R2);
	value &= ~RP_ECTL_6_R2_RX_EQ_CTRL_H_1C_MASK;
	value |= soc->ectl.regs.rp_ectl_6_r2;
	writel(value, port->base + RP_ECTL_6_R2);
}

static void tegra_pcie_enable_wrap(void)
{
	u32 val;
	void __iomem *msel_base;

#define MSELECT_CONFIG_BASE     0x50060000
#define MSELECT_CONFIG_WRAP_TO_INCR_SLAVE1      BIT(28)
#define MSELECT_CONFIG_ERR_RESP_EN_SLAVE1       BIT(24)

	/* Config MSELECT to support wrap trans for normal NC & GRE mapping */
	msel_base = ioremap(MSELECT_CONFIG_BASE, 4);
	val = readl(msel_base);
	/* Enable WRAP_TO_INCR_SLAVE1 */
	val |= MSELECT_CONFIG_WRAP_TO_INCR_SLAVE1;
	/* Disable ERR_RESP_EN_SLAVE1 */
	val &= ~MSELECT_CONFIG_ERR_RESP_EN_SLAVE1;
	writel(val, msel_base);
	iounmap(msel_base);
}

static void tegra_pcie_apply_sw_fixup(struct tegra_pcie_port *port)
{
	const struct tegra_pcie_soc *soc = port->pcie->soc;
	u32 value;

	/*
	 * Sometimes link speed change from Gen2 to Gen1 fails due to
	 * instability in deskew logic on lane-0. Increase the deskew
	 * retry time to resolve this issue.
	 */
	if (soc->program_deskew_time) {
		value = readl(port->base + RP_VEND_CTL0);
		value &= ~RP_VEND_CTL0_DSK_RST_PULSE_WIDTH_MASK;
		value |= RP_VEND_CTL0_DSK_RST_PULSE_WIDTH;
		writel(value, port->base + RP_VEND_CTL0);
	}

	if (soc->update_fc_timer) {
		value = readl(port->base + RP_VEND_XP);
		value &= ~RP_VEND_XP_UPDATE_FC_THRESHOLD_MASK;
		value |= soc->update_fc_threshold;
		writel(value, port->base + RP_VEND_XP);
	}

	/*
	 * PCIe link doesn't come up with few legacy PCIe endpoints if
	 * root port advertises both Gen-1 and Gen-2 speeds in Tegra.
	 * Hence, the strategy followed here is to initially advertise
	 * only Gen-1 and after link is up, retrain link to Gen-2 speed
	 */
	value = readl(port->base + RP_LINK_CONTROL_STATUS_2);
	value &= ~PCI_EXP_LNKSTA_CLS;
	value |= PCI_EXP_LNKSTA_CLS_2_5GB;
	writel(value, port->base + RP_LINK_CONTROL_STATUS_2);

	if (soc->enable_wrap)
		tegra_pcie_enable_wrap();

	if (soc->has_aspm_l1ss) {
		/* Set port Common_Mode_Restore_Time to 30us */
		value = readl(port->base + RP_L1_PM_SUBSTATES_CTL);
		value &= ~RP_L1_PM_SUBSTATES_CTL_CM_RTIME_MASK;
		value |= (0x1E << RP_L1_PM_SUBSTATES_CTL_CM_RTIME_SHIFT);
		writel(value, port->base + RP_L1_PM_SUBSTATES_CTL);

		/* set port T_POWER_ON to 70us */
		value = readl(port->base + RP_L1_PM_SUBSTATES_CTL);
		value &= ~(RP_L1_PM_SUBSTATES_CTL_T_PWRN_SCL_MASK |
				RP_L1_PM_SUBSTATES_CTL_T_PWRN_VAL_MASK);
		value |= (1 << RP_L1_PM_SUBSTATES_CTL_T_PWRN_SCL_SHIFT) |
			(7 << RP_L1_PM_SUBSTATES_CTL_T_PWRN_VAL_SHIFT);
		writel(value, port->base + RP_L1_PM_SUBSTATES_CTL);

		/* Following is based on clk_m being 19.2 MHz */
		value = readl(port->base + RP_TIMEOUT0);
		value &= ~RP_TIMEOUT0_PAD_PWRUP_MASK;
		value |= RP_TIMEOUT0_PAD_PWRUP;
		value &= ~RP_TIMEOUT0_PAD_PWRUP_CM_MASK;
		value |= RP_TIMEOUT0_PAD_PWRUP_CM;
		value &= ~RP_TIMEOUT0_PAD_SPDCHNG_GEN2_MASK;
		value |= RP_TIMEOUT0_PAD_SPDCHNG_GEN2;
		writel(value, port->base + RP_TIMEOUT0);

		value = readl(port->base + RP_TIMEOUT1);
		value &= ~RP_TIMEOUT1_RCVRY_SPD_SUCCESS_EIDLE_MASK;
		value |= RP_TIMEOUT1_RCVRY_SPD_SUCCESS_EIDLE;
		value &= ~RP_TIMEOUT1_RCVRY_SPD_UNSUCCESS_EIDLE_MASK;
		value |= RP_TIMEOUT1_RCVRY_SPD_UNSUCCESS_EIDLE;
		writel(value, port->base + RP_TIMEOUT1);

		value = readl(port->base + RP_XP_REF);
		value &= ~RP_XP_REF_MICROSECOND_LIMIT_MASK;
		value |= RP_XP_REF_MICROSECOND_LIMIT;
		value |= RP_XP_REF_MICROSECOND_ENABLE;
		value |= RP_XP_REF_CPL_TO_OVERRIDE;
		value &= ~RP_XP_REF_CPL_TO_CUSTOM_VALUE_MASK;
		value |= RP_XP_REF_CPL_TO_CUSTOM_VALUE;
		writel(value, port->base + RP_XP_REF);

		value = readl(port->base + RP_L1_PM_SUBSTATES_1_CTL);
		value &= ~RP_L1_PM_SUBSTATES_1_CTL_PWR_OFF_DLY_MASK;
		value |= RP_L1_PM_SUBSTATES_1_CTL_PWR_OFF_DLY;
		writel(value, port->base + RP_L1_PM_SUBSTATES_1_CTL);

		value = readl(port->base + RP_L1_PM_SUBSTATES_2_CTL);
		value &= ~RP_L1_PM_SUBSTATES_2_CTL_T_L1_2_DLY_MASK;
		value |= RP_L1_PM_SUBSTATES_2_CTL_T_L1_2_DLY;
		value &= ~RP_L1_PM_SUBSTATES_2_CTL_MICROSECOND_MASK;
		value |= RP_L1_PM_SUBSTATES_2_CTL_MICROSECOND;
		value &= ~RP_L1_PM_SUBSTATES_2_CTL_MICROSECOND_COMP_MASK;
		value |= RP_L1_PM_SUBSTATES_2_CTL_MICROSECOND_COMP;
		writel(value, port->base + RP_L1_PM_SUBSTATES_2_CTL);
	}

	if (soc->l1ss_rp_wake_fixup) {
		/*
		 * Set CLKREQ asserted delay greater than Power_Off
		 * time (2us) to avoid RP wakeup in L1.2.ENTRY
		 */
		value = readl(port->base + RP_L1_PM_SUBSTATES_1_CTL);
		value &= ~RP_L1SS_1_CTL_CLKREQ_ASSERTED_DLY_MASK;
		value |= RP_L1SS_1_CTL_CLKREQ_ASSERTED_DLY;
		writel(value, port->base + RP_L1_PM_SUBSTATES_1_CTL);
	}
}

static void tegra_pcie_port_enable(struct tegra_pcie_port *port)
{
	unsigned long ctrl = tegra_pcie_port_get_pex_ctrl(port);
	const struct tegra_pcie_soc *soc = port->pcie->soc;
	unsigned long value;

	/* enable reference clock */
	value = afi_readl(port->pcie, ctrl);
	value |= AFI_PEX_CTRL_REFCLK_EN;

	if (soc->has_pex_clkreq_en) {
		if (port->supports_clkreq)
			value &= ~AFI_PEX_CTRL_CLKREQ_EN;
		else
			value |= AFI_PEX_CTRL_CLKREQ_EN;
	}

	value |= AFI_PEX_CTRL_OVERRIDE_EN;

	afi_writel(port->pcie, value, ctrl);

	tegra_pcie_port_reset(port);

	/*
	 * On platforms where MXM is not directly connected to Tegra root port,
	 * 200 ms delay (worst case) is required after reset, to ensure linkup
	 * between PCIe switch and MXM
	 */
	if (port->has_mxm_port)
		mdelay(200);

	if (soc->force_pca_enable) {
		value = readl(port->base + RP_VEND_CTL2);
		value |= RP_VEND_CTL2_PCA_ENABLE;
		writel(value, port->base + RP_VEND_CTL2);
	}

	tegra_pcie_enable_rp_features(port);

	if (soc->ectl.enable)
		tegra_pcie_program_ectl_settings(port);

	tegra_pcie_apply_sw_fixup(port);
}

static void tegra_pcie_port_disable(struct tegra_pcie_port *port)
{
	unsigned long ctrl = tegra_pcie_port_get_pex_ctrl(port);
	const struct tegra_pcie_soc *soc = port->pcie->soc;
	unsigned long value;

	/* assert port reset */
	value = afi_readl(port->pcie, ctrl);
	value &= ~AFI_PEX_CTRL_RST;
	afi_writel(port->pcie, value, ctrl);

	/* disable reference clock */
	value = afi_readl(port->pcie, ctrl);

	if (soc->has_pex_clkreq_en)
		value &= ~AFI_PEX_CTRL_CLKREQ_EN;

	value &= ~AFI_PEX_CTRL_REFCLK_EN;
	afi_writel(port->pcie, value, ctrl);

	/* disable PCIe port and set CLKREQ# as GPIO to allow PLLE power down */
	value = afi_readl(port->pcie, AFI_PCIE_CONFIG);
	value |= AFI_PCIE_CONFIG_PCIE_DISABLE(port->index);
	value |= AFI_PCIE_CONFIG_PCIE_CLKREQ_GPIO(port->index);
	afi_writel(port->pcie, value, AFI_PCIE_CONFIG);
}

static void tegra_pcie_port_free(struct tegra_pcie_port *port)
{
	struct tegra_pcie *pcie = port->pcie;
	struct device *dev = pcie->dev;

	devm_iounmap(dev, port->base);
	devm_release_mem_region(dev, port->regs.start,
				resource_size(&port->regs));
	list_del(&port->list);
	devm_kfree(dev, port);
}

/* Tegra PCIE root complex wrongly reports device class */
static void tegra_pcie_fixup_class(struct pci_dev *dev)
{
	dev->class = PCI_CLASS_BRIDGE_PCI << 8;
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0bf0, tegra_pcie_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0bf1, tegra_pcie_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1c, tegra_pcie_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA, 0x0e1d, tegra_pcie_fixup_class);

/* Tegra20 and Tegra30 PCIE requires relaxed ordering */
static void tegra_pcie_relax_enable(struct pci_dev *dev)
{
	pcie_capability_set_word(dev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_RELAX_EN);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, 0x0bf0, tegra_pcie_relax_enable);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, 0x0bf1, tegra_pcie_relax_enable);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, 0x0e1c, tegra_pcie_relax_enable);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, 0x0e1d, tegra_pcie_relax_enable);

static int tegra_pcie_map_irq(const struct pci_dev *pdev, u8 slot, u8 pin)
{
	struct tegra_pcie *pcie = pdev->bus->sysdata;
	int irq;

	tegra_cpuidle_pcie_irqs_in_use();

	irq = of_irq_parse_and_map_pci(pdev, slot, pin);
	if (!irq)
		irq = pcie->irq;

	return irq;
}

static irqreturn_t tegra_pcie_isr(int irq, void *arg)
{
	const char *err_msg[] = {
		"Unknown",
		"AXI slave error",
		"AXI decode error",
		"Target abort",
		"Master abort",
		"Invalid write",
		"Legacy interrupt",
		"Response decoding error",
		"AXI response decoding error",
		"Transaction timeout",
		"Slot present pin change",
		"Slot clock request change",
		"TMS clock ramp change",
		"TMS ready for power down",
		"Peer2Peer error",
	};
	struct tegra_pcie *pcie = arg;
	struct device *dev = pcie->dev;
	u32 code, signature;

	code = afi_readl(pcie, AFI_INTR_CODE) & AFI_INTR_CODE_MASK;
	signature = afi_readl(pcie, AFI_INTR_SIGNATURE);
	afi_writel(pcie, 0, AFI_INTR_CODE);

	if (code == AFI_INTR_LEGACY)
		return IRQ_NONE;

	if (code >= ARRAY_SIZE(err_msg))
		code = 0;

	/*
	 * do not pollute kernel log with master abort reports since they
	 * happen a lot during enumeration
	 */
	if (code == AFI_INTR_MASTER_ABORT || code == AFI_INTR_PE_PRSNT_SENSE)
		dev_dbg(dev, "%s, signature: %08x\n", err_msg[code], signature);
	else
		dev_err(dev, "%s, signature: %08x\n", err_msg[code], signature);

	if (code == AFI_INTR_TARGET_ABORT || code == AFI_INTR_MASTER_ABORT ||
	    code == AFI_INTR_FPCI_DECODE_ERROR) {
		u32 fpci = afi_readl(pcie, AFI_UPPER_FPCI_ADDRESS) & 0xff;
		u64 address = (u64)fpci << 32 | (signature & 0xfffffffc);

		if (code == AFI_INTR_MASTER_ABORT)
			dev_dbg(dev, "  FPCI address: %10llx\n", address);
		else
			dev_err(dev, "  FPCI address: %10llx\n", address);
	}

	return IRQ_HANDLED;
}

/*
 * FPCI map is as follows:
 * - 0xfdfc000000: I/O space
 * - 0xfdfe000000: type 0 configuration space
 * - 0xfdff000000: type 1 configuration space
 * - 0xfe00000000: type 0 extended configuration space
 * - 0xfe10000000: type 1 extended configuration space
 */
static void tegra_pcie_setup_translations(struct tegra_pcie *pcie)
{
	u32 size;
	struct resource_entry *entry;
	struct pci_host_bridge *bridge = pci_host_bridge_from_priv(pcie);

	/* Bar 0: type 1 extended configuration space */
	size = resource_size(&pcie->cs);
	afi_writel(pcie, pcie->cs.start, AFI_AXI_BAR0_START);
	afi_writel(pcie, size >> 12, AFI_AXI_BAR0_SZ);

	resource_list_for_each_entry(entry, &bridge->windows) {
		u32 fpci_bar, axi_address;
		struct resource *res = entry->res;

		size = resource_size(res);

		switch (resource_type(res)) {
		case IORESOURCE_IO:
			/* Bar 1: downstream IO bar */
			fpci_bar = 0xfdfc0000;
			axi_address = pci_pio_to_address(res->start);
			afi_writel(pcie, axi_address, AFI_AXI_BAR1_START);
			afi_writel(pcie, size >> 12, AFI_AXI_BAR1_SZ);
			afi_writel(pcie, fpci_bar, AFI_FPCI_BAR1);
			break;
		case IORESOURCE_MEM:
			fpci_bar = (((res->start >> 12) & 0x0fffffff) << 4) | 0x1;
			axi_address = res->start;

			if (res->flags & IORESOURCE_PREFETCH) {
				/* Bar 2: prefetchable memory BAR */
				afi_writel(pcie, axi_address, AFI_AXI_BAR2_START);
				afi_writel(pcie, size >> 12, AFI_AXI_BAR2_SZ);
				afi_writel(pcie, fpci_bar, AFI_FPCI_BAR2);

			} else {
				/* Bar 3: non prefetchable memory BAR */
				afi_writel(pcie, axi_address, AFI_AXI_BAR3_START);
				afi_writel(pcie, size >> 12, AFI_AXI_BAR3_SZ);
				afi_writel(pcie, fpci_bar, AFI_FPCI_BAR3);
			}
			break;
		}
	}

	/* NULL out the remaining BARs as they are not used */
	afi_writel(pcie, 0, AFI_AXI_BAR4_START);
	afi_writel(pcie, 0, AFI_AXI_BAR4_SZ);
	afi_writel(pcie, 0, AFI_FPCI_BAR4);

	afi_writel(pcie, 0, AFI_AXI_BAR5_START);
	afi_writel(pcie, 0, AFI_AXI_BAR5_SZ);
	afi_writel(pcie, 0, AFI_FPCI_BAR5);

	if (pcie->soc->has_cache_bars) {
		/* map all upstream transactions as uncached */
		afi_writel(pcie, 0, AFI_CACHE_BAR0_ST);
		afi_writel(pcie, 0, AFI_CACHE_BAR0_SZ);
		afi_writel(pcie, 0, AFI_CACHE_BAR1_ST);
		afi_writel(pcie, 0, AFI_CACHE_BAR1_SZ);
	}

	/* MSI translations are setup only when needed */
	afi_writel(pcie, 0, AFI_MSI_FPCI_BAR_ST);
	afi_writel(pcie, 0, AFI_MSI_BAR_SZ);
	afi_writel(pcie, 0, AFI_MSI_AXI_BAR_ST);
	afi_writel(pcie, 0, AFI_MSI_BAR_SZ);
}

static int tegra_pcie_pll_wait(struct tegra_pcie *pcie, unsigned long timeout)
{
	const struct tegra_pcie_soc *soc = pcie->soc;
	u32 value;

	timeout = jiffies + msecs_to_jiffies(timeout);

	while (time_before(jiffies, timeout)) {
		value = pads_readl(pcie, soc->pads_pll_ctl);
		if (value & PADS_PLL_CTL_LOCKDET)
			return 0;
	}

	return -ETIMEDOUT;
}

static int tegra_pcie_phy_enable(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	const struct tegra_pcie_soc *soc = pcie->soc;
	u32 value;
	int err;

	/* initialize internal PHY, enable up to 16 PCIE lanes */
	pads_writel(pcie, 0x0, PADS_CTL_SEL);

	/* override IDDQ to 1 on all 4 lanes */
	value = pads_readl(pcie, PADS_CTL);
	value |= PADS_CTL_IDDQ_1L;
	pads_writel(pcie, value, PADS_CTL);

	/*
	 * Set up PHY PLL inputs select PLLE output as refclock,
	 * set TX ref sel to div10 (not div5).
	 */
	value = pads_readl(pcie, soc->pads_pll_ctl);
	value &= ~(PADS_PLL_CTL_REFCLK_MASK | PADS_PLL_CTL_TXCLKREF_MASK);
	value |= PADS_PLL_CTL_REFCLK_INTERNAL_CML | soc->tx_ref_sel;
	pads_writel(pcie, value, soc->pads_pll_ctl);

	/* reset PLL */
	value = pads_readl(pcie, soc->pads_pll_ctl);
	value &= ~PADS_PLL_CTL_RST_B4SM;
	pads_writel(pcie, value, soc->pads_pll_ctl);

	usleep_range(20, 100);

	/* take PLL out of reset  */
	value = pads_readl(pcie, soc->pads_pll_ctl);
	value |= PADS_PLL_CTL_RST_B4SM;
	pads_writel(pcie, value, soc->pads_pll_ctl);

	/* wait for the PLL to lock */
	err = tegra_pcie_pll_wait(pcie, 500);
	if (err < 0) {
		dev_err(dev, "PLL failed to lock: %d\n", err);
		return err;
	}

	/* turn off IDDQ override */
	value = pads_readl(pcie, PADS_CTL);
	value &= ~PADS_CTL_IDDQ_1L;
	pads_writel(pcie, value, PADS_CTL);

	/* enable TX/RX data */
	value = pads_readl(pcie, PADS_CTL);
	value |= PADS_CTL_TX_DATA_EN_1L | PADS_CTL_RX_DATA_EN_1L;
	pads_writel(pcie, value, PADS_CTL);

	return 0;
}

static int tegra_pcie_phy_disable(struct tegra_pcie *pcie)
{
	const struct tegra_pcie_soc *soc = pcie->soc;
	u32 value;

	/* disable TX/RX data */
	value = pads_readl(pcie, PADS_CTL);
	value &= ~(PADS_CTL_TX_DATA_EN_1L | PADS_CTL_RX_DATA_EN_1L);
	pads_writel(pcie, value, PADS_CTL);

	/* override IDDQ */
	value = pads_readl(pcie, PADS_CTL);
	value |= PADS_CTL_IDDQ_1L;
	pads_writel(pcie, value, PADS_CTL);

	/* reset PLL */
	value = pads_readl(pcie, soc->pads_pll_ctl);
	value &= ~PADS_PLL_CTL_RST_B4SM;
	pads_writel(pcie, value, soc->pads_pll_ctl);

	usleep_range(20, 100);

	return 0;
}

static int tegra_pcie_port_phy_power_on(struct tegra_pcie_port *port)
{
	struct device *dev = port->pcie->dev;
	unsigned int i;
	int err;

	for (i = 0; i < port->lanes; i++) {
		err = phy_power_on(port->phys[i]);
		if (err < 0) {
			dev_err(dev, "failed to power on PHY#%u: %d\n", i, err);
			return err;
		}
	}

	return 0;
}

static int tegra_pcie_port_phy_power_off(struct tegra_pcie_port *port)
{
	struct device *dev = port->pcie->dev;
	unsigned int i;
	int err;

	for (i = 0; i < port->lanes; i++) {
		err = phy_power_off(port->phys[i]);
		if (err < 0) {
			dev_err(dev, "failed to power off PHY#%u: %d\n", i,
				err);
			return err;
		}
	}

	return 0;
}

static int tegra_pcie_phy_power_on(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct tegra_pcie_port *port;
	int err;

	if (pcie->legacy_phy) {
		if (pcie->phy)
			err = phy_power_on(pcie->phy);
		else
			err = tegra_pcie_phy_enable(pcie);

		if (err < 0)
			dev_err(dev, "failed to power on PHY: %d\n", err);

		return err;
	}

	list_for_each_entry(port, &pcie->ports, list) {
		err = tegra_pcie_port_phy_power_on(port);
		if (err < 0) {
			dev_err(dev,
				"failed to power on PCIe port %u PHY: %d\n",
				port->index, err);
			return err;
		}
	}

	return 0;
}

static int tegra_pcie_phy_power_off(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct tegra_pcie_port *port;
	int err;

	if (pcie->legacy_phy) {
		if (pcie->phy)
			err = phy_power_off(pcie->phy);
		else
			err = tegra_pcie_phy_disable(pcie);

		if (err < 0)
			dev_err(dev, "failed to power off PHY: %d\n", err);

		return err;
	}

	list_for_each_entry(port, &pcie->ports, list) {
		err = tegra_pcie_port_phy_power_off(port);
		if (err < 0) {
			dev_err(dev,
				"failed to power off PCIe port %u PHY: %d\n",
				port->index, err);
			return err;
		}
	}

	return 0;
}

static void tegra_pcie_enable_controller(struct tegra_pcie *pcie)
{
	const struct tegra_pcie_soc *soc = pcie->soc;
	struct tegra_pcie_port *port;
	unsigned long value;

	/* enable PLL power down */
	if (soc->has_aspm_l1ss) {
		value = afi_readl(pcie, AFI_PLLE_CONTROL);
		value &= ~AFI_PLLE_CONTROL_BYPASS_PADS2PLLE_CONTROL;
		value |= AFI_PLLE_CONTROL_PADS2PLLE_CONTROL_EN;

		list_for_each_entry(port, &pcie->ports, list) {
			if (!port->supports_clkreq) {
				value &= ~AFI_PLLE_CONTROL_PADS2PLLE_CONTROL_EN;
				break;
			}
		}

		value &= ~AFI_PLLE_CONTROL_BYPASS_PCIE2PLLE_CONTROL;
		value |= AFI_PLLE_CONTROL_PCIE2PLLE_CONTROL_EN;
		afi_writel(pcie, value, AFI_PLLE_CONTROL);
	}

	/* power down PCIe slot clock bias pad */
	if (soc->has_pex_bias_ctrl)
		afi_writel(pcie, 0, AFI_PEXBIAS_CTRL_0);

	/* configure mode and disable all ports */
	value = afi_readl(pcie, AFI_PCIE_CONFIG);
	value &= ~AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK;
	value |= AFI_PCIE_CONFIG_PCIE_DISABLE_ALL | pcie->xbar_config;
	value |= AFI_PCIE_CONFIG_PCIE_CLKREQ_GPIO_ALL;

	list_for_each_entry(port, &pcie->ports, list) {
		value &= ~AFI_PCIE_CONFIG_PCIE_DISABLE(port->index);
		value &= ~AFI_PCIE_CONFIG_PCIE_CLKREQ_GPIO(port->index);
	}

	afi_writel(pcie, value, AFI_PCIE_CONFIG);

	if (soc->has_gen2) {
		value = afi_readl(pcie, AFI_FUSE);
		value &= ~AFI_FUSE_PCIE_T0_GEN2_DIS;
		afi_writel(pcie, value, AFI_FUSE);
	} else {
		value = afi_readl(pcie, AFI_FUSE);
		value |= AFI_FUSE_PCIE_T0_GEN2_DIS;
		afi_writel(pcie, value, AFI_FUSE);
	}

	/* Disable AFI dynamic clock gating and enable PCIe */
	value = afi_readl(pcie, AFI_CONFIGURATION);
	value |= AFI_CONFIGURATION_EN_FPCI;
	value |= AFI_CONFIGURATION_CLKEN_OVERRIDE;
	afi_writel(pcie, value, AFI_CONFIGURATION);

	value = AFI_INTR_EN_INI_SLVERR | AFI_INTR_EN_INI_DECERR |
		AFI_INTR_EN_TGT_SLVERR | AFI_INTR_EN_TGT_DECERR |
		AFI_INTR_EN_TGT_WRERR | AFI_INTR_EN_DFPCI_DECERR;

	if (soc->has_intr_prsnt_sense)
		value |= AFI_INTR_EN_PRSNT_SENSE;

	afi_writel(pcie, value, AFI_AFI_INTR_ENABLE);
	afi_writel(pcie, 0xffffffff, AFI_SM_INTR_ENABLE);

	/* don't enable MSI for now, only when needed */
	afi_writel(pcie, AFI_INTR_MASK_INT_MASK, AFI_INTR_MASK);

	/* disable all exceptions */
	afi_writel(pcie, 0, AFI_FPCI_ERROR_MASKS);
}

static void tegra_pcie_power_off(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	const struct tegra_pcie_soc *soc = pcie->soc;
	int err;

	reset_control_assert(pcie->afi_rst);

	clk_disable_unprepare(pcie->pll_e);
	if (soc->has_cml_clk)
		clk_disable_unprepare(pcie->cml_clk);
	clk_disable_unprepare(pcie->afi_clk);

	if (!dev->pm_domain)
		tegra_powergate_power_off(TEGRA_POWERGATE_PCIE);

	err = regulator_bulk_disable(pcie->num_supplies, pcie->supplies);
	if (err < 0)
		dev_warn(dev, "failed to disable regulators: %d\n", err);
}

static int tegra_pcie_power_on(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	const struct tegra_pcie_soc *soc = pcie->soc;
	int err;

	reset_control_assert(pcie->pcie_xrst);
	reset_control_assert(pcie->afi_rst);
	reset_control_assert(pcie->pex_rst);

	if (!dev->pm_domain)
		tegra_powergate_power_off(TEGRA_POWERGATE_PCIE);

	/* enable regulators */
	err = regulator_bulk_enable(pcie->num_supplies, pcie->supplies);
	if (err < 0)
		dev_err(dev, "failed to enable regulators: %d\n", err);

	if (!dev->pm_domain) {
		err = tegra_powergate_power_on(TEGRA_POWERGATE_PCIE);
		if (err) {
			dev_err(dev, "failed to power ungate: %d\n", err);
			goto regulator_disable;
		}
		err = tegra_powergate_remove_clamping(TEGRA_POWERGATE_PCIE);
		if (err) {
			dev_err(dev, "failed to remove clamp: %d\n", err);
			goto powergate;
		}
	}

	err = clk_prepare_enable(pcie->afi_clk);
	if (err < 0) {
		dev_err(dev, "failed to enable AFI clock: %d\n", err);
		goto powergate;
	}

	if (soc->has_cml_clk) {
		err = clk_prepare_enable(pcie->cml_clk);
		if (err < 0) {
			dev_err(dev, "failed to enable CML clock: %d\n", err);
			goto disable_afi_clk;
		}
	}

	err = clk_prepare_enable(pcie->pll_e);
	if (err < 0) {
		dev_err(dev, "failed to enable PLLE clock: %d\n", err);
		goto disable_cml_clk;
	}

	reset_control_deassert(pcie->afi_rst);

	return 0;

disable_cml_clk:
	if (soc->has_cml_clk)
		clk_disable_unprepare(pcie->cml_clk);
disable_afi_clk:
	clk_disable_unprepare(pcie->afi_clk);
powergate:
	if (!dev->pm_domain)
		tegra_powergate_power_off(TEGRA_POWERGATE_PCIE);
regulator_disable:
	regulator_bulk_disable(pcie->num_supplies, pcie->supplies);

	return err;
}

static void tegra_pcie_apply_pad_settings(struct tegra_pcie *pcie)
{
	const struct tegra_pcie_soc *soc = pcie->soc;

	/* Configure the reference clock driver */
	pads_writel(pcie, soc->pads_refclk_cfg0, PADS_REFCLK_CFG0);

	if (soc->num_ports > 2)
		pads_writel(pcie, soc->pads_refclk_cfg1, PADS_REFCLK_CFG1);
}

static int tegra_pcie_clocks_get(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	const struct tegra_pcie_soc *soc = pcie->soc;

	pcie->pex_clk = devm_clk_get(dev, "pex");
	if (IS_ERR(pcie->pex_clk))
		return PTR_ERR(pcie->pex_clk);

	pcie->afi_clk = devm_clk_get(dev, "afi");
	if (IS_ERR(pcie->afi_clk))
		return PTR_ERR(pcie->afi_clk);

	pcie->pll_e = devm_clk_get(dev, "pll_e");
	if (IS_ERR(pcie->pll_e))
		return PTR_ERR(pcie->pll_e);

	if (soc->has_cml_clk) {
		pcie->cml_clk = devm_clk_get(dev, "cml");
		if (IS_ERR(pcie->cml_clk))
			return PTR_ERR(pcie->cml_clk);
	}

	return 0;
}

static int tegra_pcie_resets_get(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;

	pcie->pex_rst = devm_reset_control_get_exclusive(dev, "pex");
	if (IS_ERR(pcie->pex_rst))
		return PTR_ERR(pcie->pex_rst);

	pcie->afi_rst = devm_reset_control_get_exclusive(dev, "afi");
	if (IS_ERR(pcie->afi_rst))
		return PTR_ERR(pcie->afi_rst);

	pcie->pcie_xrst = devm_reset_control_get_exclusive(dev, "pcie_x");
	if (IS_ERR(pcie->pcie_xrst))
		return PTR_ERR(pcie->pcie_xrst);

	return 0;
}

static int tegra_pcie_phys_get_legacy(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	int err;

	pcie->phy = devm_phy_optional_get(dev, "pcie");
	if (IS_ERR(pcie->phy)) {
		err = PTR_ERR(pcie->phy);
		dev_err(dev, "failed to get PHY: %d\n", err);
		return err;
	}

	err = phy_init(pcie->phy);
	if (err < 0) {
		dev_err(dev, "failed to initialize PHY: %d\n", err);
		return err;
	}

	pcie->legacy_phy = true;

	return 0;
}

static struct phy *devm_of_phy_optional_get_index(struct device *dev,
						  struct device_node *np,
						  const char *consumer,
						  unsigned int index)
{
	struct phy *phy;
	char *name;

	name = kasprintf(GFP_KERNEL, "%s-%u", consumer, index);
	if (!name)
		return ERR_PTR(-ENOMEM);

	phy = devm_of_phy_get(dev, np, name);
	kfree(name);

	if (PTR_ERR(phy) == -ENODEV)
		phy = NULL;

	return phy;
}

static int tegra_pcie_port_get_phys(struct tegra_pcie_port *port)
{
	struct device *dev = port->pcie->dev;
	struct phy *phy;
	unsigned int i;
	int err;

	port->phys = devm_kcalloc(dev, sizeof(phy), port->lanes, GFP_KERNEL);
	if (!port->phys)
		return -ENOMEM;

	for (i = 0; i < port->lanes; i++) {
		phy = devm_of_phy_optional_get_index(dev, port->np, "pcie", i);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to get PHY#%u: %ld\n", i,
				PTR_ERR(phy));
			return PTR_ERR(phy);
		}

		err = phy_init(phy);
		if (err < 0) {
			dev_err(dev, "failed to initialize PHY#%u: %d\n", i,
				err);
			return err;
		}

		port->phys[i] = phy;
	}

	return 0;
}

static int tegra_pcie_phys_get(struct tegra_pcie *pcie)
{
	const struct tegra_pcie_soc *soc = pcie->soc;
	struct device_node *np = pcie->dev->of_node;
	struct tegra_pcie_port *port;
	int err;

	if (!soc->has_gen2 || of_find_property(np, "phys", NULL) != NULL)
		return tegra_pcie_phys_get_legacy(pcie);

	list_for_each_entry(port, &pcie->ports, list) {
		err = tegra_pcie_port_get_phys(port);
		if (err < 0)
			return err;
	}

	return 0;
}

static void tegra_pcie_phys_put(struct tegra_pcie *pcie)
{
	struct tegra_pcie_port *port;
	struct device *dev = pcie->dev;
	int err, i;

	if (pcie->legacy_phy) {
		err = phy_exit(pcie->phy);
		if (err < 0)
			dev_err(dev, "failed to teardown PHY: %d\n", err);
		return;
	}

	list_for_each_entry(port, &pcie->ports, list) {
		for (i = 0; i < port->lanes; i++) {
			err = phy_exit(port->phys[i]);
			if (err < 0)
				dev_err(dev, "failed to teardown PHY#%u: %d\n",
					i, err);
		}
	}
}


static int tegra_pcie_get_resources(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;
	const struct tegra_pcie_soc *soc = pcie->soc;
	int err;

	err = tegra_pcie_clocks_get(pcie);
	if (err) {
		dev_err(dev, "failed to get clocks: %d\n", err);
		return err;
	}

	err = tegra_pcie_resets_get(pcie);
	if (err) {
		dev_err(dev, "failed to get resets: %d\n", err);
		return err;
	}

	if (soc->program_uphy) {
		err = tegra_pcie_phys_get(pcie);
		if (err < 0) {
			dev_err(dev, "failed to get PHYs: %d\n", err);
			return err;
		}
	}

	pcie->pads = devm_platform_ioremap_resource_byname(pdev, "pads");
	if (IS_ERR(pcie->pads)) {
		err = PTR_ERR(pcie->pads);
		goto phys_put;
	}

	pcie->afi = devm_platform_ioremap_resource_byname(pdev, "afi");
	if (IS_ERR(pcie->afi)) {
		err = PTR_ERR(pcie->afi);
		goto phys_put;
	}

	/* request configuration space, but remap later, on demand */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cs");
	if (!res) {
		err = -EADDRNOTAVAIL;
		goto phys_put;
	}

	pcie->cs = *res;

	/* constrain configuration space to 4 KiB */
	pcie->cs.end = pcie->cs.start + SZ_4K - 1;

	pcie->cfg = devm_ioremap_resource(dev, &pcie->cs);
	if (IS_ERR(pcie->cfg)) {
		err = PTR_ERR(pcie->cfg);
		goto phys_put;
	}

	/* request interrupt */
	err = platform_get_irq_byname(pdev, "intr");
	if (err < 0)
		goto phys_put;

	pcie->irq = err;

	err = request_irq(pcie->irq, tegra_pcie_isr, IRQF_SHARED, "PCIE", pcie);
	if (err) {
		dev_err(dev, "failed to register IRQ: %d\n", err);
		goto phys_put;
	}

	return 0;

phys_put:
	if (soc->program_uphy)
		tegra_pcie_phys_put(pcie);
	return err;
}

static int tegra_pcie_put_resources(struct tegra_pcie *pcie)
{
	const struct tegra_pcie_soc *soc = pcie->soc;

	if (pcie->irq > 0)
		free_irq(pcie->irq, pcie);

	if (soc->program_uphy)
		tegra_pcie_phys_put(pcie);

	return 0;
}

static void tegra_pcie_config_plat(struct tegra_pcie *pcie, bool set)
{
	struct tegra_pcie_port *port;
	int count;

	list_for_each_entry(port, &pcie->ports, list) {
		for (count = 0; count < port->n_gpios; ++count)
			gpiod_set_value(gpio_to_desc(port->gpios[count]), set);
	}
}

static void tegra_pcie_pme_turnoff(struct tegra_pcie_port *port)
{
	struct tegra_pcie *pcie = port->pcie;
	const struct tegra_pcie_soc *soc = pcie->soc;
	int err;
	u32 val;
	u8 ack_bit;

	val = afi_readl(pcie, AFI_PCIE_PME);
	val |= (0x1 << soc->ports[port->index].pme.turnoff_bit);
	afi_writel(pcie, val, AFI_PCIE_PME);

	ack_bit = soc->ports[port->index].pme.ack_bit;
	err = readl_poll_timeout(pcie->afi + AFI_PCIE_PME, val,
				 val & (0x1 << ack_bit), 1, PME_ACK_TIMEOUT);
	if (err)
		dev_err(pcie->dev, "PME Ack is not received on port: %d\n",
			port->index);

	usleep_range(10000, 11000);

	val = afi_readl(pcie, AFI_PCIE_PME);
	val &= ~(0x1 << soc->ports[port->index].pme.turnoff_bit);
	afi_writel(pcie, val, AFI_PCIE_PME);

	/* PCIe link is in L2, bypass CLKREQ# control over PLLE power down */
	val = afi_readl(pcie, AFI_PLLE_CONTROL);
	val |= AFI_PLLE_CONTROL_BYPASS_PADS2PLLE_CONTROL;
	afi_writel(pcie, val, AFI_PLLE_CONTROL);
}

static int tegra_msi_alloc(struct tegra_msi *chip)
{
	int msi;

	mutex_lock(&chip->lock);

	msi = find_first_zero_bit(chip->used, INT_PCI_MSI_NR);
	if (msi < INT_PCI_MSI_NR)
		set_bit(msi, chip->used);
	else
		msi = -ENOSPC;

	mutex_unlock(&chip->lock);

	return msi;
}

static void tegra_msi_free(struct tegra_msi *chip, unsigned long irq)
{
	struct device *dev = chip->chip.dev;

	mutex_lock(&chip->lock);

	if (!test_bit(irq, chip->used))
		dev_err(dev, "trying to free unused MSI#%lu\n", irq);
	else
		clear_bit(irq, chip->used);

	mutex_unlock(&chip->lock);
}

static irqreturn_t tegra_pcie_msi_irq(int irq, void *data)
{
	struct tegra_pcie *pcie = data;
	struct device *dev = pcie->dev;
	struct tegra_msi *msi = &pcie->msi;
	unsigned int i, processed = 0;

	for (i = 0; i < 8; i++) {
		unsigned long reg = afi_readl(pcie, AFI_MSI_VEC0 + i * 4);

		while (reg) {
			unsigned int offset = find_first_bit(&reg, 32);
			unsigned int index = i * 32 + offset;
			unsigned int irq;

			/* clear the interrupt */
			afi_writel(pcie, 1 << offset, AFI_MSI_VEC0 + i * 4);

			irq = irq_find_mapping(msi->domain, index);
			if (irq) {
				if (test_bit(index, msi->used))
					generic_handle_irq(irq);
				else
					dev_info(dev, "unhandled MSI\n");
			} else {
				/*
				 * that's weird who triggered this?
				 * just clear it
				 */
				dev_info(dev, "unexpected MSI\n");
			}

			/* see if there's any more pending in this vector */
			reg = afi_readl(pcie, AFI_MSI_VEC0 + i * 4);

			processed++;
		}
	}

	return processed > 0 ? IRQ_HANDLED : IRQ_NONE;
}

static int tegra_msi_setup_irq(struct msi_controller *chip,
			       struct pci_dev *pdev, struct msi_desc *desc)
{
	struct tegra_msi *msi = to_tegra_msi(chip);
	struct msi_msg msg;
	unsigned int irq;
	int hwirq;

	hwirq = tegra_msi_alloc(msi);
	if (hwirq < 0)
		return hwirq;

	irq = irq_create_mapping(msi->domain, hwirq);
	if (!irq) {
		tegra_msi_free(msi, hwirq);
		return -EINVAL;
	}

	irq_set_msi_desc(irq, desc);

	msg.address_lo = lower_32_bits(msi->phys);
	msg.address_hi = upper_32_bits(msi->phys);
	msg.data = hwirq;

	pci_write_msi_msg(irq, &msg);

	return 0;
}

static void tegra_msi_teardown_irq(struct msi_controller *chip,
				   unsigned int irq)
{
	struct tegra_msi *msi = to_tegra_msi(chip);
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);

	irq_dispose_mapping(irq);
	tegra_msi_free(msi, hwirq);
}

static struct irq_chip tegra_msi_irq_chip = {
	.name = "Tegra PCIe MSI",
	.irq_enable = pci_msi_unmask_irq,
	.irq_disable = pci_msi_mask_irq,
	.irq_mask = pci_msi_mask_irq,
	.irq_unmask = pci_msi_unmask_irq,
};

static int tegra_msi_map(struct irq_domain *domain, unsigned int irq,
			 irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &tegra_msi_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);

	tegra_cpuidle_pcie_irqs_in_use();

	return 0;
}

static const struct irq_domain_ops msi_domain_ops = {
	.map = tegra_msi_map,
};

static int tegra_pcie_msi_setup(struct tegra_pcie *pcie)
{
	struct pci_host_bridge *host = pci_host_bridge_from_priv(pcie);
	struct platform_device *pdev = to_platform_device(pcie->dev);
	struct tegra_msi *msi = &pcie->msi;
	struct device *dev = pcie->dev;
	int err;

	mutex_init(&msi->lock);

	msi->chip.dev = dev;
	msi->chip.setup_irq = tegra_msi_setup_irq;
	msi->chip.teardown_irq = tegra_msi_teardown_irq;

	msi->domain = irq_domain_add_linear(dev->of_node, INT_PCI_MSI_NR,
					    &msi_domain_ops, &msi->chip);
	if (!msi->domain) {
		dev_err(dev, "failed to create IRQ domain\n");
		return -ENOMEM;
	}

	err = platform_get_irq_byname(pdev, "msi");
	if (err < 0)
		goto free_irq_domain;

	msi->irq = err;

	err = request_irq(msi->irq, tegra_pcie_msi_irq, IRQF_NO_THREAD,
			  tegra_msi_irq_chip.name, pcie);
	if (err < 0) {
		dev_err(dev, "failed to request IRQ: %d\n", err);
		goto free_irq_domain;
	}

	/* Though the PCIe controller can address >32-bit address space, to
	 * facilitate endpoints that support only 32-bit MSI target address,
	 * the mask is set to 32-bit to make sure that MSI target address is
	 * always a 32-bit address
	 */
	err = dma_set_coherent_mask(dev, DMA_BIT_MASK(32));
	if (err < 0) {
		dev_err(dev, "failed to set DMA coherent mask: %d\n", err);
		goto free_irq;
	}

	msi->virt = dma_alloc_attrs(dev, PAGE_SIZE, &msi->phys, GFP_KERNEL,
				    DMA_ATTR_NO_KERNEL_MAPPING);
	if (!msi->virt) {
		dev_err(dev, "failed to allocate DMA memory for MSI\n");
		err = -ENOMEM;
		goto free_irq;
	}

	host->msi = &msi->chip;

	return 0;

free_irq:
	free_irq(msi->irq, pcie);
free_irq_domain:
	irq_domain_remove(msi->domain);
	return err;
}

static void tegra_pcie_enable_msi(struct tegra_pcie *pcie)
{
	const struct tegra_pcie_soc *soc = pcie->soc;
	struct tegra_msi *msi = &pcie->msi;
	u32 reg;

	afi_writel(pcie, msi->phys >> soc->msi_base_shift, AFI_MSI_FPCI_BAR_ST);
	afi_writel(pcie, msi->phys, AFI_MSI_AXI_BAR_ST);
	/* this register is in 4K increments */
	afi_writel(pcie, 1, AFI_MSI_BAR_SZ);

	/* enable all MSI vectors */
	afi_writel(pcie, 0xffffffff, AFI_MSI_EN_VEC0);
	afi_writel(pcie, 0xffffffff, AFI_MSI_EN_VEC1);
	afi_writel(pcie, 0xffffffff, AFI_MSI_EN_VEC2);
	afi_writel(pcie, 0xffffffff, AFI_MSI_EN_VEC3);
	afi_writel(pcie, 0xffffffff, AFI_MSI_EN_VEC4);
	afi_writel(pcie, 0xffffffff, AFI_MSI_EN_VEC5);
	afi_writel(pcie, 0xffffffff, AFI_MSI_EN_VEC6);
	afi_writel(pcie, 0xffffffff, AFI_MSI_EN_VEC7);

	/* and unmask the MSI interrupt */
	reg = afi_readl(pcie, AFI_INTR_MASK);
	reg |= AFI_INTR_MASK_MSI_MASK;
	afi_writel(pcie, reg, AFI_INTR_MASK);
}

static void tegra_pcie_msi_teardown(struct tegra_pcie *pcie)
{
	struct tegra_msi *msi = &pcie->msi;
	unsigned int i, irq;

	dma_free_attrs(pcie->dev, PAGE_SIZE, msi->virt, msi->phys,
		       DMA_ATTR_NO_KERNEL_MAPPING);

	if (msi->irq > 0)
		free_irq(msi->irq, pcie);

	for (i = 0; i < INT_PCI_MSI_NR; i++) {
		irq = irq_find_mapping(msi->domain, i);
		if (irq > 0)
			irq_dispose_mapping(irq);
	}

	irq_domain_remove(msi->domain);
}

static int tegra_pcie_disable_msi(struct tegra_pcie *pcie)
{
	u32 value;

	/* mask the MSI interrupt */
	value = afi_readl(pcie, AFI_INTR_MASK);
	value &= ~AFI_INTR_MASK_MSI_MASK;
	afi_writel(pcie, value, AFI_INTR_MASK);

	/* disable all MSI vectors */
	afi_writel(pcie, 0, AFI_MSI_EN_VEC0);
	afi_writel(pcie, 0, AFI_MSI_EN_VEC1);
	afi_writel(pcie, 0, AFI_MSI_EN_VEC2);
	afi_writel(pcie, 0, AFI_MSI_EN_VEC3);
	afi_writel(pcie, 0, AFI_MSI_EN_VEC4);
	afi_writel(pcie, 0, AFI_MSI_EN_VEC5);
	afi_writel(pcie, 0, AFI_MSI_EN_VEC6);
	afi_writel(pcie, 0, AFI_MSI_EN_VEC7);

	return 0;
}

static void tegra_pcie_disable_interrupts(struct tegra_pcie *pcie)
{
	u32 value;

	value = afi_readl(pcie, AFI_INTR_MASK);
	value &= ~AFI_INTR_MASK_INT_MASK;
	afi_writel(pcie, value, AFI_INTR_MASK);
}

static int tegra_pcie_get_xbar_config(struct tegra_pcie *pcie, u32 lanes,
				      u32 *xbar)
{
	struct device *dev = pcie->dev;
	struct device_node *np = dev->of_node;

	if (of_device_is_compatible(np, "nvidia,tegra186-pcie")) {
		switch (lanes) {
		case 0x010004:
			dev_info(dev, "4x1, 1x1 configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_401;
			return 0;

		case 0x010102:
			dev_info(dev, "2x1, 1X1, 1x1 configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_211;
			return 0;

		case 0x010101:
			dev_info(dev, "1x1, 1x1, 1x1 configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_111;
			return 0;

		default:
			dev_info(dev, "wrong configuration updated in DT, "
				 "switching to default 2x1, 1x1, 1x1 "
				 "configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_211;
			return 0;
		}
	} else if (of_device_is_compatible(np, "nvidia,tegra210b01-pcie")) {
		dev_info(dev, "4x1, 1x1 configuration\n");
		*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X4_X1;
		return 0;
	} else if (of_device_is_compatible(np, "nvidia,tegra124-pcie") ||
		   of_device_is_compatible(np, "nvidia,tegra210-pcie")) {
		switch (lanes) {
		case 0x0000104:
			dev_info(dev, "4x1, 1x1 configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X4_X1;
			return 0;

		case 0x0000102:
			dev_info(dev, "2x1, 1x1 configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_X2_X1;
			return 0;
		}
	} else if (of_device_is_compatible(np, "nvidia,tegra30-pcie")) {
		switch (lanes) {
		case 0x00000204:
			dev_info(dev, "4x1, 2x1 configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_420;
			return 0;

		case 0x00020202:
			dev_info(dev, "2x3 configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_222;
			return 0;

		case 0x00010104:
			dev_info(dev, "4x1, 1x2 configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_411;
			return 0;
		}
	} else if (of_device_is_compatible(np, "nvidia,tegra20-pcie")) {
		switch (lanes) {
		case 0x00000004:
			dev_info(dev, "single-mode configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_SINGLE;
			return 0;

		case 0x00000202:
			dev_info(dev, "dual-mode configuration\n");
			*xbar = AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_DUAL;
			return 0;
		}
	}

	return -EINVAL;
}

/*
 * Check whether a given set of supplies is available in a device tree node.
 * This is used to check whether the new or the legacy device tree bindings
 * should be used.
 */
static bool of_regulator_bulk_available(struct device_node *np,
					struct regulator_bulk_data *supplies,
					unsigned int num_supplies)
{
	char property[32];
	unsigned int i;

	for (i = 0; i < num_supplies; i++) {
		snprintf(property, 32, "%s-supply", supplies[i].supply);

		if (of_find_property(np, property, NULL) == NULL)
			return false;
	}

	return true;
}

/*
 * Old versions of the device tree binding for this device used a set of power
 * supplies that didn't match the hardware inputs. This happened to work for a
 * number of cases but is not future proof. However to preserve backwards-
 * compatibility with old device trees, this function will try to use the old
 * set of supplies.
 */
static int tegra_pcie_get_legacy_regulators(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct device_node *np = dev->of_node;

	if (of_device_is_compatible(np, "nvidia,tegra30-pcie"))
		pcie->num_supplies = 3;
	else if (of_device_is_compatible(np, "nvidia,tegra20-pcie"))
		pcie->num_supplies = 2;

	if (pcie->num_supplies == 0) {
		dev_err(dev, "device %pOF not supported in legacy mode\n", np);
		return -ENODEV;
	}

	pcie->supplies = devm_kcalloc(dev, pcie->num_supplies,
				      sizeof(*pcie->supplies),
				      GFP_KERNEL);
	if (!pcie->supplies)
		return -ENOMEM;

	pcie->supplies[0].supply = "pex-clk";
	pcie->supplies[1].supply = "vdd";

	if (pcie->num_supplies > 2)
		pcie->supplies[2].supply = "avdd";

	return devm_regulator_bulk_get(dev, pcie->num_supplies, pcie->supplies);
}

/*
 * Obtains the list of regulators required for a particular generation of the
 * IP block.
 *
 * This would've been nice to do simply by providing static tables for use
 * with the regulator_bulk_*() API, but unfortunately Tegra30 is a bit quirky
 * in that it has two pairs or AVDD_PEX and VDD_PEX supplies (PEXA and PEXB)
 * and either seems to be optional depending on which ports are being used.
 */
static int tegra_pcie_get_regulators(struct tegra_pcie *pcie, u32 lane_mask)
{
	struct device *dev = pcie->dev;
	struct device_node *np = dev->of_node;
	unsigned int i = 0;

	if (of_device_is_compatible(np, "nvidia,tegra186-pcie")) {
		pcie->num_supplies = 4;

		pcie->supplies = devm_kcalloc(pcie->dev, pcie->num_supplies,
					      sizeof(*pcie->supplies),
					      GFP_KERNEL);
		if (!pcie->supplies)
			return -ENOMEM;

		pcie->supplies[i++].supply = "dvdd-pex";
		pcie->supplies[i++].supply = "hvdd-pex-pll";
		pcie->supplies[i++].supply = "hvdd-pex";
		pcie->supplies[i++].supply = "vddio-pexctl-aud";
	} else if (of_device_is_compatible(np, "nvidia,tegra210-pcie")) {
		pcie->num_supplies = 3;

		pcie->supplies = devm_kcalloc(pcie->dev, pcie->num_supplies,
					      sizeof(*pcie->supplies),
					      GFP_KERNEL);
		if (!pcie->supplies)
			return -ENOMEM;

		pcie->supplies[i++].supply = "hvddio-pex";
		pcie->supplies[i++].supply = "dvddio-pex";
		pcie->supplies[i++].supply = "vddio-pex-ctl";
	} else if (of_device_is_compatible(np, "nvidia,tegra124-pcie")) {
		pcie->num_supplies = 4;

		pcie->supplies = devm_kcalloc(dev, pcie->num_supplies,
					      sizeof(*pcie->supplies),
					      GFP_KERNEL);
		if (!pcie->supplies)
			return -ENOMEM;

		pcie->supplies[i++].supply = "avddio-pex";
		pcie->supplies[i++].supply = "dvddio-pex";
		pcie->supplies[i++].supply = "hvdd-pex";
		pcie->supplies[i++].supply = "vddio-pex-ctl";
	} else if (of_device_is_compatible(np, "nvidia,tegra30-pcie")) {
		bool need_pexa = false, need_pexb = false;

		/* VDD_PEXA and AVDD_PEXA supply lanes 0 to 3 */
		if (lane_mask & 0x0f)
			need_pexa = true;

		/* VDD_PEXB and AVDD_PEXB supply lanes 4 to 5 */
		if (lane_mask & 0x30)
			need_pexb = true;

		pcie->num_supplies = 4 + (need_pexa ? 2 : 0) +
					 (need_pexb ? 2 : 0);

		pcie->supplies = devm_kcalloc(dev, pcie->num_supplies,
					      sizeof(*pcie->supplies),
					      GFP_KERNEL);
		if (!pcie->supplies)
			return -ENOMEM;

		pcie->supplies[i++].supply = "avdd-pex-pll";
		pcie->supplies[i++].supply = "hvdd-pex";
		pcie->supplies[i++].supply = "vddio-pex-ctl";
		pcie->supplies[i++].supply = "avdd-plle";

		if (need_pexa) {
			pcie->supplies[i++].supply = "avdd-pexa";
			pcie->supplies[i++].supply = "vdd-pexa";
		}

		if (need_pexb) {
			pcie->supplies[i++].supply = "avdd-pexb";
			pcie->supplies[i++].supply = "vdd-pexb";
		}
	} else if (of_device_is_compatible(np, "nvidia,tegra20-pcie")) {
		pcie->num_supplies = 5;

		pcie->supplies = devm_kcalloc(dev, pcie->num_supplies,
					      sizeof(*pcie->supplies),
					      GFP_KERNEL);
		if (!pcie->supplies)
			return -ENOMEM;

		pcie->supplies[0].supply = "avdd-pex";
		pcie->supplies[1].supply = "vdd-pex";
		pcie->supplies[2].supply = "avdd-pex-pll";
		pcie->supplies[3].supply = "avdd-plle";
		pcie->supplies[4].supply = "vddio-pex-clk";
	}

	if (of_regulator_bulk_available(dev->of_node, pcie->supplies,
					pcie->num_supplies))
		return devm_regulator_bulk_get(dev, pcie->num_supplies,
					       pcie->supplies);

	/*
	 * If not all regulators are available for this new scheme, assume
	 * that the device tree complies with an older version of the device
	 * tree binding.
	 */
	dev_info(dev, "using legacy DT binding for power supplies\n");

	devm_kfree(dev, pcie->supplies);
	pcie->num_supplies = 0;

	return tegra_pcie_get_legacy_regulators(pcie);
}

static int tegra_pcie_parse_dt(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct device_node *np = dev->of_node, *port;
	const struct tegra_pcie_soc *soc = pcie->soc;
	u32 lanes = 0, mask = 0;
	unsigned int lane = 0;
	int err;

	pcie->pex_wake = of_get_named_gpio(np, "nvidia,wake-gpio", 0);
	if (gpio_is_valid(pcie->pex_wake)) {
		err = devm_gpio_request(dev, pcie->pex_wake, "pex_wake");
		if (err < 0) {
			dev_err(dev, "pex_wake gpio request failed: %d\n", err);
			return err;
		}
		err = gpio_direction_input(pcie->pex_wake);
		if (err < 0) {
			dev_err(dev, "pex_wake set gpio dir input failed: %d\n",
				err);
			return err;
		}
	}

	/* parse root ports */
	for_each_child_of_node(np, port) {
		struct tegra_pcie_port *rp;
		unsigned int index;
		const char *type;
		u32 value;
		char *label;

		if (!of_property_read_string(port, "device_type", &type)) {
			if (strncmp(type, "pci", sizeof("pci")))
				continue;
		} else {
			continue;
		}

		err = of_pci_get_devfn(port);
		if (err < 0) {
			dev_err(dev, "failed to parse address: %d\n", err);
			goto err_node_put;
		}

		index = PCI_SLOT(err);

		if (index < 1 || index > soc->num_ports) {
			dev_err(dev, "invalid port number: %d\n", index);
			err = -EINVAL;
			goto err_node_put;
		}

		index--;

		err = of_property_read_u32(port, "nvidia,num-lanes", &value);
		if (err < 0) {
			dev_err(dev, "failed to parse # of lanes: %d\n",
				err);
			goto err_node_put;
		}

		if (value > 16) {
			dev_err(dev, "invalid # of lanes: %u\n", value);
			err = -EINVAL;
			goto err_node_put;
		}

		lanes |= value << (index << 3);

		if (!of_device_is_available(port)) {
			lane += value;
			continue;
		}

		mask |= ((1 << value) - 1) << lane;
		lane += value;

		rp = devm_kzalloc(dev, sizeof(*rp), GFP_KERNEL);
		if (!rp) {
			err = -ENOMEM;
			goto err_node_put;
		}

		err = of_address_to_resource(port, 0, &rp->regs);
		if (err < 0) {
			dev_err(dev, "failed to parse address: %d\n", err);
			goto err_node_put;
		}

		INIT_LIST_HEAD(&rp->list);
		rp->index = index;
		rp->lanes = value;
		rp->pcie = pcie;
		rp->np = port;

		rp->base = devm_pci_remap_cfg_resource(dev, &rp->regs);
		if (IS_ERR(rp->base)) {
			err = PTR_ERR(rp->base);
			goto err_node_put;
		}

		label = devm_kasprintf(dev, GFP_KERNEL, "pex-reset-%u", index);
		if (!label) {
			err = -ENOMEM;
			goto err_node_put;
		}

		/*
		 * Returns -ENOENT if reset-gpios property is not populated
		 * and in this case fall back to using AFI per port register
		 * to toggle PERST# SFIO line.
		 */
		rp->reset_gpio = devm_gpiod_get_from_of_node(dev, port,
							     "reset-gpios", 0,
							     GPIOD_OUT_LOW,
							     label);
		if (IS_ERR(rp->reset_gpio)) {
			if (PTR_ERR(rp->reset_gpio) == -ENOENT) {
				rp->reset_gpio = NULL;
			} else {
				dev_err(dev, "failed to get reset GPIO: %ld\n",
					PTR_ERR(rp->reset_gpio));
				err = PTR_ERR(rp->reset_gpio);
				goto err_node_put;
			}
		}

		rp->n_gpios = of_gpio_named_count(port, "nvidia,plat-gpios");
		if (rp->n_gpios > 0) {
			int count, gpio;
			enum of_gpio_flags flags;
			unsigned long f;

			rp->gpios = devm_kzalloc(dev, rp->n_gpios * sizeof(int),
						 GFP_KERNEL);
			if (!rp->gpios)
				return -ENOMEM;

			for (count = 0; count < rp->n_gpios; ++count) {
				gpio = of_get_named_gpio_flags(port,
							"nvidia,plat-gpios",
							count, &flags);
				if (!gpio_is_valid(gpio))
					return gpio;

				f = (flags & OF_GPIO_ACTIVE_LOW) ?
				    (GPIOF_OUT_INIT_LOW | GPIOF_ACTIVE_LOW) :
				    GPIOF_OUT_INIT_HIGH;

				err = devm_gpio_request_one(dev, gpio, f, NULL);
				if (err < 0) {
					dev_err(dev, "gpio %d request failed\n",
						gpio);
					return err;
				}
				rp->gpios[count] = gpio;
			}
		}

		rp->has_mxm_port = of_property_read_bool(port,
							 "nvidia,has-mxm-port");
		if (rp->has_mxm_port) {
			rp->pwr_gd_gpio = of_get_named_gpio(port,
						"nvidia,pwr-gd-gpio", 0);
			if (gpio_is_valid(rp->pwr_gd_gpio)) {
				err = devm_gpio_request(dev, rp->pwr_gd_gpio,
							"pwr_gd_gpio");
				if (err < 0) {
					dev_err(dev, "%s: pwr_gd_gpio request failed %d\n",
						__func__, err);
					return err;
				}

				err = gpio_direction_input(rp->pwr_gd_gpio);
				if (err < 0) {
					dev_err(dev, "%s: pwr_gd_gpio direction input failed %d\n",
						__func__, err);
				}
			}
		}

		err = of_property_read_u32(port, "nvidia,disable-aspm-states",
					   &rp->aspm_state);
		if (err < 0)
			rp->aspm_state = 0;

		rp->supports_clkreq = of_property_read_bool(port,
							    "supports-clkreq");

		list_add_tail(&rp->list, &pcie->ports);
	}

	err = tegra_pcie_get_xbar_config(pcie, lanes, &pcie->xbar_config);
	if (err < 0) {
		dev_err(dev, "invalid lane configuration\n");
		return err;
	}

	err = tegra_pcie_get_regulators(pcie, mask);
	if (err < 0)
		return err;

	return 0;

err_node_put:
	of_node_put(port);
	return err;
}

/*
 * FIXME: If there are no PCIe cards attached, then calling this function
 * can result in the increase of the bootup time as there are big timeout
 * loops.
 */
#define TEGRA_PCIE_LINKUP_TIMEOUT	200	/* up to 1.2 seconds */
static bool tegra_pcie_port_check_link(struct tegra_pcie_port *port)
{
	struct device *dev = port->pcie->dev;
	unsigned int retries = 3;
	unsigned long value;

	/* override presence detection */
	value = readl(port->base + RP_PRIV_MISC);
	value &= ~RP_PRIV_MISC_PRSNT_MAP_EP_ABSNT;
	value |= RP_PRIV_MISC_PRSNT_MAP_EP_PRSNT;
	writel(value, port->base + RP_PRIV_MISC);

	do {
		unsigned int timeout = TEGRA_PCIE_LINKUP_TIMEOUT;

		do {
			value = readl(port->base + RP_VEND_XP);

			if (value & RP_VEND_XP_DL_UP)
				break;

			usleep_range(1000, 2000);
		} while (--timeout);

		if (!timeout) {
			dev_dbg(dev, "link %u down, retrying\n", port->index);
			goto retry;
		}

		timeout = TEGRA_PCIE_LINKUP_TIMEOUT;

		do {
			value = readl(port->base + RP_LINK_CONTROL_STATUS);

			if (value & RP_LINK_CONTROL_STATUS_DL_LINK_ACTIVE)
				return true;

			usleep_range(1000, 2000);
		} while (--timeout);

retry:
		tegra_pcie_port_reset(port);
	} while (--retries);

	return false;
}

static void tegra_pcie_change_link_speed(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct tegra_pcie_port *port;
	ktime_t deadline;
	u32 value;

	list_for_each_entry(port, &pcie->ports, list) {
		/*
		 * "Supported Link Speeds Vector" in "Link Capabilities 2"
		 * is not supported by Tegra. tegra_pcie_change_link_speed()
		 * is called only for Tegra chips which support Gen2.
		 * So there no harm if supported link speed is not verified.
		 */
		value = readl(port->base + RP_LINK_CONTROL_STATUS_2);
		value &= ~PCI_EXP_LNKSTA_CLS;
		value |= PCI_EXP_LNKSTA_CLS_5_0GB;
		writel(value, port->base + RP_LINK_CONTROL_STATUS_2);

		/*
		 * Poll until link comes back from recovery to avoid race
		 * condition.
		 */
		deadline = ktime_add_us(ktime_get(), LINK_RETRAIN_TIMEOUT);

		while (ktime_before(ktime_get(), deadline)) {
			value = readl(port->base + RP_LINK_CONTROL_STATUS);
			if ((value & PCI_EXP_LNKSTA_LT) == 0)
				break;

			usleep_range(2000, 3000);
		}

		if (value & PCI_EXP_LNKSTA_LT)
			dev_warn(dev, "PCIe port %u link is in recovery\n",
				 port->index);

		/* Retrain the link */
		value = readl(port->base + RP_LINK_CONTROL_STATUS);
		value |= PCI_EXP_LNKCTL_RL;
		writel(value, port->base + RP_LINK_CONTROL_STATUS);

		deadline = ktime_add_us(ktime_get(), LINK_RETRAIN_TIMEOUT);

		while (ktime_before(ktime_get(), deadline)) {
			value = readl(port->base + RP_LINK_CONTROL_STATUS);
			if ((value & PCI_EXP_LNKSTA_LT) == 0)
				break;

			usleep_range(2000, 3000);
		}

		if (value & PCI_EXP_LNKSTA_LT)
			dev_err(dev, "failed to retrain link of port %u\n",
				port->index);
	}
}

static int tegra_pcie_scale_freq(struct tegra_pcie *pcie)
{
	struct tegra_pcie_port *port, *tmp;
	const struct tegra_pcie_soc *soc = pcie->soc;
	int err = 0;
	u32 val = 0;
	u32 active_lanes = 0;
	bool is_gen2 = false;

	list_for_each_entry_safe(port, tmp, &pcie->ports, list) {
		val = readl(port->base + RP_LINK_CONTROL_STATUS);
		active_lanes += ((val &
					RP_LINK_CONTROL_STATUS_NEG_LINK_WIDTH) >> 20);
		if (((val & RP_LINK_CONTROL_STATUS_LINK_SPEED) >> 16) == 2)
			is_gen2 = true;
	}

	if (soc->dvfs_mselect) {
		struct clk *mselect_clk;

		active_lanes = 0;
		dev_dbg(pcie->dev, "mselect_clk is set @ %u\n",
				soc->dfs_tbl[active_lanes][is_gen2].afi_clk);
		mselect_clk = devm_clk_get(pcie->dev, "mselect");
		if (IS_ERR(mselect_clk)) {
			dev_err(pcie->dev, "mselect clk_get failed: %ld\n",
					PTR_ERR(mselect_clk));
			return PTR_ERR(mselect_clk);
		}
		err = clk_set_rate(mselect_clk,
				soc->dfs_tbl[active_lanes][is_gen2].afi_clk);
		if (err) {
			dev_err(pcie->dev,
					"setting mselect clk to %u failed : %d\n",
					soc->dfs_tbl[active_lanes][is_gen2].afi_clk,
					err);
			return err;
		}
	}

	if (soc->dvfs_afi) {
		dev_dbg(pcie->dev, "afi_clk is set @ %u\n",
				soc->dfs_tbl[active_lanes][is_gen2].afi_clk);
		err = clk_set_rate(devm_clk_get(pcie->dev, "afi"),
				soc->dfs_tbl[active_lanes][is_gen2].afi_clk);
		if (err) {
			dev_err(pcie->dev,
					"setting afi clk to %u failed : %d\n",
					soc->dfs_tbl[active_lanes][is_gen2].afi_clk,
					err);
			return err;
		}
	}

	dev_dbg(pcie->dev, "emc_clk is set @ %u\n",
			soc->dfs_tbl[active_lanes][is_gen2].emc_clk);
#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	err = tegra_bwmgr_set_emc(pcie->emc_bwmgr,
			soc->dfs_tbl[active_lanes][is_gen2].emc_clk,
			TEGRA_BWMGR_SET_EMC_FLOOR);
	if (err < 0) {
		dev_err(pcie->dev, "setting emc clk to %u failed : %d\n",
				soc->dfs_tbl[active_lanes][is_gen2].emc_clk, err);
		return err;
	}
#endif

	return err;
}

static int tegra_pcie_mxm_pwr_init(struct tegra_pcie_port *port)
{
	mdelay(100);

	if (!gpio_get_value(port->pwr_gd_gpio))
		return 1;

	return 0;
}

static void tegra_pcie_enable_ports(struct tegra_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct tegra_pcie_port *port, *tmp;

	list_for_each_entry_safe(port, tmp, &pcie->ports, list) {
		dev_info(dev, "probing port %u, using %u lanes\n",
			 port->index, port->lanes);

		tegra_pcie_port_enable(port);
	}

	/* Start LTSSM from Tegra side */
	reset_control_deassert(pcie->pcie_xrst);

	list_for_each_entry_safe(port, tmp, &pcie->ports, list) {
		if (tegra_pcie_port_check_link(port))
			continue;

		dev_info(dev, "link %u down, ignoring\n", port->index);

		tegra_pcie_port_disable(port);
		tegra_pcie_port_free(port);
	}

	if (pcie->soc->has_gen2)
		tegra_pcie_change_link_speed(pcie);

	tegra_pcie_scale_freq(pcie);
}

static void tegra_pcie_disable_ports(struct tegra_pcie *pcie)
{
	struct tegra_pcie_port *port, *tmp;

	reset_control_assert(pcie->pcie_xrst);

	list_for_each_entry_safe(port, tmp, &pcie->ports, list)
		tegra_pcie_port_disable(port);
}

static const struct tegra_pcie_port_soc tegra20_pcie_ports[] = {
	{ .pme.turnoff_bit = 0, .pme.ack_bit =  5 },
	{ .pme.turnoff_bit = 8, .pme.ack_bit = 10 },
};

static const struct tegra_pcie_soc tegra20_pcie = {
	.num_ports = 2,
	.ports = tegra20_pcie_ports,
	.msi_base_shift = 0,
	.pads_pll_ctl = PADS_PLL_CTL_TEGRA20,
	.tx_ref_sel = PADS_PLL_CTL_TXCLKREF_DIV10,
	.pads_refclk_cfg0 = 0xfa5cfa5c,
	.has_pex_clkreq_en = false,
	.has_pex_bias_ctrl = false,
	.has_intr_prsnt_sense = false,
	.has_cml_clk = false,
	.has_gen2 = false,
	.force_pca_enable = false,
	.program_uphy = true,
	.update_clamp_threshold = false,
	.program_deskew_time = false,
	.update_fc_timer = false,
	.has_cache_bars = true,
	.enable_wrap = false,
	.has_aspm_l1 = false,
	.has_aspm_l1ss = false,
	.l1ss_rp_wake_fixup = false,
	.dvfs_mselect = false,
	.dvfs_afi = false,
	.ectl.enable = false,
};

static const struct tegra_pcie_port_soc tegra30_pcie_ports[] = {
	{ .pme.turnoff_bit =  0, .pme.ack_bit =  5 },
	{ .pme.turnoff_bit =  8, .pme.ack_bit = 10 },
	{ .pme.turnoff_bit = 16, .pme.ack_bit = 18 },
};

static const struct tegra_pcie_soc tegra30_pcie = {
	.num_ports = 3,
	.ports = tegra30_pcie_ports,
	.msi_base_shift = 8,
	.afi_pex2_ctrl = 0x128,
	.pads_pll_ctl = PADS_PLL_CTL_TEGRA30,
	.tx_ref_sel = PADS_PLL_CTL_TXCLKREF_BUF_EN,
	.pads_refclk_cfg0 = 0xfa5cfa5c,
	.pads_refclk_cfg1 = 0xfa5cfa5c,
	.has_pex_clkreq_en = true,
	.has_pex_bias_ctrl = true,
	.has_intr_prsnt_sense = true,
	.has_cml_clk = true,
	.has_gen2 = false,
	.force_pca_enable = false,
	.program_uphy = true,
	.update_clamp_threshold = false,
	.program_deskew_time = false,
	.update_fc_timer = false,
	.has_cache_bars = false,
	.enable_wrap = false,
	.has_aspm_l1 = true,
	.has_aspm_l1ss = false,
	.l1ss_rp_wake_fixup = false,
	.dvfs_mselect = false,
	.dvfs_afi = false,
	.ectl.enable = false,
};

static const struct tegra_pcie_soc tegra124_pcie = {
	.num_ports = 2,
	.ports = tegra20_pcie_ports,
	.msi_base_shift = 8,
	.pads_pll_ctl = PADS_PLL_CTL_TEGRA30,
	.tx_ref_sel = PADS_PLL_CTL_TXCLKREF_BUF_EN,
	.pads_refclk_cfg0 = 0x44ac44ac,
	.has_pex_clkreq_en = true,
	.has_pex_bias_ctrl = true,
	.has_intr_prsnt_sense = true,
	.has_cml_clk = true,
	.has_gen2 = true,
	.force_pca_enable = false,
	.program_uphy = true,
	.update_clamp_threshold = true,
	.program_deskew_time = false,
	.update_fc_timer = false,
	.has_cache_bars = false,
	.enable_wrap = false,
	.has_aspm_l1 = true,
	.has_aspm_l1ss = false,
	.l1ss_rp_wake_fixup = false,
	.dvfs_mselect = false,
	.dvfs_afi = false,
	.ectl.enable = false,
};

static const struct tegra_pcie_soc tegra210_pcie = {
	.num_ports = 2,
	.ports = tegra20_pcie_ports,
	.msi_base_shift = 8,
	.pads_pll_ctl = PADS_PLL_CTL_TEGRA30,
	.tx_ref_sel = PADS_PLL_CTL_TXCLKREF_BUF_EN,
	.pads_refclk_cfg0 = 0x90b890b8,
	/* FC threshold is bit[25:18] */
	.update_fc_threshold = 0x01800000,
	.has_pex_clkreq_en = true,
	.has_pex_bias_ctrl = true,
	.has_intr_prsnt_sense = true,
	.has_cml_clk = true,
	.has_gen2 = true,
	.force_pca_enable = true,
	.program_uphy = true,
	.update_clamp_threshold = true,
	.program_deskew_time = true,
	.update_fc_timer = true,
	.has_cache_bars = false,
	.enable_wrap = true,
	.has_aspm_l1 = true,
	.has_aspm_l1ss = true,
	.l1ss_rp_wake_fixup = true,
	.dvfs_mselect = true,
	.dvfs_afi = false,
	.dfs_tbl = {
		{{204000000, 102000000}, {408000000, 528000000} } },
	.ectl = {
		.regs = {
			.rp_ectl_1_r1 = 0x0000001f,
			.rp_ectl_2_r1 = 0x0000000f,
			.rp_ectl_4_r1 = 0x00000067,
			.rp_ectl_5_r1 = 0x55010000,
			.rp_ectl_6_r1 = 0x00000001,
			.rp_ectl_1_r2 = 0x0000001f,
			.rp_ectl_2_r2 = 0x0000008f,
			.rp_ectl_4_r2 = 0x000000c7,
			.rp_ectl_5_r2 = 0x55010000,
			.rp_ectl_6_r2 = 0x00000001,
		},
		.enable = true,
	},
};

static const struct tegra_pcie_soc tegra210b01_pcie = {
	.num_ports = 2,
	.ports = tegra20_pcie_ports,
	.msi_base_shift = 8,
	.pads_pll_ctl = PADS_PLL_CTL_TEGRA30,
	.tx_ref_sel = PADS_PLL_CTL_TXCLKREF_BUF_EN,
	.pads_refclk_cfg0 = 0x90b890b8,
	/* FC threshold is bit[25:18] */
	.update_fc_threshold = 0x01800000,
	.has_pex_clkreq_en = true,
	.has_pex_bias_ctrl = true,
	.has_intr_prsnt_sense = true,
	.has_cml_clk = true,
	.has_gen2 = true,
	.force_pca_enable = true,
	.program_uphy = true,
	.update_clamp_threshold = false,
	.program_deskew_time = true,
	.update_fc_timer = true,
	.has_cache_bars = false,
	.enable_wrap = false,
	.has_aspm_l1 = true,
	.has_aspm_l1ss = true,
	.l1ss_rp_wake_fixup = true,
	.dvfs_mselect = true,
	.dvfs_afi = false,
	.dfs_tbl = {
		{{204000000, 102000000}, {408000000, 528000000} } },
	.ectl = {
		.regs = {
			.rp_ectl_1_r1 = 0x00000027,
			.rp_ectl_2_r1 = 0x0000000f,
			.rp_ectl_4_r1 = 0x00000067,
			.rp_ectl_5_r1 = 0x00000000,
			.rp_ectl_6_r1 = 0x00000000,
			.rp_ectl_1_r2 = 0x00000027,
			.rp_ectl_2_r2 = 0x0000008f,
			.rp_ectl_4_r2 = 0x000000c7,
			.rp_ectl_5_r2 = 0x00000000,
			.rp_ectl_6_r2 = 0x00000000,
		},
		.enable = true,
	},
};

static const struct tegra_pcie_port_soc tegra186_pcie_ports[] = {
	{ .pme.turnoff_bit =  0, .pme.ack_bit =  5 },
	{ .pme.turnoff_bit =  8, .pme.ack_bit = 10 },
	{ .pme.turnoff_bit = 12, .pme.ack_bit = 14 },
};

static const struct tegra_pcie_soc tegra186_pcie = {
	.num_ports = 3,
	.ports = tegra186_pcie_ports,
	.msi_base_shift = 8,
	.afi_pex2_ctrl = 0x19c,
	.pads_pll_ctl = PADS_PLL_CTL_TEGRA30,
	.tx_ref_sel = PADS_PLL_CTL_TXCLKREF_BUF_EN,
	.pads_refclk_cfg0 = 0x80b880b8,
	.pads_refclk_cfg1 = 0x000480b8,
	.has_pex_clkreq_en = true,
	.has_pex_bias_ctrl = true,
	.has_intr_prsnt_sense = true,
	.has_cml_clk = false,
	.has_gen2 = true,
	.force_pca_enable = false,
	.program_uphy = false,
	.update_clamp_threshold = false,
	.program_deskew_time = false,
	.update_fc_timer = false,
	.has_cache_bars = false,
	.enable_wrap = false,
	.has_aspm_l1 = true,
	.has_aspm_l1ss = true,
	.l1ss_rp_wake_fixup = false,
	.dvfs_mselect = false,
	.dvfs_afi = true,
	.dfs_tbl = {
		{{0, 0}, {0, 0} },
		{{102000000, 480000000}, {102000000, 480000000} },
		{{102000000, 480000000}, {204000000, 480000000} },
		{{102000000, 480000000}, {204000000, 480000000} },
		{{204000000, 480000000}, {408000000, 480000000} },
		{{204000000, 480000000}, {408000000, 640000000} } },
	.ectl.enable = false,
};

static const struct of_device_id tegra_pcie_of_match[] = {
	{ .compatible = "nvidia,tegra186-pcie", .data = &tegra186_pcie },
	{ .compatible = "nvidia,tegra210b01-pcie", .data = &tegra210b01_pcie },
	{ .compatible = "nvidia,tegra210-pcie", .data = &tegra210_pcie },
	{ .compatible = "nvidia,tegra124-pcie", .data = &tegra124_pcie },
	{ .compatible = "nvidia,tegra30-pcie", .data = &tegra30_pcie },
	{ .compatible = "nvidia,tegra20-pcie", .data = &tegra20_pcie },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_pcie_of_match);

static int list_devices(struct seq_file *s, void *data)
{
	struct pci_dev *pdev = NULL;
	u16 vendor, device, devclass, speed;
	bool pass = false;
	int ret = 0;

	for_each_pci_dev(pdev) {
		pass = true;
		ret = pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
		if (ret) {
			pass = false;
			break;
		}
		ret = pci_read_config_word(pdev, PCI_DEVICE_ID, &device);
		if (ret) {
			pass = false;
			break;
		}
		ret = pci_read_config_word(pdev, PCI_CLASS_DEVICE, &devclass);
		if (ret) {
			pass = false;
			break;
		}
		pcie_capability_read_word(pdev, PCI_EXP_LNKSTA, &speed);

		seq_printf(s, "%s  Vendor:%04x  Device id:%04x  ",
				kobject_name(&pdev->dev.kobj), vendor,
				device);
		seq_printf(s, "Class:%04x  Speed:%s  Driver:%s(%s)\n", devclass,
				((speed & PCI_EXP_LNKSTA_CLS_5_0GB) ==
				 PCI_EXP_LNKSTA_CLS_5_0GB) ?
				"Gen2" : "Gen1",
				(pdev->driver) ? "enabled" : "disabled",
				(pdev->driver) ? pdev->driver->name : NULL);
	}
	if (!pass)
		seq_puts(s, "Couldn't read devices\n");

	return ret;
}

static void tegra_pcie_link_speed(struct tegra_pcie *pcie, bool is_gen2)
{
	struct device *dev = pcie->dev;
	struct tegra_pcie_port *port, *tmp;
	ktime_t deadline;
	unsigned long val;

	list_for_each_entry_safe(port, tmp, &pcie->ports, list) {
		/*
		 * Link Capabilities 2 register is hardwired to 0 in Tegra,
		 * so no need to read it before setting target speed.
		 */
		val = readl(port->base + RP_LINK_CONTROL_STATUS_2);
		val &= ~PCI_EXP_LNKSTA_CLS;
		if (is_gen2)
			val |= PCI_EXP_LNKSTA_CLS_5_0GB;
		else
			val |= PCI_EXP_LNKSTA_CLS_2_5GB;
		writel(val, port->base + RP_LINK_CONTROL_STATUS_2);

		/*
		 * Poll until link comes back from recovery to avoid race
		 * condition.
		 */
		deadline = ktime_add_us(ktime_get(), LINK_RETRAIN_TIMEOUT);
		for (;;) {
			val = readl(port->base + RP_LINK_CONTROL_STATUS);
			if (!(val & PCI_EXP_LNKSTA_LT))
				break;
			if (ktime_after(ktime_get(), deadline))
				break;
			usleep_range(2000, 3000);
		}
		if (val & PCI_EXP_LNKSTA_LT)
			dev_err(dev, "PCIe port %u link is still in recovery\n",
					port->index);

		/* Retrain the link */
		val = readl(port->base + RP_LINK_CONTROL_STATUS);
		val |= PCI_EXP_LNKCTL_RL;
		writel(val, port->base + RP_LINK_CONTROL_STATUS);

		deadline = ktime_add_us(ktime_get(), LINK_RETRAIN_TIMEOUT);
		for (;;) {
			val = readl(port->base + RP_LINK_CONTROL_STATUS);
			if (!(val & PCI_EXP_LNKSTA_LT))
				break;
			if (ktime_after(ktime_get(), deadline))
				break;
			usleep_range(2000, 3000);
		}
		if (val & PCI_EXP_LNKSTA_LT)
			dev_err(dev, "link retrain of PCIe port %u failed\n",
					port->index);
	}
}

static int apply_link_speed(struct seq_file *s, void *data)
{
	struct tegra_pcie *pcie = (struct tegra_pcie *)(s->private);

	seq_printf(s, "Changing link speed to %s... ",
			(is_gen2_speed) ? "Gen2" : "Gen1");
	tegra_pcie_link_speed(pcie, is_gen2_speed);
	seq_puts(s, "Done\n");
	return 0;
}

static int check_d3hot(struct seq_file *s, void *data)
{
	u16 val;
	struct pci_dev *pdev = NULL;

	/* Force all the devices (including RPs) in d3 hot state */
	for_each_pci_dev(pdev) {
		if (pci_pcie_type(pdev) == PCI_EXP_TYPE_ROOT_PORT ||
				pci_pcie_type(pdev) == PCI_EXP_TYPE_DOWNSTREAM)
			continue;
		/* First, keep Downstream component in D3_Hot */
		pci_read_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL,
				&val);
		if ((val & PCI_PM_CTRL_STATE_MASK) == (__force u16)PCI_D3hot)
			seq_printf(s, "device[%x:%x] is already in D3_hot]\n",
					pdev->vendor, pdev->device);
		val &= ~PCI_PM_CTRL_STATE_MASK;
		val |= (__force u16)PCI_D3hot;
		pci_write_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, val);
		/* Keep corresponding upstream component in D3_Hot */
		pci_read_config_word(pdev->bus->self,
				pdev->bus->self->pm_cap + PCI_PM_CTRL,
				&val);
		val &= ~PCI_PM_CTRL_STATE_MASK;
		val |= (__force u16)PCI_D3hot;
		pci_write_config_word(pdev->bus->self,
				pdev->bus->self->pm_cap + PCI_PM_CTRL,
				val);
		mdelay(100);
		/* check if they have changed their state */
		pci_read_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, &val);
		if ((val & PCI_PM_CTRL_STATE_MASK) == (__force u16)PCI_D3hot)
			seq_printf(s, "device[%x:%x] transitioned to D3_hot]\n",
					pdev->vendor, pdev->device);
		else
			seq_printf(s, "device[%x:%x] couldn't transition to D3_hot]\n",
					pdev->vendor, pdev->device);
		pci_read_config_word(pdev->bus->self,
				pdev->bus->self->pm_cap + PCI_PM_CTRL,
				&val);
		if ((val & PCI_PM_CTRL_STATE_MASK) == (__force u16)PCI_D3hot)
			seq_printf(s, "device[%x:%x] transitioned to D3_hot]\n",
					pdev->bus->self->vendor,
					pdev->bus->self->device);
		else
			seq_printf(s, "device[%x:%x] couldn't transition to D3_hot]\n",
					pdev->bus->self->vendor,
					pdev->bus->self->device);
	}

	return 0;
}

static int dump_config_space(struct seq_file *s, void *data)
{
	u8 val;
	int row, col;
	struct pci_dev *pdev = NULL;

	for_each_pci_dev(pdev) {
		int row_cnt = pci_is_pcie(pdev) ?
			PCI_EXT_CFG_SPACE_SIZE : PCI_CFG_SPACE_SIZE;
		seq_printf(s, "%s\n", kobject_name(&pdev->dev.kobj));
		seq_printf(s, "%s\n", "------------");

		for (row = 0; row < (row_cnt / 16); row++) {
			seq_printf(s, "%02x: ", (row * 16));
			for (col = 0; col < 16; col++) {
				pci_read_config_byte(pdev, ((row * 16) + col),
						&val);
				seq_printf(s, "%02x ", val);
			}
			seq_puts(s, "\n");
		}
	}
	return 0;
}

static int dump_afi_space(struct seq_file *s, void *data)
{
	u32 val, offset;
	struct tegra_pcie_port *port = NULL;
	struct tegra_pcie *pcie = (struct tegra_pcie *)(s->private);

	list_for_each_entry(port, &pcie->ports, list) {
		seq_puts(s, "Offset:  Values\n");
		for (offset = 0; offset < 0x200; offset += 0x10) {
			val = afi_readl(port->pcie, offset);
			seq_printf(s, "%6x: %8x %8x %8x %8x\n", offset,
					afi_readl(port->pcie, offset),
					afi_readl(port->pcie, offset + 4),
					afi_readl(port->pcie, offset + 8),
					afi_readl(port->pcie, offset + 12));
		}
	}
	return 0;
}

static int config_read(struct seq_file *s, void *data)
{
	struct pci_dev *pdev = NULL;

	pdev = pci_get_domain_bus_and_slot(0, (bdf >> 8), (bdf & 0xFF));
	if (!pdev) {
		seq_printf(s, "%02d:%02d.%02d : Doesn't exist\n",
				(bdf >> 8), PCI_SLOT(bdf), PCI_FUNC(bdf));
		seq_puts(s, "Enter (bus<<8 | dev<<3 | fn) val to bdf file\n");
		goto end;
	}
	if (config_offset >= PCI_EXT_CFG_SPACE_SIZE) {
		seq_printf(s, "Config offset exceeds max (i.e %d) value\n",
				PCI_EXT_CFG_SPACE_SIZE);
	}
	if (!(config_offset & 0x3)) {
		u32 val;
		/* read 32 */
		pci_read_config_dword(pdev, config_offset, &val);
		seq_printf(s, "%08x\n", val);
		config_val = val;
	} else if (!(config_offset & 0x1)) {
		u16 val;
		/* read 16 */
		pci_read_config_word(pdev, config_offset, &val);
		seq_printf(s, "%04x\n", val);
		config_val = val;
	} else {
		u8 val;
		/* read 8 */
		pci_read_config_byte(pdev, config_offset, &val);
		seq_printf(s, "%02x\n", val);
		config_val = val;
	}

end:
	return 0;
}

static int config_write(struct seq_file *s, void *data)
{
	struct pci_dev *pdev = NULL;

	pdev = pci_get_domain_bus_and_slot(0, (bdf >> 8), (bdf & 0xFF));
	if (!pdev) {
		seq_printf(s, "%02d:%02d.%02d : Doesn't exist\n",
				(bdf >> 8), PCI_SLOT(bdf), PCI_FUNC(bdf));
		seq_puts(s, "Enter (bus<<8 | dev<<3 | fn) val to bdf file\n");
		goto end;
	}
	if (config_offset >= PCI_EXT_CFG_SPACE_SIZE) {
		seq_printf(s, "Config offset exceeds max (i.e %d) value\n",
				PCI_EXT_CFG_SPACE_SIZE);
	}
	if (!(config_offset & 0x3)) {
		/* write 32 */
		pci_write_config_dword(pdev, config_offset, config_val);
	} else if (!(config_offset & 0x1)) {
		/* write 16 */
		pci_write_config_word(pdev, config_offset,
				(u16)(config_val & 0xFFFF));
	} else {
		/* write 8 */
		pci_write_config_byte(pdev, config_offset,
				(u8)(config_val & 0xFF));
	}

end:
	return 0;
}

static int power_down(struct seq_file *s, void *data)
{
	struct tegra_pcie_port *port = NULL;
	struct tegra_pcie *pcie = (struct tegra_pcie *)(s->private);
	const struct tegra_pcie_soc *soc = pcie->soc;
	bool pass = true;
	u8 ack_bit;
	u32 val;
	int err;

	list_for_each_entry(port, &pcie->ports, list) {
		val = afi_readl(pcie, AFI_PCIE_PME);
		val |= (0x1 << soc->ports[port->index].pme.turnoff_bit);
		afi_writel(pcie, val, AFI_PCIE_PME);

		ack_bit = soc->ports[port->index].pme.ack_bit;
		err = readl_poll_timeout(pcie->afi + AFI_PCIE_PME, val,
					 val & (0x1 << ack_bit), 1,
					 PME_ACK_TIMEOUT);
		if (err)
			dev_err(pcie->dev, "PME Ack is not received on port: %d\n",
				port->index);

		usleep_range(10000, 11000);

		val = afi_readl(pcie, AFI_PCIE_PME);
		val &= ~(0x1 << soc->ports[port->index].pme.turnoff_bit);
		afi_writel(pcie, val, AFI_PCIE_PME);

		mdelay(1000);

		val = readl(port->base + RP_LTSSM_DBGREG);
		if (!(val & RP_LTSSM_DBGREG_LINKFSM16)) {
			pass = false;
			goto out;
		}
	}

out:
	if (pass)
		seq_puts(s, "[pass: pcie_power_down]\n");
	else
		seq_puts(s, "[fail: pcie_power_down]\n");
	pr_info("PCIE power down test END..\n");
	return 0;
}

static int loopback(struct seq_file *s, void *data)
{
	struct tegra_pcie_port *port = (struct tegra_pcie_port *)(s->private);
	unsigned int new, i, val;

	new = readl(port->base + RP_LINK_CONTROL_STATUS);

	if (!(new & RP_LINK_CONTROL_STATUS_DL_LINK_ACTIVE)) {
		pr_info("PCIE port %d not active\n", port->index);
		return -EINVAL;
	}

	/* trigger trace ram on loopback states */
	val = LTSSM_TRACE_CONTROL_CLEAR_STORE_EN |
		LTSSM_TRACE_CONTROL_TRIG_ON_EVENT |
		(0x08 << LTSSM_TRACE_CONTROL_TRIG_LTSSM_MAJOR_OFFSET) |
		(0x00 << LTSSM_TRACE_CONTROL_TRIG_PTX_LTSSM_MINOR_OFFSET) |
		(0x00 << LTSSM_TRACE_CONTROL_TRIG_PRX_LTSSM_MAJOR_OFFSET);
	writel(val, port->base + RP_LTSSM_TRACE_CONTROL);

	/* clear trace ram */
	val = readl(port->base + RP_LTSSM_TRACE_CONTROL);
	val |= LTSSM_TRACE_CONTROL_CLEAR_RAM;
	writel(val, port->base + RP_LTSSM_TRACE_CONTROL);
	val &= ~LTSSM_TRACE_CONTROL_CLEAR_RAM;
	writel(val, port->base + RP_LTSSM_TRACE_CONTROL);

	/* reset and clear status */
	port->loopback_stat = 0;

	new = readl(port->base + RP_VEND_XP);
	new &= ~RP_VEND_XP_PRBS_EN;
	writel(new, port->base + RP_VEND_XP);

	new = readl(port->base + RP_XP_CTL_1);
	new &= ~RP_XP_CTL_1_OLD_IOBIST_EN;
	writel(new, port->base + RP_XP_CTL_1);

	writel(0x10000001, port->base + RP_VEND_XP_BIST);
	writel(0, port->base + RP_PRBS);

	mdelay(1);

	writel(0x90820001, port->base + RP_VEND_XP_BIST);
	new = readl(port->base + RP_VEND_XP_BIST);

	new = readl(port->base + RP_XP_CTL_1);
	new |= RP_XP_CTL_1_OLD_IOBIST_EN;
	writel(new, port->base + RP_XP_CTL_1);

	new = readl(port->base + RP_VEND_XP);
	new |= RP_VEND_XP_PRBS_EN;
	writel(new, port->base + RP_VEND_XP);

	mdelay(1000);

	new = readl(port->base + RP_VEND_XP);
	port->loopback_stat = (new & RP_VEND_XP_PRBS_STAT) >> 2;
	pr_info("--- loopback status ---\n");
	for (i = 0; i < port->lanes; ++i)
		pr_info("@lane %d: %s\n", i,
				(port->loopback_stat & 0x01 << i) ? "pass" : "fail");

	new = readl(port->base + RP_PRBS);
	pr_info("--- PRBS pattern locked ---\n");
	for (i = 0; i < port->lanes; ++i)
		pr_info("@lane %d: %s\n", i,
				(new >> 16 & 0x01 << i) ? "Y" : "N");
	pr_info("--- err overflow bits ---\n");
	for (i = 0; i < port->lanes; ++i)
		pr_info("@lane %d: %s\n", i,
				((new & 0xffff) & 0x01 << i) ? "Y" : "N");

	new = readl(port->base + RP_XP_CTL_1);
	new &= ~RP_XP_CTL_1_OLD_IOBIST_EN;
	writel(new, port->base + RP_XP_CTL_1);

	pr_info("--- err counts ---\n");
	for (i = 0; i < port->lanes; ++i) {
		writel(i, port->base + RP_LANE_PRBS_ERR_COUNT);
		new = readl(port->base + RP_LANE_PRBS_ERR_COUNT);
		pr_info("@lane %d: %u\n", i, new >> 16);
	}

	writel(0x90000001, port->base + RP_VEND_XP_BIST);

	new = readl(port->base + RP_VEND_XP);
	new &= ~RP_VEND_XP_PRBS_EN;
	writel(new, port->base + RP_VEND_XP);

	mdelay(1);

	writel(0x92000001, port->base + RP_VEND_XP_BIST);
	writel(0x90000001, port->base + RP_VEND_XP_BIST);
	pr_info("pcie loopback test is done\n");

	return 0;
}

static int apply_lane_width(struct seq_file *s, void *data)
{
	unsigned int new;
	struct tegra_pcie_port *port = (struct tegra_pcie_port *)(s->private);

	if (port->lanes > 0x10) {
		seq_puts(s, "link width cannot be grater than 16\n");
		new = readl(port->base + RP_LINK_CONTROL_STATUS);
		port->lanes = (new &
				RP_LINK_CONTROL_STATUS_NEG_LINK_WIDTH) >> 20;
		return 0;
	}
	new = readl(port->base + RP_VEND_XP1);
	new &= ~RP_VEND_XP1_RNCTRL_MAXWIDTH_MASK;
	new |= port->lanes | RP_VEND_XP1_RNCTRL_EN;
	writel(new, port->base + RP_VEND_XP1);
	mdelay(1);

	new = readl(port->base + RP_LINK_CONTROL_STATUS);
	new = (new & RP_LINK_CONTROL_STATUS_NEG_LINK_WIDTH) >> 20;
	if (new != port->lanes)
		seq_printf(s, "can't set link width %u, falling back to %u\n",
				port->lanes, new);
	else
		seq_printf(s, "lane width %d applied\n", new);
	port->lanes = new;
	return 0;
}

static int aspm_state_cnt(struct seq_file *s, void *data)
{
	u32 val, cs;
	struct tegra_pcie_port *port = (struct tegra_pcie_port *)(s->private);

	cs = readl(port->base + RP_LINK_CONTROL_STATUS);
	/* check if L0s is enabled on this port */
	if (cs & RP_LINK_CONTROL_STATUS_L0s_ENABLED) {
		val = readl(port->base + RP_PRIV_XP_TX_L0S_ENTRY_COUNT);
		seq_printf(s, "Tx L0s entry count : %u\n", val);
	} else {
		seq_printf(s, "Tx L0s entry count : %s\n", "disabled");
	}

	val = readl(port->base + RP_PRIV_XP_RX_L0S_ENTRY_COUNT);
	seq_printf(s, "Rx L0s entry count : %u\n", val);

	/* check if L1 is enabled on this port */
	if (cs & RP_LINK_CONTROL_STATUS_L1_ENABLED) {
		val = readl(port->base + RP_PRIV_XP_TX_L1_ENTRY_COUNT);
		seq_printf(s, "Link L1 entry count : %u\n", val);
	} else {
		seq_printf(s, "Link L1 entry count : %s\n", "disabled");
	}

	cs = readl(port->base + RP_L1_PM_SS_CONTROL);
	/*
	 * Resetting the count value is not possible by any means
	 * because of HW Bug : 200034278
	 */
	/* check if L1.1 is enabled */
	if (cs & RP_L1_PM_SS_CONTROL_ASPM_L11_ENABLE) {
		val = readl(port->base + RP_L1_1_ENTRY_COUNT);
		val |= RP_L1_1_ENTRY_COUNT_RESET;
		writel(val, port->base + RP_L1_1_ENTRY_COUNT);
		seq_printf(s, "Link L1.1 entry count : %u\n", (val & 0xFFFF));
	} else {
		seq_printf(s, "Link L1.1 entry count : %s\n", "disabled");
	}
	/* check if L1.2 is enabled */
	if (cs & RP_L1_PM_SS_CONTROL_ASPM_L12_ENABLE) {
		val = readl(port->base + RP_L1_2_ENTRY_COUNT);
		val |= RP_L1_2_ENTRY_COUNT_RESET;
		writel(val, port->base + RP_L1_2_ENTRY_COUNT);
		seq_printf(s, "Link L1.2 entry count : %u\n", (val & 0xFFFF));
	} else {
		seq_printf(s, "Link L1.2 entry count : %s\n", "disabled");
	}

	return 0;
}

static char *aspm_states[] = {
	"Tx-L0s",
	"Rx-L0s",
	"L1",
	"IDLE ((Tx-L0s && Rx-L0s) + L1)"
};

static int list_aspm_states(struct seq_file *s, void *data)
{
	u32 i = 0;

	seq_puts(s, "----------------------------------------------------\n");
	seq_puts(s, "Note: Duration of link's residency is calcualated\n");
	seq_puts(s, "      only for one of the ASPM states at a time\n");
	seq_puts(s, "----------------------------------------------------\n");
	seq_puts(s, "write(echo) number from below table corresponding to\n");
	seq_puts(s, "one of the ASPM states for which link duration needs\n");
	seq_puts(s, "to be calculated to 'config_aspm_state'\n");
	seq_puts(s, "-----------------\n");
	for (i = 0; i < ARRAY_SIZE(aspm_states); i++)
		seq_printf(s, "%d : %s\n", i, aspm_states[i]);
	seq_puts(s, "-----------------\n");
	return 0;
}

static int apply_aspm_state(struct seq_file *s, void *data)
{
	u32 val;
	struct tegra_pcie_port *port = (struct tegra_pcie_port *)(s->private);

	if (config_aspm_state >= ARRAY_SIZE(aspm_states)) {
		seq_printf(s, "Invalid ASPM state : %u\n", config_aspm_state);
		list_aspm_states(s, data);
	} else {
		val = readl(port->base + RP_PRIV_XP_CONFIG);
		val &= ~RP_PRIV_XP_CONFIG_LOW_PWR_DURATION_MASK;
		val |= config_aspm_state;
		writel(val, port->base + RP_PRIV_XP_CONFIG);
		seq_printf(s, "Configured for ASPM-%s state...\n",
				aspm_states[config_aspm_state]);
	}
	return 0;
}

static int get_aspm_duration(struct seq_file *s, void *data)
{
	u32 val;
	struct tegra_pcie_port *port = (struct tegra_pcie_port *)(s->private);

	val = readl(port->base + RP_PRIV_XP_DURATION_IN_LOW_PWR_100NS);
	/* 52.08 = 1000 / 19.2MHz is rounded to 52      */
	seq_printf(s, "ASPM-%s duration = %d ns\n",
			aspm_states[config_aspm_state], (u32)((val * 100)/52));
	return 0;
}

static int secondary_bus_reset(struct seq_file *s, void *data)
{
	u32 val;
	struct tegra_pcie_port *port = (struct tegra_pcie_port *)(s->private);

	val = readl(port->base + RP_INTR_BCR);
	val |= RP_INTR_BCR_SB_RESET;
	writel(val, port->base + RP_INTR_BCR);
	usleep_range(10, 11);
	val = readl(port->base + RP_INTR_BCR);
	val &= ~RP_INTR_BCR_SB_RESET;
	writel(val, port->base + RP_INTR_BCR);

	seq_puts(s, "Secondary Bus Reset applied successfully...\n");
	return 0;
}

static void reset_l1ss_counter(struct tegra_pcie_port *port, u32 val,
		unsigned long offset)
{
	int c = 0;

	if ((val & 0xFFFF) == 0xFFFF) {
		pr_info(" Trying reset L1ss entry count to 0\n");
		while (val) {
			if (c++ > 50) {
				pr_info("Timeout: reset did not happen!\n");
				break;
			}
			val |= RP_L1_1_ENTRY_COUNT_RESET;
			writel(val, port->base + offset);
			mdelay(1);
			val = readl(port->base + offset);
		}
		if (!val)
			pr_info("L1ss entry count reset to 0\n");
	}
}

static int aspm_l11(struct seq_file *s, void *data)
{
	struct pci_dev *pdev = NULL;
	u32 val = 0, pos = 0;
	struct tegra_pcie_port *port = NULL;
	struct tegra_pcie *pcie = (struct tegra_pcie *)(s->private);

	pr_info("\nPCIE aspm l1.1 test START..\n");
	list_for_each_entry(port, &pcie->ports, list) {
		/* reset RP L1.1 counter */
		val = readl(port->base + RP_L1_1_ENTRY_COUNT);
		val |= RP_L1_1_ENTRY_COUNT_RESET;
		writel(val, port->base + RP_L1_1_ENTRY_COUNT);

		val = readl(port->base + RP_L1_1_ENTRY_COUNT);
		pr_info("L1.1 Entry count before %x\n", val);
		reset_l1ss_counter(port, val, RP_L1_1_ENTRY_COUNT);
	}
	/* disable automatic l1ss exit by gpu */
	for_each_pci_dev(pdev)
		if (pci_pcie_type(pdev) != PCI_EXP_TYPE_ROOT_PORT) {
			pci_write_config_dword(pdev, 0x658, 0);
			pci_write_config_dword(pdev, 0x150, 0xE0000015);
		}
	for_each_pci_dev(pdev) {
		u16 aspm;

		pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &aspm);
		aspm |= PCI_EXP_LNKCTL_ASPM_L1;
		pcie_capability_write_word(pdev, PCI_EXP_LNKCTL, aspm);
		pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
		pci_read_config_dword(pdev, pos + PCI_L1SS_CTL1, &val);
		val &= ~PCI_L1SS_CTL1_L1SS_MASK;
		val |= PCI_L1SS_CTL1_ASPM_L1_1;
		pci_write_config_dword(pdev, pos + PCI_L1SS_CTL1, val);
		if (pci_pcie_type(pdev) != PCI_EXP_TYPE_ROOT_PORT)
			break;
	}
	mdelay(2000);
	for_each_pci_dev(pdev) {
		pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
		pci_read_config_dword(pdev, pos + PCI_L1SS_CTL1, &val);
		if (pci_pcie_type(pdev) != PCI_EXP_TYPE_ROOT_PORT)
			break;
	}
	list_for_each_entry(port, &pcie->ports, list) {
		val = readl(port->base + RP_L1_1_ENTRY_COUNT);
		pr_info("L1.1 Entry count after %x\n", val);
	}

	pr_info("PCIE aspm l1.1 test END..\n");
	return 0;
}

static int aspm_l1ss(struct seq_file *s, void *data)
{
	struct pci_dev *pdev = NULL;
	u32 val = 0, pos = 0;
	struct tegra_pcie_port *port = NULL;
	struct tegra_pcie *pcie = (struct tegra_pcie *)(s->private);

	pr_info("\nPCIE aspm l1ss test START..\n");
	list_for_each_entry(port, &pcie->ports, list) {
		/* reset RP L1.1 L1.2 counters */
		val = readl(port->base + RP_L1_1_ENTRY_COUNT);
		val |= RP_L1_1_ENTRY_COUNT_RESET;
		writel(val, port->base + RP_L1_1_ENTRY_COUNT);
		val = readl(port->base + RP_L1_1_ENTRY_COUNT);
		pr_info("L1.1 Entry count before %x\n", val);
		reset_l1ss_counter(port, val, RP_L1_1_ENTRY_COUNT);

		val = readl(port->base + RP_L1_2_ENTRY_COUNT);
		val |= RP_L1_2_ENTRY_COUNT_RESET;
		writel(val, port->base + RP_L1_2_ENTRY_COUNT);
		val = readl(port->base + RP_L1_2_ENTRY_COUNT);
		pr_info("L1.2 Entry count before %x\n", val);
		reset_l1ss_counter(port, val, RP_L1_2_ENTRY_COUNT);
	}
	/* disable automatic l1ss exit by gpu */
	for_each_pci_dev(pdev)
		if (pci_pcie_type(pdev) != PCI_EXP_TYPE_ROOT_PORT) {
			pci_write_config_dword(pdev, 0x658, 0);
			pci_write_config_dword(pdev, 0x150, 0xE0000015);
		}

	for_each_pci_dev(pdev) {
		u16 aspm;

		pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &aspm);
		aspm |= PCI_EXP_LNKCTL_ASPM_L1;
		pcie_capability_write_word(pdev, PCI_EXP_LNKCTL, aspm);
		pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
		pci_read_config_dword(pdev, pos + PCI_L1SS_CTL1, &val);
		val &= ~PCI_L1SS_CTL1_L1SS_MASK;
		val |= (PCI_L1SS_CTL1_ASPM_L1_1 | PCI_L1SS_CTL1_ASPM_L1_2);
		pci_write_config_dword(pdev, pos + PCI_L1SS_CTL1, val);
		if (pci_pcie_type(pdev) != PCI_EXP_TYPE_ROOT_PORT)
			break;
	}
	mdelay(2000);
	for_each_pci_dev(pdev) {
		pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
		pci_read_config_dword(pdev, pos + PCI_L1SS_CTL1, &val);
		if (pci_pcie_type(pdev) != PCI_EXP_TYPE_ROOT_PORT)
			break;
	}
	list_for_each_entry(port, &pcie->ports, list) {
		u32 ltr_val;

		val = readl(port->base + RP_L1_1_ENTRY_COUNT);
		pr_info("L1.1 Entry count after %x\n", val);
		val = readl(port->base + RP_L1_2_ENTRY_COUNT);
		pr_info("L1.2 Entry count after %x\n", val);

		val = readl(port->base + RP_LTR_REP_VAL);
		pr_info("LTR reproted by EP %x\n", val);
		ltr_val = (val & 0x1FF) * (1 << (5 * ((val & 0x1C00) >> 10)));
		if (ltr_val > (106 * 1000)) {
			pr_info("EP's LTR = %u ns is > RP's threshold = %u ns\n",
					ltr_val, 106 * 1000);
			pr_info("Hence only L1.2 entry allowed\n");
		} else {
			pr_info("EP's LTR = %u ns is < RP's threshold = %u ns\n",
					ltr_val, 106 * 1000);
			pr_info("Hence only L1.1 entry allowed\n");
		}
	}

	pr_info("PCIE aspm l1ss test END..\n");
	return 0;
}

struct ltssm_major_state {
	const char *name;
	const char *minor[8];
};

struct ltssm_state {
	struct ltssm_major_state major[12];
};

static struct ltssm_state ltssm_state = {
	.major[0]  = {"detect", {"quiet", "active", "retry", "wait", "entry"} },
	.major[1]  = {"polling", {"active", "config", "idle", NULL,
		"compliance", "cspeed"} },
	.major[2]  = {"config", {"link start", "link accept", "lane accept",
		"lane wait", "idle", "pwrup", "complete"} },
	.major[3]  = {NULL, {NULL} },
	.major[4]  = {"l0", {"normal", "l0s entry", "l0s idle", "l0s wait",
		"l0s fts", "pwrup"} },
	.major[5]  = {"l1", {"entry", "waitrx", "idle", "wait", "pwrup",
		"beacon entry", "beacon exit"} },
	.major[6]  = {"l2", {"entry", "waitrx", "transmitwake", "idle"} },
	.major[7]  = {"recovery", {"rcvrlock", "rcvrcfg", "speed", "idle",
		NULL, NULL, NULL, "finish pkt"} },
	.major[8]  = {"loopback", {"entry", "active", "idle", "exit", "speed",
		"pre speed"} },
	.major[9]  = {"hotreset", {NULL} },
	.major[10] = {"disabled", {NULL} },
	.major[11] = {"txchar", {NULL} },
};

static const char *ltssm_get_major(unsigned int major)
{
	const char *state;

	state = ltssm_state.major[major].name;
	if (!state)
		return "unknown";
	return state;
}

static const char *ltssm_get_minor(unsigned int major, unsigned int minor)
{
	const char *state;

	state = ltssm_state.major[major].minor[minor];
	if (!state)
		return "unknown";
	return state;
}

static int dump_ltssm_trace(struct seq_file *s, void *data)
{
	struct tegra_pcie_port *port = (struct tegra_pcie_port *)(s->private);
	unsigned int val, ridx, widx, entries;

	seq_puts(s, "LTSSM trace dump:\n");
	val = readl(port->base + RP_LTSSM_TRACE_STATUS);
	widx = LTSSM_TRACE_STATUS_WRITE_POINTER(val);
	entries = LTSSM_TRACE_STATUS_RAM_FULL(val) ? 32 : widx;
	seq_printf(s, "LTSSM trace dump - %d entries:\n", entries);
	for (ridx = 0; ridx < entries; ridx++) {
		val = LTSSM_TRACE_STATUS_READ_ADDR(ridx);
		writel(val, port->base + RP_LTSSM_TRACE_STATUS);
		val = readl(port->base + RP_LTSSM_TRACE_STATUS);

		seq_printf(s, "[0x%08x] major: %-10s minor_tx: %-15s minor_rx: %s\n",
				val,
				ltssm_get_major(LTSSM_TRACE_STATUS_MAJOR(val)),
				ltssm_get_minor(LTSSM_TRACE_STATUS_MAJOR(val),
					LTSSM_TRACE_STATUS_PTX_MINOR(val)),
				ltssm_get_minor(LTSSM_TRACE_STATUS_MAJOR(val),
					LTSSM_TRACE_STATUS_PRX_MINOR(val)));
	}
	/* clear trace ram */
	val = readl(port->base + RP_LTSSM_TRACE_CONTROL);
	val |= LTSSM_TRACE_CONTROL_CLEAR_RAM;
	writel(val, port->base + RP_LTSSM_TRACE_CONTROL);
	val &= ~LTSSM_TRACE_CONTROL_CLEAR_RAM;
	writel(val, port->base + RP_LTSSM_TRACE_CONTROL);

	return 0;
}

static struct dentry *create_tegra_pcie_debufs_file(char *name,
		const struct file_operations *ops,
		struct dentry *parent,
		void *data)
{
	struct dentry *d;

	d = debugfs_create_file(name, 0444, parent, data, ops);
	if (!d)
		debugfs_remove_recursive(parent);

	return d;
}

#define DEFINE_ENTRY(__name)	\
static int __name ## _open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, __name, inode->i_private);		\
}									\
static const struct file_operations __name ## _fops = {		\
	.open		= __name ## _open,			\
	.read		= seq_read,				\
	.llseek		= seq_lseek,				\
	.release	= single_release,			\
};

/* common */
DEFINE_ENTRY(list_devices)
DEFINE_ENTRY(apply_link_speed)
DEFINE_ENTRY(check_d3hot)
DEFINE_ENTRY(dump_config_space)
DEFINE_ENTRY(dump_afi_space)
DEFINE_ENTRY(config_read)
DEFINE_ENTRY(config_write)
DEFINE_ENTRY(aspm_l11)
DEFINE_ENTRY(aspm_l1ss)
DEFINE_ENTRY(power_down)

/* Port specific */
DEFINE_ENTRY(loopback)
DEFINE_ENTRY(apply_lane_width)
DEFINE_ENTRY(aspm_state_cnt)
DEFINE_ENTRY(list_aspm_states)
DEFINE_ENTRY(apply_aspm_state)
DEFINE_ENTRY(get_aspm_duration)
DEFINE_ENTRY(secondary_bus_reset)
DEFINE_ENTRY(dump_ltssm_trace)

static int tegra_pcie_port_debugfs_init(struct tegra_pcie_port *port)
{
	struct dentry *d;
	char port_name[2] = {0};

	snprintf(port_name, sizeof(port_name), "%d", port->index);
	port->port_debugfs = debugfs_create_dir(port_name,
			port->pcie->debugfs);
	if (!port->port_debugfs)
		return -ENOMEM;

	debugfs_create_u32("lane_width", 0664, port->port_debugfs,
			&(port->lanes));
	debugfs_create_x32("loopback_status", 0664, port->port_debugfs,
			&(port->loopback_stat));

	d = debugfs_create_file("loopback", 0444, port->port_debugfs,
			(void *)port, &loopback_fops);
	if (!d)
		goto remove;

	d = debugfs_create_file("apply_lane_width", 0444, port->port_debugfs,
			(void *)port, &apply_lane_width_fops);
	if (!d)
		goto remove;

	d = debugfs_create_file("aspm_state_cnt", 0444, port->port_debugfs,
			(void *)port, &aspm_state_cnt_fops);
	if (!d)
		goto remove;

	debugfs_create_u16("config_aspm_state", 0664, port->port_debugfs,
			&config_aspm_state);

	d = debugfs_create_file("apply_aspm_state", 0444, port->port_debugfs,
			(void *)port, &apply_aspm_state_fops);
	if (!d)
		goto remove;

	d = debugfs_create_file("list_aspm_states", 0444, port->port_debugfs,
			(void *)port, &list_aspm_states_fops);
	if (!d)
		goto remove;

	d = debugfs_create_file("dump_ltssm_trace", 0444, port->port_debugfs,
			(void *)port, &dump_ltssm_trace_fops);
	if (!d)
		goto remove;

	d = debugfs_create_file("get_aspm_duration", 0444, port->port_debugfs,
			(void *)port, &get_aspm_duration_fops);
	if (!d)
		goto remove;

	d = debugfs_create_file("secondary_bus_reset", 0444, port->port_debugfs,
			(void *)port, &secondary_bus_reset_fops);
	if (!d)
		goto remove;

	return 0;

remove:
	debugfs_remove_recursive(port->port_debugfs);
	port->port_debugfs = NULL;
	return -ENOMEM;
}

static void *tegra_pcie_ports_seq_start(struct seq_file *s, loff_t *pos)
{
	struct tegra_pcie *pcie = s->private;

	if (list_empty(&pcie->ports))
		return NULL;

	seq_puts(s, "Index  Status\n");

	return seq_list_start(&pcie->ports, *pos);
}

static void *tegra_pcie_ports_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct tegra_pcie *pcie = s->private;

	return seq_list_next(v, &pcie->ports, pos);
}

static void tegra_pcie_ports_seq_stop(struct seq_file *s, void *v)
{
}

static int tegra_pcie_ports_seq_show(struct seq_file *s, void *v)
{
	bool up = false, active = false;
	struct tegra_pcie_port *port;
	unsigned int value;

	port = list_entry(v, struct tegra_pcie_port, list);

	value = readl(port->base + RP_VEND_XP);

	if (value & RP_VEND_XP_DL_UP)
		up = true;

	value = readl(port->base + RP_LINK_CONTROL_STATUS);

	if (value & RP_LINK_CONTROL_STATUS_DL_LINK_ACTIVE)
		active = true;

	seq_printf(s, "%2u     ", port->index);

	if (up)
		seq_puts(s, "up");

	if (active) {
		if (up)
			seq_puts(s, ", ");

		seq_puts(s, "active");
	}

	seq_puts(s, "\n");
	return 0;
}

static const struct seq_operations tegra_pcie_ports_sops = {
	.start = tegra_pcie_ports_seq_start,
	.next = tegra_pcie_ports_seq_next,
	.stop = tegra_pcie_ports_seq_stop,
	.show = tegra_pcie_ports_seq_show,
};

DEFINE_SEQ_ATTRIBUTE(tegra_pcie_ports);

static void tegra_pcie_debugfs_exit(struct tegra_pcie *pcie)
{
	debugfs_remove_recursive(pcie->debugfs);
	pcie->debugfs = NULL;
}

static int tegra_pcie_debugfs_init(struct tegra_pcie *pcie)
{
	struct dentry *d;
	struct tegra_pcie_port *port;

	pcie->debugfs = debugfs_create_dir("pcie", NULL);

	debugfs_create_file("ports", S_IFREG | S_IRUGO, pcie->debugfs, pcie,
			    &tegra_pcie_ports_fops);

	d = create_tegra_pcie_debufs_file("list_devices",
			&list_devices_fops, pcie->debugfs,
			(void *)pcie);
	if (!d)
		goto remove;

	d = debugfs_create_bool("is_gen2_speed(WO)", 0200, pcie->debugfs,
			&is_gen2_speed);
	if (!d)
		goto remove;

	d = create_tegra_pcie_debufs_file("apply_link_speed",
			&apply_link_speed_fops, pcie->debugfs,
			(void *)pcie);
	if (!d)
		goto remove;

	d = create_tegra_pcie_debufs_file("check_d3hot",
			&check_d3hot_fops, pcie->debugfs,
			(void *)pcie);
	if (!d)
		goto remove;

	d = create_tegra_pcie_debufs_file("power_down",
			&power_down_fops, pcie->debugfs,
			(void *)pcie);
	if (!d)
		goto remove;

	d = create_tegra_pcie_debufs_file("dump_config_space",
			&dump_config_space_fops,
			pcie->debugfs, (void *)pcie);
	if (!d)
		goto remove;

	d = create_tegra_pcie_debufs_file("dump_afi_space",
			&dump_afi_space_fops, pcie->debugfs,
			(void *)pcie);
	if (!d)
		goto remove;

	debugfs_create_u16("bus_dev_func", 0664, pcie->debugfs, &bdf);
	debugfs_create_u16("config_offset", 0664, pcie->debugfs,
			&config_offset);

	debugfs_create_u32("config_val", 0664, pcie->debugfs, &config_val);

	d = create_tegra_pcie_debufs_file("config_read", &config_read_fops,
			pcie->debugfs, (void *)pcie);
	if (!d)
		goto remove;

	d = create_tegra_pcie_debufs_file("config_write", &config_write_fops,
			pcie->debugfs, (void *)pcie);
	if (!d)
		goto remove;
	d = create_tegra_pcie_debufs_file("aspm_l11", &aspm_l11_fops,
			pcie->debugfs, (void *)pcie);
	if (!d)
		goto remove;
	d = create_tegra_pcie_debufs_file("aspm_l1ss", &aspm_l1ss_fops,
			pcie->debugfs, (void *)pcie);
	if (!d)
		goto remove;

	list_for_each_entry(port, &pcie->ports, list) {
		if (tegra_pcie_port_debugfs_init(port))
			goto remove;
	}

	return 0;

remove:
	tegra_pcie_debugfs_exit(pcie);
	return -ENOMEM;
}

static int tegra_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pci_host_bridge *host;
	struct tegra_pcie *pcie;
	struct tegra_pcie_port *port = NULL;
	int err;

	host = devm_pci_alloc_host_bridge(dev, sizeof(*pcie));
	if (!host)
		return -ENOMEM;

	pcie = pci_host_bridge_priv(host);
	host->sysdata = pcie;
	platform_set_drvdata(pdev, pcie);

	pcie->soc = of_device_get_match_data(dev);
	INIT_LIST_HEAD(&pcie->ports);
	pcie->dev = dev;

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	pcie->emc_bwmgr = tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_PCIE);
	if (!pcie->emc_bwmgr)
		dev_err(dev, "couldn't register with EMC BwMgr\n");
#endif

	err = tegra_pcie_parse_dt(pcie);
	if (err < 0)
		return err;

	err = tegra_pcie_get_resources(pcie);
	if (err < 0) {
		dev_err(dev, "failed to request resources: %d\n", err);
		return err;
	}

	err = tegra_pcie_msi_setup(pcie);
	if (err < 0) {
		dev_err(dev, "failed to enable MSI support: %d\n", err);
		goto put_resources;
	}

	list_for_each_entry(port, &pcie->ports, list)
		if (port->has_mxm_port && tegra_pcie_mxm_pwr_init(port))
			dev_info(dev, "pwr_good is down for port %d, ignoring\n",
				 port->index);

	pm_runtime_enable(pcie->dev);
	err = pm_runtime_get_sync(pcie->dev);
	if (err < 0) {
		dev_err(dev, "fail to enable pcie controller: %d\n", err);
		goto pm_runtime_put;
	}

	/*
	 * If all PCIe ports are down, power gate PCIe. This can happen if
	 * no endpoints are connected, so don't fail probe.
	 */
	port = NULL;
	err = -ENOMEDIUM;

	list_for_each_entry(port, &pcie->ports, list) {
		if (tegra_pcie_link_up(port)) {
			err = 0;
			break;
		}
	}

	if (err == -ENOMEDIUM) {
		err = 0;
		goto pm_runtime_put;
	}

	pci_add_flags(PCI_REASSIGN_ALL_BUS);

	host->ops = &tegra_pcie_ops;
	host->map_irq = tegra_pcie_map_irq;

	err = pci_host_probe(host);
	if (err < 0) {
		dev_err(dev, "failed to register host: %d\n", err);
		goto pm_runtime_put;
	}

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		tegra_pcie_debugfs_init(pcie);

	device_init_wakeup(dev, true);

	return 0;

pm_runtime_put:
	pm_runtime_put_sync(pcie->dev);
	pm_runtime_disable(pcie->dev);
	tegra_pcie_msi_teardown(pcie);
put_resources:
	tegra_pcie_put_resources(pcie);
	return err;
}

static int tegra_pcie_remove(struct platform_device *pdev)
{
	struct tegra_pcie *pcie = platform_get_drvdata(pdev);
	struct pci_host_bridge *host = pci_host_bridge_from_priv(pcie);
	struct tegra_pcie_port *port, *tmp;

	if (list_empty(&pcie->ports))
		return 0;

	device_init_wakeup(&pdev->dev, false);

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		tegra_pcie_debugfs_exit(pcie);

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	tegra_bwmgr_set_emc(pcie->emc_bwmgr, 0, TEGRA_BWMGR_SET_EMC_FLOOR);
	tegra_bwmgr_unregister(pcie->emc_bwmgr);
#endif

	pci_stop_root_bus(host->bus);
	pci_remove_root_bus(host->bus);
	pm_runtime_put_sync(pcie->dev);
	pm_runtime_disable(pcie->dev);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		tegra_pcie_msi_teardown(pcie);

	tegra_pcie_put_resources(pcie);

	list_for_each_entry_safe(port, tmp, &pcie->ports, list)
		tegra_pcie_port_free(port);

	return 0;
}

static int __maybe_unused tegra_pcie_pm_suspend(struct device *dev)
{
	struct tegra_pcie *pcie = dev_get_drvdata(dev);
	struct tegra_pcie_port *port;
	int err;

	if (list_empty(&pcie->ports))
		return 0;

	list_for_each_entry(port, &pcie->ports, list)
		tegra_pcie_pme_turnoff(port);

	tegra_pcie_disable_ports(pcie);

	/*
	 * AFI_INTR is unmasked in tegra_pcie_enable_controller(), mask it to
	 * avoid unwanted interrupts raised by AFI after pex_rst is asserted.
	 */
	tegra_pcie_disable_interrupts(pcie);

	if (pcie->soc->program_uphy) {
		err = tegra_pcie_phy_power_off(pcie);
		if (err < 0)
			dev_err(dev, "failed to power off PHY(s): %d\n", err);
	}

	reset_control_assert(pcie->pex_rst);
	clk_disable_unprepare(pcie->pex_clk);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		tegra_pcie_disable_msi(pcie);

	pinctrl_pm_select_idle_state(dev);
	tegra_pcie_power_off(pcie);
	tegra_pcie_config_plat(pcie, 0);

	return 0;
}

static int __maybe_unused tegra_pcie_pm_resume(struct device *dev)
{
	struct tegra_pcie *pcie = dev_get_drvdata(dev);
	int err;

	if (list_empty(&pcie->ports))
		return 0;

	tegra_pcie_config_plat(pcie, 1);

	err = tegra_pcie_power_on(pcie);
	if (err) {
		dev_err(dev, "tegra pcie power on fail: %d\n", err);
		return err;
	}

	err = pinctrl_pm_select_default_state(dev);
	if (err < 0) {
		dev_err(dev, "failed to disable PCIe IO DPD: %d\n", err);
		goto poweroff;
	}

	tegra_pcie_enable_controller(pcie);
	tegra_pcie_setup_translations(pcie);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		tegra_pcie_enable_msi(pcie);

	err = clk_prepare_enable(pcie->pex_clk);
	if (err) {
		dev_err(dev, "failed to enable PEX clock: %d\n", err);
		goto pex_dpd_enable;
	}

	reset_control_deassert(pcie->pex_rst);

	if (pcie->soc->program_uphy) {
		err = tegra_pcie_phy_power_on(pcie);
		if (err < 0) {
			dev_err(dev, "failed to power on PHY(s): %d\n", err);
			goto disable_pex_clk;
		}
	}

	tegra_pcie_apply_pad_settings(pcie);
	tegra_pcie_enable_ports(pcie);

	return 0;

disable_pex_clk:
	reset_control_assert(pcie->pex_rst);
	clk_disable_unprepare(pcie->pex_clk);
pex_dpd_enable:
	pinctrl_pm_select_idle_state(dev);
poweroff:
	tegra_pcie_power_off(pcie);

	return err;
}

static int tegra_pcie_suspend_late(struct device *dev)
{
	struct tegra_pcie *pcie = dev_get_drvdata(dev);

	if (list_empty(&pcie->ports))
		return 0;

	if (gpio_is_valid(pcie->pex_wake))
		enable_irq_wake(gpio_to_irq(pcie->pex_wake));

	return 0;
}

static int tegra_pcie_resume_early(struct device *dev)
{
	struct tegra_pcie *pcie = dev_get_drvdata(dev);

	if (list_empty(&pcie->ports))
		return 0;

	if (gpio_is_valid(pcie->pex_wake))
		disable_irq_wake(gpio_to_irq(pcie->pex_wake));

	return 0;
}

static const struct dev_pm_ops tegra_pcie_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra_pcie_pm_suspend, tegra_pcie_pm_resume, NULL)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(tegra_pcie_pm_suspend,
				      tegra_pcie_pm_resume)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(tegra_pcie_suspend_late,
				     tegra_pcie_resume_early)
};

static struct platform_driver tegra_pcie_driver = {
	.driver = {
		.name = "tegra-pcie",
		.of_match_table = tegra_pcie_of_match,
		.suppress_bind_attrs = true,
		.pm = &tegra_pcie_pm_ops,
	},
	.probe = tegra_pcie_probe,
	.remove = tegra_pcie_remove,
};
module_platform_driver(tegra_pcie_driver);
MODULE_LICENSE("GPL");
