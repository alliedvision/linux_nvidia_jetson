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

#include <linux/devfreq.h>
#ifdef CONFIG_DEVFREQ_THERMAL
#include <linux/devfreq_cooling.h>
#endif
#include <linux/export.h>
#ifdef CONFIG_GK20A_PM_QOS
#include <linux/pm_qos.h>
#endif
#include <linux/version.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
#include <governor.h>
#endif

#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/pmu/pmu_perfmon.h>

#include "platform_gk20a.h"
#include "scale.h"
#include "os_linux.h"

/*
 * gk20a_scale_qos_notify()
 *
 * This function is called when the minimum QoS requirement for the device
 * has changed. The function calls postscaling callback if it is defined.
 */

#if defined(CONFIG_GK20A_PM_QOS) && defined(CONFIG_COMMON_CLK)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
int gk20a_scale_qos_min_notify(struct notifier_block *nb,
			  unsigned long n, void *p)
{
	struct gk20a_scale_profile *profile =
			container_of(nb, struct gk20a_scale_profile,
			qos_min_notify_block);
	struct gk20a *g = get_gk20a(profile->dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct devfreq *devfreq = l->devfreq;

	if (!devfreq)
		return NOTIFY_OK;

	nvgpu_mutex_acquire(&profile->lock);

	profile->qos_min_freq = (unsigned long)n * 1000UL;

	nvgpu_mutex_release(&profile->lock);

	return NOTIFY_OK;
}

int gk20a_scale_qos_max_notify(struct notifier_block *nb,
			  unsigned long n, void *p)
{
	struct gk20a_scale_profile *profile =
			container_of(nb, struct gk20a_scale_profile,
			qos_max_notify_block);
	struct gk20a *g = get_gk20a(profile->dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct devfreq *devfreq = l->devfreq;

	if (!devfreq)
		return NOTIFY_OK;

	nvgpu_mutex_acquire(&profile->lock);

	profile->qos_max_freq = (unsigned long)n * 1000UL;

	nvgpu_mutex_release(&profile->lock);

	return NOTIFY_OK;
}

u16 gk20a_scale_clamp_clk_target(struct gk20a *g,
				 u16 gpc2clk_target)
{
	struct gk20a_scale_profile *profile = g->scale_profile;
	u16 min_freq_mhz, max_freq_mhz;

	if (!profile)
		return gpc2clk_target;

	nvgpu_mutex_acquire(&profile->lock);

	min_freq_mhz = (u16) (profile->qos_min_freq / 1000000UL);
	max_freq_mhz = (u16) (profile->qos_max_freq / 1000000UL);

	nvgpu_log_info(g, "target %u qos_min %u qos_max %u", gpc2clk_target,
		       min_freq_mhz, max_freq_mhz);

	if (gpc2clk_target < min_freq_mhz) {
		gpc2clk_target = min_freq_mhz;
	}

	if (gpc2clk_target > max_freq_mhz) {
		gpc2clk_target = max_freq_mhz;
	}

	nvgpu_mutex_release(&profile->lock);

	return gpc2clk_target;
}
#else
int gk20a_scale_qos_notify(struct notifier_block *nb,
			  unsigned long n, void *p)
{
	struct gk20a_scale_profile *profile =
			container_of(nb, struct gk20a_scale_profile,
			qos_notify_block);
	struct gk20a *g = get_gk20a(profile->dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct devfreq *devfreq = l->devfreq;

	if (!devfreq)
		return NOTIFY_OK;

	mutex_lock(&devfreq->lock);
	/* check for pm_qos min and max frequency requirement */
	profile->qos_min_freq =
	  (unsigned long)pm_qos_read_min_bound(PM_QOS_GPU_FREQ_BOUNDS) * 1000UL;
	profile->qos_max_freq =
	  (unsigned long)pm_qos_read_max_bound(PM_QOS_GPU_FREQ_BOUNDS) * 1000UL;

	if (profile->qos_min_freq > profile->qos_max_freq) {
		nvgpu_err(g,
			"QoS: setting invalid limit, min_freq=%lu max_freq=%lu",
			profile->qos_min_freq, profile->qos_max_freq);
		profile->qos_min_freq = profile->qos_max_freq;
	}

	update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);

	return NOTIFY_OK;
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */
#elif defined(CONFIG_GK20A_PM_QOS)
int gk20a_scale_qos_notify(struct notifier_block *nb,
			  unsigned long n, void *p)
{
	struct gk20a_scale_profile *profile =
		container_of(nb, struct gk20a_scale_profile,
			     qos_notify_block);
	struct gk20a_platform *platform = dev_get_drvdata(profile->dev);
	struct gk20a *g = get_gk20a(profile->dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	unsigned long freq;

	if (!platform->postscale)
		return NOTIFY_OK;

	/* get the frequency requirement. if devfreq is enabled, check if it
	 * has higher demand than qos */
	freq = platform->clk_round_rate(profile->dev,
			(u32)pm_qos_read_min_bound(PM_QOS_GPU_FREQ_BOUNDS));
	if (l->devfreq)
		freq = max(l->devfreq->previous_freq, freq);

	/* Update gpu load because we may scale the emc target
	 * if the gpu load changed. */
	nvgpu_pmu_load_update(g);
	platform->postscale(profile->dev, freq);

	return NOTIFY_OK;
}
#else
int gk20a_scale_qos_notify(struct notifier_block *nb,
			  unsigned long n, void *p)
{
	return 0;
}
#endif

/*
 * gk20a_scale_make_freq_table(profile)
 *
 * This function initialises the frequency table for the given device profile
 */

static int gk20a_scale_make_freq_table(struct gk20a_scale_profile *profile)
{
	struct gk20a_platform *platform = dev_get_drvdata(profile->dev);
	int num_freqs, err;
	unsigned long *freqs;

	if (platform->get_clk_freqs) {
		/* get gpu frequency table */
		err = platform->get_clk_freqs(profile->dev, &freqs,
					&num_freqs);

		if (err)
			return -ENOSYS;
	} else
		return -ENOSYS;

	profile->devfreq_profile.freq_table = (unsigned long *)freqs;
	profile->devfreq_profile.max_state = num_freqs;

	return 0;
}

/*
 * gk20a_scale_target(dev, *freq, flags)
 *
 * This function scales the clock
 */

static int gk20a_scale_target(struct device *dev, unsigned long *freq,
			      u32 flags)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = platform->g;
	struct gk20a_scale_profile *profile = g->scale_profile;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct devfreq *devfreq = l->devfreq;
#endif
	unsigned long local_freq = *freq;
	unsigned long rounded_rate = 0;
	unsigned long min_freq = 0, max_freq = 0;

	if (nvgpu_clk_arb_has_active_req(g)) {
		rounded_rate = g->last_freq;
		goto post_scale;
	}
	/*
	 * Calculate floor and cap frequency values
	 *
	 * Policy :
	 * We have two APIs to clip the frequency
	 *  1. devfreq
	 *  2. pm_qos
	 *
	 * To calculate floor (min) freq, we select MAX of floor frequencies
	 * requested from both APIs
	 * To get cap (max) freq, we select MIN of max frequencies
	 *
	 * In case we have conflict (min_freq > max_freq) after above
	 * steps, we ensure that max_freq wins over min_freq
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	min_freq = max_t(u32, devfreq->min_freq, profile->qos_min_freq);
	max_freq = min_t(u32, devfreq->max_freq, profile->qos_max_freq);
#else
	/*
	 * devfreq takes care of min/max freq clipping in update_devfreq() then
	 * invoked devfreq->profile->target(), thus we only need to do freq
	 * clipping based on pm_qos constraint
	 */
	min_freq = profile->qos_min_freq;
	max_freq = profile->qos_max_freq;
#endif

	if (min_freq > max_freq)
		min_freq = max_freq;

	/* Clip requested frequency */
	if (local_freq < min_freq)
		local_freq = min_freq;

	if (local_freq > max_freq)
		local_freq = max_freq;

	/* set the final frequency */
	rounded_rate = platform->clk_round_rate(dev, local_freq);

	/* Check for duplicate request */
	if (rounded_rate == g->last_freq)
		return 0;

	if (g->ops.clk.get_rate(g, CTRL_CLK_DOMAIN_GPCCLK) == rounded_rate)
		*freq = rounded_rate;
	else {
		g->ops.clk.set_rate(g, CTRL_CLK_DOMAIN_GPCCLK, rounded_rate);
		*freq = g->ops.clk.get_rate(g, CTRL_CLK_DOMAIN_GPCCLK);
	}

	g->last_freq = *freq;

post_scale:
	/* postscale will only scale emc (dram clock) if evaluating
	 * gk20a_tegra_get_emc_rate() produces a new or different emc
	 * target because the load or_and gpufreq has changed */
	if (platform->postscale)
		platform->postscale(dev, rounded_rate);

	return 0;
}

/*
 * update_load_estimate_busy_cycles(dev)
 *
 * Update load estimate using pmu idle counters. Result is normalised
 * based on the time it was asked last time.
 */

static void update_load_estimate_busy_cycles(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_scale_profile *profile = g->scale_profile;
	unsigned long dt;
	u32 busy_cycles_norm;
	ktime_t t;

	t = ktime_get();
	dt = ktime_us_delta(t, profile->last_event_time);

	profile->dev_stat.total_time = dt;
	profile->last_event_time = t;
	nvgpu_pmu_busy_cycles_norm(g, &busy_cycles_norm);
	profile->dev_stat.busy_time =
		(busy_cycles_norm * dt) / PMU_BUSY_CYCLES_NORM_MAX;
}

/*
 * gk20a_scale_suspend(dev)
 *
 * This function informs devfreq of suspend
 */

void gk20a_scale_suspend(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct devfreq *devfreq = l->devfreq;

	if (!devfreq)
		return;

	devfreq_suspend_device(devfreq);
}

/*
 * gk20a_scale_resume(dev)
 *
 * This functions informs devfreq of resume
 */

void gk20a_scale_resume(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct devfreq *devfreq = l->devfreq;

	if (!devfreq)
		return;

	g->last_freq = 0;
	devfreq_resume_device(devfreq);
}

/*
 * gk20a_scale_get_dev_status(dev, *stat)
 *
 * This function queries the current device status.
 */

static int gk20a_scale_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_scale_profile *profile = g->scale_profile;
	struct gk20a_platform *platform = dev_get_drvdata(dev);

	/* inform edp about new constraint */
	if (platform->prescale)
		platform->prescale(dev);

	/* Make sure there are correct values for the current frequency */
	profile->dev_stat.current_frequency =
				g->ops.clk.get_rate(g, CTRL_CLK_DOMAIN_GPCCLK);

	/* Update load estimate */
	update_load_estimate_busy_cycles(dev);

	/* Copy the contents of the current device status */
	*stat = profile->dev_stat;

	/* Finally, clear out the local values */
	profile->dev_stat.total_time = 0;
	profile->dev_stat.busy_time = 0;

	return 0;
}

/*
 * get_cur_freq(struct device *dev, unsigned long *freq)
 *
 * This function gets the current GPU clock rate.
 */

static int get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct gk20a *g = get_gk20a(dev);
	*freq = g->ops.clk.get_rate(g, CTRL_CLK_DOMAIN_GPCCLK);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
static int register_gpu_opp(struct device *dev)
{
	return 0;
}

static void unregister_gpu_opp(struct device *dev)
{
}
#else
static void unregister_gpu_opp(struct device *dev)
{
	dev_pm_opp_remove_all_dynamic(dev);
}

static int register_gpu_opp(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = platform->g;
	struct gk20a_scale_profile *profile = g->scale_profile;
	unsigned long *freq_table = profile->devfreq_profile.freq_table;
	int max_states = profile->devfreq_profile.max_state;
	int i;
	int err = 0;

	for (i = 0; i < max_states; ++i) {
		err = dev_pm_opp_add(dev, freq_table[i], 0);
		if (err) {
			nvgpu_err(g,
				"Failed to add OPP %lu: %d\n",
				freq_table[i],
				err);
			unregister_gpu_opp(dev);
			break;
		}
	}

	return err;
}
#endif

/*
 * gk20a_scale_init(dev)
 */

void gk20a_scale_init(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = platform->g;
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct gk20a_scale_profile *profile;
#ifdef CONFIG_DEVFREQ_THERMAL
	struct thermal_cooling_device *cooling;
#endif
	struct devfreq *devfreq;
	int err;

	if (g->scale_profile)
		return;

	if (!platform->devfreq_governor)
		return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	if (!platform->qos_min_notify && !platform->qos_max_notify) {
		return;
	}
#else
	if (!platform->qos_notify) {
		return;
	}
#endif

	profile = nvgpu_kzalloc(g, sizeof(*profile));
	if (!profile)
		return;

	profile->dev = dev;
#ifdef CONFIG_GK20A_PM_QOS
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	profile->dev_stat.busy = false;
#endif
#endif

	/* Create frequency table */
	err = gk20a_scale_make_freq_table(profile);
	if (err || !profile->devfreq_profile.max_state)
		goto err_get_freqs;

	profile->qos_min_freq = 0;
	profile->qos_max_freq = UINT_MAX;

	/* Store device profile so we can access it if devfreq governor
	 * init needs that */
	g->scale_profile = profile;

	if (platform->devfreq_governor) {
		int error = 0;

		register_gpu_opp(dev);

		profile->devfreq_profile.initial_freq =
			profile->devfreq_profile.freq_table[0];
		profile->devfreq_profile.target = gk20a_scale_target;
		profile->devfreq_profile.get_dev_status =
			gk20a_scale_get_dev_status;
		profile->devfreq_profile.get_cur_freq = get_cur_freq;
		profile->devfreq_profile.polling_ms = 25;

		devfreq = devfreq_add_device(dev,
					&profile->devfreq_profile,
					platform->devfreq_governor, NULL);

		if (IS_ERR(devfreq)) {
			devfreq = NULL;
		} else {
			nvgpu_info(g, "enabled scaling for GPU\n");
		}

		l->devfreq = devfreq;

#ifdef CONFIG_DEVFREQ_THERMAL
		cooling = of_devfreq_cooling_register(dev->of_node, devfreq);
		if (IS_ERR(cooling))
			dev_info(dev, "Failed to register cooling device\n");
		else
			l->cooling = cooling;
#endif

		/* create symlink /sys/devices/gpu.0/devfreq_dev */
		if (devfreq != NULL) {
			error = sysfs_create_link(&dev->kobj,
				&devfreq->dev.kobj, "devfreq_dev");

			if (error) {
				nvgpu_err(g,
					"Failed to create devfreq_dev: %d",
					error);
			}
		}
	}

#ifdef CONFIG_GK20A_PM_QOS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	nvgpu_mutex_init(&profile->lock);

	/* Should we register min frequency QoS callback for this device? */
	if (devfreq && platform->qos_min_notify) {
		profile->qos_min_notify_block.notifier_call =
					platform->qos_min_notify;

		err = dev_pm_qos_add_notifier(devfreq->dev.parent,
					      &profile->qos_min_notify_block,
					      DEV_PM_QOS_MIN_FREQUENCY);
		if (err) {
			nvgpu_err(g, "failed to add min freq notifier %d", err);
		}
	}

	/* Should we register max frequency QoS callback for this device? */
	if (devfreq && platform->qos_max_notify) {
		profile->qos_max_notify_block.notifier_call =
					platform->qos_max_notify;

		err = dev_pm_qos_add_notifier(devfreq->dev.parent,
					      &profile->qos_max_notify_block,
					      DEV_PM_QOS_MAX_FREQUENCY);
		if (err) {
			nvgpu_err(g, "failed to add max freq notifier %d", err);
		}
	}
#else
	/* Should we register QoS callback for this device? */
	if (platform->qos_notify) {
		profile->qos_notify_block.notifier_call =
					platform->qos_notify;

		pm_qos_add_min_notifier(PM_QOS_GPU_FREQ_BOUNDS,
					&profile->qos_notify_block);
		pm_qos_add_max_notifier(PM_QOS_GPU_FREQ_BOUNDS,
					&profile->qos_notify_block);
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */
#endif

	return;

err_get_freqs:
	nvgpu_kfree(g, profile);
}

void gk20a_scale_exit(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = platform->g;
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	struct devfreq *devfreq = l->devfreq;
	struct gk20a_scale_profile *profile;
#endif
	int err;

	if (!platform->devfreq_governor)
		return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	if (!platform->qos_min_notify && !platform->qos_max_notify) {
		return;
	}
#else
	if (!platform->qos_notify) {
		return;
	}
#endif

#ifdef CONFIG_GK20A_PM_QOS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	if (devfreq) {
		profile = g->scale_profile;

		err = dev_pm_qos_remove_notifier(devfreq->dev.parent,
						 &profile->qos_min_notify_block,
						 DEV_PM_QOS_MIN_FREQUENCY);
		if (err) {
			nvgpu_err(g, "failed to remove min freq notifier %d", err);
		}

		err = dev_pm_qos_remove_notifier(devfreq->dev.parent,
						 &profile->qos_max_notify_block,
						 DEV_PM_QOS_MAX_FREQUENCY);
		if (err) {
			nvgpu_err(g, "failed to remove max freq notifier %d", err);
		}
	}

	nvgpu_mutex_destroy(&profile->lock);
#else
	if (platform->qos_notify) {
		pm_qos_remove_min_notifier(PM_QOS_GPU_FREQ_BOUNDS,
				&g->scale_profile->qos_notify_block);
		pm_qos_remove_max_notifier(PM_QOS_GPU_FREQ_BOUNDS,
				&g->scale_profile->qos_notify_block);
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */
#endif

#ifdef CONFIG_DEVFREQ_THERMAL
	if (l->cooling) {
		devfreq_cooling_unregister(l->cooling);
		l->cooling = NULL;
	}
#endif

	if (platform->devfreq_governor) {
		sysfs_remove_link(&dev->kobj, "devfreq_dev");

		err = devfreq_remove_device(l->devfreq);
		l->devfreq = NULL;

		unregister_gpu_opp(dev);
	}

	nvgpu_kfree(g, g->scale_profile);
	g->scale_profile = NULL;
}

/*
 * gk20a_scale_hw_init(dev)
 *
 * Initialize hardware portion of the device
 */

void gk20a_scale_hw_init(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;

	/* make sure that scaling has bee initialised */
	if (!profile)
		return;

	profile->dev_stat.total_time = 0;
	profile->last_event_time = ktime_get();
}
