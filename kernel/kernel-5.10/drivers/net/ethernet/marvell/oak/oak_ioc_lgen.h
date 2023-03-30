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
#ifndef H_OAK_IOC_LGEN
#define H_OAK_IOC_LGEN

typedef struct oak_ioc_lgenStruct
{
#define OAK_IOCTL_LGEN (SIOCDEVPRIVATE + 3)
#define OAK_IOCTL_LGEN_INIT (1 << 0)
#define OAK_IOCTL_LGEN_TX_DATA (1 << 1)
#define OAK_IOCTL_LGEN_TX_START (1 << 2)
#define OAK_IOCTL_LGEN_TX_STOP (1 << 3)
#define OAK_IOCTL_LGEN_RX_START (1 << 4)
#define OAK_IOCTL_LGEN_RX_STOP (1 << 5)
#define OAK_IOCTL_LGEN_RX_DATA (1 << 6)
#define OAK_IOCTL_LGEN_RELEASE (1 << 7)
#define OAK_IOCTL_LGEN_TX_RESET (1 << 8)
#define OAK_IOCTL_LGEN_RX_RESET (1 << 9)
    __u32 cmd;
    __u32 offs;
    __u32 len;
    __u32 channel;
    __u32 count;
    char data[32];
    int error;
} oak_ioc_lgen;

#endif /* #ifndef H_OAK_IOC_LGEN */

