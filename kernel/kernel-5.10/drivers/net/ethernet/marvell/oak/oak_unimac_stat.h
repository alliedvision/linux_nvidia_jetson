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
#ifndef H_OAK_UNIMAC_STAT
#define H_OAK_UNIMAC_STAT

typedef struct oak_unimac_statStruct
{
    uint64_t rx_good_frames;
    uint64_t rx_bad_frames;
    uint64_t rx_stall_fifo;
    uint64_t rx_stall_desc;
    uint64_t rx_discard_desc;
    uint64_t tx_pause;
    uint64_t tx_stall_fifo;
} oak_unimac_stat;

#endif /* #ifndef H_OAK_UNIMAC_STAT */

