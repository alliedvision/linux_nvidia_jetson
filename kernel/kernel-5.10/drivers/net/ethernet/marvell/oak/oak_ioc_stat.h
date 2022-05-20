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
#ifndef H_OAK_IOC_STAT
#define H_OAK_IOC_STAT

typedef struct oak_ioc_statStruct
{
#define OAK_IOCTL_STAT (SIOCDEVPRIVATE + 4)
#define OAK_IOCTL_STAT_GET_TXS (1 << 0)
#define OAK_IOCTL_STAT_GET_RXS (1 << 1)
#define OAK_IOCTL_STAT_GET_TXC (1 << 2)
#define OAK_IOCTL_STAT_GET_RXC (1 << 3)
#define OAK_IOCTL_STAT_GET_RXB (1 << 4)
#define OAK_IOCTL_STAT_GET_LDG (1 << 5)
    __u32 cmd;
    __u32 idx;
    __u32 offs;
    char data[128];
    int error;
} oak_ioc_stat;

#endif /* #ifndef H_OAK_IOC_STAT */

