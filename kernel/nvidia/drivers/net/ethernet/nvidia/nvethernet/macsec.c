/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION.  All rights reserved.
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

#ifdef MACSEC_SUPPORT
#include "ether_linux.h"
#ifdef MACSEC_KEY_PROGRAM
#include <linux/crypto.h>
#endif /* MACSEC_KEY_PROGRAM */
#ifdef HSI_SUPPORT
#include <linux/tegra-epl.h>
#endif
static int macsec_get_tx_next_pn(struct sk_buff *skb, struct genl_info *info);

#ifndef MACSEC_KEY_PROGRAM
static int macsec_tz_kt_config(struct ether_priv_data *pdata,
			unsigned char cmd,
			struct osi_macsec_kt_config *const kt_config,
			struct genl_info *const info, struct nvpkcs_data *pkcs);
#endif

static irqreturn_t macsec_s_isr(int irq, void *data)
{
	struct macsec_priv_data *macsec_pdata = (struct macsec_priv_data *)data;
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;

	osi_macsec_isr(pdata->osi_core);

	return IRQ_HANDLED;
}

#ifdef HSI_SUPPORT
static inline u64 rdtsc(void)
{
	u64 val;

	asm volatile("mrs %0, cntvct_el0" : "=r" (val));

	return val;
}

static irqreturn_t macsec_ns_isr_thread(int irq, void *data)
{
	struct macsec_priv_data *macsec_pdata = (struct macsec_priv_data *)data;
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;
	struct device *dev = pdata->dev;
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	int ret = 0;
	int i = 0;
	struct epl_error_report_frame error_report;

	mutex_lock(&pdata->hsi_lock);
	if (osi_core->hsi.macsec_report_err) {
		error_report.reporter_id = osi_core->hsi.reporter_id;
		error_report.timestamp = lower_32_bits(rdtsc());

		for (i = 0; i < HSI_MAX_MACSEC_ERROR_CODE; i++) {
			if (osi_core->hsi.macsec_err_code[i] > 0 &&
			    osi_core->hsi.macsec_report_count_err[i] == OSI_ENABLE) {
				error_report.error_code =
					osi_core->hsi.macsec_err_code[i];
				ret = epl_report_error(error_report);
				if (ret < 0) {
					dev_err(dev, "Failed to report error: reporter ID: 0x%x, Error code: 0x%x, return: %d\n",
						osi_core->hsi.reporter_id,
						osi_core->hsi.macsec_err_code[i], ret);
				} else {
					dev_info(dev, "EPL report error: reporter ID: 0x%x, Error code: 0x%x",
						 osi_core->hsi.reporter_id,
						 osi_core->hsi.macsec_err_code[i]);
				}
				osi_core->hsi.macsec_err_code[i] = 0;
				osi_core->hsi.macsec_report_count_err[i] = OSI_DISABLE;
			}
		}
	}
	mutex_unlock(&pdata->hsi_lock);

	return IRQ_HANDLED;
}
#endif

static irqreturn_t macsec_ns_isr(int irq, void *data)
{
	struct macsec_priv_data *macsec_pdata = (struct macsec_priv_data *)data;
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;
	int irq_ret = IRQ_HANDLED;

	osi_macsec_isr(pdata->osi_core);

#ifdef HSI_SUPPORT
	if (pdata->osi_core->hsi.enabled == OSI_ENABLE &&
	    pdata->osi_core->hsi.macsec_report_err == OSI_ENABLE)
		irq_ret = IRQ_WAKE_THREAD;
#endif
	return irq_ret;
}

static int macsec_disable_car(struct macsec_priv_data *macsec_pdata)
{
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;

	PRINT_ENTRY();
	if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
		if (!IS_ERR_OR_NULL(macsec_pdata->mgbe_clk)) {
			clk_disable_unprepare(macsec_pdata->mgbe_clk);
		}
	} else {
		if (!IS_ERR_OR_NULL(macsec_pdata->eqos_tx_clk)) {
			clk_disable_unprepare(macsec_pdata->eqos_tx_clk);
		}

		if (!IS_ERR_OR_NULL(macsec_pdata->eqos_rx_clk)) {
			clk_disable_unprepare(macsec_pdata->eqos_rx_clk);
		}
	}

	if (macsec_pdata->ns_rst) {
		reset_control_assert(macsec_pdata->ns_rst);
	}

	PRINT_EXIT();
	return 0;
}

static int macsec_enable_car(struct macsec_priv_data *macsec_pdata)
{
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;
	struct device *dev = pdata->dev;
	int ret = 0;

	PRINT_ENTRY();
	if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
		if (!IS_ERR_OR_NULL(macsec_pdata->mgbe_clk)) {
			ret = clk_prepare_enable(macsec_pdata->mgbe_clk);
			if (ret < 0) {
				dev_err(dev, "failed to enable macsec clk\n");
				goto exit;
			}
		}
	} else {
		if (!IS_ERR_OR_NULL(macsec_pdata->eqos_tx_clk)) {
			ret = clk_prepare_enable(macsec_pdata->eqos_tx_clk);
			if (ret < 0) {
				dev_err(dev, "failed to enable macsec tx clk\n");
				goto exit;
			}
		}

		if (!IS_ERR_OR_NULL(macsec_pdata->eqos_rx_clk)) {
			ret = clk_prepare_enable(macsec_pdata->eqos_rx_clk);
			if (ret < 0) {
				dev_err(dev, "failed to enable macsec rx clk\n");
				goto err_rx_clk;
			}
		}
	}

	if (macsec_pdata->ns_rst) {
		ret = reset_control_reset(macsec_pdata->ns_rst);
		if (ret < 0) {
			dev_err(dev, "failed to reset macsec\n");
			goto err_ns_rst;
		}
	}

	goto exit;

err_ns_rst:
	if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
		if (!IS_ERR_OR_NULL(macsec_pdata->mgbe_clk)) {
			clk_disable_unprepare(macsec_pdata->mgbe_clk);
		}
	} else {
		if (!IS_ERR_OR_NULL(macsec_pdata->eqos_rx_clk)) {
			clk_disable_unprepare(macsec_pdata->eqos_rx_clk);
		}
err_rx_clk:
		if (!IS_ERR_OR_NULL(macsec_pdata->eqos_tx_clk)) {
			clk_disable_unprepare(macsec_pdata->eqos_tx_clk);
		}
	}
exit:
	PRINT_EXIT();
	return ret;
}

int macsec_close(struct macsec_priv_data *macsec_pdata)
{
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;
	struct device *dev = pdata->dev;
	int ret = 0;

	PRINT_ENTRY();
	/* Disable the macsec controller */
	ret = osi_macsec_en(pdata->osi_core, OSI_DISABLE);
	if (ret < 0) {
		dev_err(dev, "%s: Failed to enable macsec Tx/Rx, %d\n",
			__func__, ret);
		return ret;
	}
	macsec_pdata->enabled = OSI_DISABLE;
	osi_macsec_deinit(pdata->osi_core);

	if (macsec_pdata->is_irq_allocated & OSI_BIT(1)) {
		devm_free_irq(dev, macsec_pdata->ns_irq, macsec_pdata);
		macsec_pdata->is_irq_allocated &= ~OSI_BIT(1);
	}
	if (macsec_pdata->is_irq_allocated & OSI_BIT(0)) {
		devm_free_irq(dev, macsec_pdata->s_irq, macsec_pdata);
		macsec_pdata->is_irq_allocated &= ~OSI_BIT(0);
	}

	PRINT_EXIT();

	return ret;
}

int macsec_open(struct macsec_priv_data *macsec_pdata,
		void *const genl_info)
{
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;
	struct device *dev = pdata->dev;
	int ret = 0;

	PRINT_ENTRY();
	/* Request macsec irqs */
	snprintf(macsec_pdata->irq_name[0], MACSEC_IRQ_NAME_SZ, "%s.macsec_s",
		 netdev_name(pdata->ndev));
	ret = devm_request_irq(dev, macsec_pdata->s_irq, macsec_s_isr,
			       IRQF_TRIGGER_NONE, macsec_pdata->irq_name[0],
			       macsec_pdata);
	if (ret < 0) {
		dev_err(dev, "failed to request irq %d\n", __LINE__);
		goto exit;
	}

	dev_info(dev, "%s: requested s_irq %d: %s\n", __func__,
		 macsec_pdata->s_irq, macsec_pdata->irq_name[0]);
	macsec_pdata->is_irq_allocated |= OSI_BIT(0);

	snprintf(macsec_pdata->irq_name[1], MACSEC_IRQ_NAME_SZ, "%s.macsec_ns",
		 netdev_name(pdata->ndev));

#ifdef HSI_SUPPORT
	ret = devm_request_threaded_irq(dev, macsec_pdata->ns_irq, macsec_ns_isr,
					macsec_ns_isr_thread,
					IRQF_TRIGGER_NONE | IRQF_ONESHOT,
					macsec_pdata->irq_name[1],
					macsec_pdata);
#else
	ret = devm_request_irq(dev, macsec_pdata->ns_irq, macsec_ns_isr,
			       IRQF_TRIGGER_NONE, macsec_pdata->irq_name[1],
			       macsec_pdata);
#endif
	if (ret < 0) {
		dev_err(dev, "failed to request irq %d\n", __LINE__);
		goto err_ns_irq;
	}

	dev_info(dev, "%s: requested ns_irq %d: %s\n", __func__,
		 macsec_pdata->ns_irq, macsec_pdata->irq_name[1]);
	macsec_pdata->is_irq_allocated |= OSI_BIT(1);

	/* Invoke OSI HW initialization, initialize standard BYP entries */
	ret = osi_macsec_init(pdata->osi_core, pdata->osi_core->mtu);
	if (ret < 0) {
		dev_err(dev, "osi_macsec_init failed, %d\n", ret);
		goto err_osi_init;
	}

#if !defined(MACSEC_KEY_PROGRAM) && !defined(NVPKCS_MACSEC)
	/* Clear KT entries */
	ret = macsec_tz_kt_config(pdata, NV_MACSEC_CMD_TZ_KT_RESET,
				  OSI_NULL, genl_info, NULL);
	if (ret < 0) {
		dev_err(dev, "TZ key config failed %d\n", ret);
		goto err_osi_en;
	}
#endif /* !MACSEC_KEY_PROGRAM */

	/* Enable the macsec controller */
	ret = osi_macsec_en(pdata->osi_core,
			    (OSI_MACSEC_TX_EN | OSI_MACSEC_RX_EN));
	if (ret < 0) {
		dev_err(dev, "%s: Failed to enable macsec Tx/Rx, %d\n",
			__func__, ret);
		goto err_osi_en;
	}
	macsec_pdata->enabled = (OSI_MACSEC_TX_EN | OSI_MACSEC_RX_EN);

	goto exit;

err_osi_en:
	osi_macsec_deinit(pdata->osi_core);
err_osi_init:
	devm_free_irq(dev, macsec_pdata->ns_irq, macsec_pdata);
err_ns_irq:
	devm_free_irq(dev, macsec_pdata->s_irq, macsec_pdata);
exit:
	PRINT_EXIT();
	return ret;
}

/**
 * Calling macsec_close as part of macsec_suspend as supplicant
 * is disabling the current AN and creating new AN as part of
 * resume.
 */
int macsec_suspend(struct macsec_priv_data *macsec_pdata)
{
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;
	struct device *dev = pdata->dev;
	int ret = 0;

	ret = macsec_close(macsec_pdata);
	if (ret < 0) {
		dev_err(dev, "Failed to close macsec\n");
		return ret;
	}
	return ret;
}

/**
 * Calling macsec_open as part of macsec_resume as supplicant
 * is disabling the current AN and creating new AN as part of
 * resume.
 */
int macsec_resume(struct macsec_priv_data *macsec_pdata)
{
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;
	struct device *dev = pdata->dev;
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	int ret = 0;

	if ((osi_core->use_virtualization == OSI_DISABLE) &&
	    (macsec_pdata->ns_rst)) {
		ret = reset_control_reset(macsec_pdata->ns_rst);
		if (ret < 0) {
			dev_err(dev, "failed to reset macsec\n");
			return ret;
		}
	}

	return macsec_open(macsec_pdata, OSI_NULL);
}

static int macsec_get_platform_res(struct macsec_priv_data *macsec_pdata)
{
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;
	struct device *dev = pdata->dev;
	struct platform_device *pdev = to_platform_device(dev);
	int ret = 0;

	PRINT_ENTRY();
	/* 1. Get resets */
	macsec_pdata->ns_rst = devm_reset_control_get(dev, "macsec_ns_rst");
	if (IS_ERR_OR_NULL(macsec_pdata->ns_rst)) {
		dev_err(dev, "Failed to get macsec_ns_rst\n");
		ret = PTR_ERR(macsec_pdata->ns_rst);
		goto exit;
	}

	/* 2. Get clks */
	if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
		macsec_pdata->mgbe_clk = devm_clk_get(dev, "mgbe_macsec");
		if (IS_ERR(macsec_pdata->mgbe_clk)) {
			dev_err(dev, "failed to get macsec clk\n");
			ret = PTR_ERR(macsec_pdata->mgbe_clk);
			goto exit;
		}
	} else {
		macsec_pdata->eqos_tx_clk = devm_clk_get(dev, "eqos_macsec_tx");
		if (IS_ERR(macsec_pdata->eqos_tx_clk)) {
			dev_err(dev, "failed to get eqos_tx clk\n");
			ret = PTR_ERR(macsec_pdata->eqos_tx_clk);
			goto exit;
		}
		macsec_pdata->eqos_rx_clk = devm_clk_get(dev,
						"eqos_macsec_rx");
		if (IS_ERR(macsec_pdata->eqos_rx_clk)) {
			dev_err(dev, "failed to get eqos_rx_clk clk\n");
			ret = PTR_ERR(macsec_pdata->eqos_rx_clk);
			goto exit;
		}
	}

	/* 3. Get irqs */
	macsec_pdata->ns_irq = platform_get_irq_byname(pdev, "macsec-ns-irq");
	if (macsec_pdata->ns_irq < 0) {
		dev_err(dev, "failed to get macsec-ns-irq\n");
		ret = macsec_pdata->ns_irq;
		goto exit;
	}

	macsec_pdata->s_irq = platform_get_irq_byname(pdev, "macsec-s-irq");
	if (macsec_pdata->s_irq < 0) {
		dev_err(dev, "failed to get macsec-s-irq\n");
		ret = macsec_pdata->s_irq;
		goto exit;
	}

exit:
	PRINT_EXIT();
	return ret;
}

static void macsec_release_platform_res(struct macsec_priv_data *macsec_pdata)
{
	struct ether_priv_data *pdata = macsec_pdata->ether_pdata;
	struct device *dev = pdata->dev;

	PRINT_ENTRY();
	if (pdata->osi_core->mac == OSI_MAC_HW_MGBE) {
		if (!IS_ERR_OR_NULL(macsec_pdata->mgbe_clk)) {
			devm_clk_put(dev, macsec_pdata->mgbe_clk);
		}
	} else {
		if (!IS_ERR_OR_NULL(macsec_pdata->eqos_tx_clk)) {
			devm_clk_put(dev, macsec_pdata->eqos_tx_clk);
		}

		if (!IS_ERR_OR_NULL(macsec_pdata->eqos_rx_clk)) {
			devm_clk_put(dev, macsec_pdata->eqos_rx_clk);
		}
	}

	PRINT_EXIT();
}

static struct macsec_priv_data *genl_to_macsec_pdata(struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct net_device *ndev;
	struct ether_priv_data *pdata = NULL;
	struct macsec_priv_data *macsec_pdata = NULL;
	char ifname[IFNAMSIZ];

	PRINT_ENTRY();

	nla_strlcpy(ifname, attrs[NV_MACSEC_ATTR_IFNAME], sizeof(ifname));
	ndev = dev_get_by_name(genl_info_net(info),
			       ifname);
	if (!ndev) {
		pr_err("%s: Unable to get netdev\n", __func__);
		goto exit;
	}

	pdata = netdev_priv(ndev);
	macsec_pdata = pdata->macsec_pdata;
	dev_put(ndev);
exit:
	PRINT_EXIT();
	return macsec_pdata;
}

static struct macsec_supplicant_data *macsec_get_supplicant(
		struct macsec_priv_data *macsec_pdata,
		unsigned int portid)
{
	struct macsec_supplicant_data *supplicant = macsec_pdata->supplicant;
	int i;

	/* check for already exist instance */
	for (i = 0; i < OSI_MAX_NUM_SC; i++) {
		if (supplicant[i].snd_portid == portid &&
		    supplicant[i].in_use == OSI_ENABLE) {
			return &supplicant[i];
		}
	}
	return NULL;
}

static int update_prot_frame(
			struct macsec_priv_data *macsec_pdata) {
	struct macsec_supplicant_data *supplicant = macsec_pdata->supplicant;
	int i;
	int enable = OSI_NONE;

	/* check any supplicant instance set */
	for (i = 0; i < OSI_MAX_NUM_SC; i++) {
		if (supplicant[i].protect_frames == OSI_ENABLE) {
			enable = OSI_ENABLE;
			break;
		}
	}
	return enable;
}

static int update_set_controlled_port(
			struct macsec_priv_data *macsec_pdata) {
	struct macsec_supplicant_data *supplicant = macsec_pdata->supplicant;
	int i;
	int enable = OSI_NONE;

	/* check any supplicant instance set */
	for (i = 0; i < OSI_MAX_NUM_SC; i++) {
		if (supplicant[i].enabled == OSI_ENABLE) {
			enable = OSI_ENABLE;
			break;
		}
	}
	return enable;
}

static int macsec_set_prot_frames(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	unsigned int enable;
	struct macsec_priv_data *macsec_pdata;
	struct macsec_supplicant_data *supplicant;
	struct ether_priv_data *pdata = NULL;
	int ret = 0;

	PRINT_ENTRY();
	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    !attrs[NV_MACSEC_ATTR_PROT_FRAMES_EN]) {
		ret = -EINVAL;
		goto exit;
	}

	macsec_pdata = genl_to_macsec_pdata(info);
	if (!macsec_pdata) {
		ret = -EPROTO;
		goto exit;
	}
	pdata = macsec_pdata->ether_pdata;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(pdata->dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	mutex_lock(&macsec_pdata->lock);
	supplicant = macsec_get_supplicant(macsec_pdata, info->snd_portid);
	if (!supplicant) {
		ret = -EPROTO;
		dev_err(pdata->dev, "%s: failed to get supplicant data",
			__func__);
		goto err_unlock;
	}
	supplicant->protect_frames =
		nla_get_u32(attrs[NV_MACSEC_ATTR_PROT_FRAMES_EN]);
	enable = update_prot_frame(macsec_pdata);
	macsec_pdata->protect_frames = enable;

err_unlock:
	mutex_unlock(&macsec_pdata->lock);
exit:
	PRINT_EXIT();
	return ret;
}

static int macsec_set_cipher(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata;
	struct macsec_supplicant_data *supplicant;
	struct ether_priv_data *pdata = NULL;
	int ret = 0;

	PRINT_ENTRY();
	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    !attrs[NV_MACSEC_ATTR_CIPHER_SUITE]) {
		ret = -EINVAL;
		goto exit;
	}

	macsec_pdata = genl_to_macsec_pdata(info);
	if (!macsec_pdata) {
		ret = -EPROTO;
		goto exit;
	}
	pdata = macsec_pdata->ether_pdata;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(pdata->dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	mutex_lock(&macsec_pdata->lock);
	supplicant = macsec_get_supplicant(macsec_pdata, info->snd_portid);
	if (!supplicant) {
		ret = -EPROTO;
		dev_err(pdata->dev, "%s: failed to get supplicant data\n",
			__func__);
		goto err_unlock;
	}
	supplicant->cipher = nla_get_u32(attrs[NV_MACSEC_ATTR_CIPHER_SUITE]);

	if (supplicant->cipher != OSI_MACSEC_CIPHER_AES128 &&
	    supplicant->cipher != OSI_MACSEC_CIPHER_AES256) {
		ret = -EPROTO;
		dev_err(pdata->dev, "%s: Invalid cipher suit %d\n",
			__func__, supplicant->cipher);
		goto err_unlock;
	}

	if (macsec_pdata->cipher != supplicant->cipher) {
		ret = osi_macsec_cipher_config(pdata->osi_core,
					       supplicant->cipher);
		if (ret < 0) {
			dev_err(pdata->dev, "Failed to set macsec cipher\n");
		}
		macsec_pdata->cipher = supplicant->cipher;
	}
err_unlock:
	mutex_unlock(&macsec_pdata->lock);
exit:
	return ret;
}

static int macsec_set_controlled_port(struct sk_buff *skb,
				      struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata;
	unsigned int enable = 0;
	unsigned int macsec_en = 0;
	struct macsec_supplicant_data *supplicant;
	struct ether_priv_data *pdata = NULL;
	int ret = 0;

	PRINT_ENTRY();
	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    !attrs[NV_MACSEC_ATTR_CTRL_PORT_EN]) {
		ret = -EINVAL;
		goto exit;
	}

	macsec_pdata = genl_to_macsec_pdata(info);
	if (!macsec_pdata) {
		ret = -EPROTO;
		goto exit;
	}
	pdata = macsec_pdata->ether_pdata;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(pdata->dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	mutex_lock(&macsec_pdata->lock);
	supplicant = macsec_get_supplicant(macsec_pdata, info->snd_portid);
	if (!supplicant) {
		ret = -EPROTO;
		dev_err(pdata->dev, "%s: failed to get supplicant data",
			__func__);
		goto err_unlock;
	}

	supplicant->enabled = nla_get_u32(attrs[NV_MACSEC_ATTR_CTRL_PORT_EN]);
	enable = update_set_controlled_port(macsec_pdata);
	if (enable) {
		macsec_en |= OSI_MACSEC_RX_EN;
		if (macsec_pdata->protect_frames)
			macsec_en |= OSI_MACSEC_TX_EN;
	}

	if (macsec_pdata->enabled != macsec_en) {
		ret = osi_macsec_en(macsec_pdata->ether_pdata->osi_core,
				    macsec_en);
		if (ret < 0) {
			ret = -EPROTO;
			goto err_unlock;
		}
		macsec_pdata->enabled = macsec_en;
	}

err_unlock:
	mutex_unlock(&macsec_pdata->lock);
exit:
	PRINT_EXIT();
	return ret;
}

static int parse_sa_config(struct nlattr **attrs, struct nlattr **tb_sa,
			   struct osi_macsec_sc_info *sc_info,
			   struct nvpkcs_data *pkcs)
{
	if (!attrs[NV_MACSEC_ATTR_SA_CONFIG])
		return -EINVAL;

	if (nla_parse_nested(tb_sa, NV_MACSEC_SA_ATTR_MAX,
			     attrs[NV_MACSEC_ATTR_SA_CONFIG],
			     nv_macsec_sa_genl_policy, NULL))
		return -EINVAL;

	if (tb_sa[NV_MACSEC_SA_ATTR_SCI]) {
		memcpy(sc_info->sci, nla_data(tb_sa[NV_MACSEC_SA_ATTR_SCI]),
			sizeof(sc_info->sci));
	}
	if (tb_sa[NV_MACSEC_SA_ATTR_AN]) {
		sc_info->curr_an = nla_get_u8(tb_sa[NV_MACSEC_SA_ATTR_AN]);
	}
	if (tb_sa[NV_MACSEC_SA_ATTR_PN]) {
		sc_info->next_pn = nla_get_u32(tb_sa[NV_MACSEC_SA_ATTR_PN]);
	}
	if (tb_sa[NV_MACSEC_SA_ATTR_LOWEST_PN]) {
		sc_info->lowest_pn = nla_get_u32(tb_sa[NV_MACSEC_SA_ATTR_LOWEST_PN]);
	}
#ifdef NVPKCS_MACSEC
	if (pkcs) {
		if (tb_sa[NV_MACSEC_SA_PKCS_KEY_WRAP]) {
			memcpy(pkcs->nv_key,
				nla_data(tb_sa[NV_MACSEC_SA_PKCS_KEY_WRAP]),
				sizeof(pkcs->nv_key));
		}
		if (tb_sa[NV_MACSEC_SA_PKCS_KEK_HANDLE]) {
			pkcs->nv_kek = nla_get_u64(tb_sa[NV_MACSEC_SA_PKCS_KEK_HANDLE]);
		}
	}
#else
	if (tb_sa[NV_MACSEC_SA_ATTR_KEY]) {
		memcpy(sc_info->sak, nla_data(tb_sa[NV_MACSEC_SA_ATTR_KEY]),
		       sizeof(sc_info->sak));
	}
#endif /* NVPKCS_MACSEC */
	return 0;
}

static int macsec_dis_rx_sa(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata;
	struct ether_priv_data *pdata;
	struct osi_macsec_sc_info rx_sa = {0};
	struct nlattr *tb_sa[NUM_NV_MACSEC_SA_ATTR];
	int ret = 0;
	unsigned short kt_idx;
	struct device *dev = NULL;
#ifndef MACSEC_KEY_PROGRAM
	struct osi_macsec_kt_config kt_config = {0};
	struct osi_macsec_table_config *table_config;
#endif /* !MACSEC_KEY_PROGRAM */
	struct nvpkcs_data pkcs = {0};

	PRINT_ENTRY();

	macsec_pdata = genl_to_macsec_pdata(info);
	if (macsec_pdata) {
		pdata = macsec_pdata->ether_pdata;
	} else {
		ret = -EPROTO;
		goto exit;
	}
	dev = pdata->dev;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    parse_sa_config(attrs, tb_sa, &rx_sa, &pkcs)) {
		dev_err(dev, "%s: failed to parse nlattrs", __func__);
		ret = -EINVAL;
		goto exit;
	}

	dev_info(dev, "%s:\n"
		"\tsci: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n"
		"\tan: %u\n"
		"\tpn: %u",
		__func__,
		rx_sa.sci[0], rx_sa.sci[1], rx_sa.sci[2], rx_sa.sci[3],
		rx_sa.sci[4], rx_sa.sci[5], rx_sa.sci[6], rx_sa.sci[7],
		rx_sa.curr_an, rx_sa.next_pn);
	dev_info(dev, "\tkey: " KEYSTR, KEY2STR(rx_sa.sak));

	mutex_lock(&macsec_pdata->lock);
	ret = osi_macsec_config(pdata->osi_core, &rx_sa, OSI_DISABLE,
				OSI_CTLR_SEL_RX, &kt_idx);
	if (ret < 0) {
		dev_err(dev, "%s: failed to disable Rx SA", __func__);
		mutex_unlock(&macsec_pdata->lock);
			goto exit;
	}
	mutex_unlock(&macsec_pdata->lock);
#ifndef MACSEC_KEY_PROGRAM
	table_config = &kt_config.table_config;
	table_config->ctlr_sel = OSI_CTLR_SEL_RX;
	table_config->rw = OSI_LUT_WRITE;
	table_config->index = kt_idx;

	ret = macsec_tz_kt_config(pdata, NV_MACSEC_CMD_TZ_CONFIG, &kt_config,
				  info, &pkcs);
	if (ret < 0) {
		dev_err(dev, "%s: failed to program SAK through TZ %d",
			__func__, ret);
		goto exit;
	}
#endif /* !MACSEC_KEY_PROGRAM */
	/* Update the macsec pdata when AN is disabled */
	macsec_pdata->macsec_rx_an_map &= ~((1U) << (rx_sa.curr_an & 0xFU));
exit:
	PRINT_EXIT();
	return ret;
}

#ifdef MACSEC_KEY_PROGRAM
/**
 * @brief hkey_generation - Generate HKey for a given SAK
 *
 * @note
 * Algorithm:
 *  - Calls Crypto APIs to generate HKey
 *
 * @param[in] sak:  Pointer to SA Key.
 * @param[out] hkey:  Pointer to HKey generated.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: No
 */
static int hkey_generation(nveu8_t *sak, nveu8_t *hkey)
{
	struct crypto_cipher *tfm;
	nveu8_t zeros[OSI_KEY_LEN_128] = {0};

	tfm = crypto_alloc_cipher("aes", 0, CRYPTO_ALG_ASYNC);
	if (!tfm)
		return -ENOMEM;

	if (crypto_cipher_setkey(tfm, sak, OSI_KEY_LEN_128))
		return -EINVAL;

	crypto_cipher_encrypt_one(tfm, hkey, zeros);
	crypto_free_cipher(tfm);
	return 0;
}
#endif /* MACSEC_KEY_PROGRAM */

static int macsec_create_rx_sa(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata;
	struct ether_priv_data *pdata;
	struct osi_macsec_sc_info rx_sa = {0};
	struct nlattr *tb_sa[NUM_NV_MACSEC_SA_ATTR];
	int ret = 0;
	unsigned short kt_idx;
	struct device *dev = NULL;
#ifndef MACSEC_KEY_PROGRAM
	int i = 0;
	struct osi_macsec_kt_config kt_config = {0};
	struct osi_macsec_table_config *table_config;
#endif /* !MACSEC_KEY_PROGRAM */
	struct nvpkcs_data pkcs = {0};

	PRINT_ENTRY();
	macsec_pdata = genl_to_macsec_pdata(info);
	if (macsec_pdata) {
		pdata = macsec_pdata->ether_pdata;
	} else {
		ret = -EPROTO;
		goto exit;
	}
	dev = pdata->dev;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    parse_sa_config(attrs, tb_sa, &rx_sa, &pkcs)) {
		dev_err(dev, "%s: failed to parse nlattrs", __func__);
		ret = -EINVAL;
		goto exit;
	}

	rx_sa.pn_window = macsec_pdata->pn_window;
	dev_info(dev, "%s:\n"
		"\tsci: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n"
		"\tan: %u\n"
		"\tpn: %u\n"
		"\tlowest pn: %u\n"
		"\twindow: %u",
		__func__,
		rx_sa.sci[0], rx_sa.sci[1], rx_sa.sci[2], rx_sa.sci[3],
		rx_sa.sci[4], rx_sa.sci[5], rx_sa.sci[6], rx_sa.sci[7],
		rx_sa.curr_an, rx_sa.next_pn, rx_sa.lowest_pn, rx_sa.pn_window);
	dev_info(dev, "\tkey: " KEYSTR, KEY2STR(rx_sa.sak));

#ifdef MACSEC_KEY_PROGRAM
	ret = hkey_generation(rx_sa.sak, rx_sa.hkey);
	if (ret != 0) {
		dev_err(dev, "%s: failed to Generate HKey", __func__);
		ret = -EINVAL;
		goto exit;
	}
	rx_sa.flags = OSI_CREATE_SA;
#endif /* MACSEC_KEY_PROGRAM */

	mutex_lock(&macsec_pdata->lock);
	ret = osi_macsec_config(pdata->osi_core, &rx_sa, OSI_ENABLE,
				OSI_CTLR_SEL_RX, &kt_idx);
	if (ret < 0) {
		dev_err(dev, "%s: failed to enable Rx SA", __func__);
		mutex_unlock(&macsec_pdata->lock);
		goto exit;
	}
	mutex_unlock(&macsec_pdata->lock);

#ifndef MACSEC_KEY_PROGRAM
	table_config = &kt_config.table_config;
	table_config->ctlr_sel = OSI_CTLR_SEL_RX;
	table_config->rw = OSI_LUT_WRITE;
	table_config->index = kt_idx;
	kt_config.flags |= OSI_LUT_FLAGS_ENTRY_VALID;

	for (i = 0; i < OSI_KEY_LEN_256; i++) {
		kt_config.entry.sak[i] = rx_sa.sak[i];
	}

	ret = macsec_tz_kt_config(pdata, NV_MACSEC_CMD_TZ_CONFIG, &kt_config,
				  info, &pkcs);
	if (ret < 0) {
		dev_err(dev, "%s: failed to program SAK through TZ %d",
			__func__, ret);
		goto exit;
	}
#endif /* !MACSEC_KEY_PROGRAM */

exit:
	PRINT_EXIT();
	return ret;
}

static int macsec_en_rx_sa(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata;
	struct ether_priv_data *pdata;
	struct osi_macsec_sc_info rx_sa = {0};
	struct nlattr *tb_sa[NUM_NV_MACSEC_SA_ATTR];
	int ret = 0;
	unsigned short kt_idx;
	struct device *dev = NULL;

	PRINT_ENTRY();
	macsec_pdata = genl_to_macsec_pdata(info);
	if (macsec_pdata) {
		pdata = macsec_pdata->ether_pdata;
	} else {
		ret = -EPROTO;
		goto exit;
	}
	dev = pdata->dev;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    parse_sa_config(attrs, tb_sa, &rx_sa, NULL)) {
		dev_err(dev, "%s: failed to parse nlattrs", __func__);
		ret = -EINVAL;
		goto exit;
	}

	rx_sa.pn_window = macsec_pdata->pn_window;
	rx_sa.flags = OSI_ENABLE_SA;
	mutex_lock(&macsec_pdata->lock);
	ret = osi_macsec_config(pdata->osi_core, &rx_sa, OSI_ENABLE,
				OSI_CTLR_SEL_RX, &kt_idx);
	if (ret < 0) {
		dev_err(dev, "%s: failed to enable Rx SA", __func__);
		mutex_unlock(&macsec_pdata->lock);
		goto exit;
	}
	mutex_unlock(&macsec_pdata->lock);
	/* Update the macsec pdata when AN is enabled */
	macsec_pdata->macsec_rx_an_map |= ((1U) << (rx_sa.curr_an & 0xFU));
exit:
	PRINT_EXIT();
	return ret;
}

static int macsec_dis_tx_sa(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata;
	struct ether_priv_data *pdata;
	struct osi_macsec_sc_info tx_sa = {0};
	struct nlattr *tb_sa[NUM_NV_MACSEC_SA_ATTR];
	int ret = 0;
	unsigned short kt_idx;
	struct device *dev = NULL;
#ifndef MACSEC_KEY_PROGRAM
	struct osi_macsec_kt_config kt_config = {0};
	struct osi_macsec_table_config *table_config;
#endif /* !MACSEC_KEY_PROGRAM */
	struct nvpkcs_data pkcs = {0};

	PRINT_ENTRY();
	macsec_pdata = genl_to_macsec_pdata(info);
	if (macsec_pdata) {
		pdata = macsec_pdata->ether_pdata;
	} else {
		ret = -EPROTO;
		goto exit;
	}
	dev = pdata->dev;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    parse_sa_config(attrs, tb_sa, &tx_sa, &pkcs)) {
		dev_err(dev, "%s: failed to parse nlattrs", __func__);
		ret = -EINVAL;
		goto exit;
	}

	dev_info(dev, "%s:\n"
		"\tsci: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n"
		"\tan: %u\n"
		"\tpn: %u",
		__func__,
		tx_sa.sci[0], tx_sa.sci[1], tx_sa.sci[2], tx_sa.sci[3],
		tx_sa.sci[4], tx_sa.sci[5], tx_sa.sci[6], tx_sa.sci[7],
		tx_sa.curr_an, tx_sa.next_pn);
	dev_info(dev, "\tkey: " KEYSTR, KEY2STR(tx_sa.sak));

	mutex_lock(&macsec_pdata->lock);
	ret = osi_macsec_config(pdata->osi_core, &tx_sa, OSI_DISABLE,
				OSI_CTLR_SEL_TX, &kt_idx);
	if (ret < 0) {
		dev_err(dev, "%s: failed to disable Tx SA", __func__);
		mutex_unlock(&macsec_pdata->lock);
		goto exit;
	}
	mutex_unlock(&macsec_pdata->lock);

#ifndef MACSEC_KEY_PROGRAM
	table_config = &kt_config.table_config;
	table_config->ctlr_sel = OSI_CTLR_SEL_TX;
	table_config->rw = OSI_LUT_WRITE;
	table_config->index = kt_idx;

	ret = macsec_tz_kt_config(pdata, NV_MACSEC_CMD_TZ_CONFIG, &kt_config,
				  info, &pkcs);
	if (ret < 0) {
		dev_err(dev, "%s: failed to program SAK through TZ %d",
			__func__, ret);
		goto exit;
	}
#endif /* !MACSEC_KEY_PROGRAM */

	/* Update the macsec pdata when AN is disbled */
	macsec_pdata->macsec_tx_an_map &= ~((1U) << (tx_sa.curr_an & 0xFU));
exit:
	PRINT_EXIT();
	return ret;
}

static int macsec_create_tx_sa(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata;
	struct ether_priv_data *pdata;
	struct osi_macsec_sc_info tx_sa = {0};
	struct nlattr *tb_sa[NUM_NV_MACSEC_SA_ATTR];
	int ret = 0;
	unsigned short kt_idx;
	struct device *dev = NULL;
#ifndef MACSEC_KEY_PROGRAM
	int i = 0;
	struct osi_macsec_kt_config kt_config = {0};
	struct osi_macsec_table_config *table_config;
#endif /* !MACSEC_KEY_PROGRAM */
	struct nvpkcs_data pkcs = {0};

	PRINT_ENTRY();
	macsec_pdata = genl_to_macsec_pdata(info);
	if (macsec_pdata) {
		pdata = macsec_pdata->ether_pdata;
	} else {
		ret = -EPROTO;
		goto exit;
	}
	dev = pdata->dev;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    parse_sa_config(attrs, tb_sa, &tx_sa, &pkcs)) {
		dev_err(dev, "%s: failed to parse nlattrs", __func__);
		ret = -EINVAL;
		goto exit;
	}

	tx_sa.pn_window = macsec_pdata->pn_window;
	dev_info(dev, "%s:\n"
		"\tsci: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n"
		"\tan: %u\n"
		"\tpn: %u",
		__func__,
		tx_sa.sci[0], tx_sa.sci[1], tx_sa.sci[2], tx_sa.sci[3],
		tx_sa.sci[4], tx_sa.sci[5], tx_sa.sci[6], tx_sa.sci[7],
		tx_sa.curr_an, tx_sa.next_pn);
	dev_info(dev, "\tkey: " KEYSTR, KEY2STR(tx_sa.sak));

#ifdef MACSEC_KEY_PROGRAM
	tx_sa.flags = OSI_CREATE_SA;
	ret = hkey_generation(tx_sa.sak, tx_sa.hkey);
	if (ret != 0) {
		dev_err(dev, "%s: failed to Generate HKey", __func__);
		ret = -EINVAL;
		goto exit;
	}
#endif /* MACSEC_KEY_PROGRAM */

	mutex_lock(&macsec_pdata->lock);
	ret = osi_macsec_config(pdata->osi_core, &tx_sa, OSI_ENABLE,
				OSI_CTLR_SEL_TX, &kt_idx);
	if (ret < 0) {
		dev_err(dev, "%s: failed to enable Tx SA", __func__);
		mutex_unlock(&macsec_pdata->lock);
		goto exit;
	}

	mutex_unlock(&macsec_pdata->lock);
#ifndef MACSEC_KEY_PROGRAM
	table_config = &kt_config.table_config;
	table_config->ctlr_sel = OSI_CTLR_SEL_TX;
	table_config->rw = OSI_LUT_WRITE;
	table_config->index = kt_idx;
	kt_config.flags |= OSI_LUT_FLAGS_ENTRY_VALID;

	for (i = 0; i < OSI_KEY_LEN_256; i++) {
		kt_config.entry.sak[i] = tx_sa.sak[i];
	}

	ret = macsec_tz_kt_config(pdata, NV_MACSEC_CMD_TZ_CONFIG, &kt_config,
				  info, &pkcs);
	if (ret < 0) {
		dev_err(dev, "%s: failed to program SAK through TZ %d",
			__func__, ret);
		goto exit;
	}
#endif /* !MACSEC_KEY_PROGRAM */

exit:
	PRINT_EXIT();
	return ret;
}

static int macsec_en_tx_sa(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata;
	struct ether_priv_data *pdata;
	struct osi_macsec_sc_info tx_sa = {0};
	struct nlattr *tb_sa[NUM_NV_MACSEC_SA_ATTR];
	int ret = 0;
	unsigned short kt_idx;
	struct device *dev = NULL;

	PRINT_ENTRY();
	macsec_pdata = genl_to_macsec_pdata(info);
	if (macsec_pdata) {
		pdata = macsec_pdata->ether_pdata;
	} else {
		ret = -EPROTO;
		goto exit;
	}
	dev = pdata->dev;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    parse_sa_config(attrs, tb_sa, &tx_sa, NULL)) {
		dev_err(dev, "%s: failed to parse nlattrs", __func__);
		ret = -EINVAL;
		goto exit;
	}

	tx_sa.pn_window = macsec_pdata->pn_window;
	tx_sa.flags = OSI_ENABLE_SA;
	mutex_lock(&macsec_pdata->lock);
	ret = osi_macsec_config(pdata->osi_core, &tx_sa, OSI_ENABLE,
				OSI_CTLR_SEL_TX, &kt_idx);
	if (ret < 0) {
		dev_err(dev, "%s: failed to enable Tx SA", __func__);
		mutex_unlock(&macsec_pdata->lock);
		goto exit;
	}

	mutex_unlock(&macsec_pdata->lock);
	/* Update the macsec pdata when AN is enabled */
	macsec_pdata->macsec_tx_an_map |= ((1U) << (tx_sa.curr_an & 0xFU));
exit:
	PRINT_EXIT();
	return ret;
}

static int macsec_deinit(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata = NULL;
	struct macsec_supplicant_data *supplicant;
	struct ether_priv_data *pdata;
	int ret = 0;

	PRINT_ENTRY();

	if (!attrs[NV_MACSEC_ATTR_IFNAME]) {
		ret = -EINVAL;
		goto exit;
	}

	macsec_pdata = genl_to_macsec_pdata(info);
	if (!macsec_pdata) {
		pr_err("%s: failed to get macsec_pdata", __func__);
		ret = -EPROTO;
		goto exit;
	}
	pdata = macsec_pdata->ether_pdata;

	mutex_lock(&macsec_pdata->lock);
	supplicant = macsec_get_supplicant(macsec_pdata, info->snd_portid);

	if (!supplicant) {
		ret = -EPROTO;
		mutex_unlock(&macsec_pdata->lock);
		dev_err(pdata->dev, "%s: failed to get supplicant data",
			__func__);
		goto exit;
	}

	supplicant->snd_portid = OSI_NONE;
	supplicant->in_use = OSI_NONE;
	macsec_pdata->next_supp_idx--;

	/* check for reference count to zero before deinit macsec */
	if ((atomic_read(&macsec_pdata->ref_count) - 1) > 0) {
		ret = 0;
		mutex_unlock(&macsec_pdata->lock);
		goto done;
	}

	mutex_unlock(&macsec_pdata->lock);

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(pdata->dev, "%s: MAC interface down!!", __func__);
		goto done;
	}

	ret = macsec_close(macsec_pdata);
	if (ret < 0) {
		ret = -EPROTO;
		goto exit;
	}
done:
	if (atomic_read(&macsec_pdata->ref_count) > 0) {
		atomic_dec(&macsec_pdata->ref_count);
	}
	dev_info(pdata->dev, "%s: ref_count %d", __func__,
		 atomic_read(&macsec_pdata->ref_count));
exit:
	PRINT_EXIT();
	return ret;
}

static int macsec_init(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata = NULL;
	struct macsec_supplicant_data *supplicant;
	struct ether_priv_data *pdata = NULL;
	struct device *dev = NULL;
	int ret = 0;

	PRINT_ENTRY();

	if (!attrs[NV_MACSEC_ATTR_IFNAME]) {
		ret = -EINVAL;
		goto exit;
	}

	macsec_pdata = genl_to_macsec_pdata(info);
	if (!macsec_pdata) {
		ret = -EPROTO;
		pr_err("%s: failed to get macsec_pdata", __func__);
		goto exit;
	}

	pdata = macsec_pdata->ether_pdata;
	dev = pdata->dev;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}
	mutex_lock(&macsec_pdata->lock);
	/* only one supplicant is allowed per VF */
	if (macsec_pdata->next_supp_idx >= MAX_SUPPLICANTS_ALLOWED) {
		ret = -EPROTO;
		mutex_unlock(&macsec_pdata->lock);
		dev_err(dev, "%s: Reached max supported supplicants %u", __func__,
			macsec_pdata->next_supp_idx);
		goto exit;
	}

	supplicant = macsec_get_supplicant(macsec_pdata, info->snd_portid);
	if (!supplicant) {
		supplicant = &macsec_pdata->supplicant[macsec_pdata->next_supp_idx];
		macsec_pdata->next_supp_idx++;
	}

	supplicant->snd_portid = info->snd_portid;
	supplicant->in_use = OSI_ENABLE;

	/* check reference count and if macsec already init'd return success  */
	if (atomic_read(&macsec_pdata->ref_count) > 0) {
		ret = 0;
		mutex_unlock(&macsec_pdata->lock);
		goto done;
	}

	mutex_unlock(&macsec_pdata->lock);
	ret = macsec_open(macsec_pdata, info);
	if (ret < 0) {
		ret = -EPROTO;
		goto exit;
	}
	macsec_pdata->macsec_rx_an_map = 0U;
	macsec_pdata->macsec_tx_an_map = 0U;
done:
	atomic_inc(&macsec_pdata->ref_count);
	dev_info(dev, "%s: ref_count %d", __func__,
		 atomic_read(&macsec_pdata->ref_count));
exit:
	PRINT_EXIT();
	return ret;
}

static int macsec_set_replay_prot(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	unsigned int replay_prot, window;
	struct macsec_priv_data *macsec_pdata = NULL;
	struct ether_priv_data *pdata = NULL;
	struct device *dev = NULL;
	int ret = 0;

	PRINT_ENTRY();

	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    !attrs[NV_MACSEC_ATTR_REPLAY_PROT_EN] ||
	    !attrs[NV_MACSEC_ATTR_REPLAY_WINDOW]) {
		ret = -EINVAL;
		goto exit;
	}

	replay_prot = nla_get_u32(attrs[NV_MACSEC_ATTR_REPLAY_PROT_EN]);
	window = nla_get_u32(attrs[NV_MACSEC_ATTR_REPLAY_WINDOW]);
	macsec_pdata = genl_to_macsec_pdata(info);

	if (!macsec_pdata) {
		ret = -EPROTO;
		pr_err("%s: failed to get macsec_pdata", __func__);
		goto exit;
	}

	pdata = macsec_pdata->ether_pdata;
	dev = pdata->dev;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(pdata->dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	/* If Replay protection is disabled from supplicant use maximum
	 * PN window as replay protecion is already enabled in macsec_init
	 */
	if (replay_prot == OSI_ENABLE)
		macsec_pdata->pn_window = window;
	else
		macsec_pdata->pn_window = OSI_PN_MAX_DEFAULT;

exit:
	PRINT_EXIT();
	return ret;
}

static const struct genl_ops nv_macsec_genl_ops[] = {
	{
		.cmd = NV_MACSEC_CMD_INIT,
		.doit = macsec_init,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_SET_PROT_FRAMES,
		.doit = macsec_set_prot_frames,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_SET_REPLAY_PROT,
		.doit = macsec_set_replay_prot,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_SET_CIPHER,
		.doit = macsec_set_cipher,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_SET_CONTROLLED_PORT,
		.doit = macsec_set_controlled_port,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_DEINIT,
		.doit = macsec_deinit,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_EN_TX_SA,
		.doit = macsec_en_tx_sa,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_CREATE_TX_SA,
		.doit = macsec_create_tx_sa,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_DIS_TX_SA,
		.doit = macsec_dis_tx_sa,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_EN_RX_SA,
		.doit = macsec_en_rx_sa,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_CREATE_RX_SA,
		.doit = macsec_create_rx_sa,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_DIS_RX_SA,
		.doit = macsec_dis_rx_sa,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = NV_MACSEC_CMD_GET_TX_NEXT_PN,
		.doit = macsec_get_tx_next_pn,
		.flags = GENL_ADMIN_PERM,
	},
};

void macsec_remove(struct ether_priv_data *pdata)
{
	struct macsec_priv_data *macsec_pdata = NULL;
	struct macsec_supplicant_data *supplicant = NULL;
	int i;

	PRINT_ENTRY();
	macsec_pdata = pdata->macsec_pdata;
	if (macsec_pdata) {
		mutex_lock(&macsec_pdata->lock);
		/* Delete if any supplicant active heartbeat timer */
		supplicant = macsec_pdata->supplicant;
		for (i = 0; i < OSI_MAX_NUM_SC; i++) {
			if (supplicant[i].in_use == OSI_ENABLE) {
				supplicant->snd_portid = OSI_NONE;
				supplicant->in_use = OSI_NONE;
			}
		}
		mutex_unlock(&macsec_pdata->lock);
		/* if macsec_close() is not called by supplicant gracefully
		 * close it now.
		 */
		if (atomic_read(&macsec_pdata->ref_count) > 0) {
			macsec_close(macsec_pdata);
		}

		/* Unregister generic netlink */
		if (macsec_pdata->is_nv_macsec_fam_registered == OSI_ENABLE) {
			genl_unregister_family(&macsec_pdata->nv_macsec_fam);
			macsec_pdata->is_nv_macsec_fam_registered = OSI_DISABLE;
		}

		/* Release platform resources */
		macsec_release_platform_res(macsec_pdata);
		/* free macsec priv */
		devm_kfree(pdata->dev, macsec_pdata);
	}
	PRINT_EXIT();
}

int macsec_probe(struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct macsec_priv_data *macsec_pdata = NULL;
	struct resource *res = NULL;
	struct device_node *np = dev->of_node;
	int ret = 0;
#ifdef MACSEC_KEY_PROGRAM
	unsigned long tz_addr = 0;
#endif

	PRINT_ENTRY();
	/* Check if MACsec is enabled in DT, if so map the I/O base addr */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "macsec-base");
	if (res) {
		osi_core->macsec_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(osi_core->macsec_base)) {
			dev_err(dev, "failed to ioremap MACsec base addr\n");
			ret = PTR_ERR(osi_core->macsec_base);
			goto exit;
		}
#ifdef MACSEC_KEY_PROGRAM
		/* store TZ window base address */
		tz_addr = (res->start - MACSEC_SIZE);
#endif
	} else {
		/* MACsec not supported per DT config, nothing more to do */
		osi_core->macsec_base = NULL;
		osi_core->tz_base = NULL;
		pdata->macsec_pdata = NULL;
		/* Return positive value to indicate MACsec not enabled in DT */
		ret = 1;
		goto exit;
	}

#ifdef MACSEC_KEY_PROGRAM
	osi_core->tz_base = devm_ioremap(dev, tz_addr, MACSEC_SIZE);
	if (IS_ERR(osi_core->tz_base)) {
		dev_err(dev, "failed to ioremap TZ base addr\n");
		ret = PTR_ERR(osi_core->tz_base);
		goto exit;
	}
#endif
	/* Alloc macsec priv data structure */
	macsec_pdata = devm_kzalloc(dev, sizeof(struct macsec_priv_data),
				    GFP_KERNEL);
	if (macsec_pdata == NULL) {
		dev_err(dev, "failed to alloc macsec_priv_data\n");
		ret = -ENOMEM;
		goto exit;
	}
	macsec_pdata->ether_pdata = pdata;
	pdata->macsec_pdata = macsec_pdata;

	/* Read if macsec is enabled in DT */
	ret = of_property_read_u32(np, "nvidia,macsec-enable",
				   &macsec_pdata->is_macsec_enabled_in_dt);
	if ((ret != 0) || (macsec_pdata->is_macsec_enabled_in_dt == 0U)) {
		dev_info(dev,
			 "macsec param in DT is missing or disabled\n");
		ret = 1;
		goto init_err;
	}

	mutex_init(&pdata->macsec_pdata->lock);

	/* Read MAC instance id and used in TZ api's */
	ret = of_property_read_u32(np, "nvidia,instance_id", &macsec_pdata->id);
	if (ret != 0) {
		dev_info(dev,
			 "DT instance_id missing, setting default to MGBE0\n");
		macsec_pdata->id = 0;
	}

	osi_core->instance_id = macsec_pdata->id;

	/* Get OSI MACsec ops */
	if (osi_init_macsec_ops(osi_core) != 0) {
		dev_err(dev, "osi_init_macsec_ops failed\n");
		ret = -1;
		goto init_err;
	}

	/* Get platform resources - clks, resets, irqs.
	 * CAR is not enabled and irqs not requested until macsec_init()
	 */
	ret = macsec_get_platform_res(macsec_pdata);
	if (ret < 0) {
		dev_err(dev, "macsec_get_platform_res failed\n");
		goto init_err;
	}

	/* Enable CAR */
	if (osi_core->use_virtualization == OSI_DISABLE) {
		ret = macsec_enable_car(macsec_pdata);
		if (ret < 0) {
			dev_err(dev, "Unable to enable macsec clks & reset\n");
			goto car_err;
		}
	}

	/* Register macsec generic netlink ops */
	macsec_pdata->nv_macsec_fam.hdrsize = 0;
	macsec_pdata->nv_macsec_fam.version = NV_MACSEC_GENL_VERSION;
	macsec_pdata->nv_macsec_fam.maxattr = NV_MACSEC_ATTR_MAX;
	macsec_pdata->nv_macsec_fam.module = THIS_MODULE;
	macsec_pdata->nv_macsec_fam.ops = nv_macsec_genl_ops;
	macsec_pdata->nv_macsec_fam.n_ops = ARRAY_SIZE(nv_macsec_genl_ops);
	if (macsec_pdata->is_nv_macsec_fam_registered == OSI_DISABLE) {
		if (strlen(netdev_name(pdata->ndev)) >= GENL_NAMSIZ) {
			dev_err(dev, "Intf name %s of len %lu exceed nl_family name size\n",
				netdev_name(pdata->ndev),
				strlen(netdev_name(pdata->ndev)));
			ret = -1;
			goto genl_err;
		} else {
			strncpy(macsec_pdata->nv_macsec_fam.name,
				netdev_name(pdata->ndev), GENL_NAMSIZ - 1);
		}
		ret = genl_register_family(&macsec_pdata->nv_macsec_fam);
			if (ret) {
				dev_err(dev, "Failed to register GENL ops %d\n",
					ret);
				goto genl_err;
			}

			macsec_pdata->is_nv_macsec_fam_registered = OSI_ENABLE;
	}

	PRINT_EXIT();
	return ret;
genl_err:
	if (osi_core->use_virtualization == OSI_DISABLE) {
		macsec_disable_car(macsec_pdata);
	}
car_err:
	macsec_release_platform_res(macsec_pdata);
init_err:
	devm_kfree(dev, pdata->macsec_pdata);
exit:
	PRINT_EXIT();
	return ret;
}

#ifndef MACSEC_KEY_PROGRAM
/**
 * @brief macsec_tz_kt_config - Program macsec key table entry.
 *
 * @param[in] priv: OSD private data structure.
 * @param[in] cmd: macsec TZ config cmd
 * @param[in] kt_config: Pointer to osi_macsec_kt_config structure
 * @param[in] info: Pointer to netlink msg structure
 *
 * @retval 0 on success
 * @retval negative value on failure.
 */
static int macsec_tz_kt_config(struct ether_priv_data *pdata,
			unsigned char cmd,
			struct osi_macsec_kt_config *const kt_config,
			struct genl_info *const info, struct nvpkcs_data *pkcs)
{
	struct sk_buff *msg;
	struct nlattr *nest;
	void *msg_head;
	int ret = 0;
	struct device *dev = pdata->dev;
	struct macsec_priv_data *macsec_pdata = pdata->macsec_pdata;

	PRINT_ENTRY();
	if (info == OSI_NULL) {
		dev_info(dev,"Can not config key through TZ, genl_info NULL\n");
		/* return success, as info can be NULL if called from
		 * sysfs calls
		 */
		ret = 0;
		goto fail;
	}

	if (cmd != NV_MACSEC_CMD_TZ_KT_RESET &&
	    cmd != NV_MACSEC_CMD_TZ_CONFIG) {
		dev_err(dev, "%s: Wrong TZ cmd %d\n", __func__, cmd);
		ret = -1;
		goto fail;
	}

	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (msg == NULL) {
		dev_err(dev, "Unable to alloc genl reply\n");
		ret = -ENOMEM;
		goto fail;
	}

	msg_head = genlmsg_put_reply(msg, info, &macsec_pdata->nv_macsec_fam, 0, cmd);
	if (msg_head == NULL) {
		dev_err(dev, "unable to get replyhead\n");
		ret = -EINVAL;
		goto failure;
	}
	if (cmd == NV_MACSEC_CMD_TZ_KT_RESET) {
		nest = nla_nest_start(msg, NV_MACSEC_ATTR_TZ_KT_RESET);
		if (!nest) {
			ret = EINVAL;
			goto failure;
		}
		nla_put_u32(msg, NV_MACSEC_TZ_KT_RESET_INSTANCE_ID,
			    macsec_pdata->id);
		nla_nest_end(msg, nest);
	}

	if (cmd == NV_MACSEC_CMD_TZ_CONFIG && kt_config != NULL) {
		/* pr_err("%s: ctrl: %hu rw: %hu idx: %hu flags: %#x\n"
		 *	  __func__,
		 *	 kt_config->table_config.ctlr_sel,
		 *	 kt_config->table_config.rw,
		 *	 kt_config->table_config.index,
		 *	 kt_config->flags);
		 */

		nest = nla_nest_start(msg, NV_MACSEC_ATTR_TZ_CONFIG);
		if (!nest) {
			ret = EINVAL;
			goto failure;
		}
		nla_put_u32(msg, NV_MACSEC_TZ_INSTANCE_ID, macsec_pdata->id);
		nla_put_u8(msg, NV_MACSEC_TZ_ATTR_CTRL,
			   kt_config->table_config.ctlr_sel);
		nla_put_u8(msg, NV_MACSEC_TZ_ATTR_RW,
			   kt_config->table_config.rw);
		nla_put_u8(msg, NV_MACSEC_TZ_ATTR_INDEX,
			   kt_config->table_config.index);
		nla_put_u32(msg, NV_MACSEC_TZ_ATTR_FLAG, kt_config->flags);
#ifdef NVPKCS_MACSEC
		if (pkcs) {
			nla_put(msg, NV_MACSEC_TZ_PKCS_KEY_WRAP,
				sizeof(pkcs->nv_key),
				pkcs->nv_key);
			nla_put_u64_64bit(msg, NV_MACSEC_TZ_PKCS_KEK_HANDLE,
					  pkcs->nv_kek,
					  NL_POLICY_TYPE_ATTR_PAD);
		}
#else
		nla_put(msg, NV_MACSEC_TZ_ATTR_KEY, OSI_KEY_LEN_256,
			kt_config->entry.sak);
#endif /* NVPKCS_MACSEC */
		nla_nest_end(msg, nest);
	}
	genlmsg_end(msg, msg_head);
	ret = genlmsg_reply(msg, info);
	if (ret != 0) {
		dev_err(dev, "Unable to send reply\n");
	}

	PRINT_EXIT();
	return ret;
failure:
	nlmsg_free(msg);
fail:
	PRINT_EXIT();
	return ret;
}
#endif /* MACSEC_KEY_PROGRAM */

static int macsec_get_tx_next_pn(struct sk_buff *skb, struct genl_info *info)
{
	struct nlattr **attrs = info->attrs;
	struct macsec_priv_data *macsec_pdata;
	struct ether_priv_data *pdata;
	struct osi_macsec_sc_info tx_sa;
	struct nlattr *tb_sa[NUM_NV_MACSEC_SA_ATTR];
	int ret = 0;
	struct device *dev = NULL;
	unsigned char cmd;
	struct sk_buff *msg;
	void *msg_head;
	struct nlattr *nest;
	struct osi_macsec_lut_config lut_config = {0};
	unsigned int  key_index = 0;
	struct osi_core_priv_data *osi_core = NULL;

	PRINT_ENTRY();

	macsec_pdata = genl_to_macsec_pdata(info);
	if (macsec_pdata) {
		pdata = macsec_pdata->ether_pdata;
	} else {
		ret = -EPROTO;
		goto exit;
	}
	dev = pdata->dev;
	osi_core = pdata->osi_core;

	if (!netif_running(pdata->ndev)) {
		ret = -ENETDOWN;
		dev_err(dev, "%s: MAC interface down!!\n", __func__);
		goto exit;
	}

	if (!attrs[NV_MACSEC_ATTR_IFNAME] ||
	    parse_sa_config(attrs, tb_sa, &tx_sa, NULL)) {
		dev_err(dev, "%s: failed to parse nlattrs", __func__);
		ret = -EINVAL;
		goto exit;
	}

	ret = osi_macsec_get_sc_lut_key_index(osi_core, tx_sa.sci, &key_index,
					      OSI_CTLR_SEL_TX);
	if (ret < 0) {
		dev_err(dev, "Failed to get Key_index\n");
		goto exit;
	}

	memset(&lut_config, OSI_NONE, sizeof(lut_config));
	lut_config.table_config.ctlr_sel = OSI_CTLR_SEL_TX;
	lut_config.table_config.rw = OSI_LUT_READ;
	lut_config.table_config.index = key_index + tx_sa.curr_an;
	lut_config.lut_sel = OSI_LUT_SEL_SA_STATE;
	if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
		pr_err("%s: Failed to read SA STATE LUT\n", __func__);
		goto exit;
	}

	cmd = NV_MACSEC_CMD_GET_TX_NEXT_PN;
	msg = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!msg) {
		dev_err(dev, "Unable to alloc genl reply\n");
		ret = -ENOMEM;
		goto exit;
	}

	msg_head = genlmsg_put_reply(msg, info, &macsec_pdata->nv_macsec_fam, 0, cmd);
	if (!msg_head) {
		dev_err(dev, "unable to get replyhead\n");
		ret = -EINVAL;
		goto failure;
	}
	nest = nla_nest_start(msg, NV_MACSEC_ATTR_SA_CONFIG);
	if (!nest) {
		ret = -EINVAL;
		goto failure;
	}
	nla_put_u32(msg, NV_MACSEC_SA_ATTR_PN, lut_config.sa_state_out.next_pn);
	nla_put_u8(msg, NV_MACSEC_SA_ATTR_AN, tx_sa.curr_an);
	nla_put(msg, NV_MACSEC_SA_ATTR_SCI, OSI_SCI_LEN, tx_sa.sci);
	nla_nest_end(msg, nest);
	genlmsg_end(msg, msg_head);
	ret = genlmsg_reply(msg, info);
	if (ret != 0)
		dev_err(dev, "Unable to send reply\n");

	PRINT_EXIT();
	return ret;
failure:
	nlmsg_free(msg);
exit:
	PRINT_EXIT();
	return ret;
}
#endif /* MACSEC_SUPPORT */
