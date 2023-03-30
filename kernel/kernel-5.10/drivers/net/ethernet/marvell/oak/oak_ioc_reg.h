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
#ifndef H_OAK_IOC_REG
#define H_OAK_IOC_REG

typedef struct oak_ioc_regStruct
{
#define OAK_IOCTL_REG_ESU_REQ (SIOCDEVPRIVATE + 1)
#define OAK_IOCTL_REG_MAC_REQ (SIOCDEVPRIVATE + 2)
#define OAK_IOCTL_REG_RD 1
#define OAK_IOCTL_REG_WR 2
#define OAK_IOCTL_REG_WS 3
#define OAK_IOCTL_REG_WC 4
#define OAK_IOCTL_REG_OR 5
#define OAK_IOCTL_REG_AND 6
    __u32 cmd;
    __u32 offs;
    __u32 dev_no;
    __u32 data;
    __u32 device;
    int error;
} oak_ioc_reg;

#endif /* #ifndef H_OAK_IOC_REG */

