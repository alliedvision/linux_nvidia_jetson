/**
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
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

#ifndef __IMX274_H__
#define __IMX274_H__

#include <linux/ioctl.h>  /* For IOCTL macros */
#include <media/nvc.h>
#include <media/nvc_image.h>

#define IMX274_IOCTL_SET_MODE			_IOW('o', 1, struct imx274_mode)
#define IMX274_IOCTL_GET_STATUS		_IOR('o', 2, __u8)
#define IMX274_IOCTL_SET_FRAME_LENGTH		_IOW('o', 3, __u32)
#define IMX274_IOCTL_SET_COARSE_TIME		_IOW('o', 4, __u32)
#define IMX274_IOCTL_SET_GAIN			_IOW('o', 5, __u16)
#define IMX274_IOCTL_GET_SENSORDATA		_IOR('o', 6, \
	 struct imx274_sensordata)
#define IMX274_IOCTL_SET_GROUP_HOLD		_IOW('o', 7, struct imx274_ae)
#define IMX274_IOCTL_SET_HDR_COARSE_TIME	_IOW('o', 8, struct imx274_hdr)
#define IMX274_IOCTL_SET_POWER			_IOW('o', 20, __u32)

#define IMX274_SVR_ADDR					0x300E

#define IMX274_SHR_ADDR_LSB				0x300C
#define IMX274_SHR_ADDR_MSB				0x300D

#define IMX274_SHR_DOL1_ADDR_LSB		0x302E
#define IMX274_SHR_DOL1_ADDR_MSB		0x302F
#define IMX274_SHR_DOL2_ADDR_LSB		0x3030
#define IMX274_SHR_DOL2_ADDR_MSB		0x3031
#define IMX274_RHS1_ADDR_LSB			0x3032
#define IMX274_RHS1_ADDR_MSB			0x3033

#define IMX274_VMAX_ADDR_LSB			0x30F8
#define IMX274_VMAX_ADDR_MID			0x30F9
#define IMX274_VMAX_ADDR_MSB			0x30FA

#define IMX274_ANALOG_GAIN_ADDR_LSB		0x300A
#define IMX274_ANALOG_GAIN_ADDR_MSB		0x300B
#define IMX274_DIGITAL_GAIN_ADDR		0x3012

#define IMX274_GROUP_HOLD_ADDR			0x302D


struct imx274_mode {
	__u32 xres;
	__u32 yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u16 gain;
	__u8 hdr_en;
};

struct imx274_hdr {
	__u32 coarse_time_long;
	__u32 coarse_time_short;
};

struct imx274_ae {
	__u32 frame_length;
	__u8  frame_length_enable;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u8  coarse_time_enable;
	__s32 gain;
	__u8  gain_enable;
};

#ifdef __KERNEL__
struct imx274_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
	struct regulator *ext_reg1;
	struct regulator *ext_reg2;
	struct clk *mclk;
	unsigned int pwdn_gpio;
	unsigned int cam1_gpio;
	unsigned int reset_gpio;
	unsigned int af_gpio;
};

struct imx274_platform_data {
	const char *mclk_name; /* NULL for default default_mclk */
	unsigned int cam1_gpio;
	unsigned int reset_gpio;
	unsigned int af_gpio;
	bool ext_reg;
	int (*power_on)(struct imx274_power_rail *pw);
	int (*power_off)(struct imx274_power_rail *pw);
};
#endif /* __KERNEL__ */

#endif  /* __IMX274_H__ */
