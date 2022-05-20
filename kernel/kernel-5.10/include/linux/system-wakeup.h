/* SPDX-License-Identifier: GPL-2.0-only+ */
/*
 * PMC interface for NVIDIA SoCs Tegra
 *
 * Copyright (c) 2013-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 */

#ifndef __LINUX_SYSTEM_WAKEUP_H__
#define __LINUX_SYSTEM_WAKEUP_H__

#ifdef CONFIG_PM_SLEEP
extern int get_wakeup_reason_irq(void);
#else
static inline int get_wakeup_reason_irq(void)
{
	return -EINVAL;
}
#endif

#endif	/* __LINUX_SYSTEM_WAKEUP_H__ */
