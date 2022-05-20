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

#include "oak_debug.h"

/* Name      : oak_dbg_set_level
 * Returns   : void
 * Parameters:  struct net_device * dev,  uint32_t level
 *  */
void oak_dbg_set_level(struct net_device* dev, uint32_t level)
{
    debug = level;
}

/* Name      : oak_dbg_get_level
 * Returns   : uint32_t
 * Parameters:  struct net_device * dev
 *  */
uint32_t oak_dbg_get_level(struct net_device* dev)
{
    return debug;
}

