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
#include "oak_net.h"
#include "oak_debug.h"
#include "oak_chksum.h"

/* Name        : oak_chksum_get_config
 * Returns     : netdev_features_t
 * Parameters  : void
 * Description : This function provides Oak Hardware's Checksum Offload 
 *               capabilities
 *  */
netdev_features_t oak_chksum_get_config(void)
{
    netdev_features_t features = OAK_CHKSUM_TYPE;

    /* Oak HW Supports L3 & L3 Checksum Offload and Freagmented frame, 
       so scatter gather need to be enabled */
    features |= NETIF_F_SG;

    return features;
}

/* Name        : oak_chksum_get_tx_config
 * Returns     : uint32_t
 * Parameters  : struct sk_buff *skb, uint32_t *cs_l3, uint32_t *cs_l4
 * Description : This function returns the Checksum Offload configuration 
 *               for the transmit frame.
 *  */
uint32_t oak_chksum_get_tx_config(struct sk_buff *skb, uint32_t *cs_l3, 
                                  uint32_t *cs_l4)
{
    int prot;
    int retval = 0;

    if (skb->ip_summed == CHECKSUM_PARTIAL)
    {
        prot = oak_net_skb_tx_protocol_type(skb);
        if (prot == L3_L4_CHKSUM)
        {
            *cs_l3 = 1;
            *cs_l4 = 1;
        }
        else if (prot == L3_CHKSUM)
        {
            *cs_l3 = 1;
            *cs_l4 = 0;
        }
        else
        {
            /* Setting cs_l3 & cs_l4 to zero is done in error case after this
               function returns. */
            retval = 1;
        }
    }
    else
        retval = 1;

    return retval;
}

/* Name        : oak_chksum_get_rx_config
 * Returns     : uint32_t
 * Parameters  : struct oak_rx_chan_t *rxc, struct oak_rxs_t *rsr
 * Description : This function returns the current receive frames 
 *               checksum state.
 *  */
uint32_t oak_chksum_get_rx_config(oak_rx_chan_t *rxc, oak_rxs_t *rsr)
{
    int retval = CHECKSUM_NONE;

    if (rsr->vlan == 1)
        ++rxc->stat.rx_vlan;

    if (rsr->l3_ipv4 == 1 || rsr->l3_ipv6 == 1)
    {
        if (rsr->l4_prot == OAK_TCP_IP_FRAME || rsr->l4_prot == OAK_TCP_UDP_FRAME)
        {
            if (rsr->l4_chk_ok == 1)
            {
                rxc->stat.rx_l4p_ok++;
                retval = CHECKSUM_UNNECESSARY;
            }
        }
        else if (rsr->l3_ipv4 == 1 && rsr->ipv4_hdr_ok == 1)
        {
            /*
             Linux documentation for CHECKSUM_PARTIAL
             in include/linux/skbuff.h this state may occur on a packet
             received directly from another Linux OS,
             e.g., a virtualized Linux kernel on the same host,
             or it may be set in the input path in GRO or remote checksum offload.
             As per the discussion with the Linux kernel netdev forum
             setting ip_summed to PARTIAL on receive is only valid
             for software/virtual devices, never real HW.
             For a frame where the checksum is not verified by the HW,
             the flag will be set as CHECKSUM_NONE such that
             linux netdev layer verifies the same.
            */
            rxc->stat.rx_ip4_ok++;
        }
    }

    return retval;
}

