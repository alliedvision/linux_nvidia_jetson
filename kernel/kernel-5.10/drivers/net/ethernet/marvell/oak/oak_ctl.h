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
#ifndef H_OAK_CTL
#define H_OAK_CTL

#include "oak_unimac.h" /* Include for relation to classifier oak_unimac */
#include "oak_net.h" /* Include for relation to classifier oak_net */

extern int debug;
/* Name      : channel_status_access
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_channel_status_access(oak_t* np, struct ifreq* ifr, int cmd);

/* Name      : set_mac_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd
 *  */
int oak_ctl_set_mac_rate(oak_t* np, struct ifreq* ifr, int cmd);

/* Name      : set_rx_flow
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_set_rx_flow(oak_t* np, struct ifreq* ifr, int cmd);

/* Name      : set_txr_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_set_txr_rate(oak_t* np, struct ifreq* ifr, int cmd);

/* Name      : direct_register_access
 * Returns   : int
 * Parameters:  oak_t * np,  struct ifreq * ifr,  int cmd
 *  */
int oak_ctl_direct_register_access(oak_t* np, struct ifreq* ifr, int cmd);

#endif /* #ifndef H_OAK_CTL */

