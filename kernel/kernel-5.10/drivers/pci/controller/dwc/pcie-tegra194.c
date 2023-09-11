// SPDX-License-Identifier: GPL-2.0+
/*
 * PCIe host controller driver for Tegra194 SoC
 *
 * Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
 *
 * Author: Vidya Sagar <vidyas@nvidia.com>
 */

#include <linux/clk.h>
#include <linux/crc32.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/tegra-epl.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#if IS_ENABLED(CONFIG_INTERCONNECT)
#include <linux/interconnect.h>
#include <dt-bindings/interconnect/tegra_icc_id.h>
#endif
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/mmc/sdhci-tegra-notify.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/pcie_dma.h>
#include <linux/phy/phy.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/platform/tegra/mc_utils.h>
#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
#include <linux/platform/tegra/emc_bwmgr.h>
#include <linux/platform/tegra/bwmgr_mc.h>
#endif
#include <linux/pm_runtime.h>
#include <linux/random.h>
#include <linux/reset.h>
#include <linux/resource.h>
#include <linux/types.h>
#include "pcie-designware.h"
#include <soc/tegra/bpmp.h>
#include <soc/tegra/bpmp-abi.h>
#include <soc/tegra/fuse.h>
#include "../../pci.h"

#define TEGRA194_DWC_IP_VER			0x490A
#define TEGRA234_DWC_IP_VER			0x562A

#define APPL_PINMUX				0x0
#define APPL_PINMUX_PEX_RST			BIT(0)
#define APPL_PINMUX_CLKREQ_IN			BIT(1)
#define APPL_PINMUX_CLKREQ_OVERRIDE_EN		BIT(2)
#define APPL_PINMUX_CLKREQ_OVERRIDE		BIT(3)
#define APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE_EN	BIT(4)
#define APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE	BIT(5)
#define APPL_PINMUX_PEX_RST_IN_OVERRIDE_EN	BIT(11)
#define APPL_PINMUX_CLKREQ_DEFAULT_VALUE	BIT(13)

#define APPL_CTRL				0x4
#define APPL_CTRL_SYS_PRE_DET_STATE		BIT(6)
#define APPL_CTRL_LTSSM_EN			BIT(7)
#define APPL_CTRL_HW_HOT_RST_EN			BIT(20)
#define APPL_CTRL_HW_HOT_RST_MODE_MASK		GENMASK(1, 0)
#define APPL_CTRL_HW_HOT_RST_MODE_SHIFT		22
#define APPL_CTRL_HW_HOT_RST_MODE_IMDT_RST	0x1
#define APPL_CTRL_HW_HOT_RST_MODE_IMDT_RST_LTSSM_EN	0x2

#define APPL_INTR_EN_L0_0			0x8
#define APPL_INTR_EN_L0_0_LINK_STATE_INT_EN	BIT(0)
#define APPL_INTR_EN_L0_0_MSI_RCV_INT_EN	BIT(4)
#define APPL_INTR_EN_L0_0_INT_INT_EN		BIT(8)
#define APPL_INTR_EN_L0_0_TLP_ERR_INT_EN	BIT(11)
#define APPL_INTR_EN_L0_0_RASDP_INT_EN		BIT(12)
#define APPL_INTR_EN_L0_0_PARITY_ERR_INT_EN	BIT(14)
#define APPL_INTR_EN_L0_0_PCI_CMD_EN_INT_EN	BIT(15)
#define APPL_INTR_EN_L0_0_PEX_RST_INT_EN	BIT(16)
#define APPL_INTR_EN_L0_0_SAFETY_CORR_INT_EN	BIT(19)
#define APPL_INTR_EN_L0_0_SAFETY_UNCORR_INT_EN	BIT(20)
#define APPL_INTR_EN_L0_0_SYS_INTR_EN		BIT(30)
#define APPL_INTR_EN_L0_0_SYS_MSI_INTR_EN	BIT(31)

#define APPL_INTR_STATUS_L0			0xC
#define APPL_INTR_STATUS_L0_LINK_STATE_INT	BIT(0)
#define APPL_INTR_STATUS_L0_INT_INT		BIT(8)
#define APPL_INTR_STATUS_L0_TLP_ERR_INT		BIT(11)
#define APPL_INTR_STATUS_L0_RASDP_INT		BIT(12)
#define APPL_INTR_STATUS_L0_PARITY_ERR_INT	BIT(14)
#define APPL_INTR_STATUS_L0_PCI_CMD_EN_INT	BIT(15)
#define APPL_INTR_STATUS_L0_PEX_RST_INT		BIT(16)
#define APPL_INTR_STATUS_L0_CDM_REG_CHK_INT	BIT(18)
#define APPL_INTR_STATUS_L0_SAFETY_CORR_INT	BIT(19)
#define APPL_INTR_STATUS_L0_SAFETY_UNCORR_INT	BIT(20)

#define APPL_FAULT_EN_L0			0x10
#define APPL_FAULT_EN_L0_TLP_ERR_FAULT_EN	BIT(11)
#define APPL_FAULT_EN_L0_RASDP_FAULT_EN		BIT(12)
#define APPL_FAULT_EN_L0_PARITY_ERR_FAULT_EN	BIT(14)
#define APPL_FAULT_EN_L0_CDM_REG_CHK_FAULT_EN	BIT(18)
#define APPL_FAULT_EN_L0_SAFETY_UNCORR_FAULT_EN	BIT(20)

#define APPL_INTR_EN_L1_0_0				0x1C
#define APPL_INTR_EN_L1_0_0_LINK_REQ_RST_NOT_INT_EN	BIT(1)
#define APPL_INTR_EN_L1_0_0_RDLH_LINK_UP_INT_EN		BIT(3)
#define APPL_INTR_EN_L1_0_0_HOT_RESET_DONE_INT_EN	BIT(30)

#define APPL_INTR_STATUS_L1_0_0				0x20
#define APPL_INTR_STATUS_L1_0_0_LINK_REQ_RST_NOT_CHGED	BIT(1)
#define APPL_INTR_STATUS_L1_0_0_RDLH_LINK_UP_CHGED	BIT(3)
#define APPL_INTR_STATUS_L1_0_0_HOT_RESET_DONE		BIT(30)

#define APPL_INTR_STATUS_L1_1			0x2C
#define APPL_INTR_STATUS_L1_2			0x30
#define APPL_INTR_STATUS_L1_3			0x34
#define APPL_INTR_STATUS_L1_6			0x3C
#define APPL_INTR_STATUS_L1_7			0x40
#define APPL_INTR_STATUS_L1_15_CFG_BME_CHGED	BIT(1)

#define APPL_INTR_EN_L1_8_0			0x44
#define APPL_INTR_EN_L1_8_BW_MGT_INT_EN		BIT(2)
#define APPL_INTR_EN_L1_8_AUTO_BW_INT_EN	BIT(3)
#define APPL_INTR_EN_L1_8_EDMA_INT_EN		BIT(6)
#define APPL_INTR_EN_L1_8_INTX_EN		BIT(11)
#define APPL_INTR_EN_L1_8_AER_INT_EN		BIT(15)

#define APPL_INTR_STATUS_L1_8_0			0x4C
#define APPL_INTR_STATUS_L1_8_0_EDMA_INT_MASK	GENMASK(11, 6)
#define APPL_INTR_STATUS_L1_8_0_BW_MGT_INT_STS	BIT(2)
#define APPL_INTR_STATUS_L1_8_0_AUTO_BW_INT_STS	BIT(3)

#define APPL_INTR_STATUS_L1_9			0x54
#define APPL_INTR_STATUS_L1_10			0x58

#define APPL_FAULT_EN_L1_11			0x5c
#define APPL_FAULT_EN_L1_11_NF_ERR_FAULT_EN	BIT(2)
#define APPL_FAULT_EN_L1_11_F_ERR_FAULT_EN	BIT(1)

#define APPL_INTR_EN_L1_11			0x60
#define APPL_INTR_EN_L1_11_NF_ERR_INT_EN	BIT(2)
#define APPL_INTR_EN_L1_11_F_ERR_INT_EN		BIT(1)

#define APPL_INTR_STATUS_L1_11			0x64
#define APPL_INTR_STATUS_L1_11_NF_ERR_STATE	BIT(2)
#define APPL_INTR_STATUS_L1_11_F_ERR_STATE	BIT(1)

#define APPL_FAULT_EN_L1_12			0x68
#define APPL_FAULT_EN_L1_12_SLV_RASDP_ERR	BIT(1)
#define APPL_FAULT_EN_L1_12_MSTR_RASDP_ERR	BIT(0)

#define APPL_INTR_EN_L1_12			0x6c
#define APPL_INTR_EN_L1_12_SLV_RASDP_ERR	BIT(1)
#define APPL_INTR_EN_L1_12_MSTR_RASDP_ERR	BIT(0)

#define APPL_INTR_STATUS_L1_12			0x70
#define APPL_INTR_STATUS_L1_12_SLV_RASDP_ERR	BIT(1)
#define APPL_INTR_STATUS_L1_12_MSTR_RASDP_ERR	BIT(0)

#define APPL_INTR_STATUS_L1_13			0x74

#define APPL_INTR_STATUS_L1_14			0x78
#define APPL_INTR_STATUS_L1_14_MASK		GENMASK(29, 0)
#define APPL_INTR_STATUS_L1_14_RETRYRAM		BIT(23)

#define APPL_INTR_STATUS_L1_15			0x7C
#define APPL_INTR_STATUS_L1_17			0x88

#define APPL_FAULT_EN_L1_18				0x8c
#define APPL_FAULT_EN_L1_18_CDM_REG_CHK_CMP_ERR		BIT(1)
#define APPL_FAULT_EN_L1_18_CDM_REG_CHK_LOGIC_ERR	BIT(0)

#define APPL_INTR_EN_L1_18				0x90
#define APPL_INTR_EN_L1_18_CDM_REG_CHK_CMPLT		BIT(2)
#define APPL_INTR_EN_L1_18_CDM_REG_CHK_CMP_ERR		BIT(1)
#define APPL_INTR_EN_L1_18_CDM_REG_CHK_LOGIC_ERR	BIT(0)

#define APPL_INTR_STATUS_L1_18				0x94
#define APPL_INTR_STATUS_L1_18_CDM_REG_CHK_CMPLT	BIT(2)
#define APPL_INTR_STATUS_L1_18_CDM_REG_CHK_CMP_ERR	BIT(1)
#define APPL_INTR_STATUS_L1_18_CDM_REG_CHK_LOGIC_ERR	BIT(0)

#define APPL_MSI_CTRL_1				0xAC

#define APPL_MSI_CTRL_2				0xB0

#define APPL_LEGACY_INTX			0xB8

#define APPL_LTR_MSG_1				0xC4
#define LTR_MSG_REQ				BIT(15)
#define LTR_MST_NO_SNOOP_SHIFT			16

#define APPL_LTR_MSG_2				0xC8
#define APPL_LTR_MSG_2_LTR_MSG_REQ_STATE	BIT(3)

#define APPL_LINK_STATUS			0xCC
#define APPL_LINK_STATUS_RDLH_LINK_UP		BIT(0)

#define APPL_DEBUG				0xD0
#define APPL_DEBUG_PM_LINKST_IN_L2_LAT		BIT(21)
#define APPL_DEBUG_PM_LINKST_IN_L0		0x11
#define APPL_DEBUG_LTSSM_STATE_MASK		GENMASK(8, 3)
#define APPL_DEBUG_LTSSM_STATE_SHIFT		3
#define LTSSM_STATE_DETECT_QUIET		0x00
#define LTSSM_STATE_DETECT_ACT			0x08
#define LTSSM_STATE_PRE_DETECT_QUIET		0x28
#define LTSSM_STATE_DETECT_WAIT			0x30
#define LTSSM_STATE_L2_IDLE			0xa8

#define APPL_RADM_STATUS			0xE4
#define APPL_PM_XMT_TURNOFF_STATE		BIT(0)

#define APPL_DM_TYPE				0x100
#define APPL_DM_TYPE_MASK			GENMASK(3, 0)
#define APPL_DM_TYPE_RP				0x4
#define APPL_DM_TYPE_EP				0x0

#define APPL_CFG_BASE_ADDR			0x104
#define APPL_CFG_BASE_ADDR_MASK			GENMASK(31, 12)

#define APPL_CFG_IATU_DMA_BASE_ADDR		0x108
#define APPL_CFG_IATU_DMA_BASE_ADDR_MASK	GENMASK(31, 18)

#define APPL_CFG_MISC				0x110
#define APPL_CFG_MISC_SLV_EP_MODE		BIT(14)
#define APPL_CFG_MISC_ARCACHE_MASK		GENMASK(13, 10)
#define APPL_CFG_MISC_ARCACHE_SHIFT		10
#define APPL_CFG_MISC_ARCACHE_VAL		3

#define APPL_CFG_SLCG_OVERRIDE			0x114
#define APPL_CFG_SLCG_OVERRIDE_SLCG_EN_MASTER	BIT(0)

#define APPL_CAR_RESET_OVRD				0x12C
#define APPL_CAR_RESET_OVRD_CYA_OVERRIDE_CORE_RST_N	BIT(0)

#define APPL_GTH_PHY				0x138
#define APPL_GTH_PHY_PHY_RST			BIT(0)
#define APPL_GTH_PHY_L1SS_PHY_RST_OVERRIDE	BIT(1)
#define APPL_GTH_PHY_L1SS_WAKE_COUNT_MASK	GENMASK(15, 2)
#define APPL_GTH_PHY_L1SS_WAKE_COUNT_SHIFT	2

#define APPL_FAULT_EN_L1_19			0x17c
#define APPL_FAULT_EN_L1_19_SAFETY_CORR		BIT(0)

#define APPL_INTR_EN_L1_19			0x180
#define APPL_INTR_EN_L1_19_SAFETY_CORR		BIT(0)

#define APPL_INTR_STATUS_L1_19			0x184
#define APPL_INTR_STATUS_L1_19_SAFETY_CORR	BIT(0)

#define APPL_FAULT_EN_L1_20			0x188
#define APPL_FAULT_EN_L1_20_IF_TIMEOUT		BIT(1)
#define APPL_FAULT_EN_L1_20_SAFETY_UNCORR	BIT(0)

#define APPL_INTR_EN_L1_20			0x18c
#define APPL_INTR_EN_L1_20_IF_TIMEOUT		BIT(1)
#define APPL_INTR_EN_L1_20_SAFETY_UNCORR	BIT(0)

#define APPL_INTR_STATUS_L1_20			0x190
#define APPL_INTR_STATUS_L1_20_IF_TIMEOUT	BIT(1)
#define APPL_INTR_STATUS_L1_20_SAFETY_UNCORR	BIT(0)

#define APPL_SEC_EXTERNAL_MSI_ADDR_H	0x10100
#define APPL_SEC_EXTERNAL_MSI_ADDR_L	0x10104
#define APPL_SEC_INTERNAL_MSI_ADDR_H	0x10108
#define APPL_SEC_INTERNAL_MSI_ADDR_L	0x1010c

#define V2M_MSI_SETSPI_NS		0x040

#define IO_BASE_IO_DECODE				BIT(0)
#define IO_BASE_IO_DECODE_BIT8				BIT(8)

#define CFG_PREF_MEM_LIMIT_BASE_MEM_DECODE		BIT(0)
#define CFG_PREF_MEM_LIMIT_BASE_MEM_LIMIT_DECODE	BIT(16)

#define PCI_EXP_DEVCTL_PAYLOAD_256B	0x0020 /* 256 Bytes */

#define CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF	0x718
#define CFG_TIMER_CTRL_ACK_NAK_SHIFT	(19)

#define RAS_DES_CAP_EVENT_COUNTER_CONTROL_REG	0x8
#define RAS_DES_CAP_EVENT_COUNTER_DATA_REG	0xc
#define EVENT_COUNTER_ALL_CLEAR		0x3
#define EVENT_COUNTER_ENABLE_ALL	0x7
#define EVENT_COUNTER_ENABLE_SHIFT	2
#define EVENT_COUNTER_EVENT_SEL_MASK	GENMASK(7, 0)
#define EVENT_COUNTER_EVENT_SEL_SHIFT	16
#define EVENT_COUNTER_EVENT_Tx_L0S	0x2
#define EVENT_COUNTER_EVENT_Rx_L0S	0x3
#define EVENT_COUNTER_EVENT_L1		0x5
#define EVENT_COUNTER_EVENT_L1_1	0x7
#define EVENT_COUNTER_EVENT_L1_2	0x8
#define EVENT_COUNTER_GROUP_SEL_SHIFT	24
#define EVENT_COUNTER_GROUP_5		0x5

#define PORT_LOGIC_MSI_CTRL_INT_0_EN		0x828

#define GEN3_EQ_CONTROL_OFF			0x8a8
#define GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_SHIFT	8
#define GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_MASK	GENMASK(23, 8)
#define GEN3_EQ_CONTROL_OFF_FB_MODE_MASK	GENMASK(3, 0)

#define GEN3_RELATED_OFF			0x890
#define GEN3_RELATED_OFF_GEN3_ZRXDC_NONCOMPL	BIT(0)
#define GEN3_RELATED_OFF_GEN3_EQ_DISABLE	BIT(16)
#define GEN3_RELATED_OFF_RATE_SHADOW_SEL_SHIFT	24
#define GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK	GENMASK(25, 24)

#define PORT_LOGIC_AMBA_ERROR_RESPONSE_DEFAULT	0x8D0
#define AMBA_ERROR_RESPONSE_CRS_SHIFT		3
#define AMBA_ERROR_RESPONSE_CRS_MASK		GENMASK(1, 0)
#define AMBA_ERROR_RESPONSE_CRS_OKAY		0
#define AMBA_ERROR_RESPONSE_CRS_OKAY_FFFFFFFF	1
#define AMBA_ERROR_RESPONSE_CRS_OKAY_FFFF0001	2

#define PL_IF_TIMER_CONTROL_OFF			0x930
#define PL_IF_TIMER_CONTROL_OFF_IF_TIMER_EN	BIT(0)
#define PL_IF_TIMER_CONTROL_OFF_IF_TIMER_AER_EN	BIT(1)

#define PL_INTERFACE_TIMER_STATUS_OFF		0x938

#define MSIX_ADDR_MATCH_LOW_OFF			0x940
#define MSIX_ADDR_MATCH_LOW_OFF_EN		BIT(0)
#define MSIX_ADDR_MATCH_LOW_OFF_MASK		GENMASK(31, 2)

#define MSIX_ADDR_MATCH_HIGH_OFF		0x944
#define MSIX_ADDR_MATCH_HIGH_OFF_MASK		GENMASK(31, 0)

#define PL_SAFETY_MASK_OFF			0x960
#define PL_SAFETY_MASK_OFF_RASDP		BIT(0)
#define PL_SAFETY_MASK_OFF_CDM			BIT(1)
#define PL_SAFETY_MASK_OFF_IF_TIMEOUT		BIT(2)
#define PL_SAFETY_MASK_OFF_UNCOR		BIT(3)
#define PL_SAFETY_MASK_OFF_COR			BIT(4)
#define PL_SAFETY_MASK_OFF_RASDP_COR		BIT(5)

#define PL_SAFETY_STATUS_OFF			0x964
#define PL_SAFETY_STATUS_OFF_RASDP		BIT(0)
#define PL_SAFETY_STATUS_OFF_CDM		BIT(1)
#define PL_SAFETY_STATUS_OFF_IF_TIMEOUT		BIT(2)
#define PL_SAFETY_STATUS_OFF_UNCOR		BIT(3)
#define PL_SAFETY_STATUS_OFF_COR		BIT(4)
#define PL_SAFETY_STATUS_OFF_RASDP_COR		BIT(5)

#define PORT_LOGIC_MSIX_DOORBELL			0x948

#define AUX_CLK_FREQ				0xB40

#define CAP_SPCIE_CAP_OFF			0x154
#define CAP_SPCIE_CAP_OFF_DSP_TX_PRESET0_MASK	GENMASK(3, 0)
#define CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_MASK	GENMASK(11, 8)
#define CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_SHIFT	8

#define PME_ACK_DELAY	100	/* 100 us */
#define PME_ACK_TIMEOUT	10000	/* 10 ms */

#define LTSSM_DELAY	10000	/* 10 ms */
#define LTSSM_TIMEOUT	120000	/* 120 ms */

#define GEN3_GEN4_EQ_PRESET_INIT	5

#define GEN1_CORE_CLK_FREQ	62500000
#define GEN2_CORE_CLK_FREQ	125000000
#define GEN3_CORE_CLK_FREQ	250000000
#define GEN4_CORE_CLK_FREQ	500000000

#define LTR_MSG_TIMEOUT		(100 * 1000)

#define PERST_DEBOUNCE_TIME	(5 * 1000)

#define EVENT_QUEUE_LEN		(256)

#define EP_STATE_DISABLED	0
#define EP_STATE_ENABLED	1

/* Reserve 64K page in BAR0 for MSI */
#define BAR0_MSI_OFFSET		SZ_64K
#define BAR0_MSI_SIZE		SZ_64K

#if IS_ENABLED(CONFIG_ARCH_TEGRA_23x_SOC)
#define FREQ2ICC(x) (Bps_to_icc(emc_freq_to_bw(x)))
#else
#define FREQ2ICC(x) 0UL
#endif

enum ep_event {
	EP_EVENT_NONE = 0,
	EP_PEX_RST_DEASSERT,
	EP_PEX_RST_ASSERT,
	EP_HOT_RST_DONE,
	EP_BME_CHANGE,
	EP_EVENT_EXIT,
	EP_EVENT_INVALID,
};

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
static unsigned int pcie_emc_client_id[] = {
	TEGRA_BWMGR_CLIENT_PCIE,
	TEGRA_BWMGR_CLIENT_PCIE_1,
	TEGRA_BWMGR_CLIENT_PCIE_2,
	TEGRA_BWMGR_CLIENT_PCIE_3,
	TEGRA_BWMGR_CLIENT_PCIE_4,
	TEGRA_BWMGR_CLIENT_PCIE_5
};
#endif

#if IS_ENABLED(CONFIG_INTERCONNECT)
static unsigned int pcie_icc_client_id[] = {
	TEGRA_ICC_PCIE_0,
	TEGRA_ICC_PCIE_1,
	TEGRA_ICC_PCIE_2,
	TEGRA_ICC_PCIE_3,
	TEGRA_ICC_PCIE_4,
	TEGRA_ICC_PCIE_5,
	TEGRA_ICC_PCIE_6,
	TEGRA_ICC_PCIE_7,
	TEGRA_ICC_PCIE_8,
	TEGRA_ICC_PCIE_9,
	TEGRA_ICC_PCIE_10,
};
#endif

static const unsigned int pcie_gen_freq[] = {
	GEN1_CORE_CLK_FREQ,
	GEN2_CORE_CLK_FREQ,
	GEN3_CORE_CLK_FREQ,
	GEN4_CORE_CLK_FREQ
};

struct pcie_epl_error_code {
	/* Indicates source of error */
	uint16_t reporter_id;
	/* Error code indicates error reported by corresponding reporter_id */
	uint32_t error_code;
};

/*
 * Tegra234 PCIe HSI error codes and reporter ids, refer to link
 * https://nvidia.jamacloud.com/perspective.req#/items/22181007?projectId=22719
 */
static const struct pcie_epl_error_code epl_error_code[] = {
	{ 0x8023, 0x211e },
	{ 0x8024, 0x211f },
	{ 0x8025, 0x2120 },
	{ 0x8026, 0x2121 },
	{ 0x8027, 0x2122 },
	{ 0x8028, 0x2123 },
	{ 0x8029, 0x2124 },
	{ 0x802a, 0x2125 },
	{ 0x802b, 0x2126 },
	{ 0x802c, 0x2127 },
	{ 0x802d, 0x212a },
};

struct tegra_pcie_dw {
	struct device *dev;
	struct resource *appl_res;
	struct resource *dbi_res;
	struct resource *atu_dma_res;
	struct resource gic_base;
	struct resource msi_base;
	void __iomem *appl_base;
	void __iomem *dma_base;
	struct clk *core_clk;
	struct clk *core_clk_m;
	struct reset_control *core_apb_rst;
	struct reset_control *core_rst;
	struct dw_pcie pci;
	struct tegra_bpmp *bpmp;

	struct tegra_pcie_of_data *of_data;
	enum dw_pcie_device_mode mode;

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	struct tegra_bwmgr_client *emc_bw;
#endif
#if IS_ENABLED(CONFIG_INTERCONNECT)
	struct icc_path *icc_path;
#endif
	u32 dvfs_tbl[4][4]; /* Row for x1/x2/x3/x4 and Col for Gen-1/2/3/4 */

	bool supports_clkreq;
	bool enable_cdm_check;
	bool enable_srns;
	bool link_state;
	bool link_status_change;
	bool link_speed_change;
	bool disable_power_down;
	bool update_fc_fixup;
	bool gic_v2m;
	bool enable_ext_refclk;
	bool is_safety_platform;

	atomic_t report_epl_error;
	atomic_t bme_state_change;
	atomic_t ep_link_up;

	u8 init_link_width;
	u32 msi_ctrl_int;
	u32 num_lanes;
	u32 cid;
	u32 cfg_link_cap_l1sub;
	u32 pcie_cap_base;
	u32 ras_des_cap;
	u32 aspm_cmrt;
	u32 aspm_pwr_on_t;
	u32 aspm_l0s_enter_lat;
	u32 disabled_aspm_states;
	u32 link_up_to;

	struct regulator *pex_ctl_supply;
	struct regulator *slot_ctl_3v3;
	struct regulator *slot_ctl_12v;

	unsigned int phy_count;
	struct phy **phys;

	struct gpio_desc *pex_wake_gpiod;
	int wake_irq;

	u32 target_speed;
	u32 flr_rid;
#if defined(CONFIG_PCIE_RP_DMA_TEST)
	u32 dma_size;
	u32 ep_rid;
	void *dma_virt;
	dma_addr_t dma_phy;
	wait_queue_head_t wr_wq[DMA_WR_CHNL_NUM];
	wait_queue_head_t rd_wq[DMA_RD_CHNL_NUM];
	unsigned long wr_busy;
	unsigned long rd_busy;
	ktime_t wr_start_time[DMA_WR_CHNL_NUM];
	ktime_t wr_end_time[DMA_WR_CHNL_NUM];
	ktime_t rd_start_time[DMA_RD_CHNL_NUM];
	ktime_t rd_end_time[DMA_RD_CHNL_NUM];
#endif
	struct dentry *debugfs;

	wait_queue_head_t config_rp_waitq;
	bool config_rp_done;

	/* Endpoint mode specific */
	struct task_struct *pcie_ep_task;
	wait_queue_head_t wq;
	struct gpio_desc *pex_rst_gpiod;
	struct gpio_desc *pex_refclk_sel_gpiod;
	struct gpio_desc *pex_prsnt_gpiod;
	unsigned int pex_rst_irq;
	unsigned int prsnt_irq;
	bool perst_irq_enabled;
	int ep_state;
	DECLARE_KFIFO(event_fifo, u32, EVENT_QUEUE_LEN);

	/* SD 7.0 specific */
	struct device *sd_dev_handle;
	struct notifier_block nb;
};

struct tegra_pcie_of_data {
	unsigned int version;
	enum dw_pcie_device_mode mode;
	/* Bug 200378817 */
	bool msix_doorbell_access_fixup;
	/* Bug 200348006 */
	bool sbr_reset_fixup;
	/* Bug 200390637 */
	bool l1ss_exit_fixup;
	/* Bug 200337652 */
	bool ltr_req_fixup;
	u32 cdm_chk_int_en;
	/* Bug 200760279 */
	u32 gen4_preset_vec;
	/* Bug 200762207 */
	u8 n_fts[2];
	/* interconnect framework support */
	bool icc_bwmgr;
};

static void tegra_pcie_dw_pme_turnoff(struct tegra_pcie_dw *pcie);

static inline struct tegra_pcie_dw *to_tegra_pcie(struct dw_pcie *pci)
{
	return container_of(pci, struct tegra_pcie_dw, pci);
}

static inline void appl_writel(struct tegra_pcie_dw *pcie, const u32 value,
			       const u32 reg)
{
	writel_relaxed(value, pcie->appl_base + reg);
}

static inline u32 appl_readl(struct tegra_pcie_dw *pcie, const u32 reg)
{
	return readl_relaxed(pcie->appl_base + reg);
}

struct tegra_pcie_soc {
	enum dw_pcie_device_mode mode;
};

static void apply_bad_link_workaround(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);
	u32 current_link_width;
	u16 val;

	/*
	 * NOTE:- Since this scenario is uncommon and link as such is not
	 * stable anyway, not waiting to confirm if link is really
	 * transitioning to Gen-2 speed
	 */
	val = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA);
	if (val & PCI_EXP_LNKSTA_LBMS) {
		current_link_width = (val & PCI_EXP_LNKSTA_NLW) >>
				     PCI_EXP_LNKSTA_NLW_SHIFT;
		if (pcie->init_link_width > current_link_width) {
			dev_warn(pci->dev, "PCIe link is bad, width reduced\n");
			val = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base +
						PCI_EXP_LNKCTL2);
			val &= ~PCI_EXP_LNKCTL2_TLS;
			val |= PCI_EXP_LNKCTL2_TLS_2_5GT;
			dw_pcie_writew_dbi(pci, pcie->pcie_cap_base +
					   PCI_EXP_LNKCTL2, val);

			val = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base +
						PCI_EXP_LNKCTL);
			val |= PCI_EXP_LNKCTL_RL;
			dw_pcie_writew_dbi(pci, pcie->pcie_cap_base +
					   PCI_EXP_LNKCTL, val);
		}
	}
}

#if defined(CONFIG_PCIE_RP_DMA_TEST)
static void tegra_pcie_dma_status_clr(struct tegra_pcie_dw *pcie)
{
	u32 val, bit = 0;

	val = dma_common_rd(pcie->dma_base, DMA_WRITE_INT_STATUS_OFF);
	for_each_set_bit(bit, &pcie->wr_busy, DMA_WR_CHNL_NUM) {
		if (BIT(bit) & val) {
			dma_common_wr(pcie->dma_base, BIT(bit),
				      DMA_WRITE_INT_CLEAR_OFF);
			pcie->wr_end_time[bit] = ktime_get();
			pcie->wr_busy &= ~(BIT(bit));
			wake_up(&pcie->wr_wq[bit]);
		}
	}

	bit = 0;
	val = dma_common_rd(pcie->dma_base, DMA_READ_INT_STATUS_OFF);
	for_each_set_bit(bit, &pcie->rd_busy, DMA_RD_CHNL_NUM) {
		if (BIT(bit) & val) {
			dma_common_wr(pcie->dma_base, BIT(bit),
				      DMA_READ_INT_CLEAR_OFF);
			pcie->rd_end_time[bit] = ktime_get();
			pcie->rd_busy &= ~(BIT(bit));
			wake_up(&pcie->rd_wq[bit]);
		}
	}
}
#endif

/* Read TSC counter for timestamp. */
static inline u64 rdtsc(void)
{
	u64 val;

	asm volatile("mrs %0, cntvct_el0" : "=r" (val));

	return val;
}

static int tegra_pcie_safety_irq_handler(struct tegra_pcie_dw *pcie, u32 status_l0)
{
	struct dw_pcie *pci = &pcie->pci;
	u32 val, status_l1, en_l0;
	int irq_ret = IRQ_HANDLED;

	atomic_set(&pcie->report_epl_error, 0);
	en_l0 = appl_readl(pcie, APPL_INTR_EN_L0_0);

	/* Consistency Monitor for Configuration Registers(CDM) */
	if ((status_l0 & APPL_INTR_STATUS_L0_CDM_REG_CHK_INT) &&
	    (en_l0 & pcie->of_data->cdm_chk_int_en)) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_18);
		val = dw_pcie_readl_dbi(pci, PCIE_PL_CHK_REG_CONTROL_STATUS);
		if (status_l1 & APPL_INTR_STATUS_L1_18_CDM_REG_CHK_CMPLT) {
			dev_info(pcie->dev, "CDM check complete\n");
			val |= PCIE_PL_CHK_REG_CHK_REG_COMPLETE;
		}
		if (status_l1 & APPL_INTR_STATUS_L1_18_CDM_REG_CHK_CMP_ERR) {
			dev_err(pcie->dev, "CDM comparison mismatch\n");
			val |= PCIE_PL_CHK_REG_CHK_REG_COMPARISON_ERROR;
		}
		if (status_l1 & APPL_INTR_STATUS_L1_18_CDM_REG_CHK_LOGIC_ERR) {
			dev_err(pcie->dev, "CDM Logic error\n");
			val |= PCIE_PL_CHK_REG_CHK_REG_LOGIC_ERROR;
		}
		dw_pcie_writel_dbi(pci, PCIE_PL_CHK_REG_CONTROL_STATUS, val);
		val = dw_pcie_readl_dbi(pci, PCIE_PL_CHK_REG_ERR_ADDR);
		dev_err(pcie->dev, "CDM Error Address Offset = 0x%08X\n", val);

		if (status_l1 & (APPL_INTR_STATUS_L1_18_CDM_REG_CHK_CMP_ERR |
		    APPL_INTR_STATUS_L1_18_CDM_REG_CHK_LOGIC_ERR)) {
			/*
			 * Config space may not recover after CDM errors, disable all CDM
			 * interrupts to avoid interrupt storm.
			 */
			appl_writel(pcie, 0x0, APPL_INTR_EN_L1_18);
			appl_writel(pcie, 0x0, APPL_FAULT_EN_L1_18);

			val = appl_readl(pcie, APPL_INTR_EN_L0_0);
			val &= ~pcie->of_data->cdm_chk_int_en;
			appl_writel(pcie, val, APPL_INTR_EN_L0_0);

			val = appl_readl(pcie, APPL_FAULT_EN_L0);
			val &= ~APPL_FAULT_EN_L0_CDM_REG_CHK_FAULT_EN;
			appl_writel(pcie, val, APPL_FAULT_EN_L0);

			atomic_set(&pcie->report_epl_error, 1);
			irq_ret = IRQ_WAKE_THREAD;
		}
	}

	/* TLP errors like ECRC, CPL TO, etc. */
	if ((status_l0 & APPL_INTR_STATUS_L0_TLP_ERR_INT) &&
	    (en_l0 & APPL_INTR_EN_L0_0_TLP_ERR_INT_EN)) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_11);
		appl_writel(pcie, status_l1, APPL_INTR_STATUS_L1_11);

		/* Report uncorrectable errors like ECRC */
		if (status_l1 & (APPL_INTR_STATUS_L1_11_NF_ERR_STATE |
		    APPL_INTR_STATUS_L1_11_F_ERR_STATE)) {
			/* Disable TLP_ERR_INT */
			appl_writel(pcie, 0x0, APPL_INTR_EN_L1_11);
			appl_writel(pcie, 0x0, APPL_FAULT_EN_L1_11);

			val = appl_readl(pcie, APPL_INTR_EN_L0_0);
			val &= ~APPL_INTR_EN_L0_0_TLP_ERR_INT_EN;
			appl_writel(pcie, val, APPL_INTR_EN_L0_0);

			val = appl_readl(pcie, APPL_FAULT_EN_L0);
			val &= ~APPL_FAULT_EN_L0_TLP_ERR_FAULT_EN;
			appl_writel(pcie, val, APPL_FAULT_EN_L0);

			atomic_set(&pcie->report_epl_error, 1);
			irq_ret = IRQ_WAKE_THREAD;
		}
	}

	/* Uncorrectable Memory ECC errors */
	if ((status_l0 & APPL_INTR_STATUS_L0_RASDP_INT) &&
	    (en_l0 & APPL_INTR_EN_L0_0_RASDP_INT_EN)) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_12);

		if (status_l1 & (APPL_INTR_STATUS_L1_12_SLV_RASDP_ERR |
		    APPL_INTR_STATUS_L1_12_MSTR_RASDP_ERR)) {
			/* Link is not reliable after RASDP, so disable interrupts. */
			appl_writel(pcie, 0x0, APPL_FAULT_EN_L1_12);
			appl_writel(pcie, 0x0, APPL_INTR_EN_L1_12);

			val = appl_readl(pcie, APPL_INTR_EN_L0_0);
			val &= ~APPL_INTR_EN_L0_0_RASDP_INT_EN;
			appl_writel(pcie, val, APPL_INTR_EN_L0_0);

			val = appl_readl(pcie, APPL_FAULT_EN_L0);
			val &= ~APPL_FAULT_EN_L0_RASDP_FAULT_EN;
			appl_writel(pcie, val, APPL_FAULT_EN_L0);

			atomic_set(&pcie->report_epl_error, 1);
			irq_ret = IRQ_WAKE_THREAD;
		}
	}

	/* Parity errors */
	if ((status_l0 & APPL_INTR_STATUS_L0_PARITY_ERR_INT) &&
	    (en_l0 & APPL_INTR_EN_L0_0_PARITY_ERR_INT_EN)) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_14);
		appl_writel(pcie, status_l1, APPL_INTR_STATUS_L1_14);

		if (status_l1 & APPL_INTR_STATUS_L1_14_MASK) {
			/* Don't report EPL error if only RETRYRAM is set */
			if (status_l1 & ~APPL_INTR_STATUS_L1_14_RETRYRAM) {
				/* Disable PARITY_ERR */
				val = appl_readl(pcie, APPL_INTR_EN_L0_0);
				val &= ~APPL_INTR_EN_L0_0_PARITY_ERR_INT_EN;
				appl_writel(pcie, val, APPL_INTR_EN_L0_0);

				val = appl_readl(pcie, APPL_FAULT_EN_L0);
				val &= ~APPL_FAULT_EN_L0_PARITY_ERR_FAULT_EN;
				appl_writel(pcie, val, APPL_FAULT_EN_L0);

				atomic_set(&pcie->report_epl_error, 1);
				irq_ret = IRQ_WAKE_THREAD;
			}
		}
	}

	/* Interface transaction timeout errors */
	if ((status_l0 & APPL_INTR_STATUS_L0_SAFETY_UNCORR_INT) &&
	    (en_l0 & APPL_INTR_EN_L0_0_SAFETY_UNCORR_INT_EN)) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_20);

		/* W1C interface transaction timeout errors in PL_INTERFACE_TIMER_STATUS_OFF */
		val = dw_pcie_readl_dbi(pci, PL_INTERFACE_TIMER_STATUS_OFF);
		dw_pcie_writel_dbi(pci, PL_INTERFACE_TIMER_STATUS_OFF, val);

		/* W1C interface transaction timeout error in PL_SAFETY_STATUS_OFF_IF_TIMEOUT */
		val = dw_pcie_readl_dbi(pci, PL_SAFETY_STATUS_OFF);
		dw_pcie_writel_dbi(pci, PL_SAFETY_STATUS_OFF, val);

		if (status_l1 & APPL_INTR_EN_L1_20_IF_TIMEOUT) {
			/* Disable SAFETY_UNCORR error */
			val = appl_readl(pcie, APPL_FAULT_EN_L1_20);
			val &= ~APPL_FAULT_EN_L1_20_IF_TIMEOUT;
			appl_writel(pcie, val, APPL_FAULT_EN_L1_20);

			val = appl_readl(pcie, APPL_INTR_EN_L1_20);
			val &= ~APPL_INTR_EN_L1_20_IF_TIMEOUT;
			appl_writel(pcie, val, APPL_INTR_EN_L1_20);

			val = appl_readl(pcie, APPL_INTR_EN_L0_0);
			val &= ~APPL_INTR_EN_L0_0_SAFETY_UNCORR_INT_EN;
			appl_writel(pcie, val, APPL_INTR_EN_L0_0);

			val = appl_readl(pcie, APPL_FAULT_EN_L0);
			val &= ~APPL_FAULT_EN_L0_SAFETY_UNCORR_FAULT_EN;
			appl_writel(pcie, val, APPL_FAULT_EN_L0);

			atomic_set(&pcie->report_epl_error, 1);
			irq_ret = IRQ_WAKE_THREAD;
		}
	}

	return irq_ret;
}

static irqreturn_t tegra_pcie_rp_irq_handler(int irq, void *arg)
{
	struct tegra_pcie_dw *pcie = arg;
	struct dw_pcie *pci = &pcie->pci;
	struct pcie_port *pp = &pci->pp;
	u32 val, status_l0, status_l1;
	u16 val_w;
	int irq_ret = IRQ_HANDLED;

	status_l0 = appl_readl(pcie, APPL_INTR_STATUS_L0);
	if (status_l0 & APPL_INTR_STATUS_L0_LINK_STATE_INT) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_0_0);
		writel(status_l1, pcie->appl_base + APPL_INTR_STATUS_L1_0_0);
		if (pcie->of_data->sbr_reset_fixup &&
		    status_l1 & APPL_INTR_STATUS_L1_0_0_LINK_REQ_RST_NOT_CHGED) {
			/* SBR & Surprise Link Down WAR */
			val = appl_readl(pcie, APPL_CAR_RESET_OVRD);
			val &= ~APPL_CAR_RESET_OVRD_CYA_OVERRIDE_CORE_RST_N;
			appl_writel(pcie, val, APPL_CAR_RESET_OVRD);
			udelay(1);
			val = appl_readl(pcie, APPL_CAR_RESET_OVRD);
			val |= APPL_CAR_RESET_OVRD_CYA_OVERRIDE_CORE_RST_N;
			appl_writel(pcie, val, APPL_CAR_RESET_OVRD);

			val = dw_pcie_readl_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL);
			val |= PORT_LOGIC_SPEED_CHANGE;
			dw_pcie_writel_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL, val);
		}
		if (status_l1 & APPL_INTR_STATUS_L1_0_0_RDLH_LINK_UP_CHGED) {
			val = appl_readl(pcie, APPL_LINK_STATUS);
			if (val & APPL_LINK_STATUS_RDLH_LINK_UP) {
				dev_info(pcie->dev, "Link is up\n");
				pcie->link_status_change = true;
				irq_ret = IRQ_WAKE_THREAD;
			}
		}
	}

	if (status_l0 & APPL_INTR_STATUS_L0_INT_INT) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_8_0);
		if (status_l1 & APPL_INTR_STATUS_L1_8_0_EDMA_INT_MASK) {
#if defined(CONFIG_PCIE_RP_DMA_TEST)
			irq_ret = IRQ_WAKE_THREAD;
#else
			irq_ret = IRQ_NONE;
#endif
		}

		if (status_l1 & APPL_INTR_STATUS_L1_8_0_AUTO_BW_INT_STS) {
			appl_writel(pcie,
				    APPL_INTR_STATUS_L1_8_0_AUTO_BW_INT_STS,
				    APPL_INTR_STATUS_L1_8_0);
			apply_bad_link_workaround(pp);
		}
		if (status_l1 & APPL_INTR_STATUS_L1_8_0_BW_MGT_INT_STS) {
			/* Clear BW Management Status */
			val_w = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base +
						  PCI_EXP_LNKSTA);
			val_w |= PCI_EXP_LNKSTA_LBMS;
			dw_pcie_writew_dbi(pci, pcie->pcie_cap_base +
					   PCI_EXP_LNKSTA, val_w);

			appl_writel(pcie,
				    APPL_INTR_STATUS_L1_8_0_BW_MGT_INT_STS,
				    APPL_INTR_STATUS_L1_8_0);
			pcie->link_speed_change = true;
			irq_ret = IRQ_WAKE_THREAD;
			dev_dbg(pci->dev, "Link Speed : Gen-%u\n", val_w &
				PCI_EXP_LNKSTA_CLS);
		}
	}

	/* don't overwrite irq_ret if return value is not IRQ_WAKE_THREAD */
	if (tegra_pcie_safety_irq_handler(pcie, status_l0) == IRQ_WAKE_THREAD)
		irq_ret = IRQ_WAKE_THREAD;

	return irq_ret;
}

static irqreturn_t tegra_pcie_rp_irq_thread(int irq, void *arg)
{
	struct tegra_pcie_dw *pcie = arg;
	struct dw_pcie *pci = &pcie->pci;
	struct pcie_port *pp;
	struct pci_bus *bus;
	struct epl_error_report_frame error_report;
	u16 speed;
	u32 status_l0, status_l1;
	int ret;

	if (atomic_dec_and_test(&pcie->report_epl_error)) {
		error_report.error_code = epl_error_code[pcie->cid].error_code;
		error_report.timestamp = lower_32_bits(rdtsc());
		error_report.reporter_id = epl_error_code[pcie->cid].reporter_id;

		ret = epl_report_error(error_report);
		if (ret < 0)
			dev_err(pci->dev, "failed to report EPL error: %d\n", ret);
	}

	pp = &pcie->pci.pp;
	bus = pp->bridge->bus;

	status_l0 = appl_readl(pcie, APPL_INTR_STATUS_L0);
	if (status_l0 & APPL_INTR_STATUS_L0_INT_INT) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_8_0);
		if (status_l1 & APPL_INTR_STATUS_L1_8_0_EDMA_INT_MASK) {
#if defined(CONFIG_PCIE_RP_DMA_TEST)
			tegra_pcie_dma_status_clr(pcie);
#endif
		}
	}

	if (bus && pcie->link_status_change) {
		pci_lock_rescan_remove();
		pci_rescan_bus(bus);
		pci_unlock_rescan_remove();
	}

	if (pcie->link_status_change || pcie->link_speed_change) {
		pcie->link_status_change = false;
		pcie->link_speed_change = false;
		speed = dw_pcie_readw_dbi(pci,
					  pcie->pcie_cap_base + PCI_EXP_LNKSTA);
		speed &= PCI_EXP_LNKSTA_CLS;
		if ((speed > 0) && (speed <= 4) && !pcie->is_safety_platform)
			clk_set_rate(pcie->core_clk, pcie_gen_freq[speed - 1]);
	}

	return IRQ_HANDLED;
}

static void pex_ep_event_hot_rst_done(struct tegra_pcie_dw *pcie)
{
	u32 val;

	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L0);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_0_0);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_1);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_2);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_3);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_6);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_7);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_8_0);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_9);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_10);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_11);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_13);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_14);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_15);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_17);
	appl_writel(pcie, 0xFFFFFFFF, APPL_MSI_CTRL_2);

	val = appl_readl(pcie, APPL_CTRL);
	val |= APPL_CTRL_LTSSM_EN;
	appl_writel(pcie, val, APPL_CTRL);
}

static irqreturn_t tegra_pcie_ep_irq_thread(int irq, void *arg)
{
	struct tegra_pcie_dw *pcie = arg;
	struct dw_pcie *pci = &pcie->pci;
	u32 val, speed, width;
	struct epl_error_report_frame error_report;
	int ret;
	unsigned long freq;

	if (atomic_dec_and_test(&pcie->report_epl_error)) {
		error_report.error_code = epl_error_code[pcie->cid].error_code;
		error_report.timestamp = lower_32_bits(rdtsc());
		error_report.reporter_id = epl_error_code[pcie->cid].reporter_id;

		ret = epl_report_error(error_report);
		if (ret < 0)
			dev_err(pci->dev, "failed to report EPL error: %d\n", ret);
	}

	if (atomic_dec_and_test(&pcie->ep_link_up)) {
		val = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA);

		speed = val & PCI_EXP_LNKSTA_CLS;
		width = (val & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;
		width = find_first_bit((const unsigned long *)&width, 6);

		freq = pcie->dvfs_tbl[width][speed - 1];

#if IS_ENABLED(CONFIG_INTERCONNECT)
		if (pcie->icc_path) {
			if (icc_set_bw(pcie->icc_path, 0, FREQ2ICC(freq)))
				dev_err(pcie->dev, "icc: can't set emc clock[%lu]\n", freq);
		}
#endif

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
		if (pcie->emc_bw) {
			if (tegra_bwmgr_set_emc(pcie->emc_bw, freq, TEGRA_BWMGR_SET_EMC_FLOOR))
				dev_err(pcie->dev, "bwmgr: can't set emc clock[%lu]\n", freq);
		}
#endif

		if ((speed > 0) && (speed <= 4) && !pcie->is_safety_platform)
			clk_set_rate(pcie->core_clk, pcie_gen_freq[speed - 1]);
	}

	if (atomic_dec_and_test(&pcie->bme_state_change)) {
		if (!pcie->of_data->ltr_req_fixup)
			return IRQ_HANDLED;

		/* If EP doesn't advertise L1SS, just return */
		val = dw_pcie_readl_dbi(pci, pcie->cfg_link_cap_l1sub);
		if (!(val & (PCI_L1SS_CAP_ASPM_L1_1 | PCI_L1SS_CAP_ASPM_L1_2)))
			return IRQ_HANDLED;

		/* Check if BME is set to '1' */
		val = dw_pcie_readl_dbi(pci, PCI_COMMAND);
		if (val & PCI_COMMAND_MASTER) {
			ktime_t timeout;

			/* Send LTR upstream */
			val = appl_readl(pcie, APPL_LTR_MSG_2);
			val |= APPL_LTR_MSG_2_LTR_MSG_REQ_STATE;
			appl_writel(pcie, val, APPL_LTR_MSG_2);

			timeout = ktime_add_us(ktime_get(), LTR_MSG_TIMEOUT);
			for (;;) {
				val = appl_readl(pcie, APPL_LTR_MSG_2);
				if (!(val & APPL_LTR_MSG_2_LTR_MSG_REQ_STATE))
					break;
				if (ktime_after(ktime_get(), timeout))
					break;
				usleep_range(1000, 1100);
			}
			if (val & APPL_LTR_MSG_2_LTR_MSG_REQ_STATE)
				dev_err(pcie->dev, "Failed to send LTR message\n");
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t tegra_pcie_ep_hard_irq(int irq, void *arg)
{
	struct tegra_pcie_dw *pcie = arg;
	struct dw_pcie_ep *ep = &pcie->pci.ep;
	int irq_ret = IRQ_HANDLED;
	u32 status_l0, status_l1, link_status;

	atomic_set(&pcie->ep_link_up, 0);
	atomic_set(&pcie->bme_state_change, 0);

	status_l0 = appl_readl(pcie, APPL_INTR_STATUS_L0);
	if (status_l0 & APPL_INTR_STATUS_L0_LINK_STATE_INT) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_0_0);
		appl_writel(pcie, status_l1, APPL_INTR_STATUS_L1_0_0);

		if (status_l1 & APPL_INTR_STATUS_L1_0_0_HOT_RESET_DONE)
			pex_ep_event_hot_rst_done(pcie);

		if (status_l1 & APPL_INTR_STATUS_L1_0_0_RDLH_LINK_UP_CHGED) {
			link_status = appl_readl(pcie, APPL_LINK_STATUS);
			if (link_status & APPL_LINK_STATUS_RDLH_LINK_UP) {
				dev_dbg(pcie->dev, "Link is up with Host\n");
				dw_pcie_ep_linkup(ep);
				atomic_set(&pcie->ep_link_up, 1);
				irq_ret = IRQ_WAKE_THREAD;
			}
		}
	}

	if (status_l0 & APPL_INTR_STATUS_L0_PCI_CMD_EN_INT) {
		status_l1 = appl_readl(pcie, APPL_INTR_STATUS_L1_15);
		appl_writel(pcie, status_l1, APPL_INTR_STATUS_L1_15);

		if (status_l1 & APPL_INTR_STATUS_L1_15_CFG_BME_CHGED) {
			atomic_set(&pcie->bme_state_change, 1);
			irq_ret = IRQ_WAKE_THREAD;
		}
	}

	/* don't overwrite irq_ret if return value is not IRQ_WAKE_THREAD*/
	if (tegra_pcie_safety_irq_handler(pcie, status_l0) == IRQ_WAKE_THREAD)
		irq_ret = IRQ_WAKE_THREAD;

	return irq_ret;
}

static int tegra_pcie_dw_rd_own_conf(struct pci_bus *bus, u32 devfn, int where,
				     int size, u32 *val)
{
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);

	/*
	 * This is an endpoint mode specific register happen to appear even
	 * when controller is operating in root port mode and system hangs
	 * when it is accessed with link being in ASPM-L1 state for dw ver 1.
	 * So skip accessing it altogether
	 */
	if (!PCI_SLOT(devfn) && where == PORT_LOGIC_MSIX_DOORBELL &&
	    pcie->of_data->msix_doorbell_access_fixup) {
		*val = 0x00000000;
		return PCIBIOS_SUCCESSFUL;
	}

	return pci_generic_config_read(bus, devfn, where, size, val);
}

static int tegra_pcie_dw_wr_own_conf(struct pci_bus *bus, u32 devfn, int where,
				     int size, u32 val)
{
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);

	/*
	 * This is an endpoint mode specific register happen to appear even
	 * when controller is operating in root port mode and system hangs
	 * when it is accessed with link being in ASPM-L1 state.
	 * So skip accessing it altogether
	 */
	if (!PCI_SLOT(devfn) && where == PORT_LOGIC_MSIX_DOORBELL &&
	    pcie->of_data->msix_doorbell_access_fixup)
		return PCIBIOS_SUCCESSFUL;

	return pci_generic_config_write(bus, devfn, where, size, val);
}

static struct pci_ops tegra_pci_ops = {
	.map_bus = dw_pcie_own_conf_map_bus,
	.read = tegra_pcie_dw_rd_own_conf,
	.write = tegra_pcie_dw_wr_own_conf,
};

#if defined(CONFIG_PCIEASPM)
static void disable_aspm_l0s(struct tegra_pcie_dw *pcie)
{
	u32 val = 0;

	val = dw_pcie_readl_dbi(&pcie->pci, pcie->pcie_cap_base + PCI_EXP_LNKCAP);
	val &= ~PCI_EXP_LNKCAP_ASPM_L0S;
	dw_pcie_writel_dbi(&pcie->pci, pcie->pcie_cap_base + PCI_EXP_LNKCAP, val);
}

static void disable_aspm_l10(struct tegra_pcie_dw *pcie)
{
	u32 val = 0;

	val = dw_pcie_readl_dbi(&pcie->pci, pcie->pcie_cap_base + PCI_EXP_LNKCAP);
	val &= ~PCI_EXP_LNKCAP_ASPM_L1;
	dw_pcie_writel_dbi(&pcie->pci, pcie->pcie_cap_base + PCI_EXP_LNKCAP, val);
}
static void disable_aspm_l11(struct tegra_pcie_dw *pcie)
{
	u32 val;

	val = dw_pcie_readl_dbi(&pcie->pci, pcie->cfg_link_cap_l1sub);
	val &= ~PCI_L1SS_CAP_ASPM_L1_1;
	dw_pcie_writel_dbi(&pcie->pci, pcie->cfg_link_cap_l1sub, val);
}

static void disable_aspm_l12(struct tegra_pcie_dw *pcie)
{
	u32 val;

	val = dw_pcie_readl_dbi(&pcie->pci, pcie->cfg_link_cap_l1sub);
	val &= ~PCI_L1SS_CAP_ASPM_L1_2;
	dw_pcie_writel_dbi(&pcie->pci, pcie->cfg_link_cap_l1sub, val);
}

static inline u32 event_counter_prog(struct tegra_pcie_dw *pcie, u32 event)
{
	u32 val;

	val = dw_pcie_readl_dbi(&pcie->pci, pcie->ras_des_cap +
				RAS_DES_CAP_EVENT_COUNTER_CONTROL_REG);
	val &= ~(EVENT_COUNTER_EVENT_SEL_MASK << EVENT_COUNTER_EVENT_SEL_SHIFT);
	val |= EVENT_COUNTER_GROUP_5 << EVENT_COUNTER_GROUP_SEL_SHIFT;
	val |= event << EVENT_COUNTER_EVENT_SEL_SHIFT;
	val |= EVENT_COUNTER_ENABLE_ALL << EVENT_COUNTER_ENABLE_SHIFT;
	dw_pcie_writel_dbi(&pcie->pci, pcie->ras_des_cap +
			   RAS_DES_CAP_EVENT_COUNTER_CONTROL_REG, val);
	val = dw_pcie_readl_dbi(&pcie->pci, pcie->ras_des_cap +
				RAS_DES_CAP_EVENT_COUNTER_DATA_REG);

	return val;
}

static int aspm_state_cnt(struct seq_file *s, void *data)
{
	struct tegra_pcie_dw *pcie = (struct tegra_pcie_dw *)
				     dev_get_drvdata(s->private);
	u32 val;

	seq_printf(s, "Tx L0s entry count : %u\n",
		   event_counter_prog(pcie, EVENT_COUNTER_EVENT_Tx_L0S));

	seq_printf(s, "Rx L0s entry count : %u\n",
		   event_counter_prog(pcie, EVENT_COUNTER_EVENT_Rx_L0S));

	seq_printf(s, "Link L1 entry count : %u\n",
		   event_counter_prog(pcie, EVENT_COUNTER_EVENT_L1));

	seq_printf(s, "Link L1.1 entry count : %u\n",
		   event_counter_prog(pcie, EVENT_COUNTER_EVENT_L1_1));

	seq_printf(s, "Link L1.2 entry count : %u\n",
		   event_counter_prog(pcie, EVENT_COUNTER_EVENT_L1_2));

	/* Clear all counters */
	dw_pcie_writel_dbi(&pcie->pci, pcie->ras_des_cap +
			   RAS_DES_CAP_EVENT_COUNTER_CONTROL_REG,
			   EVENT_COUNTER_ALL_CLEAR);

	/* Re-enable counting */
	val = EVENT_COUNTER_ENABLE_ALL << EVENT_COUNTER_ENABLE_SHIFT;
	val |= EVENT_COUNTER_GROUP_5 << EVENT_COUNTER_GROUP_SEL_SHIFT;
	dw_pcie_writel_dbi(&pcie->pci, pcie->ras_des_cap +
			   RAS_DES_CAP_EVENT_COUNTER_CONTROL_REG, val);

	return 0;
}

static void init_host_aspm(struct tegra_pcie_dw *pcie)
{
	struct dw_pcie *pci = &pcie->pci;
	u32 val;

	val = dw_pcie_find_ext_capability(pci, PCI_EXT_CAP_ID_L1SS);
	pcie->cfg_link_cap_l1sub = val + PCI_L1SS_CAP;

	/* Enable ASPM counters */
	val = EVENT_COUNTER_ENABLE_ALL << EVENT_COUNTER_ENABLE_SHIFT;
	val |= EVENT_COUNTER_GROUP_5 << EVENT_COUNTER_GROUP_SEL_SHIFT;
	dw_pcie_writel_dbi(pci, pcie->ras_des_cap +
			   RAS_DES_CAP_EVENT_COUNTER_CONTROL_REG, val);

	/* Program T_cmrt and T_pwr_on values */
	val = dw_pcie_readl_dbi(pci, pcie->cfg_link_cap_l1sub);
	val &= ~(PCI_L1SS_CAP_CM_RESTORE_TIME | PCI_L1SS_CAP_P_PWR_ON_VALUE);
	val |= (pcie->aspm_cmrt << 8);
	val |= (pcie->aspm_pwr_on_t << 19);
	dw_pcie_writel_dbi(pci, pcie->cfg_link_cap_l1sub, val);

	/* Program L0s and L1 entrance latencies */
	val = dw_pcie_readl_dbi(pci, PCIE_PORT_AFR);
	val &= ~PORT_AFR_L0S_ENTRANCE_LAT_MASK;
	val |= (pcie->aspm_l0s_enter_lat << PORT_AFR_L0S_ENTRANCE_LAT_SHIFT);
	val |= PORT_AFR_ENTER_ASPM;
	dw_pcie_writel_dbi(pci, PCIE_PORT_AFR, val);
}

static void init_aspm_debugfs(struct tegra_pcie_dw *pcie)
{
	debugfs_create_devm_seqfile(pcie->dev, "aspm_state_cnt", pcie->debugfs,
				    aspm_state_cnt);
}
#else
static inline void disable_aspm_l12(struct tegra_pcie_dw *pcie) { return; }
static inline void disable_aspm_l11(struct tegra_pcie_dw *pcie) { return; }
static inline void init_host_aspm(struct tegra_pcie_dw *pcie) { return; }
static inline void init_aspm_debugfs(struct tegra_pcie_dw *pcie) { return; }
#endif

static int apply_speed_change(struct seq_file *s, void *data)
{
	struct tegra_pcie_dw *pcie = (struct tegra_pcie_dw *)
				     dev_get_drvdata(s->private);
	struct dw_pcie *pci = &pcie->pci;
	unsigned long start;
	u16 val_w;

	if ((pcie->target_speed == 0) ||
	    (pcie->target_speed > PCI_EXP_LNKSTA_CLS_16_0GB)) {
		seq_puts(s, "Invalid target speed. Should be 1 ~ 4\n");
		return 0;
	}

	val_w = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA);
	if ((val_w & PCI_EXP_LNKSTA_CLS) == pcie->target_speed) {
		seq_puts(s, "Link speed is already the target speed\n");
		return 0;
	}

	val_w = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKCTL2);
	val_w &= ~PCI_EXP_LNKSTA_CLS;
	val_w |= pcie->target_speed;
	dw_pcie_writew_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKCTL2, val_w);

	/* Wait for previous link training to complete */
	start = jiffies;
	for (;;) {
		val_w = dw_pcie_readw_dbi(pci,
					  pcie->pcie_cap_base + PCI_EXP_LNKSTA);
		if (!(val_w & PCI_EXP_LNKSTA_LT))
			break;
		if (time_after(jiffies, start + msecs_to_jiffies(1000))) {
			seq_puts(s, "Link Retrain Timeout\n");
			break;
		}
		usleep_range(1000, 1100);
	}

	if (val_w & PCI_EXP_LNKSTA_LT) {
		seq_puts(s, "Previous link training didn't complete\n");
		return 0;
	}

	/* Clear BW Management Status */
	val_w = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA);
	val_w |= PCI_EXP_LNKSTA_LBMS;
	dw_pcie_writew_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA, val_w);

	val_w = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKCTL);
	val_w |= PCI_EXP_LNKCTL_RL;
	dw_pcie_writew_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKCTL, val_w);

	/* Wait for link training end. Break out after waiting for timeout */
	start = jiffies;
	for (;;) {
		val_w = dw_pcie_readw_dbi(pci,
					  pcie->pcie_cap_base + PCI_EXP_LNKSTA);
		if (!(val_w & PCI_EXP_LNKSTA_LT))
			break;
		if (time_after(jiffies, start + msecs_to_jiffies(1000))) {
			seq_puts(s, "Link Training Timeout\n");
			break;
		}
		usleep_range(1000, 1100);
	}

	/* Wait for link BW management status to be updated */
	start = jiffies;
	for (;;) {
		val_w = dw_pcie_readw_dbi(pci,
					  pcie->pcie_cap_base + PCI_EXP_LNKSTA);
		if (val_w & PCI_EXP_LNKSTA_LBMS) {
			/* Clear BW Management Status */
			val_w = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base +
						  PCI_EXP_LNKSTA);
			val_w |= PCI_EXP_LNKSTA_LBMS;
			dw_pcie_writew_dbi(pci, pcie->pcie_cap_base +
					   PCI_EXP_LNKSTA, val_w);
			break;
		}
		if (time_after(jiffies, start + msecs_to_jiffies(1000))) {
			seq_puts(s, "Bandwidth Management Status Timeout\n");
			break;
		}
		usleep_range(1000, 1100);
	}

	/* Give 20ms time for new link status to appear in LnkSta register */
	msleep(20);

	val_w = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA);
	if ((val_w & PCI_EXP_LNKSTA_CLS) == pcie->target_speed) {
		seq_puts(s, "Link speed is successful\n");
	} else {
		seq_puts(s, "Link speed change failed");
		seq_printf(s, "Settled for Gen-%u\n", (val_w >> 16) &
			   PCI_EXP_LNKSTA_CLS);
	}

	return 0;
}

static int apply_pme_turnoff(struct seq_file *s, void *data)
{
	struct tegra_pcie_dw *pcie = (struct tegra_pcie_dw *)
				     dev_get_drvdata(s->private);

	tegra_pcie_dw_pme_turnoff(pcie);
	seq_puts(s, "PME_TurnOff sent and Link is in L2 state\n");

	return 0;
}

static int apply_sbr(struct seq_file *s, void *data)
{
	struct tegra_pcie_dw *pcie = (struct tegra_pcie_dw *)
				     dev_get_drvdata(s->private);
	struct dw_pcie *pci = &pcie->pci;
	struct pci_dev *pdev = NULL;
	u16 val = 0, lnkspd = 0, tls = 0;
	bool pass = true;
	int domain = of_get_pci_domain_nr(pcie->dev->of_node);

	/* save config state */
	for_each_pci_dev(pdev) {
		if (pci_domain_nr(pdev->bus) == domain)
			pci_save_state(pdev);
	}

	lnkspd = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA);
	lnkspd &= PCI_EXP_LNKSTA_CLS;
	tls = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKCTL2);
	tls &= PCI_EXP_LNKCTL2_TLS;

	pdev = pci_get_domain_bus_and_slot(domain, 0x0, 0x0);
	if (!pdev) {
		seq_printf(s, "RP pci_dev not found in domain: %d\n", domain);
		return 0;
	}

	if (pci_bridge_secondary_bus_reset(pdev)) {
		seq_printf(s, "%s: secondary bus reset failed\n", __func__);
		return 0;
	}

	pci_dev_put(pdev);

	/* Compare PCIE_CAP_TARGET_LINK_SPEED sticky bit before & after SBR */
	val = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA);
	val &= PCI_EXP_LNKSTA_CLS;
	if (lnkspd != val) {
		seq_printf(s, "Link speed not restored to %d, cur speed: %d\n",
			   lnkspd, val);
		pass = false;
	}

	val = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKCTL2);
	val &= PCI_EXP_LNKCTL2_TLS;
	if (tls != val) {
		seq_printf(s, "Sticky reg changed, prev tls: %d, cur tls: %d\n",
			   tls, val);
		pass = false;
	}

	mdelay(100);

	/* restore config state */
	pdev = NULL;
	for_each_pci_dev(pdev) {
		if (pci_domain_nr(pdev->bus) == domain) {
			pci_restore_state(pdev);
			mdelay(10);
		}
	}

	if (pass)
		seq_puts(s, "Secondary Bus Reset applied successfully\n");
	else
		seq_puts(s, "Secondary Bus Reset failed\n");

	return 0;
}

static int apply_flr(struct seq_file *s, void *data)
{
	struct tegra_pcie_dw *pcie = (struct tegra_pcie_dw *)
				     dev_get_drvdata(s->private);
	struct pci_dev *pdev = NULL;
	int domain = of_get_pci_domain_nr(pcie->dev->of_node);
	int ret;

	pdev = pci_get_domain_bus_and_slot(domain, (pcie->flr_rid >> 8) & 0xff,
					   pcie->flr_rid & 0xff);
	pci_dev_put(pdev);
	if (!pdev) {
		seq_printf(s, "No PCIe device with RID: 0x%x\n", pcie->flr_rid);
		return 0;
	}

	/* save config state */
	pci_save_state(pdev);

	if (!pcie_has_flr(pdev)) {
		seq_printf(s, "PCIe device: 0x%x has no FLR\n", pcie->flr_rid);
		return 0;
	}

	ret = pcie_flr(pdev);
	if (ret < 0) {
		seq_printf(s, "FLR failed for PCIe dev: 0x%x\n", pcie->flr_rid);
		return 0;
	}

	/* restore config state */
	pci_restore_state(pdev);

	seq_puts(s, "Functional Level Reset applied successfully\n");

	return 0;
}

#if defined(CONFIG_PCIE_RP_DMA_TEST)
struct edma_desc {
	dma_addr_t src;
	dma_addr_t dst;
	size_t sz;
};

static int edma_init(struct tegra_pcie_dw *pcie, bool lie)
{
	u32 val;
	int i;

	/* Enable LIE or RIE for all write channels */
	if (lie) {
		val = dma_common_rd(pcie->dma_base, DMA_WRITE_INT_MASK_OFF);
		val &= ~0xf;
		val &= ~(0xf << 16);
		dma_common_wr(pcie->dma_base, val, DMA_WRITE_INT_MASK_OFF);
	}

	val = DMA_CH_CONTROL1_OFF_WRCH_LIE;
	if (!lie)
		val |= DMA_CH_CONTROL1_OFF_WRCH_RIE;
	for (i = 0; i < DMA_WR_CHNL_NUM; i++)
		dma_channel_wr(pcie->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_WRCH);

	/* Enable LIE or RIE for all read channels */
	if (lie) {
		val = dma_common_rd(pcie->dma_base, DMA_READ_INT_MASK_OFF);
		val &= ~0x3;
		val &= ~(0x3 << 16);
		dma_common_wr(pcie->dma_base, val, DMA_READ_INT_MASK_OFF);
	}

	val = DMA_CH_CONTROL1_OFF_RDCH_LIE;
	if (!lie)
		val |= DMA_CH_CONTROL1_OFF_RDCH_RIE;
	for (i = 0; i < DMA_RD_CHNL_NUM; i++)
		dma_channel_wr(pcie->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_RDCH);

	dma_common_wr(pcie->dma_base, WRITE_ENABLE, DMA_WRITE_ENGINE_EN_OFF);
	dma_common_wr(pcie->dma_base, READ_ENABLE, DMA_READ_ENGINE_EN_OFF);

	return 0;
}

static void edma_deinit(struct tegra_pcie_dw *pcie)
{
	u32 val;

	/* Mask channel interrupts */
	val = dma_common_rd(pcie->dma_base, DMA_WRITE_INT_MASK_OFF);
	val |= 0xf;
	val |= (0xf << 16);
	dma_common_wr(pcie->dma_base, val, DMA_WRITE_INT_MASK_OFF);

	val = dma_common_rd(pcie->dma_base, DMA_READ_INT_MASK_OFF);
	val |= 0x3;
	val |= (0x3 << 16);
	dma_common_wr(pcie->dma_base, val, DMA_READ_INT_MASK_OFF);

	dma_common_wr(pcie->dma_base, WRITE_DISABLE, DMA_WRITE_ENGINE_EN_OFF);
	dma_common_wr(pcie->dma_base, READ_DISABLE, DMA_READ_ENGINE_EN_OFF);
}

static int edma_ll_init(struct tegra_pcie_dw *pcie)
{
	u32 val;
	int i;

	/* Enable linked list mode and set CCS */
	val = DMA_CH_CONTROL1_OFF_WRCH_LLE | DMA_CH_CONTROL1_OFF_WRCH_CCS;
	for (i = 0; i < DMA_WR_CHNL_NUM; i++)
		dma_channel_wr(pcie->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_WRCH);

	val = DMA_CH_CONTROL1_OFF_RDCH_LLE | DMA_CH_CONTROL1_OFF_WRCH_CCS;
	for (i = 0; i < DMA_RD_CHNL_NUM; i++)
		dma_channel_wr(pcie->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_RDCH);

	return 0;
}

static void edma_ll_deinit(struct tegra_pcie_dw *pcie)
{
	u32 val;
	int i;

	/* Disable linked list mode and clear CCS */
	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		val = dma_channel_rd(pcie->dma_base, i,
				     DMA_CH_CONTROL1_OFF_WRCH);
		val &= ~(DMA_CH_CONTROL1_OFF_WRCH_LLE |
			 DMA_CH_CONTROL1_OFF_WRCH_CCS);
		dma_channel_wr(pcie->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_WRCH);
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		val = dma_channel_rd(pcie->dma_base, i,
				     DMA_CH_CONTROL1_OFF_RDCH);
		val &= ~(DMA_CH_CONTROL1_OFF_RDCH_LLE |
			 DMA_CH_CONTROL1_OFF_RDCH_CCS);
		dma_channel_wr(pcie->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_RDCH);
	}
}

static int edma_submit_direct_tx(struct tegra_pcie_dw *pcie,
				 struct edma_desc *desc, u32 ch)
{
	int ret = 0;

	pcie->wr_busy |= 1 << ch;

	/* Populate desc in DMA registers */
	dma_channel_wr(pcie->dma_base, ch, desc->sz,
		       DMA_TRANSFER_SIZE_OFF_WRCH);
	dma_channel_wr(pcie->dma_base, ch, lower_32_bits(desc->src),
		       DMA_SAR_LOW_OFF_WRCH);
	dma_channel_wr(pcie->dma_base, ch, upper_32_bits(desc->src),
		       DMA_SAR_HIGH_OFF_WRCH);
	dma_channel_wr(pcie->dma_base, ch, lower_32_bits(desc->dst),
		       DMA_DAR_LOW_OFF_WRCH);
	dma_channel_wr(pcie->dma_base, ch, upper_32_bits(desc->dst),
		       DMA_DAR_HIGH_OFF_WRCH);

	pcie->wr_start_time[ch] = ktime_get();
	dma_common_wr(pcie->dma_base, ch, DMA_WRITE_DOORBELL_OFF);

	/* Wait 5 sec to get DMA done interrupt */
	ret = wait_event_timeout(pcie->wr_wq[ch],
				 !(pcie->wr_busy & (1 << ch)),
				 msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_err(pcie->dev, "%s: DD WR CH: %d TO\n", __func__, ch);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int edma_submit_direct_rx(struct tegra_pcie_dw *pcie,
				 struct edma_desc *desc, u32 ch)
{
	int ret = 0;

	pcie->rd_busy |= 1 << ch;

	/* Populate desc in DMA registers */
	dma_channel_wr(pcie->dma_base, ch, desc->sz,
		       DMA_TRANSFER_SIZE_OFF_RDCH);
	dma_channel_wr(pcie->dma_base, ch, lower_32_bits(desc->src),
		       DMA_SAR_LOW_OFF_RDCH);
	dma_channel_wr(pcie->dma_base, ch, upper_32_bits(desc->src),
		       DMA_SAR_HIGH_OFF_RDCH);
	dma_channel_wr(pcie->dma_base, ch, lower_32_bits(desc->dst),
		       DMA_DAR_LOW_OFF_RDCH);
	dma_channel_wr(pcie->dma_base, ch, upper_32_bits(desc->dst),
		       DMA_DAR_HIGH_OFF_RDCH);

	pcie->rd_start_time[ch] = ktime_get();
	dma_common_wr(pcie->dma_base, ch, DMA_READ_DOORBELL_OFF);

	ret = wait_event_timeout(pcie->rd_wq[ch],
				 !(pcie->rd_busy & (1 << ch)),
				 msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_err(pcie->dev, "%s: DD RD CH: %d TO\n", __func__, ch);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int edma_submit_sync_tx(struct tegra_pcie_dw *pcie,
			       struct edma_desc *desc,
			       int nents, u32 ch, bool lie)
{
	dma_addr_t ll_phy_addr = pcie->dma_phy + DMA_LL_WR_OFFSET(ch);
	struct dma_ll *dma_ll_virt;
	int i, ret;

	pcie->wr_busy |= 1 << ch;

	/* Program DMA LL base address in DMA LL pointer register */
	dma_channel_wr(pcie->dma_base, ch, lower_32_bits(ll_phy_addr),
		       DMA_LLP_LOW_OFF_WRCH);
	dma_channel_wr(pcie->dma_base, ch, upper_32_bits(ll_phy_addr),
		       DMA_LLP_HIGH_OFF_WRCH);

	/* Populate DMA descriptors in LL */
	dma_ll_virt = (struct dma_ll *)
		(pcie->dma_virt + DMA_LL_WR_OFFSET(ch));
	for (i = 0; i < nents; i++) {
		dma_ll_virt->size = desc[i].sz;
		dma_ll_virt->src_low = lower_32_bits(desc[i].src);
		dma_ll_virt->src_high = upper_32_bits(desc[i].src);
		dma_ll_virt->dst_low = lower_32_bits(desc[i].dst);
		dma_ll_virt->dst_high = upper_32_bits(desc[i].dst);
		dma_ll_virt->ele.cb = 1;
		dma_ll_virt++;
	}
	/* Set LIE or RIE in last element */
	dma_ll_virt--;
	dma_ll_virt->ele.lie = 1;
	if (!lie)
		dma_ll_virt->ele.rie = 1;

	pcie->wr_start_time[ch] = ktime_get();
	dma_common_wr(pcie->dma_base, ch, DMA_WRITE_DOORBELL_OFF);

	ret = wait_event_timeout(pcie->wr_wq[ch],
				 !(pcie->wr_busy & (1 << ch)),
				 msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_err(pcie->dev, "%s: LL WR CH: %d TO\n", __func__, ch);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int edma_submit_sync_rx(struct tegra_pcie_dw *pcie,
			       struct edma_desc *desc,
			       int nents, u32 ch, bool lie)
{
	dma_addr_t ll_phy_addr = pcie->dma_phy + DMA_LL_RD_OFFSET(ch);
	struct dma_ll *dma_ll_virt;
	int i, ret;

	pcie->rd_busy |= 1 << ch;

	/* Program DMA LL base address in DMA LL pointer register */
	dma_channel_wr(pcie->dma_base, ch, lower_32_bits(ll_phy_addr),
		       DMA_LLP_LOW_OFF_RDCH);
	dma_channel_wr(pcie->dma_base, ch, upper_32_bits(ll_phy_addr),
		       DMA_LLP_HIGH_OFF_RDCH);

	/* Populate DMA descriptors in LL */
	dma_ll_virt = (struct dma_ll *)
		(pcie->dma_virt + DMA_LL_RD_OFFSET(ch));
	for (i = 0; i < nents; i++) {
		dma_ll_virt->size = desc[i].sz;
		dma_ll_virt->src_low = lower_32_bits(desc[i].src);
		dma_ll_virt->src_high = upper_32_bits(desc[i].src);
		dma_ll_virt->dst_low = lower_32_bits(desc[i].dst);
		dma_ll_virt->dst_high = upper_32_bits(desc[i].dst);
		dma_ll_virt->ele.cb = 1;
		dma_ll_virt++;
	}
	/* Set LIE or RIE in last element */
	dma_ll_virt--;
	dma_ll_virt->ele.lie = 1;
	if (!lie)
		dma_ll_virt->ele.rie = 1;

	pcie->rd_start_time[ch] = ktime_get();
	dma_common_wr(pcie->dma_base, ch, DMA_READ_DOORBELL_OFF);

	ret = wait_event_timeout(pcie->rd_wq[ch],
				 !(pcie->rd_busy & (1 << ch)),
				 msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_err(pcie->dev, "%s: LL RD CH: %d TO\n",  __func__, ch);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int perf_test(struct seq_file *s, void *data)
{
	struct tegra_pcie_dw *pcie = (struct tegra_pcie_dw *)
					dev_get_drvdata(s->private);
	struct edma_desc desc;
	struct edma_desc ll_desc[DMA_LL_DEFAULT_SIZE];
	dma_addr_t ep_dma_addr;
	dma_addr_t rp_dma_addr = pcie->dma_phy + BAR0_DMA_BUF_OFFSET;
	long long time;
	u32 ch = 0;
	int nents = DMA_LL_MIN_SIZE, i, ret;
	struct pci_dev *pdev = NULL;
	int domain = of_get_pci_domain_nr(pcie->dev->of_node);

	pdev = pci_get_domain_bus_and_slot(domain, pcie->ep_rid >> 8,
					   pcie->ep_rid & 0xff);
	pci_dev_put(pdev);
	if (!pdev) {
		dev_err(pcie->dev, "%s: EP RID: 0x%x not found\n",
			__func__, pcie->ep_rid);
		return 0;
	}

	ep_dma_addr = pci_resource_start(pdev, 0) + BAR0_DMA_BUF_OFFSET;

	edma_init(pcie, 1);

	/* Direct DMA perf test with size BAR0_DMA_BUF_SIZE */
	desc.src = rp_dma_addr;
	desc.dst = ep_dma_addr;
	desc.sz = BAR0_DMA_BUF_SIZE;
	ret = edma_submit_direct_tx(pcie, &desc, ch);
	if (ret < 0) {
		dev_err(pcie->dev, "%s: DD WR, SZ: %lu B CH: %d failed\n",
			__func__, desc.sz, ch);
		goto fail;
	}

	time = ktime_to_ns(pcie->wr_end_time[ch]) -
		ktime_to_ns(pcie->wr_start_time[ch]);
	dev_info(pcie->dev, "%s: DD WR, CH: %d SZ: %lu B, time: %lld ns\n",
		 __func__, ch, desc.sz, time);

	desc.src = ep_dma_addr;
	desc.dst = rp_dma_addr;
	desc.sz = BAR0_DMA_BUF_SIZE;
	ret = edma_submit_direct_rx(pcie, &desc, ch);
	if (ret < 0) {
		dev_err(pcie->dev, "%s: DD RD, SZ: %lu B CH: %d failed\n",
			__func__, desc.sz, ch);
		goto fail;
	}
	time = ktime_to_ns(pcie->rd_end_time[ch]) -
		ktime_to_ns(pcie->rd_start_time[ch]);
	dev_info(pcie->dev, "%s: DD RD, CH: %d SZ: %lu B, time: %lld ns\n",
		 __func__, ch, desc.sz, time);

	/* Clean DMA LL */
	memset(pcie->dma_virt + DMA_LL_WR_OFFSET(0), 0, 6 * DMA_LL_SIZE);
	edma_ll_init(pcie);

	/* LL DMA perf test with size BAR0_DMA_BUF_SIZE and one desc */
	for (i = 0; i < nents; i++) {
		ll_desc[i].src = rp_dma_addr + (i * BAR0_DMA_BUF_SIZE);
		ll_desc[i].dst = ep_dma_addr + (i * BAR0_DMA_BUF_SIZE);
		ll_desc[i].sz = BAR0_DMA_BUF_SIZE;
	}
	ret = edma_submit_sync_tx(pcie, ll_desc, nents, ch, 1);
	if (ret < 0) {
		dev_err(pcie->dev, "%s: LL WR, SZ: %u B CH: %d failed\n",
			__func__, BAR0_DMA_BUF_SIZE * nents, ch);
		goto fail;
	}
	time = ktime_to_ns(pcie->wr_end_time[ch]) -
		ktime_to_ns(pcie->wr_start_time[ch]);
	dev_info(pcie->dev, "%s: LL WR, CH: %d N: %d SZ: %d B, time: %lld ns\n",
		 __func__, ch, nents, BAR0_DMA_BUF_SIZE, time);

	for (i = 0; i < nents; i++) {
		ll_desc[i].src = ep_dma_addr + (i * BAR0_DMA_BUF_SIZE);
		ll_desc[i].dst = rp_dma_addr + (i * BAR0_DMA_BUF_SIZE);
		ll_desc[i].sz = BAR0_DMA_BUF_SIZE;
	}

	ret = edma_submit_sync_rx(pcie, ll_desc, nents, ch, 1);
	if (ret < 0) {
		dev_err(pcie->dev, "%s: LL RD, SZ: %u B CH: %d failed\n",
			__func__, BAR0_DMA_BUF_SIZE * nents, ch);
		goto fail;
	}
	time = ktime_to_ns(pcie->rd_end_time[ch]) -
		ktime_to_ns(pcie->rd_start_time[ch]);
	dev_info(pcie->dev, "%s: LL RD, CH: %d N: %d SZ: %d B, time: %lld ns\n",
		 __func__, ch, nents, BAR0_DMA_BUF_SIZE, time);

	edma_ll_deinit(pcie);
	edma_deinit(pcie);

fail:
	return 0;
}

static int sanity_test(struct seq_file *s, void *data)
{
	struct tegra_pcie_dw *pcie = (struct tegra_pcie_dw *)
		dev_get_drvdata(s->private);
	struct edma_desc desc;
	struct edma_desc ll_desc[DMA_LL_DEFAULT_SIZE];
	dma_addr_t ep_dma_addr;
	dma_addr_t rp_dma_addr = pcie->dma_phy + BAR0_DMA_BUF_OFFSET;
	int nents = DMA_LL_DEFAULT_SIZE;
	int i, j, ret;
	u32 rp_crc, ep_crc;
	struct pci_dev *pdev = NULL;
	int domain = of_get_pci_domain_nr(pcie->dev->of_node);
	void __iomem *bar0_virt;

	if (pcie->dma_size > SZ_16M) {
		dev_err(pcie->dev, "%s: dma_size should be <= 0x%x\n",
			__func__, SZ_16M);
		goto fail;
	}

	pdev = pci_get_domain_bus_and_slot(domain, pcie->ep_rid >> 8,
					   pcie->ep_rid & 0xff);
	pci_dev_put(pdev);
	if (!pdev) {
		dev_err(pcie->dev, "%s: EP RID: 0x%x not found\n",
			__func__, pcie->ep_rid);
		return 0;
	}

	bar0_virt = devm_ioremap(&pdev->dev, pci_resource_start(pdev, 0),
				 pci_resource_len(pdev, 0));
	if (!bar0_virt) {
		dev_err(pcie->dev, "BAR0 ioremap fail\n");
		return 0;
	}

	ep_dma_addr = pci_resource_start(pdev, 0) + BAR0_DMA_BUF_OFFSET;

	edma_init(pcie, 1);

	/* Direct DMA of size pcie->dma_size */
	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		desc.src = rp_dma_addr;
		desc.dst = ep_dma_addr;
		desc.sz = pcie->dma_size;
		ret = edma_submit_direct_tx(pcie, &desc, i);
		if (ret < 0) {
			dev_err(pcie->dev, "%s: DD WR CH: %d failed\n",
				__func__, i);
			goto fail;
		}
		rp_crc = crc32_le(~0, pcie->dma_virt + BAR0_DMA_BUF_OFFSET,
				  desc.sz);
		ep_crc = crc32_le(~0, bar0_virt + BAR0_DMA_BUF_OFFSET, desc.sz);
		if (rp_crc != ep_crc) {
			dev_err(pcie->dev, "%s: DD WR, SZ: %lu B CH: %d CRC failed\n",
				__func__, desc.sz, i);
			goto fail;
		}
		dev_info(pcie->dev, "%s: DD WR, SZ: %lu B CH: %d success\n",
			 __func__, desc.sz, i);
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		desc.src = ep_dma_addr;
		desc.dst = rp_dma_addr;
		desc.sz = pcie->dma_size;
		ret = edma_submit_direct_rx(pcie, &desc, i);
		if (ret < 0) {
			dev_err(pcie->dev, "%s: DD RD CH: %d failed\n",
				__func__, i);
			goto fail;
		}
		rp_crc = crc32_le(~0, pcie->dma_virt + BAR0_DMA_BUF_OFFSET,
				  desc.sz);
		ep_crc = crc32_le(~0, bar0_virt + BAR0_DMA_BUF_OFFSET, desc.sz);
		if (rp_crc != ep_crc) {
			dev_err(pcie->dev, "%s: DD RD, SZ: %lu B CH: %d CRC failed\n",
				__func__, desc.sz, i);
			goto fail;
		}
		dev_info(pcie->dev, "%s: DD RD, SZ: %lu B CH: %d success\n",
			 __func__, desc.sz, i);
	}

	/* Clean DMA LL */
	memset(pcie->dma_virt + DMA_LL_WR_OFFSET(0), 0, 6 * DMA_LL_SIZE);
	edma_ll_init(pcie);

	/* LL DMA with size pcie->dma_size per desc */
	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		for (j = 0; j < nents; j++) {
			ll_desc[j].src = rp_dma_addr + (j * pcie->dma_size);
			ll_desc[j].dst = ep_dma_addr + (j * pcie->dma_size);
			ll_desc[j].sz = pcie->dma_size;
		}

		ret = edma_submit_sync_tx(pcie, ll_desc, nents, i, 1);
		if (ret < 0) {
			dev_err(pcie->dev, "%s: LL WR CH: %d failed\n",
				__func__, i);
			goto fail;
		}
		rp_crc = crc32_le(~0, pcie->dma_virt + BAR0_DMA_BUF_OFFSET,
				  pcie->dma_size * nents);
		ep_crc = crc32_le(~0, bar0_virt + BAR0_DMA_BUF_OFFSET,
				  pcie->dma_size * nents);
		if (rp_crc != ep_crc) {
			dev_err(pcie->dev, "%s: LL WR, SZ: %u B CH: %d CRC failed\n",
				__func__, pcie->dma_size, i);
			goto fail;
		}
		dev_info(pcie->dev, "%s: LL WR, SZ: %u B CH: %d success\n",
			 __func__, pcie->dma_size, i);
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		for (j = 0; j < nents; j++) {
			ll_desc[j].src = ep_dma_addr + (j * pcie->dma_size);
			ll_desc[j].dst = rp_dma_addr + (j * pcie->dma_size);
			ll_desc[j].sz = pcie->dma_size;
		}

		ret = edma_submit_sync_rx(pcie, ll_desc, nents, i, 1);
		if (ret < 0) {
			dev_err(pcie->dev, "%s: LL RD failed\n", __func__);
			goto fail;
		}
		rp_crc = crc32_le(~0, pcie->dma_virt + BAR0_DMA_BUF_OFFSET,
				  pcie->dma_size * nents);
		ep_crc = crc32_le(~0, bar0_virt + BAR0_DMA_BUF_OFFSET,
				  pcie->dma_size * nents);
		if (rp_crc != ep_crc) {
			dev_err(pcie->dev, "%s: LL RD, SZ: %u B CH: %d CRC failed\n",
				__func__, pcie->dma_size, i);
			goto fail;
		}
		dev_info(pcie->dev, "%s: LL RD, SZ: %u B CH: %d success\n",
			 __func__, pcie->dma_size, i);
	}

	edma_ll_deinit(pcie);
	edma_deinit(pcie);

fail:
	return 0;
}
#endif

static void init_debugfs(struct tegra_pcie_dw *pcie)
{
#if defined(CONFIG_PCIE_RP_DMA_TEST)
	int i;
#endif

	init_aspm_debugfs(pcie);

	debugfs_create_u32("target_speed", 0644, pcie->debugfs,
			   &pcie->target_speed);
	debugfs_create_devm_seqfile(pcie->dev, "apply_speed_change",
				    pcie->debugfs, apply_speed_change);
	debugfs_create_devm_seqfile(pcie->dev, "apply_pme_turnoff",
				    pcie->debugfs, apply_pme_turnoff);
	debugfs_create_devm_seqfile(pcie->dev, "apply_sbr", pcie->debugfs,
				    apply_sbr);
	debugfs_create_u32("flr_rid", 0644, pcie->debugfs, &pcie->flr_rid);
	debugfs_create_devm_seqfile(pcie->dev, "apply_flr", pcie->debugfs,
				    apply_flr);

#if defined(CONFIG_PCIE_RP_DMA_TEST)
	pcie->dma_virt = dma_alloc_coherent(pcie->dev, BAR0_SIZE,
					    &pcie->dma_phy, GFP_KERNEL);
	if (!pcie->dma_virt) {
		dev_err(pcie->dev, "Failed to allocate DMA memory\n");
		return;
	}
	dev_err(pcie->dev, "RP host DMA buf: 0x%llx size: %dn",
		pcie->dma_phy, BAR0_SIZE);
	get_random_bytes(pcie->dma_virt, BAR0_SIZE);

	debugfs_create_devm_seqfile(pcie->dev, "perf_test", pcie->debugfs,
				    perf_test);
	debugfs_create_devm_seqfile(pcie->dev, "sanity_test", pcie->debugfs,
				    sanity_test);

	debugfs_create_u32("dma_size", 0644, pcie->debugfs, &pcie->dma_size);
	/* Set default dma_size as SZ_64K */
	pcie->dma_size = SZ_64K;
	debugfs_create_u32("ep_rid", 0644, pcie->debugfs, &pcie->ep_rid);
	/* Set default to bus=1, devfn=0 */
	pcie->ep_rid = 0x100;

	for (i = 0; i < DMA_WR_CHNL_NUM; i++)
		init_waitqueue_head(&pcie->wr_wq[i]);

	for (i = 0; i < DMA_RD_CHNL_NUM; i++)
		init_waitqueue_head(&pcie->rd_wq[i]);
#endif
}

static void tegra_pcie_enable_fault_interrupts(struct tegra_pcie_dw *pcie)
{
	struct dw_pcie *pci = &pcie->pci;
	u32 val;

	val = appl_readl(pcie, APPL_FAULT_EN_L0);
	val |= APPL_FAULT_EN_L0_TLP_ERR_FAULT_EN;
	val |= APPL_FAULT_EN_L0_RASDP_FAULT_EN;
	val |= APPL_FAULT_EN_L0_PARITY_ERR_FAULT_EN;
	val |= APPL_FAULT_EN_L0_SAFETY_UNCORR_FAULT_EN;
	appl_writel(pcie, val, APPL_FAULT_EN_L0);

	val = appl_readl(pcie, APPL_INTR_EN_L0_0);
	val |= APPL_INTR_EN_L0_0_TLP_ERR_INT_EN;
	val |= APPL_INTR_EN_L0_0_RASDP_INT_EN;
	val |= APPL_INTR_EN_L0_0_PARITY_ERR_INT_EN;
	val |= APPL_INTR_EN_L0_0_SAFETY_UNCORR_INT_EN;
	appl_writel(pcie, val, APPL_INTR_EN_L0_0);

	/* Enable correctable errors reporting */
	val = appl_readl(pcie, APPL_FAULT_EN_L1_11);
	val |= (APPL_FAULT_EN_L1_11_NF_ERR_FAULT_EN | APPL_FAULT_EN_L1_11_F_ERR_FAULT_EN);
	appl_writel(pcie, val, APPL_FAULT_EN_L1_11);

	val = appl_readl(pcie, APPL_INTR_EN_L1_11);
	val |= (APPL_INTR_EN_L1_11_NF_ERR_INT_EN | APPL_INTR_EN_L1_11_F_ERR_INT_EN);
	appl_writel(pcie, val, APPL_INTR_EN_L1_11);

	/* Enable uncorrectable memory ECC */
	val = appl_readl(pcie, APPL_FAULT_EN_L1_12);
	val |= APPL_FAULT_EN_L1_12_SLV_RASDP_ERR;
	val |= APPL_FAULT_EN_L1_12_MSTR_RASDP_ERR;
	appl_writel(pcie, val, APPL_FAULT_EN_L1_12);

	val = appl_readl(pcie, APPL_INTR_EN_L1_12);
	val |= APPL_INTR_EN_L1_12_SLV_RASDP_ERR;
	val |= APPL_INTR_EN_L1_12_MSTR_RASDP_ERR;
	appl_writel(pcie, val, APPL_INTR_EN_L1_12);

	/* Enable interface transaction timeout */
	val = appl_readl(pcie, APPL_FAULT_EN_L1_20);
	val |= APPL_FAULT_EN_L1_20_IF_TIMEOUT;
	appl_writel(pcie, val, APPL_FAULT_EN_L1_20);

	val = appl_readl(pcie, APPL_INTR_EN_L1_20);
	val |= APPL_INTR_EN_L1_20_IF_TIMEOUT;
	appl_writel(pcie, val, APPL_INTR_EN_L1_20);

	val = dw_pcie_readl_dbi(pci, PL_IF_TIMER_CONTROL_OFF);
	val |= PL_IF_TIMER_CONTROL_OFF_IF_TIMER_EN |
		PL_IF_TIMER_CONTROL_OFF_IF_TIMER_AER_EN;
	dw_pcie_writel_dbi(pci, PL_SAFETY_MASK_OFF, val);

	/* Mask all uncorectable error except transaction timeout */
	val = dw_pcie_readl_dbi(pci, PL_SAFETY_MASK_OFF);
	val |= (PL_SAFETY_MASK_OFF_RASDP | PL_SAFETY_MASK_OFF_CDM |
			PL_SAFETY_MASK_OFF_UNCOR | PL_SAFETY_MASK_OFF_COR |
			PL_SAFETY_MASK_OFF_RASDP_COR);
	dw_pcie_writel_dbi(pci, PL_SAFETY_MASK_OFF, val);
}

static void tegra_pcie_enable_system_interrupts(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);
	u32 val;
	u16 val_w;

	val = appl_readl(pcie, APPL_INTR_EN_L0_0);
	val |= APPL_INTR_EN_L0_0_LINK_STATE_INT_EN;
	appl_writel(pcie, val, APPL_INTR_EN_L0_0);

	if (pcie->of_data->sbr_reset_fixup) {
		val = appl_readl(pcie, APPL_INTR_EN_L1_0_0);
		val |= APPL_INTR_EN_L1_0_0_LINK_REQ_RST_NOT_INT_EN;
		appl_writel(pcie, val, APPL_INTR_EN_L1_0_0);
	}

	if (pcie->enable_cdm_check) {
		val = appl_readl(pcie, APPL_INTR_EN_L0_0);
		val |= pcie->of_data->cdm_chk_int_en;
		appl_writel(pcie, val, APPL_INTR_EN_L0_0);

		val = appl_readl(pcie, APPL_FAULT_EN_L0);
		val |= APPL_FAULT_EN_L0_CDM_REG_CHK_FAULT_EN;
		appl_writel(pcie, val, APPL_FAULT_EN_L0);

		val = appl_readl(pcie, APPL_INTR_EN_L1_18);
		val |= APPL_INTR_EN_L1_18_CDM_REG_CHK_CMP_ERR;
		val |= APPL_INTR_EN_L1_18_CDM_REG_CHK_LOGIC_ERR;
		appl_writel(pcie, val, APPL_INTR_EN_L1_18);

		val = appl_readl(pcie, APPL_FAULT_EN_L1_18);
		val |= APPL_FAULT_EN_L1_18_CDM_REG_CHK_CMP_ERR;
		val |= APPL_FAULT_EN_L1_18_CDM_REG_CHK_LOGIC_ERR;
		appl_writel(pcie, val, APPL_FAULT_EN_L1_18);
	}

	if (pcie->is_safety_platform)
		tegra_pcie_enable_fault_interrupts(pcie);

	val_w = dw_pcie_readw_dbi(&pcie->pci, pcie->pcie_cap_base +
				  PCI_EXP_LNKSTA);
	pcie->init_link_width = (val_w & PCI_EXP_LNKSTA_NLW) >>
				PCI_EXP_LNKSTA_NLW_SHIFT;

	val_w = dw_pcie_readw_dbi(&pcie->pci, pcie->pcie_cap_base +
				  PCI_EXP_LNKCTL);
	val_w |= PCI_EXP_LNKCTL_LBMIE;
	dw_pcie_writew_dbi(&pcie->pci, pcie->pcie_cap_base + PCI_EXP_LNKCTL,
			   val_w);
}

static void tegra_pcie_enable_legacy_interrupts(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);
	u32 val;

	/* Enable legacy interrupt generation */
	val = appl_readl(pcie, APPL_INTR_EN_L0_0);
	val |= APPL_INTR_EN_L0_0_SYS_INTR_EN;
	val |= APPL_INTR_EN_L0_0_INT_INT_EN;
	appl_writel(pcie, val, APPL_INTR_EN_L0_0);

	val = appl_readl(pcie, APPL_INTR_EN_L1_8_0);
	val |= APPL_INTR_EN_L1_8_INTX_EN;
	val |= APPL_INTR_EN_L1_8_AUTO_BW_INT_EN;
	val |= APPL_INTR_EN_L1_8_BW_MGT_INT_EN;
	val |= APPL_INTR_EN_L1_8_EDMA_INT_EN;
	if (IS_ENABLED(CONFIG_PCIEAER))
		val |= APPL_INTR_EN_L1_8_AER_INT_EN;
	appl_writel(pcie, val, APPL_INTR_EN_L1_8_0);
}

static void tegra_pcie_enable_msi_interrupts(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);
	u32 val;

	dw_pcie_msi_init(pp);

	/* Enable MSI interrupt generation */
	val = appl_readl(pcie, APPL_INTR_EN_L0_0);
	val |= APPL_INTR_EN_L0_0_SYS_MSI_INTR_EN;
	val |= APPL_INTR_EN_L0_0_MSI_RCV_INT_EN;
	appl_writel(pcie, val, APPL_INTR_EN_L0_0);
}

static void tegra_pcie_enable_interrupts(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);

	/* Clear interrupt statuses before enabling interrupts */
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L0);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_0_0);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_1);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_2);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_3);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_6);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_7);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_8_0);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_9);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_10);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_11);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_13);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_14);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_15);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_17);

	tegra_pcie_enable_system_interrupts(pp);
	tegra_pcie_enable_legacy_interrupts(pp);
	if (IS_ENABLED(CONFIG_PCI_MSI))
		tegra_pcie_enable_msi_interrupts(pp);
}

static void config_gen3_gen4_eq_presets(struct tegra_pcie_dw *pcie)
{
	struct dw_pcie *pci = &pcie->pci;
	u32 val, offset, i;

	/* Program init preset */
	for (i = 0; i < pcie->num_lanes; i++) {
		val = dw_pcie_readw_dbi(pci, CAP_SPCIE_CAP_OFF + (i * 2));
		val &= ~CAP_SPCIE_CAP_OFF_DSP_TX_PRESET0_MASK;
		val |= GEN3_GEN4_EQ_PRESET_INIT;
		val &= ~CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_MASK;
		val |= (GEN3_GEN4_EQ_PRESET_INIT <<
			   CAP_SPCIE_CAP_OFF_USP_TX_PRESET0_SHIFT);
		dw_pcie_writew_dbi(pci, CAP_SPCIE_CAP_OFF + (i * 2), val);

		offset = dw_pcie_find_ext_capability(pci,
						     PCI_EXT_CAP_ID_PL_16GT) +
				PCI_PL_16GT_LE_CTRL;
		val = dw_pcie_readb_dbi(pci, offset + i);
		val &= ~PCI_PL_16GT_LE_CTRL_DSP_TX_PRESET_MASK;
		val |= GEN3_GEN4_EQ_PRESET_INIT;
		val &= ~PCI_PL_16GT_LE_CTRL_USP_TX_PRESET_MASK;
		val |= (GEN3_GEN4_EQ_PRESET_INIT <<
			PCI_PL_16GT_LE_CTRL_USP_TX_PRESET_SHIFT);
		dw_pcie_writeb_dbi(pci, offset + i, val);
	}

	val = dw_pcie_readl_dbi(pci, GEN3_RELATED_OFF);
	val &= ~GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK;
	dw_pcie_writel_dbi(pci, GEN3_RELATED_OFF, val);

	val = dw_pcie_readl_dbi(pci, GEN3_EQ_CONTROL_OFF);
	val &= ~GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_MASK;
	val |= (0x3ff << GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_SHIFT);
	val &= ~GEN3_EQ_CONTROL_OFF_FB_MODE_MASK;
	dw_pcie_writel_dbi(pci, GEN3_EQ_CONTROL_OFF, val);

	val = dw_pcie_readl_dbi(pci, GEN3_RELATED_OFF);
	val &= ~GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK;
	val |= (0x1 << GEN3_RELATED_OFF_RATE_SHADOW_SEL_SHIFT);
	dw_pcie_writel_dbi(pci, GEN3_RELATED_OFF, val);

	val = dw_pcie_readl_dbi(pci, GEN3_EQ_CONTROL_OFF);
	val &= ~GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_MASK;
	val |= (pcie->of_data->gen4_preset_vec <<
		GEN3_EQ_CONTROL_OFF_PSET_REQ_VEC_SHIFT);
	val &= ~GEN3_EQ_CONTROL_OFF_FB_MODE_MASK;
	dw_pcie_writel_dbi(pci, GEN3_EQ_CONTROL_OFF, val);

	val = dw_pcie_readl_dbi(pci, GEN3_RELATED_OFF);
	val &= ~GEN3_RELATED_OFF_RATE_SHADOW_SEL_MASK;
	dw_pcie_writel_dbi(pci, GEN3_RELATED_OFF, val);
}

static void tegra_pcie_prepare_host(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);
	u32 val;
	u16 val_16;

	val_16 = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_DEVCTL);
	val_16 &= ~PCI_EXP_DEVCTL_PAYLOAD;
	val_16 |= PCI_EXP_DEVCTL_PAYLOAD_256B;
	dw_pcie_writew_dbi(pci, pcie->pcie_cap_base + PCI_EXP_DEVCTL, val_16);

	val = dw_pcie_readl_dbi(pci, PCI_IO_BASE);
	val &= ~(IO_BASE_IO_DECODE | IO_BASE_IO_DECODE_BIT8);
	dw_pcie_writel_dbi(pci, PCI_IO_BASE, val);

	val = dw_pcie_readl_dbi(pci, PCI_PREF_MEMORY_BASE);
	val |= CFG_PREF_MEM_LIMIT_BASE_MEM_DECODE;
	val |= CFG_PREF_MEM_LIMIT_BASE_MEM_LIMIT_DECODE;
	dw_pcie_writel_dbi(pci, PCI_PREF_MEMORY_BASE, val);

	dw_pcie_writel_dbi(pci, PCI_BASE_ADDRESS_0, 0);

	/* Enable as 0xFFFF0001 response for CRS */
	val = dw_pcie_readl_dbi(pci, PORT_LOGIC_AMBA_ERROR_RESPONSE_DEFAULT);
	val &= ~(AMBA_ERROR_RESPONSE_CRS_MASK << AMBA_ERROR_RESPONSE_CRS_SHIFT);
	val |= (AMBA_ERROR_RESPONSE_CRS_OKAY_FFFF0001 <<
		AMBA_ERROR_RESPONSE_CRS_SHIFT);
	dw_pcie_writel_dbi(pci, PORT_LOGIC_AMBA_ERROR_RESPONSE_DEFAULT, val);

	/* Configure Max lane width from DT */
	val = dw_pcie_readl_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKCAP);
	val &= ~PCI_EXP_LNKCAP_MLW;
	val |= (pcie->num_lanes << PCI_EXP_LNKSTA_NLW_SHIFT);
	if (tegra_platform_is_fpga()) {
		val &= ~PCI_EXP_LNKCAP_L1EL;
		val |= (0x6 << 15); /* 32us to 64us */
	}
	dw_pcie_writel_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKCAP, val);

	/* Clear Slot Clock Configuration bit if SRNS configuration */
	if (pcie->enable_srns) {
		val_16 = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base +
					   PCI_EXP_LNKSTA);
		val_16 &= ~PCI_EXP_LNKSTA_SLC;
		dw_pcie_writew_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA,
				   val_16);
	}

	if (!tegra_platform_is_fpga())
		config_gen3_gen4_eq_presets(pcie);

	init_host_aspm(pcie);

	/* Disable ASPM-L1SS advertisement as there is no CLKREQ routing */
	if (!pcie->supports_clkreq) {
		disable_aspm_l11(pcie);
		disable_aspm_l12(pcie);
	}

	/* Disable ASPM advertisement as per device-tree configuration */
	if (pcie->disabled_aspm_states & 0x1)
		disable_aspm_l0s(pcie);
	if (pcie->disabled_aspm_states & 0x2) {
		disable_aspm_l10(pcie);
		disable_aspm_l11(pcie);
		disable_aspm_l12(pcie);
	}
	if (pcie->disabled_aspm_states & 0x4)
		disable_aspm_l11(pcie);
	if (pcie->disabled_aspm_states & 0x8)
		disable_aspm_l12(pcie);

	if (pcie->of_data->l1ss_exit_fixup) {
		val = dw_pcie_readl_dbi(pci, GEN3_RELATED_OFF);
		val &= ~GEN3_RELATED_OFF_GEN3_ZRXDC_NONCOMPL;
		dw_pcie_writel_dbi(pci, GEN3_RELATED_OFF, val);
	}

	if (pcie->update_fc_fixup) {
		val = dw_pcie_readl_dbi(pci, CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF);
		val |= 0x1 << CFG_TIMER_CTRL_ACK_NAK_SHIFT;
		dw_pcie_writel_dbi(pci, CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF, val);
	}

	dw_pcie_setup_rc(pp);

	/*
	 * In safety platform link retrain can bump up or down link speed, so
	 * set core clk to Gen4 freq and enable monitor clk.
	 */
	clk_set_rate(pcie->core_clk, GEN4_CORE_CLK_FREQ);

	if (pcie->is_safety_platform)
		if (clk_prepare_enable(pcie->core_clk_m))
			dev_err(pci->dev,
				"Failed to enable monitor core clock\n");

	/* Assert RST */
	val = appl_readl(pcie, APPL_PINMUX);
	val &= ~APPL_PINMUX_PEX_RST;
	appl_writel(pcie, val, APPL_PINMUX);

	usleep_range(100, 200);

	/* Enable LTSSM */
	val = appl_readl(pcie, APPL_CTRL);
	val |= APPL_CTRL_LTSSM_EN;
	appl_writel(pcie, val, APPL_CTRL);

	/* De-assert RST */
	val = appl_readl(pcie, APPL_PINMUX);
	val |= APPL_PINMUX_PEX_RST;
	appl_writel(pcie, val, APPL_PINMUX);

	msleep(100);
}

static int tegra_pcie_dw_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);
	u32 val, tmp, offset, speed, width, link_up_to = pcie->link_up_to, linkup = 0;
	unsigned long freq;

	pp->bridge->ops = &tegra_pci_ops;

	tegra_pcie_prepare_host(pp);

	do {
		if (dw_pcie_wait_for_link(pci) == 0) {
			linkup = 1;
			break;
		}
		if (link_up_to > (LINK_WAIT_MAX_RETRIES * LINK_WAIT_USLEEP_MAX)) {
			link_up_to -= (LINK_WAIT_MAX_RETRIES * LINK_WAIT_USLEEP_MAX);
			dev_info(pci->dev, "Link up timeout set, retrying Link up");
		} else
			break;
	} while (link_up_to);

	if (!linkup) {
		/*
		 * There are some endpoints which can't get the link up if
		 * root port has Data Link Feature (DLF) enabled.
		 * Refer Spec rev 4.0 ver 1.0 sec 3.4.2 & 7.7.4 for more info
		 * on Scaled Flow Control and DLF.
		 * So, need to confirm that is indeed the case here and attempt
		 * link up once again with DLF disabled.
		 */
		val = appl_readl(pcie, APPL_DEBUG);
		val &= APPL_DEBUG_LTSSM_STATE_MASK;
		val >>= APPL_DEBUG_LTSSM_STATE_SHIFT;
		tmp = appl_readl(pcie, APPL_LINK_STATUS);
		tmp &= APPL_LINK_STATUS_RDLH_LINK_UP;
		if (!(val == 0x11 && !tmp)) {
			/* Link is down for all good reasons */
			goto link_down;
		}

		dev_info(pci->dev, "Link is down in DLL");
		dev_info(pci->dev, "Trying again with DLFE disabled\n");
		/* Disable LTSSM */
		val = appl_readl(pcie, APPL_CTRL);
		val &= ~APPL_CTRL_LTSSM_EN;
		appl_writel(pcie, val, APPL_CTRL);

		reset_control_assert(pcie->core_rst);
		reset_control_deassert(pcie->core_rst);

		offset = dw_pcie_find_ext_capability(pci, PCI_EXT_CAP_ID_DLF);
		val = dw_pcie_readl_dbi(pci, offset + PCI_DLF_CAP);
		val &= ~PCI_DLF_EXCHANGE_ENABLE;
		dw_pcie_writel_dbi(pci, offset + PCI_DLF_CAP, val);

		tegra_pcie_prepare_host(pp);

		if (dw_pcie_wait_for_link(pci))
			goto link_down;
	}

	val = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA);

	speed = val & PCI_EXP_LNKSTA_CLS;
	width = (val & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;
	width = find_first_bit((const unsigned long *)&width, 6);

	freq = pcie->dvfs_tbl[width][speed - 1];

#if IS_ENABLED(CONFIG_INTERCONNECT)
	if (pcie->icc_path) {
		if (icc_set_bw(pcie->icc_path, 0, FREQ2ICC(freq)))
			dev_err(pcie->dev, "icc: can't set emc clock[%lu]\n", freq);
	}
#endif

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	if (pcie->emc_bw) {
		if (tegra_bwmgr_set_emc(pcie->emc_bw, freq, TEGRA_BWMGR_SET_EMC_FLOOR))
			dev_err(pcie->dev, "bwmgr: can't set emc clock[%lu]\n", freq);
	}
#endif

	if ((speed > 0) && (speed <= 4) && !pcie->is_safety_platform)
		clk_set_rate(pcie->core_clk, pcie_gen_freq[speed - 1]);

link_down:
	tegra_pcie_enable_interrupts(pp);

	return 0;
}

static int tegra_pcie_dw_link_up(struct dw_pcie *pci)
{
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);
	u32 val = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA);

	return !!(val & PCI_EXP_LNKSTA_DLLLA);
}

static void tegra_pcie_set_msi_vec_num(struct pcie_port *pp)
{
	pp->num_vectors = MAX_MSI_IRQS;
}

static int tegra_pcie_dw_start_link(struct dw_pcie *pci)
{
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);

	if (!pcie->perst_irq_enabled) {
		enable_irq(pcie->pex_rst_irq);
		pcie->perst_irq_enabled = true;
	}

	if (pcie->pex_prsnt_gpiod)
		gpiod_set_value_cansleep(pcie->pex_prsnt_gpiod, 1);

	return 0;
}

static void tegra_pcie_dw_stop_link(struct dw_pcie *pci)
{
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);

	if (pcie->pex_prsnt_gpiod)
		gpiod_set_value_cansleep(pcie->pex_prsnt_gpiod, 0);
}

static const struct dw_pcie_ops tegra_dw_pcie_ops = {
	.link_up = tegra_pcie_dw_link_up,
	.start_link = tegra_pcie_dw_start_link,
	.stop_link = tegra_pcie_dw_stop_link,
};

static struct dw_pcie_host_ops tegra_pcie_dw_host_ops = {
	.host_init = tegra_pcie_dw_host_init,
	.set_num_vectors = tegra_pcie_set_msi_vec_num,
};

static void tegra_pcie_disable_phy(struct tegra_pcie_dw *pcie)
{
	unsigned int phy_count = pcie->phy_count;

	while (phy_count--) {
		phy_power_off(pcie->phys[phy_count]);
		phy_exit(pcie->phys[phy_count]);
	}
}

static int tegra_pcie_enable_phy(struct tegra_pcie_dw *pcie)
{
	unsigned int i;
	int ret;

	for (i = 0; i < pcie->phy_count; i++) {
		ret = phy_init(pcie->phys[i]);
		if (ret < 0)
			goto phy_power_off;

		ret = phy_power_on(pcie->phys[i]);
		if (ret < 0)
			goto phy_exit;

		if (pcie->mode == DW_PCIE_EP_TYPE)
			phy_calibrate(pcie->phys[i]);
	}

	return 0;

phy_power_off:
	while (i--) {
		phy_power_off(pcie->phys[i]);
phy_exit:
		phy_exit(pcie->phys[i]);
	}

	return ret;
}

static int tegra_pcie_dw_parse_dt(struct tegra_pcie_dw *pcie)
{
	struct device_node *np = pcie->dev->of_node;
	int ret;
	enum gpiod_flags flags;

	ret = of_property_read_u32(np, "nvidia,disable-aspm-states",
				   &pcie->disabled_aspm_states);
	if (ret < 0) {
		dev_info(pcie->dev,
			 "Disabling advertisement of all ASPM states\n");
		pcie->disabled_aspm_states = 0xF;
	}

	ret = of_property_read_u32(np, "nvidia,aspm-cmrt-us", &pcie->aspm_cmrt);
	if (ret < 0) {
		dev_info(pcie->dev, "Failed to read ASPM T_cmrt: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "nvidia,aspm-pwr-on-t-us",
				   &pcie->aspm_pwr_on_t);
	if (ret < 0)
		dev_info(pcie->dev, "Failed to read ASPM Power On time: %d\n",
			 ret);

	ret = of_property_read_u32(np, "nvidia,aspm-l0s-entrance-latency-us",
				   &pcie->aspm_l0s_enter_lat);
	if (ret < 0)
		dev_info(pcie->dev,
			 "Failed to read ASPM L0s Entrance latency: %d\n", ret);

	ret = of_property_read_u32(np, "num-lanes", &pcie->num_lanes);
	if (ret < 0) {
		dev_err(pcie->dev, "Failed to read num-lanes: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32_index(np, "nvidia,bpmp", 1, &pcie->cid);
	if (ret) {
		dev_err(pcie->dev, "Failed to read Controller-ID: %d\n", ret);
		return ret;
	}

	if (tegra_platform_is_silicon()) {
		ret = of_property_count_strings(np, "phy-names");
		if (ret < 0) {
			dev_err(pcie->dev, "Failed to find PHY entries: %d\n",
				ret);
			return ret;
		}
		pcie->phy_count = ret;
	}

	if (of_property_read_bool(np, "nvidia,update-fc-fixup"))
		pcie->update_fc_fixup = true;

	pcie->enable_ext_refclk = of_property_read_bool(pcie->dev->of_node,
							"nvidia,enable-ext-refclk");
	/* RP using an external REFCLK is supported only in Tegra234 */
	if (pcie->of_data->version == TEGRA194_DWC_IP_VER) {
		if (pcie->mode == DW_PCIE_RC_TYPE)
			pcie->enable_ext_refclk = false;
		else
			pcie->enable_ext_refclk = true;
	}

	pcie->supports_clkreq =
		of_property_read_bool(pcie->dev->of_node, "supports-clkreq");

	pcie->enable_cdm_check =
		of_property_read_bool(np, "snps,enable-cdm-check");

	pcie->is_safety_platform =
		of_property_read_bool(np, "nvidia,enable-safety");

	pcie->enable_srns = of_property_read_bool(np, "nvidia,enable-srns");

	ret = of_property_read_u32_array(np, "nvidia,dvfs-tbl", &pcie->dvfs_tbl[0][0], 16);
	if (ret < 0) {
		dev_err(pcie->dev, "fail to read EMC BW table: %d\n", ret);
		return ret;
	}

	pcie->disable_power_down =
		of_property_read_bool(np, "nvidia,disable-power-down");

	if (pcie->mode == DW_PCIE_RC_TYPE)
		flags = GPIOD_IN;
	else
		flags = GPIOD_OUT_LOW;

	pcie->pex_prsnt_gpiod = devm_gpiod_get(pcie->dev, "nvidia,pex-prsnt",
					       flags);
	if (IS_ERR(pcie->pex_prsnt_gpiod)) {
		int err = PTR_ERR(pcie->pex_prsnt_gpiod);

		if (err == -EPROBE_DEFER)
			return err;

		dev_dbg(pcie->dev, "Failed to get PCIe PRSNT GPIO: %d\n", err);
		pcie->pex_prsnt_gpiod = NULL;
	}

	ret = of_property_read_u32(np, "nvidia,link_up_to", &pcie->link_up_to);
	/* convert to usec */
	pcie->link_up_to *= 1000;
	if ((ret < 0) || (pcie->link_up_to < (LINK_WAIT_MAX_RETRIES * LINK_WAIT_USLEEP_MAX))) {
		dev_dbg(pcie->dev,
			"configuring default link up timeout\n");
		pcie->link_up_to = (LINK_WAIT_MAX_RETRIES * LINK_WAIT_USLEEP_MAX);
	}

	if (pcie->mode == DW_PCIE_RC_TYPE) {
		pcie->sd_dev_handle = get_sdhci_device_handle(pcie->dev);
		if (!pcie->sd_dev_handle)
			dev_dbg(pcie->dev, "SD7.0 is not supported\n");

		return 0;
	}

	if (tegra_platform_is_fpga()) {
		pcie->pex_rst_gpiod = NULL;
		pcie->pex_refclk_sel_gpiod = NULL;
		return 0;
	}

	/* Endpoint mode specific DT entries */
	pcie->pex_rst_gpiod = devm_gpiod_get(pcie->dev, "reset", GPIOD_IN);
	if (IS_ERR(pcie->pex_rst_gpiod)) {
		int err = PTR_ERR(pcie->pex_rst_gpiod);
		const char *level = KERN_ERR;

		if (err == -EPROBE_DEFER)
			level = KERN_DEBUG;

		dev_printk(level, pcie->dev,
			   dev_fmt("Failed to get PERST GPIO: %d\n"),
			   err);
		return err;
	}

	pcie->pex_refclk_sel_gpiod = devm_gpiod_get_optional(pcie->dev,
							     "nvidia,refclk-select",
							     GPIOD_OUT_HIGH);
	if (IS_ERR(pcie->pex_refclk_sel_gpiod)) {
		int err = PTR_ERR(pcie->pex_refclk_sel_gpiod);
		const char *level = KERN_ERR;

		if (err == -EPROBE_DEFER)
			level = KERN_DEBUG;

		dev_printk(level, pcie->dev,
			   dev_fmt("Failed to get REFCLK select GPIOs: %d\n"),
			   err);
		pcie->pex_refclk_sel_gpiod = NULL;
	}

	return 0;
}

/*
 * Parse msi-parent and gic-v2m resources. On failure don't return error
 * and use default DWC MSI framework.
 */
void tegra_pcie_parse_msi_parent(struct tegra_pcie_dw *pcie)
{
	struct device_node *np = pcie->dev->of_node;
	struct device_node *msi_node;
	int ret;

	msi_node = of_parse_phandle(np, "msi-parent", 0);
	if (!msi_node) {
		dev_dbg(pcie->dev, "Failed to find msi-parent\n");
		return;
	}

	if (!of_device_is_compatible(msi_node, "arm,gic-v2m-frame")) {
		dev_err(pcie->dev, "msi-parent is not gic-v2m\n");
		return;
	}

	ret = of_address_to_resource(msi_node, 0, &pcie->gic_base);
	if (ret) {
		dev_err(pcie->dev, "Failed to allocate gic_base resource\n");
		return;
	}

	ret = of_address_to_resource(msi_node, 1, &pcie->msi_base);
	if (ret) {
		dev_err(pcie->dev, "Failed to allocate msi_base resource\n");
		return;
	}

	dev_info(pcie->dev, "Using GICv2m MSI allocator\n");
	pcie->gic_v2m = true;
}

static int tegra_pcie_bpmp_set_ctrl_state(struct tegra_pcie_dw *pcie,
					  bool enable)
{
	struct mrq_uphy_response resp;
	struct tegra_bpmp_message msg;
	struct mrq_uphy_request req;
	int err;

	/*
	 * Controller-5 doesn't need to have its state set by BPMP-FW in
	 * Tegra194
	 */
	if (pcie->cid == 5 && pcie->of_data->version == 0x490A)
		return 0;

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.cmd = CMD_UPHY_PCIE_CONTROLLER_STATE;
	req.controller_state.pcie_controller = pcie->cid;
	req.controller_state.enable = enable;

	memset(&msg, 0, sizeof(msg));
	msg.mrq = MRQ_UPHY;
	msg.tx.data = &req;
	msg.tx.size = sizeof(req);
	msg.rx.data = &resp;
	msg.rx.size = sizeof(resp);

	err = tegra_bpmp_transfer(pcie->bpmp, &msg);
	if (err)
		return err;
	if (msg.rx.ret)
		return -EINVAL;

	return 0;
}

static int tegra_pcie_bpmp_set_pll_state(struct tegra_pcie_dw *pcie,
					 bool enable)
{
	struct mrq_uphy_response resp;
	struct tegra_bpmp_message msg;
	struct mrq_uphy_request req;
	int err;

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	if (enable) {
		req.cmd = CMD_UPHY_PCIE_EP_CONTROLLER_PLL_INIT;
		req.ep_ctrlr_pll_init.ep_controller = pcie->cid;
	} else {
		req.cmd = CMD_UPHY_PCIE_EP_CONTROLLER_PLL_OFF;
		req.ep_ctrlr_pll_off.ep_controller = pcie->cid;
	}

	memset(&msg, 0, sizeof(msg));
	msg.mrq = MRQ_UPHY;
	msg.tx.data = &req;
	msg.tx.size = sizeof(req);
	msg.rx.data = &resp;
	msg.rx.size = sizeof(resp);

	err = tegra_bpmp_transfer(pcie->bpmp, &msg);
	if (err)
		return err;
	if (msg.rx.ret)
		return -EINVAL;

	return 0;
}

static int tegra_pcie_get_slot_regulators(struct tegra_pcie_dw *pcie)
{
	pcie->slot_ctl_3v3 = devm_regulator_get_optional(pcie->dev, "vpcie3v3");
	if (IS_ERR(pcie->slot_ctl_3v3)) {
		if (PTR_ERR(pcie->slot_ctl_3v3) != -ENODEV)
			return PTR_ERR(pcie->slot_ctl_3v3);

		pcie->slot_ctl_3v3 = NULL;
	}

	pcie->slot_ctl_12v = devm_regulator_get_optional(pcie->dev, "vpcie12v");
	if (IS_ERR(pcie->slot_ctl_12v)) {
		if (PTR_ERR(pcie->slot_ctl_12v) != -ENODEV)
			return PTR_ERR(pcie->slot_ctl_12v);

		pcie->slot_ctl_12v = NULL;
	}

	return 0;
}

static int tegra_pcie_enable_slot_regulators(struct tegra_pcie_dw *pcie)
{
	int ret;

	if (pcie->slot_ctl_3v3) {
		ret = regulator_enable(pcie->slot_ctl_3v3);
		if (ret < 0) {
			dev_err(pcie->dev,
				"Failed to enable 3.3V slot supply: %d\n", ret);
			return ret;
		}
	}

	if (pcie->slot_ctl_12v) {
		ret = regulator_enable(pcie->slot_ctl_12v);
		if (ret < 0) {
			dev_err(pcie->dev,
				"Failed to enable 12V slot supply: %d\n", ret);
			goto fail_12v_enable;
		}
	}

	/*
	 * According to PCI Express Card Electromechanical Specification
	 * Revision 1.1, Table-2.4, T_PVPERL (Power stable to PERST# inactive)
	 * should be a minimum of 100ms.
	 */
	if (pcie->slot_ctl_3v3 || pcie->slot_ctl_12v)
		msleep(100);

	return 0;

fail_12v_enable:
	if (pcie->slot_ctl_3v3)
		regulator_disable(pcie->slot_ctl_3v3);
	return ret;
}

static void tegra_pcie_disable_slot_regulators(struct tegra_pcie_dw *pcie)
{
	if (pcie->slot_ctl_12v)
		regulator_disable(pcie->slot_ctl_12v);
	if (pcie->slot_ctl_3v3)
		regulator_disable(pcie->slot_ctl_3v3);
}

static int tegra_pcie_config_controller(struct tegra_pcie_dw *pcie,
					bool en_hw_hot_rst)
{
	int ret;
	u32 val;

	if (tegra_platform_is_silicon()) {
		ret = tegra_pcie_bpmp_set_ctrl_state(pcie, true);
		if (ret) {
			dev_err(pcie->dev,
				"Failed to enable controller %u: %d\n",
				pcie->cid, ret);
			return ret;
		}

		if (pcie->enable_ext_refclk) {
			ret = tegra_pcie_bpmp_set_pll_state(pcie, true);
			if (ret) {
				dev_err(pcie->dev,
					"Failed to init UPHY for RP: %d\n",
					ret);
				goto fail_pll_init;
			}
		}

		ret = tegra_pcie_enable_slot_regulators(pcie);
		if (ret < 0)
			goto fail_slot_reg_en;

		ret = regulator_enable(pcie->pex_ctl_supply);
		if (ret < 0) {
			dev_err(pcie->dev, "Failed to enable regulator: %d\n",
				ret);
			goto fail_reg_en;
		}
	}

	ret = clk_prepare_enable(pcie->core_clk);
	if (ret) {
		dev_err(pcie->dev, "Failed to enable core clock: %d\n", ret);
		goto fail_core_clk;
	}

	ret = reset_control_deassert(pcie->core_apb_rst);
	if (ret) {
		dev_err(pcie->dev, "Failed to deassert core APB reset: %d\n",
			ret);
		goto fail_core_apb_rst;
	}

	if (pcie->sd_dev_handle) {
		val = readl(pcie->appl_base + APPL_PINMUX);
		if (val & APPL_PINMUX_CLKREQ_IN) {
			/* CLKREQ# is not asserted */
			ret = -EPERM;
			goto fail_phy;
		}
	}

	if (en_hw_hot_rst || !pcie->of_data->sbr_reset_fixup) {
		/* Enable HW_HOT_RST mode */
		val = appl_readl(pcie, APPL_CTRL);
		val &= ~(APPL_CTRL_HW_HOT_RST_MODE_MASK <<
			 APPL_CTRL_HW_HOT_RST_MODE_SHIFT);
		val |= (APPL_CTRL_HW_HOT_RST_MODE_IMDT_RST_LTSSM_EN <<
			APPL_CTRL_HW_HOT_RST_MODE_SHIFT);
		val |= APPL_CTRL_HW_HOT_RST_EN;
		appl_writel(pcie, val, APPL_CTRL);
	}

	if (tegra_platform_is_silicon()) {
		ret = tegra_pcie_enable_phy(pcie);
		if (ret) {
			dev_err(pcie->dev, "Failed to enable PHY: %d\n", ret);
			goto fail_phy;
		}
	}

	/* Update CFG base address */
	appl_writel(pcie, pcie->dbi_res->start & APPL_CFG_BASE_ADDR_MASK,
		    APPL_CFG_BASE_ADDR);

	/* Configure this core for RP mode operation */
	appl_writel(pcie, APPL_DM_TYPE_RP, APPL_DM_TYPE);

	appl_writel(pcie, 0x0, APPL_CFG_SLCG_OVERRIDE);

	val = appl_readl(pcie, APPL_CTRL);
	appl_writel(pcie, val | APPL_CTRL_SYS_PRE_DET_STATE, APPL_CTRL);

	val = appl_readl(pcie, APPL_CFG_MISC);
	val |= (APPL_CFG_MISC_ARCACHE_VAL << APPL_CFG_MISC_ARCACHE_SHIFT);
	appl_writel(pcie, val, APPL_CFG_MISC);

	if (pcie->enable_srns || pcie->enable_ext_refclk) {
		/*
		 * When Tegra PCIe RP is using external clock, it cannot
		 * supply same clock back to EP, which makes it separate clock.
		 * Gate PCIe RP REFCLK out pads when RP & EP are using separate
		 * clock or RP is using external REFCLK.
		 */
		val = appl_readl(pcie, APPL_PINMUX);
		val |= APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE_EN;
		val &= ~APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE;
		appl_writel(pcie, val, APPL_PINMUX);
	}

	if (!pcie->supports_clkreq) {
		val = appl_readl(pcie, APPL_PINMUX);
		val |= APPL_PINMUX_CLKREQ_OVERRIDE_EN;
		val &= ~APPL_PINMUX_CLKREQ_OVERRIDE;
		val &= ~APPL_PINMUX_CLKREQ_DEFAULT_VALUE;
		appl_writel(pcie, val, APPL_PINMUX);
	}

	/* Update iATU_DMA base address */
	appl_writel(pcie,
		    pcie->atu_dma_res->start & APPL_CFG_IATU_DMA_BASE_ADDR_MASK,
		    APPL_CFG_IATU_DMA_BASE_ADDR);

	reset_control_deassert(pcie->core_rst);

	if (tegra_platform_is_fpga()) {
		val = readl(pcie->appl_base + APPL_GTH_PHY);
		val &= ~APPL_GTH_PHY_L1SS_WAKE_COUNT_MASK;
		val |= (0x1e4 << APPL_GTH_PHY_L1SS_WAKE_COUNT_SHIFT);
		/* FPGA PHY initialization */
		val |= APPL_GTH_PHY_PHY_RST;
		writel(val, pcie->appl_base + APPL_GTH_PHY);

		val = dw_pcie_readl_dbi(&pcie->pci, AUX_CLK_FREQ);
		val &= ~(0x3FF);
		val |= 0x6;
		dw_pcie_writel_dbi(&pcie->pci, AUX_CLK_FREQ, val);
	}

	pcie->pcie_cap_base = dw_pcie_find_capability(&pcie->pci,
						      PCI_CAP_ID_EXP);
	pcie->ras_des_cap = dw_pcie_find_ext_capability(&pcie->pci,
							PCI_EXT_CAP_ID_VNDR);

	return ret;

fail_phy:
	reset_control_assert(pcie->core_apb_rst);
fail_core_apb_rst:
	clk_disable_unprepare(pcie->core_clk);
fail_core_clk:
	if (tegra_platform_is_silicon())
		regulator_disable(pcie->pex_ctl_supply);
fail_reg_en:
	if (tegra_platform_is_silicon())
		tegra_pcie_disable_slot_regulators(pcie);
fail_slot_reg_en:
	if (tegra_platform_is_silicon() && pcie->enable_ext_refclk)
		tegra_pcie_bpmp_set_pll_state(pcie, false);
fail_pll_init:
	if (tegra_platform_is_silicon())
		tegra_pcie_bpmp_set_ctrl_state(pcie, false);

	return ret;
}

static void tegra_pcie_unconfig_controller(struct tegra_pcie_dw *pcie)
{
	int ret;

	ret = reset_control_assert(pcie->core_rst);
	if (ret)
		dev_err(pcie->dev, "Failed to assert \"core\" reset: %d\n",
			ret);

	if (tegra_platform_is_silicon())
		tegra_pcie_disable_phy(pcie);

	ret = reset_control_assert(pcie->core_apb_rst);
	if (ret)
		dev_err(pcie->dev, "Failed to assert APB reset: %d\n", ret);

	clk_disable_unprepare(pcie->core_clk);

	if (tegra_platform_is_silicon()) {
		ret = regulator_disable(pcie->pex_ctl_supply);
		if (ret)
			dev_err(pcie->dev, "Failed to disable regulator: %d\n",
				ret);

		tegra_pcie_disable_slot_regulators(pcie);

		if (pcie->enable_ext_refclk) {
			ret = tegra_pcie_bpmp_set_pll_state(pcie, false);
			if (ret)
				dev_err(pcie->dev,
					"Failed to deinit UPHY for RP: %d\n",
					ret);
		}

		ret = tegra_pcie_bpmp_set_ctrl_state(pcie, false);
		if (ret)
			dev_err(pcie->dev, "Failed to disable controller %d: %d\n",
				pcie->cid, ret);
	}
}

static int tegra_pcie_msi_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);

	writel(lower_32_bits(pcie->gic_base.start + V2M_MSI_SETSPI_NS),
	       pcie->appl_base + APPL_SEC_EXTERNAL_MSI_ADDR_L);
	writel(upper_32_bits(pcie->gic_base.start + V2M_MSI_SETSPI_NS),
	       pcie->appl_base + APPL_SEC_EXTERNAL_MSI_ADDR_H);

	writel(lower_32_bits(pcie->msi_base.start),
	       pcie->appl_base + APPL_SEC_INTERNAL_MSI_ADDR_L);
	writel(upper_32_bits(pcie->msi_base.start),
	       pcie->appl_base + APPL_SEC_INTERNAL_MSI_ADDR_H);

	return 0;
}

static int tegra_pcie_init_controller(struct tegra_pcie_dw *pcie)
{
	struct dw_pcie *pci = &pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int ret;

	ret = tegra_pcie_config_controller(pcie, false);
	if (ret < 0)
		return ret;

	if (pcie->gic_v2m)
		tegra_pcie_dw_host_ops.msi_host_init = tegra_pcie_msi_host_init;

	pp->ops = &tegra_pcie_dw_host_ops;

	ret = dw_pcie_host_init(pp);
	if (ret < 0) {
		dev_err(pcie->dev, "Failed to add PCIe port: %d\n", ret);
		goto fail_host_init;
	}

	return 0;

fail_host_init:
	tegra_pcie_unconfig_controller(pcie);
	return ret;
}

static int tegra_pcie_try_link_l2(struct tegra_pcie_dw *pcie)
{
	u32 val;

	if (!tegra_pcie_dw_link_up(&pcie->pci))
		return 0;

	val = appl_readl(pcie, APPL_RADM_STATUS);
	val |= APPL_PM_XMT_TURNOFF_STATE;
	appl_writel(pcie, val, APPL_RADM_STATUS);

	return readl_poll_timeout_atomic(pcie->appl_base + APPL_DEBUG, val,
					 val & APPL_DEBUG_PM_LINKST_IN_L2_LAT,
					 PME_ACK_DELAY, PME_ACK_TIMEOUT);
}

static void tegra_pcie_dw_pme_turnoff(struct tegra_pcie_dw *pcie)
{
	u32 data;
	int err;

	if (!tegra_pcie_dw_link_up(&pcie->pci)) {
		dev_dbg(pcie->dev, "PCIe link is not up...!\n");
		return;
	}

	/*
	 * PCIe controller exit from L2 only if reset is applied, so
	 * controller doesn't handle interrupts. But in cases where
	 * L2 entry fails, PERST# asserted which can trigger surprise
	 * link down AER. However this function call happens in
	 * suspend_noirq(), so AER interrupt will not be processed.
	 * Disable all interrupts to avoid such scenario.
	 */
	appl_writel(pcie, 0x0, APPL_INTR_EN_L0_0);

	if (tegra_pcie_try_link_l2(pcie)) {
		dev_info(pcie->dev, "Link didn't transition to L2 state\n");
		/*
		 * TX lane clock freq will reset to Gen1 only if link is in L2
		 * or detect state.
		 * So apply pex_rst to end point to force RP to go into detect
		 * state
		 */
		data = appl_readl(pcie, APPL_PINMUX);
		data &= ~APPL_PINMUX_PEX_RST;
		appl_writel(pcie, data, APPL_PINMUX);

		err = readl_poll_timeout_atomic(pcie->appl_base + APPL_DEBUG,
						data,
						((data &
						APPL_DEBUG_LTSSM_STATE_MASK) ==
						LTSSM_STATE_DETECT_QUIET) ||
						((data &
						APPL_DEBUG_LTSSM_STATE_MASK) ==
						LTSSM_STATE_DETECT_ACT) ||
						((data &
						APPL_DEBUG_LTSSM_STATE_MASK) ==
						LTSSM_STATE_PRE_DETECT_QUIET) ||
						((data &
						APPL_DEBUG_LTSSM_STATE_MASK) ==
						LTSSM_STATE_DETECT_WAIT),
						LTSSM_DELAY, LTSSM_TIMEOUT);
		if (err)
			dev_info(pcie->dev, "Link didn't go to detect state\n");

		/*
		 * Deassert LTSSM state to stop the state toggling between
		 * polling and detect.
		 */
		data = readl(pcie->appl_base + APPL_CTRL);
		data &= ~APPL_CTRL_LTSSM_EN;
		writel(data, pcie->appl_base + APPL_CTRL);

	}
	/*
	 * DBI registers may not be accessible after this as PLL-E would be
	 * down depending on how CLKREQ is pulled by end point
	 */
	data = appl_readl(pcie, APPL_PINMUX);
	data |= (APPL_PINMUX_CLKREQ_OVERRIDE_EN | APPL_PINMUX_CLKREQ_OVERRIDE);
	/* Cut REFCLK to slot */
	data |= APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE_EN;
	data &= ~APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE;
	appl_writel(pcie, data, APPL_PINMUX);
}

static void tegra_pcie_deinit_controller(struct tegra_pcie_dw *pcie)
{
	struct dw_pcie *pci = &pcie->pci;
	u32 val;
	u16 val_w;

	/*
	 * Surprise down AER error and edma_deinit are racing. Disable
	 * AER error reporting, since controller is going down anyway.
	 */
	val = appl_readl(pcie, APPL_INTR_EN_L1_8_0);
	val &= ~APPL_INTR_EN_L1_8_AER_INT_EN;
	appl_writel(pcie, val, APPL_INTR_EN_L1_8_0);

	val = dw_pcie_readl_dbi(pci, PCI_COMMAND);
	val &= ~PCI_COMMAND_SERR;
	dw_pcie_writel_dbi(pci, PCI_COMMAND, val);

	val_w = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_DEVCTL);
	val_w &= ~(PCI_EXP_DEVCTL_CERE | PCI_EXP_DEVCTL_NFERE | PCI_EXP_DEVCTL_FERE |
		   PCI_EXP_DEVCTL_URRE);
	dw_pcie_writew_dbi(pci, pcie->pcie_cap_base + PCI_EXP_DEVCTL, val_w);

	val_w = dw_pcie_find_ext_capability(pci, PCI_EXT_CAP_ID_ERR);
	val = dw_pcie_readl_dbi(pci, val_w + PCI_ERR_ROOT_STATUS);
	dw_pcie_writel_dbi(pci, val_w + PCI_ERR_ROOT_STATUS, val);

	synchronize_irq(pcie->pci.pp.irq);

	pcie->link_state = false;
	if (pcie->is_safety_platform)
		clk_disable_unprepare(pcie->core_clk_m);
	dw_pcie_host_deinit(&pcie->pci.pp);
	tegra_pcie_dw_pme_turnoff(pcie);
	tegra_pcie_unconfig_controller(pcie);
}

static int tegra_pcie_config_rp(struct tegra_pcie_dw *pcie)
{
	struct device *dev = pcie->dev;
	char *name;
	int ret;

	pm_runtime_enable(dev);

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "Failed to get runtime sync for PCIe dev: %d\n",
			ret);
		goto fail_pm_get_sync;
	}

	ret = tegra_pcie_init_controller(pcie);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize controller: %d\n", ret);
		goto fail_pm_get_sync;
	}

	pcie->link_state = tegra_pcie_dw_link_up(&pcie->pci);
	if (!pcie->link_state && !pcie->disable_power_down) {
		ret = -ENOMEDIUM;
		goto fail_host_init;
	}

	name = devm_kasprintf(dev, GFP_KERNEL, "%pOFP", dev->of_node);
	if (!name) {
		ret = -ENOMEM;
		goto fail_host_init;
	}

	pcie->debugfs = debugfs_create_dir(name, NULL);
	init_debugfs(pcie);

	return ret;

fail_host_init:
	tegra_pcie_deinit_controller(pcie);
fail_pm_get_sync:
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	return ret;
}

static int notify_pcie_from_sd(struct notifier_block *self,
			       unsigned long action,
			       void *dev)
{
	struct tegra_pcie_dw *pcie =
		container_of(self, struct tegra_pcie_dw, nb);
	int ret = NOTIFY_OK;

	switch (action) {
	case CARD_INSERTED:
		dev_dbg(pcie->dev, "SD card is inserted\n");
		/* Currently not doing anything */
		ret = NOTIFY_OK;
		break;
	case CARD_IS_SD_EXPRESS:
		dev_info(pcie->dev, "Enumerating SD Express card\n");
		ret = tegra_pcie_config_rp(pcie);
		if (ret < 0)
			ret = NOTIFY_BAD;
		else
			ret = NOTIFY_OK;
		break;
	case CARD_REMOVED:
		debugfs_remove_recursive(pcie->debugfs);
		tegra_pcie_deinit_controller(pcie);
		pm_runtime_put_sync(pcie->dev);
		pm_runtime_disable(pcie->dev);
		ret = NOTIFY_OK;
		break;
	}

	return ret;
}

static void pex_ep_event_pex_rst_assert(struct tegra_pcie_dw *pcie)
{
	struct dw_pcie *pci = &pcie->pci;
	struct dw_pcie_ep *ep = &pci->ep;
	u32 val;
	int ret;

	if (pcie->ep_state == EP_STATE_DISABLED)
		return;

	/* Endpoint is going away, assert PRSNT# to mask EP from RP until it is ready link up */
	if (pcie->pex_prsnt_gpiod)
		gpiod_set_value_cansleep(pcie->pex_prsnt_gpiod, 0);

	dw_pcie_ep_deinit_notify(ep);

	if (pcie->is_safety_platform)
		clk_disable_unprepare(pcie->core_clk_m);

	ret = readl_poll_timeout(pcie->appl_base + APPL_DEBUG, val,
				 ((val & APPL_DEBUG_LTSSM_STATE_MASK) ==
				 LTSSM_STATE_DETECT_QUIET) ||
				 ((val & APPL_DEBUG_LTSSM_STATE_MASK) ==
				 LTSSM_STATE_DETECT_ACT) ||
				 ((val & APPL_DEBUG_LTSSM_STATE_MASK) ==
				 LTSSM_STATE_PRE_DETECT_QUIET) ||
				 ((val & APPL_DEBUG_LTSSM_STATE_MASK) ==
				 LTSSM_STATE_DETECT_WAIT) ||
				 ((val & APPL_DEBUG_LTSSM_STATE_MASK) ==
				 LTSSM_STATE_L2_IDLE),
				 LTSSM_DELAY, LTSSM_TIMEOUT);
	if (ret)
		dev_err(pcie->dev, "LTSSM state: 0x%x timeout: %d\n", val, ret);

	/*
	 * Deassert LTSSM state to stop the state toggling between
	 * polling and detect.
	 */
	val = appl_readl(pcie, APPL_CTRL);
	val &= ~APPL_CTRL_LTSSM_EN;
	appl_writel(pcie, val, APPL_CTRL);

	reset_control_assert(pcie->core_rst);

	if (tegra_platform_is_silicon())
		tegra_pcie_disable_phy(pcie);

	reset_control_assert(pcie->core_apb_rst);

	clk_disable_unprepare(pcie->core_clk);

	pm_runtime_put_sync(pcie->dev);

	if (tegra_platform_is_silicon() && pcie->enable_ext_refclk) {
		ret = tegra_pcie_bpmp_set_pll_state(pcie, false);
		if (ret)
			dev_err(pcie->dev, "Failed to turn off UPHY: %d\n",
				ret);
	}

	if (tegra_platform_is_silicon()) {
		ret = tegra_pcie_bpmp_set_ctrl_state(pcie, false);
		if (ret)
			dev_err(pcie->dev,
				"Failed to disable controller %u: %d\n",
				pcie->cid, ret);
	}

	pcie->ep_state = EP_STATE_DISABLED;

	dw_pcie_ep_deinit(ep);

	dev_dbg(pcie->dev, "Uninitialization of endpoint is completed\n");
}

static void pex_ep_event_pex_rst_deassert(struct tegra_pcie_dw *pcie)
{
	struct dw_pcie *pci = &pcie->pci;
	struct dw_pcie_ep *ep = &pci->ep;
	struct device *dev = pcie->dev;
	u32 val, ptm_cap_base = 0;
	u16 val_16;
	int ret;

	if (pcie->ep_state == EP_STATE_ENABLED)
		return;

	ret = pm_runtime_resume_and_get(dev);
	if (ret < 0) {
		dev_err(dev, "Failed to get runtime sync for PCIe dev: %d\n",
			ret);
		return;
	}

	if (tegra_platform_is_silicon()) {
		ret = tegra_pcie_bpmp_set_ctrl_state(pcie, true);
		if (ret) {
			dev_err(pcie->dev,
				"Failed to enable controller %u: %d\n",
				pcie->cid, ret);
			goto fail_set_ctrl_state;
		}
	}

	if (tegra_platform_is_silicon() && pcie->enable_ext_refclk) {
		ret = tegra_pcie_bpmp_set_pll_state(pcie, true);
		if (ret) {
			dev_err(dev, "Failed to init UPHY for PCIe EP: %d\n",
				ret);
			goto fail_pll_init;
		}
	}

	ret = clk_prepare_enable(pcie->core_clk);
	if (ret) {
		dev_err(dev, "Failed to enable core clock: %d\n", ret);
		goto fail_core_clk_enable;
	}

	ret = reset_control_deassert(pcie->core_apb_rst);
	if (ret) {
		dev_err(dev, "Failed to deassert core APB reset: %d\n", ret);
		goto fail_core_apb_rst;
	}

	if (tegra_platform_is_silicon()) {
		ret = tegra_pcie_enable_phy(pcie);
		if (ret) {
			dev_err(dev, "Failed to enable PHY: %d\n", ret);
			goto fail_phy;
		}
	}

	/* Clear any stale interrupt statuses */
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L0);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_0_0);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_1);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_2);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_3);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_6);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_7);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_8_0);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_9);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_10);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_11);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_13);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_14);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_15);
	appl_writel(pcie, 0xFFFFFFFF, APPL_INTR_STATUS_L1_17);

	/* configure this core for EP mode operation */
	val = appl_readl(pcie, APPL_DM_TYPE);
	val &= ~APPL_DM_TYPE_MASK;
	val |= APPL_DM_TYPE_EP;
	appl_writel(pcie, val, APPL_DM_TYPE);

	appl_writel(pcie, 0x0, APPL_CFG_SLCG_OVERRIDE);

	val = appl_readl(pcie, APPL_CTRL);
	val |= APPL_CTRL_SYS_PRE_DET_STATE;
	val |= APPL_CTRL_HW_HOT_RST_EN;
	appl_writel(pcie, val, APPL_CTRL);

	val = appl_readl(pcie, APPL_CFG_MISC);
	val |= APPL_CFG_MISC_SLV_EP_MODE;
	val |= (APPL_CFG_MISC_ARCACHE_VAL << APPL_CFG_MISC_ARCACHE_SHIFT);
	appl_writel(pcie, val, APPL_CFG_MISC);

	val = appl_readl(pcie, APPL_PINMUX);
	val |= APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE_EN;
	val |= APPL_PINMUX_CLK_OUTPUT_IN_OVERRIDE;
	if (tegra_platform_is_fpga())
		val &= ~APPL_PINMUX_PEX_RST_IN_OVERRIDE_EN;
	appl_writel(pcie, val, APPL_PINMUX);

	appl_writel(pcie, pcie->dbi_res->start & APPL_CFG_BASE_ADDR_MASK,
		    APPL_CFG_BASE_ADDR);

	appl_writel(pcie, pcie->atu_dma_res->start &
		    APPL_CFG_IATU_DMA_BASE_ADDR_MASK,
		    APPL_CFG_IATU_DMA_BASE_ADDR);

	val = appl_readl(pcie, APPL_INTR_EN_L0_0);
	val |= APPL_INTR_EN_L0_0_SYS_INTR_EN;
	val |= APPL_INTR_EN_L0_0_LINK_STATE_INT_EN;
	val |= APPL_INTR_EN_L0_0_PCI_CMD_EN_INT_EN;
	val |= APPL_INTR_EN_L0_0_INT_INT_EN;
	if (tegra_platform_is_fpga())
		val |= APPL_INTR_EN_L0_0_PEX_RST_INT_EN;
	appl_writel(pcie, val, APPL_INTR_EN_L0_0);

	val = appl_readl(pcie, APPL_INTR_EN_L1_0_0);
	val |= APPL_INTR_EN_L1_0_0_HOT_RESET_DONE_INT_EN;
	val |= APPL_INTR_EN_L1_0_0_RDLH_LINK_UP_INT_EN;
	appl_writel(pcie, val, APPL_INTR_EN_L1_0_0);

	val = appl_readl(pcie, APPL_INTR_EN_L1_8_0);
	val |= APPL_INTR_EN_L1_8_EDMA_INT_EN;
	appl_writel(pcie, val, APPL_INTR_EN_L1_8_0);

	if (pcie->enable_cdm_check) {
		val = appl_readl(pcie, APPL_INTR_EN_L0_0);
		val |= pcie->of_data->cdm_chk_int_en;
		appl_writel(pcie, val, APPL_INTR_EN_L0_0);

		val = appl_readl(pcie, APPL_FAULT_EN_L0);
		val |= APPL_FAULT_EN_L0_CDM_REG_CHK_FAULT_EN;
		appl_writel(pcie, val, APPL_FAULT_EN_L0);

		val = appl_readl(pcie, APPL_INTR_EN_L1_18);
		val |= APPL_INTR_EN_L1_18_CDM_REG_CHK_CMP_ERR;
		val |= APPL_INTR_EN_L1_18_CDM_REG_CHK_LOGIC_ERR;
		appl_writel(pcie, val, APPL_INTR_EN_L1_18);

		val = appl_readl(pcie, APPL_FAULT_EN_L1_18);
		val |= APPL_FAULT_EN_L1_18_CDM_REG_CHK_CMP_ERR;
		val |= APPL_FAULT_EN_L1_18_CDM_REG_CHK_LOGIC_ERR;
		appl_writel(pcie, val, APPL_FAULT_EN_L1_18);
	}

	/* 110us for both snoop and no-snoop */
	val = 110 | (2 << PCI_LTR_SCALE_SHIFT) | LTR_MSG_REQ;
	val |= (val << LTR_MST_NO_SNOOP_SHIFT);
	appl_writel(pcie, val, APPL_LTR_MSG_1);

	reset_control_deassert(pcie->core_rst);

	/* FPGA specific PHY initialization */
	if (tegra_platform_is_fpga()) {
		val = readl(pcie->appl_base + APPL_GTH_PHY);
		val &= ~APPL_GTH_PHY_PHY_RST;
		writel(val, pcie->appl_base + APPL_GTH_PHY);

		usleep_range(900, 1100);

		val = readl(pcie->appl_base + APPL_GTH_PHY);
		val &= ~APPL_GTH_PHY_L1SS_WAKE_COUNT_MASK;
		val |= (0x1e4 << APPL_GTH_PHY_L1SS_WAKE_COUNT_SHIFT);
		val |= APPL_GTH_PHY_PHY_RST;
		writel(val, pcie->appl_base + APPL_GTH_PHY);

		usleep_range(900, 1100);

		val = dw_pcie_readl_dbi(pci, AUX_CLK_FREQ);
		val &= ~(0x3FF);
		val |= 0x6;
		dw_pcie_writel_dbi(pci, AUX_CLK_FREQ, val);
	}

	if (pcie->is_safety_platform)
		tegra_pcie_enable_fault_interrupts(pcie);

	val = dw_pcie_readl_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL);
	val &= ~PORT_LOGIC_SPEED_CHANGE;
	dw_pcie_writel_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL, val);

	if (pcie->update_fc_fixup) {
		val = dw_pcie_readl_dbi(pci, CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF);
		val |= 0x1 << CFG_TIMER_CTRL_ACK_NAK_SHIFT;
		dw_pcie_writel_dbi(pci, CFG_TIMER_CTRL_MAX_FUNC_NUM_OFF, val);
	}

	if (!tegra_platform_is_fpga())
		config_gen3_gen4_eq_presets(pcie);

	init_host_aspm(pcie);

	/* Disable ASPM-L1SS advertisement if there is no CLKREQ routing */
	if (!pcie->supports_clkreq) {
		disable_aspm_l11(pcie);
		disable_aspm_l12(pcie);
	}

	/* Disable ASPM advertisement as per device-tree configuration */
	if (pcie->disabled_aspm_states & 0x1)
		disable_aspm_l0s(pcie);
	if (pcie->disabled_aspm_states & 0x2) {
		disable_aspm_l10(pcie);
		disable_aspm_l11(pcie);
		disable_aspm_l12(pcie);
	}
	if (pcie->disabled_aspm_states & 0x4)
		disable_aspm_l11(pcie);
	if (pcie->disabled_aspm_states & 0x8)
		disable_aspm_l12(pcie);

	if (pcie->of_data->l1ss_exit_fixup) {
		val = dw_pcie_readl_dbi(pci, GEN3_RELATED_OFF);
		val &= ~GEN3_RELATED_OFF_GEN3_ZRXDC_NONCOMPL;
		dw_pcie_writel_dbi(pci, GEN3_RELATED_OFF, val);
	}

	pcie->pcie_cap_base = dw_pcie_find_capability(&pcie->pci,
						      PCI_CAP_ID_EXP);

	val_16 = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base + PCI_EXP_DEVCTL);
	val_16 &= ~PCI_EXP_DEVCTL_PAYLOAD;
	val_16 |= PCI_EXP_DEVCTL_PAYLOAD_256B;
	dw_pcie_writew_dbi(pci, pcie->pcie_cap_base + PCI_EXP_DEVCTL, val_16);

	/* Clear Slot Clock Configuration bit if SRNS configuration */
	if (pcie->enable_srns) {
		val_16 = dw_pcie_readw_dbi(pci, pcie->pcie_cap_base +
					   PCI_EXP_LNKSTA);
		val_16 &= ~PCI_EXP_LNKSTA_SLC;
		dw_pcie_writew_dbi(pci, pcie->pcie_cap_base + PCI_EXP_LNKSTA,
				   val_16);
	}

	/*
	 * In safety platform link retrain can bump up or down link speed, so
	 * set core clk to Gen4 freq and enable monitor clk.
	 */
	clk_set_rate(pcie->core_clk, GEN4_CORE_CLK_FREQ);
	if (pcie->is_safety_platform) {
		if (clk_prepare_enable(pcie->core_clk_m)) {
			dev_err(pcie->dev,
				"Failed to enable monitor core clock\n");
			goto fail_core_clk_m;
		}
	}

	/*
	 * PTM responder capability can be disabled only after disabling
	 * PTM root capability.
	 */
	ptm_cap_base = dw_pcie_find_ext_capability(&pcie->pci,
						   PCI_EXT_CAP_ID_PTM);
	if (ptm_cap_base) {
		dw_pcie_dbi_ro_wr_en(pci);
		val = dw_pcie_readl_dbi(pci, ptm_cap_base + PCI_PTM_CAP);
		val &= ~PCI_PTM_CAP_ROOT;
		dw_pcie_writel_dbi(pci, ptm_cap_base + PCI_PTM_CAP, val);

		val = dw_pcie_readl_dbi(pci, ptm_cap_base + PCI_PTM_CAP);
		val &= ~(PCI_PTM_CAP_RES | PCI_PTM_GRANULARITY_MASK);
		dw_pcie_writel_dbi(pci, ptm_cap_base + PCI_PTM_CAP, val);
		dw_pcie_dbi_ro_wr_dis(pci);
	}

	val = (ep->msi_mem_phys & MSIX_ADDR_MATCH_LOW_OFF_MASK);
	val |= MSIX_ADDR_MATCH_LOW_OFF_EN;
	dw_pcie_writel_dbi(pci, MSIX_ADDR_MATCH_LOW_OFF, val);
	val = (upper_32_bits(ep->msi_mem_phys) & MSIX_ADDR_MATCH_HIGH_OFF_MASK);
	dw_pcie_writel_dbi(pci, MSIX_ADDR_MATCH_HIGH_OFF, val);

	ret = dw_pcie_ep_init_complete(ep);
	if (ret) {
		dev_err(dev, "Failed to complete initialization: %d\n", ret);
		goto fail_init_complete;
	}

	dw_pcie_ep_init_notify(ep);

	/* Send LTR upstream */
	if (!pcie->of_data->ltr_req_fixup) {
		val = appl_readl(pcie, APPL_LTR_MSG_2);
		val |= APPL_LTR_MSG_2_LTR_MSG_REQ_STATE;
		appl_writel(pcie, val, APPL_LTR_MSG_2);
	}

	/* Enable LTSSM */
	val = appl_readl(pcie, APPL_CTRL);
	val |= APPL_CTRL_LTSSM_EN;
	appl_writel(pcie, val, APPL_CTRL);

	pcie->ep_state = EP_STATE_ENABLED;
	dev_dbg(dev, "Initialization of endpoint is completed\n");

	return;

fail_init_complete:
	if (pcie->is_safety_platform)
		clk_disable_unprepare(pcie->core_clk_m);
fail_core_clk_m:
	reset_control_assert(pcie->core_rst);
	if (tegra_platform_is_silicon())
		tegra_pcie_disable_phy(pcie);
fail_phy:
	reset_control_assert(pcie->core_apb_rst);
fail_core_apb_rst:
	clk_disable_unprepare(pcie->core_clk);
fail_core_clk_enable:
	if (tegra_platform_is_silicon())
		tegra_pcie_bpmp_set_pll_state(pcie, false);
fail_pll_init:
	if (tegra_platform_is_silicon())
		tegra_pcie_bpmp_set_ctrl_state(pcie, false);
fail_set_ctrl_state:
	pm_runtime_put_sync(dev);
}

static irqreturn_t tegra_pcie_prsnt_irq(int irq, void *arg)
{
	struct tegra_pcie_dw *pcie = arg;
	int ret;

	wait_event(pcie->config_rp_waitq, pcie->config_rp_done);

	if (!gpiod_get_value(pcie->pex_prsnt_gpiod)) {
		if (!pcie->link_state && !pcie->disable_power_down)
			return IRQ_HANDLED;

		debugfs_remove_recursive(pcie->debugfs);
		tegra_pcie_deinit_controller(pcie);
		pm_runtime_put_sync(pcie->dev);
		pm_runtime_disable(pcie->dev);
	} else {
		if (pcie->link_state)
			return IRQ_HANDLED;

		ret = tegra_pcie_config_rp(pcie);
		if (ret < 0)
			dev_err(pcie->dev, "Failed to link up during PCIe hotplug: %d\n",
				ret);
	}

	return IRQ_HANDLED;
}

static irqreturn_t tegra_pcie_ep_pex_rst_irq(int irq, void *arg)
{
	struct tegra_pcie_dw *pcie = arg;

	if (gpiod_get_value(pcie->pex_rst_gpiod))
		pex_ep_event_pex_rst_assert(pcie);
	else
		pex_ep_event_pex_rst_deassert(pcie);

	return IRQ_HANDLED;
}

static int tegra_pcie_ep_raise_legacy_irq(struct tegra_pcie_dw *pcie, u16 irq)
{
	/* Tegra194 supports only INTA */
	if (irq > 1)
		return -EINVAL;

	appl_writel(pcie, 1, APPL_LEGACY_INTX);
	usleep_range(1000, 2000);
	appl_writel(pcie, 0, APPL_LEGACY_INTX);
	return 0;
}

static int tegra_pcie_ep_raise_msi_irq(struct tegra_pcie_dw *pcie, u16 irq)
{
	if (unlikely(irq > 31))
		return -EINVAL;

	appl_writel(pcie, BIT(irq), APPL_MSI_CTRL_1);

	return 0;
}

static int tegra_pcie_ep_raise_msix_irq(struct tegra_pcie_dw *pcie, u16 irq)
{
	struct dw_pcie_ep *ep = &pcie->pci.ep;

	writel(irq, ep->msi_mem);

	return 0;
}

static int tegra_pcie_ep_raise_irq(struct dw_pcie_ep *ep, u8 func_no,
				   enum pci_epc_irq_type type,
				   u16 interrupt_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);

	switch (type) {
	case PCI_EPC_IRQ_LEGACY:
		return tegra_pcie_ep_raise_legacy_irq(pcie, interrupt_num);

	case PCI_EPC_IRQ_MSI:
		return tegra_pcie_ep_raise_msi_irq(pcie, interrupt_num);

	case PCI_EPC_IRQ_MSIX:
		return tegra_pcie_ep_raise_msix_irq(pcie, interrupt_num);

	default:
		dev_err(pci->dev, "Unknown IRQ type\n");
		return -EPERM;
	}

	return 0;
}

static const struct pci_epc_features tegra_pcie_epc_features = {
	.linkup_notifier = true,
	.core_init_notifier = true,
	.msi_capable = false,
	.msix_capable = false,
	.reserved_bar = 1 << BAR_2 | 1 << BAR_3 | 1 << BAR_4 | 1 << BAR_5,
	.bar_fixed_64bit = 1 << BAR_0,
	.bar_fixed_size[0] = SZ_1M,
	.msi_rcv_bar = BAR_0,
	.msi_rcv_offset = BAR0_MSI_OFFSET,
	.msi_rcv_size = BAR0_MSI_SIZE,
};

static const struct pci_epc_features*
tegra_pcie_ep_get_features(struct dw_pcie_ep *ep)
{
	return &tegra_pcie_epc_features;
}

/* Reserve BAR0_BASE + BAR0_MSI_OFFSET of size SZ_64K as MSI page */
static int tegra_pcie_ep_set_bar(struct dw_pcie_ep *ep, u8 func_no,
				 struct pci_epf_bar *epf_bar)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct tegra_pcie_dw *pcie = to_tegra_pcie(pci);
	enum pci_barno bar = epf_bar->barno;
	dma_addr_t msi_phy = epf_bar->phys_addr + BAR0_MSI_OFFSET;

	if (pcie->gic_v2m && (bar == BAR_0)) {
		appl_writel(pcie, lower_32_bits(msi_phy),
			    APPL_SEC_EXTERNAL_MSI_ADDR_L);
		appl_writel(pcie, upper_32_bits(msi_phy),
			    APPL_SEC_EXTERNAL_MSI_ADDR_H);

		appl_writel(pcie, lower_32_bits(pcie->msi_base.start),
			    APPL_SEC_INTERNAL_MSI_ADDR_L);
		appl_writel(pcie, upper_32_bits(pcie->msi_base.start),
			    APPL_SEC_INTERNAL_MSI_ADDR_H);
	}

	return 0;
}

static struct dw_pcie_ep_ops pcie_ep_ops = {
	.raise_irq = tegra_pcie_ep_raise_irq,
	.get_features = tegra_pcie_ep_get_features,
	.set_bar = tegra_pcie_ep_set_bar,
};

static int tegra_pcie_config_ep(struct tegra_pcie_dw *pcie,
				struct platform_device *pdev)
{
	struct dw_pcie *pci = &pcie->pci;
	struct device *dev = pcie->dev;
	struct dw_pcie_ep *ep;
	struct resource *res;
	char *name;
	int ret;

	ep = &pci->ep;
	ep->ops = &pcie_ep_ops;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "addr_space");
	if (!res)
		return -EINVAL;

	ep->phys_base = res->start;
	ep->addr_size = resource_size(res);
	ep->page_size = SZ_64K;

	if (tegra_platform_is_silicon()) {
		ret = gpiod_set_debounce(pcie->pex_rst_gpiod,
					 PERST_DEBOUNCE_TIME);
		if (ret < 0) {
			dev_err(dev, "Failed to set PERST GPIO debounce time: %d\n",
				ret);
			return ret;
		}

		ret = gpiod_to_irq(pcie->pex_rst_gpiod);
		if (ret < 0) {
			dev_err(dev, "Failed to get IRQ for PERST GPIO: %d\n",
				ret);
			return ret;
		}
		pcie->pex_rst_irq = (unsigned int)ret;

		name = devm_kasprintf(dev, GFP_KERNEL,
				      "tegra_pcie_%u_pex_rst_irq",
				      pcie->cid);
		if (!name) {
			dev_err(dev, "Failed to create PERST IRQ string\n");
			return -ENOMEM;
		}

		pcie->perst_irq_enabled = false;
		irq_set_status_flags(pcie->pex_rst_irq, IRQ_NOAUTOEN);

		ret = devm_request_threaded_irq(dev, pcie->pex_rst_irq, NULL,
						tegra_pcie_ep_pex_rst_irq,
						IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						name, (void *)pcie);
		if (ret < 0) {
			dev_err(dev, "Failed to request IRQ for PERST: %d\n",
				ret);
			return ret;
		}
	}

	pcie->ep_state = EP_STATE_DISABLED;

	name = devm_kasprintf(dev, GFP_KERNEL, "tegra_pcie_%u_ep_work",
			      pcie->cid);
	if (!name) {
		dev_err(dev, "Failed to create PCIe EP work thread string\n");
		return -ENOMEM;
	}

	pm_runtime_enable(dev);

	ret = dw_pcie_ep_init(ep);
	if (ret) {
		dev_err(dev, "Failed to initialize DWC Endpoint subsystem: %d\n",
			ret);
		return ret;
	}

	return 0;
}

static int tegra_pcie_dw_probe(struct platform_device *pdev)
{
	const struct tegra_pcie_of_data *data;
	struct device *dev = &pdev->dev;
	struct resource *atu_dma_res;
	struct tegra_pcie_dw *pcie;
	struct resource *dbi_res;
	struct pcie_port *pp;
	struct dw_pcie *pci;
	struct phy **phys;
	char *name;
	int ret;
	u32 i;

	data = of_device_get_match_data(dev);

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pci = &pcie->pci;
	pci->dev = &pdev->dev;
	pci->ops = &tegra_dw_pcie_ops;
	pci->version = data->version;

	pp = &pci->pp;
	pcie->dev = &pdev->dev;
	pcie->mode = (enum dw_pcie_device_mode)data->mode;
	pcie->of_data = (struct tegra_pcie_of_data *)data;
	pci->n_fts[0] = pcie->of_data->n_fts[0];
	pci->n_fts[1] = pcie->of_data->n_fts[1];

	ret = pinctrl_pm_select_default_state(dev);
	if (ret < 0) {
		dev_err(dev, "Failed to configure sideband pins: %d\n", ret);
		return ret;
	}

	ret = tegra_pcie_dw_parse_dt(pcie);
	if (ret < 0) {
		const char *level = KERN_ERR;

		if (ret == -EPROBE_DEFER)
			level = KERN_DEBUG;

		dev_printk(level, dev,
			   dev_fmt("Failed to parse device tree: %d\n"),
			   ret);
		return ret;
	}

	tegra_pcie_parse_msi_parent(pcie);

	ret = tegra_pcie_get_slot_regulators(pcie);
	if (ret < 0) {
		const char *level = KERN_ERR;

		if (ret == -EPROBE_DEFER)
			level = KERN_DEBUG;

		dev_printk(level, dev,
			   dev_fmt("Failed to get slot regulators: %d\n"),
			   ret);
		return ret;
	}

	if (pcie->pex_refclk_sel_gpiod)
		gpiod_set_value(pcie->pex_refclk_sel_gpiod, 1);

	if (tegra_platform_is_silicon()) {
		pcie->pex_ctl_supply = devm_regulator_get(dev, "vddio-pex-ctl");
		if (IS_ERR(pcie->pex_ctl_supply)) {
			ret = PTR_ERR(pcie->pex_ctl_supply);
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "Failed to get regulator: %ld\n",
					PTR_ERR(pcie->pex_ctl_supply));
			return ret;
		}
	}

	pcie->core_clk = devm_clk_get(dev, "core");
	if (IS_ERR(pcie->core_clk)) {
		dev_err(dev, "Failed to get core clock: %ld\n",
			PTR_ERR(pcie->core_clk));
		return PTR_ERR(pcie->core_clk);
	}

	if (pcie->is_safety_platform) {
		pcie->core_clk_m = devm_clk_get(dev, "core_m");
		if (IS_ERR(pcie->core_clk_m)) {
			dev_err(dev, "Failed to get monitor clock: %ld\n",
				PTR_ERR(pcie->core_clk_m));
			return PTR_ERR(pcie->core_clk_m);
		}
	}

	pcie->appl_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						      "appl");
	if (!pcie->appl_res) {
		dev_err(dev, "Failed to find \"appl\" region\n");
		return -ENODEV;
	}

	pcie->appl_base = devm_ioremap_resource(dev, pcie->appl_res);
	if (IS_ERR(pcie->appl_base))
		return PTR_ERR(pcie->appl_base);

	pcie->core_apb_rst = devm_reset_control_get(dev, "apb");
	if (IS_ERR(pcie->core_apb_rst)) {
		dev_err(dev, "Failed to get APB reset: %ld\n",
			PTR_ERR(pcie->core_apb_rst));
		return PTR_ERR(pcie->core_apb_rst);
	}

	phys = devm_kcalloc(dev, pcie->phy_count, sizeof(*phys), GFP_KERNEL);
	if (!phys)
		return -ENOMEM;

	if (tegra_platform_is_silicon()) {
		for (i = 0; i < pcie->phy_count; i++) {
			name = kasprintf(GFP_KERNEL, "p2u-%u", i);
			if (!name) {
				dev_err(dev, "Failed to create P2U string\n");
				return -ENOMEM;
			}
			phys[i] = devm_phy_get(dev, name);
			kfree(name);
			if (IS_ERR(phys[i])) {
				ret = PTR_ERR(phys[i]);
				if (ret != -EPROBE_DEFER)
					dev_err(dev, "Failed to get PHY: %d\n",
						ret);
				return ret;
			}
		}

		pcie->phys = phys;
	}

	dbi_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	if (!dbi_res) {
		dev_err(dev, "Failed to find \"dbi\" region\n");
		return -ENODEV;
	}
	pcie->dbi_res = dbi_res;

	pci->dbi_base = devm_ioremap_resource(dev, dbi_res);
	if (IS_ERR(pci->dbi_base))
		return PTR_ERR(pci->dbi_base);

	/* Tegra HW locates DBI2 at a fixed offset from DBI */
	pci->dbi_base2 = pci->dbi_base + 0x1000;

	atu_dma_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "atu_dma");
	if (!atu_dma_res) {
		dev_err(dev, "Failed to find \"atu_dma\" region\n");
		return -ENODEV;
	}
	pcie->atu_dma_res = atu_dma_res;

	pci->atu_base = devm_ioremap_resource(dev, atu_dma_res);
	if (IS_ERR(pci->atu_base))
		return PTR_ERR(pci->atu_base);
	pcie->dma_base = pci->atu_base + SZ_128K;

	pcie->core_rst = devm_reset_control_get(dev, "core");
	if (IS_ERR(pcie->core_rst)) {
		dev_err(dev, "Failed to get core reset: %ld\n",
			PTR_ERR(pcie->core_rst));
		return PTR_ERR(pcie->core_rst);
	}

	pp->irq = platform_get_irq_byname(pdev, "intr");
	if (pp->irq < 0)
		return pp->irq;

	if (pcie->of_data->icc_bwmgr) {
#if IS_ENABLED(CONFIG_INTERCONNECT)
		pcie->icc_path = icc_get(dev, pcie_icc_client_id[pcie->cid], TEGRA_ICC_PRIMARY);
		if (IS_ERR_OR_NULL(pcie->icc_path)) {
			ret = IS_ERR(pcie->icc_path) ? PTR_ERR(pcie->icc_path) : -ENODEV;
			dev_info(pcie->dev, "icc bwmgr registration failed: %d\n", ret);
			return ret;
		}
#endif
	} else {
#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
		pcie->emc_bw = tegra_bwmgr_register(pcie_emc_client_id[pcie->cid]);
		if (IS_ERR_OR_NULL(pcie->emc_bw)) {
			ret = IS_ERR(pcie->emc_bw) ? PTR_ERR(pcie->emc_bw) : -ENODEV;
			dev_info(pcie->dev, "bwmgr registration failed: %d\n", ret);
			return ret;
		}
#endif
	}

	pcie->bpmp = tegra_bpmp_get(dev);
	if (IS_ERR(pcie->bpmp))
		return PTR_ERR(pcie->bpmp);

	platform_set_drvdata(pdev, pcie);

	switch (pcie->mode) {
	case DW_PCIE_RC_TYPE:
		pcie->pex_wake_gpiod = devm_gpiod_get_optional(dev, "nvidia,pex-wake",
							       GPIOD_IN);
		if (IS_ERR_OR_NULL(pcie->pex_wake_gpiod)) {
			int err = PTR_ERR(pcie->pex_wake_gpiod);

			if (err == -EPROBE_DEFER)
				goto fail;

			dev_dbg(dev, "Failed to get PCIe wake GPIO: %d\n", err);
			pcie->pex_wake_gpiod = NULL;
		} else {
			device_init_wakeup(dev, true);

			pcie->wake_irq = gpiod_to_irq(pcie->pex_wake_gpiod);
			if (pcie->wake_irq < 0) {
				dev_info(dev, "Invalid pcie_wake irq %d\n", pcie->wake_irq);
				pcie->wake_irq = 0;
			}
		}

		ret = devm_request_threaded_irq(dev, pp->irq, tegra_pcie_rp_irq_handler,
						tegra_pcie_rp_irq_thread,
						IRQF_SHARED,
						"tegra-pcie-intr", (void *)pcie);
		if (ret) {
			dev_err(dev, "Failed to request IRQ %d: %d\n", pp->irq,
				ret);
			goto fail;
		}

		if (IS_ENABLED(CONFIG_PCI_MSI)) {
			pp->msi_irq = of_irq_get_byname(dev->of_node, "msi");
			if (!pp->msi_irq) {
				dev_err(dev, "Failed to get MSI interrupt\n");
				goto fail;
			}
		}

		if (pcie->sd_dev_handle) {
			pcie->nb.notifier_call = notify_pcie_from_sd;
			ret = register_notifier_from_sd(pcie->sd_dev_handle,
							&pcie->nb);
			if (ret < 0) {
				dev_err(dev, "failed to register with SD notify: %d\n",
					ret);
				goto fail;
			}
			/*
			 * Controller initialization in probe and PRSNT#
			 * notification is not required for SD7.0, return from
			 * here.
			 */
			return ret;
		}

		init_waitqueue_head(&pcie->config_rp_waitq);
		pcie->config_rp_done = false;

		if (pcie->pex_prsnt_gpiod) {
			ret = gpiod_to_irq(pcie->pex_prsnt_gpiod);
			if (ret < 0) {
				dev_err(dev, "Failed to get PRSNT IRQ: %d\n",
					ret);
				goto fail;
			}
			pcie->prsnt_irq = (unsigned int)ret;

			name = devm_kasprintf(dev, GFP_KERNEL,
					      "tegra_pcie_%u_prsnt_irq",
					      pcie->cid);
			if (!name) {
				dev_err(dev, "Failed to create PRSNT IRQ string\n");
				goto fail;
			}

			ret = devm_request_threaded_irq(dev,
							pcie->prsnt_irq,
							NULL,
							tegra_pcie_prsnt_irq,
							IRQF_TRIGGER_RISING |
							IRQF_TRIGGER_FALLING |
							IRQF_ONESHOT,
							name, (void *)pcie);
			if (ret < 0) {
				dev_err(dev, "Failed to request IRQ for PRSNT: %d\n",
					ret);
				goto fail;
			}
			if (gpiod_get_value(pcie->pex_prsnt_gpiod))
				ret = tegra_pcie_config_rp(pcie);
		} else {
			ret = tegra_pcie_config_rp(pcie);
		}

		/* Now PRST# IRQ thread is ready to execute */
		pcie->config_rp_done = true;
		wake_up(&pcie->config_rp_waitq);
		if (ret && ret != -ENOMEDIUM)
			goto fail;
		else
			ret = 0;
		break;

	case DW_PCIE_EP_TYPE:
		ret = devm_request_threaded_irq(dev, pp->irq,
						tegra_pcie_ep_hard_irq,
						tegra_pcie_ep_irq_thread,
						IRQF_SHARED,
						"tegra-pcie-ep-intr", pcie);
		if (ret) {
			dev_err(dev, "Failed to request IRQ %d: %d\n", pp->irq,
				ret);
			goto fail;
		}

		ret = tegra_pcie_config_ep(pcie, pdev);
		if (ret < 0)
			goto fail;
		break;

	default:
		dev_err(dev, "Invalid PCIe device type %d\n", pcie->mode);
	}

	return ret;

fail:
	tegra_bpmp_put(pcie->bpmp);
	return ret;
}

static int tegra_pcie_dw_remove(struct platform_device *pdev)
{
	struct tegra_pcie_dw *pcie = platform_get_drvdata(pdev);

	if (pcie->mode == DW_PCIE_RC_TYPE) {
		if (!pcie->link_state && !pcie->disable_power_down)
			return 0;
		if (!pm_runtime_enabled(pcie->dev))
			return 0;
		disable_irq(pcie->prsnt_irq);
		debugfs_remove_recursive(pcie->debugfs);
		tegra_pcie_deinit_controller(pcie);
		pm_runtime_put_sync(pcie->dev);
	} else {
		if (pcie->perst_irq_enabled)
			disable_irq(pcie->pex_rst_irq);
		if (pcie->pex_prsnt_gpiod)
			gpiod_set_value_cansleep(pcie->pex_prsnt_gpiod, 0);
		pex_ep_event_pex_rst_assert(pcie);
	}

#if IS_ENABLED(CONFIG_INTERCONNECT)
	if (pcie->icc_path)
		icc_put(pcie->icc_path);
#endif

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	if (pcie->emc_bw)
		tegra_bwmgr_unregister(pcie->emc_bw);
#endif

	pm_runtime_disable(pcie->dev);
	tegra_bpmp_put(pcie->bpmp);
	if (pcie->pex_refclk_sel_gpiod)
		gpiod_set_value(pcie->pex_refclk_sel_gpiod, 0);

	return 0;
}


static int tegra_pcie_dw_suspend(struct device *dev)
{
	struct tegra_pcie_dw *pcie = dev_get_drvdata(dev);
	int ret;

	if (!pcie->link_state && !pcie->disable_power_down)
		return 0;

	/* wake_irq is set only for RP mode, below check fails for EP which is the intention. */
	if (pcie->wake_irq && device_may_wakeup(dev)) {
		ret = enable_irq_wake(pcie->wake_irq);
		if (ret < 0)
			dev_err(dev, "Failed to enable wake irq: %d\n", ret);
	}

	return 0;
}

static int tegra_pcie_dw_suspend_late(struct device *dev)
{
	struct tegra_pcie_dw *pcie = dev_get_drvdata(dev);
	u32 val;

	if (pcie->mode == DW_PCIE_EP_TYPE) {
		dev_err(dev, "Tegra PCIe is in EP mode, suspend not allowed");
		return -EPERM;
	}

	if (!pcie->link_state && !pcie->disable_power_down)
		return 0;

	/* Enable HW_HOT_RST mode */
	if (pcie->of_data->sbr_reset_fixup) {
		val = appl_readl(pcie, APPL_CTRL);
		val &= ~(APPL_CTRL_HW_HOT_RST_MODE_MASK <<
			 APPL_CTRL_HW_HOT_RST_MODE_SHIFT);
		val |= APPL_CTRL_HW_HOT_RST_EN;
		appl_writel(pcie, val, APPL_CTRL);
	}

	return 0;
}

static int tegra_pcie_dw_suspend_noirq(struct device *dev)
{
	struct tegra_pcie_dw *pcie = dev_get_drvdata(dev);

	if (!pcie->link_state && !pcie->disable_power_down)
		return 0;

	/* Save MSI interrupt vector */
	pcie->msi_ctrl_int = dw_pcie_readl_dbi(&pcie->pci,
					       PORT_LOGIC_MSI_CTRL_INT_0_EN);
	tegra_pcie_dw_pme_turnoff(pcie);
	tegra_pcie_unconfig_controller(pcie);

	return 0;
}

static int tegra_pcie_dw_resume(struct device *dev)
{
	struct tegra_pcie_dw *pcie = dev_get_drvdata(dev);
	int ret;

	if (!pcie->link_state && !pcie->disable_power_down)
		return 0;

	if (pcie->wake_irq && device_may_wakeup(dev)) {
		ret = disable_irq_wake(pcie->wake_irq);
		if (ret < 0)
			dev_err(dev, "Failed to disable wake irq: %d\n", ret);
	}

	return 0;
}

static int tegra_pcie_dw_resume_noirq(struct device *dev)
{
	struct tegra_pcie_dw *pcie = dev_get_drvdata(dev);
	int ret;

	if (!pcie->link_state && !pcie->disable_power_down)
		return 0;

	ret = tegra_pcie_config_controller(pcie, true);
	if (ret < 0)
		return ret;

	if (pcie->gic_v2m) {
		writel(lower_32_bits(pcie->gic_base.start + V2M_MSI_SETSPI_NS),
		       pcie->appl_base + APPL_SEC_EXTERNAL_MSI_ADDR_L);
		writel(upper_32_bits(pcie->gic_base.start + V2M_MSI_SETSPI_NS),
		       pcie->appl_base + APPL_SEC_EXTERNAL_MSI_ADDR_H);

		writel(lower_32_bits(pcie->msi_base.start),
		       pcie->appl_base + APPL_SEC_INTERNAL_MSI_ADDR_L);
		writel(upper_32_bits(pcie->msi_base.start),
		       pcie->appl_base + APPL_SEC_INTERNAL_MSI_ADDR_H);
	}

	ret = tegra_pcie_dw_host_init(&pcie->pci.pp);
	if (ret < 0) {
		dev_err(dev, "Failed to init host: %d\n", ret);
		goto fail_host_init;
	}

	/* Restore MSI interrupt vector */
	dw_pcie_writel_dbi(&pcie->pci, PORT_LOGIC_MSI_CTRL_INT_0_EN,
			   pcie->msi_ctrl_int);

	return 0;

fail_host_init:
	tegra_pcie_unconfig_controller(pcie);
	return ret;
}

static int tegra_pcie_dw_resume_early(struct device *dev)
{
	struct tegra_pcie_dw *pcie = dev_get_drvdata(dev);
	u32 val;

	if (!pcie->link_state && !pcie->disable_power_down)
		return 0;

	/* Disable HW_HOT_RST mode */
	if (pcie->of_data->sbr_reset_fixup) {
		val = appl_readl(pcie, APPL_CTRL);
		val &= ~(APPL_CTRL_HW_HOT_RST_MODE_MASK <<
			 APPL_CTRL_HW_HOT_RST_MODE_SHIFT);
		val |= APPL_CTRL_HW_HOT_RST_MODE_IMDT_RST <<
		       APPL_CTRL_HW_HOT_RST_MODE_SHIFT;
		val &= ~APPL_CTRL_HW_HOT_RST_EN;
		appl_writel(pcie, val, APPL_CTRL);
	}

	return 0;
}

static void tegra_pcie_dw_shutdown(struct platform_device *pdev)
{
	struct tegra_pcie_dw *pcie = platform_get_drvdata(pdev);

	if (pcie->mode == DW_PCIE_RC_TYPE) {
		if (!pcie->link_state && !pcie->disable_power_down)
			return;
		if (!pm_runtime_enabled(pcie->dev))
			return;
		disable_irq(pcie->prsnt_irq);
		disable_irq(pcie->pci.pp.irq);
		if (IS_ENABLED(CONFIG_PCI_MSI))
			disable_irq(pcie->pci.pp.msi_irq);
		tegra_pcie_dw_pme_turnoff(pcie);
		tegra_pcie_unconfig_controller(pcie);
		pm_runtime_put_sync(pcie->dev);
	} else {
		if (pcie->perst_irq_enabled)
			disable_irq(pcie->pex_rst_irq);
		if (pcie->pex_prsnt_gpiod)
			gpiod_set_value_cansleep(pcie->pex_prsnt_gpiod, 0);
		pex_ep_event_pex_rst_assert(pcie);
	}
}

static const struct tegra_pcie_of_data tegra_pcie_of_data_t194 = {
	.version = TEGRA194_DWC_IP_VER,
	.mode = DW_PCIE_RC_TYPE,
	.msix_doorbell_access_fixup = true,
	.sbr_reset_fixup = true,
	.l1ss_exit_fixup = true,
	.ltr_req_fixup = false,
	.cdm_chk_int_en = BIT(19),
	/* Gen4 - 5, 6, 8 and 9 presets enabled */
	.gen4_preset_vec = 0x360,
	.n_fts = { 52, 52 },
	.icc_bwmgr = false,
};

static const struct tegra_pcie_of_data tegra_pcie_of_data_t194_ep = {
	.version = TEGRA194_DWC_IP_VER,
	.mode = DW_PCIE_EP_TYPE,
	.msix_doorbell_access_fixup = false,
	.sbr_reset_fixup = false,
	.l1ss_exit_fixup = true,
	.ltr_req_fixup = true,
	.cdm_chk_int_en = BIT(19),
	/* Gen4 - 5, 6, 8 and 9 presets enabled */
	.gen4_preset_vec = 0x360,
	.n_fts = { 52, 52 },
	.icc_bwmgr = false,
};

static const struct tegra_pcie_of_data tegra_pcie_of_data_t234 = {
	.version = TEGRA234_DWC_IP_VER,
	.mode = DW_PCIE_RC_TYPE,
	.msix_doorbell_access_fixup = false,
	.sbr_reset_fixup = false,
	.l1ss_exit_fixup = false,
	.ltr_req_fixup = false,
	.cdm_chk_int_en = BIT(18),
	/* Gen4 - 6, 8 and 9 presets enabled */
	.gen4_preset_vec = 0x340,
	.n_fts = { 52, 80 },
	.icc_bwmgr = true,
};

static const struct tegra_pcie_of_data tegra_pcie_of_data_t234_ep = {
	.version = TEGRA234_DWC_IP_VER,
	.mode = DW_PCIE_EP_TYPE,
	.msix_doorbell_access_fixup = false,
	.sbr_reset_fixup = false,
	.l1ss_exit_fixup = false,
	.ltr_req_fixup = false,
	.cdm_chk_int_en = BIT(18),
	/* Gen4 - 6, 8 and 9 presets enabled */
	.gen4_preset_vec = 0x340,
	.n_fts = { 52, 80 },
	.icc_bwmgr = true,
};

static const struct of_device_id tegra_pcie_dw_of_match[] = {
	{
		.compatible = "nvidia,tegra194-pcie",
		.data = &tegra_pcie_of_data_t194,
	},
	{
		.compatible = "nvidia,tegra194-pcie-ep",
		.data = &tegra_pcie_of_data_t194_ep,
	},
	{
		.compatible = "nvidia,tegra234-pcie",
		.data = &tegra_pcie_of_data_t234,
	},
	{
		.compatible = "nvidia,tegra234-pcie-ep",
		.data = &tegra_pcie_of_data_t234_ep,
	},
	{},
};

static const struct dev_pm_ops tegra_pcie_dw_pm_ops = {
	.suspend = tegra_pcie_dw_suspend,
	.suspend_late = tegra_pcie_dw_suspend_late,
	.suspend_noirq = tegra_pcie_dw_suspend_noirq,
	.resume = tegra_pcie_dw_resume,
	.resume_noirq = tegra_pcie_dw_resume_noirq,
	.resume_early = tegra_pcie_dw_resume_early,
};

static struct platform_driver tegra_pcie_dw_driver = {
	.probe = tegra_pcie_dw_probe,
	.remove = tegra_pcie_dw_remove,
	.shutdown = tegra_pcie_dw_shutdown,
	.driver = {
		.name	= "tegra194-pcie",
		.pm = &tegra_pcie_dw_pm_ops,
		.of_match_table = tegra_pcie_dw_of_match,
	},
};
#if IS_MODULE(CONFIG_PCIE_TEGRA194)
module_platform_driver(tegra_pcie_dw_driver);
#else
static int __init tegra_pcie_rp_init(void)
{
	return platform_driver_register(&tegra_pcie_dw_driver);
}

late_initcall(tegra_pcie_rp_init);
#endif

MODULE_DEVICE_TABLE(of, tegra_pcie_dw_of_match);

MODULE_AUTHOR("Vidya Sagar <vidyas@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA PCIe host controller driver");
MODULE_LICENSE("GPL v2");
