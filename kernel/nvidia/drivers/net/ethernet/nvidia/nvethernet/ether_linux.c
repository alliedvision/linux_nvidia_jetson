/*
 * Copyright (c) 2018-2024, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/version.h>
#include <linux/iommu.h>
#ifdef HSI_SUPPORT
#include <linux/tegra-epl.h>
#endif
#include "ether_linux.h"

int ether_get_tx_ts(struct ether_priv_data *pdata)
{
	struct list_head *head_node, *temp_head_node;
	struct skb_shared_hwtstamps shhwtstamp;
	struct osi_ioctl ioctl_data = {};
	unsigned long long nsec = 0x0;
	struct ether_tx_ts_skb_list *pnode;
	int ret = -1;
	unsigned long flags;
	bool pending = false;

	if (!atomic_inc_and_test(&pdata->tx_ts_ref_cnt)) {
		/* Tx time stamp consumption already going on either from workq or func */
		return 0;
	}

	if (list_empty(&pdata->tx_ts_skb_head)) {
		atomic_set(&pdata->tx_ts_ref_cnt, -1);
		return 0;
	}

	list_for_each_safe(head_node, temp_head_node,
			   &pdata->tx_ts_skb_head) {
		pnode = list_entry(head_node,
				   struct ether_tx_ts_skb_list,
				   list_head);
		memset(&shhwtstamp, 0,
		       sizeof(struct skb_shared_hwtstamps));

		ioctl_data.cmd = OSI_CMD_GET_TX_TS;
		ioctl_data.tx_ts.pkt_id = pnode->pktid;
		ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
		if (ret == 0) {
			/* get time stamp form ethernet server */
			dev_dbg(pdata->dev, "%s() pktid = %x, skb = %p\n",
				__func__, pnode->pktid, pnode->skb);

			if ((ioctl_data.tx_ts.nsec & OSI_MAC_TCR_TXTSSMIS) ==
			    OSI_MAC_TCR_TXTSSMIS) {
				dev_warn(pdata->dev,
					 "No valid time for skb, removed\n");
				goto update_skb;
			}

			nsec = ioctl_data.tx_ts.sec * ETHER_ONESEC_NENOSEC +
			       ioctl_data.tx_ts.nsec;

			/* pass tstamp to stack */
			shhwtstamp.hwtstamp = ns_to_ktime(nsec);
			if (pnode->skb != NULL) {
				skb_tstamp_tx(pnode->skb, &shhwtstamp);
			}

update_skb:
			if (pnode->skb != NULL) {
				dev_consume_skb_any(pnode->skb);
			}

			raw_spin_lock_irqsave(&pdata->txts_lock, flags);
			list_del(head_node);
			pnode->in_use = OSI_DISABLE;
			raw_spin_unlock_irqrestore(&pdata->txts_lock, flags);

		} else {
			dev_dbg(pdata->dev, "Unable to retrieve TS from OSI\n");
			pending = true;
		}
	}

	if (pending)
		ret = -EAGAIN;

	atomic_set(&pdata->tx_ts_ref_cnt, -1);
	return ret;
}

/**
 * @brief Gets timestamp and update skb
 *
 * Algorithm:
 * - Parse through tx_ts_skb_head.
 * - Issue osi_handle_ioctl(OSI_CMD_GET_TX_TS) to read timestamp.
 * - Update skb with timestamp and give to network stack
 * - Free skb and node.
 *
 * @param[in] work: Work to handle SKB list update
 */
static void ether_get_tx_ts_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ether_priv_data *pdata = container_of(dwork,
					struct ether_priv_data, tx_ts_work);

	if (ether_get_tx_ts(pdata) < 0) {
		schedule_delayed_work(&pdata->tx_ts_work,
				      msecs_to_jiffies(ETHER_TS_MS_TIMER));
	}
}

#ifdef HSI_SUPPORT
static inline u64 rdtsc(void)
{
	u64 val;

	asm volatile("mrs %0, cntvct_el0" : "=r" (val));

	return val;
}

static irqreturn_t ether_common_isr_thread(int irq, void *data)
{
	struct ether_priv_data *pdata = (struct ether_priv_data *)data;
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	int ret = 0;
	int i;
	struct epl_error_report_frame error_report;

	error_report.reporter_id = osi_core->hsi.reporter_id;
	error_report.timestamp = lower_32_bits(rdtsc());

	mutex_lock(&pdata->hsi_lock);

	/* Called from ether_hsi_work */
	if (osi_core->hsi.report_err && irq == 0) {
		osi_core->hsi.report_err = OSI_DISABLE;
		for (i = 0; i < OSI_HSI_MAX_MAC_ERROR_CODE; i++) {
			if (osi_core->hsi.err_code[i] > 0) {
				error_report.error_code =
					osi_core->hsi.err_code[i];
				ret = epl_report_error(error_report);
				if (ret < 0) {
					dev_err(pdata->dev, "Failed to report error: reporter ID: 0x%x, Error code: 0x%x, return: %d\n",
						osi_core->hsi.reporter_id,
						osi_core->hsi.err_code[i], ret);
				} else {
					dev_info(pdata->dev, "EPL report error: reporter ID: 0x%x, Error code: 0x%x\n",
						 osi_core->hsi.reporter_id,
						 osi_core->hsi.err_code[i]);
				}
				osi_core->hsi.err_code[i] = 0;
			}
		}
	}

	/* Called from ether_hsi_work */
	if (osi_core->hsi.macsec_report_err && irq == 0) {
		osi_core->hsi.macsec_report_err = OSI_DISABLE;
		for (i = 0; i < HSI_MAX_MACSEC_ERROR_CODE; i++) {
			if (osi_core->hsi.macsec_err_code[i] > 0) {
				error_report.error_code =
					osi_core->hsi.macsec_err_code[i];
				ret = epl_report_error(error_report);
				if (ret < 0) {
					dev_err(pdata->dev, "Failed to report error: reporter ID: 0x%x, Error code: 0x%x, return: %d\n",
						osi_core->hsi.reporter_id,
						osi_core->hsi.err_code[i], ret);
				} else {
					dev_info(pdata->dev, "EPL report error: reporter ID: 0x%x, Error code: 0x%x\n",
						 osi_core->hsi.reporter_id,
						 osi_core->hsi.err_code[i]);
				}
				osi_core->hsi.macsec_err_code[i] = 0;
			}
		}
	}

	/* Called from interrupt handler */
	if (osi_core->hsi.report_err && irq != 0) {
		for (i = 0; i < OSI_HSI_MAX_MAC_ERROR_CODE; i++) {
			if (osi_core->hsi.err_code[i] > 0 &&
			    osi_core->hsi.report_count_err[i] == OSI_ENABLE) {
				error_report.error_code =
					osi_core->hsi.err_code[i];
				ret = epl_report_error(error_report);
				if (ret < 0) {
					dev_err(pdata->dev, "Failed to report error: reporter ID: 0x%x, Error code: 0x%x, return: %d\n",
						osi_core->hsi.reporter_id,
						osi_core->hsi.err_code[i], ret);
				} else {
					dev_info(pdata->dev, "EPL report error: reporter ID: 0x%x, Error code: 0x%x\n",
						 osi_core->hsi.reporter_id,
						 osi_core->hsi.err_code[i]);
				}
				osi_core->hsi.err_code[i] = 0;
				osi_core->hsi.report_count_err[i] = OSI_DISABLE;
			}
		}
	}
	mutex_unlock(&pdata->hsi_lock);
	return IRQ_HANDLED;
}
#endif

/**
 * @brief Work Queue function to call osi_read_mmc() periodically.
 *
 * Algorithm: call osi_read_mmc in periodic manner to avoid possibility of
 * overrun of 32 bit MMC hw registers.
 *
 * @param[in] work: work structure
 *
 * @note MAC and PHY need to be initialized.
 */
static inline void ether_stats_work_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ether_priv_data *pdata = container_of(dwork,
			struct ether_priv_data, ether_stats_work);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_ioctl ioctl_data = {};
	int ret;

	ioctl_data.cmd = OSI_CMD_READ_MMC;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to read MMC counters %s\n",
			__func__);
	}
	schedule_delayed_work(&pdata->ether_stats_work,
			      msecs_to_jiffies(pdata->stats_timer));
}

#ifdef HSI_SUPPORT
/**
 * @brief Work Queue function to report error periodically.
 *
 * Algorithm:
 * - periodically check if any HSI error need to be reported
 * - call ether_common_isr_thread to report error through EPL
 *
 * @param[in] work: work structure
 *
 */
static inline void ether_hsi_work_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ether_priv_data *pdata = container_of(dwork,
			struct ether_priv_data, ether_hsi_work);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	u64 rx_udp_err;
	u64 rx_tcp_err;
	u64 rx_ipv4_hderr;
	u64 rx_ipv6_hderr;
	u64 rx_crc_error;
	u64 rx_checksum_error;

	rx_crc_error =
		osi_core->mmc.mmc_rx_crc_error /
			osi_core->hsi.err_count_threshold;
	if (osi_core->hsi.rx_crc_err_count < rx_crc_error) {
		osi_core->hsi.rx_crc_err_count = rx_crc_error;
		mutex_lock(&pdata->hsi_lock);
		osi_core->hsi.err_code[RX_CRC_ERR_IDX] =
			OSI_INBOUND_BUS_CRC_ERR;
		osi_core->hsi.report_err = OSI_ENABLE;
		mutex_unlock(&pdata->hsi_lock);
	}

	rx_udp_err = osi_core->mmc.mmc_rx_udp_err;
	rx_tcp_err = osi_core->mmc.mmc_rx_tcp_err;
	rx_ipv4_hderr = osi_core->mmc.mmc_rx_ipv4_hderr;
	rx_ipv6_hderr = osi_core->mmc.mmc_rx_ipv6_hderr;
	rx_checksum_error = (rx_udp_err + rx_tcp_err +
		rx_ipv4_hderr + rx_ipv6_hderr) /
			osi_core->hsi.err_count_threshold;
	if (osi_core->hsi.rx_checksum_err_count < rx_checksum_error) {
		osi_core->hsi.rx_checksum_err_count = rx_checksum_error;
		mutex_lock(&pdata->hsi_lock);
		osi_core->hsi.err_code[RX_CSUM_ERR_IDX] =
				OSI_RECEIVE_CHECKSUM_ERR;
		osi_core->hsi.report_err = OSI_ENABLE;
		mutex_unlock(&pdata->hsi_lock);
	}

	if (osi_core->hsi.enabled == OSI_ENABLE &&
	    (osi_core->hsi.report_err == OSI_ENABLE ||
	     osi_core->hsi.macsec_report_err == OSI_ENABLE))
		ether_common_isr_thread(0, (void *)pdata);

	schedule_delayed_work(&pdata->ether_hsi_work,
			      msecs_to_jiffies(osi_core->hsi.err_time_threshold));
}
#endif

/**
 * @brief Start delayed workqueue.
 *
 * Algorithm: Start workqueue to read RMON HW counters periodically.
 * Workqueue will get schedule in every ETHER_STATS_TIMER sec.
 * Workqueue will be scheduled only if HW supports RMON HW counters.
 *
 * @param[in] pdata:OSD private data
 *
 * @note MAC and PHY need to be initialized.
 */
static inline void ether_stats_work_queue_start(struct ether_priv_data *pdata)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;

	if (pdata->hw_feat.mmc_sel == OSI_ENABLE &&
	    osi_core->use_virtualization == OSI_DISABLE) {
		schedule_delayed_work(&pdata->ether_stats_work,
				      msecs_to_jiffies(pdata->stats_timer));
	}
}

/**
 * @brief Stop delayed workqueue.
 *
 * Algorithm:
 *  Cancel workqueue.
 *
 * @param[in] pdata:OSD private data
 */
static inline void ether_stats_work_queue_stop(struct ether_priv_data *pdata)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;

	if (pdata->hw_feat.mmc_sel == OSI_ENABLE &&
	    osi_core->use_virtualization == OSI_DISABLE) {
		cancel_delayed_work_sync(&pdata->ether_stats_work);
	}
}

/**
 * @brief do PAD calibration
 *
 * Algorithm: Takes care of  doing the pad calibration
 *	      accordingly as per the MAC IP.
 *
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure or pad calibration already in progress.
 */
static int ether_pad_calibrate(struct ether_priv_data *pdata)
{
	int ret = -1;
	struct osi_ioctl ioctl_data = {};

	if (atomic_read(&pdata->padcal_in_progress) == 0) {
		atomic_set(&pdata->padcal_in_progress, OSI_ENABLE);
		ioctl_data.cmd = OSI_CMD_PAD_CALIBRATION;
		ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
		atomic_set(&pdata->padcal_in_progress, OSI_DISABLE);
	}
	return ret;
}

/**
 * @brief Disable all MAC MGBE related clks
 *
 * Algorithm: Release the reference counter for the clks by using
 * clock subsystem provided API's
 *
 * @param[in] pdata: OSD private data.
 */
static void ether_disable_mgbe_clks(struct ether_priv_data *pdata)
{
	if (!IS_ERR_OR_NULL(pdata->ptp_ref_clk)) {
		clk_disable_unprepare(pdata->ptp_ref_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->app_clk)) {
		clk_disable_unprepare(pdata->app_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->eee_pcs_clk)) {
		clk_disable_unprepare(pdata->eee_pcs_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->mac_clk)) {
		clk_disable_unprepare(pdata->mac_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->mac_div_clk)) {
		clk_disable_unprepare(pdata->mac_div_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->tx_pcs_clk)) {
		clk_disable_unprepare(pdata->tx_pcs_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->tx_clk)) {
		clk_disable_unprepare(pdata->tx_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_pcs_clk)) {
		clk_disable_unprepare(pdata->rx_pcs_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_pcs_input_clk)) {
		clk_disable_unprepare(pdata->rx_pcs_input_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_input_clk)) {
		clk_disable_unprepare(pdata->rx_input_clk);
	}

	pdata->clks_enable = false;
}

/**
 * @brief Disable all MAC EQOS related clks
 *
 * Algorithm: Release the reference counter for the clks by using
 * clock subsystem provided API's
 *
 * @param[in] pdata: OSD private data.
 */
static void ether_disable_eqos_clks(struct ether_priv_data *pdata)
{
	if (!IS_ERR_OR_NULL(pdata->axi_cbb_clk)) {
		clk_disable_unprepare(pdata->axi_cbb_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->axi_clk)) {
		clk_disable_unprepare(pdata->axi_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_clk)) {
		clk_disable_unprepare(pdata->rx_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->ptp_ref_clk)) {
		clk_disable_unprepare(pdata->ptp_ref_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->tx_clk)) {
		clk_disable_unprepare(pdata->tx_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->pllrefe_clk)) {
		clk_disable_unprepare(pdata->pllrefe_clk);
	}

	pdata->clks_enable = false;
}

/**
 * @brief Disable all MAC related clks
 *
 * Algorithm: Release the reference counter for the clks by using
 * clock subsystem provided API's
 *
 * @param[in] pdata: OSD private data.
 */
static void ether_disable_clks(struct ether_priv_data *pdata)
{
	if (pdata->osi_core->use_virtualization == OSI_DISABLE &&
	    !is_tegra_hypervisor_mode()) {
		if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
			ether_disable_mgbe_clks(pdata);
		} else {
			ether_disable_eqos_clks(pdata);
		}
	}
}

/**
 * @brief Enable all MAC MGBE related clks.
 *
 * Algorithm: Enables the clks by using clock subsystem provided API's.
 *
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_enable_mgbe_clks(struct ether_priv_data *pdata)
{
	unsigned int uphy_gbe_mode = pdata->osi_core->uphy_gbe_mode;
	unsigned long rate = 0;
	int ret;

	if (!IS_ERR_OR_NULL(pdata->rx_input_clk)) {
		ret = clk_prepare_enable(pdata->rx_input_clk);
		if (ret < 0) {
			return ret;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->rx_pcs_input_clk)) {
		ret = clk_prepare_enable(pdata->rx_pcs_input_clk);
		if (ret < 0) {
			return ret;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->rx_pcs_clk)) {
		ret = clk_prepare_enable(pdata->rx_pcs_clk);
		if (ret < 0) {
			goto err_rx_pcs;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->tx_clk)) {
		if (uphy_gbe_mode == OSI_ENABLE)
			rate = ETHER_MGBE_TX_CLK_USXGMII_10G;
		else
			rate = ETHER_MGBE_TX_CLK_USXGMII_5G;

		ret = clk_set_rate(pdata->tx_clk, rate);
		if (ret < 0) {
			dev_err(pdata->dev, "failed to set MGBE tx_clk rate\n");
			goto err_tx;
		}

		ret = clk_prepare_enable(pdata->tx_clk);
		if (ret < 0) {
			goto err_tx;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->tx_pcs_clk)) {
		if (uphy_gbe_mode == OSI_ENABLE)
			rate = ETHER_MGBE_TX_PCS_CLK_USXGMII_10G;
		else
			rate = ETHER_MGBE_TX_PCS_CLK_USXGMII_5G;

		ret = clk_set_rate(pdata->tx_pcs_clk, rate);
		if (ret < 0) {
			dev_err(pdata->dev,
				"failed to set MGBE tx_pcs_clk rate\n");
			goto err_tx_pcs;
		}

		ret = clk_prepare_enable(pdata->tx_pcs_clk);
		if (ret < 0) {
			goto err_tx_pcs;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->mac_div_clk)) {
		ret = clk_prepare_enable(pdata->mac_div_clk);
		if (ret < 0) {
			goto err_mac_div;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->mac_clk)) {
		ret = clk_prepare_enable(pdata->mac_clk);
		if (ret < 0) {
			goto err_mac;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->eee_pcs_clk)) {
		ret = clk_prepare_enable(pdata->eee_pcs_clk);
		if (ret < 0) {
			goto err_eee_pcs;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->app_clk)) {
		ret = clk_prepare_enable(pdata->app_clk);
		if (ret < 0) {
			goto err_app;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->ptp_ref_clk)) {
		ret = clk_prepare_enable(pdata->ptp_ref_clk);
		if (ret < 0) {
			goto err_ptp_ref;
		}
	}

	pdata->clks_enable = true;

	return 0;

err_ptp_ref:
	if (!IS_ERR_OR_NULL(pdata->app_clk)) {
		clk_disable_unprepare(pdata->app_clk);
	}
err_app:
	if (!IS_ERR_OR_NULL(pdata->eee_pcs_clk)) {
		clk_disable_unprepare(pdata->eee_pcs_clk);
	}
err_eee_pcs:
	if (!IS_ERR_OR_NULL(pdata->mac_clk)) {
		clk_disable_unprepare(pdata->mac_clk);
	}
err_mac:
	if (!IS_ERR_OR_NULL(pdata->mac_div_clk)) {
		clk_disable_unprepare(pdata->mac_div_clk);
	}
err_mac_div:
	if (!IS_ERR_OR_NULL(pdata->tx_pcs_clk)) {
		clk_disable_unprepare(pdata->tx_pcs_clk);
	}
err_tx_pcs:
	if (!IS_ERR_OR_NULL(pdata->tx_clk)) {
		clk_disable_unprepare(pdata->tx_clk);
	}
err_tx:
	if (!IS_ERR_OR_NULL(pdata->rx_pcs_clk)) {
		clk_disable_unprepare(pdata->rx_pcs_clk);
	}
err_rx_pcs:
	if (!IS_ERR_OR_NULL(pdata->rx_pcs_input_clk)) {
		clk_disable_unprepare(pdata->rx_pcs_input_clk);
	}

	return ret;
}

/**
 * @brief Enable all MAC EQOS related clks.
 *
 * Algorithm: Enables the clks by using clock subsystem provided API's.
 *
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_enable_eqos_clks(struct ether_priv_data *pdata)
{
	int ret;

	if (!IS_ERR_OR_NULL(pdata->pllrefe_clk)) {
		ret = clk_prepare_enable(pdata->pllrefe_clk);
		if (ret < 0) {
			return ret;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->axi_cbb_clk)) {
		ret = clk_prepare_enable(pdata->axi_cbb_clk);
		if (ret) {
			goto err_axi_cbb;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->axi_clk)) {
		ret = clk_prepare_enable(pdata->axi_clk);
		if (ret < 0) {
			goto err_axi;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->rx_clk)) {
		ret = clk_prepare_enable(pdata->rx_clk);
		if (ret < 0) {
			goto err_rx;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->ptp_ref_clk)) {
		ret = clk_prepare_enable(pdata->ptp_ref_clk);
		if (ret < 0) {
			goto err_ptp_ref;
		}
	}

	if (!IS_ERR_OR_NULL(pdata->tx_clk)) {
		ret = clk_prepare_enable(pdata->tx_clk);
		if (ret < 0) {
			goto err_tx;
		}
	}

	pdata->clks_enable = true;

	return 0;

err_tx:
	if (!IS_ERR_OR_NULL(pdata->ptp_ref_clk)) {
		clk_disable_unprepare(pdata->ptp_ref_clk);
	}
err_ptp_ref:
	if (!IS_ERR_OR_NULL(pdata->rx_clk)) {
		clk_disable_unprepare(pdata->rx_clk);
	}
err_rx:
	if (!IS_ERR_OR_NULL(pdata->axi_clk)) {
		clk_disable_unprepare(pdata->axi_clk);
	}
err_axi:
	if (!IS_ERR_OR_NULL(pdata->axi_cbb_clk)) {
		clk_disable_unprepare(pdata->axi_cbb_clk);
	}
err_axi_cbb:
	if (!IS_ERR_OR_NULL(pdata->pllrefe_clk)) {
		clk_disable_unprepare(pdata->pllrefe_clk);
	}

	return ret;
}

/**
 * @brief Enable all MAC related clks.
 *
 * Algorithm: Enables the clks by using clock subsystem provided API's.
 *
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_enable_clks(struct ether_priv_data *pdata)
{
	if (pdata->osi_core->use_virtualization == OSI_DISABLE) {
		if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
			return ether_enable_mgbe_clks(pdata);
		}

		return ether_enable_eqos_clks(pdata);
	}

	return 0;
}

/**
 * @brief ether_conf_eee - Init and configure EEE LPI in the MAC
 *
 * Algorithm:
 * 1) Check if EEE is requested to be enabled/disabled.
 * 2) If enabled, then check if current PHY speed/mode supports EEE.
 * 3) If PHY supports, then enable the Tx LPI timers in MAC and set EEE active.
 * 4) Else disable the Tx LPI timers in MAC and set EEE inactive.
 *
 * @param[in] pdata: Pointer to driver private data structure
 * @param[in] tx_lpi_enable: Flag to indicate Tx LPI enable (1) or disable (0)
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval OSI_ENABLE if EEE is enabled/active
 * @retval OSI_DISABLE if EEE is disabled/inactive
 */
int ether_conf_eee(struct ether_priv_data *pdata, unsigned int tx_lpi_enable)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	int ret = 0;
	struct phy_device *phydev = pdata->phydev;
	unsigned int enable = tx_lpi_enable;
	struct osi_ioctl ioctl_data = {};

	if (!phydev) {
		dev_err(pdata->dev, "%s() phydev is NULL\n", __func__);
		return -ENODEV;
	}

	if (tx_lpi_enable) {
		/* phy_init_eee() returns 0 if EEE is supported by the PHY */
		if (phy_init_eee(phydev,
				 (osi_core->mac_ver == OSI_EQOS_MAC_5_30) ?
				 false : true)) {
			/* PHY does not support EEE, disable it in MAC */
			enable = OSI_DISABLE;
		} else {
			/* PHY supports EEE, if link is up enable EEE */
			enable = (unsigned int)phydev->link;
		}
	}

	/* Enable EEE */
	ioctl_data.cmd = OSI_CMD_CONFIG_EEE;
	ioctl_data.arg1_u32 = enable;
	ioctl_data.arg2_u32 = pdata->tx_lpi_timer;
	ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);

	/* Return current status of EEE based on OSI API success/failure */
	if (ret != 0) {
		if (enable) {
			dev_warn(pdata->dev, "Failed to enable EEE\n");
			ret = OSI_DISABLE;
		} else {
			dev_warn(pdata->dev, "Failed to disable EEE\n");
			ret = OSI_ENABLE;
		}
	} else {
		/* EEE enabled/disabled successfully as per enable flag */
		ret = enable;
	}

	return ret;
}

/**
 * @brief Set MGBE MAC_DIV/TX clk rate
 *
 * Algorithm: Sets MGBE MAC_DIV clk_rate which will be MAC_TX/MACSEC clk rate.
 *
 * @param[in] mac_div_clk: Pointer to MAC_DIV clk.
 * @param[in] speed: PHY line speed.
 */
static inline void ether_set_mgbe_mac_div_rate(struct clk *mac_div_clk,
					       int speed)
{
	unsigned long rate;

	switch (speed) {
	case SPEED_2500:
		rate = ETHER_MGBE_MAC_DIV_RATE_2_5G;
		break;
	case SPEED_5000:
		rate = ETHER_MGBE_MAC_DIV_RATE_5G;
		break;
	case SPEED_10000:
	default:
		rate = ETHER_MGBE_MAC_DIV_RATE_10G;
		break;
	}

	if (clk_set_rate(mac_div_clk, rate) < 0)
		pr_err("%s(): failed to set mac_div_clk rate\n", __func__);
}

/**
 * @brief Set EQOS TX clk rate
 *
 * @param[in] tx_clk: Pointer to Tx clk.
 * @param[in] speed: PHY line speed.
 */
static inline void ether_set_eqos_tx_clk(struct clk *tx_clk,
					 int speed)
{
	unsigned long rate;

	switch (speed) {
	case SPEED_10:
		rate = ETHER_EQOS_TX_CLK_10M;
		break;
	case SPEED_100:
		rate = ETHER_EQOS_TX_CLK_100M;
		break;
	case SPEED_1000:
	default:
		rate = ETHER_EQOS_TX_CLK_1000M;
		break;
	}

	if (clk_set_rate(tx_clk, rate) < 0)
		pr_err("%s(): failed to set eqos tx_clk rate\n", __func__);
}

/**
 * @brief Work Queue function to call set speed.
 *
 * @param[in] work: work structure
 *
 */
static inline void set_speed_work_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ether_priv_data *pdata = container_of(dwork,
			struct ether_priv_data, set_speed_work);
	/* store last call last_uc_filter_index in temporary variable */
	struct osi_ioctl ioctl_data = {};
	struct net_device *dev = pdata->ndev;
	struct phy_device *phydev = pdata->phydev;
	nveu32_t iface_mode = pdata->osi_core->phy_iface_mode;
	unsigned int eee_enable = OSI_DISABLE;
	int speed;
	int ret = 0;

	if (pdata->osi_core->mac != OSI_MAC_HW_MGBE) {
		/* Handle retry for MGBE */
		return;
	}

	if (!phydev) {
		/* Return on no phydev */
		return;
	}

	if (atomic_read(&pdata->set_speed_ref_cnt) == 1) {
		/* set_speed already going on either from workq or interrupt */
		return;
	}

	atomic_set(&pdata->set_speed_ref_cnt, OSI_ENABLE);

	/* Speed will be overwritten as per the PHY interface mode */
	speed = phydev->speed;
	/* MAC and XFI speed should match in XFI mode */
	if (iface_mode == OSI_XFI_MODE_10G) {
		/* Set speed to 10G */
		speed = OSI_SPEED_10000;
	} else if (iface_mode == OSI_XFI_MODE_5G) {
		/* Set speed to 5G */
		speed = OSI_SPEED_5000;
	}

	/* Initiate OSI SET_SPEED ioctl */
	ioctl_data.cmd = OSI_CMD_SET_SPEED;
	ioctl_data.arg6_32 = speed;
	ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
	if (ret < 0) {
		netdev_dbg(dev, "Retry set speed\n");
		schedule_delayed_work(&pdata->set_speed_work,
				      msecs_to_jiffies(1000));
		atomic_set(&pdata->set_speed_ref_cnt, OSI_DISABLE);
		return;
	}

	/* Set MGBE MAC_DIV/TX clk rate */
	pdata->speed = speed;
	phy_print_status(phydev);
	ether_set_mgbe_mac_div_rate(pdata->mac_div_clk,
				    pdata->speed);

	if (pdata->eee_enabled && pdata->tx_lpi_enabled) {
		/* Configure EEE if it is enabled */
		eee_enable = OSI_ENABLE;
	}
	pdata->eee_active = ether_conf_eee(pdata, eee_enable);
	netif_carrier_on(dev);

	atomic_set(&pdata->set_speed_ref_cnt, OSI_DISABLE);
}

static void ether_en_dis_monitor_clks(struct ether_priv_data *pdata,
				      unsigned int en_dis)
{
	if (en_dis == OSI_ENABLE) {
		/* Enable Monitoring clocks */
		if (!IS_ERR_OR_NULL(pdata->rx_m_clk) && !pdata->rx_m_enabled) {
			if (clk_prepare_enable(pdata->rx_m_clk) < 0)
				dev_err(pdata->dev,
					"failed to enable rx_m_clk");
			else
				pdata->rx_m_enabled = true;
		}

		if (!IS_ERR_OR_NULL(pdata->rx_pcs_m_clk) && !pdata->rx_pcs_m_enabled) {
			if (clk_prepare_enable(pdata->rx_pcs_m_clk) < 0)
				dev_err(pdata->dev,
					"failed to enable rx_pcs_m_clk");
			else
				pdata->rx_pcs_m_enabled = true;
		}
	} else {
		/* Disable Monitoring clocks */
		if (!IS_ERR_OR_NULL(pdata->rx_pcs_m_clk) && pdata->rx_pcs_m_enabled) {
			clk_disable_unprepare(pdata->rx_pcs_m_clk);
			pdata->rx_pcs_m_enabled = false;
		}

		if (!IS_ERR_OR_NULL(pdata->rx_m_clk) && pdata->rx_m_enabled) {
			clk_disable_unprepare(pdata->rx_m_clk);
			pdata->rx_m_enabled = false;
		}
	}
}

/**
 * @brief Adjust link call back
 *
 * Algorithm: Callback function called by the PHY subsystem
 * whenever there is a link detected or link changed on the
 * physical layer.
 *
 * @param[in] dev: Network device data.
 *
 * @note MAC and PHY need to be initialized.
 */
static void ether_adjust_link(struct net_device *dev)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	nveu32_t iface_mode = pdata->osi_core->phy_iface_mode;
	struct phy_device *phydev = pdata->phydev;
	int new_state = 0, speed_changed = 0, speed;
	unsigned long val;
	unsigned int eee_enable = OSI_DISABLE;
	struct osi_ioctl ioctl_data = {};
	int ret = 0;

	if (phydev == NULL) {
		return;
	}

	cancel_delayed_work_sync(&pdata->set_speed_work);
	if (phydev->link) {
		if ((pdata->osi_core->pause_frames == OSI_PAUSE_FRAMES_ENABLE)
		    && (phydev->pause || phydev->asym_pause)) {
			ioctl_data.cmd = OSI_CMD_FLOW_CTRL;
			ioctl_data.arg1_u32 = pdata->osi_core->flow_ctrl;
			ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
			if (ret < 0) {
				netdev_err(dev, "Failed to set pause frame\n");
				return;
			}
		}

		if (pdata->fixed_link == OSI_ENABLE) {
			if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
				if (iface_mode == OSI_XFI_MODE_10G) {
					phydev->speed = OSI_SPEED_10000;
				} else if (iface_mode == OSI_XFI_MODE_5G) {
					phydev->speed = OSI_SPEED_5000;
				}
				phydev->duplex = OSI_FULL_DUPLEX;
			}
		}
		if (phydev->duplex != pdata->oldduplex) {
			new_state = 1;
			ioctl_data.cmd = OSI_CMD_SET_MODE;
			ioctl_data.arg6_32 = phydev->duplex;
			ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
			if (ret < 0) {
				netdev_err(dev, "Failed to set mode\n");
				return;
			}
			pdata->oldduplex = phydev->duplex;
		}

		if (phydev->speed != pdata->speed) {
			new_state = 1;
			speed_changed = 1;
			ioctl_data.cmd = OSI_CMD_SET_SPEED;
			/* FOR EQOS speed is PHY Speed and For MGBE this
			 * speed will be overwritten as per the
			 * PHY interface mode */
			speed = phydev->speed;
			/* XFI mode = 10G:
			 *	UPHY GBE mode = 10G
			 *	MAC = 10G
			 *	XPCS = 10G
			 *	PHY line side = 10G/5G/2.5G/1G/100M
			 * XFI mode = 5G:
			 *	UPHY GBE mode = 5G
			 *	MAC = 5G
			 *	XPCS = 5G
			 *	PHY line side = 10G/5G/2.5G/1G/100M
			 * USXGMII mode = 10G:
			 *	UPHY GBE mode = 10G
			 *	MAC = 10G/5G/2.5G (same as PHY line speed)
			 *	XPCS = 10G
			 *	PHY line side = 10G/5G/2.5G
			 * USXGMII mode = 5G:
			 *	UPHY GBE mode = 5G
			 *	MAC = 5G/2.5G ( same as PHY line speed)
			 *	XPCS = 5G
			 *	PHY line side = 5G/2.5G
			*/
			if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
				/* MAC and XFI speed should match in XFI mode */
				if (iface_mode == OSI_XFI_MODE_10G) {
					speed = OSI_SPEED_10000;
				} else if (iface_mode == OSI_XFI_MODE_5G) {
					speed = OSI_SPEED_5000;
				}
			}
			ioctl_data.arg6_32 = speed;
			ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
			if (ret < 0) {
				if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
					netdev_dbg(dev, "Retry set speed\n");
					netif_carrier_off(dev);
					schedule_delayed_work(&pdata->set_speed_work,
							      msecs_to_jiffies(10));
					return;
				}
				netdev_err(dev, "Failed to set speed\n");
				return;
			}

			ether_en_dis_monitor_clks(pdata, OSI_ENABLE);
			pdata->speed = speed;
		}

		if (!pdata->oldlink) {
			new_state = 1;
			pdata->oldlink = 1;
			val = pdata->xstats.link_connect_count;
			pdata->xstats.link_connect_count =
				osi_update_stats_counter(val, 1UL);
		}
	} else if (pdata->oldlink) {
		new_state = 1;
		pdata->oldlink = 0;
		pdata->speed = 0;
		pdata->oldduplex = -1;
		val = pdata->xstats.link_disconnect_count;
		pdata->xstats.link_disconnect_count =
			osi_update_stats_counter(val, 1UL);
		ether_en_dis_monitor_clks(pdata, OSI_DISABLE);
	} else {
		/* Nothing here */
	}

	if (new_state) {
		phy_print_status(phydev);
	}

	if (speed_changed) {
		if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
			ether_set_mgbe_mac_div_rate(pdata->mac_div_clk,
						    pdata->speed);
		} else {
			if (pdata->osi_core->mac_ver == OSI_EQOS_MAC_5_30) {
				ether_set_eqos_tx_clk(pdata->tx_div_clk,
					      phydev->speed);
			} else {
				ether_set_eqos_tx_clk(pdata->tx_clk,
					      phydev->speed);
			}
			if (phydev->speed != SPEED_10) {
				if (ether_pad_calibrate(pdata) < 0) {
					dev_err(pdata->dev,
						"failed to do pad caliberation\n");
				}
			}
		}
	}

	/* Configure EEE if it is enabled and link is up*/
	if (pdata->eee_enabled && pdata->tx_lpi_enabled && phydev->link) {
		eee_enable = OSI_ENABLE;
	}

	pdata->eee_active = ether_conf_eee(pdata, eee_enable);
}

/**
 * @brief Initialize the PHY
 *
 * Algorithm: 1) Resets the PHY. 2) Connect to the phy described in the device tree.
 *
 * @param[in] dev: Network device data.
 *
 * @note MAC needs to be out of reset.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_phy_init(struct net_device *dev)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct phy_device *phydev = NULL;

	pdata->oldlink = 0;
	pdata->speed = SPEED_UNKNOWN;
	pdata->oldduplex = SPEED_UNKNOWN;

	if (pdata->phy_node != NULL) {
		phydev = of_phy_connect(dev, pdata->phy_node,
					&ether_adjust_link, 0,
					pdata->interface);
	}

	if (phydev == NULL) {
		dev_err(pdata->dev, "failed to connect PHY\n");
		return -ENODEV;
	}

	if ((pdata->phy_node == NULL) && phydev->phy_id == 0U) {
		phy_disconnect(phydev);
		return -ENODEV;
	}

	pdata->phydev = phydev;

	return 0;
}

/**
 * @brief ether_vm_isr - VM based ISR routine.
 *
 * Algorithm:
 * 1) Get global DMA status (common for all VM IRQ's)
 * + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 * + RX7 + TX7 + RX6 + TX6 + . . . . . . . + RX1 + TX1 + RX0 + TX0 +
 * + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * 2) Mask the channels which are specific to VM in global DMA status.
 * 3) Process all DMA channel interrupts which are triggered the IRQ
 *	a) Find first first set from LSB with ffs
 *	b) The least significant bit is position 1 for ffs. So decremented
 *	by one to start from zero.
 *	c) Get channel number and TX/RX info by using bit position.
 *	d) Invoke OSI layer to clear interrupt source for DMA Tx/Rx at
 *	DMA and wrapper level.
 *	e) Get NAPI instance based on channel number and schedule the same.
 *
 * @param[in] irq: IRQ number.
 * @param[in] data: VM IRQ private data structure.
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval IRQ_HANDLED on success.
 * @retval IRQ_NONE on failure.
 */
static irqreturn_t ether_vm_isr(int irq, void *data)
{
	struct ether_vm_irq_data *vm_irq = (struct ether_vm_irq_data *)data;
	struct ether_priv_data *pdata = vm_irq->pdata;
	unsigned int temp = 0, chan = 0, txrx = 0;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct ether_rx_napi *rx_napi = NULL;
	struct ether_tx_napi *tx_napi = NULL;
	unsigned int dma_status;

	/* TODO: locking required since this is shared register b/w VM IRQ's */
	dma_status = osi_get_global_dma_status(osi_dma);
	dma_status &= vm_irq->chan_mask;

	while (dma_status) {
		temp = ffs(dma_status);
		temp--;

		/* divide by two get channel number */
		chan = temp >> 1U;
		/* bitwise and with one to get whether Tx or Rx */
		txrx = temp & 1U;

		if (txrx) {
			rx_napi = pdata->rx_napi[chan];

			osi_handle_dma_intr(osi_dma, chan,
					    OSI_DMA_CH_RX_INTR,
					    OSI_DMA_INTR_DISABLE);

			if (likely(napi_schedule_prep(&rx_napi->napi))) {
				/* TODO: Schedule NAPI on different CPU core */
				__napi_schedule_irqoff(&rx_napi->napi);
			}
		} else {
			tx_napi = pdata->tx_napi[chan];

			osi_handle_dma_intr(osi_dma, chan,
					    OSI_DMA_CH_TX_INTR,
					    OSI_DMA_INTR_DISABLE);

			if (likely(napi_schedule_prep(&tx_napi->napi))) {
				/* TODO: Schedule NAPI on different CPU core */
				__napi_schedule_irqoff(&tx_napi->napi);
			}
		}

		dma_status &= ~BIT(temp);
	}

	return IRQ_HANDLED;
}

/**
 * @brief Transmit done ISR Routine.
 *
 * Algorithm:
 * 1) Get channel number private data passed to ISR.
 * 2) Disable DMA Tx channel interrupt.
 * 3) Schedule TX NAPI poll handler to cleanup the buffer.
 *
 * @param[in] irq: IRQ number.
 * @param[in] data: Tx NAPI private data structure.
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval IRQ_HANDLED on success
 * @retval IRQ_NONE on failure.
 */
static irqreturn_t ether_tx_chan_isr(int irq, void *data)
{
	struct ether_tx_napi *tx_napi = (struct ether_tx_napi *)data;
	struct ether_priv_data *pdata = tx_napi->pdata;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	unsigned int chan = tx_napi->chan;
	unsigned long flags;
	unsigned long val;

	raw_spin_lock_irqsave(&pdata->rlock, flags);
	osi_handle_dma_intr(osi_dma, chan,
			    OSI_DMA_CH_TX_INTR,
			    OSI_DMA_INTR_DISABLE);
	raw_spin_unlock_irqrestore(&pdata->rlock, flags);

	val = pdata->xstats.tx_normal_irq_n[chan];
	pdata->xstats.tx_normal_irq_n[chan] =
		osi_update_stats_counter(val, 1U);

	if (likely(napi_schedule_prep(&tx_napi->napi))) {
		__napi_schedule_irqoff(&tx_napi->napi);
	/* NAPI may get scheduled when tx_usecs is enabled */
	} else if (osi_dma->use_tx_usecs == OSI_DISABLE) {
		pr_err("Tx DMA-%d IRQ when NAPI already scheduled!\n", chan);
		WARN_ON(true);
	}

	return IRQ_HANDLED;
}

/**
 * @brief Receive done ISR Routine
 *
 * Algorithm:
 * 1) Get Rx channel number from Rx NAPI private data which will be passed
 * during request_irq() API.
 * 2) Disable DMA Rx channel interrupt.
 * 3) Schedule Rx NAPI poll handler to get data from HW and pass to the
 * Linux network stack.
 *
 * @param[in] irq: IRQ number
 * @param[in] data: Rx NAPI private data structure.
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval IRQ_HANDLED on success
 * @retval IRQ_NONE on failure.
 */
static irqreturn_t ether_rx_chan_isr(int irq, void *data)
{
	struct ether_rx_napi *rx_napi = (struct ether_rx_napi *)data;
	struct ether_priv_data *pdata = rx_napi->pdata;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	unsigned int chan = rx_napi->chan;
	unsigned long val, flags;

	raw_spin_lock_irqsave(&pdata->rlock, flags);
	osi_handle_dma_intr(osi_dma, chan,
			    OSI_DMA_CH_RX_INTR,
			    OSI_DMA_INTR_DISABLE);
	raw_spin_unlock_irqrestore(&pdata->rlock, flags);

	val = pdata->xstats.rx_normal_irq_n[chan];
	pdata->xstats.rx_normal_irq_n[chan] =
		osi_update_stats_counter(val, 1U);

	if (likely(napi_schedule_prep(&rx_napi->napi))) {
		__napi_schedule_irqoff(&rx_napi->napi);
	} else {
		pr_err("Rx DMA-%d IRQ when NAPI already scheduled!\n", chan);
		WARN_ON(true);
	}

	return IRQ_HANDLED;
}

/**
 * @brief Common ISR Routine
 *
 * Algorithm: Invoke OSI layer to handle common interrupt.
 *
 * @param[in] irq: IRQ number.
 * @param[in] data: Private data from ISR.
 *
 * @note MAC and PHY need to be initialized.
 *
 * @retval IRQ_HANDLED on success
 * @retval IRQ_NONE on failure.
 */
static irqreturn_t ether_common_isr(int irq, void *data)
{
	struct ether_priv_data *pdata =	(struct ether_priv_data *)data;
	struct osi_ioctl ioctl_data = {};
	int ret;
	int irq_ret = IRQ_HANDLED;

	ioctl_data.cmd = OSI_CMD_COMMON_ISR;
	ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
	if (ret < 0) {
		dev_err(pdata->dev,
			"%s() failure in handling ISR\n", __func__);
	}
#ifdef HSI_SUPPORT
	if (pdata->osi_core->hsi.enabled == OSI_ENABLE &&
	    pdata->osi_core->hsi.report_err == OSI_ENABLE)
		irq_ret = IRQ_WAKE_THREAD;
#endif
	return irq_ret;
}

/**
 * @brief Free IRQs
 *
 * Algorithm: This routine takes care of freeing below IRQs
 * 1) Common IRQ
 * 2) TX IRQ
 * 3) RX IRQ
 *
 * @param[in] pdata: OS dependent private data structure.
 *
 * @note IRQs should have registered.
 */
static void ether_free_irqs(struct ether_priv_data *pdata)
{
	unsigned int i;
	unsigned int chan;

	if (pdata->common_irq_alloc_mask & 1U) {
		if ((pdata->osi_core->mac == OSI_MAC_HW_MGBE) &&
		    (pdata->osi_core->use_virtualization == OSI_DISABLE)) {
			irq_set_affinity_hint(pdata->common_irq, NULL);
		}
		devm_free_irq(pdata->dev, pdata->common_irq, pdata);
		pdata->common_irq_alloc_mask = 0U;
	}

	if (pdata->osi_core->mac_ver > OSI_EQOS_MAC_5_00 ||
	    pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
		for (i = 0; i < pdata->osi_core->num_vm_irqs; i++) {
			if (pdata->rx_irq_alloc_mask & (OSI_ENABLE << i)) {
				devm_free_irq(pdata->dev, pdata->vm_irqs[i],
					      &pdata->vm_irq_data[i]);
			}
		}
	} else {
		for (i = 0; i < pdata->osi_dma->num_dma_chans; i++) {
			chan = pdata->osi_dma->dma_chans[i];

			if (pdata->rx_irq_alloc_mask & (OSI_ENABLE << i)) {
				devm_free_irq(pdata->dev, pdata->rx_irqs[i],
					      pdata->rx_napi[chan]);
				pdata->rx_irq_alloc_mask &=
							(~(OSI_ENABLE << i));
			}
			if (pdata->tx_irq_alloc_mask & (OSI_ENABLE << i)) {
				devm_free_irq(pdata->dev, pdata->tx_irqs[i],
					      pdata->tx_napi[chan]);
				pdata->tx_irq_alloc_mask &=
							(~(OSI_ENABLE << i));
			}
		}
	}
}

/**
 * @brief Start IVC, initializes IVC.
 *
 * @param[in]: Priv data.
 *
 * @retval void
 */

static void ether_start_ivc(struct ether_priv_data *pdata)
{
	struct ether_ivc_ctxt *ictxt = &pdata->ictxt;
	if (ictxt->ivck != NULL && !ictxt->ivc_state) {
		tegra_hv_ivc_channel_reset(ictxt->ivck);
		ictxt->ivc_state = 1;
		raw_spin_lock_init(&ictxt->ivck_lock);
	}
}

/**
 * @brief Stop IVC, de initializes IVC
 *
 * @param[in]: Priv data.
 *
 * @retval void
 */

static void ether_stop_ivc(struct ether_priv_data *pdata)
{
	struct ether_ivc_ctxt *ictxt = &pdata->ictxt;
	if (ictxt->ivck != NULL) {
		tegra_hv_ivc_unreserve(ictxt->ivck);
		ictxt->ivc_state = 0;
	}
}

/**
 * @Register IVC
 *
 * Algorithm: routine initializes IVC for common IRQ
 *
 * @param[in] pdata: OS dependent private data structure.
 *
 * @note IVC number need to be known.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_init_ivc(struct ether_priv_data *pdata)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct ether_ivc_ctxt *ictxt = &pdata->ictxt;
	struct device *dev = pdata->dev;
	struct device_node *np, *hv_np;
	uint32_t id;
	int ret;

	np = dev->of_node;
	if (!np) {
		ictxt->ivck = NULL;
		return -EINVAL;
	}

	hv_np = of_parse_phandle(np, "ivc", 0);
	if (!hv_np) {
		return -EINVAL;
	}

	ret = of_property_read_u32_index(np, "ivc", 1, &id);
	if (ret) {
		dev_err(dev, "ivc_init: Error in reading IVC DT\n");
		of_node_put(hv_np);
		return -EINVAL;
	}

	ictxt->ivck = tegra_hv_ivc_reserve(hv_np, id, NULL);
	of_node_put(hv_np);
	if (IS_ERR_OR_NULL(ictxt->ivck)) {
		dev_err(dev, "Failed to reserve ivc channel:%u\n", id);
		ret = PTR_ERR(ictxt->ivck);
		ictxt->ivck = NULL;
		return ret;
	}

	dev_info(dev, "Reserved IVC channel #%u - frame_size=%d irq %d\n",
		 id, ictxt->ivck->frame_size, ictxt->ivck->irq);
	osi_core->osd_ops.ivc_send = osd_ivc_send_cmd;
	ether_start_ivc(pdata);
	return 0;
}

/**
 * @brief Register IRQs
 *
 * Algorithm: This routine takes care of requesting below IRQs
 * 1) Common IRQ
 * 2) TX IRQ
 * 3) RX IRQ
 *
 * @param[in] pdata: OS dependent private data structure.
 *
 * @note IRQ numbers need to be known.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_request_irqs(struct ether_priv_data *pdata)
{
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	int ret = 0, i, j = 1;
	unsigned int chan;

	snprintf(pdata->irq_names[0], ETHER_IRQ_NAME_SZ, "%s.common_irq",
		 netdev_name(pdata->ndev));

#ifdef HSI_SUPPORT
	ret = devm_request_threaded_irq(pdata->dev,
					(unsigned int)pdata->common_irq,
					ether_common_isr,
					ether_common_isr_thread,
					IRQF_SHARED | IRQF_ONESHOT,
					pdata->irq_names[0], pdata);
#else
	ret = devm_request_irq(pdata->dev, (unsigned int)pdata->common_irq,
			       ether_common_isr, IRQF_SHARED,
			       pdata->irq_names[0], pdata);
#endif
	if (unlikely(ret < 0)) {
		dev_err(pdata->dev, "failed to register common interrupt: %d\n",
			pdata->common_irq);
		return ret;
	}

	pdata->common_irq_alloc_mask = 1;

	if ((osi_core->mac == OSI_MAC_HW_MGBE) &&
	    (cpu_online(pdata->common_isr_cpu_id)) &&
	    (osi_core->use_virtualization == OSI_DISABLE)) {
		cpumask_set_cpu(pdata->common_isr_cpu_id,
				&pdata->common_isr_cpu_mask);
		irq_set_affinity_hint(pdata->common_irq,
				      &pdata->common_isr_cpu_mask);
	}

	if (osi_core->mac_ver > OSI_EQOS_MAC_5_00 ||
	    osi_core->mac == OSI_MAC_HW_MGBE) {
		for (i = 0; i < osi_core->num_vm_irqs; i++) {
			snprintf(pdata->irq_names[j], ETHER_IRQ_NAME_SZ, "%s.vm%d",
				 netdev_name(pdata->ndev), i);
			ret = devm_request_irq(pdata->dev, pdata->vm_irqs[i],
					       ether_vm_isr, IRQF_TRIGGER_NONE,
					       pdata->irq_names[j++],
					       &pdata->vm_irq_data[i]);
			if (unlikely(ret < 0)) {
				dev_err(pdata->dev,
					"failed to request VM IRQ (%d)\n",
					pdata->vm_irqs[i]);
				goto err_chan_irq;
			}

			pdata->rx_irq_alloc_mask |= (OSI_ENABLE << i);
		}
	} else {
		for (i = 0; i < osi_dma->num_dma_chans; i++) {
			chan = osi_dma->dma_chans[i];

			snprintf(pdata->irq_names[j], ETHER_IRQ_NAME_SZ, "%s.rx%d",
				 netdev_name(pdata->ndev), chan);
			ret = devm_request_irq(pdata->dev, pdata->rx_irqs[i],
					       ether_rx_chan_isr,
					       IRQF_TRIGGER_NONE,
					       pdata->irq_names[j++],
					       pdata->rx_napi[chan]);
			if (unlikely(ret < 0)) {
				dev_err(pdata->dev,
					"failed to register Rx chan interrupt: %d\n",
					pdata->rx_irqs[i]);
				goto err_chan_irq;
			}

			pdata->rx_irq_alloc_mask |= (OSI_ENABLE << i);

			snprintf(pdata->irq_names[j], ETHER_IRQ_NAME_SZ, "%s.tx%d",
				 netdev_name(pdata->ndev), chan);
			ret = devm_request_irq(pdata->dev,
					       (unsigned int)pdata->tx_irqs[i],
					       ether_tx_chan_isr,
					       IRQF_TRIGGER_NONE,
					       pdata->irq_names[j++],
					       pdata->tx_napi[chan]);
			if (unlikely(ret < 0)) {
				dev_err(pdata->dev,
					"failed to register Tx chan interrupt: %d\n",
					pdata->tx_irqs[i]);
				goto err_chan_irq;
			}

			pdata->tx_irq_alloc_mask |= (OSI_ENABLE << i);
		}
	}

	return ret;

err_chan_irq:
	ether_free_irqs(pdata);
	return ret;
}

/**
 * @brief Disable NAPI.
 *
 * Algorithm:
 *	1) Wait for scheduled Tx and Rx NAPI to be completed.
 *	2) Disable Tx and Rx NAPI for the channels which
 *	are enabled.
 *
 * @param[in] pdata: OSD private data structure.
 *
 * @note NAPI resources need to be allocated as part of probe().
 */
static void ether_napi_disable(struct ether_priv_data *pdata)
{
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	unsigned int chan;
	unsigned int i;

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		chan = osi_dma->dma_chans[i];
		napi_synchronize(&pdata->tx_napi[chan]->napi);
		napi_disable(&pdata->tx_napi[chan]->napi);
		napi_synchronize(&pdata->rx_napi[chan]->napi);
		napi_disable(&pdata->rx_napi[chan]->napi);
	}
}

/**
 * @brief Enable NAPI.
 *
 * Algorithm: Enable Tx and Rx NAPI for the channels which are enabled.
 *
 * @param[in] pdata: OSD private data structure.
 *
 * @note NAPI resources need to be allocated as part of probe().
 */
static void ether_napi_enable(struct ether_priv_data *pdata)
{
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	unsigned int chan;
	unsigned int i;

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		chan = osi_dma->dma_chans[i];

		napi_enable(&pdata->tx_napi[chan]->napi);
		napi_enable(&pdata->rx_napi[chan]->napi);
	}
}

/**
 * @brief Free receive skbs
 *
 * @param[in] rx_swcx: Rx pkt SW context
 * @param[in] pdata: Ethernet private data
 * @param[in] rx_buf_len: Receive buffer length
 * @param[in] resv_buf_virt_addr: Reservered virtual buffer
 */
static void ether_free_rx_skbs(struct osi_rx_swcx *rx_swcx,
			       struct ether_priv_data *pdata,
			       unsigned int rx_buf_len,
			       void *resv_buf_virt_addr,
			       unsigned int chan)
{
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct osi_rx_swcx *prx_swcx = NULL;
	unsigned int i;

	for (i = 0; i < osi_dma->rx_ring_sz; i++) {
		prx_swcx = rx_swcx + i;

		if (prx_swcx->buf_virt_addr != NULL) {
			if (resv_buf_virt_addr != prx_swcx->buf_virt_addr) {
#ifdef ETHER_PAGE_POOL
				if (chan != OSI_INVALID_CHAN_NUM)
					page_pool_put_full_page(pdata->page_pool[chan],
								prx_swcx->buf_virt_addr,
								false);
#else
				dma_unmap_single(pdata->dev,
						 prx_swcx->buf_phy_addr,
						 rx_buf_len, DMA_FROM_DEVICE);
				dev_kfree_skb_any(prx_swcx->buf_virt_addr);
#endif
			}
			prx_swcx->buf_virt_addr = NULL;
			prx_swcx->buf_phy_addr = 0;
		}
	}
}

/**
 * @brief free_rx_dma_resources - Frees allocated Rx DMA resources.
 *	Algorithm: Release all DMA Rx resources which are allocated as part of
 *	allocated_rx_dma_ring() API.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] pdata: Ethernet private data.
 */
static void free_rx_dma_resources(struct osi_dma_priv_data *osi_dma,
				  struct ether_priv_data *pdata)
{
	unsigned long rx_desc_size = sizeof(struct osi_rx_desc) * osi_dma->rx_ring_sz;
	struct osi_rx_ring *rx_ring = NULL;
	unsigned int i, chan;

	for (i = 0; i < OSI_MGBE_MAX_NUM_CHANS; i++) {
		rx_ring = osi_dma->rx_ring[i];
		chan = osi_dma->dma_chans[i];

		if (rx_ring != NULL) {
			if (rx_ring->rx_swcx != NULL) {
				ether_free_rx_skbs(rx_ring->rx_swcx, pdata,
						   osi_dma->rx_buf_len,
						   osi_dma->resv_buf_virt_addr,
						   chan);
				kfree(rx_ring->rx_swcx);
			}

			if (rx_ring->rx_desc != NULL) {
				dma_free_coherent(pdata->dev, rx_desc_size,
						  rx_ring->rx_desc,
						  rx_ring->rx_desc_phy_addr);
			}
			kfree(rx_ring);
			osi_dma->rx_ring[i] = NULL;
			rx_ring = NULL;
		}
#ifdef ETHER_PAGE_POOL
		if (chan != OSI_INVALID_CHAN_NUM && pdata->page_pool[chan]) {
			page_pool_destroy(pdata->page_pool[chan]);
			pdata->page_pool[chan] = NULL;
		}
#endif
	}
}

/**
 * @brief Allocate Rx DMA channel ring resources
 *
 * Algorithm: DMA receive ring will be created for valid channel number.
 * Receive ring updates with descriptors and software context associated
 * with each receive descriptor.
 *
 * @param[in] osi_dma: OSI DMA private data structure.
 * @param[in] dev: device instance associated with driver.
 * @param[in] chan: Rx DMA channel number.
 *
 * @note Invalid channel need to be updated.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int allocate_rx_dma_resource(struct osi_dma_priv_data *osi_dma,
				    struct device *dev,
				    unsigned int chan)
{
	unsigned long rx_desc_size;
	unsigned long rx_swcx_size;
	int ret = 0;

	rx_desc_size = sizeof(struct osi_rx_desc) * (unsigned long)osi_dma->rx_ring_sz;
	rx_swcx_size = sizeof(struct osi_rx_swcx) * (unsigned long)osi_dma->rx_ring_sz;

	osi_dma->rx_ring[chan] = kzalloc(sizeof(struct osi_rx_ring),
					 GFP_KERNEL);
	if (osi_dma->rx_ring[chan] == NULL) {
		dev_err(dev, "failed to allocate Rx ring\n");
		return -ENOMEM;
	}
	osi_dma->rx_ring[chan]->rx_desc = dma_alloc_coherent(dev, rx_desc_size,
			(dma_addr_t *)&osi_dma->rx_ring[chan]->rx_desc_phy_addr,
			GFP_KERNEL | __GFP_ZERO);

	if (osi_dma->rx_ring[chan]->rx_desc == NULL) {
		dev_err(dev, "failed to allocate receive descriptor\n");
		ret = -ENOMEM;
		goto err_rx_desc;
	}

	osi_dma->rx_ring[chan]->rx_swcx = kzalloc(rx_swcx_size, GFP_KERNEL);
	if (osi_dma->rx_ring[chan]->rx_swcx == NULL) {
		dev_err(dev, "failed to allocate Rx ring software context\n");
		ret = -ENOMEM;
		goto err_rx_swcx;
	}

	return 0;

err_rx_swcx:
	dma_free_coherent(dev, rx_desc_size,
			  osi_dma->rx_ring[chan]->rx_desc,
			  osi_dma->rx_ring[chan]->rx_desc_phy_addr);
	osi_dma->rx_ring[chan]->rx_desc = NULL;
err_rx_desc:
	kfree(osi_dma->rx_ring[chan]);
	osi_dma->rx_ring[chan] = NULL;

	return ret;
}

/**
 * @brief Allocate receive buffers for a DMA channel ring
 *
 * @param[in] pdata: OSD private data.
 * @param[in] rx_ring: rxring data structure.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_allocate_rx_buffers(struct ether_priv_data *pdata,
				     struct osi_rx_ring *rx_ring,
				     unsigned int chan)
{
#ifndef ETHER_PAGE_POOL
	unsigned int rx_buf_len = pdata->osi_dma->rx_buf_len;
#endif
	struct osi_rx_swcx *rx_swcx = NULL;
	unsigned int i = 0;

	for (i = 0; i < pdata->osi_dma->rx_ring_sz; i++) {
#ifndef ETHER_PAGE_POOL
		struct sk_buff *skb = NULL;
#else
		struct page *page = NULL;
#endif
		dma_addr_t dma_addr = 0;

		rx_swcx = rx_ring->rx_swcx + i;

#ifdef ETHER_PAGE_POOL
		if (chan != OSI_INVALID_CHAN_NUM) {
			page = page_pool_dev_alloc_pages(pdata->page_pool[chan]);
			if (!page) {
				dev_err(pdata->dev,
					"failed to allocate page pool buffer");
				return -ENOMEM;
			}

			dma_addr = page_pool_get_dma_addr(page);
			rx_swcx->buf_virt_addr = page;
		}
#else
		skb = __netdev_alloc_skb_ip_align(pdata->ndev, rx_buf_len,
						  GFP_KERNEL);
		if (unlikely(skb == NULL)) {
			dev_err(pdata->dev, "RX skb allocation failed\n");
			return -ENOMEM;
		}

		dma_addr = dma_map_single(pdata->dev, skb->data, rx_buf_len,
					  DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(pdata->dev, dma_addr) != 0)) {
			dev_err(pdata->dev, "RX skb dma map failed\n");
			dev_kfree_skb_any(skb);
			return -ENOMEM;
		}

		rx_swcx->buf_virt_addr = skb;
#endif
		rx_swcx->buf_phy_addr = dma_addr;
	}

	return 0;
}

#ifdef ETHER_PAGE_POOL
/**
 * @brief Create Rx buffer page pool per channel
 *
 * Algorithm: Invokes page pool API to create Rx buffer pool.
 *
 * @param[in] pdata: OSD private data.
 * @param[chan] chan: Rx DMA channel number.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_page_pool_create_per_chan(struct ether_priv_data *pdata,
					   unsigned int chan)
{
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct page_pool_params pp_params = { 0 };
	unsigned int num_pages, pool_size = 1024;
	int ret = 0;

	pp_params.flags = PP_FLAG_DMA_MAP;
	pp_params.pool_size = pool_size;
	num_pages = DIV_ROUND_UP(osi_dma->rx_buf_len, PAGE_SIZE);
	pp_params.order = ilog2(roundup_pow_of_two(num_pages));
	pp_params.nid = dev_to_node(pdata->dev);
	pp_params.dev = pdata->dev;
	pp_params.dma_dir = DMA_FROM_DEVICE;

	pdata->page_pool[chan] = page_pool_create(&pp_params);
	if (IS_ERR(pdata->page_pool[chan])) {
		ret = PTR_ERR(pdata->page_pool[chan]);
		pdata->page_pool[chan] = NULL;
		return ret;
	}

	return ret;
}
#endif

/**
 * @brief Allocate Receive DMA channel ring resources.
 *
 * Algorithm: DMA receive ring will be created for valid channel number
 * provided through DT
 *
 * @param[in] osi_dma: OSI private data structure.
 * @param[in] pdata: OSD private data.
 *
 * @note Invalid channel need to be updated.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_allocate_rx_dma_resources(struct osi_dma_priv_data *osi_dma,
					   struct ether_priv_data *pdata)
{
	unsigned int chan;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < OSI_MGBE_MAX_NUM_CHANS; i++) {
		chan = osi_dma->dma_chans[i];

		if (chan != OSI_INVALID_CHAN_NUM) {
#ifdef ETHER_PAGE_POOL
			ret = ether_page_pool_create_per_chan(pdata, chan);
			if (ret < 0) {
				pr_err("%s(): failed to create page pool\n",
				       __func__);
				goto exit;
			}
#endif
			ret = allocate_rx_dma_resource(osi_dma, pdata->dev,
						       chan);
			if (ret != 0) {
				goto exit;
			}

			ret = ether_allocate_rx_buffers(pdata,
							osi_dma->rx_ring[chan],
							chan);
			if (ret < 0) {
				goto exit;
			}
		}
	}

	return 0;
exit:
	free_rx_dma_resources(osi_dma, pdata);
	return ret;
}

/**
 * @brief Frees allocated DMA resources.
 *
 * Algorithm: Release all DMA Tx resources which are allocated as part of
 * allocated_tx_dma_ring() API.
 *
 * @param[in] osi_dma: OSI private data structure.
 * @param[in] dev: device instance associated with driver.
 */
static void free_tx_dma_resources(struct osi_dma_priv_data *osi_dma,
				  struct device *dev)
{
	unsigned long tx_desc_size = sizeof(struct osi_tx_desc) * osi_dma->tx_ring_sz;
	struct osi_tx_ring *tx_ring = NULL;
	unsigned int i;

	for (i = 0; i < OSI_MGBE_MAX_NUM_CHANS; i++) {
		tx_ring = osi_dma->tx_ring[i];

		if (tx_ring != NULL) {
			if (tx_ring->tx_swcx != NULL) {
				kfree(tx_ring->tx_swcx);
			}

			if (tx_ring->tx_desc != NULL) {
				dma_free_coherent(dev, tx_desc_size,
						  tx_ring->tx_desc,
						  tx_ring->tx_desc_phy_addr);
			}

			kfree(tx_ring);
			tx_ring = NULL;
			osi_dma->tx_ring[i] = NULL;
		}
	}
}

/**
 * @brief Allocate Tx DMA ring
 *
 * Algorithm: DMA transmit ring will be created for valid channel number.
 * Transmit ring updates with descriptors and software context associated
 * with each transmit descriptor.
 *
 * @param[in] osi_dma: OSI  DMA private data structure.
 * @param[in] dev: device instance associated with driver.
 * @param[in] chan: Channel number.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int allocate_tx_dma_resource(struct osi_dma_priv_data *osi_dma,
				    struct device *dev,
				    unsigned int chan)
{
	unsigned int tx_ring_sz = osi_dma->tx_ring_sz;
	unsigned long tx_desc_size;
	unsigned long tx_swcx_size;
	int ret = 0;

	tx_desc_size = sizeof(struct osi_tx_desc) * (unsigned long)tx_ring_sz;
	tx_swcx_size = sizeof(struct osi_tx_swcx) * (unsigned long)tx_ring_sz;

	osi_dma->tx_ring[chan] = kzalloc(sizeof(struct osi_tx_ring),
					 GFP_KERNEL);
	if (osi_dma->tx_ring[chan] == NULL) {
		dev_err(dev, "failed to allocate Tx ring\n");
		return -ENOMEM;
	}
	osi_dma->tx_ring[chan]->tx_desc = dma_alloc_coherent(dev, tx_desc_size,
			(dma_addr_t *)&osi_dma->tx_ring[chan]->tx_desc_phy_addr,
			GFP_KERNEL | __GFP_ZERO);

	if (osi_dma->tx_ring[chan]->tx_desc == NULL) {
		dev_err(dev, "failed to allocate transmit descriptor\n");
		ret = -ENOMEM;
		goto err_tx_desc;
	}

	osi_dma->tx_ring[chan]->tx_swcx = kzalloc(tx_swcx_size, GFP_KERNEL);
	if (osi_dma->tx_ring[chan]->tx_swcx == NULL) {
		dev_err(dev, "failed to allocate Tx ring software context\n");
		ret = -ENOMEM;
		goto err_tx_swcx;
	}

	return 0;

err_tx_swcx:
	dma_free_coherent(dev, tx_desc_size,
			  osi_dma->tx_ring[chan]->tx_desc,
			  osi_dma->tx_ring[chan]->tx_desc_phy_addr);
	osi_dma->tx_ring[chan]->tx_desc = NULL;
err_tx_desc:
	kfree(osi_dma->tx_ring[chan]);
	osi_dma->tx_ring[chan] = NULL;
	return ret;
}

/**
 * @brief Allocate Tx DMA resources.
 *
 * Algorithm: DMA transmit ring will be created for valid channel number
 * provided through DT
 *
 * @param[in] osi_dma: OSI  DMA private data structure.
 * @param[in] dev: device instance associated with driver.
 *
 * @note Invalid channel need to be updated.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_allocate_tx_dma_resources(struct osi_dma_priv_data *osi_dma,
					   struct device *dev)
{
	unsigned int chan;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < OSI_MGBE_MAX_NUM_CHANS; i++) {
		chan = osi_dma->dma_chans[i];

		if (chan != OSI_INVALID_CHAN_NUM) {
			ret = allocate_tx_dma_resource(osi_dma, dev, chan);
			if (ret != 0) {
				goto exit;
			}
		}
	}

	return 0;
exit:
	free_tx_dma_resources(osi_dma, dev);
	return ret;
}

/**
 * @brief Updates invalid channels list and DMA rings.
 *
 * Algorithm: Initialize all DMA Tx/Rx pointers to NULL so that for valid
 * dma_addr_t *channels Tx/Rx rings will be created.
 *
 * For ex: If the number of channels are 2 (nvidia,num_dma_chans = <2>)
 * and channel numbers are 2 and 3 (nvidia,dma_chans = <2 3>),
 * then only for channel 2 and 3 DMA rings will be allocated in
 * allocate_tx/rx_dma_resources() function.
 *
 * @param[in] osi_dma: OSI  DMA private data structure.
 *
 * @note OSD needs to be update number of channels and channel
 * numbers in OSI private data structure.
 */
static void ether_init_invalid_chan_ring(struct osi_dma_priv_data *osi_dma)
{
	unsigned int i;

	for (i = 0; i < OSI_MGBE_MAX_NUM_CHANS; i++) {
		osi_dma->tx_ring[i] = NULL;
		osi_dma->rx_ring[i] = NULL;
	}

	for (i = osi_dma->num_dma_chans; i < OSI_MGBE_MAX_NUM_CHANS; i++) {
		osi_dma->dma_chans[i] = OSI_INVALID_CHAN_NUM;
	}
}

/**
 * @brief Frees allocated DMA resources.
 *
 * Algorithm: Frees all DMA resources which are allocates with
 * allocate_dma_resources() API. Unmap reserved DMA and free
 * reserve sk buffer.
 *
 * @param[in] pdata: OSD private data structure.
 */
static void free_dma_resources(struct ether_priv_data *pdata)
{
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct device *dev = pdata->dev;

	free_tx_dma_resources(osi_dma, dev);
	free_rx_dma_resources(osi_dma, pdata);

	/* unmap reserved DMA*/
	if (osi_dma->resv_buf_phy_addr) {
		dma_unmap_single(dev, osi_dma->resv_buf_phy_addr,
				 osi_dma->rx_buf_len,
				 DMA_FROM_DEVICE);
		osi_dma->resv_buf_phy_addr = 0;
	}

	/* free reserve buffer */
	if (osi_dma->resv_buf_virt_addr) {
		dev_kfree_skb_any(osi_dma->resv_buf_virt_addr);
		osi_dma->resv_buf_virt_addr = NULL;
	}
}

/**
 * @brief Allocate DMA resources for Tx and Rx.
 *
 * Algorithm:
 * 1) Allocate Reserved buffer memory
 * 2) Allocate Tx DMA resources.
 * 3) Allocate Rx DMA resources.
 *
 * @param[in] pdata: OSD private data structure.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_allocate_dma_resources(struct ether_priv_data *pdata)
{
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct sk_buff *skb = NULL;
	int ret = 0;

	ether_init_invalid_chan_ring(osi_dma);

	skb = __netdev_alloc_skb_ip_align(pdata->ndev, osi_dma->rx_buf_len,
					  GFP_KERNEL);
	if (unlikely(skb == NULL)) {
		dev_err(pdata->dev, "Reserve RX skb allocation failed\n");
		ret = -ENOMEM;
		goto error_alloc;
	}

	osi_dma->resv_buf_phy_addr = dma_map_single(pdata->dev,
						    skb->data,
						    osi_dma->rx_buf_len,
						    DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(pdata->dev, osi_dma->resv_buf_phy_addr)
		     != 0)) {
		dev_err(pdata->dev, "Reserve RX skb dma map failed\n");
		ret = -ENOMEM;
		goto error_alloc;
	}

	ret = ether_allocate_tx_dma_resources(osi_dma, pdata->dev);
	if (ret != 0) {
		goto error_alloc;
	}

	ret = ether_allocate_rx_dma_resources(osi_dma, pdata);
	if (ret != 0) {
		free_tx_dma_resources(osi_dma, pdata->dev);
		goto error_alloc;
	}

	osi_dma->resv_buf_virt_addr = (void *)skb;

	return ret;

error_alloc:
	if (skb != NULL) {
		dev_kfree_skb_any(skb);
	}
	osi_dma->resv_buf_virt_addr = NULL;
	osi_dma->resv_buf_phy_addr = 0;

	return ret;
}

/**
 * @brief Initialize default EEE LPI configurations
 *
 * Algorithm: Update the defalt configuration/timers for EEE LPI in driver
 * private data structure
 *
 * @param[in] pdata: OSD private data.
 */
static inline void ether_init_eee_params(struct ether_priv_data *pdata)
{
	if (pdata->hw_feat.eee_sel) {
		pdata->eee_enabled = OSI_ENABLE;
	} else {
		pdata->eee_enabled = OSI_DISABLE;
	}

	pdata->tx_lpi_enabled = pdata->eee_enabled;
	pdata->eee_active = OSI_DISABLE;
	pdata->tx_lpi_timer = OSI_DEFAULT_TX_LPI_TIMER;
}

/**
 * @brief function to set unicast/Broadcast MAC address filter
 *
 * Algorithm: algo to add Unicast MAC/Broadcast address in L2 filter register
 *
 * @param[in] pdata: Pointer to private data structure.
 * @param[in] ioctl_data: OSI IOCTL data structure.
 * @param[in] en_dis: enable(1)/disable(0) L2 filter.
 * @param[in] uc_bc: MAC address(1)/ Broadcast address(0).
 *
 * @note Ethernet driver probe need to be completed successfully
 * with ethernet network device created.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_update_mac_addr_filter(struct ether_priv_data *pdata,
					struct osi_ioctl *ioctl_data,
					unsigned int en_dis,
					unsigned int uc_bc)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	nveu32_t dma_channel = osi_dma->dma_chans[0];
	unsigned char bc_addr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if ((en_dis > OSI_ENABLE) || (uc_bc > ETHER_ADDRESS_MAC)) {
		dev_err(pdata->dev,
			"%s(): wrong argument en_dis=0x%01x, uc_bc=0x%01x\n",
		       __func__, en_dis, uc_bc);
		return -1;
	}

	memset(&ioctl_data->l2_filter, 0x0, sizeof(struct osi_filter));
	/* Set MAC address with DCS set to route all legacy Rx
	 * packets from RxQ0 to default DMA at index 0.
	 */
	ioctl_data->l2_filter.oper_mode = (OSI_OPER_EN_PERFECT |
					   OSI_OPER_DIS_PROMISC |
					   OSI_OPER_DIS_ALLMULTI);
	if (en_dis == OSI_ENABLE) {
		ioctl_data->l2_filter.oper_mode |= OSI_OPER_ADDR_UPDATE;
	} else {
		ioctl_data->l2_filter.oper_mode |= OSI_OPER_ADDR_DEL;
	}

	if (uc_bc == ETHER_ADDRESS_MAC) {
		ioctl_data->l2_filter.index = ETHER_MAC_ADDRESS_INDEX;
		memcpy(ioctl_data->l2_filter.mac_address, osi_core->mac_addr,
		       ETH_ALEN);
	} else {
		if (osi_dma->num_dma_chans > 1) {
			dma_channel = osi_dma->dma_chans[1];
		} else {
			dma_channel = osi_dma->dma_chans[0];
		}
		ioctl_data->l2_filter.index = ETHER_BC_ADDRESS_INDEX;
		memcpy(ioctl_data->l2_filter.mac_address, bc_addr, ETH_ALEN);
	}
	ioctl_data->l2_filter.dma_routing = OSI_ENABLE;
	ioctl_data->l2_filter.dma_chan = dma_channel;
	ioctl_data->l2_filter.addr_mask = OSI_AMASK_DISABLE;
	ioctl_data->l2_filter.src_dest = OSI_DA_MATCH;
	ioctl_data->cmd = OSI_CMD_L2_FILTER;

	return osi_handle_ioctl(osi_core, ioctl_data);
}

/**
 * @brief MII call back for MDIO register write.
 *
 * Algorithm: Invoke OSI layer for PHY register write.
 * phy_write() API from Linux PHY subsystem will call this.
 *
 * @param[in] bus: MDIO bus instances.
 * @param[in] phyaddr: PHY address (ID).
 * @param[in] phyreg: PHY register to write.
 * @param[in] phydata: Data to be written in register.
 *
 * @note  MAC has to be out of reset.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg,
			    u16 phydata)
{
	struct net_device *ndev = bus->priv;
	struct ether_priv_data *pdata = netdev_priv(ndev);

	if (!pdata->clks_enable) {
		dev_err(pdata->dev,
			"%s:No clks available, skipping PHY write\n", __func__);
		return -ENODEV;
	}

	return osi_write_phy_reg(pdata->osi_core, (unsigned int)phyaddr,
				 (unsigned int)phyreg, phydata);
}

/**
 * @brief MII call back for MDIO register read.
 *
 * Algorithm: Invoke OSI layer for PHY register read.
 * phy_read() API from Linux subsystem will call this.
 *
 * @param[in] bus: MDIO bus instances.
 * @param[in] phyaddr: PHY address (ID).
 * @param[in] phyreg: PHY register to read.
 *
 * @note  MAC has to be out of reset.
 *
 * @retval data from PHY register on success
 * @retval "nagative value" on failure.
 */
static int ether_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	struct ether_priv_data *pdata = netdev_priv(ndev);

	if (!pdata->clks_enable) {
		dev_err(pdata->dev,
			"%s:No clks available, skipping PHY read\n", __func__);
		return -ENODEV;
	}

	return osi_read_phy_reg(pdata->osi_core, (unsigned int)phyaddr,
				(unsigned int)phyreg);
}

/**
 * @brief MDIO bus registration.
 *
 * Algorithm: Registers MDIO bus if there is mdio sub DT node
 * as part of MAC DT node.
 *
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_mdio_register(struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;
	struct mii_bus *new_bus = NULL;
	int ret = 0;

	if (pdata->mdio_node == NULL) {
		pdata->mii = NULL;
		return 0;
	}

	new_bus = devm_mdiobus_alloc(dev);
	if (new_bus == NULL) {
		ret = -ENOMEM;
		dev_err(dev, "failed to allocate MDIO bus\n");
		goto exit;
	}

	new_bus->name = "nvethernet_mdio_bus";
	new_bus->read = ether_mdio_read;
	new_bus->write = ether_mdio_write;
	ret = snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s", dev_name(dev));
	if (ret < 0) {
		dev_err(dev, "%s:encoding error", __func__);
		goto exit;
	}
	new_bus->priv = pdata->ndev;
	new_bus->parent = dev;

	ret = of_mdiobus_register(new_bus, pdata->mdio_node);
	if (ret) {
		dev_err(dev, "failed to register MDIO bus (%s)\n",
			new_bus->name);
		goto exit;
	}

	pdata->mii = new_bus;

exit:
	return ret;
}

/**
 * @brief Call back to handle bring up of Ethernet interface
 *
 * Algorithm: This routine takes care of below
 * 1) PHY initialization
 * 2) request tx/rx/common irqs
 * 3) HW initialization
 * 4) OSD private data structure initialization
 * 5) Starting the PHY
 *
 * @param[in] dev: Net device data structure.
 *
 * @note Ethernet driver probe need to be completed successfully
 * with ethernet network device created.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_open(struct net_device *dev)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_ioctl ioctl_data = {};
	unsigned int chan = 0x0;
	int ret = 0, i;

	/* Reset the PHY */
	if (gpio_is_valid(pdata->phy_reset)) {
		gpio_set_value(pdata->phy_reset, 0);
		usleep_range(pdata->phy_reset_duration,
			     pdata->phy_reset_duration + 1);
		gpio_set_value(pdata->phy_reset, 1);
		msleep(pdata->phy_reset_post_delay);
	}

	ether_start_ivc(pdata);

	if (osi_core->mac == OSI_MAC_HW_MGBE) {
		ret = pm_runtime_get_sync(pdata->dev);
		if (ret < 0) {
			dev_err(&dev->dev, "failed to ungate MGBE power\n");
			goto err_get_sync;
		}
	}

	ret = ether_enable_clks(pdata);
	if (ret < 0) {
		dev_err(&dev->dev, "failed to enable clks\n");
		goto err_en_clks;
	}

	if (pdata->mac_rst) {
		ret = reset_control_reset(pdata->mac_rst);
		if (ret < 0) {
			dev_err(&dev->dev, "failed to reset MAC HW\n");
			goto err_mac_rst;
		}
	}

	if (pdata->xpcs_rst) {
		ret = reset_control_reset(pdata->xpcs_rst);
		if (ret < 0) {
			dev_err(&dev->dev, "failed to reset XPCS HW\n");
			goto err_xpcs_rst;
		}
	}

	ioctl_data.cmd = OSI_CMD_POLL_FOR_MAC_RST;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0) {
		dev_err(&dev->dev, "failed to poll MAC Software reset\n");
		goto err_poll_swr;
	}

	ret = ether_mdio_register(pdata);
	if (ret < 0) {
		dev_err(&dev->dev, "failed to register MDIO bus\n");
		goto err_mdio_reg;
	}

	atomic_set(&pdata->padcal_in_progress, OSI_DISABLE);
	/* PHY reset and initialization */
	ret = ether_phy_init(dev);
	if (ret < 0) {
		dev_err(&dev->dev, "%s: Cannot attach to PHY (error: %d)\n",
			__func__, ret);
		goto err_phy_init;
	}

	osi_set_rx_buf_len(pdata->osi_dma);

	ret = ether_allocate_dma_resources(pdata);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to allocate DMA resources\n");
		goto err_alloc;
	}

	/* initialize MAC/MTL/DMA Common registers */
	ret = osi_hw_core_init(pdata->osi_core);
	if (ret < 0) {
		dev_err(pdata->dev,
			"%s: failed to initialize MAC HW core with reason %d\n",
			__func__, ret);
		goto err_hw_init;
	}

	ret = ether_update_mac_addr_filter(pdata, &ioctl_data, OSI_ENABLE,
					   ETHER_ADDRESS_MAC);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to set MAC address\n");
		goto err_hw_init;
	}

	ret = ether_update_mac_addr_filter(pdata, &ioctl_data, OSI_ENABLE,
					   ETHER_ADDRESS_BC);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to set BC address\n");
		goto err_hw_init;
	}

	/* DMA init */
	ret = osi_hw_dma_init(pdata->osi_dma);
	if (ret < 0) {
		dev_err(pdata->dev,
			"%s: failed to initialize MAC HW DMA with reason %d\n",
			__func__, ret);
		goto err_hw_init;
	}

	for (i = 0; i < pdata->osi_dma->num_dma_chans; i++) {
		chan = pdata->osi_dma->dma_chans[i];
		ioctl_data.cmd = OSI_CMD_FREE_TS;
		if ((pdata->osi_dma->ptp_flag & OSI_PTP_SYNC_ONESTEP) ==
		    OSI_PTP_SYNC_ONESTEP) {
			ioctl_data.arg1_u32 = OSI_NONE;
		} else {
			ioctl_data.arg1_u32 = chan;
		}

		ret = osi_handle_ioctl(osi_core, &ioctl_data);
		if (ret < 0) {
			dev_err(&dev->dev,
				"%s: failed to free TX TS for channel %d\n",
				__func__, chan);
			goto err_hw_init;
		}
	}

	ret = ether_pad_calibrate(pdata);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to do pad caliberation\n");
		goto err_hw_init;
	}

	/* As all registers reset as part of ether_close(), reset private
	 * structure variable as well */
#ifdef ETHER_VLAN_VID_SUPPORT
	pdata->vlan_hash_filtering = OSI_PERFECT_FILTER_MODE;
#endif /* ETHER_VLAN_VID_SUPPORT */
	pdata->l2_filtering_mode = OSI_PERFECT_FILTER_MODE;

	/* Initialize PTP */
	ret = ether_ptp_init(pdata);
	if (ret < 0) {
		dev_err(pdata->dev,
			"%s:failed to initialize PTP with reason %d\n",
			__func__, ret);
		goto err_hw_init;
	}

	/* Enable napi before requesting irq to be ready to handle it */
	ether_napi_enable(pdata);

	/* request tx/rx/common irq */
	ret = ether_request_irqs(pdata);
	if (ret < 0) {
		dev_err(&dev->dev,
			"%s: failed to get tx rx irqs with reason %d\n",
			__func__, ret);
		goto err_r_irq;
	}

	/* Init EEE configuration */
	ether_init_eee_params(pdata);

	/* start PHY */
	phy_start(pdata->phydev);

	/* start network queues */
	netif_tx_start_all_queues(pdata->ndev);

	pdata->stats_timer = ETHER_STATS_TIMER;
#ifdef HSI_SUPPORT
	/* Override stats_timer to getting MCC error stats as per
	 * hsi.err_time_threshold configuration
	 */
	if (osi_core->hsi.err_time_threshold < ETHER_STATS_TIMER)
		pdata->stats_timer = osi_core->hsi.err_time_threshold;
#endif
	ether_stats_work_queue_start(pdata);

#ifdef HSI_SUPPORT
	schedule_delayed_work(&pdata->ether_hsi_work,
			      msecs_to_jiffies(osi_core->hsi.err_time_threshold));
#endif

#ifdef ETHER_NVGRO
	/* start NVGRO timer for purging */
	mod_timer(&pdata->nvgro_timer,
		  jiffies + msecs_to_jiffies(pdata->nvgro_timer_intrvl));
#endif
	return ret;

err_r_irq:
	ether_napi_disable(pdata);
	ether_ptp_remove(pdata);
err_hw_init:
	free_dma_resources(pdata);
err_alloc:
	if (pdata->phydev) {
		phy_disconnect(pdata->phydev);
	}
err_phy_init:
	if (pdata->mii != NULL) {
		mdiobus_unregister(pdata->mii);
	}
err_poll_swr:
err_mdio_reg:
	if (pdata->xpcs_rst) {
		reset_control_assert(pdata->xpcs_rst);
	}
err_xpcs_rst:
	if (pdata->mac_rst) {
		reset_control_assert(pdata->mac_rst);
	}
err_mac_rst:
	ether_disable_clks(pdata);
	if (gpio_is_valid(pdata->phy_reset)) {
		gpio_set_value(pdata->phy_reset, OSI_DISABLE);
	}
err_en_clks:
err_get_sync:
	if (osi_core->mac == OSI_MAC_HW_MGBE)
		pm_runtime_put_sync(pdata->dev);

	return ret;
}

/**
 * @brief Helper function to clear the sw stats structures.
 *
 * Algorithm: This routine clears the following sw stats structures.
 * 1) struct osi_mmc_counters
 * 2) struct ether_xtra_stat_counters
 * 3) struct osi_xtra_dma_stat_counters
 * 4) struct osi_pkt_err_stats
 *
 * @param[in] pdata: Pointer to OSD private data structure.
 *
 * @note  This routine is invoked when interface is going down, not for suspend.
 */
static inline void ether_reset_stats(struct ether_priv_data *pdata)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;

	memset(&osi_core->mmc, 0U, sizeof(struct osi_mmc_counters));
	memset(&pdata->xstats, 0U,
	       sizeof(struct ether_xtra_stat_counters));
	memset(&osi_dma->dstats, 0U,
	       sizeof(struct osi_xtra_dma_stat_counters));
	memset(&osi_dma->pkt_err_stats, 0U, sizeof(struct osi_pkt_err_stats));
}

/**
 * @brief Delete L2 filters from HW register when interface is down
 *
 *  Algorithm: This routine takes care of below
 *  - Remove MAC address filter
 *  - Remove BC address filter (remove DMA channel form DCS field)
 *  - Remove all L2 filters
 *
 * @param[in] pdata: Pointer to private data structure.
 *
 * @note  MAC Interface need to be registered.
 */
static inline void ether_delete_l2_filter(struct ether_priv_data *pdata)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_ioctl ioctl_data = {};
	int ret, i;

	memset(&ioctl_data.l2_filter, 0x0, sizeof(struct osi_filter));
	ret = ether_update_mac_addr_filter(pdata, &ioctl_data, OSI_DISABLE,
					   ETHER_ADDRESS_MAC);
	if (ret < 0) {
		dev_err(pdata->dev, "issue in deleting MAC address\n");
	}

	memset(&ioctl_data.l2_filter, 0x0, sizeof(struct osi_filter));
	ret = ether_update_mac_addr_filter(pdata, &ioctl_data, OSI_DISABLE,
					   ETHER_ADDRESS_BC);
	if (ret < 0) {
		dev_err(pdata->dev, "issue in deleting BC address\n");
	}

	/* Loop to delete l2 address list filters */
	for (i = ETHER_MAC_ADDRESS_INDEX + 1; i < pdata->last_filter_index;
	     i++) {
		/* Reset the filter structure to avoid any old value */
		memset(&ioctl_data.l2_filter, 0x0, sizeof(struct osi_filter));
		ioctl_data.l2_filter.oper_mode = OSI_OPER_ADDR_DEL;
		ioctl_data.l2_filter.index = i;
		ioctl_data.l2_filter.dma_routing = OSI_ENABLE;
		memcpy(ioctl_data.l2_filter.mac_address,
		       pdata->mac_addr[i].addr, ETH_ALEN);
		ioctl_data.l2_filter.dma_chan = pdata->mac_addr[i].dma_chan;
		ioctl_data.l2_filter.addr_mask = OSI_AMASK_DISABLE;
		ioctl_data.l2_filter.src_dest = OSI_DA_MATCH;
		ioctl_data.cmd = OSI_CMD_L2_FILTER;

		ret = osi_handle_ioctl(osi_core, &ioctl_data);
		if (ret < 0) {
			dev_err(pdata->dev,
				"failed to delete L2 filter index = %d\n", i);
			return;
		}
	}

	pdata->last_filter_index = 0;
}

/**
 * @brief Call to delete nodes form tx timestamp skb list
 *
 * Algorithm:
 * - Stop work queue
 * - Delete nodes from link list
 *
 * @param[in] pdata: Pointer to private data structure.
 */
static inline void ether_flush_tx_ts_skb_list(struct ether_priv_data *pdata)
{
	struct ether_tx_ts_skb_list *pnode;
	struct list_head *head_node, *temp_head_node;
	unsigned long flags;

	/* stop workqueue */
	cancel_delayed_work_sync(&pdata->tx_ts_work);

	raw_spin_lock_irqsave(&pdata->txts_lock, flags);
	/* Delete nodes from list and rest static memory for reuse */
	if (!list_empty(&pdata->tx_ts_skb_head)) {
		list_for_each_safe(head_node, temp_head_node,
				   &pdata->tx_ts_skb_head) {
			pnode = list_entry(head_node,
					   struct ether_tx_ts_skb_list,
					   list_head);
			dev_kfree_skb(pnode->skb);
			list_del(head_node);
			pnode->in_use = OSI_DISABLE;
		}
	}
	raw_spin_unlock_irqrestore(&pdata->txts_lock, flags);
}

/**
 * @brief Call back to handle bring down of Ethernet interface
 *
 * Algorithm: This routine takes care of below
 * 1) Stopping PHY
 * 2) Freeing tx/rx/common irqs
 *
 * @param[in] ndev: Net device data structure.
 *
 * @note  MAC Interface need to be registered.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_close(struct net_device *ndev)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	unsigned int chan = 0x0;
	int i;

#ifdef ETHER_NVGRO
	del_timer_sync(&pdata->nvgro_timer);
	/* TODO: purge the queues */
#endif

	/* Unregister broadcasting MAC timestamp to clients */
	tegra_unregister_hwtime_source(ndev);

	/* Stop workqueue to get further scheduled */
	ether_stats_work_queue_stop(pdata);

#ifdef HSI_SUPPORT
	cancel_delayed_work_sync(&pdata->ether_hsi_work);
#endif
	/* Stop and disconnect the PHY */
	if (pdata->phydev != NULL) {
		/* Check and clear WoL status */
		if (device_may_wakeup(&ndev->dev)) {
			if (disable_irq_wake(pdata->phydev->irq)) {
				dev_warn(pdata->dev,
					 "PHY disable irq wake fail\n");
			}
			device_init_wakeup(&ndev->dev, false);
		}

		/* Link down phy interrupt is generated asynchrounously during phy stop
		 * or cable unplug event, causing the phy state machine to run again in
		 * the workqueue context. Here explicitly disable interrupt to avoid race
		 * condition between phy framework and driver context execution of phy apis.
		 */
		if (phy_interrupt_is_valid(pdata->phydev))
			phy_disable_interrupts(pdata->phydev);

		phy_stop(pdata->phydev);
		phy_disconnect(pdata->phydev);

		if (gpio_is_valid(pdata->phy_reset)) {
			gpio_set_value(pdata->phy_reset, 0);
		}
		pdata->phydev = NULL;
	}
	cancel_delayed_work_sync(&pdata->set_speed_work);

	/* turn off sources of data into dev */
	netif_tx_disable(pdata->ndev);

	/* Free tx rx and common irqs */
	ether_free_irqs(pdata);

	/* Cancel hrtimer */
	for (i = 0; i < pdata->osi_dma->num_dma_chans; i++) {
		chan = pdata->osi_dma->dma_chans[i];
		if (atomic_read(&pdata->tx_napi[chan]->tx_usecs_timer_armed)
		    == OSI_ENABLE) {
			hrtimer_cancel(&pdata->tx_napi[chan]->tx_usecs_timer);
			atomic_set(&pdata->tx_napi[chan]->tx_usecs_timer_armed,
				   OSI_DISABLE);
		}
	}

	/* Delete MAC filters */
	ether_delete_l2_filter(pdata);

	/* DMA De init */
	osi_hw_dma_deinit(pdata->osi_dma);

	ether_napi_disable(pdata);

	/* free DMA resources after DMA stop */
	free_dma_resources(pdata);

	/* PTP de-init */
	ether_ptp_remove(pdata);

	/* MAC deinit which inturn stop MAC Tx,Rx */
	osi_hw_core_deinit(pdata->osi_core);

	/* stop tx ts pending SKB workqueue and remove skb nodes */
	ether_flush_tx_ts_skb_list(pdata);

	tasklet_kill(&pdata->lane_restart_task);

	ether_stop_ivc(pdata);

	if (pdata->xpcs_rst) {
		reset_control_assert(pdata->xpcs_rst);
	}

	/* All MDIO interfaces must be disabled before resetting the MAC */
	if (pdata->mii)
		mdiobus_unregister(pdata->mii);

	/* Assert MAC RST gpio */
	if (pdata->mac_rst) {
		reset_control_assert(pdata->mac_rst);
	}

	/* Disable clock */
	ether_disable_clks(pdata);

	if (pdata->osi_core->mac == OSI_MAC_HW_MGBE)
		pm_runtime_put_sync(pdata->dev);

	/* Reset stats since interface is going down */
	ether_reset_stats(pdata);

	/* Reset MAC loopback variable */
	pdata->mac_loopback_mode = OSI_DISABLE;

	return 0;
}

/**
 * @brief Helper function to check if TSO is used in given skb.
 *
 * Algorithm:
 * 1) Check if driver received a TSO/LSO/GSO packet
 * 2) If so, store the packet details like MSS(Maximum Segment Size),
 * packet header length, packet payload length, tcp/udp header length.
 *
 * @param[in] tx_pkt_cx: Pointer to packet context information structure.
 * @param[in] skb: socket buffer.
 *
 * @retval 0 if not a TSO packet
 * @retval 1 on success
 * @retval "negative value" on failure.
 */
static int ether_handle_tso(struct osi_tx_pkt_cx *tx_pkt_cx,
			    struct sk_buff *skb)
{
	int ret = 1;

	if (skb_is_gso(skb) == 0) {
		return 0;
	}

	if (skb_header_cloned(skb)) {
		ret = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
		if (ret) {
			return ret;
		}
	}

#if (KERNEL_VERSION(5, 9, 0) < LINUX_VERSION_CODE)
	/* Start filling packet details in Tx_pkt_cx */
	if (skb_shinfo(skb)->gso_type & (SKB_GSO_UDP_L4)) {
#else
	if (skb_shinfo(skb)->gso_type & (SKB_GSO_UDP)) {
#endif
		tx_pkt_cx->tcp_udp_hdrlen = sizeof(struct udphdr);
		tx_pkt_cx->mss = skb_shinfo(skb)->gso_size -
			sizeof(struct udphdr);
	} else {
		tx_pkt_cx->tcp_udp_hdrlen = tcp_hdrlen(skb);
		tx_pkt_cx->mss = skb_shinfo(skb)->gso_size;
	}
	tx_pkt_cx->total_hdrlen = skb_transport_offset(skb) +
			tx_pkt_cx->tcp_udp_hdrlen;
	tx_pkt_cx->payload_len = (skb->len - tx_pkt_cx->total_hdrlen);

	netdev_dbg(skb->dev, "mss           =%u\n", tx_pkt_cx->mss);
	netdev_dbg(skb->dev, "payload_len   =%u\n", tx_pkt_cx->payload_len);
	netdev_dbg(skb->dev, "tcp_udp_hdrlen=%u\n", tx_pkt_cx->tcp_udp_hdrlen);
	netdev_dbg(skb->dev, "total_hdrlen  =%u\n", tx_pkt_cx->total_hdrlen);

	return 1;
}

/**
 * @brief Rollback previous descriptor if failure.
 *
 * Algorithm:
 * - Go over all descriptor until count is 0
 *  - Unmap physical address.
 *  - Reset length.
 *  - Reset flags.
 *
 * @param[in] pdata: OSD private data.
 * @param[in] tx_ring: Tx ring instance associated with channel number.
 * @param[in] cur_tx_idx: Local descriptor index
 * @param[in] count: Number of descriptor filled
 */
static void ether_tx_swcx_rollback(struct ether_priv_data *pdata,
				   struct osi_tx_ring *tx_ring,
				   unsigned int cur_tx_idx,
				   unsigned int count)
{
	struct device *dev = pdata->dev;
	struct osi_tx_swcx *tx_swcx = NULL;

	while (count > 0) {
		DECR_TX_DESC_INDEX(cur_tx_idx, pdata->osi_dma->tx_ring_sz);
		tx_swcx = tx_ring->tx_swcx + cur_tx_idx;
		if (tx_swcx->buf_phy_addr) {
			if ((tx_swcx->flags & OSI_PKT_CX_PAGED_BUF) ==
			     OSI_PKT_CX_PAGED_BUF) {
				dma_unmap_page(dev, tx_swcx->buf_phy_addr,
					       tx_swcx->len, DMA_TO_DEVICE);
			} else {
				dma_unmap_single(dev, tx_swcx->buf_phy_addr,
						 tx_swcx->len, DMA_TO_DEVICE);
			}
			tx_swcx->buf_phy_addr = 0;
		}
		tx_swcx->len = 0;
		tx_swcx->flags = 0;
		count--;
	}
}

/**
 * @brief Tx ring software context allocation.
 *
 * Algorithm:
 * 1) Map skb data buffer to DMA mappable address.
 * 2) Updated dma address, len and buffer address. This info will be used
 * OSI layer for data transmission and buffer cleanup.
 *
 * @param[in] dev: device instance associated with driver.
 * @param[in] tx_ring: Tx ring instance associated with channel number.
 * @param[in] skb: socket buffer.
 *
 * @retval "number of descriptors" on success
 * @retval "negative value"  on failure.
 */
static int ether_tx_swcx_alloc(struct ether_priv_data *pdata,
			       struct osi_tx_ring *tx_ring,
			       struct sk_buff *skb)
{
	struct osi_tx_pkt_cx *tx_pkt_cx = &tx_ring->tx_pkt_cx;
	unsigned int cur_tx_idx = tx_ring->cur_tx_idx;
	struct osi_tx_swcx *tx_swcx = NULL;
	struct device *dev = pdata->dev;
	unsigned int len = 0, offset = 0, size = 0;
	int cnt = 0, ret = 0, i, num_frags;
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	skb_frag_t *frag;
#else
	struct skb_frag_struct *frag;
#endif
	unsigned int page_idx, page_offset;
	unsigned int max_data_len_per_txd = (unsigned int)
					ETHER_TX_MAX_BUFF_SIZE;

	memset(tx_pkt_cx, 0, sizeof(*tx_pkt_cx));

	ret = ether_handle_tso(tx_pkt_cx, skb);
	if (unlikely(ret < 0)) {
		dev_err(dev, "Unable to handle TSO packet (%d)\n", ret);
		/* Caller will take care of consuming skb */
		return ret;
	}

	if (ret == 0) {
		dev_dbg(dev, "Not a TSO packet\n");
		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			tx_pkt_cx->flags |= OSI_PKT_CX_CSUM;
		}

		tx_pkt_cx->flags |= OSI_PKT_CX_LEN;
		tx_pkt_cx->payload_len = skb->len;
	} else {
		tx_pkt_cx->flags |= OSI_PKT_CX_TSO;
	}

	if (unlikely(skb_vlan_tag_present(skb))) {
		tx_pkt_cx->vtag_id = skb_vlan_tag_get(skb);
		tx_pkt_cx->flags |= OSI_PKT_CX_VLAN;
	}

	if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)) {
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		tx_pkt_cx->flags |= OSI_PKT_CX_PTP;
	}

	if (((tx_pkt_cx->flags & OSI_PKT_CX_VLAN) == OSI_PKT_CX_VLAN) ||
	    ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) ||
	    (((tx_pkt_cx->flags & OSI_PKT_CX_PTP) == OSI_PKT_CX_PTP) &&
	      /* Check only MGBE as we need ctx for both sync mode */
	      ((pdata->osi_core->mac == OSI_MAC_HW_MGBE) ||
	       ((pdata->osi_dma->ptp_flag & OSI_PTP_SYNC_ONESTEP) ==
		OSI_PTP_SYNC_ONESTEP)))) {
		tx_swcx = tx_ring->tx_swcx + cur_tx_idx;
		if (tx_swcx->len) {
			return 0;
		}

		tx_swcx->len = -1;
		cnt++;
		INCR_TX_DESC_INDEX(cur_tx_idx, pdata->osi_dma->tx_ring_sz);
	}

	if ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) {
		/* For TSO map the header in separate desc. */
		len = tx_pkt_cx->total_hdrlen;
	} else {
		len = skb_headlen(skb);
	}

	/* Map the linear buffers from the skb first.
	 * For TSO only upto TCP header is filled in first desc.
	 */
	while (valid_tx_len(len)) {
		tx_swcx = tx_ring->tx_swcx + cur_tx_idx;
		if (unlikely(tx_swcx->len)) {
			goto desc_not_free;
		}

		size = min(len, max_data_len_per_txd);

		tx_swcx->buf_phy_addr = dma_map_single(dev,
						skb->data + offset,
						size,
						DMA_TO_DEVICE);
		if (unlikely(dma_mapping_error(dev,
			     tx_swcx->buf_phy_addr))) {
			dev_err(dev, "failed to map Tx buffer\n");
			ret = -ENOMEM;
			goto dma_map_failed;
		}
		tx_swcx->flags &= ~OSI_PKT_CX_PAGED_BUF;

		tx_swcx->len = size;
		len -= size;
		offset += size;
		cnt++;
		INCR_TX_DESC_INDEX(cur_tx_idx, pdata->osi_dma->tx_ring_sz);
	}

	/* Map remaining payload from linear buffer
	 * to subsequent descriptors in case of TSO
	 */
	if ((tx_pkt_cx->flags & OSI_PKT_CX_TSO) == OSI_PKT_CX_TSO) {
		len = skb_headlen(skb) - tx_pkt_cx->total_hdrlen;
		while (valid_tx_len(len)) {
			tx_swcx = tx_ring->tx_swcx + cur_tx_idx;

			if (unlikely(tx_swcx->len)) {
				goto desc_not_free;
			}

			size = min(len, max_data_len_per_txd);
			tx_swcx->buf_phy_addr = dma_map_single(dev,
							skb->data + offset,
							size,
							DMA_TO_DEVICE);
			if (unlikely(dma_mapping_error(dev,
				     tx_swcx->buf_phy_addr))) {
				dev_err(dev, "failed to map Tx buffer\n");
				ret = -ENOMEM;
				goto dma_map_failed;
			}

			tx_swcx->flags &= ~OSI_PKT_CX_PAGED_BUF;
			tx_swcx->len = size;
			len -= size;
			offset += size;
			cnt++;
			INCR_TX_DESC_INDEX(cur_tx_idx, pdata->osi_dma->tx_ring_sz);
		}
	}

	/* Process fragmented skb's */
	num_frags = skb_shinfo(skb)->nr_frags;
	for (i = 0; i < num_frags; i++) {
		offset = 0;
		frag = &skb_shinfo(skb)->frags[i];
		len = skb_frag_size(frag);
		while (valid_tx_len(len)) {
			tx_swcx = tx_ring->tx_swcx + cur_tx_idx;
			if (unlikely(tx_swcx->len)) {
				goto desc_not_free;
			}

			size = min(len, max_data_len_per_txd);
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
			page_idx = (frag->bv_offset + offset) >> PAGE_SHIFT;
			page_offset = (frag->bv_offset + offset) & ~PAGE_MASK;
#else
			page_idx = (frag->page_offset + offset) >> PAGE_SHIFT;
			page_offset = (frag->page_offset + offset) & ~PAGE_MASK;
#endif
			tx_swcx->buf_phy_addr = dma_map_page(dev,
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
						(frag->bv_page + page_idx),
#else
						(frag->page.p + page_idx),
#endif
						page_offset, size,
						DMA_TO_DEVICE);
			if (unlikely(dma_mapping_error(dev,
				     tx_swcx->buf_phy_addr))) {
				dev_err(dev, "failed to map Tx buffer\n");
				ret = -ENOMEM;
				goto dma_map_failed;
			}
			tx_swcx->flags |= OSI_PKT_CX_PAGED_BUF;

			tx_swcx->len = size;
			len -= size;
			offset += size;
			cnt++;
			INCR_TX_DESC_INDEX(cur_tx_idx, pdata->osi_dma->tx_ring_sz);
		}
	}

	tx_swcx->buf_virt_addr = skb;
	tx_pkt_cx->desc_cnt = cnt;

	return cnt;

desc_not_free:
	ret = 0;

dma_map_failed:
	/* Failed to fill current desc. Rollback previous desc's */
	ether_tx_swcx_rollback(pdata, tx_ring, cur_tx_idx, cnt);
	return ret;
}

/**
 * @brief Select queue based on user priority
 *
 * Algorithm:
 * 1) Select the correct queue index based which has priority of queue
 * same as skb->priority
 * 2) default select queue array index 0
 *
 * @param[in] dev: Network device pointer
 * @param[in] skb: sk_buff pointer, buffer data to send
 * @param[in] accel_priv: private data used for L2 forwarding offload
 * @param[in] fallback: fallback function pointer
 *
 * @retval "transmit queue index"
 */
static unsigned short ether_select_queue(struct net_device *dev,
					 struct sk_buff *skb,
#if (KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE)
					void *accel_priv,
					select_queue_fallback_t fallback)
#else
					 struct net_device *sb_dev)
#endif
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	unsigned short txqueue_select = 0;
	unsigned int i, mtlq;
	unsigned int priority = skb->priority;

	if (skb_vlan_tag_present(skb)) {
		priority = skb_vlan_tag_get_prio(skb);
	}

	for (i = 0; i < osi_core->num_mtl_queues; i++) {
		mtlq = osi_core->mtl_queues[i];
		if (pdata->txq_prio[mtlq] == priority) {
			txqueue_select = (unsigned short)i;
			break;
		}
	}

	return txqueue_select;
}

/**
 * @brief Network layer hook for data transmission.
 *
 * Algorithm:
 * 1) Allocate software context (DMA address for the buffer) for the data.
 * 2) Invoke OSI for data transmission.
 *
 * @param[in] skb: SKB data structure.
 * @param[in] ndev: Net device structure.
 *
 * @note  MAC and PHY need to be initialized.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	unsigned int qinx = skb_get_queue_mapping(skb);
	unsigned int chan = osi_dma->dma_chans[qinx];
	struct osi_tx_ring *tx_ring = osi_dma->tx_ring[chan];
#ifdef OSI_ERR_DEBUG
	unsigned int cur_tx_idx = tx_ring->cur_tx_idx;
#endif
	int count = 0;
	int ret;

	count = ether_tx_swcx_alloc(pdata, tx_ring, skb);
	if (count <= 0) {
		if (count == 0) {
			netif_stop_subqueue(ndev, qinx);
			netdev_err(ndev, "Tx ring[%d] is full\n", chan);
			return NETDEV_TX_BUSY;
		}
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	ret = osi_hw_transmit(osi_dma, chan);
#ifdef OSI_ERR_DEBUG
	if (ret < 0) {
		INCR_TX_DESC_INDEX(cur_tx_idx, count);
		ether_tx_swcx_rollback(pdata, tx_ring, cur_tx_idx, count);
		netdev_err(ndev, "%s() dropping corrupted skb\n", __func__);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
#endif

	if (ether_avail_txdesc_cnt(osi_dma, tx_ring) <= ETHER_TX_DESC_THRESHOLD) {
		netif_stop_subqueue(ndev, qinx);
		netdev_dbg(ndev, "Tx ring[%d] insufficient desc.\n", chan);
	}

	if (osi_dma->use_tx_usecs == OSI_ENABLE &&
	    atomic_read(&pdata->tx_napi[chan]->tx_usecs_timer_armed) ==
			OSI_DISABLE) {
		atomic_set(&pdata->tx_napi[chan]->tx_usecs_timer_armed,
			   OSI_ENABLE);
		hrtimer_start(&pdata->tx_napi[chan]->tx_usecs_timer,
			      osi_dma->tx_usecs * NSEC_PER_USEC,
			      HRTIMER_MODE_REL);
	}
	return NETDEV_TX_OK;
}

/**
 * @brief Function to configure the multicast address in device.
 *
 * Algorithm: This function collects all the multicast addresses and updates the
 * device.
 *
 * @param[in] dev: Pointer to net_device structure.
 *
 * @note  MAC and PHY need to be initialized.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_prepare_mc_list(struct net_device *dev,
				 struct osi_ioctl *ioctl_data,
				 unsigned int *mac_addr_idx)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct netdev_hw_addr *ha;
	unsigned int i = *mac_addr_idx;
	int ret = -1;

	if (ioctl_data == NULL) {
		dev_err(pdata->dev, "ioctl_data is NULL\n");
		return ret;
	}

	memset(&ioctl_data->l2_filter, 0x0, sizeof(struct osi_filter));

	if (pdata->l2_filtering_mode == OSI_HASH_FILTER_MODE) {
		dev_err(pdata->dev,
			"HASH FILTERING for mc addresses not Supported in SW\n");
		ioctl_data->l2_filter.oper_mode = (OSI_OPER_EN_PERFECT |
						   OSI_OPER_DIS_PROMISC |
						   OSI_OPER_DIS_ALLMULTI);
		ioctl_data->cmd = OSI_CMD_L2_FILTER;
		return osi_handle_ioctl(osi_core, ioctl_data);
	/* address 0 is used for DUT DA so compare with
	 *  pdata->num_mac_addr_regs - 1
	 */
	} else if (netdev_mc_count(dev) > (pdata->num_mac_addr_regs - 1)) {
		/* switch to PROMISCUOUS mode */
		ioctl_data->l2_filter.oper_mode = (OSI_OPER_DIS_PERFECT |
						   OSI_OPER_EN_PROMISC |
						   OSI_OPER_DIS_ALLMULTI);
		dev_dbg(pdata->dev, "enabling Promiscuous mode\n");

		ioctl_data->cmd = OSI_CMD_L2_FILTER;
		return osi_handle_ioctl(osi_core, ioctl_data);
	} else {
		dev_dbg(pdata->dev,
			"select PERFECT FILTERING for mc addresses, mc_count = %d, num_mac_addr_regs = %d\n",
			netdev_mc_count(dev), pdata->num_mac_addr_regs);

		ioctl_data->l2_filter.oper_mode = (OSI_OPER_EN_PERFECT |
						   OSI_OPER_ADDR_UPDATE |
						   OSI_OPER_DIS_PROMISC |
						   OSI_OPER_DIS_ALLMULTI);
		netdev_for_each_mc_addr(ha, dev) {
			dev_dbg(pdata->dev,
				"mc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n",
				i, ha->addr[0], ha->addr[1], ha->addr[2],
				ha->addr[3], ha->addr[4], ha->addr[5]);
			ioctl_data->l2_filter.index = i;
			memcpy(ioctl_data->l2_filter.mac_address, ha->addr,
			       ETH_ALEN);
			ioctl_data->l2_filter.dma_routing = OSI_ENABLE;
			if (osi_dma->num_dma_chans > 1) {
				ioctl_data->l2_filter.dma_chan =
							  osi_dma->dma_chans[1];
			} else {
				ioctl_data->l2_filter.dma_chan =
							  osi_dma->dma_chans[0];
			}
			ioctl_data->l2_filter.addr_mask = OSI_AMASK_DISABLE;
			ioctl_data->l2_filter.src_dest = OSI_DA_MATCH;
			ioctl_data->cmd = OSI_CMD_L2_FILTER;
			ret = osi_handle_ioctl(pdata->osi_core, ioctl_data);
			if (ret < 0) {
				dev_err(pdata->dev, "issue in creating mc list\n");
				*mac_addr_idx = i;
				return ret;
			}

			memcpy(pdata->mac_addr[i].addr, ha->addr, ETH_ALEN);
			pdata->mac_addr[i].dma_chan = ioctl_data->l2_filter.dma_chan;

			if (i == EQOS_MAX_MAC_ADDRESS_FILTER - 1) {
				dev_err(pdata->dev, "Configured max number of supported MAC, ignoring it\n");
				break;
			}
			i++;
		}
		*mac_addr_idx = i;
	}

	return ret;
}

/**
 * @brief Function to configure the unicast address
 * in device.
 *
 * Algorithm: This function collects all the unicast addresses and updates the
 * device.
 *
 * @param[in] dev: pointer to net_device structure.
 *
 * @note  MAC and PHY need to be initialized.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_prepare_uc_list(struct net_device *dev,
				 struct osi_ioctl *ioctl_data,
				 unsigned int *mac_addr_idx)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	/* last valid MC/MAC DA + 1 should be start of UC addresses */
	unsigned int i = *mac_addr_idx;
	struct netdev_hw_addr *ha;
	int ret = -1;

	if (ioctl_data == NULL) {
		dev_err(pdata->dev, "ioctl_data is NULL\n");
		return ret;
	}

	memset(&ioctl_data->l2_filter, 0x0, sizeof(struct osi_filter));

	if (pdata->l2_filtering_mode == OSI_HASH_FILTER_MODE) {
		dev_err(pdata->dev,
			"HASH FILTERING for uc addresses not Supported in SW\n");
		/* Perfect filtering for multicast */
		ioctl_data->l2_filter.oper_mode = (OSI_OPER_EN_PERFECT |
						   OSI_OPER_DIS_PROMISC |
						   OSI_OPER_DIS_ALLMULTI);
		ioctl_data->cmd = OSI_CMD_L2_FILTER;
		return osi_handle_ioctl(osi_core, ioctl_data);
	} else if (netdev_uc_count(dev) > (pdata->num_mac_addr_regs - i)) {
		/* switch to PROMISCUOUS mode */
		ioctl_data->l2_filter.oper_mode = (OSI_OPER_DIS_PERFECT |
						   OSI_OPER_EN_PROMISC |
						   OSI_OPER_DIS_ALLMULTI);
		dev_dbg(pdata->dev, "enabling Promiscuous mode\n");
		ioctl_data->cmd = OSI_CMD_L2_FILTER;
		return osi_handle_ioctl(osi_core, ioctl_data);
	} else {
		dev_dbg(pdata->dev,
				"select PERFECT FILTERING for uc addresses: uc_count = %d\n",
			netdev_uc_count(dev));

		ioctl_data->l2_filter.oper_mode = (OSI_OPER_EN_PERFECT |
						   OSI_OPER_ADDR_UPDATE |
						   OSI_OPER_DIS_PROMISC |
						   OSI_OPER_DIS_ALLMULTI);
		netdev_for_each_uc_addr(ha, dev) {
			dev_dbg(pdata->dev,
				"uc addr[%d] = %#x:%#x:%#x:%#x:%#x:%#x\n",
				i, ha->addr[0], ha->addr[1], ha->addr[2],
				ha->addr[3], ha->addr[4], ha->addr[5]);
			ioctl_data->l2_filter.index = i;
			memcpy(ioctl_data->l2_filter.mac_address, ha->addr,
			       ETH_ALEN);
			ioctl_data->l2_filter.dma_routing = OSI_ENABLE;
			if (osi_dma->num_dma_chans > 1) {
				ioctl_data->l2_filter.dma_chan =
							  osi_dma->dma_chans[1];
			} else {
				ioctl_data->l2_filter.dma_chan =
							  osi_dma->dma_chans[0];
			}
			ioctl_data->l2_filter.addr_mask = OSI_AMASK_DISABLE;
			ioctl_data->l2_filter.src_dest = OSI_DA_MATCH;

			ioctl_data->cmd = OSI_CMD_L2_FILTER;
			ret = osi_handle_ioctl(pdata->osi_core, ioctl_data);
			if (ret < 0) {
				dev_err(pdata->dev,
					"issue in creating uc list\n");
				*mac_addr_idx = i;
				return ret;
			}

			memcpy(pdata->mac_addr[i].addr, ha->addr, ETH_ALEN);
			pdata->mac_addr[i].dma_chan = ioctl_data->l2_filter.dma_chan;

			if (i == EQOS_MAX_MAC_ADDRESS_FILTER - 1) {
				dev_err(pdata->dev, "Already MAX MAC added\n");
				break;
			}
			i++;
		}
		*mac_addr_idx = i;
	}

	return ret;
}

/**
 * @brief This function is used to set RX mode.
 *
 * Algorithm: Based on Network interface flag, MAC registers are programmed to
 * set mode.
 *
 * @param[in] dev - pointer to net_device structure.
 *
 * @note MAC and PHY need to be initialized.
 */
void ether_set_rx_mode(struct net_device *dev)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	/* store last call last_uc_filter_index in temporary variable */
	struct osi_ioctl ioctl_data = {};
	unsigned int mac_addr_idx = ETHER_MAC_ADDRESS_INDEX + 1U, i;
	int ret = -1;

	memset(&ioctl_data.l2_filter, 0x0, sizeof(struct osi_filter));
	if ((dev->flags & IFF_PROMISC) == IFF_PROMISC) {
		if (pdata->promisc_mode == OSI_ENABLE) {
			ioctl_data.l2_filter.oper_mode =
							(OSI_OPER_DIS_PERFECT |
							 OSI_OPER_EN_PROMISC |
							 OSI_OPER_DIS_ALLMULTI);
			dev_dbg(pdata->dev, "enabling Promiscuous mode\n");
			ioctl_data.cmd = OSI_CMD_L2_FILTER;
			ret = osi_handle_ioctl(osi_core, &ioctl_data);
			if (ret < 0) {
				dev_err(pdata->dev,
					"Setting Promiscuous mode failed\n");
			}
		} else {
			dev_warn(pdata->dev,
				 "Promiscuous mode not supported\n");
		}
		return;
	} else if ((dev->flags & IFF_ALLMULTI) == IFF_ALLMULTI) {
		ioctl_data.l2_filter.oper_mode = (OSI_OPER_EN_ALLMULTI |
						  OSI_OPER_DIS_PERFECT |
						  OSI_OPER_DIS_PROMISC);
		dev_dbg(pdata->dev, "pass all multicast pkt\n");
		ioctl_data.cmd = OSI_CMD_L2_FILTER;
		ret = osi_handle_ioctl(osi_core, &ioctl_data);
		if (ret < 0) {
			dev_err(pdata->dev, "Setting All Multicast allow mode failed\n");
		}
		return;
	} else if (!netdev_mc_empty(dev)) {
		if (ether_prepare_mc_list(dev, &ioctl_data, &mac_addr_idx) != 0) {
			dev_err(pdata->dev, "Setting MC address failed\n");
		}
	} else {
		/* start index after MAC and BC address index */
		pdata->last_filter_index = ETHER_MAC_ADDRESS_INDEX;
	}

	if (!netdev_uc_empty(dev)) {
		if (ether_prepare_uc_list(dev, &ioctl_data, &mac_addr_idx) != 0) {
			dev_err(pdata->dev, "Setting UC address failed\n");
		}
	}

	if (pdata->last_filter_index > mac_addr_idx) {
		for (i = mac_addr_idx; i < pdata->last_filter_index; i++) {
			/* Reset the filter structure to avoid any old value */
			memset(&ioctl_data.l2_filter, 0x0, sizeof(struct osi_filter));
			ioctl_data.l2_filter.oper_mode = OSI_OPER_ADDR_DEL;
			ioctl_data.l2_filter.index = i;
			ioctl_data.l2_filter.dma_routing = OSI_ENABLE;
			memcpy(ioctl_data.l2_filter.mac_address,
			       pdata->mac_addr[i].addr, ETH_ALEN);
			ioctl_data.l2_filter.dma_chan = pdata->mac_addr[i].dma_chan;
			ioctl_data.l2_filter.addr_mask = OSI_AMASK_DISABLE;
			ioctl_data.l2_filter.src_dest = OSI_DA_MATCH;
			ioctl_data.cmd = OSI_CMD_L2_FILTER;

			ret = osi_handle_ioctl(osi_core, &ioctl_data);
			if (ret < 0) {
				dev_err(pdata->dev,
					"failed to delete L2 filter index = %d\n", i);
				return;
			}
		}
	}

	pdata->last_filter_index = mac_addr_idx;
	/* Set default MAC configuration because if this path is called
	 * only when flag for promiscuous or all_multi is not set.
	 */
	memset(&ioctl_data.l2_filter, 0x0, sizeof(struct osi_filter));
	ioctl_data.l2_filter.oper_mode = (OSI_OPER_EN_PERFECT |
					  OSI_OPER_DIS_PROMISC |
					  OSI_OPER_DIS_ALLMULTI);
	ioctl_data.cmd = OSI_CMD_L2_FILTER;

	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to set operation mode\n");
	}
	return;
}

/**
 * @brief Function to handle PHY read private IOCTL
 *
 * Algorithm: This function is used to write the data
 * into the specified register.
 *
 * @param [in] pdata: Pointer to private data structure.
 * @param [in] ifr: Interface request structure used for socket ioctl
 *
 * @retval 0 on success.
 * @retval "negative value" on failure.
 */

static int ether_handle_priv_rmdio_ioctl(struct ether_priv_data *pdata,
					 struct ifreq *ifr)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 9, 0))
	struct mii_ioctl_data *mii_data = if_mii(ifr);
	unsigned int prtad, devad;
	int ret = 0;

	if (mdio_phy_id_is_c45(mii_data->phy_id)) {
		prtad = mdio_phy_id_prtad(mii_data->phy_id);
		devad = mdio_phy_id_devad(mii_data->phy_id);
		devad = mdiobus_c45_addr(devad, mii_data->reg_num);
	} else {
		prtad = mii_data->phy_id;
		devad = mii_data->reg_num;
	}

	dev_dbg(pdata->dev, "%s: phy_id:%d regadd: %d devaddr:%d\n",
		__func__, mii_data->phy_id, prtad, devad);

	ret = osi_read_phy_reg(pdata->osi_core, prtad, devad);
	if (ret < 0) {
		dev_err(pdata->dev, "%s: Data read failed\n", __func__);
		return -EFAULT;
	}

	mii_data->val_out = ret;

	return 0;
#else
	dev_err(pdata->dev, "Not supported for kernel versions less than 5.10");
	return -ENOTSUPP;
#endif
}

/**
 * @brief Function to handle PHY write private IOCTL
 *
 * Algorithm: This function is used to write the data
 * into the specified register.
 *
 * @param [in] pdata: Pointer to private data structure.
 * @param [in] ifr: Interface request structure used for socket ioctl
 *
 * @retval 0 on success.
 * @retval "negative value" on failure.
 */
static int ether_handle_priv_wmdio_ioctl(struct ether_priv_data *pdata,
					 struct ifreq *ifr)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 9, 0))
	struct mii_ioctl_data *mii_data = if_mii(ifr);
	unsigned int prtad, devad;

	if (mdio_phy_id_is_c45(mii_data->phy_id)) {
		prtad = mdio_phy_id_prtad(mii_data->phy_id);
		devad = mdio_phy_id_devad(mii_data->phy_id);
		devad = mdiobus_c45_addr(devad, mii_data->reg_num);
	} else {
		prtad = mii_data->phy_id;
		devad = mii_data->reg_num;
	}

	dev_dbg(pdata->dev, "%s: phy_id:%d regadd: %d devaddr:%d val:%d\n",
		__func__, mii_data->phy_id, prtad, devad, mii_data->val_in);

	return osi_write_phy_reg(pdata->osi_core, prtad, devad,
				 mii_data->val_in);
#else
	dev_err(pdata->dev, "Not supported for kernel versions less than 5.10");
	return -ENOTSUPP;
#endif
}

/**
 * @brief Network stack IOCTL hook to driver
 *
 * Algorithm:
 * 1) Invokes MII API for phy read/write based on IOCTL command
 * 2) SIOCDEVPRIVATE for private ioctl
 *
 * @param[in] dev: network device structure
 * @param[in] rq: Interface request structure used for socket
 * @param[in] cmd: IOCTL command code
 *
 * @note  Ethernet interface need to be up.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	int ret = -EOPNOTSUPP;
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct mii_ioctl_data *mii_data = if_mii(rq);

	if (!dev || !rq) {
		dev_err(pdata->dev, "%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	if (!netif_running(dev)) {
		dev_err(pdata->dev, "%s: Interface not up\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case SIOCGMIIPHY:
		if (pdata->mdio_addr != FIXED_PHY_INVALID_MDIO_ADDR) {
			mii_data->phy_id = pdata->mdio_addr;
			ret = 0;
		} else {
			if (!dev->phydev) {
				return -EINVAL;
			}
			ret = phy_mii_ioctl(dev->phydev, rq, cmd);
		}
		break;

	case SIOCGMIIREG:
		if (pdata->mdio_addr != FIXED_PHY_INVALID_MDIO_ADDR) {
			ret = ether_handle_priv_rmdio_ioctl(pdata, rq);
		} else {
			if (!dev->phydev) {
				return -EINVAL;
			}
			ret = phy_mii_ioctl(dev->phydev, rq, cmd);
		}
		break;

	case SIOCSMIIREG:
		if (pdata->mdio_addr != FIXED_PHY_INVALID_MDIO_ADDR) {
			ret = ether_handle_priv_wmdio_ioctl(pdata, rq);
		} else {
			if (!dev->phydev) {
				return -EINVAL;
			}
			ret = phy_mii_ioctl(dev->phydev, rq, cmd);
		}
		break;

	case SIOCDEVPRIVATE:
		ret = ether_handle_priv_ioctl(dev, rq);
		break;

	case ETHER_PRV_RMDIO_IOCTL:
		ret = ether_handle_priv_rmdio_ioctl(pdata, rq);
		break;

	case ETHER_PRV_WMDIO_IOCTL:
		ret = ether_handle_priv_wmdio_ioctl(pdata, rq);
		break;

	case ETHER_PRV_TS_IOCTL:
		ret = ether_handle_priv_ts_ioctl(pdata, rq);
		break;

	case SIOCSHWTSTAMP:
		ret = ether_handle_hwtstamp_ioctl(pdata, rq);
		break;

	default:
		netdev_dbg(dev, "%s: Unsupported ioctl %d\n",
			   __func__, cmd);
		break;
	}

	return ret;
}

/**
 * @brief Set MAC address
 *
 * Algorithm:
 * 1) Checks whether given MAC address is valid or not
 * 2) Stores the MAC address in OSI core structure
 *
 * @param[in] ndev: Network device structure
 * @param[in] addr: MAC address to be programmed.
 *
 * @note  Ethernet interface need to be down to set MAC address
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_set_mac_addr(struct net_device *ndev, void *addr)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	int ret = 0;

	ret = eth_mac_addr(ndev, addr);
	if (ret) {
		dev_err(pdata->dev, "failed to set MAC address\n");
		return ret;
	}

	/* MAC address programmed in HW registers before osi_hw_core_init() */
	memcpy(osi_core->mac_addr, ndev->dev_addr, ETH_ALEN);

	return ret;
}

/**
 * @brief Change MAC MTU size
 *
 * Algorithm:
 * 1) Check and return if interface is up.
 * 2) Stores new MTU size set by user in OSI core data structure.
 *
 * @param[in] ndev: Network device structure
 * @param[in] new_mtu: New MTU size to set.
 *
 * @note  Ethernet interface need to be down to change MTU size
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_change_mtu(struct net_device *ndev, int new_mtu)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct osi_ioctl ioctl_data = {};
	int ret = 0;

	if (netif_running(ndev)) {
		netdev_err(pdata->ndev, "must be stopped to change its MTU\n");
		return -EBUSY;
	}

	if ((new_mtu > OSI_MTU_SIZE_9000) &&
	    (osi_dma->num_dma_chans != 1U) &&
	    (osi_core->mac == OSI_MAC_HW_EQOS)) {
		netdev_err(pdata->ndev,
			   "MTU greater than %d is valid only in single channel configuration\n"
			   , OSI_MTU_SIZE_9000);
		return -EINVAL;
	}

	ioctl_data.cmd = OSI_CMD_MAC_MTU;
	ioctl_data.arg1_u32 = new_mtu;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0) {
		dev_info(pdata->dev, "HW Fail to set MTU to %d\n",
			 new_mtu);
		return -EINVAL;
	}

	ndev->mtu = new_mtu;
	osi_core->mtu = new_mtu;
	osi_dma->mtu = new_mtu;

#ifdef MACSEC_SUPPORT
	/* Macsec is not supported or not enabled in DT */
	if (!pdata->macsec_pdata) {
		netdev_info(pdata->ndev, "Macsec not supported or not enabled in DT\n");
	} else if ((osi_core->mac == OSI_MAC_HW_EQOS && osi_core->mac_ver == OSI_EQOS_MAC_5_30) ||
	    (osi_core->mac == OSI_MAC_HW_MGBE && osi_core->mac_ver == OSI_MGBE_MAC_3_10)) {
		/* Macsec is supported, reduce MTU */
		ndev->mtu -= MACSEC_TAG_ICV_LEN;
		netdev_info(pdata->ndev, "Macsec: Reduced MTU: %d Max: %d\n",
			    ndev->mtu, ndev->max_mtu);
	}
#endif /*  MACSEC_SUPPORT */

	netdev_update_features(ndev);

	return 0;
}

/**
 * @brief Change HW features for the given network device.
 *
 * Algorithm:
 * 1) Check if HW supports feature requested to be changed
 * 2) If supported, check the current status of the feature and if it
 * needs to be toggled, do so.
 *
 * @param[in] ndev: Network device structure
 * @param[in] feat: New features to be updated
 *
 * @note  Ethernet interface needs to be up. Stack will enforce
 * the check.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_set_features(struct net_device *ndev, netdev_features_t feat)
{
	int ret = 0;
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	netdev_features_t hw_feat_cur_state = pdata->hw_feat_cur_state;
	struct osi_ioctl ioctl_data = {};

	if (pdata->hw_feat.rx_coe_sel == 0U) {
		return ret;
	}

	if ((feat & NETIF_F_RXCSUM) == NETIF_F_RXCSUM) {
		if (!(hw_feat_cur_state & NETIF_F_RXCSUM)) {
			ioctl_data.cmd = OSI_CMD_RXCSUM_OFFLOAD;
			ioctl_data.arg1_u32 = OSI_ENABLE;
			ret = osi_handle_ioctl(osi_core, &ioctl_data);
			dev_info(pdata->dev, "Rx Csum offload: Enable: %s\n",
				 ret ? "Failed" : "Success");
			pdata->hw_feat_cur_state |= NETIF_F_RXCSUM;
		}
	} else {
		if ((hw_feat_cur_state & NETIF_F_RXCSUM)) {
			ioctl_data.cmd = OSI_CMD_RXCSUM_OFFLOAD;
			ioctl_data.arg1_u32 = OSI_DISABLE;
			ret = osi_handle_ioctl(osi_core, &ioctl_data);
			dev_info(pdata->dev, "Rx Csum offload: Disable: %s\n",
				 ret ? "Failed" : "Success");
			pdata->hw_feat_cur_state &= ~NETIF_F_RXCSUM;
		}
	}

	return ret;
}

#ifdef ETHER_VLAN_VID_SUPPORT
/**
 * @brief Adds VLAN ID. This function is invoked by upper
 * layer when a new VLAN id is registered. This function updates the HW
 * filter with new VLAN id. New vlan id can be added with vconfig -
 * vconfig add interface_name  vlan_id
 *
 * Algorithm:
 * 1) Check for hash or perfect filtering.
 * 2) invoke osi call accordingly.
 *
 * @param[in] ndev: Network device structure
 * @param[in] vlan_proto: VLAN proto VLAN_PROTO_8021Q = 0 VLAN_PROTO_8021AD = 1
 * @param[in] vid: VLAN ID.
 *
 * @note Ethernet interface should be up
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_vlan_rx_add_vid(struct net_device *ndev, __be16 vlan_proto,
				 u16 vid)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	unsigned int vlan_id = (vid | OSI_VLAN_ACTION_ADD);
	struct osi_ioctl ioctl_data = {};
	int ret = -1;

	if (!netif_running(ndev)) {
		return 0;
	}

	if (pdata->vlan_hash_filtering == OSI_HASH_FILTER_MODE) {
		dev_err(pdata->dev,
			"HASH FILTERING for VLAN tag is not supported in SW\n");
	} else {
		ioctl_data.cmd = OSI_CMD_UPDATE_VLAN_ID;
		ioctl_data.arg1_u32 = vlan_id;
		ret = osi_handle_ioctl(osi_core, &ioctl_data);
	}

	return ret;
}

/**
 * @brief Removes VLAN ID. This function is invoked by
 * upper layer when a new VALN id is removed. This function updates the
 * HW filter. vlan id can be removed with vconfig -
 * vconfig rem interface_name vlan_id
 *
 * Algorithm:
 * 1) Check for hash or perfect filtering.
 * 2) invoke osi call accordingly.
 *
 * @param[in] ndev: Network device structure
 * @param[in] vlan_proto: VLAN proto VLAN_PROTO_8021Q = 0 VLAN_PROTO_8021AD = 1
 * @param[in] vid: VLAN ID.
 *
 * @note Ethernet interface should be up
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_vlan_rx_kill_vid(struct net_device *ndev, __be16 vlan_proto,
				  u16 vid)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	unsigned int vlan_id = (vid | OSI_VLAN_ACTION_DEL);
	struct osi_ioctl ioctl_data = {};
	int ret = -1;

	if (!netif_running(ndev)) {
		return 0;
	}

	if (pdata->vlan_hash_filtering == OSI_HASH_FILTER_MODE) {
		dev_err(pdata->dev,
			"HASH FILTERING for VLAN tag is not supported in SW\n");
	} else {
		ioctl_data.cmd = OSI_CMD_UPDATE_VLAN_ID;
		ioctl_data.arg1_u32 = vlan_id;
		ret = osi_handle_ioctl(osi_core, &ioctl_data);
	}

	return ret;
}
#endif /* ETHER_VLAN_VID_SUPPORT */

#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
/**
 * @brief ether_setup_tc - TC HW offload support
 *
 * Algorithm:
 * 1) Check the TC setup type
 * 2) Call appropriate function based on type.
 *
 * @param[in] ndev: Network device structure
 * @param[in] type: qdisc type
 * @param[in] type_data: void pointer having user passed configuration
 *
 * @note Ethernet interface should be up
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_setup_tc(struct net_device *ndev, enum tc_setup_type type,
			  void *type_data)
{
	struct ether_priv_data *pdata = netdev_priv(ndev);

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return -EOPNOTSUPP;
	}

	switch (type) {
	case TC_SETUP_QDISC_TAPRIO:
		return ether_tc_setup_taprio(pdata, type_data);
	case TC_SETUP_QDISC_CBS:
		return ether_tc_setup_cbs(pdata, type_data);
	default:
		return -EOPNOTSUPP;
	}
}
#endif

/**
 * @brief Ethernet network device operations
 */
static const struct net_device_ops ether_netdev_ops = {
	.ndo_open = ether_open,
	.ndo_stop = ether_close,
	.ndo_start_xmit = ether_start_xmit,
	.ndo_do_ioctl = ether_ioctl,
	.ndo_set_mac_address = ether_set_mac_addr,
	.ndo_change_mtu = ether_change_mtu,
	.ndo_select_queue = ether_select_queue,
	.ndo_set_features = ether_set_features,
	.ndo_set_rx_mode = ether_set_rx_mode,
#ifdef ETHER_VLAN_VID_SUPPORT
	.ndo_vlan_rx_add_vid = ether_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid = ether_vlan_rx_kill_vid,
#endif /* ETHER_VLAN_VID_SUPPORT */
#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
	.ndo_setup_tc = ether_setup_tc,
#endif
};

/**
 * @brief NAPI poll handler for receive.
 *
 * Algorithm: Invokes OSI layer to read data from HW and pass onto the
 * Linux network stack.
 *
 * @param[in] napi: NAPI instance for Rx NAPI.
 * @param[in] budget: NAPI budget.
 *
 * @note  Probe and INIT needs to be completed successfully.
 *
 *@return number of packets received.
 */
static int ether_napi_poll_rx(struct napi_struct *napi, int budget)
{
	struct ether_rx_napi *rx_napi =
		container_of(napi, struct ether_rx_napi, napi);
	struct ether_priv_data *pdata = rx_napi->pdata;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	unsigned int chan = rx_napi->chan;
	unsigned int more_data_avail;
	unsigned long flags;
	int received = 0;

	received = osi_process_rx_completions(osi_dma, chan, budget,
					      &more_data_avail);
	if (received < budget) {
		napi_complete(napi);
		raw_spin_lock_irqsave(&pdata->rlock, flags);
		osi_handle_dma_intr(osi_dma, chan,
				    OSI_DMA_CH_RX_INTR,
				    OSI_DMA_INTR_ENABLE);
		raw_spin_unlock_irqrestore(&pdata->rlock, flags);
	}

	return received;
}

/**
 * @brief NAPI poll handler for transmission.
 *
 * Algorithm: Invokes OSI layer to read data from HW and pass onto the
 * Linux network stack.
 *
 * @param[in] napi: NAPI instance for tx NAPI.
 * @param[in] budget: NAPI budget.
 *
 * @note  Probe and INIT needs to be completed successfully.
 *
 * @return Number of Tx buffer cleaned.
 */
static int ether_napi_poll_tx(struct napi_struct *napi, int budget)
{
	struct ether_tx_napi *tx_napi =
		container_of(napi, struct ether_tx_napi, napi);
	struct ether_priv_data *pdata = tx_napi->pdata;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	unsigned int chan = tx_napi->chan;
	unsigned long flags;
	int processed;

	processed = osi_process_tx_completions(osi_dma, chan, budget);

	/* re-arm the timer if tx ring is not empty */
	if (!osi_txring_empty(osi_dma, chan) &&
	    osi_dma->use_tx_usecs == OSI_ENABLE &&
	    atomic_read(&tx_napi->tx_usecs_timer_armed) == OSI_DISABLE) {
		atomic_set(&tx_napi->tx_usecs_timer_armed, OSI_ENABLE);
		hrtimer_start(&tx_napi->tx_usecs_timer,
			      osi_dma->tx_usecs * NSEC_PER_USEC,
			      HRTIMER_MODE_REL);
	}

	if (processed < budget) {
		napi_complete(napi);
		raw_spin_lock_irqsave(&pdata->rlock, flags);
		osi_handle_dma_intr(osi_dma, chan,
				    OSI_DMA_CH_TX_INTR,
				    OSI_DMA_INTR_ENABLE);
		raw_spin_unlock_irqrestore(&pdata->rlock, flags);
	}

	return processed;
}

static enum hrtimer_restart ether_tx_usecs_hrtimer(struct hrtimer *data)
{
	struct ether_tx_napi *tx_napi = container_of(data, struct ether_tx_napi,
						     tx_usecs_timer);
	struct ether_priv_data *pdata = tx_napi->pdata;
	unsigned long val;

	val = pdata->xstats.tx_usecs_swtimer_n[tx_napi->chan];
	pdata->xstats.tx_usecs_swtimer_n[tx_napi->chan] =
		osi_update_stats_counter(val, 1U);

	atomic_set(&pdata->tx_napi[tx_napi->chan]->tx_usecs_timer_armed,
		   OSI_DISABLE);
	if (likely(napi_schedule_prep(&tx_napi->napi)))
		__napi_schedule_irqoff(&tx_napi->napi);

	return HRTIMER_NORESTART;
}

/**
 * @brief Allocate NAPI resources.
 *
 * Algorithm: Allocate NAPI instances for the channels which are enabled.
 *
 * @param[in] pdata: OSD private data structure.
 *
 * @note Number of channels and channel numbers needs to be
 * updated in OSI private data structure.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_alloc_napi(struct ether_priv_data *pdata)
{
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct net_device *ndev = pdata->ndev;
	struct device *dev = pdata->dev;
	unsigned int chan;
	unsigned int i;

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		chan = osi_dma->dma_chans[i];

		pdata->tx_napi[chan] = devm_kzalloc(dev,
						sizeof(struct ether_tx_napi),
						GFP_KERNEL);
		if (pdata->tx_napi[chan] == NULL) {
			dev_err(dev, "failed to allocate Tx NAPI resource\n");
			return -ENOMEM;
		}

		pdata->tx_napi[chan]->pdata = pdata;
		pdata->tx_napi[chan]->chan = chan;
		netif_napi_add(ndev, &pdata->tx_napi[chan]->napi,
			       ether_napi_poll_tx, 64);

		pdata->rx_napi[chan] = devm_kzalloc(dev,
						sizeof(struct ether_rx_napi),
						GFP_KERNEL);
		if (pdata->rx_napi[chan] == NULL) {
			dev_err(dev, "failed to allocate RX NAPI resource\n");
			return -ENOMEM;
		}

		pdata->rx_napi[chan]->pdata = pdata;
		pdata->rx_napi[chan]->chan = chan;
		netif_napi_add(ndev, &pdata->rx_napi[chan]->napi,
			       ether_napi_poll_rx, 64);
	}

	return 0;
}

/**
 * @brief ether_set_vm_irq_chan_mask - Set VM DMA channel mask.
 *
 * Algorithm: Set VM DMA channels specific mask for ISR based on
 * number of DMA channels and list of DMA channels.
 *
 * @param[in] vm_irq_data: VM IRQ data
 * @param[in] num_vm_chan: Number of VM DMA channels
 * @param[in] vm_chans: Pointer to list of VM DMA channels
 *
 * @retval None.
 */
static void ether_set_vm_irq_chan_mask(struct ether_vm_irq_data *vm_irq_data,
				       unsigned int num_vm_chan,
				       unsigned int *vm_chans)
{
	unsigned int i;
	unsigned int chan;

	for (i = 0; i < num_vm_chan; i++) {
		chan = vm_chans[i];
		vm_irq_data->chan_mask |= ETHER_VM_IRQ_TX_CHAN_MASK(chan);
		vm_irq_data->chan_mask |= ETHER_VM_IRQ_RX_CHAN_MASK(chan);
	}
}

/**
 * @brief ether_get_vm_irq_data - Get VM IRQ data from DT.
 *
 * Algorimthm: Parse DT for VM IRQ data and get VM IRQ numbers
 * from DT.
 *
 * @param[in] pdev: Platform device instance.
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure
 */
static int ether_get_vm_irq_data(struct platform_device *pdev,
				 struct ether_priv_data *pdata)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct device_node *vm_node, *temp;
	unsigned int i, j, node = 0;
	int ret = 0;

	vm_node = of_parse_phandle(pdev->dev.of_node,
				   "nvidia,vm-irq-config", 0);
	if (vm_node == NULL) {
		dev_err(pdata->dev, "failed to found VM IRQ configuration\n");
		return -ENOMEM;
	}

	/* parse the number of VM IRQ's */
	ret = of_property_read_u32(vm_node, "nvidia,num-vm-irqs",
				   &osi_core->num_vm_irqs);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to get number of VM IRQ's (%d)\n",
			ret);
		dev_info(&pdev->dev, "Using num_vm_irqs as one\n");
		osi_core->num_vm_irqs = 1;
	}

	if (osi_core->num_vm_irqs > OSI_MAX_VM_IRQS) {
		dev_err(&pdev->dev, "Invalid Num. of VM IRQS\n");
		return -EINVAL;
	}

	pdata->vm_irq_data = devm_kzalloc(pdata->dev,
					  sizeof(struct ether_vm_irq_data) *
					  osi_core->num_vm_irqs,
					  GFP_KERNEL);
	if (pdata->vm_irq_data == NULL) {
		dev_err(&pdev->dev, "failed to allocate VM IRQ data\n");
		return -ENOMEM;
	}

	ret = of_get_child_count(vm_node);
	if (ret != osi_core->num_vm_irqs) {
		dev_err(&pdev->dev,
			"Mismatch in num_vm_irqs and VM IRQ config DT nodes\n");
		return -EINVAL;
	}

	for_each_child_of_node(vm_node, temp) {
		if (node == osi_core->num_vm_irqs)
			break;

		ret = of_property_read_u32(temp, "nvidia,num-vm-channels",
					&osi_core->irq_data[node].num_vm_chans);
		if (ret != 0) {
			dev_err(&pdev->dev,
				"failed to read number of VM channels\n");
			return ret;
		}

		ret = of_property_read_u32_array(temp, "nvidia,vm-channels",
					osi_core->irq_data[node].vm_chans,
					osi_core->irq_data[node].num_vm_chans);
		if (ret != 0) {
			dev_err(&pdev->dev, "failed to get VM channels\n");
			return ret;
		}

		ret = of_property_read_u32(temp, "nvidia,vm-num",
					   &osi_core->irq_data[node].vm_num);
		if (ret != 0) {
			dev_err(&pdev->dev, "failed to read VM Number\n");
			return ret;
		}

		ether_set_vm_irq_chan_mask(&pdata->vm_irq_data[node],
					   osi_core->irq_data[node].num_vm_chans,
					   osi_core->irq_data[node].vm_chans);

		pdata->vm_irq_data[node].pdata = pdata;

		node++;
	}

	for (i = 0, j = 1; i < osi_core->num_vm_irqs; i++, j++) {
		pdata->vm_irqs[i] = platform_get_irq(pdev, j);
		if (pdata->vm_irqs[i] < 0) {
			dev_err(&pdev->dev, "failed to get VM IRQ number\n");
			return pdata->vm_irqs[i];
		}
	}

	return ret;
}

/**
 * @brief Read IRQ numbers from DT.
 *
 * Algorithm: Reads the IRQ numbers from DT based on number of channels.
 *
 * @param[in] pdev: Platform device associated with driver.
 * @param[in] pdata: OSD private data.
 * @param[in] num_chans: Number of channels.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_get_irqs(struct platform_device *pdev,
			  struct ether_priv_data *pdata,
			  unsigned int num_chans)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	unsigned int i, j;
	int ret = -1;

	/* get common IRQ*/
	pdata->common_irq = platform_get_irq(pdev, 0);
	if (pdata->common_irq < 0) {
		dev_err(&pdev->dev, "failed to get common IRQ number\n");
		return pdata->common_irq;
	}
	if (osi_core->mac_ver > OSI_EQOS_MAC_5_00 ||
	    (osi_core->mac == OSI_MAC_HW_MGBE)) {
		ret = ether_get_vm_irq_data(pdev, pdata);
		if (ret < 0) {
			dev_err(pdata->dev, "failed to get VM IRQ info\n");
			return ret;
		}
	} else {
		/* get TX IRQ numbers */
		for (i = 0, j = 1; i < num_chans; i++) {
			pdata->tx_irqs[i] = platform_get_irq(pdev, j++);
			if (pdata->tx_irqs[i] < 0) {
				dev_err(&pdev->dev, "failed to get TX IRQ number\n");
				return pdata->tx_irqs[i];
			}
		}

		for (i = 0; i < num_chans; i++) {
			pdata->rx_irqs[i] = platform_get_irq(pdev, j++);
			if (pdata->rx_irqs[i] < 0) {
				dev_err(&pdev->dev, "failed to get RX IRQ number\n");
				return pdata->rx_irqs[i];
			}
		}
	}

	return 0;
}

/**
 * @brief Get MAC address from DT
 *
 * Algorithm: Populates MAC address by reading DT node.
 *
 * @param[in] node_name: Device tree node name.
 * @param[in] property_name: DT property name inside DT node.
 * @param[in] mac_addr: MAC address.
 *
 * @note Bootloader needs to updates chosen DT node with MAC
 * address.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_get_mac_address_dtb(const char *node_name,
				     const char *property_name,
				     unsigned char *mac_addr)
{
	struct device_node *np = of_find_node_by_path(node_name);
	const char *mac_str = NULL;
	int values[6] = {0};
	unsigned char mac_temp[6] = {0};
	int i, ret = 0;

	if (np == NULL)
		return -EADDRNOTAVAIL;

	/* If the property is present but contains an invalid value,
	 * then something is wrong. Log the error in that case.
	 */
	if (of_property_read_string(np, property_name, &mac_str)) {
		ret = -EADDRNOTAVAIL;
		goto err_out;
	}

	/* The DTB property is a string of the form xx:xx:xx:xx:xx:xx
	 * Convert to an array of bytes.
	 */
	if (sscanf(mac_str, "%x:%x:%x:%x:%x:%x",
		   &values[0], &values[1], &values[2],
		   &values[3], &values[4], &values[5]) != 6) {
		ret = -EINVAL;
		goto err_out;
	}

	for (i = 0; i < ETH_ALEN; ++i)
		mac_temp[i] = (unsigned char)values[i];

	if (!is_valid_ether_addr(mac_temp)) {
		ret = -EINVAL;
		goto err_out;
	}

	memcpy(mac_addr, mac_temp, ETH_ALEN);

	of_node_put(np);
	return ret;

err_out:
	pr_err("%s: bad mac address at %s/%s: %s.\n",
	       __func__, node_name, property_name,
	       (mac_str != NULL) ? mac_str : "NULL");

	of_node_put(np);

	return ret;
}

/**
 * @brief Get MAC address from DT
 *
 * Algorithm: Populates MAC address by reading DT node.
 *
 * @param[in] pdata: OSD private data.
 *
 * @note Bootloader needs to updates chosen DT node with MAC address.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_get_mac_address(struct ether_priv_data *pdata)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct device *dev = pdata->dev;
	struct net_device *ndev = pdata->ndev;
	struct device_node *np = dev->of_node;
	const char *eth_mac_addr = NULL;
	unsigned char mac_addr[ETH_ALEN] = {0};
	/* Default choesn node property name for MAC address */
	char str_mac_address[ETH_MAC_STR_LEN] = "nvidia,ether-mac";
	unsigned int offset = 0;
	unsigned int mac_addr_idx = 0x0;
	int ret = 0;

	/** For all new Platforms, ethernet DT node must have
	 * "nvidia,mac-addr-idx" property which give MAC address
	 * index of ethernet controller.
	 *
	 * - Algorithm: MAC address index for a functional driver is
	 *   known from platform dts file.
	 *
	 *   For example:
	 *     if there is MGBE controller DT node with index 8 MGBE,
	 *     MAC address is at /chosen/nvidia,ether-mac8
	 */
	if ((pdata->osi_core->mac_ver > OSI_EQOS_MAC_5_10) ||
	    (pdata->osi_core->mac == OSI_MAC_HW_MGBE)) {
		ret = of_property_read_u32(np, "nvidia,mac-addr-idx",
					   &mac_addr_idx);
		if (ret < 0) {
			dev_err(dev, "Ethernet MAC index missing\n");
			/* TODO Must return error if index is not
			 * present in ethernet dt node
			 * which is having status "okay".
			 */
		}

		offset = mac_addr_idx;
		sprintf(str_mac_address, "nvidia,ether-mac%d", offset);
	}

	ret = ether_get_mac_address_dtb("/chosen", str_mac_address, mac_addr);
	/* If return value is valid update eth_mac_addr */
	if (ret == 0) {
		eth_mac_addr = mac_addr;
	}

	/* if chosen nodes are not present for platform */
	if (IS_ERR_OR_NULL(eth_mac_addr)) {
		/* Read MAC address using default ethernet property
		 * upstream driver should have only this call to get
		 * MAC address
		 */
		eth_mac_addr = of_get_mac_address(np);

		if (IS_ERR_OR_NULL(eth_mac_addr)) {
			dev_err(dev, "No MAC address in local DT!\n");
			return -EINVAL;
		}
	}

	/* If neither chosen node nor kernel supported dt strings are
	 * present in platform device tree.
	 */
	if (!(is_valid_ether_addr(eth_mac_addr)) ||
	    IS_ERR_OR_NULL(eth_mac_addr)) {
		dev_err(dev, "Bad mac address exiting\n");
		return -EINVAL;
	}

	/* Found a valid mac address */
	memcpy(ndev->dev_addr, eth_mac_addr, ETH_ALEN);
	memcpy(osi_core->mac_addr, eth_mac_addr, ETH_ALEN);

	dev_info(dev, "Ethernet MAC address: %pM\n", ndev->dev_addr);

	return ret;
}

/**
 * @brief Put back MAC MGBE related clocks.
 *
 * Algorithm: Put back or release the MAC related clocks.
 *
 * @param[in] pdata: OSD private data.
 */
static void ether_put_mgbe_clks(struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;

	if (!IS_ERR_OR_NULL(pdata->ptp_ref_clk)) {
		devm_clk_put(dev, pdata->ptp_ref_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->app_clk)) {
		devm_clk_put(dev, pdata->app_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->eee_pcs_clk)) {
		devm_clk_put(dev, pdata->eee_pcs_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->mac_clk)) {
		devm_clk_put(dev, pdata->mac_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->mac_div_clk)) {
		devm_clk_put(dev, pdata->mac_div_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->tx_pcs_clk)) {
		devm_clk_put(dev, pdata->tx_pcs_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->tx_clk)) {
		devm_clk_put(dev, pdata->tx_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_pcs_clk)) {
		devm_clk_put(dev, pdata->rx_pcs_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_pcs_input_clk)) {
		devm_clk_put(dev, pdata->rx_pcs_input_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_pcs_m_clk)) {
		devm_clk_put(dev, pdata->rx_pcs_m_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_m_clk)) {
		devm_clk_put(dev, pdata->rx_m_clk);
	}
}

/**
 * @brief Put back MAC EQOS related clocks.
 *
 * Algorithm: Put back or release the MAC related clocks.
 *
 * @param[in] pdata: OSD private data.
 */
static void ether_put_eqos_clks(struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;

	if (!IS_ERR_OR_NULL(pdata->tx_clk)) {
		devm_clk_put(dev, pdata->tx_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->tx_div_clk)) {
		devm_clk_put(dev, pdata->tx_div_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_m_clk)) {
		devm_clk_put(dev, pdata->rx_m_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_input_clk)) {
		devm_clk_put(dev, pdata->rx_input_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->ptp_ref_clk)) {
		devm_clk_put(dev, pdata->ptp_ref_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->rx_clk)) {
		devm_clk_put(dev, pdata->rx_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->axi_clk)) {
		devm_clk_put(dev, pdata->axi_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->axi_cbb_clk)) {
		devm_clk_put(dev, pdata->axi_cbb_clk);
	}

	if (!IS_ERR_OR_NULL(pdata->pllrefe_clk)) {
		devm_clk_put(dev, pdata->pllrefe_clk);
	}
}

/**
 * @brief Put back MAC related clocks.
 *
 * Algorithm: Put back or release the MAC related clocks.
 *
 * @param[in] pdata: OSD private data.
 */
static inline void ether_put_clks(struct ether_priv_data *pdata)
{
	if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
		ether_put_mgbe_clks(pdata);
	} else {
		ether_put_eqos_clks(pdata);
	}
}

/**
 * @brief Set clk rates for mgbe#_rx_input/mgbe#_rx_pcs_input
 *
 * Algorithm: Sets clk rates based on UPHY GBE mode for
 * mgbe#_rx_input/mgbe#_rx_pcs_input clk ID's.
 *
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_set_mgbe_rx_fmon_rates(struct ether_priv_data *pdata)
{
	unsigned int uphy_gbe_mode = pdata->osi_core->uphy_gbe_mode;
	unsigned long rx_rate, rx_pcs_rate;
	int ret;

	if (uphy_gbe_mode == OSI_ENABLE) {
		rx_rate = ETHER_MGBE_RX_CLK_USXGMII_10G;
		rx_pcs_rate = ETHER_MGBE_RX_PCS_CLK_USXGMII_10G;
	} else {
		rx_rate = ETHER_MGBE_RX_CLK_USXGMII_5G;
		rx_pcs_rate = ETHER_MGBE_RX_PCS_CLK_USXGMII_5G;
	}

	ret = clk_set_rate(pdata->rx_input_clk, rx_rate);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to set rx_input_clk rate\n");
		return ret;
	}

	ret = clk_set_rate(pdata->rx_pcs_input_clk, rx_pcs_rate);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to set rx_pcs_input_clk rate\n");
		return ret;
	}

	return 0;
}

/**
 * @brief Get MAC MGBE related clocks.
 *
 * Algorithm: Get the clocks from DT and stores in OSD private data.
 *
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_get_mgbe_clks(struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;
	int ret;

	pdata->rx_m_clk = devm_clk_get(dev, "rx-input-m");
	if (IS_ERR(pdata->rx_m_clk)) {
		ret = PTR_ERR(pdata->rx_m_clk);
		dev_err(dev, "failed to get rx-input-m\n");
		goto err_rx_m;
	}

	pdata->rx_pcs_m_clk = devm_clk_get(dev, "rx-pcs-m");
	if (IS_ERR(pdata->rx_pcs_m_clk)) {
		ret = PTR_ERR(pdata->rx_pcs_m_clk);
		dev_err(dev, "failed to get rx-pcs-m clk\n");
		goto err_rx_pcs_m;
	}

	pdata->rx_pcs_input_clk = devm_clk_get(dev, "rx-pcs-input");
	if (IS_ERR(pdata->rx_pcs_input_clk)) {
		ret = PTR_ERR(pdata->rx_pcs_input_clk);
		dev_err(dev, "failed to get rx-pcs-input clk\n");
		goto err_rx_pcs_input;
	}

	pdata->rx_pcs_clk = devm_clk_get(dev, "rx-pcs");
	if (IS_ERR(pdata->rx_pcs_clk)) {
		ret = PTR_ERR(pdata->rx_pcs_clk);
		dev_err(dev, "failed to get rx-pcs clk\n");
		goto err_rx_pcs;
	}

	pdata->tx_clk = devm_clk_get(dev, "tx");
	if (IS_ERR(pdata->tx_clk)) {
		ret = PTR_ERR(pdata->tx_clk);
		dev_err(dev, "failed to get tx clk\n");
		goto err_tx;
	}

	pdata->tx_pcs_clk = devm_clk_get(dev, "tx-pcs");
	if (IS_ERR(pdata->tx_pcs_clk)) {
		ret = PTR_ERR(pdata->tx_pcs_clk);
		dev_err(dev, "failed to get tx-pcs clk\n");
		goto err_tx_pcs;
	}

	pdata->mac_div_clk = devm_clk_get(dev, "mac-divider");
	if (IS_ERR(pdata->mac_div_clk)) {
		ret = PTR_ERR(pdata->mac_div_clk);
		dev_err(dev, "failed to get mac-divider clk\n");
		goto err_mac_div;
	}

	pdata->mac_clk = devm_clk_get(dev, "mac");
	if (IS_ERR(pdata->mac_clk)) {
		ret = PTR_ERR(pdata->mac_clk);
		dev_err(dev, "failed to get mac clk\n");
		goto err_mac;
	}

	pdata->eee_pcs_clk = devm_clk_get(dev, "eee-pcs");
	if (IS_ERR(pdata->eee_pcs_clk)) {
		ret = PTR_ERR(pdata->eee_pcs_clk);
		dev_err(dev, "failed to get eee-pcs clk\n");
		goto err_eee_pcs;
	}

	pdata->app_clk = devm_clk_get(dev, "mgbe");
	if (IS_ERR(pdata->app_clk)) {
		ret = PTR_ERR(pdata->app_clk);
		dev_err(dev, "failed to get mgbe clk\n");
		goto err_app;
	}

	pdata->ptp_ref_clk = devm_clk_get(dev, "ptp-ref");
	if (IS_ERR(pdata->ptp_ref_clk)) {
		ret = PTR_ERR(pdata->ptp_ref_clk);
		dev_err(dev, "failed to get ptp-ref clk\n");
		goto err_ptp_ref;
	}

	pdata->rx_input_clk = devm_clk_get(dev, "rx-input");
	if (IS_ERR(pdata->rx_input_clk)) {
		ret = PTR_ERR(pdata->rx_input_clk);
		dev_err(dev, "failed to get rx-input clk\n");
		goto err_rx_input;
	}

	ret = ether_set_mgbe_rx_fmon_rates(pdata);
	if (ret < 0)
		goto err_rx_input;

	return 0;

err_rx_input:
	devm_clk_put(dev, pdata->ptp_ref_clk);
err_ptp_ref:
	devm_clk_put(dev, pdata->app_clk);
err_app:
	devm_clk_put(dev, pdata->eee_pcs_clk);
err_eee_pcs:
	devm_clk_put(dev, pdata->mac_clk);
err_mac:
	devm_clk_put(dev, pdata->mac_div_clk);
err_mac_div:
	devm_clk_put(dev, pdata->tx_pcs_clk);
err_tx_pcs:
	devm_clk_put(dev, pdata->tx_clk);
err_tx:
	devm_clk_put(dev, pdata->rx_pcs_clk);
err_rx_pcs:
	devm_clk_put(dev, pdata->rx_pcs_input_clk);
err_rx_pcs_input:
	devm_clk_put(dev, pdata->rx_pcs_m_clk);
err_rx_pcs_m:
	devm_clk_put(dev, pdata->rx_m_clk);
err_rx_m:
	return ret;
}

/**
 * @brief Get EQOS MAC related clocks.
 *
 * Algorithm: Get the clocks from DT and stores in OSD private data.
 *
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_get_eqos_clks(struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;
	int ret;

	/* Skip pll_refe clock initialisation for t18x platform */
	pdata->pllrefe_clk = devm_clk_get(dev, "pllrefe_vcoout");
	if (IS_ERR(pdata->pllrefe_clk)) {
		dev_info(dev, "failed to get pllrefe_vcoout clk\n");
	}

	pdata->axi_cbb_clk = devm_clk_get(dev, "axi_cbb");
	if (IS_ERR(pdata->axi_cbb_clk)) {
		ret = PTR_ERR(pdata->axi_cbb_clk);
		dev_err(dev, "failed to get axi_cbb clk\n");
		goto err_axi_cbb;
	}

	pdata->axi_clk = devm_clk_get(dev, "eqos_axi");
	if (IS_ERR(pdata->axi_clk)) {
		ret = PTR_ERR(pdata->axi_clk);
		dev_err(dev, "failed to get eqos_axi clk\n");
		goto err_axi;
	}

	pdata->rx_clk = devm_clk_get(dev, "eqos_rx");
	if (IS_ERR(pdata->rx_clk)) {
		ret = PTR_ERR(pdata->rx_clk);
		dev_err(dev, "failed to get eqos_rx clk\n");
		goto err_rx;
	}

	pdata->ptp_ref_clk = devm_clk_get(dev, "eqos_ptp_ref");
	if (IS_ERR(pdata->ptp_ref_clk)) {
		ret = PTR_ERR(pdata->ptp_ref_clk);
		dev_err(dev, "failed to get eqos_ptp_ref clk\n");
		goto err_ptp_ref;
	}

	pdata->tx_clk = devm_clk_get(dev, "eqos_tx");
	if (IS_ERR(pdata->tx_clk)) {
		ret = PTR_ERR(pdata->tx_clk);
		dev_err(dev, "failed to get eqos_tx clk\n");
		goto err_tx;
	}

	/* This is optional clk */
	pdata->rx_m_clk = devm_clk_get(dev, "eqos_rx_m");
	if (IS_ERR(pdata->rx_m_clk)) {
		dev_info(dev, "failed to get eqos_rx_m clk\n");
	}

	/* This is optional clk */
	pdata->rx_input_clk = devm_clk_get(dev, "eqos_rx_input");
	if (IS_ERR(pdata->rx_input_clk)) {
		dev_info(dev, "failed to get eqos_rx_input clk\n");
	}

	pdata->tx_div_clk = devm_clk_get(dev, "eqos_tx_divider");
	if (IS_ERR(pdata->tx_div_clk)) {
		ret = PTR_ERR(pdata->tx_div_clk);
		dev_info(dev, "failed to get eqos_tx_divider clk\n");
	}

	/* Set default rate to 1G */
	if (!IS_ERR_OR_NULL(pdata->rx_input_clk)) {
		clk_set_rate(pdata->rx_input_clk,
			     ETHER_RX_INPUT_CLK_RATE);
	}

	return 0;

err_tx:
	devm_clk_put(dev, pdata->ptp_ref_clk);
err_ptp_ref:
	devm_clk_put(dev, pdata->rx_clk);
err_rx:
	devm_clk_put(dev, pdata->axi_clk);
err_axi:
	devm_clk_put(dev, pdata->axi_cbb_clk);
err_axi_cbb:
	if (!IS_ERR_OR_NULL(pdata->pllrefe_clk)) {
		devm_clk_put(dev, pdata->pllrefe_clk);
	}

	return ret;
}

/**
 * @brief Get MAC related clocks.
 *
 * Algorithm: Get the clocks from DT and stores in OSD private data.
 *
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_get_clks(struct ether_priv_data *pdata)
{
	if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
		return ether_get_mgbe_clks(pdata);
	}

	return ether_get_eqos_clks(pdata);
}

/**
 * @brief Get Reset and MAC related clocks.
 *
 * Algorithm: Get the resets and MAC related clocks from DT and stores in
 * OSD private data. It also sets MDC clock rate by invoking OSI layer
 * with osi_set_mdc_clk_rate().
 *
 * @param[in] pdev: Platform device.
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_configure_car(struct platform_device *pdev,
			       struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;
	struct device_node *np = dev->of_node;
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	unsigned long csr_clk_rate = 0;
	struct osi_ioctl ioctl_data = {};
	int ret = 0;


	/* get MAC reset */
	if (!pdata->skip_mac_reset) {
		pdata->mac_rst = devm_reset_control_get(&pdev->dev, "mac");
		if (IS_ERR_OR_NULL(pdata->mac_rst)) {
			if (PTR_ERR(pdata->mac_rst) != -EPROBE_DEFER)
				dev_err(&pdev->dev, "failed to get MAC rst\n");
			return PTR_ERR(pdata->mac_rst);
		}
	}

	if (osi_core->mac == OSI_MAC_HW_MGBE) {
		pdata->xpcs_rst = devm_reset_control_get(&pdev->dev,
							 "pcs");
		if (IS_ERR_OR_NULL(pdata->xpcs_rst)) {
			dev_info(&pdev->dev, "failed to get XPCS reset\n");
			return PTR_ERR(pdata->xpcs_rst);
		}
	} else {
		pdata->xpcs_rst = NULL;
	}

	/* get PHY reset */
	pdata->phy_reset = of_get_named_gpio(np, "nvidia,phy-reset-gpio", 0);
	if (pdata->phy_reset < 0) {
		if (pdata->phy_reset == -EPROBE_DEFER)
			return pdata->phy_reset;
		else
			dev_info(dev, "failed to get phy reset gpio error: %d\n",
				pdata->phy_reset);
	}

	if (gpio_is_valid(pdata->phy_reset)) {
		ret = devm_gpio_request_one(dev, (unsigned int)pdata->phy_reset,
					    GPIOF_OUT_INIT_HIGH,
					    "phy_reset");
		if (ret < 0) {
			dev_err(dev, "failed to request PHY reset gpio\n");
			goto exit;
		}

		gpio_set_value(pdata->phy_reset, 0);
		usleep_range(pdata->phy_reset_duration,
			     pdata->phy_reset_duration + 1);
		gpio_set_value(pdata->phy_reset, 1);
		msleep(pdata->phy_reset_post_delay);
	}

	ret = ether_get_clks(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get clks\n");
		goto err_get_clks;
	}

	/* set PTP clock rate*/
	ret = clk_set_rate(pdata->ptp_ref_clk, pdata->ptp_ref_clock_speed);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set ptp clk rate\n");
		goto err_set_ptp_rate;
	} else {
		osi_core->ptp_config.ptp_ref_clk_rate = pdata->ptp_ref_clock_speed;
	}

	ret = ether_enable_clks(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable clks\n");
		goto err_enable_clks;
	}

	if (pdata->mac_rst) {
		ret = reset_control_reset(pdata->mac_rst);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to reset MAC HW\n");
			goto err_rst;
		}
	}

	csr_clk_rate = clk_get_rate(pdata->axi_cbb_clk);
	ioctl_data.cmd = OSI_CMD_MDC_CONFIG;
	ioctl_data.arg5_u64 = csr_clk_rate;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to configure MDC\n");
		goto err_mdc;
	}

	return ret;
err_mdc:
	if (pdata->mac_rst) {
		reset_control_assert(pdata->mac_rst);
	}
err_rst:
	ether_disable_clks(pdata);
err_enable_clks:
err_set_ptp_rate:
	ether_put_clks(pdata);
err_get_clks:
	if (gpio_is_valid(pdata->phy_reset)) {
		gpio_set_value(pdata->phy_reset, OSI_DISABLE);
	}
exit:
	return ret;
}

/**
 * @brief Get platform resources
 *
 * Algorithm: Populates base address, clks, reset and MAC address.
 *
 * @param[in] pdev: Platform device associated with platform driver.
 * @param[in] pdata: OSD private data.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_init_plat_resources(struct platform_device *pdev,
				     struct ether_priv_data *pdata)
{
	bool tegra_hypervisor_mode = is_tegra_hypervisor_mode();
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct resource *res;
	int ret = 0;

	/* get base address and remap */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mac");
	osi_core->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(osi_core->base)) {
		dev_err(&pdev->dev, "failed to ioremap MAC base address\n");
		return PTR_ERR(osi_core->base);
	}

	if (!tegra_hypervisor_mode) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hypervisor");
		if (res) {
			osi_core->hv_base = devm_ioremap_resource(&pdev->dev,
								  res);
			if (IS_ERR(osi_core->hv_base)) {
				dev_err(&pdev->dev,
					"failed to ioremap HV address\n");
				return PTR_ERR(osi_core->hv_base);
			}
		} else {
			osi_core->hv_base = NULL;
			dev_dbg(&pdev->dev, "HV base address is not present\n");
		}
	} else {
		osi_core->hv_base = NULL;
		dev_dbg(&pdev->dev, "Hypervisor mode enabled\n");
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "dma_base");
	if (res) {
		/* Update dma base */
		osi_dma->base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(osi_dma->base)) {
			dev_err(&pdev->dev, "failed to ioremap DMA address\n");
			return PTR_ERR(osi_dma->base);
		}
	} else {
		/* Update core base to dma/common base */
		osi_dma->base = osi_core->base;
	}

	if (osi_core->mac == OSI_MAC_HW_MGBE) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "xpcs");
		if (res) {
			osi_core->xpcs_base = devm_ioremap_resource(&pdev->dev,
								    res);
			if (IS_ERR(osi_core->xpcs_base)) {
				dev_err(&pdev->dev, "failed to ioremap XPCS address\n");
				return PTR_ERR(osi_core->xpcs_base);
			}
		}
	} else {
		osi_core->xpcs_base = NULL;
	}

	if (osi_core->use_virtualization == OSI_DISABLE) {
		ret = ether_configure_car(pdev, pdata);
		if (ret < 0) {
			if (ret != -EPROBE_DEFER)
				dev_err(&pdev->dev, "failed to get clks/reset");
		}
	} else {
		pdata->clks_enable = true;
	}

	return ret;
}

/**
 * @brief Parse PHY DT.
 *
 * Algorithm: Reads PHY DT. Updates required data.
 *
 * @param[in] pdata: OS dependent private data structure.
 * @param[in] node: DT node entry of MAC
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_parse_phy_dt(struct ether_priv_data *pdata,
			      struct device_node *node)
{
	int err;

#if KERNEL_VERSION(5, 5, 0) > LINUX_VERSION_CODE
	pdata->interface = of_get_phy_mode(node);
#else
	err = of_get_phy_mode(node, &pdata->interface);
	if (err < 0)
		pr_debug("%s(): phy interface not found\n", __func__);
#endif

	pdata->phy_node = of_parse_phandle(node, "phy-handle", 0);
	if (pdata->phy_node == NULL)
		pr_debug("%s(): phy handle not found\n", __func__);

	/* If nvidia,eqos-mdio is passed from DT, always register the MDIO */
	for_each_child_of_node(node, pdata->mdio_node) {
		if (of_device_is_compatible(pdata->mdio_node,
					    "nvidia,eqos-mdio"))
			break;
	}

	/* Read PHY delays */
	err = of_property_read_u32(pdata->phy_node, "nvidia,phy-rst-duration-usec",
				   &pdata->phy_reset_duration);
	if (err < 0) {
		pr_debug("failed to read PHY reset duration,setting to default 10usec\n");
		pdata->phy_reset_duration = 10;
	}

	err = of_property_read_u32(pdata->phy_node, "nvidia,phy-rst-pdelay-msec",
				   &pdata->phy_reset_post_delay);
	if (err < 0) {
		pr_debug("failed to read PHY post delay,setting to default 0msec\n");
		pdata->phy_reset_post_delay = 0;
	}

	/* In the case of a fixed PHY, the DT node associated
	 * to the PHY is the Ethernet MAC DT node.
	 */
	if ((pdata->phy_node == NULL) && of_phy_is_fixed_link(node)) {
		if ((of_phy_register_fixed_link(node) < 0))
			return -ENODEV;
		pdata->fixed_link = OSI_ENABLE;
		pdata->phy_node = of_node_get(node);
	}

	return 0;
}

/**
 * @brief ether_parse_residual_queue - Parse RQ DT entry.
 *
 * Algorithm: Reads residual queue form DT. Updates
 * data either by DT values or by default value.
 *
 * @param[in] pdata: OS dependent private data structure.
 * @param[in] pdt_prop: name of property
 * @param[in] pval: structure pointer where value will be filed
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_parse_residual_queue(struct ether_priv_data *pdata,
				      const char *pdt_prop, unsigned int *pval)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct device_node *pnode = pdata->dev->of_node;
	int ret = 0;

	ret = of_property_read_u32(pnode, pdt_prop, pval);
	if ((ret < 0) ||
	    (*pval >= osi_core->num_mtl_queues) ||
	    (*pval == 0U)) {
		dev_err(pdata->dev, "No/incorrect residual queue defined\n");
		/* TODO we should return -EINVAL */
		*pval = 0x2U;
	}

	return 0;
}

/**
 * @brief Parse queue priority DT.
 *
 * Algorithm: Reads queue priority form DT. Updates
 * data either by DT values or by default value.
 *
 * @param[in] pdata: OS dependent private data structure.
 * @param[in] pdt_prop: name of property
 * @param[in] pval: structure pointer where value will be filed
 * @param[in] val_def: default value if DT entry not reset
 * @param[in] val_max: max value supported
 * @param[in] num_entries: number of entries to be read form DT
 *
 * @note All queue priorities should be different from DT.
 */
static void ether_parse_queue_prio(struct ether_priv_data *pdata,
				   const char *pdt_prop,
				   unsigned int *pval, unsigned int val_def,
				   unsigned int val_max,
				   unsigned int num_entries)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct device_node *pnode = pdata->dev->of_node;
	unsigned int i, pmask = 0x0U;
	unsigned int mtlq;
	unsigned int tval[OSI_MGBE_MAX_NUM_QUEUES];
	int ret = 0;

	ret = of_property_read_u32_array(pnode, pdt_prop, pval, num_entries);
	if (ret < 0) {
		dev_info(pdata->dev, "%s(): \"%s\" read failed %d."
			 "Using default\n", __func__, pdt_prop, ret);
		for (i = 0; i < num_entries; i++) {
			pval[i] = val_def;
		}

		return;
	}

	for (i = 0; i < num_entries; i++) {
		tval[i] = pval[i];
	}

	/* If Some priority is already given to queue or priority in DT is
	 * more than MAX priority, assign default priority to queue with
	 * error message
	 */
	for (i = 0; i < num_entries; i++) {
		mtlq = osi_core->mtl_queues[i];
		if ((tval[i] > val_max) || ((pmask & (1U << tval[i])) != 0U)) {
			dev_dbg(pdata->dev, "%s():Wrong or duplicate priority"
				" in DT entry for Q(%d)\n", __func__, mtlq);
			pval[mtlq] = val_def;
			continue;
		}
		pval[mtlq] = tval[i];
		pmask |= 1U << tval[i];
	}
}

static void ether_get_dma_ring_size(struct device *dev,
				    struct osi_dma_priv_data *osi_dma)
{
	unsigned int tx_ring_sz_max[] = {1024, 4096};
	unsigned int rx_ring_sz_max[] = {1024, 16384};
	/* 1K for EQOS and 4K for MGBE */
	unsigned int default_sz[] = {1024, 4096};
	struct device_node *np = dev->of_node;
	int ret = 0;

	/* Ovveride with DT if property is available */
	ret = of_property_read_u32(np, "nvidia,dma_tx_ring_sz",
				   &osi_dma->tx_ring_sz);
	if (ret < 0) {
		dev_info(dev, "Failed to read DMA Tx ring size, using default [%d]\n",
			 default_sz[osi_dma->mac]);
		osi_dma->tx_ring_sz = default_sz[osi_dma->mac];
	}

	if ((osi_dma->tx_ring_sz > tx_ring_sz_max[osi_dma->mac]) ||
	    (!is_power_of_2(osi_dma->tx_ring_sz))) {
		dev_info(dev, "Invalid Tx ring length - %d using default [%d]\n",
			 osi_dma->tx_ring_sz, default_sz[osi_dma->mac]);
		osi_dma->tx_ring_sz = default_sz[osi_dma->mac];
	}

	ret = of_property_read_u32(np, "nvidia,dma_rx_ring_sz",
				   &osi_dma->rx_ring_sz);
	if (ret < 0) {
		dev_info(dev, "Failed to read DMA Rx ring size, using default [%d]\n",
			 default_sz[osi_dma->mac]);
		osi_dma->rx_ring_sz = default_sz[osi_dma->mac];
	}

	if ((osi_dma->rx_ring_sz > rx_ring_sz_max[osi_dma->mac]) ||
	     (!is_power_of_2(osi_dma->rx_ring_sz))) {
		dev_info(dev, "Invalid Rx ring length - %d using default [%d]\n",
			 osi_dma->rx_ring_sz, default_sz[osi_dma->mac]);
		osi_dma->rx_ring_sz = default_sz[osi_dma->mac];
	}
}

/**
 * @brief Parse MAC and PHY DT.
 *
 * Algorithm: Reads MAC and PHY DT. Updates required data.
 *
 * @param[in] pdata: OS dependent private data structure.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_parse_dt(struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	unsigned int tmp_value[OSI_MGBE_MAX_NUM_QUEUES];
	struct device_node *np = dev->of_node;
	int ret = -EINVAL;
	unsigned int i, mtlq, chan, bitmap;
	unsigned int dt_pad_calibration_enable;
	unsigned int dt_pad_auto_cal_pu_offset;
	unsigned int dt_pad_auto_cal_pd_offset;
	/* This variable is for DT entry which should not fail bootup */
	int ret_val = 0;

	/* Read flag to skip MAC reset on platform */
	ret = of_property_read_u32(np, "nvidia,skip_mac_reset",
				   &pdata->skip_mac_reset);
	if (ret != 0) {
		dev_info(dev, "failed to read skip mac reset flag, default 0\n");
		pdata->skip_mac_reset = 0U;
	}
	/* Read MDIO address */
	ret = of_property_read_u32(np, "nvidia,mdio_addr",
				   &pdata->mdio_addr);
	if (ret != 0) {
		dev_info(dev, "failed to read MDIO address\n");
		pdata->mdio_addr = FIXED_PHY_INVALID_MDIO_ADDR;
	}
	/* read ptp clock */
	ret = of_property_read_u32(np, "nvidia,ptp_ref_clock_speed",
				   &pdata->ptp_ref_clock_speed);
	if (ret != 0) {
		dev_err(dev, "setting default PTP clk rate as 312.5MHz\n");
		pdata->ptp_ref_clock_speed = ETHER_DFLT_PTP_CLK;
	}
	/* read promiscuous mode supported or not */
	ret = of_property_read_u32(np, "nvidia,promisc_mode",
				   &pdata->promisc_mode);
	if (ret != 0) {
		dev_info(dev, "setting default promiscuous mode supported\n");
		pdata->promisc_mode = OSI_ENABLE;
	}

	ret = of_property_read_u32(np, "nvidia,common_irq-cpu-id",
				   &pdata->common_isr_cpu_id);
	if (ret < 0) {
		pdata->common_isr_cpu_id = ETHER_COMMON_IRQ_DEFAULT_CPU;
		ret = 0;
	}

	/* any other invalid promiscuous mode DT value */
	if (pdata->promisc_mode != OSI_DISABLE &&
	    pdata->promisc_mode != OSI_ENABLE){
		dev_info(dev, "Invalid promiscuous mode - setting supported\n");
		pdata->promisc_mode = OSI_ENABLE;
	}
	/* Read Pause frame feature support */
	ret = of_property_read_u32(np, "nvidia,pause_frames",
				   &pdata->osi_core->pause_frames);
	if (ret < 0) {
		dev_info(dev, "Failed to read nvida,pause_frames, so"
			 " setting to default support as disable\n");
		pdata->osi_core->pause_frames = OSI_PAUSE_FRAMES_DISABLE;
	}

	/* Check if IOMMU is enabled */
	if (iommu_get_domain_for_dev(&pdev->dev) != NULL) {
		/* Read and set dma-mask from DT only if IOMMU is enabled*/
		ret = of_property_read_u64(np, "dma-mask", &pdata->dma_mask);
	}

	if (ret != 0) {
		dev_info(dev, "setting to default DMA bit mask\n");
		pdata->dma_mask = DMA_MASK_NONE;
	}

	ret = of_property_read_u32_array(np, "nvidia,mtl-queues",
					 osi_core->mtl_queues,
					 osi_core->num_mtl_queues);
	if (ret < 0) {
		dev_err(dev, "failed to read MTL Queue numbers\n");
		if (osi_core->num_mtl_queues == 1) {
			osi_core->mtl_queues[0] = 0;
			dev_info(dev, "setting default MTL queue: 0\n");
		} else {
			goto exit;
		}
	}

	ret = of_property_read_u32_array(np, "nvidia,tc-mapping",
					 osi_core->tc,
					 osi_core->num_mtl_queues);
	for (i = 0; i < osi_core->num_mtl_queues; i++) {
		if (ret < 0) {
			dev_info(dev, "set default TXQ to TC mapping\n");
			osi_core->tc[osi_core->mtl_queues[i]] =
				(osi_core->mtl_queues[i] %
				 OSI_MAX_TC_NUM);
		} else if ((osi_core->tc[osi_core->mtl_queues[i]] >=
		    OSI_MAX_TC_NUM)) {
			dev_err(dev, "Wrong TC %din DT, setting to TC 0\n",
				osi_core->tc[osi_core->mtl_queues[i]]);
			osi_core->tc[osi_core->mtl_queues[i]] = 0U;
		}
	}

	/* Read PTP Rx queue index */
	ret = of_property_read_u32(np, "nvidia,ptp-rx-queue",
				   &osi_core->ptp_config.ptp_rx_queue);
	if (ret != 0) {
		dev_info(dev, "Setting default PTP RX queue\n");
		osi_core->ptp_config.ptp_rx_queue = ETHER_DEFAULT_PTP_QUEUE;
	} else {
		/* Validate PTP Rx queue index */
		for (i = 0; i < osi_core->num_mtl_queues; i++) {
			if (osi_core->mtl_queues[i] ==
					osi_core->ptp_config.ptp_rx_queue)
				break;
		}
		if (i == osi_core->num_mtl_queues) {
			dev_err(dev, "Invalid PTP RX queue in DT:%d\n",
				osi_core->ptp_config.ptp_rx_queue);
			osi_core->ptp_config.ptp_rx_queue =
				ETHER_DEFAULT_PTP_QUEUE;
		}
	}

	ret = of_property_read_u32_array(np, "nvidia,dma-chans",
					 osi_dma->dma_chans,
					 osi_dma->num_dma_chans);
	if (ret < 0) {
		dev_err(dev, "failed to read DMA channel numbers\n");
		if (osi_dma->num_dma_chans == 1) {
			osi_dma->dma_chans[0] = 0;
			dev_info(dev, "setting default DMA channel: 0\n");
		} else {
			goto exit;
		}
	}

	if (osi_dma->num_dma_chans != osi_core->num_mtl_queues) {
		dev_err(dev, "mismatch in numbers of DMA channel and MTL Q\n");
		return -EINVAL;
	}

	/* Allow to set non zero DMA channel for virtualization */
	if (!ether_init_ivc(pdata)) {
		osi_dma->use_virtualization = OSI_ENABLE;
		osi_core->use_virtualization = OSI_ENABLE;
		dev_info(dev, "Virtualization is enabled\n");
	} else {
		ret = -1;
	}

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		if (osi_dma->dma_chans[i] != osi_core->mtl_queues[i]) {
			dev_err(dev,
				"mismatch in DMA channel and MTL Q number at index %d\n",
				i);
			return -EINVAL;
		}
		if (osi_dma->dma_chans[i] == 0) {
			ret = 0;
		}
	}

	if (ret != 0) {
		dev_err(dev, "Q0 Must be enabled for rx path\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "nvidia,rxq_enable_ctrl",
					 tmp_value,
					 osi_core->num_mtl_queues);
	if (ret < 0) {
		dev_err(dev, "failed to read rxq enable ctrl\n");
		return ret;
	} else {
		for (i = 0; i < osi_core->num_mtl_queues; i++) {
			mtlq = osi_core->mtl_queues[i];
			osi_core->rxq_ctrl[mtlq] = tmp_value[i];
		}
	}

	/* Read tx queue priority */
	ether_parse_queue_prio(pdata, "nvidia,tx-queue-prio", pdata->txq_prio,
			       ETHER_QUEUE_PRIO_DEFAULT, ETHER_QUEUE_PRIO_MAX,
			       osi_core->num_mtl_queues);

	/* Read TX slot enable check array DT node */
	ret = of_property_read_u32_array(np, "nvidia,slot_num_check",
					 tmp_value,
					 osi_dma->num_dma_chans);
	if (ret < 0) {
		dev_info(dev,
			 "Failed to read slot_num_check, disabling slot\n");
		for (i = 0; i < osi_dma->num_dma_chans; i++)
			osi_dma->slot_enabled[i] = OSI_DISABLE;
	} else {
		/* Set slot enable flags */
		for (i = 0; i < osi_dma->num_dma_chans; i++) {
			chan = osi_dma->dma_chans[i];
			osi_dma->slot_enabled[chan] = tmp_value[i];
		}

		/* Read TX slot intervals DT node */
		ret = of_property_read_u32_array(np, "nvidia,slot_intvl_vals",
						 tmp_value,
						 osi_dma->num_dma_chans);
		if (ret < 0) {
			for (i = 0; i < osi_dma->num_dma_chans; i++) {
				chan = osi_dma->dma_chans[i];
				osi_dma->slot_interval[chan] =
					OSI_SLOT_INTVL_DEFAULT;
			}
		} else {
			/* Copy slot intervals */
			for (i = 0; i < osi_dma->num_dma_chans; i++) {
				chan = osi_dma->dma_chans[i];
				osi_dma->slot_interval[chan] = tmp_value[i];
			}
		}
	}

	/* Read Rx Queue - User priority mapping for tagged packets */
	ret = of_property_read_u32_array(np, "nvidia,rx-queue-prio",
					 tmp_value,
					 osi_core->num_mtl_queues);
	if (ret < 0) {
		dev_info(dev, "failed to read rx Queue priority mapping, Setting default 0x0\n");
		for (i = 0; i < osi_core->num_mtl_queues; i++) {
			osi_core->rxq_prio[i] = 0x0U;
		}
	} else {
		for (i = 0; i < osi_core->num_mtl_queues; i++) {
			mtlq = osi_core->mtl_queues[i];
			osi_core->rxq_prio[mtlq] = tmp_value[i];
		}
	}

	/* Read DCS enable/disable input, default disable */
	ret = of_property_read_u32(np, "nvidia,dcs-enable", &osi_core->dcs_en);
	if (ret < 0 || osi_core->dcs_en != OSI_ENABLE) {
		osi_core->dcs_en = OSI_DISABLE;
	}

	/* Read XDCS input for DMA channels route */
	ret = of_property_read_u32(np, "nvidia,mc-dmasel",
				   &osi_core->mc_dmasel);
	if (ret < 0) {
		/* Disable Multiple DMA selection on DT read failure */
		osi_core->mc_dmasel = osi_dma->dma_chans[0];
	} else {
		/* Validate MC DMA channel selection flags */
		bitmap = osi_core->mc_dmasel;
		while (bitmap != 0U) {
			chan = __builtin_ctz(bitmap);
			for (i = 0; i < osi_dma->num_dma_chans; i++) {
				if (osi_dma->dma_chans[i] == chan) {
					/* channel is enabled */
					break;
				}
			}
			if (i == osi_dma->num_dma_chans) {
				/* Invalid MC DMA selection */
				dev_err(dev, "Invalid %d MC DMA selection\n", chan);
				osi_core->mc_dmasel = osi_dma->dma_chans[0];
				break;
			}
			bitmap &= ~OSI_BIT(chan);
		}
	}

	/* Read MAX MTU size supported */
	ret = of_property_read_u32(np, "nvidia,max-platform-mtu",
				   &pdata->max_platform_mtu);
	if (ret < 0) {
		dev_info(dev, "max-platform-mtu DT entry missing, setting default %d\n",
			 OSI_DFLT_MTU_SIZE);
		pdata->max_platform_mtu = OSI_DFLT_MTU_SIZE;
	} else {
		if (pdata->max_platform_mtu > OSI_MAX_MTU_SIZE ||
		    pdata->max_platform_mtu < ETH_MIN_MTU) {
			dev_info(dev, "Invalid max-platform-mtu, setting default %d\n",
				 OSI_DFLT_MTU_SIZE);
			pdata->max_platform_mtu = OSI_DFLT_MTU_SIZE;
		}
	}

	ether_get_dma_ring_size(dev, osi_dma);

	/* tx_usecs value to be set */
	ret = of_property_read_u32(np, "nvidia,tx_usecs", &osi_dma->tx_usecs);
	if (ret < 0) {
		osi_dma->use_tx_usecs = OSI_DISABLE;
	} else {
		if (osi_dma->tx_usecs > OSI_MAX_TX_COALESCE_USEC ||
		    osi_dma->tx_usecs < OSI_MIN_TX_COALESCE_USEC) {
			dev_err(dev,
				"invalid tx_riwt, must be inrange %d to %d\n",
				OSI_MIN_TX_COALESCE_USEC,
				OSI_MAX_TX_COALESCE_USEC);
			return -EINVAL;
		}
		osi_dma->use_tx_usecs = OSI_ENABLE;
	}
	/* tx_frames value to be set */
	ret = of_property_read_u32(np, "nvidia,tx_frames",
				   &osi_dma->tx_frames);
	if (ret < 0) {
		osi_dma->use_tx_frames = OSI_DISABLE;
	} else {
		if (osi_dma->tx_frames > ETHER_TX_MAX_FRAME(osi_dma->tx_ring_sz) ||
		    osi_dma->tx_frames < OSI_MIN_TX_COALESCE_FRAMES) {
			dev_err(dev,
				"invalid tx-frames, must be inrange %d to %ld",
				OSI_MIN_TX_COALESCE_FRAMES,
				ETHER_TX_MAX_FRAME(osi_dma->tx_ring_sz));
			return -EINVAL;
		}
		osi_dma->use_tx_frames = OSI_ENABLE;
	}

	if (osi_dma->use_tx_usecs == OSI_DISABLE &&
	    osi_dma->use_tx_frames == OSI_ENABLE) {
		dev_err(dev, "invalid settings : tx_frames must be enabled"
			   " along with tx_usecs in DT\n");
		return -EINVAL;
	}

	/* RIWT value to be set */
	ret = of_property_read_u32(np, "nvidia,rx_riwt", &osi_dma->rx_riwt);
	if (ret < 0) {
		osi_dma->use_riwt = OSI_DISABLE;
	} else {
		if (osi_dma->mac == OSI_MAC_HW_MGBE &&
		    (osi_dma->rx_riwt  > OSI_MAX_RX_COALESCE_USEC ||
		     osi_dma->rx_riwt  < OSI_MGBE_MIN_RX_COALESCE_USEC)) {
			dev_err(dev,
				"invalid rx_riwt, must be inrange %d to %d\n",
				OSI_MGBE_MIN_RX_COALESCE_USEC,
				OSI_MAX_RX_COALESCE_USEC);
			return -EINVAL;
		} else if (osi_dma->mac == OSI_MAC_HW_EQOS &&
			   (osi_dma->rx_riwt  > OSI_MAX_RX_COALESCE_USEC ||
			    osi_dma->rx_riwt  <
			    OSI_EQOS_MIN_RX_COALESCE_USEC)) {
			dev_err(dev,
				"invalid rx_riwt, must be inrange %d to %d\n",
				OSI_EQOS_MIN_RX_COALESCE_USEC,
				OSI_MAX_RX_COALESCE_USEC);
			return -EINVAL;
		}

		osi_dma->use_riwt = OSI_ENABLE;
	}
	/* rx_frames value to be set */
	ret = of_property_read_u32(np, "nvidia,rx_frames",
				   &osi_dma->rx_frames);
	if (ret < 0) {
		osi_dma->use_rx_frames = OSI_DISABLE;
	} else {
		if (osi_dma->rx_frames > osi_dma->rx_ring_sz ||
		    osi_dma->rx_frames < OSI_MIN_RX_COALESCE_FRAMES) {
			dev_err(dev,
				"invalid rx-frames, must be inrange %d to %d",
				OSI_MIN_RX_COALESCE_FRAMES, osi_dma->rx_ring_sz);
			return -EINVAL;
		}
		osi_dma->use_rx_frames = OSI_ENABLE;
	}

	if (osi_dma->use_riwt == OSI_DISABLE &&
	    osi_dma->use_rx_frames == OSI_ENABLE) {
		dev_err(dev, "invalid settings : rx-frames must be enabled"
			   " along with use_riwt in DT\n");
		return -EINVAL;
	}

	if (osi_core->mac == OSI_MAC_HW_MGBE) {
		ret = of_property_read_u32(np, "nvidia,uphy-gbe-mode",
					   &osi_core->uphy_gbe_mode);
		if (ret < 0) {
			dev_info(dev,
				 "failed to read UPHY GBE mode"
				 "- default to 10G\n");
			osi_core->uphy_gbe_mode = OSI_ENABLE;
		}

		if ((osi_core->uphy_gbe_mode != OSI_ENABLE) &&
		    (osi_core->uphy_gbe_mode != OSI_DISABLE)) {
			dev_err(dev, "Invalid UPHY GBE mode"
				"- default to 10G\n");
			osi_core->uphy_gbe_mode = OSI_ENABLE;
		}

		ret = of_property_read_u32(np, "nvidia,phy-iface-mode",
					   &osi_core->phy_iface_mode);
		if (ret < 0) {
			dev_info(dev,
				 "failed to read PHY iface mode"
				 "- default to 10G XFI\n");
			osi_core->phy_iface_mode = OSI_XFI_MODE_10G;
		}

		if ((osi_core->phy_iface_mode != OSI_XFI_MODE_10G) &&
		    (osi_core->phy_iface_mode != OSI_XFI_MODE_5G) &&
		    (osi_core->phy_iface_mode != OSI_USXGMII_MODE_10G) &&
		    (osi_core->phy_iface_mode != OSI_USXGMII_MODE_5G)) {
			dev_err(dev, "Invalid PHY iface mode"
				"- default to 10G\n");
			osi_core->phy_iface_mode = OSI_XFI_MODE_10G;
		}

		/* GBE and XFI/USXGMII must be in same mode */
		if ((osi_core->uphy_gbe_mode == OSI_ENABLE) &&
		    ((osi_core->phy_iface_mode == OSI_XFI_MODE_5G) ||
		    (osi_core->phy_iface_mode == OSI_USXGMII_MODE_5G))) {
			dev_err(dev, "Invalid combination of UPHY 10GBE mode"
				"and XFI/USXGMII 5G mode\n");
			return -EINVAL;
		}

		if ((osi_core->uphy_gbe_mode == OSI_DISABLE) &&
		    ((osi_core->phy_iface_mode == OSI_XFI_MODE_10G) ||
		    (osi_core->phy_iface_mode == OSI_USXGMII_MODE_10G))) {
			dev_err(dev, "Invalid combination of UPHY 5GBE mode"
				"and XFI/USXGMII 10G mode\n");
			return -EINVAL;
		}
	}

	/* Enable VLAN strip by default */
	osi_core->strip_vlan_tag = OSI_ENABLE;

	ret = ether_parse_phy_dt(pdata, np);
	if (ret < 0) {
		dev_err(dev, "failed to parse PHY DT\n");
		goto exit;
	}

	if (osi_core->mac == OSI_MAC_HW_EQOS) {
		/* Read pad calibration enable/disable input, default enable */
		ret = of_property_read_u32(np, "nvidia,pad_calibration",
					   &dt_pad_calibration_enable);
		if (ret < 0) {
			dev_info(dev, "missing nvidia,pad_calibration enabling by default\n");
			osi_core->padctrl.pad_calibration_enable = OSI_ENABLE;
		} else if ((dt_pad_calibration_enable != OSI_ENABLE) &&
			   (dt_pad_calibration_enable != OSI_DISABLE)) {
			dev_info(dev, "Wrong dt pad_calibration: %u, setting by default\n",
				 dt_pad_calibration_enable);
			osi_core->padctrl.pad_calibration_enable = OSI_ENABLE;
		} else {
			osi_core->padctrl.pad_calibration_enable = dt_pad_calibration_enable;
		}

		/* Read pad calibration config reg offset, default 0 */
		ret = of_property_read_u32(np, "nvidia,pad_auto_cal_pu_offset",
					   &dt_pad_auto_cal_pu_offset);
		if (ret < 0) {
			dev_info(dev, "missing nvidia,pad_auto_cal_pu_offset, "
				 "setting default 0\n");
			osi_core->padctrl.pad_auto_cal_pu_offset = 0U;
			ret = 0;
		} else if (dt_pad_auto_cal_pu_offset >
			   OSI_PAD_CAL_CONFIG_PD_PU_OFFSET_MAX) {
			dev_err(dev, "Error: Invalid dt "
				"pad_auto_cal_pu_offset: %u value\n",
				dt_pad_auto_cal_pu_offset);
			ret = -EINVAL;
			goto exit;
		} else {
			osi_core->padctrl.pad_auto_cal_pu_offset =
						 dt_pad_auto_cal_pu_offset;
		}
		ret = of_property_read_u32(np, "nvidia,pad_auto_cal_pd_offset",
					   &dt_pad_auto_cal_pd_offset);
		if (ret < 0) {
			dev_info(dev, "missing nvidia,pad_auto_cal_pd_offset, "
				 "setting default 0\n");
			osi_core->padctrl.pad_auto_cal_pd_offset = 0U;
			ret = 0;
		} else if (dt_pad_auto_cal_pd_offset >
			   OSI_PAD_CAL_CONFIG_PD_PU_OFFSET_MAX) {
			dev_err(dev, "Error: Invalid dt "
				"pad_auto_cal_pu_offset: %u value\n",
				dt_pad_auto_cal_pd_offset);
			ret = -EINVAL;
			goto exit;
		} else {
			osi_core->padctrl.pad_auto_cal_pd_offset =
						dt_pad_auto_cal_pd_offset;
		}

		pdata->pin = devm_pinctrl_get(dev);
		if (IS_ERR(pdata->pin)) {
			dev_err(dev, "DT: missing eqos pinctrl device\n");
			ret = PTR_ERR(pdata->pin);
			goto exit;
		}
		pdata->mii_rx_enable_state = pinctrl_lookup_state(pdata->pin,
							"mii_rx_enable");
		if (IS_ERR(pdata->mii_rx_enable_state)) {
			dev_err(dev, "DT: missing eqos rx pin enabled state\n");
			ret = PTR_ERR(pdata->pin);
			goto exit;
		}
		pdata->mii_rx_disable_state = pinctrl_lookup_state(pdata->pin,
							"mii_rx_disable");
		if (IS_ERR(pdata->mii_rx_disable_state)) {
			dev_err(dev, "DT: missing eqos rx pin disabled state\n");
			ret = PTR_ERR(pdata->pin);
			goto exit;
		}
	}

	/* Set MAC to MAC time sync role */
	ret_val = of_property_read_u32(np, "nvidia,ptp_m2m_role",
				       &osi_core->m2m_role);
	if (ret_val < 0 || osi_core->m2m_role > OSI_PTP_M2M_SECONDARY) {
		osi_core->m2m_role = OSI_PTP_M2M_INACTIVE;
	}

	/* Set PPS output control, 0 - default.
	 * 1 - Binary rollover is 2 Hz, and the digital rollover is 1 Hz.
	 */
	ret_val = of_property_read_u32(np, "nvidia,pps_op_ctrl",
			&osi_core->pps_frq);
	if (ret_val < 0 || osi_core->pps_frq > OSI_ENABLE) {
		osi_core->pps_frq = OSI_DISABLE;
	}

#ifdef HSI_SUPPORT
	ret_val = of_property_read_u32(np, "nvidia,hsi_err_time_threshold",
				       &osi_core->hsi.err_time_threshold);
	if (ret_val < 0 ||
	    osi_core->hsi.err_time_threshold < OSI_HSI_ERR_TIME_THRESHOLD_MIN ||
	    osi_core->hsi.err_time_threshold > OSI_HSI_ERR_TIME_THRESHOLD_MAX)
		osi_core->hsi.err_time_threshold = OSI_HSI_ERR_TIME_THRESHOLD_DEFAULT;

	ret_val = of_property_read_u32(np, "nvidia,hsi_err_count_threshold",
				       &osi_core->hsi.err_count_threshold);
	if (ret_val < 0 || osi_core->hsi.err_count_threshold <= 0)
		osi_core->hsi.err_count_threshold = OSI_HSI_ERR_COUNT_THRESHOLD;
#endif

	/* Only for orin, Read instance id for the interface, default 0 */
	if (osi_core->mac_ver > OSI_EQOS_MAC_5_00 ||
	    osi_core->mac == OSI_MAC_HW_MGBE) {
		ret = of_property_read_u32(np, "nvidia,instance_id", &osi_core->instance_id);
		if (ret != 0) {
			dev_info(dev,
				 "DT instance_id missing, setting default to MGBE0\n");
			osi_core->instance_id = 0;
			ret = 0;
		}
	}

exit:
	return ret;
}

/**
 * @brief Populate number of MTL and DMA channels.
 *
 * Algorithm:
 * 1) Updates MAC HW type based on DT compatible property.
 * 2) Read number of channels from DT.
 * 3) Updates number of channels based on min and max number of channels
 *
 * @param[in] pdev: Platform device
 * @param[in] num_dma_chans: Number of channels
 * @param[in] mac: MAC type based on compatible property
 * @param[in] num_mtl_queues: Number of MTL queues.
 */
static void ether_get_num_dma_chan_mtl_q(struct platform_device *pdev,
					 unsigned int *num_dma_chans,
					 unsigned int *mac,
					 unsigned int *num_mtl_queues)
{
	struct device_node *np = pdev->dev.of_node;
	/* intializing with 1 channel */
	unsigned int max_chans = 1;
	int ret = 0;

	if (of_device_is_compatible(np, "nvidia,nveqos") ||
	    of_device_is_compatible(np, "nvidia,tegra234-eqos")) {
		*mac = OSI_MAC_HW_EQOS;
		max_chans = OSI_EQOS_MAX_NUM_CHANS;
	}

	if (of_device_is_compatible(np, "nvidia,nvmgbe") ||
	    of_device_is_compatible(np, "nvidia,tegra234-mgbe")) {
		*mac = OSI_MAC_HW_MGBE;
		max_chans = OSI_MGBE_MAX_NUM_CHANS;
	}

	/* parse the number of DMA channels */
	ret = of_property_read_u32(np, "nvidia,num-dma-chans", num_dma_chans);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"failed to get number of DMA channels (%d)\n", ret);
		dev_info(&pdev->dev, "Setting number of channels to one\n");
		*num_dma_chans = 1;
	} else if (*num_dma_chans < 1U || *num_dma_chans > max_chans) {
		dev_warn(&pdev->dev, "Invalid num_dma_chans(=%d), setting to 1\n",
			 *num_dma_chans);
		*num_dma_chans = 1;
	} else {
		/* No action required */
	}

	/* parse the number of mtl queues */
	ret = of_property_read_u32(np, "nvidia,num-mtl-queues", num_mtl_queues);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"failed to get number of MTL queueus (%d)\n", ret);
		dev_info(&pdev->dev, "Setting number of queues to one\n");
		*num_mtl_queues = 1;
	} else if (*num_mtl_queues < 1U || *num_mtl_queues > max_chans) {
		dev_warn(&pdev->dev, "Invalid num_mtl_queues(=%d), setting to 1\n",
			 *num_mtl_queues);
		*num_mtl_queues = 1;
	} else {
		/* No action required */
	}
}

/**
 * @brief Set DMA address mask.
 *
 * Algorithm:
 * Based on the addressing capability (address bit length) supported in the HW,
 * the dma mask is set accordingly.
 *
 * @param[in] pdata: OS dependent private data structure.
 *
 * @note MAC_HW_Feature1 register need to read and store the value of ADDR64.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_set_dma_mask(struct ether_priv_data *pdata)
{
	int ret = 0;

	/* Set DMA addressing limitations based on the value read from HW if
	 * dma_mask is not defined in DT
	 */
	if (pdata->dma_mask == DMA_MASK_NONE) {
		switch (pdata->hw_feat.addr_64) {
		case OSI_ADDRESS_32BIT:
			pdata->dma_mask = DMA_BIT_MASK(32);
			break;
		case OSI_ADDRESS_40BIT:
			pdata->dma_mask = DMA_BIT_MASK(40);
			break;
		case OSI_ADDRESS_48BIT:
			pdata->dma_mask = DMA_BIT_MASK(48);
			break;
		default:
			pdata->dma_mask = DMA_BIT_MASK(40);
			break;
		}
	}

	ret = dma_set_mask_and_coherent(pdata->dev, pdata->dma_mask);
	if (ret < 0) {
		dev_err(pdata->dev, "dma_set_mask_and_coherent failed\n");
		return ret;
	}

	return ret;
}

/**
 * @brief Set the network device feature flags
 *
 * Algorithm:
 * 1) Check the HW features supported
 * 2) Enable corresponding feature flag so that network subsystem of OS
 * is aware of device capabilities.
 * 3) Update current enable/disable state of features currently enabled
 *
 * @param[in] ndev: Network device instance
 * @param[in] pdata: OS dependent private data structure.
 *
 * @note Netdev allocated and HW features are already parsed.
 *
 */
static void ether_set_ndev_features(struct net_device *ndev,
				    struct ether_priv_data *pdata)
{
	netdev_features_t features = 0;

	if (pdata->hw_feat.tso_en) {
		features |= NETIF_F_TSO;
		features |= NETIF_F_SG;
	}

#if (KERNEL_VERSION(5, 9, 0) < LINUX_VERSION_CODE)
	if (pdata->osi_core->mac == OSI_MAC_HW_MGBE)
		features |= NETIF_F_GSO_UDP_L4;
#endif

	if (pdata->hw_feat.tx_coe_sel) {
		features |= NETIF_F_IP_CSUM;
		features |= NETIF_F_IPV6_CSUM;
	}

	if (pdata->hw_feat.rx_coe_sel) {
		features |= NETIF_F_RXCSUM;
	}

	/* GRO is independent of HW features */
	features |= NETIF_F_GRO;

	if (pdata->hw_feat.sa_vlan_ins) {
		features |= NETIF_F_HW_VLAN_CTAG_TX;
	}

#ifdef ETHER_VLAN_VID_SUPPORT
	/* Rx VLAN tag stripping/filtering enabled by default */
	features |= NETIF_F_HW_VLAN_CTAG_RX;
	features |= NETIF_F_HW_VLAN_CTAG_FILTER;
#endif /* ETHER_VLAN_VID_SUPPORT */

	/* Receive Hashing offload */
	if (pdata->hw_feat.rss_en) {
		features |= NETIF_F_RXHASH;
	}

	/* Features available in HW */
	ndev->hw_features = features;
	/* Features that can be changed by user */
	ndev->features = features;
	/* Features that can be inherited by vlan devices */
	ndev->vlan_features = features;

	/* Set current state of features enabled by default in HW */
	pdata->hw_feat_cur_state = features;
}

/**
 * @brief Static function to initialize filter register count
 * in private data structure
 *
 * Algorithm: Updates addr_reg_cnt based on HW feature
 *
 * @param[in] pdata: ethernet private data structure
 *
 * @note MAC_HW_Feature1 register need to read and store the value of ADDR64.
 */
static void init_filter_values(struct ether_priv_data *pdata)
{
	if (pdata->hw_feat.mac_addr64_sel == OSI_ENABLE) {
		pdata->num_mac_addr_regs = ETHER_ADDR_REG_CNT_128;
	} else if (pdata->hw_feat.mac_addr32_sel == OSI_ENABLE) {
		pdata->num_mac_addr_regs = ETHER_ADDR_REG_CNT_64;
	} else if (pdata->hw_feat.mac_addr_sel ==
		   (ETHER_ADDR_REG_CNT_32 - 1U)) {
		pdata->num_mac_addr_regs = ETHER_ADDR_REG_CNT_32;
	} else {
		pdata->num_mac_addr_regs = ETHER_ADDR_REG_CNT_1;
	}
}

/**
 * @brief ether_init_rss - Init OSI RSS structure
 *
 * Algorithm: Populates RSS hash key and table in OSI core structure.
 *
 * @param[in] pdata: Ethernet private data
 * @param[in] features: Netdev features
 */
static void ether_init_rss(struct ether_priv_data *pdata,
			   netdev_features_t features)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	unsigned int num_q = osi_core->num_mtl_queues;
	unsigned int i = 0;

	if ((features & NETIF_F_RXHASH) == NETIF_F_RXHASH) {
		osi_core->rss.enable = 1;
	} else {
		osi_core->rss.enable = 0;
		return;
	}

	/* generate random key */
	netdev_rss_key_fill(osi_core->rss.key, sizeof(osi_core->rss.key));

	/* initialize hash table */
	for (i = 0; i < OSI_RSS_MAX_TABLE_SIZE; i++)
		osi_core->rss.table[i] = ethtool_rxfh_indir_default(i, num_q);
}

/**
 * @brief Ethernet platform driver probe.
 *
 * Algorithm:
 * 1) Get the number of channels from DT.
 * 2) Allocate the network device for those many channels.
 * 3) Parse MAC and PHY DT.
 * 4) Get all required clks/reset/IRQ's.
 * 5) Register MDIO bus and network device.
 * 6) Initialize spinlock.
 * 7) Update filter value based on HW feature.
 * 8) Update osi_core->hw_feature with pdata->hw_feat pointer
 * 9) Initialize Workqueue to read MMC counters periodically.
 *
 * @param[in] pdev: platform device associated with platform driver.
 *
 * @retval 0 on success
 * @retval "negative value" on failure
 *
 */
static int ether_probe(struct platform_device *pdev)
{
	struct ether_priv_data *pdata;
	unsigned int num_dma_chans, mac, num_mtl_queues, chan;
	struct osi_core_priv_data *osi_core;
	struct osi_dma_priv_data *osi_dma;
	struct osi_ioctl ioctl_data = {};
	struct net_device *ndev;
	int ret = 0, i;
	const char *if_name;

	ether_get_num_dma_chan_mtl_q(pdev, &num_dma_chans,
				     &mac, &num_mtl_queues);

	if (mac == OSI_MAC_HW_MGBE) {
		ret = pinctrl_pm_select_default_state(&pdev->dev);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to apply pinctl states: %d\n", ret);
			return ret;
		}
	}

	osi_core = osi_get_core();
	if (osi_core == NULL) {
		return -ENOMEM;
	}

	osi_dma = osi_get_dma();
	if (osi_dma == NULL) {
		return -ENOMEM;
	}

	if_name = (const char *)of_get_property(pdev->dev.of_node,
						"nvidia,if-name", NULL);
	if (if_name) {
		ndev = alloc_netdev_mqs((int)sizeof(struct ether_priv_data), if_name,
					NET_NAME_UNKNOWN, ether_setup,
					num_dma_chans, num_dma_chans);
	} else {
		/* allocate and set up the ethernet device */
		ndev = alloc_etherdev_mq((int)sizeof(struct ether_priv_data),
					 num_dma_chans);
	}

	if (ndev == NULL) {
		dev_err(&pdev->dev, "failed to allocate net device\n");
		return -ENOMEM;
	}

	SET_NETDEV_DEV(ndev, &pdev->dev);

	pdata = netdev_priv(ndev);
	pdata->dev = &pdev->dev;
	pdata->ndev = ndev;
	platform_set_drvdata(pdev, ndev);

	pdata->osi_core = osi_core;
	pdata->osi_dma = osi_dma;
	osi_core->osd = pdata;
	osi_dma->osd = pdata;

	osi_core->num_mtl_queues = num_mtl_queues;
	osi_dma->num_dma_chans = num_dma_chans;

	osi_core->mac = mac;
	osi_dma->mac = mac;

	osi_core->mtu = ndev->mtu;
	osi_dma->mtu = ndev->mtu;

	/* Parse the ethernet DT node */
	ret = ether_parse_dt(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to parse DT\n");
		goto err_parse_dt;
	}

	ether_assign_osd_ops(osi_core, osi_dma);

	/* Initialize core and DMA ops based on MAC type */
	ret = osi_init_core_ops(osi_core);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get osi_init_core_ops\n");
		goto err_core_ops;
	}

	ret = osi_init_dma_ops(osi_dma);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get osi_init_dma_ops\n");
		goto err_dma_ops;
	}

	ndev->max_mtu = pdata->max_platform_mtu;

	/* get base address, clks, reset ID's and MAC address*/
	ret = ether_init_plat_resources(pdev, pdata);
	if (ret < 0) {
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"failed to allocate platform resources\n");
		goto err_init_res;
	}

	ioctl_data.cmd = OSI_CMD_GET_MAC_VER;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get MAC version (%u)\n",
			osi_core->mac_ver);
		goto err_dma_mask;
	}
	osi_core->mac_ver = ioctl_data.arg1_u32;

	ret = ether_get_mac_address(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get MAC address\n");
		goto err_dma_mask;
	}

	ioctl_data.cmd = OSI_CMD_GET_HW_FEAT;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get HW features\n");
		goto err_dma_mask;
	}
	memcpy(&pdata->hw_feat, &ioctl_data.hw_feat,
	       sizeof(struct osi_hw_features));

	ret = ether_set_dma_mask(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set dma mask\n");
		goto err_dma_mask;
	}

	if (pdata->hw_feat.fpe_sel) {
		ret = ether_parse_residual_queue(pdata, "nvidia,residual-queue",
						 &osi_core->residual_queue);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to read RQ\n");
			goto err_dma_mask;
		}
	}

	/* Set netdev features based on hw features */
	ether_set_ndev_features(ndev, pdata);

	/* RSS init */
	ether_init_rss(pdata, ndev->features);

	ret = ether_get_irqs(pdev, pdata, num_dma_chans);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get IRQ's\n");
		goto err_dma_mask;
	}

	ndev->netdev_ops = &ether_netdev_ops;
	ether_set_ethtool_ops(ndev);

	ret = ether_alloc_napi(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to allocate NAPI\n");
		goto err_dma_mask;
	}

	/* Setup the tx_usecs timer */
	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		chan = osi_dma->dma_chans[i];
		atomic_set(&pdata->tx_napi[chan]->tx_usecs_timer_armed,
			   OSI_DISABLE);
		hrtimer_init(&pdata->tx_napi[chan]->tx_usecs_timer,
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		pdata->tx_napi[chan]->tx_usecs_timer.function =
			ether_tx_usecs_hrtimer;
	}

	ret = register_netdev(ndev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register netdev\n");
		goto err_netdev;
	}

#ifdef MACSEC_SUPPORT
	ret = macsec_probe(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to setup macsec\n");
		goto err_macsec;
	} else if (ret == 1) {
		/* Nothing to do, macsec is not supported */
		dev_info(&pdev->dev, "Macsec not supported/Not enabled in DT\n");
	} else {
		dev_info(&pdev->dev, "Macsec not enabled\n");
		/* Macsec is supported, reduce MTU */
		ndev->mtu -= MACSEC_TAG_ICV_LEN;
		dev_info(&pdev->dev, "Macsec: Reduced MTU: %d Max: %d\n",
			 ndev->mtu, ndev->max_mtu);
	}
#endif /*  MACSEC_SUPPORT */

	/* Register sysfs entry */
	ret = ether_sysfs_register(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"failed to create nvethernet sysfs group\n");
		goto err_sysfs;
	}


	raw_spin_lock_init(&pdata->rlock);
	raw_spin_lock_init(&pdata->txts_lock);
	init_filter_values(pdata);

	if (osi_core->mac == OSI_MAC_HW_MGBE)
		pm_runtime_enable(pdata->dev);

	/* Disable Clocks */
	ether_disable_clks(pdata);

	dev_info(&pdev->dev,
		 "%s (HW ver: %02x) created with %u DMA channels\n",
		 netdev_name(ndev), osi_core->mac_ver, num_dma_chans);

	if (gpio_is_valid(pdata->phy_reset)) {
		gpio_set_value(pdata->phy_reset, OSI_DISABLE);
	}
	/* Initialization of delayed workqueue */
	INIT_DELAYED_WORK(&pdata->ether_stats_work, ether_stats_work_func);
#ifdef HSI_SUPPORT
	/* Initialization of delayed workqueue for HSI error reporting */
	INIT_DELAYED_WORK(&pdata->ether_hsi_work, ether_hsi_work_func);
#endif
	/* Initialization of set speed workqueue */
	INIT_DELAYED_WORK(&pdata->set_speed_work, set_speed_work_func);
	osi_core->hw_feature = &pdata->hw_feat;
	INIT_LIST_HEAD(&pdata->tx_ts_skb_head);
	INIT_DELAYED_WORK(&pdata->tx_ts_work, ether_get_tx_ts_work);
	pdata->rx_m_enabled = false;
	pdata->rx_pcs_m_enabled = false;
	atomic_set(&pdata->tx_ts_ref_cnt, -1);
	atomic_set(&pdata->set_speed_ref_cnt, OSI_DISABLE);
	tasklet_setup(&pdata->lane_restart_task,
		      ether_restart_lane_bringup_task);
#ifdef ETHER_NVGRO
	__skb_queue_head_init(&pdata->mq);
	__skb_queue_head_init(&pdata->fq);
	pdata->pkt_age_msec = NVGRO_AGE_THRESHOLD;
	pdata->nvgro_timer_intrvl = NVGRO_PURGE_TIMER_THRESHOLD;
	pdata->nvgro_dropped = 0;
	timer_setup(&pdata->nvgro_timer, ether_nvgro_purge_timer, 0);
#endif

#ifdef HSI_SUPPORT
	mutex_init(&pdata->hsi_lock);
#endif

	return 0;

err_sysfs:
#ifdef MACSEC_SUPPORT
err_macsec:
#endif /* MACSEC_SUPPORT */
	unregister_netdev(ndev);
err_netdev:
err_dma_mask:
	ether_disable_clks(pdata);
	ether_put_clks(pdata);
	if (gpio_is_valid(pdata->phy_reset)) {
		gpio_set_value(pdata->phy_reset, OSI_DISABLE);
	}
err_init_res:
err_parse_dt:
err_core_ops:
err_dma_ops:
	ether_stop_ivc(pdata);
	free_netdev(ndev);
	return ret;
}

/**
 * @brief Ethernet platform driver remove.
 *
 * Alogorithm: Release all the resources
 *
 * @param[in] pdev: Platform device associated with platform driver.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct ether_priv_data *pdata = netdev_priv(ndev);

#ifdef MACSEC_SUPPORT
	macsec_remove(pdata);
#endif /* MACSEC_SUPPORT */

	unregister_netdev(ndev);

	/* remove nvethernet sysfs group under /sys/devices/<ether_device>/ */
	ether_sysfs_unregister(pdata);

	ether_put_clks(pdata);

	/* Assert MAC RST gpio */
	if (pdata->mac_rst) {
		reset_control_assert(pdata->mac_rst);
		if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
			pm_runtime_disable(pdata->dev);
		}
	}

	if (pdata->xpcs_rst) {
		reset_control_assert(pdata->xpcs_rst);
	}

	free_netdev(ndev);

	return 0;
}

/**
 * @brief Ethernet platform driver shutdown.
 *
 * Alogorithm: Stops and Deinits PHY, MAC, DMA and Clks hardware and
 *             release SW allocated resources(buffers, workqueues etc).
 *
 * @param[in] pdev: Platform device associated with platform driver.
 */
static void ether_shutdown(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	int ret = -1;

	if (!netif_running(ndev))
		return;

	ret = ether_close(ndev);
	if (ret)
		dev_err(pdata->dev, "Failure in ether_close");
}

#ifdef CONFIG_PM
/**
 * @brief Ethernet platform driver resume call.
 *
 * Alogorithm: Init OSI core, DMA and TX/RX interrupts.
 *	Enable PHY device if it does not wake capable,
 *	all data queues and NAPI.
 *
 * @param[in] pdata: OS dependent private data structure.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_resume(struct ether_priv_data *pdata)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct device *dev = pdata->dev;
	struct net_device *ndev = pdata->ndev;
	struct osi_ioctl ioctl_data = {};
	int ret = 0;

	if (osi_core->mac == OSI_MAC_HW_MGBE)
		pm_runtime_get_sync(pdata->dev);

	if (pdata->mac_rst) {
		ret = reset_control_reset(pdata->mac_rst);
		if (ret < 0) {
			dev_err(dev, "failed to reset mac hw\n");
			return -1;
		}
	}

	ioctl_data.cmd = OSI_CMD_POLL_FOR_MAC_RST;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0) {
		dev_err(dev, "failed to poll mac software reset\n");
		return ret;
	}

	if (pdata->xpcs_rst) {
		ret = reset_control_reset(pdata->xpcs_rst);
		if (ret < 0) {
			dev_err(dev, "failed to reset XPCS hw\n");
			return ret;
		}
	}

	ret = ether_pad_calibrate(pdata);
	if (ret < 0) {
		dev_err(dev, "failed to do pad caliberation\n");
		return ret;
	}
	osi_set_rx_buf_len(osi_dma);

	ret = ether_allocate_dma_resources(pdata);
	if (ret < 0) {
		dev_err(dev, "failed to allocate dma resources\n");
		return ret;
	}

	ioctl_data.cmd =  OSI_CMD_RESUME;
	if (osi_handle_ioctl(osi_core, &ioctl_data)) {
		dev_err(dev, "Failed to perform OSI resume\n");
		goto err_resume;
	}

	/* dma init */
	ret = osi_hw_dma_init(osi_dma);
	if (ret < 0) {
		dev_err(dev,
			"%s: failed to initialize mac hw dma with reason %d\n",
			__func__, ret);
		goto err_dma;
	}

	/* enable NAPI */
	ether_napi_enable(pdata);

	if (pdata->phydev && !(device_may_wakeup(&ndev->dev))) {
		/* configure phy init */
		phy_init_hw(pdata->phydev);
		/* start phy */
		phy_start(pdata->phydev);
	}
	/* start network queues */
	netif_tx_start_all_queues(ndev);
	/* re-start workqueue */
	ether_stats_work_queue_start(pdata);
#ifdef HSI_SUPPORT
	schedule_delayed_work(&pdata->ether_hsi_work,
			      msecs_to_jiffies(osi_core->hsi.err_time_threshold));
#endif
	/* Keep MACSEC also to Resume if MACSEC is supported on this platform */
#ifdef MACSEC_SUPPORT
	if (pdata->macsec_pdata && pdata->macsec_pdata->next_supp_idx != OSI_DISABLE) {
		ret = macsec_resume(pdata->macsec_pdata);
		if (ret < 0)
			dev_err(pdata->dev, "Failed to resume MACSEC ");
	}
#endif /* MACSEC_SUPPORT */

	return 0;

err_dma:
	ether_napi_disable(pdata);
	osi_hw_core_deinit(osi_core);
err_resume:
	free_dma_resources(pdata);

	return ret;
}

/**
 * @brief Ethernet platform driver suspend noirq callback.
 *
 * Alogorithm: Stops all data queues and PHY if the device
 *	does not wake capable. Disable TX and NAPI.
 *	Deinit OSI core, DMA and TX/RX interrupts.
 *
 * @param[in] dev: Platform device associated with platform driver.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_suspend_noirq(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct osi_ioctl ioctl_data = {};
	unsigned int i = 0, chan = 0;
	int ret;

	if (!netif_running(ndev))
		return 0;

	/* Keep MACSEC to suspend if MACSEC is supported on this platform */
#ifdef MACSEC_SUPPORT
	if (pdata->macsec_pdata && pdata->macsec_pdata->next_supp_idx != OSI_DISABLE) {
		ret = macsec_suspend(pdata->macsec_pdata);
		if (ret < 0)
			dev_err(pdata->dev, "Failed to suspend macsec");
	}
#endif /* MACSEC_SUPPORT */

	tasklet_kill(&pdata->lane_restart_task);

	/* stop workqueue */
	cancel_delayed_work_sync(&pdata->tx_ts_work);
	cancel_delayed_work_sync(&pdata->set_speed_work);

	/* Stop workqueue while DUT is going to suspend state */
	ether_stats_work_queue_stop(pdata);
#ifdef HSI_SUPPORT
	cancel_delayed_work_sync(&pdata->ether_hsi_work);
#endif
	if (pdata->phydev && !(device_may_wakeup(&ndev->dev))) {
		phy_stop(pdata->phydev);
		if (gpio_is_valid(pdata->phy_reset))
			gpio_set_value(pdata->phy_reset, 0);
	}

	netif_tx_disable(ndev);
	ether_napi_disable(pdata);

	osi_hw_dma_deinit(osi_dma);

	ioctl_data.cmd =  OSI_CMD_SUSPEND;
	if (osi_handle_ioctl(osi_core, &ioctl_data)) {
		dev_err(dev, "Failed to perform OSI core suspend\n");
		if (ether_resume(pdata) < 0) {
			dev_err(dev, "Failed to perform resume on suspend fail\n");
		}
		return -EBUSY;
	}

	for (i = 0; i < osi_dma->num_dma_chans; i++) {
		chan = osi_dma->dma_chans[i];
		osi_handle_dma_intr(osi_dma, chan,
				    OSI_DMA_CH_TX_INTR,
				    OSI_DMA_INTR_DISABLE);
		osi_handle_dma_intr(osi_dma, chan,
				    OSI_DMA_CH_RX_INTR,
				    OSI_DMA_INTR_DISABLE);
	}

	free_dma_resources(pdata);

	if (osi_core->mac == OSI_MAC_HW_MGBE)
		pm_runtime_put_sync(pdata->dev);

	return 0;
}


/**
 * @brief Ethernet platform driver resume noirq callback.
 *
 * Alogorithm: Enable clocks and call resume sequence,
 *	refer ether_resume function for resume sequence.
 *
 * @param[in] dev: Platform device associated with platform driver.
 *
 * @retval 0 on success
 * @retval "negative value" on failure.
 */
static int ether_resume_noirq(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	int ret = 0;

	if (!netif_running(ndev))
		return 0;

	if (!device_may_wakeup(&ndev->dev) &&
	    gpio_is_valid(pdata->phy_reset) &&
	    !gpio_get_value(pdata->phy_reset)) {
		/* deassert phy reset */
		gpio_set_value(pdata->phy_reset, 1);
	}

	ret = ether_resume(pdata);
	if (ret < 0) {
		dev_err(dev, "failed to resume the MAC\n");
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops ether_pm_ops = {
#if (KERNEL_VERSION(5, 9, 0) < LINUX_VERSION_CODE)
	.suspend = ether_suspend_noirq,
	.resume = ether_resume_noirq,
#else
	.suspend_noirq = ether_suspend_noirq,
	.resume_noirq = ether_resume_noirq,
#endif
};
#endif

/**
 * @brief Ethernet device tree compatible match name
 */
static const struct of_device_id ether_of_match[] = {
	{ .compatible = "nvidia,nveqos" },
	{ .compatible = "nvidia,nvmgbe" },
	{ .compatible = "nvidia,tegra234-mgbe" },
	{ .compatible = "nvidia,tegra234-eqos" },
	{},
};
MODULE_DEVICE_TABLE(of, ether_of_match);

/**
 * @brief Ethernet platform driver instance
 */
static struct platform_driver ether_driver = {
	.probe = ether_probe,
	.remove = ether_remove,
	.shutdown = ether_shutdown,
	.driver = {
		.name = "nvethernet",
		.of_match_table = ether_of_match,
#ifdef CONFIG_PM
		.pm = &ether_pm_ops,
#endif
	},
};

static int __init nvethernet_driver_init(void)
{
	return platform_driver_register(&ether_driver);
}

#if IS_MODULE(CONFIG_NVETHERNET)
static void __exit nvethernet_driver_deinit(void)
{
	platform_driver_unregister(&ether_driver);
}

module_init(nvethernet_driver_init);
module_exit(nvethernet_driver_deinit);
#else
late_initcall(nvethernet_driver_init);
#endif

MODULE_LICENSE("GPL v2");
