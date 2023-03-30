/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <net/udp.h>

/**
 * @brief Ethernet packet context for loopback packet
 */
struct ether_packet_ctxt {
	/** Destination MAC address in Ethernet header */
	unsigned char *dst;
};

/**
 * @brief Ethernet selftests private data
 */
struct ether_test_priv_data {
	/** Ethernet selftest packet context */
	struct ether_packet_ctxt *ctxt;
	/** Ethernet packet type to handover the packet to Rx handler */
	struct packet_type pt;
	/** Packet reception completion indication */
	struct completion comp;
	/** Indication for loopback packet reception process completed */
	bool completed;
};


/**
 * @brief Ethernet packet test header
 */
struct ether_testhdr {
	/** Ethernet selftest data */
	__be64 magic;
};

/**
 * @addtogroup Ethernet selftest helper macros
 *
 * @brief Helper macros for Ethernet selftests
 * @{
 */
#define ETHER_TEST_PKT_MAGIC	0xdeadcafecafedeadULL
#define ETHER_TEST_PKT_SIZE	(sizeof(struct ethhdr) +\
				 sizeof(struct iphdr) +\
				 sizeof(struct udphdr) +\
				 sizeof(struct ether_testhdr))
/* UDP discard protocol port */
#define ETHER_UDP_TEST_PORT	9
#define ETHER_IP_IHL		5
#define ETHER_IP_TTL		32
/** @} */

/**
 * @brief ether_test_get_udp_skb - Fill socket buffer with UDP packet
 *
 * Algorithm: Fills socket buffer with UDP packet.
 *
 * @param[in] pdata: Ethernet OSD private data
 * @param[in] ctxt: Ethernet packet context
 *
 * @retval skb pointer on success
 * @retval NULL on failure.
 */
static struct sk_buff *ether_test_get_udp_skb(struct ether_priv_data *pdata,
					      struct ether_packet_ctxt *ctxt)
{
	struct sk_buff *skb = NULL;
	struct ether_testhdr *testhdr;
	struct ethhdr *ethh;
	struct udphdr *udph;
	struct iphdr *iph;
	int    iplen;

	skb = netdev_alloc_skb(pdata->ndev, ETHER_TEST_PKT_SIZE);
	if (!skb) {
		netdev_err(pdata->ndev, "Failed to allocate loopback skb\n");
		return NULL;
	}

	skb_reserve(skb, NET_IP_ALIGN);

	/*  Reserve for ethernet and IP header  */
	ethh = skb_push(skb, ETH_HLEN);
	skb_reset_mac_header(skb);

	skb_set_network_header(skb, skb->len);
	iph = skb_put(skb, sizeof(struct iphdr));

	skb_set_transport_header(skb, skb->len);
	udph = skb_put(skb, sizeof(struct udphdr));

	/* Fill ETH header */
	ether_addr_copy(ethh->h_dest, ctxt->dst);
	eth_zero_addr(ethh->h_source);
	ethh->h_proto = htons(ETH_P_IP);

	/* Fill UDP header */
	udph->source = htons(ETHER_UDP_TEST_PORT);
	udph->dest = htons(ETHER_UDP_TEST_PORT); /* Discard Protocol */
	udph->len = htons(sizeof(struct ether_testhdr) + sizeof(struct udphdr));
	udph->check = OSI_NONE;

	/* Fill IP header */
	iph->ihl = ETHER_IP_IHL;
	iph->ttl = ETHER_IP_TTL;
	iph->version = IPVERSION;
	iph->protocol = IPPROTO_UDP;
	iplen = sizeof(struct iphdr) + sizeof(struct udphdr) +
		sizeof(struct ether_testhdr);
	iph->tot_len = htons(iplen);
	iph->frag_off = OSI_NONE;
	iph->saddr = OSI_NONE;
	iph->daddr = OSI_NONE;
	iph->tos = OSI_NONE;
	iph->id = OSI_NONE;
	ip_send_check(iph);

	/* Fill test header and data */
	testhdr = skb_put(skb, sizeof(*testhdr));
	testhdr->magic = cpu_to_be64(ETHER_TEST_PKT_MAGIC);

	skb->csum = OSI_NONE;
	skb->ip_summed = CHECKSUM_PARTIAL;
	udp4_hwcsum(skb, iph->saddr, iph->daddr);
	skb->protocol = htons(ETH_P_IP);
	skb->pkt_type = PACKET_HOST;
	skb->dev = pdata->ndev;

	return skb;
}

/**
 * @brief ether_test_loopback_validate - Loopback packet validation
 *
 * Algorithm: Validated received loopback packet with the packet sent.
 *
 * @param[in] skb: socket buffer pointer
 * @param[in] ndev: Network device pointer
 * @param[in] pt: Packet type for ethernet received packet
 * @param[in] orig_dev: Original network device pointer
 *
 * @retval 0 always
 */
static int ether_test_loopback_validate(struct sk_buff *skb,
					struct net_device *ndev,
					struct packet_type *pt,
					struct net_device *orig_ndev)
{
	struct ether_test_priv_data *tpdata = pt->af_packet_priv;
	unsigned char *dst = tpdata->ctxt->dst;
	struct ether_testhdr *thdr;
	struct ethhdr *ehdr;
	struct udphdr *uhdr;
	struct iphdr *ihdr;

	skb = skb_unshare(skb, GFP_ATOMIC);
	if (!skb)
		goto out;

	if (skb_linearize(skb))
		goto out;
	if (skb_headlen(skb) < (ETHER_TEST_PKT_SIZE - ETH_HLEN))
		goto out;

	ehdr = (struct ethhdr *)skb_mac_header(skb);
	if (dst) {
		if (!ether_addr_equal_unaligned(ehdr->h_dest, dst))
			goto out;
	}

	ihdr = ip_hdr(skb);
	if (ihdr->protocol != IPPROTO_UDP)
		goto out;

	uhdr = (struct udphdr *)((u8 *)ihdr + 4 * ihdr->ihl);
	if (uhdr->dest != htons(ETHER_UDP_TEST_PORT))
		goto out;

	thdr = (struct ether_testhdr *)((u8 *)uhdr + sizeof(*uhdr));

	if (thdr->magic != cpu_to_be64(ETHER_TEST_PKT_MAGIC))
		goto out;

	tpdata->completed = true;
	complete(&tpdata->comp);
out:
	kfree_skb(skb);
	return 0;
}

/**
 * @brief ether_test_loopback - Ethernet selftest for loopback
 *
 * Algorithm:
 * 1) It registers Rx handler with network type for specifc packet type.
 * 2) Gets a SKB with UDP/Ethernet headers updated
 * 3) Transmits packet with dev_queue_xmit().
 *
 * @param[in] pdata: Ethernet OSD private data
 * @param[in] ctxt: Ethernet packet context
 *
 * @retval zero on success.
 * @retval negative value on failure.
 */
static int ether_test_loopback(struct ether_priv_data *pdata,
			       struct ether_packet_ctxt *ctxt)
{
	struct ether_test_priv_data *tpdata;
	struct sk_buff *skb = NULL;
	int ret = 0;

	tpdata = kzalloc(sizeof(*tpdata), GFP_KERNEL);
	if (!tpdata)
		return -ENOMEM;

	tpdata->completed = false;
	init_completion(&tpdata->comp);

	tpdata->pt.type = htons(ETH_P_IP);
	tpdata->pt.func = ether_test_loopback_validate;
	tpdata->pt.dev = pdata->ndev;
	tpdata->pt.af_packet_priv = tpdata;
	tpdata->ctxt = ctxt;
	dev_add_pack(&tpdata->pt);

	skb = ether_test_get_udp_skb(pdata, ctxt);
	if (!skb) {
		ret = -ENOMEM;
		goto cleanup;
	}

	skb_set_queue_mapping(skb, 0);
	ret = dev_queue_xmit(skb);
	if (ret)
		goto cleanup;

	if (!wait_for_completion_timeout(&tpdata->comp,
					 msecs_to_jiffies(200))) {
		ret = -ETIMEDOUT;
		goto cleanup;
	}

	ret = !tpdata->completed;
cleanup:
	dev_remove_pack(&tpdata->pt);
	kfree(tpdata);
	return ret;
}

/**
 * @brief ether_test_mac_loopback - Ethernet selftest for MAC loopback
 *
 * @param[in] pdata: Ethernet OSD private data
 *
 * @retval zero on success
 * @retval negative value on failure.
 */
static int ether_test_mac_loopback(struct ether_priv_data *pdata)
{
	struct ether_packet_ctxt ctxt = { };

	ctxt.dst = pdata->ndev->dev_addr;
	return ether_test_loopback(pdata, &ctxt);
}

/**
 * @brief ether_test_phy_loopback - Ethernet selftest for PHY loopback
 *
 * @param[in] pdata: Ethernet OSD private data
 *
 * @retval zero on success
 * @retval negative value on failure.
 */
static int ether_test_phy_loopback(struct ether_priv_data *pdata)
{
	struct ether_packet_ctxt ctxt = { };
	int ret = 0;

	if (!pdata->phydev)
		return -ENODEV;

	ret = phy_loopback(pdata->phydev, true);
	if (ret != 0 && ret != -EBUSY)
		return ret;

	ctxt.dst = pdata->ndev->dev_addr;

	return ether_test_loopback(pdata, &ctxt);
}

/**
 * @brief ether_test_mmc_counters - Ethernet selftest for MMC Counters
 *
 * @param[in] pdata: Ethernet OSD private data
 *
 * @retval zero on success
 * @retval negative value on failure.
 */
static int ether_test_mmc_counters(struct ether_priv_data *pdata)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct osi_ioctl ioctl_data = {};
	unsigned int mmc_tx_framecount_g = 0;
	unsigned int mmc_rx_framecount_gb = 0;
	unsigned int mmc_rx_ipv4_gd = 0;
	unsigned int mmc_rx_udp_gd = 0;
	int ret = 0;

	ioctl_data.cmd = OSI_CMD_READ_MMC;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0)
		return ret;

	mmc_tx_framecount_g = osi_core->mmc.mmc_tx_framecount_g;
	mmc_rx_framecount_gb = osi_core->mmc.mmc_rx_framecount_gb;
	mmc_rx_ipv4_gd = osi_core->mmc.mmc_rx_ipv4_gd;
	mmc_rx_udp_gd = osi_core->mmc.mmc_rx_udp_gd;

	ret = ether_test_mac_loopback(pdata);
	if (ret < 0)
		return ret;
	ioctl_data.cmd = OSI_CMD_READ_MMC;
	ret = osi_handle_ioctl(osi_core, &ioctl_data);
	if (ret < 0)
		return ret;

	if (osi_core->mmc.mmc_tx_framecount_g <= mmc_tx_framecount_g)
		return -1;
	if (osi_core->mmc.mmc_rx_framecount_gb <= mmc_rx_framecount_gb)
		return -1;
	if (osi_core->mmc.mmc_rx_ipv4_gd <= mmc_rx_ipv4_gd)
		return -1;
	if (osi_core->mmc.mmc_rx_udp_gd <= mmc_rx_udp_gd)
		return -1;

	return 0;
}

#define ETHER_LOOPBACK_NONE	0
#define ETHER_LOOPBACK_MAC	1
#define ETHER_LOOPBACK_PHY	2

static const struct ether_test {
	char name[ETH_GSTRING_LEN];
	int lb;
	int (*fn)(struct ether_priv_data *pdata);
} ether_selftests[] = {
	{
		.name = "MAC Loopback		",
		.lb = ETHER_LOOPBACK_MAC,
		.fn = ether_test_mac_loopback,
	}, {
		.name = "PHY Loopback		",
		.lb = ETHER_LOOPBACK_PHY,
		.fn = ether_test_phy_loopback,
	}, {
		.name = "MMC Counters		",
		.lb = ETHER_LOOPBACK_MAC,
		.fn = ether_test_mmc_counters,
	},
};

/**
 * @brief ether_selftest_run - Ethernet selftests.
 *
 * @param[in] dev: Network device pointer.
 * @param[in] etest: Ethernet ethtool test pointer.
 * @param[in] buf: Buffer pointer to hold test status.
 *
 * @retval zero on success.
 * @retval negative value on failure.
 */
void ether_selftest_run(struct net_device *dev,
			struct ethtool_test *etest, u64 *buf)
{
	struct ether_priv_data *pdata = netdev_priv(dev);
	struct osi_ioctl ioctl_data = {};
	int count = ether_selftest_get_count(pdata);
	int carrier = netif_carrier_ok(dev);
	int i, ret;

	if (!netif_running(dev)) {
		netdev_err(dev, "%s(): Interface is not up\n", __func__);
		return;
	}

	memset(buf, 0, sizeof(*buf) * count);

	netif_carrier_off(dev);

	for (i = 0; i < count; i++) {
		ret = 0;

		switch (ether_selftests[i].lb) {
		case ETHER_LOOPBACK_PHY:
			ret = -EOPNOTSUPP;
			if (dev->phydev)
				ret = phy_loopback(dev->phydev, true);
			if (!ret)
				break;
		/* Fallthrough */
		case ETHER_LOOPBACK_MAC:
			if (pdata->osi_core) {
				ioctl_data.cmd = OSI_CMD_MAC_LB;
				ioctl_data.arg1_u32 = OSI_ENABLE;
				ret = osi_handle_ioctl(pdata->osi_core,
						       &ioctl_data);
			}
			break;
		default:
			ret = -EOPNOTSUPP;
			break;
		}

		if (ret) {
			netdev_err(dev, "Loopback is not supported\n");
			etest->flags |= ETH_TEST_FL_FAILED;
			break;
		}

		ret = ether_selftests[i].fn(pdata);
		if (ret && (ret != -EOPNOTSUPP))
			etest->flags |= ETH_TEST_FL_FAILED;
		buf[i] = ret;

		switch (ether_selftests[i].lb) {
		case ETHER_LOOPBACK_PHY:
			ret = -EOPNOTSUPP;
			if (dev->phydev)
				ret = phy_loopback(dev->phydev, false);
			if (!ret)
				break;
		/* Fallthrough */
		case ETHER_LOOPBACK_MAC:
			if (pdata->osi_core) {
				ioctl_data.cmd = OSI_CMD_MAC_LB;
				ioctl_data.arg1_u32 = OSI_DISABLE;
				ret = osi_handle_ioctl(pdata->osi_core,
						       &ioctl_data);
			}
			if (!ret)
				break;
		default:
			break;
		}
	}

	/* Restart everything */
	if (carrier)
		netif_carrier_on(dev);
}

/**
 * @brief ether_selftest_get_strings - Getting strings for selftests
 *
 * @param[in] pdata: Ethernet OSD private data
 * @param[in] data: Data pointer to store strings for selftest names
 */
void ether_selftest_get_strings(struct ether_priv_data *pdata, u8 *data)
{
	u8 *p = data;
	int i;

	for (i = 0; i < ether_selftest_get_count(pdata); i++) {
		scnprintf(p, ETH_GSTRING_LEN, "%2d. %s", i + 1,
			  ether_selftests[i].name);
		p += ETH_GSTRING_LEN;
	}
}

/**
 * @brief ether_selftest_get_count - Count for Ethernet selftests
 *
 * @param[in] pdata: Ethernet OSD private data
 *
 * @retval Number of Ethernet selftests
 */
int ether_selftest_get_count(struct ether_priv_data *pdata)
{
	return ARRAY_SIZE(ether_selftests);
}
