/*
 *
 * If you received this File from Marvell, you may opt to use, redistribute and/or
 * modify this File in accordance with the terms and conditions of the General
 * Public License Version 2, June 1991 (the "GPL License"), a copy of which is
 * available along with the File in the license.txt file or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
 * on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this warranty
 * disclaimer.
 *
 */
#ifndef H_OAK_NET
#define H_OAK_NET

#include "oak_unimac.h" /* Include for relation to classifier oak_unimac */
#include "oak_ctl.h" /* Include for relation to classifier oak_ctl */
#include "linux/ip.h" /* Include for relation to classifier linux/ip.h */
#include "linux/ipv6.h" /* Include for relation to classifier linux/ipv6.h */
#include "linux/if_vlan.h" /* Include for relation to classifier linux/if_vlan.h */

#define OAK_ONEBYTE 1

extern int debug;
extern int rxs;
extern int txs;
extern int chan;
extern int rto;
extern int mhdr;
extern int port_speed;

/* Name      : esu_set_mtu
 * Returns   : int
 * Parameters:  struct net_device * net_dev = net_dev,  int new_mtu = new_mtu
 * Description: This function set the MTU size of the Ethernet interface.
 *  */
int oak_net_esu_set_mtu(struct net_device* net_dev, int new_mtu);

/* Name      : oak_set_mtu_config
 * Returns   : void
 * Parameters:  struct net_device *netdev
 * Description: This function sets the min and max MTU size in the linux netdev.
 */
void oak_set_mtu_config(struct net_device *netdev);

/* Name      : esu_ena_speed
 * Returns   : void
 * Parameters:  int gbit = gbit,  oak_t * np = np
 *  */
void oak_net_esu_ena_speed(int gbit, oak_t* np);

/* Name      : rbr_refill
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t ring = ring
 *  */
int oak_net_rbr_refill(oak_t* np, uint32_t ring);

/* Name      : open
 * Returns   : int
 * Parameters:  struct net_device * net_dev
 *  */
int oak_net_open(struct net_device* net_dev);

/* Name      : close
 * Returns   : int
 * Parameters:  struct net_device * net_dev
 *  */
int oak_net_close(struct net_device* net_dev);

/* Name      : ioctl
 * Returns   : int
 * Parameters:  struct net_device * net_dev,  struct ifreq * ifr,  int cmd
 *  */
int oak_net_ioctl(struct net_device* net_dev, struct ifreq* ifr, int cmd);

/* Name      : add_napi
 * Returns   : void
 * Parameters:  struct net_device * netdev
 *  */
void oak_net_add_napi(struct net_device* netdev);

/* Name      : del_napi
 * Returns   : void
 * Parameters:  struct net_device * netdev
 *  */
void oak_net_del_napi(struct net_device* netdev);

/* Name      : set_mac_addr
 * Returns   : int
 * Parameters:  struct net_device * dev = dev,  void * p_addr = addr
 *  */
int oak_net_set_mac_addr(struct net_device* dev, void* p_addr);

/* Name      : alloc_page
 * Returns   : struct page *
 * Parameters:  oak_t * np = np,  dma_addr_t * dma = dma,  int direction = direction
 *  */
struct page* oak_net_alloc_page(oak_t* np, dma_addr_t* dma, int direction);

/* Name      : select_queue
 * Returns   : uint16_t
 * Parameters:  struct net_device * dev = dev,  struct sk_buff * skb = skb,  struct net_device * sb_dev = sb_dev
 *  */
uint16_t oak_net_select_queue(struct net_device* dev, struct sk_buff* skb, struct net_device* sb_dev);

/* Name      : xmit_frame
 * Returns   : int
 * Parameters:  struct sk_buff * skb,  struct net_device * net_dev
 *  */
int oak_net_xmit_frame(struct sk_buff* skb, struct net_device* net_dev);

/* Name      : process_tx_pkt
 * Returns   : int
 * Parameters:  oak_tx_chan_t * txc = txc,  int desc_num = desc_num
 *  */
int oak_net_process_tx_pkt(oak_tx_chan_t* txc, int desc_num);

/* Name      : start_all
 * Returns   : int
 * Parameters:  oak_t * np = np
 *  */
int oak_net_start_all(oak_t* np);

/* Name      : stop_all
 * Returns   : void
 * Parameters:  oak_t * np = np
 *  */
void oak_net_stop_all(oak_t* np);

/* Name      : tx_stats
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc = txc,  int len = len
 *  */
void oak_net_tx_stats(oak_tx_chan_t* txc, int len);

/* Name      : rx_stats
 * Returns   : void
 * Parameters:  oak_rx_chan_t * rxc = rxc,  int len = len
 *  */
void oak_net_rx_stats(oak_rx_chan_t* rxc, int len);

/* Name      : tbr_free
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txp = txp
 *  */
void oak_net_tbr_free(oak_tx_chan_t* txp);

/* Name      : rbr_free
 * Returns   : void
 * Parameters:  oak_rx_chan_t * rxp = rxp
 *  */
void oak_net_rbr_free(oak_rx_chan_t* rxp);

/* Name      : get_stats
 * Returns   : void
 * Parameters:  oak_t * np,  uint64_t * data
 *  */
void oak_net_get_stats(oak_t* np, uint64_t* data);

/* Name      : add_txd_length
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc,  uint16_t len
 *  */
void oak_net_add_txd_length(oak_tx_chan_t* txc, uint16_t len);

/* Name      : set_txd_first
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc,  uint16_t len,  uint32_t g3,  uint32_t g4,  dma_addr_t map,  uint32_t sz,  int flags
 *  */
void oak_net_set_txd_first(oak_tx_chan_t* txc, uint16_t len, uint32_t g3, uint32_t g4, dma_addr_t map, uint32_t sz, int flags);

/* Name      : set_txd_page
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc,  uint16_t len,  dma_addr_t map,  uint32_t sz,  int flags
 *  */
void oak_net_set_txd_page(oak_tx_chan_t* txc, uint16_t len, dma_addr_t map, uint32_t sz, int flags);

/* Name      : set_txd_last
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc,  struct sk_buff * skb,  struct page * page
 *  */
void oak_net_set_txd_last(oak_tx_chan_t* txc, struct sk_buff* skb, struct page* page);

/* Name      : oak_net_pcie_get_width_cap
 * Returns   : enum pcie_link_width
 * Parameters: struct pci_dev *
 *  */
enum pcie_link_width oak_net_pcie_get_width_cap(struct pci_dev *dev);

/* Name        : oak_net_skb_tx_protocol_type 
 * Returns     : int
 * Parameters  : struct sk_buff *skb
 * Description : This function returns the transmit frames protocol 
 *               type for deciding the checksum offload configuration.
 *  */
int oak_net_skb_tx_protocol_type(struct sk_buff* skb);
#endif /* #ifndef H_OAK_NET */

