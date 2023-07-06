/*
 * Copyright (c) 2018-2023, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef ETHER_LINUX_H
#define ETHER_LINUX_H

#include <linux/platform/tegra/ptp-notifier.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/etherdevice.h>
#include <linux/net_tstamp.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/of_mdio.h>
#include <linux/if_vlan.h>
#include <linux/thermal.h>
#include <linux/debugfs.h>
#include <linux/of_net.h>
#include <linux/module.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/of.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/version.h>
#include <linux/list.h>
#include <net/pkt_sched.h>
#include <linux/tegra-ivc.h>
#if (KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE)
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#if IS_ENABLED(CONFIG_PAGE_POOL)
#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
#include <net/page_pool.h>
#define ETHER_PAGE_POOL
#endif
#endif
#include <osi_core.h>
#include <osi_dma.h>
#include <mmc.h>
#include <ivc_core.h>
#include "ioctl.h"
#ifdef MACSEC_SUPPORT
#include "macsec.h"
#endif
#ifdef ETHER_NVGRO
#include <net/inet_common.h>
#include <uapi/linux/ip.h>
#include <net/udp.h>
#endif /* ETHER_NVGRO */

/**
 * @brief Constant for CBS value calculate
 */
#define ETH_1K				1000
#define MULTIPLIER_32			32
#define MULTIPLIER_8			8
#define MULTIPLIER_4			4
/**
 * @brief Max number of Ethernet IRQs supported in HW
 */
#define ETHER_MAX_IRQS			4
/**
 * @brief Maximum index for IRQ numbers array.
 */
#define ETHER_IRQ_MAX_IDX		9
/**
 * @brief Size of Ethernet IRQ name.
 */
#define ETHER_IRQ_NAME_SZ		32
/**
 * @brief CPU to handle ethernet common interrupt
 */
#define ETHER_COMMON_IRQ_DEFAULT_CPU	4U

/**
 * @addtogroup MAC address DT string
 */
#define ETH_MAC_STR_LEN			20

/**
 * @addtogroup Ethernet Transmit Queue Priority
 *
 * @brief Macros to define the default, maximum and invalid range of Transmit
 * queue priority. These macros are used to check the bounds of Tx queue
 * priority provided in the device tree.
 * @{
 */
#define ETHER_QUEUE_PRIO_DEFAULT	0U
#define ETHER_QUEUE_PRIO_MAX		7U
#define ETHER_QUEUE_PRIO_INVALID	0xFFU
/** @} */

/**
 * @brief Ethernet default PTP clock frequency
 */
#define ETHER_DFLT_PTP_CLK		312500000U

/**
 * @brief Ethernet default PTP default RxQ
 */
#define ETHER_DEFAULT_PTP_QUEUE		3U

/**
 * @brief SEC to MSEC converter
 */
#define ETHER_SECTOMSEC			1000U

/**
 * @brief Ethernet clk rates
 */
#define ETHER_RX_INPUT_CLK_RATE		125000000UL
#define ETHER_MGBE_MAC_DIV_RATE_10G	312500000UL
#define ETHER_MGBE_MAC_DIV_RATE_5G	156250000UL
#define ETHER_MGBE_MAC_DIV_RATE_2_5G	78125000UL
// gbe_pll2_txclkref (644 MHz) --> programmable link TX_CLK divider
// --> link_Tx_clk --> fixed 1/2 gear box divider --> lane TX clk.
#define ETHER_MGBE_TX_CLK_USXGMII_10G	644531250UL
#define ETHER_MGBE_TX_CLK_USXGMII_5G	322265625UL
#define ETHER_MGBE_RX_CLK_USXGMII_10G	644531250UL
#define ETHER_MGBE_RX_CLK_USXGMII_5G	322265625UL
#define ETHER_MGBE_TX_PCS_CLK_USXGMII_10G	156250000UL
#define ETHER_MGBE_TX_PCS_CLK_USXGMII_5G	78125000UL
#define ETHER_MGBE_RX_PCS_CLK_USXGMII_10G	156250000UL
#define ETHER_MGBE_RX_PCS_CLK_USXGMII_5G	78125000UL
#define ETHER_EQOS_TX_CLK_1000M		125000000UL
#define ETHER_EQOS_TX_CLK_100M		25000000UL
#define ETHER_EQOS_TX_CLK_10M		2500000UL

/**
 * @brief 1 Second in Neno Second
 */
#define ETHER_ONESEC_NENOSEC           1000000000ULL

/**
 * @addtogroup CONFIG Ethernet configuration error codes
 *
 * @brief Error codes for fail/success.
 * @{
 */
#define EQOS_CONFIG_FAIL		-3
#define EQOS_CONFIG_SUCCESS		0
/** @} */

/**
 * @addtogroup ADDR Ethernet MAC address register count
 *
 * @brief MAC L2 address filter count
 * @{
 */
#define ETHER_ADDR_REG_CNT_128		128
#define ETHER_ADDR_REG_CNT_64		64
#define ETHER_ADDR_REG_CNT_32		32
#define ETHER_ADDR_REG_CNT_1		1
/** @} */

/**
 * @addtogroup HW MAC HW Filter Hash Table size
 *
 * @brief Represents Hash Table sizes.
 * @{
 */
#define HW_HASH_TBL_SZ_3		3
#define HW_HASH_TBL_SZ_2		2
#define HW_HASH_TBL_SZ_1		1
#define HW_HASH_TBL_SZ_0		0
/** @} */

/**
 * @brief Max pending SKB count
 */
#define ETHER_MAX_PENDING_SKB_CNT	(64 * OSI_MGBE_MAX_NUM_CHANS)

/**
 * @brief Maximum buffer length per DMA descriptor (16KB).
 */
#define ETHER_TX_MAX_BUFF_SIZE	0x3FFF

/**
 * @brief Maximum skb frame(GSO/TSO) size (64KB)
 */
#define ETHER_TX_MAX_FRAME_SIZE	GSO_MAX_SIZE

/**
 * @brief IVC wait timeout cnt in micro seconds.
 */
#define IVC_WAIT_TIMEOUT_CNT		200000

/**
 * @brief Broadcast and MAC address macros
 */
#define ETHER_MAC_ADDRESS_INDEX		1U
#define ETHER_BC_ADDRESS_INDEX		0
#define ETHER_ADDRESS_MAC		1
#define ETHER_ADDRESS_BC		0

#ifdef ETHER_NVGRO
/* NVGRO packets purge threshold in msec */
#define NVGRO_AGE_THRESHOLD		500
#define NVGRO_PURGE_TIMER_THRESHOLD	5000
#define NVGRO_RX_RUNNING		OSI_BIT(0)
#define NVGRO_PURGE_TIMER_RUNNING	OSI_BIT(1)
#endif

/**
 * @brief Invalid MDIO address for fixed link
 */
#define FIXED_PHY_INVALID_MDIO_ADDR	0xFFU


/**
 * @brief Check if Tx data buffer length is within bounds.
 *
 * Algorithm: Check the data length if it is valid.
 *
 * @param[in] length: Tx data buffer length to check
 *
 * @retval true if length is valid
 * @retval false otherwise
 */
static inline bool valid_tx_len(unsigned int length)
{
	if (length > 0U && length <= ETHER_TX_MAX_FRAME_SIZE) {
		return true;
	} else {
		return false;
	}
}

/* Descriptors required for maximum contiguous TSO/GSO packet
 * one extra descriptor if there is linear buffer payload
 */
#define ETHER_TX_MAX_SPLIT	((GSO_MAX_SIZE / ETHER_TX_MAX_BUFF_SIZE) + 1)

/* Maximum possible descriptors needed for an SKB:
 * - Maximum number of SKB frags
 * - Maximum descriptors for contiguous TSO/GSO packet
 * - Possible context descriptor
 * - Possible TSO header descriptor
 */
#define ETHER_TX_DESC_THRESHOLD	(MAX_SKB_FRAGS + ETHER_TX_MAX_SPLIT + 2)

#define ETHER_TX_MAX_FRAME(x)	((x) / ETHER_TX_DESC_THRESHOLD)
/**
 *@brief Returns count of available transmit descriptors
 *
 * Algorithm: Check the difference between current descriptor index
 * and the descriptor index to be cleaned.
 *
 * @param[in] tx_ring: Tx ring instance associated with channel number
 *
 * @note MAC needs to be initialized and Tx ring allocated.
 *
 * @returns Number of available descriptors in the given Tx ring.
 */
static inline int ether_avail_txdesc_cnt(struct osi_dma_priv_data *osi_dma,
					 struct osi_tx_ring *tx_ring)
{
	return ((tx_ring->clean_idx - tx_ring->cur_tx_idx - 1) &
		(osi_dma->tx_ring_sz - 1));
}

/**
 * @brief Timer to trigger Work queue periodically which read HW counters
 * and store locally. If data is at line rate, 2^32 entry get will filled in
 * 36 second for 1 G interface and 3.6 sec for 10 G interface.
 */
#define ETHER_STATS_TIMER		3000U

/**
 * @brief Timer to trigger Work queue periodically which read TX timestamp
 * for PTP packets. Timer is in milisecond.
 */
#define ETHER_TS_MS_TIMER			1U

#define ETHER_VM_IRQ_TX_CHAN_MASK(x)	BIT((x) * 2U)
#define ETHER_VM_IRQ_RX_CHAN_MASK(x)	BIT(((x) * 2U) + 1U)

/**
 * @brief DMA Transmit Channel NAPI
 */
struct ether_tx_napi {
	/** Transmit channel number */
	unsigned int chan;
	/** OSD private data */
	struct ether_priv_data *pdata;
	/** NAPI instance associated with transmit channel */
	struct napi_struct napi;
	/** SW timer associated with transmit channel */
	struct hrtimer tx_usecs_timer;
	/** SW timer flag associated with transmit channel */
	atomic_t tx_usecs_timer_armed;
};

/**
 *@brief DMA Receive Channel NAPI
 */
struct ether_rx_napi {
	/** Receive channel number */
	unsigned int chan;
	/** OSD private data */
	struct ether_priv_data *pdata;
	/** NAPI instance associated with transmit channel */
	struct napi_struct napi;
};

/**
 * @brief VM Based IRQ data
 */
struct ether_vm_irq_data {
	/** List of DMA Tx/Rx channel mask */
	unsigned int chan_mask;
	/** OSD private data */
	struct ether_priv_data *pdata;
};

/**
 * @brief Ethernet IVC context
 */
struct ether_ivc_ctxt {
	/** ivc cookie */
	struct tegra_hv_ivc_cookie *ivck;
	/** ivc lock */
	raw_spinlock_t ivck_lock;
	/** Flag to indicate ivc started or stopped */
	unsigned int ivc_state;
};

/**
 * @brief local L2 filter table structure
 */
struct ether_mac_addr {
	/** L2 address */
	unsigned char addr[ETH_ALEN];
	/** DMA channel to route packets */
	unsigned int dma_chan;
};

/**
 * @brief tx timestamp pending skb list node structure
 */
struct ether_tx_ts_skb_list {
	/** Link list node head */
	struct list_head list_head;
	/** if node is in use */
	unsigned int in_use;
	/** skb pointer */
	struct sk_buff *skb;
	/** packet id to identify timestamp */
	unsigned int pktid;
	/** SKB jiffies to find time */
	unsigned long pkt_jiffies;
};

/**
 * @brief ether_xtra_stat_counters - OSI core extra stat counters
 */
struct ether_xtra_stat_counters {
	/** rx skb allocation failure count */
	nveu64_t re_alloc_rxbuf_failed[OSI_MGBE_MAX_NUM_QUEUES];
	/** TX per channel interrupt count */
	nveu64_t tx_normal_irq_n[OSI_MGBE_MAX_NUM_QUEUES];
	/** TX per channel SW timer callback count */
	nveu64_t tx_usecs_swtimer_n[OSI_MGBE_MAX_NUM_QUEUES];
	/** RX per channel interrupt count */
	nveu64_t rx_normal_irq_n[OSI_MGBE_MAX_NUM_QUEUES];
	/** link connect count */
	nveu64_t link_connect_count;
	/** link disconnect count */
	nveu64_t link_disconnect_count;
};

/**
 * @brief Ethernet driver private data
 */
struct ether_priv_data {
	/** OSI core private data */
	struct osi_core_priv_data *osi_core;
	/** OSI DMA private data */
	struct osi_dma_priv_data *osi_dma;
	/** HW supported feature list */
	struct osi_hw_features hw_feat;
	/** Array of DMA Transmit channel NAPI */
	struct ether_tx_napi *tx_napi[OSI_MGBE_MAX_NUM_CHANS];
	/** Array of DMA Receive channel NAPI */
	struct ether_rx_napi *rx_napi[OSI_MGBE_MAX_NUM_CHANS];
	/** Network device associated with driver */
	struct net_device *ndev;
	/** Base device associated with driver */
	struct device *dev;
	/** Reset for the MAC */
	struct reset_control *mac_rst;
	/** Reset for the XPCS */
	struct reset_control *xpcs_rst;
	/** PLLREFE clock */
	struct clk *pllrefe_clk;
	/** Clock from AXI */
	struct clk *axi_clk;
	/** Clock from AXI CBB */
	struct clk *axi_cbb_clk;
	/** Receive clock (which will be driven from the PHY) */
	struct clk *rx_clk;
	/** PTP reference clock from AXI */
	struct clk *ptp_ref_clk;
	/** Transmit clock */
	struct clk *tx_clk;
	/** Transmit clock divider */
	struct clk *tx_div_clk;
	/** Receive Monitoring clock */
	struct clk *rx_m_clk;
	/** RX PCS monitoring clock */
	struct clk *rx_pcs_m_clk;
	/** RX PCS input clock */
	struct clk *rx_pcs_input_clk;
	/** RX PCS clock */
	struct clk *rx_pcs_clk;
	/** TX PCS clock */
	struct clk *tx_pcs_clk;
	/** MAC DIV clock */
	struct clk *mac_div_clk;
	/** MAC clock */
	struct clk *mac_clk;
	/** EEE PCS clock */
	struct clk *eee_pcs_clk;
	/** APP clock */
	struct clk *app_clk;
	/** MAC Rx input clk */
	struct clk *rx_input_clk;
	/** Pointer to PHY device tree node */
	struct device_node *phy_node;
	/** Pointer to MDIO device tree node */
	struct device_node *mdio_node;
	/** Pointer to MII bus instance */
	struct mii_bus *mii;
	/** Pointer to the PHY device */
	struct phy_device *phydev;
	/** Interface type assciated with MAC (SGMII/RGMII/...)
	 * this information will be provided with phy-mode DT entry */
	phy_interface_t interface;
	/** Previous detected link */
	unsigned int oldlink;
	/** PHY link speed */
	int speed;
	/** Previous detected mode */
	int oldduplex;
	/** Reset for PHY */
	int phy_reset;
	/** Rx IRQ alloc mask */
	unsigned int rx_irq_alloc_mask;
	/** Tx IRQ alloc mask */
	unsigned int tx_irq_alloc_mask;
	/** Common IRQ alloc mask */
	unsigned int common_irq_alloc_mask;
	/** Common IRQ number for MAC */
	int common_irq;
	/** CPU affinity mask for Common IRQ */
	cpumask_t common_isr_cpu_mask;
	/** CPU ID for handling Common IRQ */
	unsigned int common_isr_cpu_id;
	/** Array of DMA Transmit channel IRQ numbers */
	int tx_irqs[ETHER_MAX_IRQS];
	/** Array of DMA Receive channel IRQ numbers */
	int rx_irqs[ETHER_MAX_IRQS];
	/** Array of VM IRQ numbers */
	int vm_irqs[OSI_MAX_VM_IRQS];
	/** IRQ name */
	char irq_names[ETHER_IRQ_MAX_IDX][ETHER_IRQ_NAME_SZ];
	/** memory allocation mask */
	unsigned long long dma_mask;
	/** Current state of features enabled in HW*/
	netdev_features_t hw_feat_cur_state;
	/** MAC loopback mode */
	unsigned int mac_loopback_mode;
	/** Array of MTL queue TX priority */
	unsigned int txq_prio[OSI_MGBE_MAX_NUM_CHANS];
	/** Spin lock for Tx/Rx interrupt enable registers */
	raw_spinlock_t rlock;
	/** max address register count, 2*mac_addr64_sel */
	int num_mac_addr_regs;
	/** Last address reg filter index added in last call*/
	unsigned int last_filter_index;
	/** vlan hash filter 1: hash, 0: perfect */
	unsigned int vlan_hash_filtering;
	/** L2 filter mode */
	unsigned int l2_filtering_mode;
	/** PTP clock operations structure */
	struct ptp_clock_info ptp_clock_ops;
	/** PTP system clock */
	struct ptp_clock *ptp_clock;
	/** PTP reference clock supported by platform */
	unsigned int ptp_ref_clock_speed;
	/** HW tx time stamping enable */
	unsigned int hwts_tx_en;
	/** HW rx time stamping enable */
	unsigned int hwts_rx_en;
	/** Max MTU supported by platform */
	unsigned int max_platform_mtu;
	/** Spin lock for PTP registers */
	raw_spinlock_t ptp_lock;
	/** Clocks enable check */
	bool clks_enable;
	/** Promiscuous mode support, configuration in DT */
	unsigned int promisc_mode;
	/** Delayed work queue to read RMON counters periodically */
	struct delayed_work ether_stats_work;
	/** set speed work */
	struct delayed_work set_speed_work;
	/** Flag to check if EEE LPI is enabled for the MAC */
	unsigned int eee_enabled;
	/** Flag to check if EEE LPI is active currently */
	unsigned int eee_active;
	/** Flag to check if EEE LPI is enabled for MAC transmitter */
	unsigned int tx_lpi_enabled;
	/** Time (usec) MAC waits to enter LPI after Tx complete */
	unsigned int tx_lpi_timer;
	/** ivc context */
	struct ether_ivc_ctxt ictxt;
	/** VM channel info data associated with VM IRQ */
	struct ether_vm_irq_data *vm_irq_data;
#ifdef ETHER_PAGE_POOL
	/** Pointer to page pool */
	struct page_pool *page_pool;
#endif
#ifdef CONFIG_DEBUG_FS
	/** Debug fs directory pointer */
	struct dentry *dbgfs_dir;
	/** HW features dump debug fs pointer */
	struct dentry *dbgfs_hw_feat;
	/** Descriptor dump debug fs pointer */
	struct dentry *dbgfs_desc_dump;
	/** Register dump debug fs pointer */
	struct dentry *dbgfs_reg_dump;
#endif
#ifdef MACSEC_SUPPORT
	/** MACsec priv data */
	struct macsec_priv_data *macsec_pdata;
#endif /* MACSEC_SUPPORT */
	/** local L2 filter address list head pointer */
	struct ether_mac_addr mac_addr[ETHER_ADDR_REG_CNT_128];
	/** skb tx timestamp update work queue */
	struct delayed_work tx_ts_work;
	/** local skb list head */
	struct list_head tx_ts_skb_head;
	/** pre allocated memory for ether_tx_ts_skb_list list */
	struct ether_tx_ts_skb_list tx_ts_skb[ETHER_MAX_PENDING_SKB_CNT];
	/** Atomic variable to hold the current pad calibration status */
	atomic_t padcal_in_progress;
	/** eqos dev pinctrl handle */
	struct pinctrl		*pin;
	/** eqos rgmii rx input pins enable state */
	struct pinctrl_state	*mii_rx_enable_state;
	/** eqos rgmii rx input pins disable state */
	struct pinctrl_state	*mii_rx_disable_state;
	/** PHY reset Post delay */
	int phy_reset_post_delay;
	/** PHY reset duration delay */
	int phy_reset_duration;
#ifdef ETHER_NVGRO
	/** Master queue */
	struct sk_buff_head mq;
	/** Master queue */
	struct sk_buff_head fq;
	/** expected IP ID */
	u16 expected_ip_id;
	/** Timer for purginging the packets in FQ and MQ based on threshold */
	struct timer_list nvgro_timer;
	/** Rx processing state for NVGRO */
	atomic_t rx_state;
	/** Purge timer state for NVGRO */
	atomic_t timer_state;
	/** NVGRO packet age threshold in milseconds */
	u32 pkt_age_msec;
	/** NVGRO purge timer interval */
	u32 nvgro_timer_intrvl;
	/** NVGRO packet dropped count */
	u64 nvgro_dropped;
#endif
	/** Platform MDIO address */
	unsigned int mdio_addr;
	/** Skip MAC reset */
	unsigned int skip_mac_reset;
	/** Fixed link enable/disable */
	unsigned int fixed_link;
	/** Flag to represent rx_m clk enabled or not */
	bool rx_m_enabled;
	/** Flag to represent rx_pcs_m clk enabled or not */
	bool rx_pcs_m_enabled;
	/* Timer value in msec for ether_stats_work thread */
	unsigned int stats_timer;
#ifdef HSI_SUPPORT
	/** Delayed work queue for error reporting */
	struct delayed_work ether_hsi_work;
	/** HSI lock */
	struct mutex hsi_lock;
#endif
	/** Protect critical section of TX TS SKB list */
	raw_spinlock_t txts_lock;
	/** Ref count for ether_get_tx_ts_func */
	atomic_t tx_ts_ref_cnt;
	/** Ref count for set_speed_work_func */
	atomic_t set_speed_ref_cnt;
	/** flag to enable logs using ethtool */
	u32 msg_enable;
	/** flag to indicate to start/stop the Tx */
	unsigned int tx_start_stop;
	/** Tasklet for restarting UPHY lanes */
	struct tasklet_struct lane_restart_task;
	/** xtra sw error counters */
	struct ether_xtra_stat_counters xstats;
};

/**
 * @brief Set ethtool operations
 *
 * @param[in] ndev: network device instance
 *
 * @note Network device needs to created.
 */
void ether_set_ethtool_ops(struct net_device *ndev);
/**
 * @brief Creates Ethernet sysfs group
 *
 * @param[in] pdata: Ethernet driver private data
 *
 * @retval 0 - success,
 * @retval "negative value" - failure.
 */
int ether_sysfs_register(struct ether_priv_data *pdata);

/**
 * @brief Removes Ethernet sysfs group
 *
 * @param[in] pdata: Ethernet driver private data
 *
 * @note  nvethernet sysfs group need to be registered during probe.
 */
void ether_sysfs_unregister(struct ether_priv_data *pdata);

/**
 * @brief Function to register ptp clock driver
 *
 * Algorithm: This function is used to register the ptp clock driver to kernel
 *
 * @param[in] pdata: Pointer to private data structure.
 *
 * @note Driver probe need to be completed successfully with ethernet
 * 	 network device created
 *
 * @retval 0 on success
 * @retval "negative value" on Failure
 */
int ether_ptp_init(struct ether_priv_data *pdata);

/**
 * @brief Function to unregister ptp clock driver
 *
 * Algorithm: This function is used to remove ptp clock driver from kernel.
 *
 * @param[in] pdata: Pointer to private data structure.
 *
 * @note PTP clock driver need to be successfully registered during init
 */
void ether_ptp_remove(struct ether_priv_data *pdata);

/**
 * @brief Function to handle PTP settings.
 *
 * Algorithm: This function is used to handle the hardware PTP settings.
 *
 * @param[in] pdata: Pointer to private data structure.
 * @param[in] ifr: Interface request structure used for socket ioctl
 *
 * @note PTP clock driver need to be successfully registered during
 * 	 initialization and HW need to support PTP functionality.
 *
 * @retval 0 on success
 * @retval "negative value" on Failure
 */
int ether_handle_hwtstamp_ioctl(struct ether_priv_data *pdata,
				struct ifreq *ifr);
int ether_handle_priv_ts_ioctl(struct ether_priv_data *pdata,
			       struct ifreq *ifr);
int ether_conf_eee(struct ether_priv_data *pdata, unsigned int tx_lpi_enable);

/**
 * @brief ether_padctrl_mii_rx_pins - Enable/Disable RGMII Rx pins.
 *
 * @param[in] priv: OSD private data structure.
 * @param[in] enable: Enable/Disable EQOS RGMII Rx pins
 *
 * @retval 0 on success
 * @retval negative value on failure.
 */
int ether_padctrl_mii_rx_pins(void *priv, unsigned int enable);

#if IS_ENABLED(CONFIG_NVETHERNET_SELFTESTS)
void ether_selftest_run(struct net_device *dev,
			struct ethtool_test *etest, u64 *buf);
void ether_selftest_get_strings(struct ether_priv_data *pdata, u8 *data);
int ether_selftest_get_count(struct ether_priv_data *pdata);
#else
static inline void ether_selftest_run(struct net_device *dev,
				      struct ethtool_test *etest, u64 *buf)
{
}
static inline void ether_selftest_get_strings(struct ether_priv_data *pdata,
					      u8 *data)
{
}
static inline int ether_selftest_get_count(struct ether_priv_data *pdata)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_NVETHERNET_SELFTESTS */

/**
 * @brief ether_assign_osd_ops - Assigns OSD ops for OSI
 *
 * @param[in] osi_core: OSI CORE data structure
 * @param[in] osi_dma: OSI DMA data structure.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void ether_assign_osd_ops(struct osi_core_priv_data *osi_core,
			  struct osi_dma_priv_data *osi_dma);

/**
 * @brief osd_ivc_send_cmd - OSD ivc send cmd
 *
 * @param[in] priv: OSD private data
 * @param[in] ivc_buf: ivc_msg_common structure
 * @param[in] len: length of data
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 */
int osd_ivc_send_cmd(void *priv, ivc_msg_common_t *ivc_buf,
		     unsigned int len);

void ether_set_rx_mode(struct net_device *dev);

#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
/**
 * @brief Function to configure traffic class
 *
 * Algorithm: This function is used to handle the hardware TC
 * settings.
 *
 * @param[in] pdata: Pointer to private data structure.
 * @param[in] qopt:  Pointer to qdisc taprio offload data.
 *
 * @note MAC interface should be up.
 *
 * @retval 0 on success
 * @retval "negative value" on Failure
 */
int ether_tc_setup_taprio(struct ether_priv_data *pdata,
			  struct tc_taprio_qopt_offload *qopt);

/**
 * @brief Function to configure credit base shapper
 *
 * Algorithm: This function is used to handle the hardware CBS
 * settings.
 *
 * @param[in] pdata: Pointer to private data structure.
 * @param[in] qopt:  Pointer to qdisc taprio offload data.
 *
 * @note MAC interface should be up.
 *
 * @retval 0 on success
 * @retval "negative value" on Failure
 */
int ether_tc_setup_cbs(struct ether_priv_data *pdata,
		       struct tc_cbs_qopt_offload *qopt);

#endif

/**
 * @brief Get Tx done timestamp from OSI and update in skb
 *
 * @param[in] pdata: Pointer to private data structure.
 *
 * @note Network interface should be up
 *
 * @retval 0 on success
 * @retval EAGAIN on Failure
 */
int ether_get_tx_ts(struct ether_priv_data *pdata);
void ether_restart_lane_bringup_task(struct tasklet_struct *t);
#ifdef ETHER_NVGRO
void ether_nvgro_purge_timer(struct timer_list *t);
#endif /* ETHER_NVGRO */
#endif /* ETHER_LINUX_H */
