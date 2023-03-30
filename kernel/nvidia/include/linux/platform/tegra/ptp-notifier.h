/*
 * include/linux/platform/tegra/ptp-notifier.h
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __PTP_NOTIFIER_H
#define __PTP_NOTIFIER_H

#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/etherdevice.h>

#if IS_ENABLED(CONFIG_TEGRA_PTP_NOTIFIER)

#define PTP_HWTIME		1
#define PTP_TSC_HWTIME		2
#define TSC_HIGH_SHIFT		32U

#define MAX_MAC_INSTANCES	5
/**
 * @brief ptp_tsc_data - Struture used to store TSC and PTP time
 * information.
 */
struct ptp_tsc_data {
	/** PTP TimeStamp in nSec*/
	u64 ptp_ts;
	/** TSC TimeStamp in nSec*/
	u64 tsc_ts;
};

/* register / unregister HW time source */
void tegra_register_hwtime_source(int (*func)(struct net_device *, void *, int),
				  struct net_device *dev);
void tegra_unregister_hwtime_source(struct net_device *dev);

/* clients registering / unregistering for time update events */
int tegra_register_hwtime_notifier(struct notifier_block *nb);
int tegra_unregister_hwtime_notifier(struct notifier_block *nb);

/* Notify time updates to registered clients */
int tegra_hwtime_notifier_call_chain(unsigned int val, void *v);

/*
 * Get HW time counter.
 * Clients may call the API every anytime PTP/TSC time is needed.
 * If HW time source is not registered, returns -EINVAL
 */
int tegra_get_hwtime(const char *intf_name, void *ts, int ts_type);

#else /* CONFIG_TEGRA_PTP_NOTIFIER */

/* register / unregister HW time source */
static inline void tegra_register_hwtime_source(int (*func)(struct net_device *,
						void *, int), struct net_device *)
{
}

static inline void tegra_unregister_hwtime_source(struct net_device *)
{
}

/* clients registering / unregistering for time update events */
static inline int tegra_register_hwtime_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int tegra_unregister_hwtime_notifier(struct notifier_block *nb)
{
	return -ENOENT;
}

/* Notify time updates to registered clients */
static inline int tegra_hwtime_notifier_call_chain(unsigned int val, void *v)
{
	return notifier_to_errno(NOTIFY_DONE);
}

/*
 * Get HW time counter.
 * Clients may call the API every anytime PTP/TSC time is needed.
 * If HW time source is not registered, returns -EINVAL
 */
static inline int tegra_get_hwtime(const char *intf_name, void *ts, int ts_type)
{
	return -EINVAL;
}

#endif /* CONFIG_TEGRA_PTP_NOTIFIER */

#endif /* __PTP_NOTIFIER_H */
