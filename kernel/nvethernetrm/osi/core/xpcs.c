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

#include "xpcs.h"
#include "core_local.h"

/**
 * @brief xpcs_poll_for_an_complete - Polling for AN complete.
 *
 * Algorithm: This routine poll for AN completion status from
 *		XPCS IP.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[out] an_status: AN status from XPCS
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t xpcs_poll_for_an_complete(struct osi_core_priv_data *osi_core,
					    nveu32_t *an_status)
{
	void *xpcs_base = osi_core->xpcs_base;
	nveu32_t status = 0;
	nveu32_t retry = 1000;
	nveu32_t count;
	nve32_t cond = 1;
	nve32_t ret = 0;

	/* 14. Poll for AN complete */
	cond = 1;
	count = 0;
	while (cond == 1) {
		if (count > retry) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "XPCS AN completion timed out\n", 0ULL);
#ifdef HSI_SUPPORT
			if (osi_core->hsi.enabled == OSI_ENABLE) {
				osi_core->hsi.err_code[AUTONEG_ERR_IDX] =
						OSI_PCS_AUTONEG_ERR;
				osi_core->hsi.report_err = OSI_ENABLE;
				osi_core->hsi.report_count_err[AUTONEG_ERR_IDX] = OSI_ENABLE;
			}
#endif
			ret = -1;
			goto fail;
		}

		count++;

		status = xpcs_read(xpcs_base, XPCS_VR_MII_AN_INTR_STS);
		if ((status & XPCS_VR_MII_AN_INTR_STS_CL37_ANCMPLT_INTR) == 0U) {
			/* autoneg not completed - poll */
			osi_core->osd_ops.udelay(1000U);
		} else {
			/* 15. clear interrupt */
			status &= ~XPCS_VR_MII_AN_INTR_STS_CL37_ANCMPLT_INTR;
			ret = xpcs_write_safety(osi_core, XPCS_VR_MII_AN_INTR_STS, status);
			if (ret != 0) {
				goto fail;
			}
			cond = 0;
		}
	}

	if ((status & XPCS_USXG_AN_STS_SPEED_MASK) == 0U) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "XPCS AN completed with zero speed\n", 0ULL);
		ret = -1;
		goto fail;
	}

	*an_status = status;
fail:
	return ret;
}

/**
 * @brief xpcs_set_speed - Set speed at XPCS
 *
 * Algorithm: This routine program XPCS speed based on AN status.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[in] status: Autonegotation Status.
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static inline nve32_t xpcs_set_speed(struct osi_core_priv_data *osi_core,
				  nveu32_t status)
{
	nveu32_t speed = status & XPCS_USXG_AN_STS_SPEED_MASK;
	nveu32_t ctrl = 0;
	void *xpcs_base = osi_core->xpcs_base;

	ctrl = xpcs_read(xpcs_base, XPCS_SR_MII_CTRL);

	switch (speed) {
	case XPCS_USXG_AN_STS_SPEED_2500:
		/* 2.5Gbps */
		ctrl |= XPCS_SR_MII_CTRL_SS5;
		ctrl &= ~(XPCS_SR_MII_CTRL_SS6 | XPCS_SR_MII_CTRL_SS13);
		break;
	case XPCS_USXG_AN_STS_SPEED_5000:
		/* 5Gbps */
		ctrl |= (XPCS_SR_MII_CTRL_SS5 | XPCS_SR_MII_CTRL_SS13);
		ctrl &= ~XPCS_SR_MII_CTRL_SS6;
		break;
	case XPCS_USXG_AN_STS_SPEED_10000:
	default:
		/* 10Gbps */
		ctrl |= (XPCS_SR_MII_CTRL_SS6 | XPCS_SR_MII_CTRL_SS13);
		ctrl &= ~XPCS_SR_MII_CTRL_SS5;
		break;
	}

	return xpcs_write_safety(osi_core, XPCS_SR_MII_CTRL, ctrl);
}

/**
 * @brief xpcs_start - Start XPCS
 *
 * Algorithm: This routine enables AN and set speed based on AN status
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t xpcs_start(struct osi_core_priv_data *osi_core)
{
	void *xpcs_base = osi_core->xpcs_base;
	nveu32_t an_status = 0;
	nveu32_t retry = RETRY_COUNT;
	nveu32_t count = 0;
	nveu32_t ctrl = 0;
	nve32_t ret = 0;
	nve32_t cond = COND_NOT_MET;

	if (osi_core->xpcs_base == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "XPCS base is NULL", 0ULL);
		ret = -1;
		goto fail;
	}

	if ((osi_core->phy_iface_mode == OSI_USXGMII_MODE_10G) ||
	    (osi_core->phy_iface_mode == OSI_USXGMII_MODE_5G)) {
		ctrl = xpcs_read(xpcs_base, XPCS_SR_MII_CTRL);
		ctrl |= XPCS_SR_MII_CTRL_AN_ENABLE;
		ret = xpcs_write_safety(osi_core, XPCS_SR_MII_CTRL, ctrl);
		if (ret != 0) {
			goto fail;
		}
		ret = xpcs_poll_for_an_complete(osi_core, &an_status);
		if (ret < 0) {
			goto fail;
		}

		ret = xpcs_set_speed(osi_core, an_status);
		if (ret != 0) {
			goto fail;
		}
		/* USXGMII Rate Adaptor Reset before data transfer */
		ctrl = xpcs_read(xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
		ctrl |= XPCS_VR_XS_PCS_DIG_CTRL1_USRA_RST;
		xpcs_write(xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1, ctrl);
		while (cond == COND_NOT_MET) {
			if (count > retry) {
				ret = -1;
				goto fail;
			}

			count++;

			ctrl = xpcs_read(xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
			if ((ctrl & XPCS_VR_XS_PCS_DIG_CTRL1_USRA_RST) == 0U) {
				cond = COND_MET;
			} else {
				osi_core->osd_ops.udelay(1000U);
			}
		}
	}

	/* poll for Rx link up */
	cond = COND_NOT_MET;
	count = 0;
	while (cond == COND_NOT_MET) {
		if (count > retry) {
			ret = -1;
			break;
		}

		count++;

		ctrl = xpcs_read(xpcs_base, XPCS_SR_XS_PCS_STS1);
		if ((ctrl & XPCS_SR_XS_PCS_STS1_RLU) ==
		    XPCS_SR_XS_PCS_STS1_RLU) {
			cond = COND_MET;
		} else {
			/* Maximum wait delay as per HW team is 1msec.
			 * So add a loop for 1000 iterations with 1usec delay,
			 * so that if check get satisfies before 1msec will come
			 * out of loop and it can save some boot time
			 */
			osi_core->osd_ops.udelay(1U);
		}
	}
fail:
	return ret;
}

/**
 * @brief xpcs_uphy_lane_bring_up - Bring up UPHY Tx/Rx lanes
 *
 * Algorithm: This routine bring up the UPHY Tx/Rx lanes
 * through XPCS FSM wrapper.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[in] lane_init_en: Tx/Rx lane init value.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t xpcs_uphy_lane_bring_up(struct osi_core_priv_data *osi_core,
				       nveu32_t lane_init_en)
{
	void *xpcs_base = osi_core->xpcs_base;
	nveu32_t retry = 5U;
	nve32_t cond = COND_NOT_MET;
	nveu32_t val = 0;
	nveu32_t count;
	nve32_t ret = 0;

	val = osi_readla(osi_core,
			 (nveu8_t *)xpcs_base + XPCS_WRAP_UPHY_STATUS);
	if ((val & XPCS_WRAP_UPHY_STATUS_TX_P_UP_STATUS) !=
	    XPCS_WRAP_UPHY_STATUS_TX_P_UP_STATUS) {
		val = osi_readla(osi_core,
				(nveu8_t *)xpcs_base + XPCS_WRAP_UPHY_HW_INIT_CTRL);
		val |= lane_init_en;
		osi_writela(osi_core, val,
				(nveu8_t *)xpcs_base + XPCS_WRAP_UPHY_HW_INIT_CTRL);

		count = 0;
		while (cond == COND_NOT_MET) {
			if (count > retry) {
				ret = -1;
				goto fail;
			}
			count++;

			val = osi_readla(osi_core,
					(nveu8_t *)xpcs_base + XPCS_WRAP_UPHY_HW_INIT_CTRL);
			if ((val & lane_init_en) == OSI_NONE) {
				/* exit loop */
				cond = COND_MET;
			} else {
				/* Max wait time is 1usec.
				 * Most of the time loop got exited in first iteration.
				 * but added an extra count of 4 for safer side
				 */
				osi_core->osd_ops.udelay(1U);
			}
		}
	}

fail:
	return ret;
}

/**
 * @brief xpcs_check_pcs_lock_status - Checks whether PCS lock happened or not.
 *
 * Algorithm: This routine helps to check whether PCS lock happened or not.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t xpcs_check_pcs_lock_status(struct osi_core_priv_data *osi_core)
{
	void *xpcs_base = osi_core->xpcs_base;
	nveu32_t retry = RETRY_COUNT;
	nve32_t cond = COND_NOT_MET;
	nveu32_t val = 0;
	nveu32_t count;
	nve32_t ret = 0;

	count = 0;
	while (cond == COND_NOT_MET) {
		if (count > retry) {
			ret = -1;
			goto fail;
		}
		count++;

		val = osi_readla(osi_core,
				 (nveu8_t *)xpcs_base + XPCS_WRAP_IRQ_STATUS);
		if ((val & XPCS_WRAP_IRQ_STATUS_PCS_LINK_STS) ==
		    XPCS_WRAP_IRQ_STATUS_PCS_LINK_STS) {
			/* exit loop */
			cond = COND_MET;
		} else {
			/* Maximum wait delay as per HW team is 1msec.
			 * So add a loop for 1000 iterations with 1usec delay,
			 * so that if check get satisfies before 1msec will come
			 * out of loop and it can save some boot time
			 */
			osi_core->osd_ops.udelay(1U);
		}
	}

	/* Clear the status */
	osi_writela(osi_core, val, (nveu8_t *)xpcs_base + XPCS_WRAP_IRQ_STATUS);
fail:
	return ret;
}

/**
 * @brief xpcs_lane_bring_up - Bring up UPHY Tx/Rx lanes
 *
 * Algorithm: This routine bring up the UPHY Tx/Rx lanes
 * through XPCS FSM wrapper.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t xpcs_lane_bring_up(struct osi_core_priv_data *osi_core)
{
	struct core_local *l_core = (struct core_local *)(void *)osi_core;
	nveu32_t retry = 7U;
	nveu32_t count;
	nveu32_t val = 0;
	nve32_t cond;
	nve32_t ret = 0;

	if (xpcs_uphy_lane_bring_up(osi_core,
				    XPCS_WRAP_UPHY_HW_INIT_CTRL_TX_EN) < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "UPHY TX lane bring-up failed\n", 0ULL);
		ret = -1;
		goto fail;
	}

	val = osi_readla(osi_core,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);
	/* Step1 RX_SW_OVRD */
	val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_SW_OVRD;
	osi_writela(osi_core, val,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);

	val = osi_readla(osi_core,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);
	/* Step2 RX_IDDQ */
	val &= ~(XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_IDDQ);
	osi_writela(osi_core, val,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);

	val = osi_readla(osi_core,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);
	/* Step2 AUX_RX_IDDQ */
	val &= ~(XPCS_WRAP_UPHY_RX_CONTROL_0_0_AUX_RX_IDDQ);
	osi_writela(osi_core, val,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);

	val = osi_readla(osi_core,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);
	/* Step3 RX_SLEEP */
	val &= ~(XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_SLEEP);
	osi_writela(osi_core, val,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);

	val = osi_readla(osi_core,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);
	/* Step4 RX_CAL_EN */
	val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CAL_EN;
	osi_writela(osi_core, val,
			(nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);

	/* Step5 poll for Rx cal enable */
	cond = COND_NOT_MET;
	count = 0;
	while (cond == COND_NOT_MET) {
		if (count > retry) {
			ret = -1;
			goto fail;
		}

		count++;

		val = osi_readla(osi_core,
				(nveu8_t *)osi_core->xpcs_base +
				XPCS_WRAP_UPHY_RX_CONTROL_0_0);
		if ((val & XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CAL_EN) == 0U) {
			cond = COND_MET;
		} else {
			/* Maximum wait delay as per HW team is 100 usec.
			 * But most of the time as per experiments it takes
			 * around 14usec to satisy the condition, so add a
			 * minimum delay of 14usec and loop it for 7times.
			 * With this 14usec delay condition gets satifies
			 * in first iteration itself.
			 */
			osi_core->osd_ops.udelay(14U);
		}
	}

	/* Step6 RX_DATA_EN */
	val = osi_readla(osi_core, (nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);
	val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_DATA_EN;
	osi_writela(osi_core, val, (nveu8_t *)osi_core->xpcs_base +
			XPCS_WRAP_UPHY_RX_CONTROL_0_0);

	/* Step7 RX_CDR_RESET */
	val = osi_readla(osi_core, (nveu8_t *)osi_core->xpcs_base +
			 XPCS_WRAP_UPHY_RX_CONTROL_0_0);
	val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CDR_RESET;
	osi_writela(osi_core, val, (nveu8_t *)osi_core->xpcs_base +
		    XPCS_WRAP_UPHY_RX_CONTROL_0_0);

	/* Step8 RX_CDR_RESET */
	val = osi_readla(osi_core, (nveu8_t *)osi_core->xpcs_base +
			 XPCS_WRAP_UPHY_RX_CONTROL_0_0);
	val &= ~(XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_CDR_RESET);
	osi_writela(osi_core, val, (nveu8_t *)osi_core->xpcs_base +
		    XPCS_WRAP_UPHY_RX_CONTROL_0_0);

	val = osi_readla(osi_core, (nveu8_t *)osi_core->xpcs_base +
			 XPCS_WRAP_UPHY_RX_CONTROL_0_0);
	/* Step9 RX_PCS_PHY_RDY */
	val |= XPCS_WRAP_UPHY_RX_CONTROL_0_0_RX_PCS_PHY_RDY;
	osi_writela(osi_core, val, (nveu8_t *)osi_core->xpcs_base +
		    XPCS_WRAP_UPHY_RX_CONTROL_0_0);

	if (xpcs_check_pcs_lock_status(osi_core) < 0) {
		if (l_core->lane_status == OSI_ENABLE) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Failed to get PCS block lock\n", 0ULL);
			l_core->lane_status = OSI_DISABLE;
		}
		ret = -1;
		goto fail;
	} else {
		OSI_CORE_INFO(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			      "PCS block lock SUCCESS\n", 0ULL);
		l_core->lane_status = OSI_ENABLE;
	}
fail:
	return ret;
}

/**
 * @brief xpcs_init - XPCS initialization
 *
 * Algorithm: This routine initialize XPCS in USXMII mode.
 *
 * @param[in] osi_core: OSI core data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t xpcs_init(struct osi_core_priv_data *osi_core)
{
	void *xpcs_base = osi_core->xpcs_base;
	nveu32_t retry = 1000;
	nveu32_t count;
	nveu32_t ctrl = 0;
	nve32_t cond = 1;
	nve32_t ret = 0;

	if (osi_core->xpcs_base == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "XPCS base is NULL", 0ULL);
		ret = -1;
		goto fail;
	}

	if (xpcs_lane_bring_up(osi_core) < 0) {
		ret = -1;
		goto fail;
	}

	/* Switching to USXGMII Mode based on
	 * XPCS programming guideline 7.6
	 */

	/* 1. switch DWC_xpcs to BASE-R mode */
	ctrl = xpcs_read(xpcs_base, XPCS_SR_XS_PCS_CTRL2);
	ctrl |= XPCS_SR_XS_PCS_CTRL2_PCS_TYPE_SEL_BASE_R;
	ret = xpcs_write_safety(osi_core, XPCS_SR_XS_PCS_CTRL2, ctrl);
	if (ret != 0) {
		goto fail;
	}
	/* 2. enable USXGMII Mode inside DWC_xpcs */

	/* 3.  USXG_MODE = 10G - default it will be 10G mode */
	if ((osi_core->phy_iface_mode == OSI_USXGMII_MODE_10G) ||
	    (osi_core->phy_iface_mode == OSI_USXGMII_MODE_5G)) {
		ctrl = xpcs_read(xpcs_base, XPCS_VR_XS_PCS_KR_CTRL);
		ctrl &= ~(XPCS_VR_XS_PCS_KR_CTRL_USXG_MODE_MASK);

		if (osi_core->uphy_gbe_mode == OSI_DISABLE) {
			ctrl |= XPCS_VR_XS_PCS_KR_CTRL_USXG_MODE_5G;
		}
	}

	ret = xpcs_write_safety(osi_core, XPCS_VR_XS_PCS_KR_CTRL, ctrl);
	if (ret != 0) {
		goto fail;
	}
	/* 4. Program PHY to operate at 10Gbps/5Gbps/2Gbps
         * this step not required since PHY speed programming
         * already done as part of phy INIT
	 */
	/* 5. Vendor specific software reset */
	ctrl = xpcs_read(xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
	ctrl |= XPCS_VR_XS_PCS_DIG_CTRL1_USXG_EN;
	ret = xpcs_write_safety(osi_core, XPCS_VR_XS_PCS_DIG_CTRL1, ctrl);
	if (ret != 0) {
		goto fail;
	}

	/* XPCS_VR_XS_PCS_DIG_CTRL1_VR_RST bit is self clearing
	 * value readback varification is not needed
	 */
        ctrl |= XPCS_VR_XS_PCS_DIG_CTRL1_VR_RST;
	xpcs_write(xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1, ctrl);

	/* 6. Programming for Synopsys PHY - NA */

	/* 7. poll until vendor specific software reset */
	cond = 1;
	count = 0;
	while (cond == 1) {
		if (count > retry) {
			ret = -1;
			goto fail;
		}

		count++;

		ctrl = xpcs_read(xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
		if ((ctrl & XPCS_VR_XS_PCS_DIG_CTRL1_VR_RST) == 0U) {
			cond = 0;
		} else {
			osi_core->osd_ops.udelay(1000U);
		}
	}

	/* 8. Backplane Ethernet PCS configurations
	 * clear AN_EN in SR_AN_CTRL
	 * set CL37_BP in VR_XS_PCS_DIG_CTRL1
	 */
	if ((osi_core->phy_iface_mode == OSI_USXGMII_MODE_10G) ||
	    (osi_core->phy_iface_mode == OSI_USXGMII_MODE_5G)) {
		ctrl = xpcs_read(xpcs_base, XPCS_SR_AN_CTRL);
		ctrl &= ~XPCS_SR_AN_CTRL_AN_EN;
		ret = xpcs_write_safety(osi_core, XPCS_SR_AN_CTRL, ctrl);
		if (ret != 0) {
			goto fail;
		}
		ctrl = xpcs_read(xpcs_base, XPCS_VR_XS_PCS_DIG_CTRL1);
		ctrl |= XPCS_VR_XS_PCS_DIG_CTRL1_CL37_BP;
		ret = xpcs_write_safety(osi_core, XPCS_VR_XS_PCS_DIG_CTRL1, ctrl);
		if (ret != 0) {
			goto fail;
		}
	}

	/* TODO: 9. MII_AN_INTR_EN to 1, to enable auto-negotiation
	 * complete interrupt */

	/* 10. (Optional step) Duration of link timer change */

	/* 11. XPCS configured as MAC-side USGMII - NA */

	/* 13.  TODO: If there is interrupt enabled for AN interrupt */
fail:
	return ret;
}

#ifndef OSI_STRIPPED_LIB
/**
 * @brief xpcs_eee - XPCS enable/disable EEE
 *
 * Algorithm: This routine update register related to EEE
 * for XPCS.
 *
 * @param[in] osi_core: OSI core data structure.
 * @param[in] en_dis: enable - 1 or disable - 0
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t xpcs_eee(struct osi_core_priv_data *osi_core, nveu32_t en_dis)
{
	void *xpcs_base = osi_core->xpcs_base;
	nveu32_t val = 0x0U;
	nve32_t ret = 0;

	if ((en_dis != OSI_ENABLE) && (en_dis != OSI_DISABLE)) {
		ret = -1;
		goto fail;
	}

	if (xpcs_base == OSI_NULL) {
		ret = -1;
		goto fail;
	}

	if (en_dis == OSI_DISABLE) {
		val = xpcs_read(xpcs_base, XPCS_VR_XS_PCS_EEE_MCTRL0);
		val &= ~XPCS_VR_XS_PCS_EEE_MCTRL0_LTX_EN;
		val &= ~XPCS_VR_XS_PCS_EEE_MCTRL0_LRX_EN;
		ret = xpcs_write_safety(osi_core, XPCS_VR_XS_PCS_EEE_MCTRL0, val);
	} else {

		/* 1. Check if DWC_xpcs supports the EEE feature by
		 * reading the SR_XS_PCS_EEE_ABL register
		 * 1000BASEX-Only is different config then else so can (skip)
		 */

		/* 2. Program various timers used in the EEE mode depending on the
		 * clk_eee_i clock frequency. default times are same as IEEE std
		 * clk_eee_i() is 102MHz. MULT_FACT_100NS = 9 because 9.8ns*10 = 98
		 * which is between 80 and 120  this leads to default setting match
		 */

		val = xpcs_read(xpcs_base, XPCS_VR_XS_PCS_EEE_MCTRL0);
		/* 3. If FEC is enabled in the KR mode (skip in FPGA)*/
		/* 4. enable the EEE feature on the Tx path and Rx path */
		val |= (XPCS_VR_XS_PCS_EEE_MCTRL0_LTX_EN |
				XPCS_VR_XS_PCS_EEE_MCTRL0_LRX_EN);
		ret = xpcs_write_safety(osi_core, XPCS_VR_XS_PCS_EEE_MCTRL0, val);
		if (ret != 0) {
			goto fail;
		}
		/* Transparent Tx LPI Mode Enable */
		val = xpcs_read(xpcs_base, XPCS_VR_XS_PCS_EEE_MCTRL1);
		val |= XPCS_VR_XS_PCS_EEE_MCTRL1_TRN_LPI;
		ret = xpcs_write_safety(osi_core, XPCS_VR_XS_PCS_EEE_MCTRL1, val);
	}
fail:
	return ret;
}
#endif /* !OSI_STRIPPED_LIB */
