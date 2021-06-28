/*
################################################################################
#
# r8168 is the Linux device driver released for Realtek Gigabit Ethernet
# controllers with PCI-Express interface.
#
# Copyright(c) 2014-2018, Realtek Semiconductor Corp. All rights reserved.
# Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#
# Author:
# Realtek NIC software team <nicfae@realtek.com>
# No. 2, Innovation Road II, Hsinchu Science Park, Hsinchu 300, Taiwan
#
################################################################################
*/

/************************************************************************************
 *  This product is covered by one or more of the following patents:
 *  US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

#ifndef _UAPI_PG_DRV_H
#define _UAPI_PG_DRV_H

#include <linux/cdev.h>

typedef struct _PCI_CONFIG_RW_ {
	union {
		unsigned char	byte;
		unsigned short	word;
		unsigned int	dword;
	};
	unsigned int		bRead:1;
	unsigned int		size:7;
	unsigned int		addr:8;
	unsigned int		reserve:16;
} PCI_CONFIG_RW, *PPCI_CONFIG_RW;

#define RTL_IOC_MAGIC					0x95

#define IOC_PCI_CONFIG					_IOWR(RTL_IOC_MAGIC, 0, PCI_CONFIG_RW)
#define IOC_IOMEM_OFFSET				_IOR(RTL_IOC_MAGIC, 1, unsigned int)
#define IOC_DEV_FUN					_IOR(RTL_IOC_MAGIC, 2, unsigned int)

#endif /* _UAPI_PG_DRV_H */
