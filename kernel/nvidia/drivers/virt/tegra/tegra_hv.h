/*
 * Copyright (C) 2015-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#ifndef __TEGRA_HV_H__
#define __TEGRA_HV_H__

#include <soc/tegra/virt/syscalls.h>
#include <linux/version.h>

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
#define SUPPORTS_TRAP_MSI_NOTIFICATION
#endif

#define IVC_INFO_PAGE_SIZE 65536

const struct ivc_info_page *tegra_hv_get_ivc_info(void);
int tegra_hv_get_vmid(void);

#endif /* __TEGRA_HV_H__ */
