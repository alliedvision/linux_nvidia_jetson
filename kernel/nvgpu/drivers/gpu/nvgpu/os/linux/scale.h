/*
 * gk20a clock scaling profile
 *
 * Copyright (c) 2013-2022, NVIDIA Corporation. All rights reserved.
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

#ifndef GK20A_SCALE_H
#define GK20A_SCALE_H

#include <linux/devfreq.h>
#include <linux/version.h>

struct clk;

struct gk20a_scale_profile {
	struct device			*dev;
	ktime_t				last_event_time;
	struct devfreq_dev_profile	devfreq_profile;
	struct devfreq_dev_status	dev_stat;
	struct notifier_block		qos_notify_block;
	struct notifier_block		qos_min_notify_block;
	struct notifier_block		qos_max_notify_block;
	unsigned long			qos_min_freq;
	unsigned long			qos_max_freq;
	void				*private_data;
	struct nvgpu_mutex		lock;
};

/* Initialization and de-initialization for module */
void gk20a_scale_init(struct device *);
void gk20a_scale_exit(struct device *);
void gk20a_scale_hw_init(struct device *dev);

#if defined(CONFIG_GK20A_DEVFREQ)
/*
 * call when performing submit to notify scaling mechanism that the module is
 * in use
 */
void gk20a_scale_notify_busy(struct device *);
void gk20a_scale_notify_idle(struct device *);

void gk20a_scale_suspend(struct device *);
void gk20a_scale_resume(struct device *);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
int gk20a_scale_qos_min_notify(struct notifier_block *nb,
			  unsigned long n, void *p);
int gk20a_scale_qos_max_notify(struct notifier_block *nb,
			  unsigned long n, void *p);
u16 gk20a_scale_clamp_clk_target(struct gk20a *g,
				 u16 gpc2clk_target);
#else
int gk20a_scale_qos_notify(struct notifier_block *nb,
			unsigned long n, void *p);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */

#else
static inline void gk20a_scale_notify_busy(struct device *dev) {}
static inline void gk20a_scale_notify_idle(struct device *dev) {}
static inline void gk20a_scale_suspend(struct device *dev) {}
static inline void gk20a_scale_resume(struct device *dev) {}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
static inline int gk20a_scale_qos_min_notify(struct notifier_block *nb,
			  unsigned long n, void *p)
{
	return -ENOSYS;
}
static inline int gk20a_scale_qos_max_notify(struct notifier_block *nb,
			  unsigned long n, void *p)
{
	return -ENOSYS;
}
static inline u16 gk20a_scale_clamp_clk_target(struct gk20a *g,
				 u16 gpc2clk_target)
{
	return gpc2clk_target;
}
#else
static inline int gk20a_scale_qos_notify(struct notifier_block *nb,
			unsigned long n, void *p)
{
	return -ENOSYS;
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */

#endif

#endif
