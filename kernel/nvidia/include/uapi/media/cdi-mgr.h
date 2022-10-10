/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __UAPI_TEGRA_CDI_MGR_H__
#define __UAPI_TEGRA_CDI_MGR_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#define CDI_MGR_IOCTL_PWR_DN	        _IOW('o', 1, __s16)
#define CDI_MGR_IOCTL_PWR_UP	        _IOR('o', 2, __s16)
#define CDI_MGR_IOCTL_SET_PID	        _IOW('o', 3, struct cdi_mgr_sinfo)
#define CDI_MGR_IOCTL_SIGNAL	        _IOW('o', 4, int)
#define CDI_MGR_IOCTL_DEV_ADD	        _IOW('o', 5, struct cdi_mgr_new_dev)
#define CDI_MGR_IOCTL_DEV_DEL	        _IOW('o', 6, int)
#define CDI_MGR_IOCTL_PWR_INFO	        _IOW('o', 7, struct cdi_mgr_pwr_info)
#define CDI_MGR_IOCTL_PWM_ENABLE        _IOW('o', 8, int)
#define CDI_MGR_IOCTL_PWM_CONFIG        _IOW('o', 9, struct cdi_mgr_pwm_info)
#define CDI_MGR_IOCTL_WAIT_ERR	        _IOWR('o', 10, __u32)
#define CDI_MGR_IOCTL_ABORT_WAIT_ERR	    _IO('o', 11)
#define CDI_MGR_IOCTL_GET_EXT_PWR_CTRL	    _IOR('o', 12, __u8)
#define CDI_MGR_IOCTL_SET_CAM_PWR_ON		_IOW('o', 13, uint8_t)
#define CDI_MGR_IOCTL_SET_CAM_PWR_OFF		_IOW('o', 14, uint8_t)
#define CDI_MGR_IOCTL_ENABLE_ERROR_REPORT	_IO('o', 15)
#define CDI_MGR_IOCTL_GET_PWR_MODE	_IOW('o', 16, struct cdi_mgr_pwr_mode)

#define CDI_MGR_POWER_ALL	5
#define MAX_CDI_NAME_LENGTH	32

#define DES_PWR_NVCCP    0U
#define DES_PWR_GPIO     1U
#define DES_PWR_NO_PWR   2U
#define CAM_PWR_NVCCP    0U
#define CAM_PWR_MAX20087 1U
#define CAM_PWR_NO_PWR   2U

struct cdi_mgr_new_dev {
	__u16 addr;
	__u8 reg_bits;
	__u8 val_bits;
	__u8 drv_name[MAX_CDI_NAME_LENGTH];
};

struct cdi_mgr_sinfo {
	__s32 pid;
	__s32 sig_no;
	__u64 context;
};

struct cdi_mgr_pwr_info {
	__s32 pwr_gpio;
	__s32 pwr_status;
};

struct cdi_mgr_pwr_mode {
	__s8 des_pwr_mode;
	__s8 cam_pwr_mode;
};

struct cdi_mgr_pwm_info {
	__u64 duty_ns;
	__u64 period_ns;
};

enum {
	CDI_MGR_PWM_DISABLE = 0,
	CDI_MGR_PWM_ENABLE,
};

enum {
	CDI_MGR_SIGNAL_RESUME = 0,
	CDI_MGR_SIGNAL_SUSPEND,
};

#endif  /* __UAPI_TEGRA_CDI_MGR_H__ */
