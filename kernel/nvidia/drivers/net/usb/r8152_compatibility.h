/*
 * Copyright (c) 2014-2019 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LINUX_COMPATIBILITY_H
#define LINUX_COMPATIBILITY_H

/*
 * Definition and macro
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/in.h>

#include <linux/mdio.h>

#ifndef FALSE
	#define TRUE	1
	#define FALSE	0
#endif

/*
 * inline function
 */

enum rtl_cmd {
	RTLTOOL_PLA_OCP_READ_DWORD = 0,
	RTLTOOL_PLA_OCP_WRITE_DWORD,
	RTLTOOL_USB_OCP_READ_DWORD,
	RTLTOOL_USB_OCP_WRITE_DWORD,
	RTLTOOL_PLA_OCP_READ,
	RTLTOOL_PLA_OCP_WRITE,
	RTLTOOL_USB_OCP_READ,
	RTLTOOL_USB_OCP_WRITE,
	RTLTOOL_USB_INFO,
	RTL_ENABLE_USB_DIAG,
	RTL_DISABLE_USB_DIAG,

	RTLTOOL_INVALID
};

struct usb_device_info {
	__u16		idVendor;
	__u16		idProduct;
	__u16		bcdDevice;
	__u8		dev_addr[8];
	char		devpath[16];
};

struct rtltool_cmd {
	__u32	cmd;
	__u32	offset;
	__u32	byteen;
	__u32	data;
	void	*buf;
	struct usb_device_info nic_info;
	struct sockaddr ifru_addr;
	struct sockaddr ifru_netmask;
	struct sockaddr ifru_hwaddr;
};

#endif /* LINUX_COMPATIBILITY_H */
