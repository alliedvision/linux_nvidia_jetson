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
#ifndef H_OAK_CHKSUM
#define H_OAK_CHKSUM

/* Checksum configurations supported by the Oak HW */
#define OAK_CHKSUM_TYPE (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | NETIF_F_RXCSUM)

#define L3_L4_CHKSUM 2
#define L3_CHKSUM    1
#define NO_CHKSUM    0

#define OAK_TCP_IP_FRAME  1
#define OAK_TCP_UDP_FRAME 2

/* Name        : oak_chksum_get_config
 * Returns     : netdev_features_t
 * Parameters  : void
 * Description : This function provides Oak Hardware's Checksum Offload 
 *               capabilities.
 *  */
netdev_features_t oak_chksum_get_config(void);

/* Name        : oak_chksum_get_tx_config
 * Returns     : uint32_t
 * Parameters  : struct sk_buff *skb, uint32_t *cs_l3, uint32_t *cs_l4
 * Description : This function returns the Checksum Offload configuration for 
 *               the transmit frame.
 *  */
uint32_t oak_chksum_get_tx_config(struct sk_buff *skb, uint32_t *cs_l3, 
                                  uint32_t *cs_l4);

/* Name        : oak_chksum_get_rx_config
 * Returns     : uint32_t
 * Parameters  : oak_rx_chan_t*, oak_rxs_t*
 * Description : This function returns the current receive frames 
 *               checksum state.
 *  */
uint32_t oak_chksum_get_rx_config(oak_rx_chan_t *rxc, oak_rxs_t *rsr);

#endif /* #ifndef H_OAK_CHKSUM */
