/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Defining registers address and its bit definitions of MAX77620 and MAX20024
 *
 * Copyright (C) 2020 NVIDIA CORPORATION. All rights reserved.
 */

#ifndef _MFD_NVVRS_PSEQ_H_
#define _MFD_NVVRS_PSEQ_H_

#include <linux/types.h>

/* Vendor ID */
#define NVVRS_PSEQ_REG_VENDOR_ID		0x00
#define NVVRS_PSEQ_REG_MODEL_REV		0x01

/*  Interrupts and Status registers */
#define NVVRS_PSEQ_REG_INT_SRC1			0x10
#define NVVRS_PSEQ_REG_INT_SRC2			0x11
#define NVVRS_PSEQ_REG_INT_VENDOR		0x12
#define NVVRS_PSEQ_REG_CTL_STAT			0x13
#define NVVRS_PSEQ_REG_EN_STDR1			0x14
#define NVVRS_PSEQ_REG_EN_STDR2			0x15
#define NVVRS_PSEQ_REG_EN_STRD1			0x16
#define NVVRS_PSEQ_REG_EN_STRD2			0x17
#define NVVRS_PSEQ_REG_WDT_STAT			0x18
#define NVVRS_PSEQ_REG_TEST_STAT		0x19
#define NVVRS_PSEQ_REG_LAST_RST			0x1A

/* Configuration Registers */
#define NVVRS_PSEQ_REG_EN_ALT_F			0x20
#define NVVRS_PSEQ_REG_AF_IN_OUT		0x21
#define NVVRS_PSEQ_REG_EN_CFG1			0x22
#define NVVRS_PSEQ_REG_EN_CFG2			0x23
#define NVVRS_PSEQ_REG_CLK_CFG			0x24
#define NVVRS_PSEQ_REG_GP_OUT			0x25
#define NVVRS_PSEQ_REG_DEB_IN			0x26
#define NVVRS_PSEQ_REG_LP_TTSHLD		0x27
#define NVVRS_PSEQ_REG_CTL_1			0x28
#define NVVRS_PSEQ_REG_CTL_2			0x29
#define NVVRS_PSEQ_REG_TEST_CFG			0x2A
#define NVVRS_PSEQ_REG_IEN_VENDOR		0x2B

/* RTC */
#define NVVRS_PSEQ_REG_RTC_T3			0x70
#define NVVRS_PSEQ_REG_RTC_T2			0x71
#define NVVRS_PSEQ_REG_RTC_T1			0x72
#define NVVRS_PSEQ_REG_RTC_T0			0x73
#define NVVRS_PSEQ_REG_RTC_A3			0x74
#define NVVRS_PSEQ_REG_RTC_A2			0x75
#define NVVRS_PSEQ_REG_RTC_A1			0x76
#define NVVRS_PSEQ_REG_RTC_A0			0x77

/* WDT */
#define NVVRS_PSEQ_REG_WDT_CFG			0x80
#define NVVRS_PSEQ_REG_WDT_CLOSE		0x81
#define NVVRS_PSEQ_REG_WDT_OPEN			0x82
#define NVVRS_PSEQ_REG_WDTKEY			0x83

/* Interrupt Mask */
#define NVVRS_PSEQ_INT_SRC1_RSTIRQ_MASK		BIT(0)
#define NVVRS_PSEQ_INT_SRC1_OSC_MASK		BIT(1)
#define NVVRS_PSEQ_INT_SRC1_EN_MASK		BIT(2)
#define NVVRS_PSEQ_INT_SRC1_RTC_MASK		BIT(3)
#define NVVRS_PSEQ_INT_SRC1_PEC_MASK		BIT(4)
#define NVVRS_PSEQ_INT_SRC1_WDT_MASK		BIT(5)
#define NVVRS_PSEQ_INT_SRC1_EM_PD_MASK		BIT(6)
#define NVVRS_PSEQ_INT_SRC1_INTERNAL_MASK	BIT(7)
#define NVVRS_PSEQ_INT_SRC2_PBSP_MASK		BIT(0)
#define NVVRS_PSEQ_INT_SRC2_ECC_DED_MASK	BIT(1)
#define NVVRS_PSEQ_INT_SRC2_TSD_MASK		BIT(2)
#define NVVRS_PSEQ_INT_SRC2_LDO_MASK		BIT(3)
#define NVVRS_PSEQ_INT_SRC2_BIST_MASK		BIT(4)
#define NVVRS_PSEQ_INT_SRC2_RT_CRC_MASK		BIT(5)
#define NVVRS_PSEQ_INT_SRC2_VENDOR_MASK		BIT(7)
#define NVVRS_PSEQ_INT_VENDOR0_MASK		BIT(0)
#define NVVRS_PSEQ_INT_VENDOR1_MASK		BIT(1)
#define NVVRS_PSEQ_INT_VENDOR2_MASK		BIT(2)
#define NVVRS_PSEQ_INT_VENDOR3_MASK		BIT(3)
#define NVVRS_PSEQ_INT_VENDOR4_MASK		BIT(4)
#define NVVRS_PSEQ_INT_VENDOR5_MASK		BIT(5)
#define NVVRS_PSEQ_INT_VENDOR6_MASK		BIT(6)
#define NVVRS_PSEQ_INT_VENDOR7_MASK		BIT(7)

/* Controller Register Mask */
#define NVVRS_PSEQ_REG_CTL_1_FORCE_SHDN		(BIT(0) | BIT(1))
#define NVVRS_PSEQ_REG_CTL_1_FORCE_ACT		BIT(2)
#define NVVRS_PSEQ_REG_CTL_1_FORCE_INT		BIT(3)
#define NVVRS_PSEQ_REG_CTL_2_EN_PEC		BIT(0)
#define NVVRS_PSEQ_REG_CTL_2_REQ_PEC		BIT(1)
#define NVVRS_PSEQ_REG_CTL_2_RTC_PU		BIT(2)
#define NVVRS_PSEQ_REG_CTL_2_RTC_WAKE		BIT(3)
#define NVVRS_PSEQ_REG_CTL_2_RST_DLY		0xF0

enum {
	NVVRS_PSEQ_INT_SRC1_RSTIRQ,		/* Reset or Interrupt Pin Fault */
	NVVRS_PSEQ_INT_SRC1_OSC,		/* Crystal Oscillator Fault */
	NVVRS_PSEQ_INT_SRC1_EN,			/* Enable Output Pin Fault */
	NVVRS_PSEQ_INT_SRC1_RTC,		/* RTC Alarm */
	NVVRS_PSEQ_INT_SRC1_PEC,		/* Packet Error Checking */
	NVVRS_PSEQ_INT_SRC1_WDT,		/* Watchdog Violation */
	NVVRS_PSEQ_INT_SRC1_EM_PD,		/* Emergency Power Down */
	NVVRS_PSEQ_INT_SRC1_INTERNAL,		/* Internal Fault*/
	NVVRS_PSEQ_INT_SRC2_PBSP,		/* PWR_BTN Short Pulse Detection */
	NVVRS_PSEQ_INT_SRC2_ECC_DED,		/* ECC Double-Error Detection */
	NVVRS_PSEQ_INT_SRC2_TSD,		/* Thermal Shutdown */
	NVVRS_PSEQ_INT_SRC2_LDO,		/* LDO Fault */
	NVVRS_PSEQ_INT_SRC2_BIST,		/* Built-In Self Test Fault */
	NVVRS_PSEQ_INT_SRC2_RT_CRC,		/* Runtime Register CRC Fault */
	NVVRS_PSEQ_INT_SRC2_VENDOR,		/* Vendor Specific Internal Fault */
	NVVRS_PSEQ_INT_VENDOR0,			/* Vendor Internal Fault Bit 0 */
	NVVRS_PSEQ_INT_VENDOR1,			/* Vendor Internal Fault Bit 1 */
	NVVRS_PSEQ_INT_VENDOR2,			/* Vendor Internal Fault Bit 2 */
	NVVRS_PSEQ_INT_VENDOR3,			/* Vendor Internal Fault Bit 3 */
	NVVRS_PSEQ_INT_VENDOR4,			/* Vendor Internal Fault Bit 4 */
	NVVRS_PSEQ_INT_VENDOR5,			/* Vendor Internal Fault Bit 5 */
	NVVRS_PSEQ_INT_VENDOR6,			/* Vendor Internal Fault Bit 6 */
	NVVRS_PSEQ_INT_VENDOR7,			/* Vendor Internal Fault Bit 7 */
};

struct nvvrs_pseq_chip {
	struct device *dev;
	struct regmap *rmap;
	int chip_irq;
	struct i2c_client *client;
	struct regmap_irq_chip_data *irq_data;
	struct regmap_irq_chip *irq_chip;
};

#endif /* _MFD_NVVRS_PSEQ_H_ */
