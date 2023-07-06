/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
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

#include "ether_linux.h"
#include "macsec.h"

#if (IS_ENABLED(CONFIG_TEGRA_HSIERRRPTINJ))
#include <linux/tegra-hsierrrptinj.h>
#endif

#ifdef CONFIG_DEBUG_FS
/* As per IAS Docs */
#define EOQS_MAX_REGISTER_ADDRESS 0x12FC
#endif

#ifdef OSI_DEBUG
/**
 * @brief Shows the current setting of descriptor dump
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current MAC loopback setting
 */
static ssize_t ether_desc_dump_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 (osi_dma->enable_desc_dump == 1U) ?
			 "enabled" : "disabled");
}

/**
 * @brief Set the user setting for enable_desc_dump
 *
 * Algorithm: This is used to update osi_dma->enable_desc_dump
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of MAC loopback
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t ether_desc_dump_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;

	if (strncmp(buf, "enable", 6) == 0U) {
		osi_dma->enable_desc_dump = 1U;
	} else if (strncmp(buf, "disable", 7) == 0U) {
		osi_dma->enable_desc_dump = 0U;
	} else {
		dev_err(pdata->dev,
			"Invalid entry. Valid Entries are enable or disable\n");
	}

	return size;
}

/**
 * @brief Sysfs attribute for enable descriptor dump
 *
 */
static DEVICE_ATTR(desc_dump_enable, (S_IRUGO | S_IWUSR),
		   ether_desc_dump_show,
		   ether_desc_dump_store);
#endif /* OSI_DEBUG */

/**
 * @brief Shows the current setting of MAC loopback
 *
 * Algorithm: Display the current MAC loopback setting.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current MAC loopback setting
 *
 * @note MAC and PHY need to be initialized.
 */
static ssize_t ether_mac_loopback_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 (pdata->mac_loopback_mode == 1U) ?
			 "enabled" : "disabled");
}

/**
 * @brief Set the user setting of MAC loopback mode
 *
 * Algorithm: This is used to set the user mode settings of MAC loopback.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of MAC loopback
 * @param[in] size: size of buffer
 *
 * @note MAC and PHY need to be initialized.
 *
 * @return size of buffer.
 */
static ssize_t ether_mac_loopback_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct phy_device *phydev = ndev->phydev;
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_ioctl ioctl_data = {};
	int ret = -1;

	/* Interface is not up so LB mode can't be set */
	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	if (strncmp(buf, "enable", 6) == 0U) {
		if (!phydev->link) {
			/* If no phy link, then turn on carrier explicitly so
			 * that nw stack can send packets. If PHY link is
			 * present, PHY framework would have already taken care
			 * of netif_carrier_* status.
			 */
			netif_carrier_on(ndev);
		}
		/* Enabling the MAC Loopback Mode */
		ioctl_data.arg1_u32 = OSI_ENABLE;
		ioctl_data.cmd = OSI_CMD_MAC_LB;
		ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
		if (ret < 0) {
			dev_err(pdata->dev, "Enabling MAC Loopback failed\n");
		} else {
			pdata->mac_loopback_mode = 1;
			dev_info(pdata->dev, "Enabling MAC Loopback\n");
		}
	} else if (strncmp(buf, "disable", 7) == 0U) {
		if (!phydev->link) {
			/* If no phy link, then turn off carrier explicitly so
			 * that nw stack doesn't send packets. If PHY link is
			 * present, PHY framework would have already taken care
			 * of netif_carrier_* status.
			 */
			netif_carrier_off(ndev);
		}
		/* Disabling the MAC Loopback Mode */
		ioctl_data.arg1_u32 = OSI_DISABLE;
		ioctl_data.cmd = OSI_CMD_MAC_LB;
		ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
		if (ret < 0) {
			dev_err(pdata->dev, "Disabling MAC Loopback failed\n");
		} else {
			pdata->mac_loopback_mode = 0;
			dev_info(pdata->dev, "Disabling MAC Loopback\n");
		}
	} else {
		dev_err(pdata->dev,
			"Invalid entry. Valid Entries are enable or disable\n");
	}

	return size;
}

#ifdef MACSEC_SUPPORT

/**
 * @brief Shows the current setting of MACsec AN status
 *
 * Algorithm: Display the current MACsec AN enable status
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current macsec an status
 */
static ssize_t macsec_an_status_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct macsec_priv_data *macsec_pdata = pdata->macsec_pdata;
	unsigned int macsec_status = 0;

	if ((macsec_pdata->macsec_tx_an_map != 0U) &&
	    (macsec_pdata->macsec_rx_an_map != 0U)) {
		macsec_status = OSI_ENABLE;
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 (macsec_status == OSI_ENABLE) ?
			 "1" : "0");
}

/**
 * @brief Sysfs attribute for MACsec irq stats
 *
 */
static DEVICE_ATTR(macsec_an_status, (S_IRUGO | S_IWUSR),
		   macsec_an_status_show,
		   NULL);

/**
 * @brief Shows the current setting of MACsec controllers enabled
 *
 * Algorithm: Display the current MACsec controllers enabled (Tx/Rx).
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current MACsec loopback setting
 */
static ssize_t macsec_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct macsec_priv_data *macsec_pdata = pdata->macsec_pdata;
	unsigned int enabled = macsec_pdata->enabled;
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 (enabled == (OSI_MACSEC_TX_EN | OSI_MACSEC_RX_EN))
				      ? "txrx" :
			 (enabled == OSI_MACSEC_TX_EN) ? "tx" :
			 (enabled == OSI_MACSEC_RX_EN) ? "rx" :
			 "None");
}

extern int macsec_open(struct macsec_priv_data *macsec_pdata,
		       void *const genl_info);
extern int macsec_close(struct macsec_priv_data *macsec_pdata);

/**
 * @brief Set the MACsec controller enabled (Tx/Rx)
 *
 * Algorithm: This is used to set the Tx/Rx MACsec controller enabled.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of MACsec loopback
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct macsec_priv_data *macsec_pdata = pdata->macsec_pdata;
	int ret = 0;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	if (strncmp(buf, "0", 1) == OSI_NONE) {
		ret = macsec_close(macsec_pdata);
	} else if (strncmp(buf, "txrx", 4) == OSI_NONE) {
		ret = macsec_open(macsec_pdata, OSI_NULL);
	} else {
		dev_err(pdata->dev,
			"Invalid. Valid inputs are 0/tx/rx/txrx\n");
	}

	return size;
}

/**
 * @brief Sysfs attribute for MACsec enable
 *
 */
static DEVICE_ATTR(macsec_enable, (S_IRUGO | S_IWUSR),
		   macsec_enable_show,
		   macsec_enable_store);

/**
 * @brief Shows the current setting of MACsec cipther set
 *
 * Algorithm: Display the current MACsec cipher setting.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current MACsec cipher setting
 */
static ssize_t macsec_cipher_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct macsec_priv_data *macsec_pdata = pdata->macsec_pdata;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 (macsec_pdata->cipher == OSI_MACSEC_CIPHER_AES128) ?
			 "aes128" : "aes256");
}

/**
 * @brief Set the user setting of MACsec AES cipher
 *
 * Algorithm: This is used to set the user mode settings of MACsec cipther.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of MACsec cipher
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_cipher_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct macsec_priv_data *macsec_pdata = pdata->macsec_pdata;
	int ret = 0;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	if (strncmp(buf, "aes128", 6) == OSI_NONE) {
		ret = osi_macsec_cipher_config(pdata->osi_core,
					       OSI_MACSEC_CIPHER_AES128);
		if (ret < 0) {
			dev_err(pdata->dev, "Failed to set macsec cipher\n");
		} else {
			macsec_pdata->cipher = OSI_MACSEC_CIPHER_AES128;
			dev_info(pdata->dev, "macsec cipher aes128 enabled\n");
		}
	} else if (strncmp(buf, "aes256", 6) == OSI_NONE) {
		ret = osi_macsec_cipher_config(pdata->osi_core,
					       OSI_MACSEC_CIPHER_AES256);
		if (ret < 0) {
			dev_err(pdata->dev, "Failed to set macsec cipher\n");
		} else {
			macsec_pdata->cipher = OSI_MACSEC_CIPHER_AES256;
			dev_info(pdata->dev, "macsec cipher aes256 enabled\n");
		}
	} else {
		dev_err(pdata->dev,
			"Invalid entry. Valid Entries are aes128/aes256\n");
	}

	return size;
}

/**
 * @brief Sysfs attribute for MACsec cipher
 *
 */
static DEVICE_ATTR(macsec_cipher, (S_IRUGO | S_IWUSR),
		   macsec_cipher_show,
		   macsec_cipher_store);

#ifdef DEBUG_MACSEC
/**
 * @brief Shows the current setting of MACsec loopback
 *
 * Algorithm: Display the current MACsec loopback setting.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current MACsec loopback setting
 */
static ssize_t macsec_loopback_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct macsec_priv_data *macsec_pdata = pdata->macsec_pdata;
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 (macsec_pdata->loopback_mode == OSI_ENABLE) ?
			 "enabled" : "disabled");
}

/**
 * @brief Set the user setting of MACsec loopback mode
 *
 * Algorithm: This is used to set the user mode settings of MACsec loopback.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of MACsec loopback
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_loopback_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct macsec_priv_data *macsec_pdata = pdata->macsec_pdata;
	int ret = 0;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	if (strncmp(buf, "enable", 6) == OSI_NONE) {
		ret = osi_macsec_loopback(pdata->osi_core, OSI_ENABLE);
		if (ret < 0) {
			dev_err(pdata->dev,
				 "Failed to enable macsec loopback\n");
		} else {
			macsec_pdata->loopback_mode = OSI_ENABLE;
			dev_info(pdata->dev, "Enabled macsec Loopback\n");
		}
	} else if (strncmp(buf, "disable", 7) == OSI_NONE) {
		ret = osi_macsec_loopback(pdata->osi_core, OSI_DISABLE);
		if (ret < 0) {
			dev_err(pdata->dev,
				 "Failed to Disable macsec loopback\n");
		} else {
			macsec_pdata->loopback_mode = OSI_DISABLE;
			dev_info(pdata->dev, "Disabled macsec Loopback\n");
		}
	} else if (strncmp(buf, "carrier_on", 10) == OSI_NONE) {
		netif_carrier_on(ndev);
	} else if (strncmp(buf, "carrier_off", 11) == OSI_NONE) {
		netif_carrier_off(ndev);
	} else {
		dev_err(pdata->dev,
			"Invalid entry. Valid Entries are "
			"enable/disable/carrier_on/carrier_off\n");
	}

	return size;
}

/**
 * @brief Sysfs attribute for MACsec loopback
 *
 */
static DEVICE_ATTR(macsec_loopback, (S_IRUGO | S_IWUSR),
		   macsec_loopback_show,
		   macsec_loopback_store);
#endif /* DEBUG_MACSEC */

#ifdef HSI_SUPPORT
#if (IS_ENABLED(CONFIG_TEGRA_HSIERRRPTINJ))
static int hsi_inject_err_fsi(unsigned int inst_id,
			      struct epl_error_report_frame error_report,
			      void *data)
{
	struct ether_priv_data *pdata = (struct ether_priv_data *)data;
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_ioctl ioctl_data = {};
	int ret;

	ioctl_data.cmd = OSI_CMD_HSI_INJECT_ERR;
	ioctl_data.arg1_u32 = error_report.error_code;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0)
		dev_err(pdata->dev, "Fail to inject error\n");

	return ret;
}
#endif

/**
 * @brief Shows HSI feature enabled status
 *
 * Algorithm: Shows HSI feature enabled status
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current status
 */
static ssize_t hsi_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;

	if (osi_core->use_virtualization == OSI_ENABLE) {
		dev_err(pdata->dev, "Not supported with Ethernet virtualization enabled\n");
		return 0;
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 (osi_core->hsi.enabled == OSI_ENABLE) ?
			 "enabled" : "disabled");
}

/**
 * @brief Set HSI enabled status
 *
 * Algorithm: This is used to set HSI feature enable status
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user input
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t hsi_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_ioctl ioctl_data = {};
	int ret = 0;
#if (IS_ENABLED(CONFIG_TEGRA_HSIERRRPTINJ))
	u32 inst_id = osi_core->instance_id;
	u32 ip_type[2] = {IP_EQOS, IP_MGBE};
#endif
	if (osi_core->use_virtualization == OSI_ENABLE) {
		dev_err(pdata->dev, "Not supported with Ethernet virtualization enabled\n");
		return size;
	}

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	ioctl_data.cmd = OSI_CMD_HSI_CONFIGURE;
	if (strncmp(buf, "enable", 6) == OSI_NONE) {
		ioctl_data.arg1_u32 = OSI_ENABLE;
		ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
		if (ret < 0) {
			dev_err(pdata->dev,
				"Failed to enable HSI\n");
		} else {
			osi_core->hsi.enabled = OSI_ENABLE;
			dev_info(pdata->dev, "HSI Enabled\n");
#if (IS_ENABLED(CONFIG_TEGRA_HSIERRRPTINJ))
			if (osi_core->instance_id == OSI_INSTANCE_ID_EQOS)
				inst_id = 0;

			ret = hsierrrpt_reg_cb(ip_type[osi_core->mac], inst_id,
					       hsi_inject_err_fsi, pdata);
			if (ret != 0) {
				dev_err(pdata->dev, "Err inj callback registration failed: %d",
					ret);
			}
#endif
		}
	} else if (strncmp(buf, "disable", 7) == OSI_NONE) {
		ioctl_data.arg1_u32 = OSI_DISABLE;
		ret = osi_handle_ioctl(pdata->osi_core, &ioctl_data);
		if (ret < 0) {
			dev_err(pdata->dev,
				"Failed to disable HSI\n");
		} else {
			osi_core->hsi.enabled = OSI_DISABLE;
			dev_info(pdata->dev, "HSI Disabled\n");
#if (IS_ENABLED(CONFIG_TEGRA_HSIERRRPTINJ))
			if (osi_core->instance_id == OSI_INSTANCE_ID_EQOS)
				inst_id = 0;

			ret = hsierrrpt_dereg_cb(ip_type[osi_core->mac], inst_id);
			if (ret != 0) {
				dev_err(pdata->dev, "Err inj callback deregistration failed: %d",
					ret);
			}
#endif
		}
	} else {
		dev_err(pdata->dev,
			"Invalid entry. Valid Entries are enable/disable\n");
	}

	return size;
}

/**
 * @brief Sysfs attribute for HSI enable
 *
 */
static DEVICE_ATTR(hsi_enable, 0644,
		   hsi_enable_show,
		   hsi_enable_store);
#endif

#define MAC_ADDR_FMT	"%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHTYPE_FMT	"%02x%02x"
#define SCI_FMT		"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"

static void format_output(char **buf_p,
			  struct osi_macsec_lut_config *lut_config)
{
	char *buf = *buf_p;
	unsigned int flags = lut_config->flags;
	struct osi_lut_inputs entry = lut_config->lut_in;

	if ((flags & OSI_LUT_FLAGS_DA_VALID) == OSI_LUT_FLAGS_DA_VALID) {
		buf += scnprintf(buf, PAGE_SIZE, "DA: " MAC_ADDR_FMT " ",
				 entry.da[5], entry.da[4], entry.da[3],
				 entry.da[2], entry.da[1], entry.da[0]);
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "DA: X ");
	}

	if ((flags & OSI_LUT_FLAGS_SA_VALID) == OSI_LUT_FLAGS_SA_VALID) {
		buf += scnprintf(buf, PAGE_SIZE, "SA: " MAC_ADDR_FMT " ",
				 entry.sa[5], entry.sa[4], entry.sa[3],
				 entry.sa[2], entry.sa[1], entry.sa[0]);
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "SA: X ");
	}

	if ((flags & OSI_LUT_FLAGS_ETHTYPE_VALID) ==
	    OSI_LUT_FLAGS_ETHTYPE_VALID) {
		buf += scnprintf(buf, PAGE_SIZE, "ethtype: " ETHTYPE_FMT " ",
				 entry.ethtype[1], entry.ethtype[0]);
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "ethtype: X ");
	}

	if ((flags & OSI_LUT_FLAGS_VLAN_VALID) == OSI_LUT_FLAGS_VLAN_VALID) {
		buf += scnprintf(buf, PAGE_SIZE, "vlan: ");
		if ((flags & OSI_LUT_FLAGS_VLAN_PCP_VALID) ==
		    OSI_LUT_FLAGS_VLAN_PCP_VALID) {
			buf += scnprintf(buf, PAGE_SIZE, "prio: %u ",
					 entry.vlan_pcp);
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "prio: X ");
		}
		if ((flags & OSI_LUT_FLAGS_VLAN_ID_VALID) ==
		    OSI_LUT_FLAGS_VLAN_ID_VALID) {
			buf += scnprintf(buf, PAGE_SIZE, "id: %u ",
					 entry.vlan_id);
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "id: X ");
		}
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "vlan: X ");
	}

	if ((flags & OSI_LUT_FLAGS_DVLAN_PKT) == OSI_LUT_FLAGS_DVLAN_PKT) {
		buf += scnprintf(buf, PAGE_SIZE, "dvlan: 1 ");
		if ((flags & OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL) ==
			OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL) {
			buf += scnprintf(buf, PAGE_SIZE, "dvlan_outer_tag: 1 ");
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "dvlan_outer_tag: 0 ");
		}
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "dvlan: X ");
	}

	if ((flags & OSI_LUT_FLAGS_BYTE0_PATTERN_VALID) ==
	    OSI_LUT_FLAGS_BYTE0_PATTERN_VALID) {
		buf += scnprintf(buf, PAGE_SIZE, "Byte0: Pattern: %x "
						  "offset: %u ",
				 entry.byte_pattern[0],
				 entry.byte_pattern_offset[0]);
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "Byte0: X ");
	}

	if ((flags & OSI_LUT_FLAGS_BYTE1_PATTERN_VALID) ==
	    OSI_LUT_FLAGS_BYTE1_PATTERN_VALID) {
		buf += scnprintf(buf, PAGE_SIZE, "Byte1: Pattern: %x "
						  "offset: %u ",
				 entry.byte_pattern[1],
				 entry.byte_pattern_offset[1]);
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "Byte1: X ");
	}

	if ((flags & OSI_LUT_FLAGS_BYTE2_PATTERN_VALID) ==
	    OSI_LUT_FLAGS_BYTE2_PATTERN_VALID) {
		buf += scnprintf(buf, PAGE_SIZE, "Byte2: Pattern: %x "
						  "offset: %u ",
				 entry.byte_pattern[2],
				 entry.byte_pattern_offset[2]);
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "Byte2: X ");
	}

	if ((flags & OSI_LUT_FLAGS_BYTE3_PATTERN_VALID) ==
	    OSI_LUT_FLAGS_BYTE3_PATTERN_VALID) {
		buf += scnprintf(buf, PAGE_SIZE, "Byte3: Pattern: %x "
						  "offset: %u ",
				 entry.byte_pattern[3],
				 entry.byte_pattern_offset[3]);
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "Byte3: X ");
	}

	if ((flags & OSI_LUT_FLAGS_PREEMPT_VALID) ==
	    OSI_LUT_FLAGS_PREEMPT_VALID) {
		if ((flags & OSI_LUT_FLAGS_PREEMPT) == OSI_LUT_FLAGS_PREEMPT) {
			buf += scnprintf(buf, PAGE_SIZE, "prempt: 1 ");
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "prempt: 0 ");
		}
	} else {
		buf += scnprintf(buf, PAGE_SIZE, "prempt: X ");
	}

	*buf_p = buf;

	return;
}


static int parse_inputs(const char *buf,
			struct osi_macsec_lut_config *lut_config, int *bufp)
{
	struct osi_lut_inputs *lut_in;
	int temp[OSI_ETH_ALEN] = {0};
	int temp2[OSI_ETH_ALEN] = {0};
	int temp3[OSI_LUT_BYTE_PATTERN_MAX] = {0};
	int temp4[OSI_ETHTYPE_LEN] = {0};
	unsigned char byte[OSI_LUT_BYTE_PATTERN_MAX] = {0};
	unsigned char mac_da[OSI_ETH_ALEN] = {0};
	unsigned char mac_sa[OSI_ETH_ALEN] = {0};
	unsigned char ethtype[OSI_ETHTYPE_LEN] = {0};
	unsigned int byte_offset[OSI_LUT_BYTE_PATTERN_MAX] = {0};
	unsigned int vlan_pcp = 0, vlan_id = 0, flags = 0;
	unsigned short controller;
	int mac_da_valid, mac_sa_valid, ethtype_valid, vlan_valid;
	int dvlan, dvlan_outer_tag;
	int prempt, prempt_valid;
	int byte_valid[OSI_LUT_BYTE_PATTERN_MAX];
	int i, valid, index;;


	i = sscanf(buf, "%d %d %hu "
			"%x:%x:%x:%x:%x:%x %d "
			"%x:%x:%x:%x:%x:%x %d "
			"%2x%2x %d "
			"%x %u %d "
			"%x %u %d "
			"%x %u %d "
			"%x %u %d "
			"%u %u %d %d %d "
			"%d %d%n",
		   &valid, &index, &controller,
		   &temp[0], &temp[1], &temp[2],
		   &temp[3], &temp[4], &temp[5], &mac_da_valid,
		   &temp2[0], &temp2[1], &temp2[2],
		   &temp2[3], &temp2[4], &temp2[5], &mac_sa_valid,
		   &temp4[0], &temp4[1], &ethtype_valid,
		   &temp3[0], &byte_offset[0], &byte_valid[0],
		   &temp3[1], &byte_offset[1], &byte_valid[1],
		   &temp3[2], &byte_offset[2], &byte_valid[2],
		   &temp3[3], &byte_offset[3], &byte_valid[3],
		   &vlan_pcp, &vlan_id, &vlan_valid, &dvlan, &dvlan_outer_tag,
		   &prempt, &prempt_valid, bufp);

	if (i != LUT_INPUTS_LEN) {
		pr_err("%s: Invalid LUT inputs(read %d)", __func__, i);
		goto err;
	}

	for (i = 0; i < OSI_ETH_ALEN; i++) {
		mac_da[i] = (unsigned char)temp[i];
		mac_sa[i] = (unsigned char)temp2[i];
	}

	for (i = 0; i < OSI_ETHTYPE_LEN; i++) {
		ethtype[i] = (unsigned char)temp4[i];
	}

	for (i = 0; i < OSI_LUT_BYTE_PATTERN_MAX; i++) {
		byte[i] = (unsigned char)temp3[i];
	}

	if (mac_da_valid && !is_valid_ether_addr(mac_da)) {
		pr_err("%s: Invalid mac DA\n", __func__);
		goto err;
	}

	if (mac_sa_valid && !is_valid_ether_addr(mac_sa)) {
		pr_err("%s: Invalid mac SA\n", __func__);
		goto err;
	}

	memset(lut_config, 0, sizeof(*lut_config));
	lut_config->table_config.ctlr_sel = controller;
	lut_config->table_config.index = index;
	lut_in = &lut_config->lut_in;

	if (mac_da_valid) {
		/* Reverse endianess for HW */
		for (i = 0; i < OSI_ETH_ALEN; i++) {
			lut_in->da[i] = mac_da[OSI_ETH_ALEN - 1 - i];
		}
		flags |= OSI_LUT_FLAGS_DA_VALID;
	}

	if (mac_sa_valid) {
		/* Reverse endianess for HW */
		for (i = 0; i < OSI_ETH_ALEN; i++) {
			lut_in->sa[i] = mac_sa[OSI_ETH_ALEN - 1 - i];
		}
		flags |= OSI_LUT_FLAGS_SA_VALID;
	}

	if (ethtype_valid) {
		/* Reverse endianess for HW */
		for (i = 0; i < OSI_ETHTYPE_LEN; i++) {
			lut_in->ethtype[i] = ethtype[OSI_ETHTYPE_LEN - 1 - i];
		}
		flags |= OSI_LUT_FLAGS_ETHTYPE_VALID;
	}

	for (i = 0; i < OSI_LUT_BYTE_PATTERN_MAX; i++) {
		if (byte_valid[i]) {
			switch (i) {
			case 0:
				flags |= OSI_LUT_FLAGS_BYTE0_PATTERN_VALID;
				break;
			case 1:
				flags |= OSI_LUT_FLAGS_BYTE1_PATTERN_VALID;
				break;
			case 2:
				flags |= OSI_LUT_FLAGS_BYTE2_PATTERN_VALID;
				break;
			case 3:
				flags |= OSI_LUT_FLAGS_BYTE3_PATTERN_VALID;
				break;
			}
			lut_in->byte_pattern[i] = byte[i];
			lut_in->byte_pattern_offset[i] = byte_offset[i];
		}
	}

	if (vlan_valid) {
		lut_in->vlan_pcp = vlan_pcp;
		lut_in->vlan_id = vlan_id;
		flags |= (OSI_LUT_FLAGS_VLAN_ID_VALID |
			 OSI_LUT_FLAGS_VLAN_PCP_VALID |
			 OSI_LUT_FLAGS_VLAN_VALID);
	}

	if (dvlan) {
		flags |= OSI_LUT_FLAGS_DVLAN_PKT;
		if (dvlan_outer_tag) {
			flags |= OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL;
		}
	}

	if (prempt_valid) {
		flags |= OSI_LUT_FLAGS_PREEMPT_VALID;
		if (prempt)
			flags |= OSI_LUT_FLAGS_PREEMPT;
	}

	if (valid) {
		flags |= OSI_LUT_FLAGS_ENTRY_VALID;
	}

	lut_config->flags = flags;

	return 0;

err:
	return -1;
}

static void dump_byp_lut(char **buf_p, unsigned short ctlr_sel,
			 struct osi_core_priv_data *osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	char *buf = *buf_p;
	int i;

	for (i = 0; i <= OSI_BYP_LUT_MAX_INDEX; i++) {
		memset(&lut_config, OSI_NONE, sizeof(lut_config));
		lut_config.table_config.ctlr_sel = ctlr_sel;
		lut_config.lut_sel = OSI_LUT_SEL_BYPASS;
		lut_config.table_config.rw = OSI_LUT_READ;
		lut_config.table_config.index = i;
		if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
			pr_err("%s: Failed to read BYP LUT\n", __func__);
			*buf_p = buf;
			return;
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "%d.\t", i);
			if ((lut_config.flags & OSI_LUT_FLAGS_ENTRY_VALID) !=
			    OSI_LUT_FLAGS_ENTRY_VALID) {
				buf += scnprintf(buf, PAGE_SIZE, "Invalid\n");
				memset(&lut_config, 0, sizeof(lut_config));
				continue;
			}

			format_output(&buf, &lut_config);
			/* BYP LUT output field */
			if ((lut_config.flags & OSI_LUT_FLAGS_CONTROLLED_PORT) ==
			    OSI_LUT_FLAGS_CONTROLLED_PORT) {
				buf += scnprintf(buf,
						 PAGE_SIZE, "ctrl port: 1\n");
			} else {
				buf += scnprintf(buf,
						 PAGE_SIZE, "ctrl port: 0\n");
			}
		}
	}

	*buf_p = buf;
}

/**
 * @brief Shows the current BYP LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current LUT configuration
 */
static ssize_t macsec_byp_lut_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	char *start = buf;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	buf += scnprintf(buf, PAGE_SIZE, "Tx:\n");
	dump_byp_lut(&buf, OSI_CTLR_SEL_TX, osi_core);

	buf += scnprintf(buf, PAGE_SIZE, "Rx:\n");
	dump_byp_lut(&buf, OSI_CTLR_SEL_RX, osi_core);

	return (buf - start);
}

/**
 * @brief Set the BYP LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the desired LUT configuration
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_byp_lut_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_macsec_lut_config lut_config;
	int ret, bufp, ctrl_port;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	ret = parse_inputs(buf, &lut_config, &bufp);
	if (ret < 0) {
		dev_err(pdata->dev, "Failed to parse inputs");
		goto exit;
	}

	ret = sscanf(buf + bufp, " %d", &ctrl_port);

	if (ret != BYP_LUT_INPUTS) {
		dev_err(pdata->dev, "Failed to parse BYP LUT arguments");
		goto exit;
	}
	if (ctrl_port)
		lut_config.flags |= OSI_LUT_FLAGS_CONTROLLED_PORT;

	lut_config.lut_sel = OSI_LUT_SEL_BYPASS;
	lut_config.table_config.rw = OSI_LUT_WRITE;
	/* Rest of LUT attributes are filled by parse_inputs() */
	if (lut_config.table_config.index > OSI_BYP_LUT_MAX_INDEX) {
		dev_err(dev, "%s: Index can't be > %d\n", __func__,
			OSI_BYP_LUT_MAX_INDEX);
		goto exit;
	}

	if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
		dev_err(dev, "%s: Failed to config BYP LUT\n", __func__);
		goto exit;
	} else {
		dev_err(dev, "%s: Added BYP LUT idx: %d", __func__,
			lut_config.table_config.index);
	}

exit:
	return size;
}

/**
 * @brief Sysfs attribute for MACsec BYP LUT config
 *
 */
static DEVICE_ATTR(macsec_byp_lut, (S_IRUGO | S_IWUSR),
		   macsec_byp_lut_show,
		   macsec_byp_lut_store);

/**
 * @brief Shows the current macsec statitics counters
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current counters
 */
static ssize_t macsec_mmc_counters_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_macsec_mmc_counters *mmc = &osi_core->macsec_mmc;
	unsigned short i;
	char *start = buf;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	osi_macsec_read_mmc(osi_core);
	buf += scnprintf(buf, PAGE_SIZE, "tx_pkts_untaged:\t%llu\n",
		mmc->tx_pkts_untaged);
	buf += scnprintf(buf, PAGE_SIZE, "tx_pkts_too_long:\t%llu\n",
		mmc->tx_pkts_too_long);
	buf += scnprintf(buf, PAGE_SIZE, "tx_octets_protected:\t%llu\n",
		mmc->tx_octets_protected);
	for (i = 0; i < OSI_MACSEC_SC_INDEX_MAX; i++) {
		buf += scnprintf(buf, PAGE_SIZE, "tx_pkts_protected sc%d:\t%llu\n",
			i, mmc->tx_pkts_protected[i]);
	}

	buf += scnprintf(buf, PAGE_SIZE, "rx_pkts_no_tag:  \t%llu\n",
		mmc->rx_pkts_no_tag);
	buf += scnprintf(buf, PAGE_SIZE, "rx_pkts_untagged:\t%llu\n",
		mmc->rx_pkts_untagged);
	buf += scnprintf(buf, PAGE_SIZE, "rx_pkts_bad_tag:\t%llu\n",
		mmc->rx_pkts_bad_tag);
	buf += scnprintf(buf, PAGE_SIZE, "rx_pkts_no_sa_err:\t%llu\n",
		mmc->rx_pkts_no_sa_err);
	buf += scnprintf(buf, PAGE_SIZE, "rx_pkts_no_sa:  \t%llu\n",
		mmc->rx_pkts_no_sa);
	buf += scnprintf(buf, PAGE_SIZE, "rx_pkts_overrun:\t%llu\n",
		mmc->rx_pkts_overrun);
	buf += scnprintf(buf, PAGE_SIZE, "rx_octets_validated:\t%llu\n",
		mmc->rx_octets_validated);

	for (i = 0; i < OSI_MACSEC_SC_INDEX_MAX; i++) {
		buf += scnprintf(buf, PAGE_SIZE, "rx_pkts_invalid sc%d:\t%llu\n",
			i, mmc->in_pkts_invalid[i]);
	}
	for (i = 0; i < OSI_MACSEC_SC_INDEX_MAX; i++) {
		buf += scnprintf(buf, PAGE_SIZE, "rx_pkts_delayed sc%d:\t%llu\n",
			i, mmc->rx_pkts_delayed[i]);
	}
	for (i = 0; i < OSI_MACSEC_SC_INDEX_MAX; i++) {
		buf += scnprintf(buf, PAGE_SIZE, "rx_pkts_ok sc%d: \t%llu\n",
			i, mmc->rx_pkts_ok[i]);
	}

	return (buf - start);
}

/**
 * @brief Sysfs attribute for MACsec irq stats
 *
 */
static DEVICE_ATTR(macsec_mmc_counters, (S_IRUGO | S_IWUSR),
		   macsec_mmc_counters_show,
		   NULL);

#ifdef DEBUG_MACSEC
static void dump_dbg_buffers(char **buf_p, unsigned short ctlr_sel,
			 struct osi_core_priv_data *osi_core)
{
	struct osi_macsec_dbg_buf_config dbg_buf_config = {0};
	char *buf = *buf_p;
	int i;
	unsigned int idx_max;

	if (ctlr_sel == OSI_CTLR_SEL_TX) {
		idx_max = OSI_TX_DBG_BUF_IDX_MAX;
	} else {
		idx_max = OSI_RX_DBG_BUF_IDX_MAX;
	}
	for (i = 0; i < idx_max; i++) {
		memset(&dbg_buf_config, OSI_NONE, sizeof(dbg_buf_config));
		dbg_buf_config.rw = OSI_LUT_READ;
		dbg_buf_config.ctlr_sel = ctlr_sel;
		dbg_buf_config.index = i;
		if (osi_macsec_config_dbg_buf(osi_core, &dbg_buf_config) < 0) {
			pr_err("%s: Failed to read debug buffers\n", __func__);
			*buf_p = buf;
			return;
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "%d.\t", i);
			buf += scnprintf(buf, PAGE_SIZE, " 0x%08X\t 0x%08X\t 0x%08X\t 0x%08X\n",
				dbg_buf_config.dbg_buf[3], dbg_buf_config.dbg_buf[2],
				dbg_buf_config.dbg_buf[1], dbg_buf_config.dbg_buf[0]);
		}
	}
	*buf_p = buf;

	/* reset debug buffer after buf read */
	for (i = 0; i < idx_max; i++) {
		memset(&dbg_buf_config, OSI_NONE, sizeof(dbg_buf_config));
		dbg_buf_config.rw = OSI_LUT_WRITE;
		dbg_buf_config.ctlr_sel = ctlr_sel;
		dbg_buf_config.index = i;
		if (osi_macsec_config_dbg_buf(osi_core, &dbg_buf_config) < 0) {
			pr_err("%s: Failed to write debug buffers\n", __func__);
			return;
		}
	}
}

/**
 * @brief Shows the current tx/rx debug buffers
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current debug buffers
 */
static ssize_t macsec_dbg_buffer_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	char *start = buf;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}
	buf += scnprintf(buf, PAGE_SIZE, "Tx Dbg Buffers:\n");
	dump_dbg_buffers(&buf, OSI_CTLR_SEL_TX, osi_core);

	buf += scnprintf(buf, PAGE_SIZE, "Rx Dbg Buffers:\n");
	dump_dbg_buffers(&buf, OSI_CTLR_SEL_RX, osi_core);

	return (buf - start);
}

/**
 * @brief Sysfs attribute for MACsec irq stats
 *
 */
static DEVICE_ATTR(macsec_dbg_buffers, (S_IRUGO | S_IWUSR),
		   macsec_dbg_buffer_show,
		   NULL);

#define DBG_EVENTS_LEN 13

/**
 * @brief Set the debug buffer trigger events
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the desired debug buffer events
 * 			configuration
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_dbg_events_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_macsec_dbg_buf_config dbg_buf_config = {0};
	int ret, events[12], i;
	unsigned short controller;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	ret = sscanf(buf, "%hu %1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x",
		&controller, &events[11], &events[10], &events[9], &events[8],
		&events[7], &events[6], &events[5], &events[4],
		&events[3], &events[2], &events[1], &events[0]);

	if (ret != DBG_EVENTS_LEN) {
		pr_err("%s: Invalid DBG inputs(read %d)", __func__, ret);
		goto exit;
	}
	/** parse all 12 trigger events */
	for (i = 0; i < 12; i++) {
		if (events[i] > OSI_ENABLE) {
			dev_err(dev, "%s: events bitmap error\n", __func__);
			goto exit;
		} else {
			dbg_buf_config.flags |= (events[i] << i);
		}
	}
	dbg_buf_config.ctlr_sel = controller;
	dbg_buf_config.rw = OSI_LUT_WRITE;

	if (osi_macsec_dbg_events_config(osi_core, &dbg_buf_config) < 0) {
		dev_err(dev, "%s: Failed to config dbg trigger events\n", __func__);
		goto exit;
	} else {
		dev_err(dev, "%s: Updated dbg trigger events: %x", __func__,
				dbg_buf_config.flags);
	}

exit:
	return size;

}

/**
 * @brief Sysfs attribute for MACsec debug events config
 *
 */
static DEVICE_ATTR(macsec_dbg_events, (S_IRUGO | S_IWUSR),
		   NULL,
		   macsec_dbg_events_store);
#endif /* DEBUG_MACSEC */

/**
 * @brief Shows the current SCI LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current SCI LUT configuration
 */
static ssize_t macsec_sci_lut_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_macsec_lut_config lut_config = {0};
	unsigned int an_valid;
	int i;
	char *start = buf;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	buf += scnprintf(buf, PAGE_SIZE, "Tx:\n");

	for (i = 0; i <= OSI_SC_LUT_MAX_INDEX; i++) {
		memset(&lut_config, OSI_NONE, sizeof(lut_config));
		lut_config.table_config.ctlr_sel = OSI_CTLR_SEL_TX;
		lut_config.lut_sel = OSI_LUT_SEL_SCI;
		lut_config.table_config.rw = OSI_LUT_READ;
		lut_config.table_config.index = i;
		if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
			dev_err(dev, "%s: Failed to read SCI LUT\n", __func__);
			goto exit;
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "%d.\t", i);
			if ((lut_config.flags & OSI_LUT_FLAGS_ENTRY_VALID) !=
			    OSI_LUT_FLAGS_ENTRY_VALID) {
				buf += scnprintf(buf, PAGE_SIZE, "Invalid\n");
				memset(&lut_config, 0, sizeof(lut_config));
				continue;
			}
			format_output(&buf, &lut_config);
			/* Tx SCI LUT output field */
			an_valid = lut_config.sci_lut_out.an_valid;
			buf += scnprintf(buf, PAGE_SIZE, "AN3: %d AN2: %d "
					 "AN1: %d AN0: %d ",
					 an_valid & OSI_AN3_VALID ? 1 : 0,
					 an_valid & OSI_AN2_VALID ? 1 : 0,
					 an_valid & OSI_AN1_VALID ? 1 : 0,
					 an_valid & OSI_AN0_VALID ? 1 : 0);
			buf += scnprintf(buf, PAGE_SIZE, "sc_index: %d\n",
					 lut_config.sci_lut_out.sc_index);
			memset(&lut_config, 0, sizeof(lut_config));
		}
	}

	buf += scnprintf(buf, PAGE_SIZE, "Rx:\n");

	for (i = 0; i <= OSI_SC_LUT_MAX_INDEX; i++) {
		memset(&lut_config, OSI_NONE, sizeof(lut_config));
		lut_config.table_config.ctlr_sel = OSI_CTLR_SEL_RX;
		lut_config.lut_sel = OSI_LUT_SEL_SCI;
		lut_config.table_config.rw = OSI_LUT_READ;
		lut_config.table_config.index = i;
		if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
			dev_err(dev, "%s: Failed to read BYP LUT\n", __func__);
			goto exit;
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "%d.\t", i);
			if ((lut_config.flags & OSI_LUT_FLAGS_ENTRY_VALID) !=
			    OSI_LUT_FLAGS_ENTRY_VALID) {
				buf += scnprintf(buf, PAGE_SIZE, "Invalid\n");
				memset(&lut_config, 0, sizeof(lut_config));
				continue;
			}

			buf += scnprintf(buf, PAGE_SIZE,
					 "SCI: " SCI_FMT
					 " sc_index: %d\n",
					 lut_config.sci_lut_out.sci[7],
					 lut_config.sci_lut_out.sci[6],
					 lut_config.sci_lut_out.sci[5],
					 lut_config.sci_lut_out.sci[4],
					 lut_config.sci_lut_out.sci[3],
					 lut_config.sci_lut_out.sci[2],
					 lut_config.sci_lut_out.sci[1],
					 lut_config.sci_lut_out.sci[0],
					 lut_config.sci_lut_out.sc_index);
			memset(&lut_config, 0, sizeof(lut_config));
		}
	}

exit:
	return (buf - start);
}

#define SCI_LUT_INPUTS 13

/**
 * @brief Set the SCI LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the desired LUT configuration
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_sci_lut_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_macsec_lut_config lut_config;
	int an_valid[OSI_MAX_NUM_SA] = {0};
	int ret, bufp;
	int temp[OSI_SCI_LEN];
	int i;
	int sc_index;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	ret = parse_inputs(buf, &lut_config, &bufp);
	if (ret < 0) {
		dev_err(pdata->dev, "Failed to parse inputs");
		goto exit;
	}

	ret = sscanf(buf+bufp, " %1x%1x%1x%1x %x:%x:%x:%x:%x:%x:%x:%x %d",
		     &an_valid[3], &an_valid[2], &an_valid[1], &an_valid[0],
		     &temp[7], &temp[6], &temp[5], &temp[4],
		     &temp[3], &temp[2], &temp[1], &temp[0],
		     &sc_index);
	if (ret != SCI_LUT_INPUTS) {
		dev_err(pdata->dev, "Failed to parse SCI LUT arguments");
		goto exit;
	}

	lut_config.lut_sel = OSI_LUT_SEL_SCI;
	lut_config.table_config.rw = OSI_LUT_WRITE;
	/* Rest of LUT attributes are filled by parse_inputs() */
	if (lut_config.table_config.index > OSI_SC_LUT_MAX_INDEX) {
		dev_err(dev, "%s: Index can't be > %d\n", __func__,
			OSI_SC_LUT_MAX_INDEX);
		goto exit;
	}
	if (sc_index > OSI_SC_LUT_MAX_INDEX) {
		dev_err(dev, "%s: SC Index can't be > %d\n", __func__,
			OSI_SC_LUT_MAX_INDEX);
		goto exit;
	}

	/* Configure the outputs */
	for (i = 0; i < OSI_SCI_LEN; i++) {
		lut_config.sci_lut_out.sci[i] = (unsigned char)temp[i];
	}
	for (i = 0; i < OSI_MAX_NUM_SA; i++) {
		if (an_valid[i] > OSI_ENABLE) {
			dev_err(dev, "%s: an_valid bitmap error\n", __func__);
			goto exit;
		} else {
			lut_config.sci_lut_out.an_valid |= (an_valid[i] << i);
		}
	}
	lut_config.sci_lut_out.sc_index = sc_index;

	if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
		dev_err(dev, "%s: Failed to config SCI LUT\n", __func__);
		goto exit;
	} else {
		dev_err(dev, "%s: Added SCI LUT idx: %d", __func__,
			lut_config.table_config.index);
	}

exit:
	return size;
}

/**
 * @brief Sysfs attribute for MACsec SCI LUT config
 *
 */
static DEVICE_ATTR(macsec_sci_lut, (S_IRUGO | S_IWUSR),
		   macsec_sci_lut_show,
		   macsec_sci_lut_store);

#ifdef MACSEC_KEY_PROGRAM
static void dump_kt(char **buf_p, unsigned short ctlr_sel,
		    struct osi_core_priv_data *osi_core)
{
	struct osi_macsec_kt_config kt_config = {0};
	char *buf = *buf_p;
	int i, j;

	for (i = 0; i <= OSI_TABLE_INDEX_MAX; i++) {
		memset(&kt_config, OSI_NONE, sizeof(kt_config));
		kt_config.table_config.ctlr_sel = ctlr_sel;
		kt_config.table_config.rw = OSI_LUT_READ;
		kt_config.table_config.index = i;
		if (osi_macsec_config_kt(osi_core, &kt_config) < 0) {
			pr_err("%s: Failed to read KT\n", __func__);
			*buf_p = buf;
			return;
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "%d.\t", i);
			if ((kt_config.flags & OSI_LUT_FLAGS_ENTRY_VALID) !=
			    OSI_LUT_FLAGS_ENTRY_VALID) {
				buf += scnprintf(buf, PAGE_SIZE, "Invalid\n");
				memset(&kt_config, 0, sizeof(kt_config));
				continue;
			}

			buf += scnprintf(buf, PAGE_SIZE, "SAK: 0x");
			for (j = 0; j < OSI_KEY_LEN_256; j++) {
				buf += scnprintf(buf, PAGE_SIZE, "%02x",
				kt_config.entry.sak[OSI_KEY_LEN_256 - 1 - j]);
			}
			buf += scnprintf(buf, PAGE_SIZE, " H: 0x");
			for (j = 0; j < OSI_KEY_LEN_128; j++) {
				buf += scnprintf(buf, PAGE_SIZE, "%02x",
				kt_config.entry.h[OSI_KEY_LEN_128 - 1 - j]);
			}
			buf += scnprintf(buf, PAGE_SIZE, "\n");
		}
	}

	*buf_p = buf;
}

/**
 * @brief Shows the current macsec Tx key table
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current LUT configuration
 */
static ssize_t macsec_tx_kt_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	char *start = buf;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	buf += scnprintf(buf, PAGE_SIZE, "Tx:\n");
	dump_kt(&buf, OSI_CTLR_SEL_TX, osi_core);

	return (buf - start);
}

/**
 * @brief Shows the current macsec Rx key table
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current LUT configuration
 */
static ssize_t macsec_rx_kt_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	char *start = buf;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	buf += scnprintf(buf, PAGE_SIZE, "Rx:\n");
	dump_kt(&buf, OSI_CTLR_SEL_RX, osi_core);

	return (buf - start);
}

#define KEY_FMT "%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x:%2x"

/**
 * @brief Set the Key table
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the desired LUT configuration
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_kt_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct crypto_cipher *tfm;
	unsigned char hkey[OSI_KEY_LEN_128];
	unsigned char zeros[OSI_KEY_LEN_128] = {0};
	struct osi_macsec_kt_config kt_config = {0};
	int temp[OSI_KEY_LEN_256] = {0};
	unsigned char sak[OSI_KEY_LEN_256] = {0};

	int valid, index, ctlr, key256bit;
	int i, ret, bufp = 0;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	ret = sscanf(buf, "%d %d %d %d" KEY_FMT"%n",
		     &valid, &index, &ctlr, &key256bit,
		     &temp[0], &temp[1], &temp[2], &temp[3],
		     &temp[4], &temp[5], &temp[6], &temp[7],
		     &temp[8], &temp[9], &temp[10], &temp[11],
			&temp[12], &temp[13], &temp[14], &temp[15], &bufp);

	if (ret != 20) {
		dev_err(pdata->dev, "Failed to parse key table arguments\n");
		goto exit;
	}

	if (key256bit == 1) {
		ret = sscanf(buf+bufp, KEY_FMT,
				 &temp[16], &temp[17], &temp[18], &temp[19],
				 &temp[20], &temp[21], &temp[22], &temp[23],
				 &temp[24], &temp[25], &temp[26], &temp[27],
				 &temp[28], &temp[29], &temp[30], &temp[31]);
			if (ret != 16) {
				dev_err(pdata->dev, "Failed to parse key table arguments\n");
				goto exit;
			}
	}

	if ((index > OSI_TABLE_INDEX_MAX) ||
	    (valid != OSI_ENABLE && valid != OSI_DISABLE) ||
	    (ctlr != OSI_CTLR_SEL_TX && ctlr != OSI_CTLR_SEL_RX)) {
		dev_err(pdata->dev, "%s: Invalid inputs\n", __func__);
		goto exit;
	}

	kt_config.table_config.ctlr_sel = ctlr;
	kt_config.table_config.rw = OSI_LUT_WRITE;
	kt_config.table_config.index = index;

	/* HKEY GENERATION */
	tfm = crypto_alloc_cipher("aes", 0, CRYPTO_ALG_ASYNC);
	if (crypto_cipher_setkey(tfm, sak, OSI_KEY_LEN_128)) {
		pr_err("%s: Failed to set cipher key for H generation",
			__func__);
		goto exit;
	}
	crypto_cipher_encrypt_one(tfm, hkey, zeros);
	crypto_free_cipher(tfm);

	for (i = 0; i < OSI_KEY_LEN_128; i++) {
		sak[i] = (unsigned char)temp[i];
	}
	if (key256bit == 1) {
		for (i = OSI_KEY_LEN_128; i < OSI_KEY_LEN_256; i++) {
			sak[i] = (unsigned char)temp[i];
		}
	}

	for (i = 0; i < OSI_KEY_LEN_128; i++) {
		kt_config.entry.h[i] = hkey[OSI_KEY_LEN_128 - 1 - i];
	}

	if (key256bit == 1) {
		for (i = 0; i < OSI_KEY_LEN_256; i++) {
			kt_config.entry.sak[i] = sak[OSI_KEY_LEN_256 - 1 - i];
		}
	} else {
		for (i = 0; i < OSI_KEY_LEN_128; i++) {
			kt_config.entry.sak[i] = sak[OSI_KEY_LEN_128 - 1 - i];
		}
	}

	if (valid) {
		kt_config.flags |= OSI_LUT_FLAGS_ENTRY_VALID;
	}

	ret = osi_macsec_config_kt(osi_core, &kt_config);
	if (ret < 0) {
		pr_err("%s: Failed to set SAK", __func__);
		goto exit;
	}

exit:
	return size;
}

/**
 * @brief Sysfs attribute for MACsec key table (Store new key)
 *
 */
static DEVICE_ATTR(macsec_kt, (S_IRUGO | S_IWUSR),
		   NULL,
		   macsec_kt_store);

/**
 * @brief Sysfs attribute for MACsec key table (show Tx table)
 *
 */
static DEVICE_ATTR(macsec_tx_kt, (S_IRUGO | S_IWUSR),
		   macsec_tx_kt_show,
		   NULL);

/**
 * @brief Sysfs attribute for MACsec key table (show Rx table)
 *
 */
static DEVICE_ATTR(macsec_rx_kt, (S_IRUGO | S_IWUSR),
		   macsec_rx_kt_show,
		   NULL);
#endif /* MACSEC_KEY_PROGRAM */

static void dump_sc_state_lut(char **buf_p, unsigned short ctlr_sel,
			      struct osi_core_priv_data *osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	char *buf = *buf_p;
	int i;

	for (i = 0; i <= OSI_SC_LUT_MAX_INDEX; i++) {
		memset(&lut_config, OSI_NONE, sizeof(lut_config));
		lut_config.table_config.ctlr_sel = ctlr_sel;
		lut_config.table_config.rw = OSI_LUT_READ;
		lut_config.table_config.index = i;
		lut_config.lut_sel = OSI_LUT_SEL_SC_STATE;
		if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
			pr_err("%s: Failed to read BYP LUT\n", __func__);
			*buf_p = buf;
			return;
		} else {
			buf += scnprintf(buf, PAGE_SIZE, "%d.\tcurr_an: %d\n",
					 i, lut_config.sc_state_out.curr_an);
		}
	}

	*buf_p = buf;
}

/**
 * @brief Shows the current SC state LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current LUT configuration
 */
static ssize_t macsec_sc_state_lut_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	char *start = buf;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	buf += scnprintf(buf, PAGE_SIZE, "Tx:\n");
	dump_sc_state_lut(&buf, OSI_CTLR_SEL_TX, osi_core);

	buf += scnprintf(buf, PAGE_SIZE, "Rx:\n");
	dump_sc_state_lut(&buf, OSI_CTLR_SEL_RX, osi_core);

	return (buf - start);
}

/**
 * @brief Set the SC state LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the desired LUT configuration
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_sc_state_lut_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_macsec_lut_config lut_config;
	int index, ctlr;
	int ret;
	nveu32_t curr_an;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	ret = sscanf(buf, "%d %d %d", &index, &ctlr, &curr_an);
	if (ret < 3) {
		dev_err(pdata->dev, "%s: Failed to parse inputs", __func__);
		goto exit;
	}

	if ((index > OSI_SC_LUT_MAX_INDEX) ||
	    (ctlr != OSI_CTLR_SEL_TX && ctlr != OSI_CTLR_SEL_RX) ||
	    (curr_an >= OSI_MAX_NUM_SA)) {
		dev_err(pdata->dev, "%s:Invalid inputs", __func__);
		goto exit;
	}

	lut_config.table_config.ctlr_sel = ctlr;
	lut_config.table_config.rw = OSI_LUT_WRITE;
	lut_config.table_config.index = index;
	lut_config.lut_sel = OSI_LUT_SEL_SC_STATE;
	lut_config.sc_state_out.curr_an = curr_an;

	if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
		dev_err(dev, "%s: Failed to config SC STATE LUT\n", __func__);
		goto exit;
	} else {
		dev_err(dev, "%s: Added SC STATE LUT idx: %d", __func__,
			lut_config.table_config.index);
	}

exit:
	return size;
}

/**
 * @brief Sysfs attribute for SC state LUT configuration
 *
 */
static DEVICE_ATTR(macsec_sc_state_lut, (S_IRUGO | S_IWUSR),
		   macsec_sc_state_lut_show,
		   macsec_sc_state_lut_store);

static void dump_sa_state_lut(char **buf_p, unsigned short ctlr_sel,
			      struct osi_core_priv_data *osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	char *buf = *buf_p;
	int i;

	for (i = 0; i <= OSI_SA_LUT_MAX_INDEX; i++) {
		memset(&lut_config, OSI_NONE, sizeof(lut_config));
		lut_config.table_config.ctlr_sel = ctlr_sel;
		lut_config.table_config.rw = OSI_LUT_READ;
		lut_config.table_config.index = i;
		lut_config.lut_sel = OSI_LUT_SEL_SA_STATE;
		if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
			pr_err("%s: Failed to read BYP LUT\n", __func__);
			goto exit;
		}

		switch (ctlr_sel) {
		case OSI_CTLR_SEL_TX:
			if ((lut_config.flags & OSI_LUT_FLAGS_ENTRY_VALID) ==
			    OSI_LUT_FLAGS_ENTRY_VALID) {
				buf += scnprintf(buf, PAGE_SIZE,
					"%d.\tnext_pn: %d\n", i,
					lut_config.sa_state_out.next_pn);
			} else {
				buf += scnprintf(buf, PAGE_SIZE,
					"%d.\tInvalid\n", i);
			}
			break;
		case OSI_CTLR_SEL_RX:
			buf += scnprintf(buf, PAGE_SIZE,
				"%d.\tnext_pn: %d lowest_pn: %d\n", i,
				lut_config.sa_state_out.next_pn,
				lut_config.sa_state_out.lowest_pn);
			break;
		default:
			goto exit;
		}
	}

exit:
	*buf_p = buf;
}

/**
 * @brief Shows the current SA state LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current LUT configuration
 */
static ssize_t macsec_sa_state_lut_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	char *start = buf;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	buf += scnprintf(buf, PAGE_SIZE, "Tx:\n");
	dump_sa_state_lut(&buf, OSI_CTLR_SEL_TX, osi_core);

	buf += scnprintf(buf, PAGE_SIZE, "Rx:\n");
	dump_sa_state_lut(&buf, OSI_CTLR_SEL_RX, osi_core);

	return (buf - start);
}

/**
 * @brief Set the SA state LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the desired LUT configuration
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_sa_state_lut_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_macsec_lut_config lut_config;
	int index, ctlr;
	int ret;
	unsigned int next_pn, lowest_pn;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	ret = sscanf(buf, "%d %d %u %u", &index, &ctlr, &next_pn, &lowest_pn);
	if (ret < 4) {
		dev_err(pdata->dev, "%s: Failed to parse inputs", __func__);
		goto exit;
	}

	if ((index > OSI_SA_LUT_MAX_INDEX) ||
	    (ctlr != OSI_CTLR_SEL_TX && ctlr != OSI_CTLR_SEL_RX)) {
		dev_err(pdata->dev, "%s:Invalid inputs", __func__);
		goto exit;
	}

	lut_config.flags = OSI_LUT_FLAGS_ENTRY_VALID;
	lut_config.table_config.ctlr_sel = ctlr;
	lut_config.table_config.rw = OSI_LUT_WRITE;
	lut_config.table_config.index = index;
	lut_config.sa_state_out.next_pn = next_pn;
	lut_config.sa_state_out.lowest_pn = lowest_pn;
	lut_config.lut_sel = OSI_LUT_SEL_SA_STATE;

	if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
		dev_err(dev, "%s: Failed to config SA STATE LUT\n", __func__);
		goto exit;
	} else {
		dev_err(dev, "%s: Added SA STATE LUT idx: %d", __func__,
			lut_config.table_config.index);
	}

exit:
	return size;
}

/**
 * @brief Sysfs attribute for SA state LUT configuration
 *
 */
static DEVICE_ATTR(macsec_sa_state_lut, (S_IRUGO | S_IWUSR),
		   macsec_sa_state_lut_show,
		   macsec_sa_state_lut_store);


static void dump_sc_param_lut(char **buf_p, unsigned short ctlr_sel,
			      struct osi_core_priv_data *osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	char *buf = *buf_p;
	int i;

	for (i = 0; i <= OSI_SC_LUT_MAX_INDEX; i++) {
		memset(&lut_config, OSI_NONE, sizeof(lut_config));
		lut_config.table_config.ctlr_sel = ctlr_sel;
		lut_config.table_config.rw = OSI_LUT_READ;
		lut_config.table_config.index = i;
		lut_config.lut_sel = OSI_LUT_SEL_SC_PARAM;
		if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
			pr_err("%s: Failed to read BYP LUT\n", __func__);
			goto exit;
		}

		switch (ctlr_sel) {
		case OSI_CTLR_SEL_TX:
			buf += scnprintf(buf, PAGE_SIZE,
				"%d.\tkey_idx_start: %d pn_max: %u "
				"pn_threshold: %u tci %01x vlan_clear %01x sci: " SCI_FMT,
				i, lut_config.sc_param_out.key_index_start,
				lut_config.sc_param_out.pn_max,
				lut_config.sc_param_out.pn_threshold,
				lut_config.sc_param_out.tci,
				lut_config.sc_param_out.vlan_in_clear,
				lut_config.sc_param_out.sci[7],
				lut_config.sc_param_out.sci[6],
				lut_config.sc_param_out.sci[5],
				lut_config.sc_param_out.sci[4],
				lut_config.sc_param_out.sci[3],
				lut_config.sc_param_out.sci[2],
				lut_config.sc_param_out.sci[1],
				lut_config.sc_param_out.sci[0]);
			buf += scnprintf(buf, PAGE_SIZE, "\n");
			break;
		case OSI_CTLR_SEL_RX:
			buf += scnprintf(buf, PAGE_SIZE,
				"%d.\tkey_idx_start: %d pn_max: %u pn_window: %u\n", i,
				lut_config.sc_param_out.key_index_start,
				lut_config.sc_param_out.pn_max,
				lut_config.sc_param_out.pn_window);
			break;
		default:
			goto exit;
		}
	}

exit:
	*buf_p = buf;
}

/**
 * @brief Shows the current SC parameters LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current LUT configuration
 */
static ssize_t macsec_sc_param_lut_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	char *start = buf;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	buf += scnprintf(buf, PAGE_SIZE, "Tx:\n");
	dump_sc_param_lut(&buf, OSI_CTLR_SEL_TX, osi_core);

	buf += scnprintf(buf, PAGE_SIZE, "Rx:\n");
	dump_sc_param_lut(&buf, OSI_CTLR_SEL_RX, osi_core);

	return (buf - start);
}

#define SC_PARAM_INPUTS_LEN 16

/**
 * @brief Set the SC parameters LUT configuration
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the desired LUT configuration
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t macsec_sc_param_lut_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_macsec_lut_config lut_config = {0};
	int index, ctlr;
	int ret, i, tci, vlan_clear;
	int sci[OSI_SCI_LEN] = {0};
	unsigned int pn_max, pn_threshold, key_index_start, pn_window;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	ret = sscanf(buf, "%d %d %u %u %u %u %d %d" SCI_FMT,
		     &index, &ctlr,
		     &key_index_start, &pn_max, &pn_threshold, &pn_window,
		     &tci, &vlan_clear,
		     &sci[7], &sci[6], &sci[5], &sci[4],
		     &sci[3], &sci[2], &sci[1], &sci[0]);
	if (ret < SC_PARAM_INPUTS_LEN) {
		dev_err(pdata->dev, "%s: Failed to parse inputs", __func__);
		goto exit;
	}

	if ((index > OSI_SC_LUT_MAX_INDEX) ||
	    (ctlr != OSI_CTLR_SEL_TX && ctlr != OSI_CTLR_SEL_RX) ||
	    (key_index_start > OSI_KEY_INDEX_MAX) ||
	    (pn_threshold > pn_max)) {
		dev_err(pdata->dev, "%s:Invalid inputs", __func__);
		goto exit;
	}

	lut_config.table_config.ctlr_sel = ctlr;
	lut_config.table_config.rw = OSI_LUT_WRITE;
	lut_config.table_config.index = index;
	lut_config.lut_sel = OSI_LUT_SEL_SC_PARAM;
	lut_config.sc_param_out.key_index_start = key_index_start;
	lut_config.sc_param_out.pn_max = pn_max;
	lut_config.sc_param_out.pn_threshold = pn_threshold;
	lut_config.sc_param_out.pn_window = pn_window;
	lut_config.sc_param_out.tci = (unsigned char)tci;
	lut_config.sc_param_out.vlan_in_clear = (unsigned char)vlan_clear;
	for (i = 0; i < OSI_SCI_LEN; i++) {
		lut_config.sc_param_out.sci[i] = (unsigned char)sci[i];
	}

	if (osi_macsec_config_lut(osi_core, &lut_config) < 0) {
		dev_err(dev, "%s: Failed to config SC PARAM LUT\n", __func__);
		goto exit;
	} else {
		dev_err(dev, "%s: Added SC PARAM LUT idx: %d", __func__,
			lut_config.table_config.index);
	}

exit:
	return size;
}

/**
 * @brief Sysfs attribute for SC param LUT configuration
 *
 */
static DEVICE_ATTR(macsec_sc_param_lut, (S_IRUGO | S_IWUSR),
		   macsec_sc_param_lut_show,
		   macsec_sc_param_lut_store);

/**
 * @brief Shows the current MACsec irq stats
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to print the current MACsec irq stats
 */
static ssize_t macsec_irq_stats_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_macsec_irq_stats *stats = &osi_core->macsec_irq_stats;

	return scnprintf(buf, PAGE_SIZE,
			 "tx_dbg_capture_done:\t%lu\n"
			 "tx_mtu_check_fail  :\t%lu\n"
			 "tx_mac_crc_error   :\t%lu\n"
			 "tx_sc_an_not_valid :\t%lu\n"
			 "tx_aes_gcm_buf_ovf :\t%lu\n"
			 "tx_lkup_miss       :\t%lu\n"
			 "tx_uninit_key_slot :\t%lu\n"
			 "tx_pn_threshold    :\t%lu\n"
			 "tx_pn_exhausted    :\t%lu\n"
			 "rx_dbg_capture_done:\t%lu\n"
			 "rx_icv_err_threshold :\t%lu\n"
			 "rx_replay_error      :\t%lu\n"
			 "rx_mtu_check_fail  :\t%lu\n"
			 "rx_mac_crc_error   :\t%lu\n"
			 "rx_aes_gcm_buf_ovf :\t%lu\n"
			 "rx_lkup_miss       :\t%lu\n"
			 "rx_uninit_key_slot :\t%lu\n"
			 "rx_pn_exhausted    :\t%lu\n"
			 "secure_reg_viol    :\t%lu\n",
			 stats->tx_dbg_capture_done,
			 stats->tx_mtu_check_fail,
			 stats->tx_mac_crc_error,
			 stats->tx_sc_an_not_valid,
			 stats->tx_aes_gcm_buf_ovf,
			 stats->tx_lkup_miss,
			 stats->tx_uninit_key_slot,
			 stats->tx_pn_threshold,
			 stats->tx_pn_exhausted,
			 stats->rx_dbg_capture_done,
			 stats->rx_icv_err_threshold,
			 stats->rx_replay_error,
			 stats->rx_mtu_check_fail,
			 stats->rx_mac_crc_error,
			 stats->rx_aes_gcm_buf_ovf,
			 stats->rx_lkup_miss,
			 stats->rx_uninit_key_slot,
			 stats->rx_pn_exhausted,
			 stats->secure_reg_viol);
}

/**
 * @brief Sysfs attribute for MACsec irq stats
 *
 */
static DEVICE_ATTR(macsec_irq_stats, (S_IRUGO | S_IWUSR),
		   macsec_irq_stats_show,
		   NULL);
#endif /* MACSEC_SUPPORT */

/**
 * @brief Shows the current driver setting for PHY iface mode
 *
 * Algorithm: Display the current PHY iface mode setting.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current PHY iface mode
 *
 * @note MAC and PHY need to be initialized.
 */
static ssize_t ether_phy_iface_mode_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;

	switch (osi_core->phy_iface_mode) {
	case OSI_XFI_MODE_10G:
		return scnprintf(buf, PAGE_SIZE, "XFI-10G\n");
	case OSI_XFI_MODE_5G:
		return scnprintf(buf, PAGE_SIZE, "XFI-5G\n");
	case OSI_USXGMII_MODE_10G:
		return scnprintf(buf, PAGE_SIZE, "USX-10G\n");
	case OSI_USXGMII_MODE_5G:
		return scnprintf(buf, PAGE_SIZE, "USX-5G\n");
	default:
		return scnprintf(buf, PAGE_SIZE, "XFI-10G\n");
	}
}

/**
 * @brief Set the user setting of PHY iface mode.
 *
 * Algorithm: This is used to set the user mode settings of PHY iface mode
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of PHY iface mode
 * @param[in] size: size of buffer
 *
 * @note MAC and PHY need to be initialized.
 *
 * @return size of buffer.
 */
static ssize_t ether_phy_iface_mode_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;

	if (netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is up\n");
		return size;
	}

	if (strncmp(buf, "XFI-10G", 7) == 0U) {
		osi_core->phy_iface_mode = OSI_XFI_MODE_10G;
	} else if (strncmp(buf, "XFI-5G", 6) == 0U) {
		osi_core->phy_iface_mode = OSI_XFI_MODE_5G;
	} else if (strncmp(buf, "USX-10G", 7) == 0U) {
		osi_core->phy_iface_mode = OSI_USXGMII_MODE_10G;
	} else if (strncmp(buf, "USX-5G", 6) == 0U) {
		osi_core->phy_iface_mode = OSI_USXGMII_MODE_5G;
	} else {
		dev_err(pdata->dev,
			"Invalid value passed. Valid values are XFI-10G/XFI-5G/USX-10G/USX-5G\n");
	}

	return size;
}

/**
 * @brief Sysfs attribute for PHY iface Mode
 *
 */
static DEVICE_ATTR(phy_iface_mode, (S_IRUGO | S_IWUSR),
		   ether_phy_iface_mode_show,
		   ether_phy_iface_mode_store);

/**
 * @brief Shows the current driver setting for UPHY GBE mocd
 *
 * Algorithm: Display the current PTP mode setting.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current PTP mode
 *
 * @note MAC and PHY need to be initialized.
 */
static ssize_t ether_uphy_gbe_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			(osi_core->uphy_gbe_mode == OSI_ENABLE) ?
			"10G" : "5G");
}

/**
 * @brief Set the user setting of UPHY GBE mode.
 *
 * Algorithm: This is used to set the user mode settings of UPHY GBE mode
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of UPHY GBE mode
 * @param[in] size: size of buffer
 *
 * @note MAC and PHY need to be initialized.
 *
 * @return size of buffer.
 */
static ssize_t ether_uphy_gbe_mode_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;

	if (netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is up\n");
		return size;
	}

	if (strncmp(buf, "10G", 3) == 0U) {
		osi_core->uphy_gbe_mode = OSI_ENABLE;
	} else if (strncmp(buf, "5G", 2) == 0U) {
		osi_core->uphy_gbe_mode = OSI_DISABLE;
	} else {
		dev_err(pdata->dev,
			"Invalid value passed. Valid values are 10G or 5G\n");
	}

	return size;
}

/**
 * @brief Sysfs attribute for UPHY GBE Mode
 *
 */
static DEVICE_ATTR(uphy_gbe_mode, (S_IRUGO | S_IWUSR),
		   ether_uphy_gbe_mode_show,
		   ether_uphy_gbe_mode_store);

/**
 * @brief Sysfs attribute for MAC loopback
 *
 */
static DEVICE_ATTR(mac_loopback, (S_IRUGO | S_IWUSR),
		   ether_mac_loopback_show,
		   ether_mac_loopback_store);

/**
 * @brief Shows the current setting of FRP Table
 *
 * Algorithm: Display the FRP table
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current MAC loopback setting
 *
 * @note MAC and PHY need to be initialized.
 */
static ssize_t ether_mac_frp_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_core_frp_entry *entry = NULL;
	struct osi_core_frp_data *data = NULL;
	int i = 0, j = 0;

	/* Write FRP table entries */
	for (i = 0, j = 0; ((i < osi_core->frp_cnt) && (j < PAGE_SIZE)); i++) {
		entry = &osi_core->frp_table[i];
		data = &entry->data;
		j += scnprintf((buf + j), (PAGE_SIZE - j),
			       "[%d] ID:%d MD:0x%x ME:0x%x AF:%d RF:%d IM:%d NIC:%d FO:%d OKI:%d DCH:x%x\n",
			       i, entry->frp_id, data->match_data,
			       data->match_en, data->accept_frame,
			       data->reject_frame, data->inverse_match,
			       data->next_ins_ctrl, data->frame_offset,
			       data->ok_index, data->dma_chsel);
	}

	return j;
}

/**
 * @brief Sysfs attribute for FRP table show
 *
 */
static DEVICE_ATTR(frp, 0644, ether_mac_frp_show, NULL);

/**
 * @brief Shows the current setting of PTP mode
 *
 * Algorithm: Display the current PTP mode setting.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current PTP mode
 *
 * @note MAC and PHY need to be initialized.
 */
static ssize_t ether_ptp_mode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 ((pdata->osi_dma->ptp_flag & OSI_PTP_SYNC_MASTER) ==
			  OSI_PTP_SYNC_MASTER) ? "master" :
			   ((pdata->osi_dma->ptp_flag & OSI_PTP_SYNC_SLAVE) ==
			    OSI_PTP_SYNC_SLAVE) ? "slave" : " ");
}

/**
 * @brief Set the user setting of PTP mode
 *
 * Algorithm: This is used to set the user mode settings of PTP mode
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of PTP mode
 * @param[in] size: size of buffer
 *
 * @note MAC and PHY need to be initialized.
 *
 * @return size of buffer.
 */
static ssize_t ether_ptp_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	if (strncmp(buf, "master", 6) == 0U) {
		pdata->osi_dma->ptp_flag &= ~(OSI_PTP_SYNC_MASTER |
					      OSI_PTP_SYNC_SLAVE);
		pdata->osi_dma->ptp_flag |= OSI_PTP_SYNC_MASTER;
	} else if (strncmp(buf, "slave", 5) == 0U) {
		pdata->osi_dma->ptp_flag &= ~(OSI_PTP_SYNC_MASTER |
					      OSI_PTP_SYNC_SLAVE);
		pdata->osi_dma->ptp_flag |= OSI_PTP_SYNC_SLAVE;
	} else {
		dev_err(pdata->dev,
			"Invalid entry. Valid Entries are master or slave\n");
	}

	return size;
}

/**
 * @brief Sysfs attribute for PTP MODE
 *
 */
static DEVICE_ATTR(ptp_mode, (S_IRUGO | S_IWUSR),
		   ether_ptp_mode_show,
		   ether_ptp_mode_store);

/**
 * @brief Shows the current setting of PTP sync method
 *
 * Algorithm: Display the current PTP sync method.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current ptp sync method
 *
 * @note MAC and PHY need to be initialized.
 */
static ssize_t ether_ptp_sync_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 ((pdata->osi_dma->ptp_flag & OSI_PTP_SYNC_TWOSTEP) ==
			  OSI_PTP_SYNC_TWOSTEP) ? "twostep" :
			   ((pdata->osi_dma->ptp_flag & OSI_PTP_SYNC_ONESTEP) ==
			    OSI_PTP_SYNC_ONESTEP) ? "onestep" : " ");
}

/**
 * @brief Set the user setting of PTP sync method
 *
 * Algorithm: This is used to set the user mode settings of PTP sync method
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of PTP sync method
 * @param[in] size: size of buffer
 *
 * @note MAC and PHY need to be initialized.
 *
 * @return size of buffer.
 */
static ssize_t ether_ptp_sync_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return size;
	}

	if (strncmp(buf, "onestep", 7) == 0U) {
		pdata->osi_dma->ptp_flag &= ~(OSI_PTP_SYNC_ONESTEP |
					      OSI_PTP_SYNC_TWOSTEP);
		pdata->osi_dma->ptp_flag |= OSI_PTP_SYNC_ONESTEP;

	} else if (strncmp(buf, "twostep", 7) == 0U) {
		pdata->osi_dma->ptp_flag &= ~(OSI_PTP_SYNC_ONESTEP |
					      OSI_PTP_SYNC_TWOSTEP);
		pdata->osi_dma->ptp_flag |= OSI_PTP_SYNC_TWOSTEP;
	} else {
		dev_err(pdata->dev,
			"Invalid entry. Valid Entries are onestep or twostep\n");
	}

	return size;
}

/**
 * @brief Sysfs attribute for PTP sync method
 *
 */
static DEVICE_ATTR(ptp_sync, (S_IRUGO | S_IWUSR),
		   ether_ptp_sync_show,
		   ether_ptp_sync_store);

#ifdef ETHER_NVGRO
/**
 * @brief Shows the current setting of NVGRO packet age threshold.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current ptp sync method
 */
static ssize_t ether_nvgro_pkt_age_msec_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pdata->pkt_age_msec);
}

/**
 * @brief Set the user setting of NVGRO packet age threshold.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of MAC loopback
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t ether_nvgro_pkt_age_msec_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	int ret;

	ret = kstrtou32(buf, 0, &pdata->pkt_age_msec);
	if (ret < 0) {
		dev_err(pdata->dev,
			"Invalid nvgro pkt age msec input\n");
		return -EINVAL;
	}

	return size;
}

/**
 * @brief Sysfs attribute for NVGRO packet age threshold
 *
 */
static DEVICE_ATTR(nvgro_pkt_age_msec, 0644,
		   ether_nvgro_pkt_age_msec_show,
		   ether_nvgro_pkt_age_msec_store);

/**
 * @brief Shows the current setting of NVGRO purge timer interval..
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current ptp sync method
 */
static ssize_t ether_nvgro_timer_interval_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pdata->nvgro_timer_intrvl);
}

/**
 * @brief Set the user setting of NVGRO purge timer interval.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer which contains the user settings of MAC loopback
 * @param[in] size: size of buffer
 *
 * @return size of buffer.
 */
static ssize_t ether_nvgro_timer_interval_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	int ret;

	ret = kstrtou32(buf, 0, &pdata->nvgro_timer_intrvl);
	if (ret < 0) {
		dev_err(pdata->dev,
			"Invalid nvgro timer interval input\n");
		return -EINVAL;
	}

	return size;
}

/**
 * @brief Sysfs attribute for NVGRO purge timer interval
 *
 */
static DEVICE_ATTR(nvgro_timer_interval, 0644,
		   ether_nvgro_timer_interval_show,
		   ether_nvgro_timer_interval_store);

/**
 * @brief Shows NVGRO stats
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current ptp sync method
 */
static ssize_t ether_nvgro_stats_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);

	return scnprintf(buf, PAGE_SIZE, "dropped = %llu\n",
			 pdata->nvgro_dropped);
}

/**
 * @brief Sysfs attribute for NVGRO stats.
 *
 */
static DEVICE_ATTR(nvgro_stats, 0644,
		   ether_nvgro_stats_show, NULL);

/**
 * @brief Dumps NVGRO queues.
 *
 * @param[in] dev: Device data.
 * @param[in] attr: Device attribute
 * @param[in] buf: Buffer to store the current ptp sync method
 */
static ssize_t ether_nvgro_dump_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct net_device *ndev = (struct net_device *)dev_get_drvdata(dev);
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct sk_buff *p, *pp;
	char *start = buf;

	buf += scnprintf(buf, PAGE_SIZE, "MQ: ");
	skb_queue_walk_safe(&pdata->mq, p, pp) {
		buf += scnprintf(buf, PAGE_SIZE, "skb %p TTL %d IPID %u\n",
				 p, NAPI_GRO_CB(p)->free,
				 NAPI_GRO_CB(p)->flush_id);
	}

	buf += scnprintf(buf, PAGE_SIZE, "FQ: ");
	skb_queue_walk_safe(&pdata->fq, p, pp) {
		buf += scnprintf(buf, PAGE_SIZE, "skb %p TTL %d IPID %u\n",
				 p, NAPI_GRO_CB(p)->free,
				 NAPI_GRO_CB(p)->flush_id);
	}

	return (buf - start);
}

/**
 * @brief Sysfs attribute for NVGRO queue dump.
 *
 */
static DEVICE_ATTR(nvgro_dump, 0644,
		   ether_nvgro_dump_show, NULL);
#endif

/**
 * @brief Attributes for nvethernet sysfs
 */
static struct attribute *ether_sysfs_attrs[] = {
#ifdef OSI_DEBUG
	&dev_attr_desc_dump_enable.attr,
#endif /* OSI_DEBUG */
	&dev_attr_mac_loopback.attr,
	&dev_attr_ptp_mode.attr,
	&dev_attr_ptp_sync.attr,
	&dev_attr_frp.attr,
#ifdef MACSEC_SUPPORT
	&dev_attr_macsec_irq_stats.attr,
	&dev_attr_macsec_byp_lut.attr,
	&dev_attr_macsec_sci_lut.attr,
#ifdef MACSEC_KEY_PROGRAM
	&dev_attr_macsec_kt.attr,
	&dev_attr_macsec_tx_kt.attr,
	&dev_attr_macsec_rx_kt.attr,
#endif /* MACSEC_KEY_PROGRAM */
	&dev_attr_macsec_sc_state_lut.attr,
	&dev_attr_macsec_sa_state_lut.attr,
	&dev_attr_macsec_sc_param_lut.attr,
	&dev_attr_macsec_cipher.attr,
	&dev_attr_macsec_enable.attr,
	&dev_attr_macsec_an_status.attr,
	&dev_attr_macsec_mmc_counters.attr,
#ifdef DEBUG_MACSEC
	&dev_attr_macsec_loopback.attr,
	&dev_attr_macsec_dbg_buffers.attr,
	&dev_attr_macsec_dbg_events.attr,
#endif /* DEBUG_MACSEC */
#endif /* MACSEC_SUPPORT */
	&dev_attr_uphy_gbe_mode.attr,
	&dev_attr_phy_iface_mode.attr,
#ifdef ETHER_NVGRO
	&dev_attr_nvgro_pkt_age_msec.attr,
	&dev_attr_nvgro_timer_interval.attr,
	&dev_attr_nvgro_stats.attr,
	&dev_attr_nvgro_dump.attr,
#endif
#ifdef HSI_SUPPORT
	&dev_attr_hsi_enable.attr,
#endif
	NULL
};

/**
 * @brief Ethernet sysfs attribute group
 */
static struct attribute_group ether_attribute_group = {
	.name = "nvethernet",
	.attrs = ether_sysfs_attrs,
};

#ifdef CONFIG_DEBUG_FS
static char *timestamp_system_source(unsigned int source)
{
	switch (source) {
	case 1:
		return "Internal";
	case 2:
		return "External";
	case 3:
		return "Internal and External";
	case 0:
		return "Reserved";
	}

	return "None";
}

static char *active_phy_selected_interface(unsigned int act_phy_sel)
{
	switch (act_phy_sel) {
	case 0:
		return "GMII or MII";
	case 1:
		return "RGMII";
	case 2:
		return "SGMII";
	case 3:
		return "TBI";
	case 4:
		return "RMII";
	case 5:
		return "RTBI";
	case 6:
		return "SMII";
	case 7:
		return "RevMII";
	}

	return "None";
}

static char *mtl_fifo_size(unsigned int fifo_size)
{
	switch (fifo_size) {
	case 0:
		return "128 Bytes";
	case 1:
		return "256 Bytes";
	case 2:
		return "512 Bytes";
	case 3:
		return "1KB";
	case 4:
		return "2KB";
	case 5:
		return "4KB";
	case 6:
		return "8KB";
	case 7:
		return "16KB";
	case 8:
		return "32KB";
	case 9:
		return "64KB";
	case 10:
		return "128KB";
	case 11:
		return "256KB";
	default:
		return "Reserved";
	}
}

static char *address_width(unsigned int val)
{
	switch (val) {
	case 0:
		return "32";
	case 1:
		return "40";
	case 2:
		return "48";
	default:
		return "Reserved";
	}
}

static char *hash_table_size(unsigned int size)
{
	switch (size) {
	case 0:
		return "No Hash Table";
	case 1:
		return "64";
	case 2:
		return "128";
	case 3:
		return "256";
	default:
		return "Invalid size";
	}
}

static char *num_vlan_filters(unsigned int filters)
{
	switch (filters) {
	case 0:
		return "Zero";
	case 1:
		return "4";
	case 2:
		return "8";
	case 3:
		return "16";
	case 4:
		return "24";
	case 5:
		return "32";
	default:
		return "Unknown";
	}
}

static char *max_frp_bytes(unsigned int bytes)
{
	switch (bytes) {
	case 0:
		return "64 Bytes";
	case 1:
		return "128 Bytes";
	case 2:
		return "256 Bytes";
	case 3:
		return "Reserved";
	default:
		return "Invalid";
	}
}

static char *max_frp_instructions(unsigned int entries)
{
	switch (entries) {
	case 0:
		return "64";
	case 1:
		return "128";
	case 2:
		return "256";
	case 3:
		return "Reserved";
	default:
		return "Invalid";
	}
}

static char *auto_safety_package(unsigned int pkg)
{
	switch (pkg) {
	case 0:
		return "No Safety features selected";
	case 1:
		return "Only 'ECC protection for external memory' feature is selected";
	case 2:
		return "All the Automotive Safety features are selected without the 'Parity Port Enable for external interface' feature";
	case 3:
		return "All the Automotive Safety features are selected with the 'Parity Port Enable for external interface' feature";
	default:
		return "Invalid";
	}
}

static char *tts_fifo_depth(unsigned int depth)
{
	switch (depth) {
	case 1:
		return "1";
	case 2:
		return "2";
	case 3:
		return "4";
	case 4:
		return "8";
	case 5:
		return "16";
	default:
		return "Reserved";
	}
}

static char *gate_ctl_depth(unsigned int depth)
{
	switch (depth) {
	case 0:
		return "No Depth Configured";
	case 1:
		return "64";
	case 2:
		return "128";
	case 3:
		return "256";
	case 4:
		return "512";
	case 5:
		return "1024";
	default:
		return "Reserved";
	}
}

static char *gate_ctl_width(unsigned int width)
{
	switch (width) {
	case 0:
		return "Width not configured";
	case 1:
		return "16";
	case 2:
		return "20";
	case 3:
		return "24";
	default:
		return "Invalid";
	}
}

static int ether_hw_features_read(struct seq_file *seq, void *v)
{
	struct net_device *ndev = seq->private;
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_hw_features *hw_feat = &pdata->hw_feat;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	seq_printf(seq, "==============================\n");
	seq_printf(seq, "\tHW features\n");
	seq_printf(seq, "==============================\n");

	seq_printf(seq, "\t10/100 Mbps: %s\n",
		   (hw_feat->mii_sel) ? "Y" : "N");
	seq_printf(seq, "\tRGMII Mode: %s\n",
		   (hw_feat->rgmii_sel) ? "Y" : "N");
	seq_printf(seq, "\tRMII Mode: %s\n",
		   (hw_feat->rmii_sel) ? "Y" : "N");
	seq_printf(seq, "\t1000 Mpbs: %s\n",
		   (hw_feat->gmii_sel) ? "Y" : "N");
	seq_printf(seq, "\tHalf duplex support: %s\n",
		   (hw_feat->hd_sel) ? "Y" : "N");
	seq_printf(seq, "\tTBI/SGMII/RTBI PHY interface: %s\n",
		   (hw_feat->pcs_sel) ? "Y" : "N");
	seq_printf(seq, "\tVLAN Hash Filtering: %s\n",
		   (hw_feat->vlan_hash_en) ? "Y" : "N");
	seq_printf(seq, "\tMDIO interface: %s\n",
		   (hw_feat->sma_sel) ? "Y" : "N");
	seq_printf(seq, "\tRemote Wake-Up Packet Detection: %s\n",
		   (hw_feat->rwk_sel) ? "Y" : "N");
	seq_printf(seq, "\tMagic Packet Detection: %s\n",
		   (hw_feat->mgk_sel) ? "Y" : "N");
	seq_printf(seq, "\tMAC Management Counters (MMC): %s\n",
		   (hw_feat->mmc_sel) ? "Y" : "N");
	seq_printf(seq, "\tARP Offload: %s\n",
		   (hw_feat->arp_offld_en) ? "Y" : "N");
	seq_printf(seq, "\tIEEE 1588 Timestamp Support: %s\n",
		   (hw_feat->ts_sel) ? "Y" : "N");
	seq_printf(seq, "\tEnergy Efficient Ethernet (EEE) Support: %s\n",
		   (hw_feat->eee_sel) ? "Y" : "N");
	seq_printf(seq, "\tTransmit TCP/IP Checksum Insertion Support: %s\n",
		   (hw_feat->tx_coe_sel) ? "Y" : "N");
	seq_printf(seq, "\tReceive TCP/IP Checksum Support: %s\n",
		   (hw_feat->rx_coe_sel) ? "Y" : "N");
	seq_printf(seq, "\t (1 - 31) MAC Address registers: %s\n",
		   (hw_feat->mac_addr_sel) ? "Y" : "N");
	seq_printf(seq, "\t(32 - 63) MAC Address Registers: %s\n",
		   (hw_feat->mac_addr32_sel) ? "Y" : "N");
	seq_printf(seq, "\t(64 - 127) MAC Address Registers: %s\n",
		   (hw_feat->mac_addr64_sel) ? "Y" : "N");
	seq_printf(seq, "\tTimestamp System Time Source: %s\n",
		   timestamp_system_source(hw_feat->tsstssel));
	seq_printf(seq, "\tSource Address or VLAN Insertion Enable: %s\n",
		   (hw_feat->sa_vlan_ins) ? "Y" : "N");
	seq_printf(seq, "\tActive PHY selected Interface: %s\n",
		   active_phy_selected_interface(hw_feat->sa_vlan_ins));
	seq_printf(seq, "\tVxLAN/NVGRE Support: %s\n",
		   (hw_feat->vxn) ? "Y" : "N");
	seq_printf(seq, "\tDifferent Descriptor Cache Support: %s\n",
		   (hw_feat->ediffc) ? "Y" : "N");
	seq_printf(seq, "\tEnhanced DMA Support: %s\n",
		   (hw_feat->edma) ? "Y" : "N");
	seq_printf(seq, "\tMTL Receive FIFO Size: %s\n",
		   mtl_fifo_size(hw_feat->rx_fifo_size));
	seq_printf(seq, "\tMTL Transmit FIFO Size: %s\n",
		   mtl_fifo_size(hw_feat->tx_fifo_size));
	seq_printf(seq, "\tPFC Enable: %s\n",
		   (hw_feat->pfc_en) ? "Y" : "N");
	seq_printf(seq, "\tOne-Step Timestamping Support: %s\n",
		   (hw_feat->ost_en) ? "Y" : "N");
	seq_printf(seq, "\tPTP Offload Enable: %s\n",
		   (hw_feat->pto_en) ? "Y" : "N");
	seq_printf(seq, "\tIEEE 1588 High Word Register Enable: %s\n",
		   (hw_feat->adv_ts_hword) ? "Y" : "N");
	seq_printf(seq, "\tAXI Address width: %s\n",
		   address_width(hw_feat->addr_64));
	seq_printf(seq, "\tDCB Feature Support: %s\n",
		   (hw_feat->dcb_en) ? "Y" : "N");
	seq_printf(seq, "\tSplit Header Feature Support: %s\n",
		   (hw_feat->sph_en) ? "Y" : "N");
	seq_printf(seq, "\tTCP Segmentation Offload Support: %s\n",
		   (hw_feat->tso_en) ? "Y" : "N");
	seq_printf(seq, "\tDMA Debug Registers Enable: %s\n",
		   (hw_feat->dma_debug_gen) ? "Y" : "N");
	seq_printf(seq, "\tAV Feature Enable: %s\n",
		   (hw_feat->av_sel) ? "Y" : "N");
	seq_printf(seq, "\tRx Side Only AV Feature Enable: %s\n",
		   (hw_feat->rav_sel) ? "Y" : "N");
	seq_printf(seq, "\tHash Table Size: %s\n",
		   hash_table_size(hw_feat->hash_tbl_sz));
	seq_printf(seq, "\tTotal number of L3 or L4 Filters: %u\n",
		   hw_feat->l3l4_filter_num);
	seq_printf(seq, "\tNumber of MTL Receive Queues: %u\n",
		   (hw_feat->rx_q_cnt + 1));
	seq_printf(seq, "\tNumber of MTL Transmit Queues: %u\n",
		   (hw_feat->tx_q_cnt + 1));
	seq_printf(seq, "\tNumber of Receive DMA channels: %u\n",
		   (hw_feat->rx_ch_cnt + 1));
	seq_printf(seq, "\tNumber of Transmit DMA channels: %u\n",
		   (hw_feat->tx_ch_cnt + 1));
	seq_printf(seq, "\tNumber of PPS outputs: %u\n",
		   hw_feat->pps_out_num);
	seq_printf(seq, "\tNumber of Auxiliary Snapshot Inputs: %u\n",
		   hw_feat->aux_snap_num);
	seq_printf(seq, "\tRSS Feature Enabled: %s\n",
		   (hw_feat->rss_en) ? "Y" : "N");
	seq_printf(seq, "\tNumber of Traffic Classes: %u\n",
		   (hw_feat->num_tc + 1));
	seq_printf(seq, "\tNumber of VLAN filters: %s\n",
		   num_vlan_filters(hw_feat->num_vlan_filters));
	seq_printf(seq,
		   "\tQueue/Channel based VLAN tag insert on Tx Enable: %s\n",
		   (hw_feat->cbti_sel) ? "Y" : "N");
	seq_printf(seq, "\tOne-Step for PTP over UDP/IP Feature Enable: %s\n",
		   (hw_feat->ost_over_udp) ? "Y" : "N");
	seq_printf(seq, "\tDouble VLAN processing support: %s\n",
		   (hw_feat->double_vlan_en) ? "Y" : "N");

	if (osi_core->mac_ver > OSI_EQOS_MAC_5_00) {
		seq_printf(seq, "\tSupported Flexible Receive Parser: %s\n",
			   (hw_feat->frp_sel) ? "Y" : "N");
		seq_printf(seq, "\tNumber of FRP Pipes: %u\n",
			   (hw_feat->num_frp_pipes + 1));
		seq_printf(seq, "\tNumber of FRP Parsable Bytes: %s\n",
			   max_frp_bytes(hw_feat->max_frp_bytes));
		seq_printf(seq, "\tNumber of FRP Instructions: %s\n",
			   max_frp_instructions(hw_feat->max_frp_entries));
		seq_printf(seq, "\tAutomotive Safety Package: %s\n",
			   auto_safety_package(hw_feat->auto_safety_pkg));
		seq_printf(seq, "\tTx Timestamp FIFO Depth: %s\n",
			   tts_fifo_depth(hw_feat->tts_fifo_depth));
		seq_printf(seq, "\tEnhancements to Scheduling Traffic Support: %s\n",
			   (hw_feat->est_sel) ? "Y" : "N");
		seq_printf(seq, "\tDepth of the Gate Control List: %s\n",
			   gate_ctl_depth(hw_feat->gcl_depth));
		seq_printf(seq, "\tWidth of the Time Interval field in GCL: %s\n",
			   gate_ctl_width(hw_feat->gcl_width));
		seq_printf(seq, "\tFrame Preemption Enable: %s\n",
			   (hw_feat->fpe_sel) ? "Y" : "N");
		seq_printf(seq, "\tTime Based Scheduling Enable: %s\n",
			   (hw_feat->tbs_sel) ? "Y" : "N");
		seq_printf(seq, "\tNumber of DMA channels enabled for TBS: %u\n",
			   (hw_feat->num_tbs_ch + 1));
	}

	return 0;
}

static int ether_hw_feat_open(struct inode *inode, struct file *file)
{
	return single_open(file, ether_hw_features_read, inode->i_private);
}

static const struct file_operations ether_hw_features_fops = {
	.owner = THIS_MODULE,
	.open = ether_hw_feat_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ether_desc_dump_read(struct seq_file *seq, void *v)
{
	struct net_device *ndev = seq->private;
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	unsigned int num_chan = osi_dma->num_dma_chans;
	struct osi_tx_ring *tx_ring = NULL;
	struct osi_rx_ring *rx_ring = NULL;
	struct osi_tx_desc *tx_desc = NULL;
	struct osi_rx_desc *rx_desc = NULL;
	unsigned int chan;
	unsigned int i;
	unsigned int j;

	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return 0;
	}

	for (i = 0; i < num_chan; i++) {
		chan = osi_dma->dma_chans[i];
		tx_ring = osi_dma->tx_ring[chan];
		rx_ring = osi_dma->rx_ring[chan];

		seq_printf(seq, "\n\tDMA Tx channel %u descriptor dump\n",
			   chan);
		seq_printf(seq, "\tcurrent Tx idx = %u, clean idx = %u\n",
			   tx_ring->cur_tx_idx, tx_ring->clean_idx);
		for (j = 0; j < osi_dma->tx_ring_sz; j++) {
			tx_desc = tx_ring->tx_desc + j;

			seq_printf(seq, "[%03u %p %#llx] = %#x:%#x:%#x:%#x\n",
				   j, tx_desc, virt_to_phys(tx_desc),
				   tx_desc->tdes3, tx_desc->tdes2,
				   tx_desc->tdes1, tx_desc->tdes0);
		}

		seq_printf(seq, "\n\tDMA Rx channel %u descriptor dump\n",
			   chan);
		seq_printf(seq, "\tcurrent Rx idx = %u, refill idx = %u\n",
			   rx_ring->cur_rx_idx, rx_ring->refill_idx);
		for (j = 0; j < osi_dma->rx_ring_sz; j++) {
			rx_desc = rx_ring->rx_desc + j;

			seq_printf(seq, "[%03u %p %#llx] = %#x:%#x:%#x:%#x\n",
				   j, rx_desc, virt_to_phys(rx_desc),
				   rx_desc->rdes3, rx_desc->rdes2,
				   rx_desc->rdes1, rx_desc->rdes0);
		}
	}

	return 0;
}

static int ether_desc_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, ether_desc_dump_read, inode->i_private);
}

static const struct file_operations ether_desc_dump_fops = {
	.owner = THIS_MODULE,
	.open = ether_desc_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ether_register_dump_read(struct seq_file *seq, void *v)
{
	struct net_device *ndev = seq->private;
	struct ether_priv_data *pdata = netdev_priv(ndev);
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	int max_address = 0x0;
	int start_addr = 0x0;
	struct osi_ioctl ioctl_data;
	int ret = 0;

	max_address = EOQS_MAX_REGISTER_ADDRESS;

	/* Interface is not up so register dump not allowed */
	if (!netif_running(ndev)) {
		dev_err(pdata->dev, "Not Allowed. Ether interface is not up\n");
		return -EBUSY;
	}

	while (1) {
		ioctl_data.cmd = OSI_CMD_READ_REG;
		ioctl_data.arg1_u32 = start_addr;
		ret = osi_handle_ioctl(osi_core, &ioctl_data);
		seq_printf(seq,
			   "\t Register offset %x value 0x%x\n",
			   start_addr,
			   ret);
		start_addr += 4;

		if (start_addr > max_address)
			break;
	}

	return 0;
}

static int ether_register_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, ether_register_dump_read, inode->i_private);
}

static const struct file_operations ether_register_dump_fops = {
	.owner = THIS_MODULE,
	.open = ether_register_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ether_create_debugfs(struct ether_priv_data *pdata)
{
	char *buf;
	int ret = 0;

	buf = kasprintf(GFP_KERNEL, "nvethernet-%s", pdata->ndev->name);
	if (!buf)
		return -ENOMEM;

	pdata->dbgfs_dir = debugfs_create_dir(buf, NULL);
	if (!pdata->dbgfs_dir || IS_ERR(pdata->dbgfs_dir)) {
		netdev_err(pdata->ndev,
			   "failed to create debugfs directory\n");
		ret = -ENOMEM;
		goto exit;
	}

	pdata->dbgfs_hw_feat = debugfs_create_file("hw_features", S_IRUGO,
						   pdata->dbgfs_dir,
						   pdata->ndev,
						   &ether_hw_features_fops);
	if (!pdata->dbgfs_hw_feat) {
		netdev_err(pdata->ndev,
			   "failed to create HW features debugfs\n");
		debugfs_remove_recursive(pdata->dbgfs_dir);
		ret = -ENOMEM;
		goto exit;
	}

	pdata->dbgfs_desc_dump = debugfs_create_file("descriptors_dump",
						     S_IRUGO,
						     pdata->dbgfs_dir,
						     pdata->ndev,
						     &ether_desc_dump_fops);
	if (!pdata->dbgfs_desc_dump) {
		netdev_err(pdata->ndev,
			   "failed to create descriptor dump debugfs\n");
		debugfs_remove_recursive(pdata->dbgfs_dir);
		ret = -ENOMEM;
		goto exit;
	}

	pdata->dbgfs_reg_dump = debugfs_create_file("register_dump", S_IRUGO,
						    pdata->dbgfs_dir,
						    pdata->ndev,
						    &ether_register_dump_fops);
	if (!pdata->dbgfs_reg_dump) {
		netdev_err(pdata->ndev,
			   "failed to create rgister dump debugfs\n");
		debugfs_remove_recursive(pdata->dbgfs_dir);
		ret = -ENOMEM;
		goto exit;
	}

exit:
	kfree(buf);
	return ret;
}

static void ether_remove_debugfs(struct ether_priv_data *pdata)
{
	debugfs_remove_recursive(pdata->dbgfs_dir);
}
#endif /* CONFIG_DEBUG_FS */

int ether_sysfs_register(struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;
	int ret = 0;

#ifdef CONFIG_DEBUG_FS
	/* Intentionally ignored the return value of debugfs
	 * and continues to initialize the driver even it fails
	 * to support Linux Production profile
	 */
	ether_create_debugfs(pdata);
#endif

	/* Create nvethernet sysfs group under /sys/devices/<ether_device>/ */
	ret = sysfs_create_group(&dev->kobj, &ether_attribute_group);
	return ret;
}

void ether_sysfs_unregister(struct ether_priv_data *pdata)
{
	struct device *dev = pdata->dev;
#ifdef CONFIG_DEBUG_FS
	ether_remove_debugfs(pdata);
#endif
	/* Remove nvethernet sysfs group under /sys/devices/<ether_device>/ */
	sysfs_remove_group(&dev->kobj, &ether_attribute_group);
}
