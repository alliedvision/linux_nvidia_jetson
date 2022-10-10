/*
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

#include "../osi/common/common.h"
#include "core_common.h"
#include "mgbe_core.h"
#include "eqos_core.h"

/**
 * @brief hw_est_read - indirect read the GCL to Software own list
 * (SWOL)
 *
 * @param[in] base: MAC base IOVA address.
 * @param[in] addr_val: Address offset for indirect write.
 * @param[in] data: Data to be written at offset.
 * @param[in] gcla: Gate Control List Address, 0 for ETS register.
 *	      1 for GCL memory.
 * @param[in] bunk: Memory bunk from which vlaues will be read. Possible
 *            value 0 or 1.
 * @param[in] mac: MAC index
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static inline nve32_t hw_est_read(struct osi_core_priv_data *osi_core,
				  nveu32_t addr_val, nveu32_t *data,
				  nveu32_t gcla, nveu32_t bunk,
				  nveu32_t mac)
{
	nve32_t retry = 1000;
	nveu32_t val = 0U;
	nve32_t ret;
	const nveu32_t MTL_EST_GCL_CONTROL[MAX_MAC_IP_TYPES] = {
			EQOS_MTL_EST_GCL_CONTROL, MGBE_MTL_EST_GCL_CONTROL};
	const nveu32_t MTL_EST_DATA[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_DATA,
						   MGBE_MTL_EST_DATA};

	*data = 0U;
	val &= ~MTL_EST_ADDR_MASK;
	val |= (gcla == 1U) ? 0x0U : MTL_EST_GCRR;
	val |= MTL_EST_SRWO | MTL_EST_R1W0 | MTL_EST_DBGM | bunk | addr_val;
	osi_writela(osi_core, val, (nveu8_t *)osi_core->base +
		    MTL_EST_GCL_CONTROL[mac]);

	while (--retry > 0) {
		val = osi_readla(osi_core, (nveu8_t *)osi_core->base +
				 MTL_EST_GCL_CONTROL[mac]);
		if ((val & MTL_EST_SRWO) == MTL_EST_SRWO) {
			continue;
		}
		osi_core->osd_ops.udelay(OSI_DELAY_1US);

		break;
	}

	if (((val & MTL_EST_ERR0) == MTL_EST_ERR0) ||
	    (retry <= 0)) {
		ret = -1;
		goto err;
	}

	*data = osi_readla(osi_core, (nveu8_t *)osi_core->base +
			   MTL_EST_DATA[mac]);
	ret = 0;
err:
	return ret;
}

/**
 * @brief eqos_gcl_validate - validate GCL from user
 *
 * Algorithm: validate GCL size and width of time interval value
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] est: Configuration input argument.
 * @param[in] mac: MAC index
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nve32_t gcl_validate(struct osi_core_priv_data *const osi_core,
		     struct osi_est_config *const est,
		     const nveu32_t *btr, nveu32_t mac)
{
	const struct core_local *l_core = (struct core_local *)osi_core;
	const nveu32_t PTP_CYCLE_8[MAX_MAC_IP_TYPES] = {EQOS_8PTP_CYCLE,
						  MGBE_8PTP_CYCLE};
	const nveu32_t MTL_EST_CONTROL[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CONTROL,
						MGBE_MTL_EST_CONTROL};
	const nveu32_t MTL_EST_STATUS[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_STATUS,
						MGBE_MTL_EST_STATUS};
	const nveu32_t MTL_EST_BTR_LOW[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_BTR_LOW,
						MGBE_MTL_EST_BTR_LOW};
	const nveu32_t MTL_EST_BTR_HIGH[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_BTR_HIGH,
						MGBE_MTL_EST_BTR_HIGH};
	const nveu32_t MTL_EST_CTR_LOW[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CTR_LOW,
						MGBE_MTL_EST_CTR_LOW};
	const nveu32_t MTL_EST_CTR_HIGH[MAX_MAC_IP_TYPES] = {EQOS_MTL_EST_CTR_HIGH,
						MGBE_MTL_EST_CTR_HIGH};
	nveu32_t i;
	nveu64_t sum_ti = 0U;
	nveu64_t sum_tin = 0U;
	nveu64_t ctr = 0U;
	nveu64_t btr_new = 0U;
	nveu32_t btr_l, btr_h, ctr_l, ctr_h;
	nveu32_t bunk = 0U;
	nveu32_t est_status;
	nveu64_t old_btr, old_ctr;
	nve32_t ret;
	nveu32_t val = 0U;
	nveu64_t rem = 0U;
	const struct est_read hw_read_arr[4] = {
				    {&btr_l, MTL_EST_BTR_LOW[mac]},
				    {&btr_h, MTL_EST_BTR_HIGH[mac]},
				    {&ctr_l, MTL_EST_CTR_LOW[mac]},
				    {&ctr_h, MTL_EST_CTR_HIGH[mac]}};

	if (est->llr > l_core->gcl_dep) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "input argument more than GCL depth\n",
			     (nveul64_t)est->llr);
		return -1;
	}

	ctr = ((nveu64_t)est->ctr[1] * OSI_NSEC_PER_SEC)  + est->ctr[0];
	btr_new = (((nveu64_t)btr[1] + est->btr_offset[1]) * OSI_NSEC_PER_SEC) +
		   (btr[0] + est->btr_offset[0]);
	for (i = 0U; i < est->llr; i++) {
		if (est->gcl[i] <= l_core->gcl_width_val) {
			sum_ti += ((nveu64_t)est->gcl[i] & l_core->ti_mask);
			if ((sum_ti > ctr) &&
			    ((ctr - sum_tin) >= PTP_CYCLE_8[mac])) {
				continue;
			} else if (((ctr - sum_ti) != 0U) &&
			    ((ctr - sum_ti) < PTP_CYCLE_8[mac])) {
				OSI_CORE_ERR(osi_core->osd,
					     OSI_LOG_ARG_INVALID,
					     "CTR issue due to trancate\n",
					     (nveul64_t)i);
				return -1;
			} else {
				//do nothing
			}
			sum_tin = sum_ti;
			continue;
		}

		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
			     "validation of GCL entry failed\n",
			     (nveul64_t)i);
		return -1;
	}

	/* Check for BTR in case of new ETS while current GCL enabled */

	val = osi_readla(osi_core,
			 (nveu8_t *)osi_core->base +
			 MTL_EST_CONTROL[mac]);
	if ((val & MTL_EST_CONTROL_EEST) != MTL_EST_CONTROL_EEST) {
		return 0;
	}

	/* Read EST_STATUS for bunk */
	est_status = osi_readla(osi_core,
				(nveu8_t *)osi_core->base +
				MTL_EST_STATUS[mac]);
	if ((est_status & MTL_EST_STATUS_SWOL) == 0U) {
		bunk = MTL_EST_DBGB;
	}

	/* Read last BTR and CTR */
	for (i = 0U; i < (sizeof(hw_read_arr) / sizeof(hw_read_arr[0])); i++) {
		ret = hw_est_read(osi_core, hw_read_arr[i].addr,
				  hw_read_arr[i].var, OSI_DISABLE,
				  bunk, mac);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "Reading failed for index\n",
				     (nveul64_t)i);
			return ret;
		}
	}

	old_btr = btr_l + ((nveu64_t)btr_h * OSI_NSEC_PER_SEC);
	old_ctr = ctr_l + ((nveu64_t)ctr_h * OSI_NSEC_PER_SEC);
	if (old_btr > btr_new) {
		rem = (old_btr - btr_new) % old_ctr;
		if ((rem != OSI_NONE) && (rem < PTP_CYCLE_8[mac])) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "invalid BTR", (nveul64_t)rem);
			return -1;
		}
	} else if (btr_new > old_btr) {
		rem = (btr_new - old_btr) % old_ctr;
		if ((rem != OSI_NONE) && (rem < PTP_CYCLE_8[mac])) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_INVALID,
				     "invalid BTR", (nveul64_t)rem);
			return -1;
		}
	} else {
		// Nothing to do
	}

	return 0;
}
