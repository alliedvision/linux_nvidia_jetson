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
#ifndef H_OAK_IOC_FLOW
#define H_OAK_IOC_FLOW

typedef struct oak_ioc_flowStruct
{
#define OAK_IOCTL_RXFLOW (SIOCDEVPRIVATE + 8)
#define OAK_IOCTL_RXFLOW_CLEAR 0
#define OAK_IOCTL_RXFLOW_MGMT (1 << 0)
#define OAK_IOCTL_RXFLOW_QPRI (1 << 3)
#define OAK_IOCTL_RXFLOW_SPID (1 << 7)
#define OAK_IOCTL_RXFLOW_FLOW (1 << 12)
#define OAK_IOCTL_RXFLOW_DA (1 << 18)
#define OAK_IOCTL_RXFLOW_ET (1 << 19)
#define OAK_IOCTL_RXFLOW_FID (1 << 20)
#define OAK_IOCTL_RXFLOW_DA_MASK (1 << 21)
    __u32 cmd;
    __u32 idx;
    uint32_t val_lo;
    uint32_t val_hi;
    char data[16];
    int ena;
    int error;
} oak_ioc_flow;

#endif /* #ifndef H_OAK_IOC_FLOW */

