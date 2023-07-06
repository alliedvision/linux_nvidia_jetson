/*
 * Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef XPCS_H_
#define XPCS_H_

#include "../osi/common/common.h"
#include <osi_core.h>

/**
 * @addtogroup XPCS Register offsets
 *
 * @brief XPCS register offsets
 * @{
 */
#define XPCS_ADDRESS				0x03FC
#define XPCS_SR_XS_PCS_STS1			0xC0004
#define XPCS_SR_XS_PCS_CTRL2			0xC001C
#define XPCS_VR_XS_PCS_DIG_CTRL1		0xE0000
#define XPCS_VR_XS_PCS_KR_CTRL			0xE001C
#define XPCS_SR_AN_CTRL				0x1C0000
#define XPCS_SR_MII_CTRL			0x7C0000
#define XPCS_VR_MII_AN_INTR_STS			0x7E0008
#define XPCS_WRAP_UPHY_HW_INIT_CTRL		0x8020
#define XPCS_WRAP_UPHY_STATUS			0x8044
#define XPCS_WRAP_IRQ_STATUS			0x8050
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0		0x801C
/** @} */

#ifndef OSI_STRIPPED_LIB
#define XPCS_VR_XS_PCS_EEE_MCTRL0		0xE0018
#define XPCS_VR_XS_PCS_EEE_MCTRL1		0xE002C

#define XPCS_VR_XS_PCS_EEE_MCTRL1_TRN_LPI	OSI_BIT(0)
#define XPCS_VR_XS_PCS_EEE_MCTRL0_LTX_EN	OSI_BIT(0)
#define XPCS_VR_XS_PCS_EEE_MCTRL0_LRX_EN	OSI_BIT(1)
#endif /* !OSI_STRIPPED_LIB */

/**
 * @addtogroup XPCS-BIT Register bit fileds
 *
 * @brief XPCS register bit fields
 * @{
 */
#define XPCS_SR_XS_PCS_CTRL2_PCS_TYPE_SEL_BASE_R 0x0U
#define XPCS_SR_XS_PCS_STS1_RLU			OSI_BIT(2)
#define XPCS_VR_XS_PCS_DIG_CTRL1_USXG_EN	OSI_BIT(9)
#define XPCS_VR_XS_PCS_DIG_CTRL1_VR_RST		OSI_BIT(15)
#define XPCS_VR_XS_PCS_DIG_CTRL1_USRA_RST	OSI_BIT(10)
#define XPCS_VR_XS_PCS_DIG_CTRL1_CL37_BP	OSI_BIT(12)
#define XPCS_SR_AN_CTRL_AN_EN			OSI_BIT(12)
#define XPCS_SR_MII_CTRL_AN_ENABLE		OSI_BIT(12)
#define XPCS_VR_MII_AN_INTR_STS_CL37_ANCMPLT_INTR OSI_BIT(0)
#define XPCS_SR_MII_CTRL_SS5			OSI_BIT(5)
#define XPCS_SR_MII_CTRL_SS6			OSI_BIT(6)
#define XPCS_SR_MII_CTRL_SS13			OSI_BIT(13)
#define XPCS_USXG_AN_STS_SPEED_MASK		0x1C00U
#define XPCS_USXG_AN_STS_SPEED_2500		0x1000U
#define XPCS_USXG_AN_STS_SPEED_5000		0x1400U
#define XPCS_USXG_AN_STS_SPEED_10000		0xC00U
#define XPCS_REG_ADDR_SHIFT			10U
#define XPCS_REG_ADDR_MASK			0x1FFFU
#define XPCS_REG_VALUE_MASK			0x3FFU
#define XPCS_VR_XS_PCS_KR_CTRL_USXG_MODE_MASK	(OSI_BIT(12) | \
						 OSI_BIT(11) | \
						 OSI_BIT(10))
#define XPCS_VR_XS_PCS_KR_CTRL_USXG_MODE_5G	OSI_BIT(10)
#define XPCS_WRAP_UPHY_HW_INIT_CTRL_TX_EN	OSI_BIT(0)
#define XPCS_WRAP_IRQ_STATUS_PCS_LINK_STS	OSI_BIT(6)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_DATA_EN	OSI_BIT(0)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_IDDQ		OSI_BIT(4)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_AUX_RX_IDDQ	OSI_BIT(5)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_SLEEP		(OSI_BIT(6) | \
							 OSI_BIT(7))
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CAL_EN		OSI_BIT(8)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CDR_RESET	OSI_BIT(9)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_PCS_PHY_RDY	OSI_BIT(10)
#define XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_SW_OVRD	OSI_BIT(31)
#define XPCS_WRAP_UPHY_STATUS_TX_P_UP_STATUS		OSI_BIT(0)

#ifdef HSI_SUPPORT
#define XPCS_WRAP_INTERRUPT_CONTROL			0x8048
#define XPCS_WRAP_INTERRUPT_STATUS			0x8050
#define XPCS_CORE_CORRECTABLE_ERR			OSI_BIT(10)
#define XPCS_CORE_UNCORRECTABLE_ERR			OSI_BIT(9)
#define XPCS_REGISTER_PARITY_ERR			OSI_BIT(8)
#define XPCS_VR_XS_PCS_SFTY_UE_INTR0			0xE03C0
#define XPCS_VR_XS_PCS_SFTY_CE_INTR			0xE03C8
#define XPCS_VR_XS_PCS_SFTY_TMR_CTRL			0xE03D4
#define XPCS_SFTY_1US_MULT_MASK				0xFFU
#define XPCS_SFTY_1US_MULT_SHIFT			0U
#endif
/** @} */

nve32_t xpcs_init(struct osi_core_priv_data *osi_core);
nve32_t xpcs_start(struct osi_core_priv_data *osi_core);
#ifndef OSI_STRIPPED_LIB
nve32_t xpcs_eee(struct osi_core_priv_data *osi_core, nveu32_t en_dis);
#endif /* !OSI_STRIPPED_LIB */

/**
 * @brief xpcs_read - read from xpcs.
 *
 * Algorithm: This routine reads data from XPCS register.
 *
 * @param[in] xpcs_base: XPCS virtual base address
 * @param[in] reg_addr: register address to be read
 *
 * @retval value read from xpcs register.
 */
static inline nveu32_t xpcs_read(void *xpcs_base, nveu32_t reg_addr)
{
	osi_writel(((reg_addr >> XPCS_REG_ADDR_SHIFT) & XPCS_REG_ADDR_MASK),
		   ((nveu8_t *)xpcs_base + XPCS_ADDRESS));
	return osi_readl((nveu8_t *)xpcs_base +
			 ((reg_addr) & XPCS_REG_VALUE_MASK));
}

/**
 * @brief xpcs_write - write to xpcs.
 *
 * Algorithm: This routine writes data to XPCS register.
 *
 * @param[in] xpcs_base: XPCS virtual base address
 * @param[in] reg_addr: register address for writing
 * @param[in] val: write value to register address
 */
static inline void xpcs_write(void *xpcs_base, nveu32_t reg_addr,
			      nveu32_t val)
{
	osi_writel(((reg_addr >> XPCS_REG_ADDR_SHIFT) & XPCS_REG_ADDR_MASK),
		   ((nveu8_t *)xpcs_base + XPCS_ADDRESS));
	osi_writel(val, (nveu8_t *)xpcs_base +
		   (((reg_addr) & XPCS_REG_VALUE_MASK)));
}

/**
 * @brief xpcs_write_safety - write to xpcs.
 *
 * Algorithm: This routine writes data to XPCS register.
 * And verifiy by reading back the value
 *
 * @param[in] osi_core: OSI core data structure
 * @param[in] reg_addr: register address for writing
 * @param[in] val: write value to register address
 *
 * @retval 0 on success
 * @retval XPCS_WRITE_FAIL_CODE on failure
 *
 */
static inline nve32_t xpcs_write_safety(struct osi_core_priv_data *osi_core,
				    nveu32_t reg_addr,
				    nveu32_t val)
{
	void *xpcs_base = osi_core->xpcs_base;
	nveu32_t read_val;
	nve32_t retry = 10;
	nve32_t ret = XPCS_WRITE_FAIL_CODE;

	while (--retry > 0) {
		xpcs_write(xpcs_base, reg_addr, val);
		read_val = xpcs_read(xpcs_base, reg_addr);
		if (val == read_val) {
			ret = 0;
			break;
		}
		osi_core->osd_ops.udelay(OSI_DELAY_1US);
	}

	if (ret != 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "xpcs_write_safety failed", reg_addr);
	}

	return ret;
}
#endif
