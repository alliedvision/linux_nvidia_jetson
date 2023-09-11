/*
 * GK20A Graphics
 *
 * Copyright (c) 2011-2023, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/reboot.h>
#include <linux/notifier.h>

#include <nvgpu/vpr.h>

#if NVGPU_VPR_RESIZE_SUPPORTED
#include <linux/platform/tegra/common.h>
#endif
#include <linux/pci.h>
#include <linux/of_gpio.h>

#include <uapi/linux/nvgpu.h>
#ifdef CONFIG_NVGPU_TEGRA_FUSE
#include <dt-bindings/soc/gm20b-fuse.h>
#include <dt-bindings/soc/gp10b-fuse.h>
#include <dt-bindings/soc/gv11b-fuse.h>

#include <dt-bindings/soc/ga10b-fuse.h>

#include <soc/tegra/fuse.h>
#endif /* CONFIG_NVGPU_TEGRA_FUSE */

#include <nvgpu/dma.h>
#include <nvgpu/kmem.h>
#include <nvgpu/nvgpu_common.h>
#include <nvgpu/soc.h>
#include <nvgpu/fbp.h>
#include <nvgpu/enabled.h>
#include <nvgpu/errata.h>
#include <nvgpu/debug.h>
#include <nvgpu/vidmem.h>
#include <nvgpu/sim.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/timers.h>
#include <nvgpu/engines.h>
#include <nvgpu/channel.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/pmu/pmu_pstate.h>
#include <nvgpu/cyclestats_snapshot.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/mc.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/cic_rm.h>
#include <nvgpu/fb.h>
#include <nvgpu/nvs.h>
#include <nvgpu/l1ss_err_reporting.h>

#include "common/gr/gr_priv.h"
#include "platform_gk20a.h"
#include "sysfs.h"
#include "vgpu/vgpu_linux.h"
#include "scale.h"
#include "pci.h"
#include "module.h"

#include "module_usermode.h"
#include "ioctl.h"
#include "ioctl_ctrl.h"

#include "os_linux.h"
#include "os_ops.h"
#include "fecs_trace_linux.h"
#include "driver_common.h"
#include "channel.h"
#include "debug_ce.h"
#include "debug_pmgr.h"
#include "dmabuf_priv.h"

#ifdef CONFIG_NVGPU_GSP_SCHEDULER
#include "nvgpu/gsp.h"
#include "nvgpu/gsp_sched.h"
#endif
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
#include "nvgpu/gsp/gsp_test.h"
#endif

#ifdef CONFIG_NVGPU_SUPPORT_CDE
#include "cde.h"
#endif

#if defined(CONFIG_NVGPU_HAL_NON_FUSA) && defined(CONFIG_NVGPU_NEXT)
#include <nvgpu_next_chips.h>
#endif

#define GK20A_WAIT_FOR_IDLE_MS	2000

#define CREATE_TRACE_POINTS
#include <nvgpu/trace.h>

static int nvgpu_wait_for_idle(struct gk20a *g)
{
	int wait_length = 150; /* 3 second overall max wait. */
	int target_usage_count = 0;
	bool done = false;

	if (g == NULL) {
		return -ENODEV;
	}

	do {
		if (nvgpu_atomic_read(&g->usage_count) == target_usage_count) {
			done = true;
		} else if (wait_length-- < 0) {
			done = true;
		} else {
			nvgpu_msleep(20);
		}
	} while (!done);

	if (wait_length < 0) {
		nvgpu_warn(g, "Timed out waiting for idle (%d)!\n",
			   nvgpu_atomic_read(&g->usage_count));
		return -ETIMEDOUT;
	}

	return 0;
}

static int nvgpu_kernel_shutdown_notification(struct notifier_block *nb,
					unsigned long event, void *unused)
{
	struct nvgpu_os_linux *l = container_of(nb, struct nvgpu_os_linux,
						nvgpu_reboot_nb);
	struct gk20a *g = &l->g;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	nvgpu_set_enabled(g, NVGPU_KERNEL_IS_DYING, true);

	/*
	 * In rmmod path, when the kernel or GPU driver is
	 * dying signal the gr wait queue so that the wait
	 * queue wakes up and further processing happens.
	 * This is needed to prevent other threads, like
	 * pmu_pg_task, to go into un-interruptible state.
	 */
	if (gr != NULL) {
		nvgpu_cond_signal(&gr->init_wq);
	}

	return NOTIFY_DONE;
}

struct device_node *nvgpu_get_node(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);

	if (dev_is_pci(dev)) {
		struct pci_bus *bus = to_pci_dev(dev)->bus;

		while (!pci_is_root_bus(bus))
			bus = bus->parent;

		return bus->bridge->parent->of_node;
	}

	return dev->of_node;
}

void gk20a_busy_noresume(struct gk20a *g)
{
	pm_runtime_get_noresume(dev_from_gk20a(g));
}

int gk20a_busy(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	int ret = 0;
	struct device *dev;

	if (!g)
		return -ENODEV;

	atomic_inc(&g->usage_count.atomic_var);

	down_read(&l->busy_lock);

	if (!nvgpu_can_busy(g)) {
		ret = -ENODEV;
		atomic_dec(&g->usage_count.atomic_var);
		goto fail;
	}

	dev = dev_from_gk20a(g);

	if (pm_runtime_enabled(dev)) {
		/* Increment usage count and attempt to resume device */
		ret = pm_runtime_get_sync(dev);
		if (ret < 0) {
			/* Mark suspended so runtime pm will retry later */
			pm_runtime_set_suspended(dev);
			pm_runtime_put_noidle(dev);
			atomic_dec(&g->usage_count.atomic_var);
			goto fail;
		}
	} else {
		ret = gk20a_gpu_is_virtual(dev) ?
			vgpu_pm_finalize_poweron(dev) :
			gk20a_pm_finalize_poweron(dev);
		if (ret) {
			atomic_dec(&g->usage_count.atomic_var);
			goto fail;
		}
	}

fail:
	up_read(&l->busy_lock);

	return ret < 0 ? ret : 0;
}

void gk20a_idle_nosuspend(struct gk20a *g)
{
	pm_runtime_put_noidle(dev_from_gk20a(g));
}

void gk20a_idle(struct gk20a *g)
{
	struct device *dev;

	atomic_dec(&g->usage_count.atomic_var);

	dev = dev_from_gk20a(g);

	if (!(dev && nvgpu_can_busy(g)))
		return;

	if (pm_runtime_enabled(dev)) {
		pm_runtime_mark_last_busy(dev);
		pm_runtime_put_sync_autosuspend(dev);
	}
}

/*
 * Undoes gk20a_lockout_registers().
 */
static int gk20a_restore_registers(struct gk20a *g)
{
	g->regs = g->regs_saved;
	g->bar1 = g->bar1_saved;

	nvgpu_restore_usermode_registers(g);

	return 0;
}

int nvgpu_finalize_poweron_linux(struct nvgpu_os_linux *l)
{
	struct gk20a *g = &l->g;
	int err;

	if (l->init_done)
		return 0;

	err = nvgpu_channel_init_support_linux(l);
	if (err) {
		nvgpu_err(g, "failed to init linux channel support");
		return err;
	}

#ifdef CONFIG_NVGPU_FECS_TRACE
	err = gk20a_ctxsw_trace_init(g);
	if (err != 0)
		nvgpu_warn(g, "could not initialize ctxsw tracing");
#endif

	if (l->ops.clk.init_debugfs) {
		err = l->ops.clk.init_debugfs(g);
		if (err) {
			nvgpu_err(g, "failed to init linux clk debugfs");
			return err;
		}
	}

	if (l->ops.therm.init_debugfs) {
		err = l->ops.therm.init_debugfs(g);
		if (err) {
			nvgpu_err(g, "failed to init linux therm debugfs");
			return err;
		}
	}

	if (l->ops.fecs_trace.init_debugfs) {
		err = l->ops.fecs_trace.init_debugfs(g);
		if (err) {
			nvgpu_err(g, "failed to init linux fecs trace debugfs");
			return err;
		}
	}

	if (l->ops.volt.init_debugfs) {
		err = l->ops.volt.init_debugfs(g);
		if (err) {
			nvgpu_err(g, "failed to init linux volt debugfs");
			return err;
		}
	}

	if (l->ops.s_param.init_debugfs) {
		err = l->ops.s_param.init_debugfs(g);
		if (err) {
			nvgpu_err(g, "failed to init linux s_param trace debugfs");
			return err;
		}
	}

	err = nvgpu_pmgr_init_debugfs_linux(l);
	if (err) {
		nvgpu_err(g, "failed to init linux pmgr debugfs");
		return err;
	}

#if defined(CONFIG_NVGPU_DGPU) && defined(CONFIG_DEBUG_FS)
	if (!g->is_virtual)
		nvgpu_ce_debugfs_init(g);
#endif

	l->init_done = true;

	return 0;
}

void gk20a_init_linux_characteristics(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);

	nvgpu_set_enabled(g, NVGPU_SUPPORT_PARTIAL_MAPPINGS, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_DETERMINISTIC_OPTS, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_USERSPACE_MANAGED_AS, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_REMAP, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_BUFFER_METADATA, true);

	if (!IS_ENABLED(CONFIG_NVGPU_SYNCFD_NONE)) {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_SYNC_FENCE_FDS, true);
	}

	if (!gk20a_gpu_is_virtual(dev)) {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_MAPPING_MODIFY, true);
	}
}

#ifdef CONFIG_NVGPU_DGPU
static void therm_alert_work_queue(struct work_struct *work)
{

	struct dgpu_thermal_alert *thermal_alert =
		container_of(work, struct dgpu_thermal_alert, work);
	struct nvgpu_os_linux *l =
		container_of(thermal_alert, struct nvgpu_os_linux,
				thermal_alert);
	struct gk20a *g = &l->g;

	nvgpu_clk_arb_send_thermal_alarm(g);
	nvgpu_msleep(l->thermal_alert.event_delay * 1000U);
	enable_irq(l->thermal_alert.therm_alert_irq);
}

static irqreturn_t therm_irq(int irq, void *dev_id)
{
	struct nvgpu_os_linux *l = (struct nvgpu_os_linux *)dev_id;

        disable_irq_nosync(irq);
        queue_work(l->thermal_alert.workqueue, &l->thermal_alert.work);
        return IRQ_HANDLED;
}

static int nvgpu_request_therm_irq(struct nvgpu_os_linux *l)
{
	struct device_node *np;
	int ret = 0, gpio, index = 0;
	u32 irq_flags = IRQ_TYPE_NONE;
	u32 event_delay = 10U;

	if (l->thermal_alert.workqueue != NULL) {
		return ret;
	}
	np = of_find_node_by_name(NULL, "nvgpu");
	if (!np) {
		return -ENOENT;
	}

	gpio = of_get_named_gpio(np, "nvgpu-therm-gpios", index);
	if (gpio < 0) {
		nvgpu_err(&l->g, "failed to get GPIO %d ", gpio);
		return gpio;
	}

	l->thermal_alert.therm_alert_irq = gpio_to_irq(gpio);

	if (of_property_read_u32(np, "alert-interrupt-level", &irq_flags))
		nvgpu_info(&l->g, "Missing interrupt-level "
				"prop using %d", irq_flags);
	if (of_property_read_u32(np, "alert-event-interval", &event_delay))
		nvgpu_info(&l->g, "Missing event-interval "
				"prop using %d seconds ", event_delay);

	l->thermal_alert.event_delay = event_delay;

	if (!l->thermal_alert.workqueue) {
		l->thermal_alert.workqueue = alloc_workqueue("%s",
					WQ_HIGHPRI, 1, "dgpu_thermal_alert");
		INIT_WORK(&l->thermal_alert.work, therm_alert_work_queue);
	}

	ret = devm_request_irq(l->dev, l->thermal_alert.therm_alert_irq ,
			therm_irq, irq_flags, "dgpu_therm", l);
	if (ret != 0) {
		nvgpu_err(&l->g, "IRQ request failed");
	}

	return ret;
}
#endif

int gk20a_pm_finalize_poweron(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	int err = 0;

	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->power_lock);

	if (nvgpu_is_powered_on(g))
		goto done;

	nvgpu_set_power_state(g, NVGPU_STATE_POWERING_ON);

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_finalize_poweron(dev_name(dev));
#endif

	/* Increment platform power refcount */
	if (platform->busy) {
		err = platform->busy(dev);
		if (err < 0) {
			nvgpu_err(g, "failed to poweron platform dependency");
			goto done;
		}
	}

#ifdef CONFIG_NVGPU_ENABLE_MISC_EC
	g->enable_polling = false;
#endif

	err = gk20a_restore_registers(g);
	if (err)
		goto done;

	nvgpu_restore_usermode_for_poweron(g);

	err = nvgpu_early_poweron(g);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_early_poweron failed[%d]", err);
		goto done;
	}

	if (!l->dev_nodes_created) {
		err = gk20a_user_nodes_init(dev);
		if (err) {
			goto done;
		}
		l->dev_nodes_created = true;
	}

	if (g->sim) {
		if (g->sim->sim_init_late)
			err = g->sim->sim_init_late(g);
		if (err)
			goto done;
	}

#ifdef CONFIG_NVGPU_DGPU
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_DGPU_PCIE_SCRIPT_EXECUTE) &&
			nvgpu_platform_is_silicon(g)) {
		if (g->ops.clk.change_host_clk_source != NULL) {
			g->ops.clk.change_host_clk_source(g);
		}
		g->ops.xve.devinit_deferred_settings(g);
	}

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_DGPU_THERMAL_ALERT) &&
		nvgpu_platform_is_silicon(g)) {
		err = nvgpu_request_therm_irq(l);
		if (err && (err != -ENOENT)) {
			nvgpu_err(g, "thermal interrupt request failed %d",
				err);
			goto done;
		}
		if (err == -ENOENT) {
			nvgpu_info(g, "nvgpu-therm-gpio DT entry is missing. "
				"Thermal Alert feature will not be enabled");
		}
	}
#endif

	err = nvgpu_enable_irqs(g);
	if (err) {
		nvgpu_err(g, "failed to enable irqs %d", err);
		goto done;
	}

	err = nvgpu_finalize_poweron(g);
	if (err)
		goto done;

	/* Initialize linux specific flags */
	gk20a_init_linux_characteristics(g);

	err = nvgpu_init_os_linux_ops(l);
	if (err)
		goto done;

	nvgpu_init_usermode_support(g);

	err = nvgpu_finalize_poweron_linux(l);
	if (err)
		goto done;

#ifdef CONFIG_NVGPU_DGPU
	nvgpu_init_mm_ce_context(g);

	nvgpu_vidmem_thread_unpause(&g->mm);
#endif


	/* Initialise scaling: it will initialize scaling drive only once */
	if (IS_ENABLED(CONFIG_GK20A_DEVFREQ) &&
			nvgpu_platform_is_silicon(g)) {
		gk20a_scale_init(dev);
		if (platform->initscale)
			platform->initscale(dev);
	}

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_finalize_poweron_done(dev_name(dev));
#endif

	gk20a_scale_resume(dev_from_gk20a(g));

#ifdef CONFIG_NVGPU_SUPPORT_CDE
	if (platform->has_cde)
		gk20a_init_cde_support(l);
#endif

	err = gk20a_sched_ctrl_init(g);
	if (err) {
		nvgpu_err(g, "failed to init sched control");
		goto done;
	}

	g->sw_ready = true;

	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_ON);

done:
	if (err != 0) {
		nvgpu_disable_irqs(g);
		nvgpu_remove_sim_support_linux(g);
		if (l->dev_nodes_created) {
			gk20a_user_nodes_deinit(dev);
		}
		nvgpu_set_power_state(g, NVGPU_STATE_POWERED_OFF);
	}

	nvgpu_mutex_release(&g->power_lock);
	return err;
}

/*
 * Locks out the driver from accessing GPU registers. This prevents access to
 * thse registers after the GPU has been clock or power gated. This should help
 * find annoying bugs where register reads and writes are silently dropped
 * after the GPU has been turned off. On older chips these reads and writes can
 * also lock the entire CPU up.
 */
static int gk20a_lockout_registers(struct gk20a *g)
{
	g->regs = 0U;
	g->bar1 = 0U;

	nvgpu_lockout_usermode_registers(g);

	return 0;
}

int nvgpu_enable_irqs(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	u32 i;

	for (i = 0U; i < l->interrupts.stall_size; i++) {
		enable_irq(l->interrupts.stall_lines[i]);
	}

	if (l->interrupts.nonstall_size > 0) {
		enable_irq(l->interrupts.nonstall_line);
	}

	return 0;
}

void nvgpu_disable_irqs(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	u32 i;

	for (i = 0U; i < l->interrupts.stall_size; i++) {
		disable_irq(l->interrupts.stall_lines[i]);
	}

	if (l->interrupts.nonstall_size > 0) {
		disable_irq(l->interrupts.nonstall_line);
	}
}

static int gk20a_pm_prepare_poweroff(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
#ifdef CONFIG_NVGPU_SUPPORT_CDE
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
#endif
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	int ret = 0;

	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->power_lock);

	if (nvgpu_is_powered_off(g))
		goto done;

	nvgpu_disable_irqs(g);

	gk20a_scale_suspend(dev);

#ifdef CONFIG_NVGPU_SUPPORT_CDE
	gk20a_cde_suspend(l);
#endif

	ret = nvgpu_prepare_poweroff(g);
	if (ret)
		goto error;

	/* Decrement platform power refcount */
	if (platform->idle)
		platform->idle(dev);

	/* Stop CPU from accessing the GPU registers. */
	gk20a_lockout_registers(g);

	nvgpu_hide_usermode_for_poweroff(g);

	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_OFF);

	nvgpu_mutex_release(&g->power_lock);
	return 0;

error:
	/* Re-enable IRQs on error. This doesn't fail on Linux. */
	(void) nvgpu_enable_irqs(g);

	gk20a_scale_resume(dev);
done:
	nvgpu_mutex_release(&g->power_lock);

	return ret;
}

static struct of_device_id tegra_gk20a_of_match[] = {
#ifdef CONFIG_TEGRA_GK20A
	{ .compatible = "nvidia,tegra210-gm20b",
		.data = &gm20b_tegra_platform },
	{ .compatible = "nvidia,gv11b",
		.data = &gv11b_tegra_platform },
#ifdef CONFIG_NVGPU_GR_VIRTUALIZATION
	{ .compatible = "nvidia,gv11b-vgpu",
		.data = &gv11b_vgpu_tegra_platform},
#endif
#if defined(CONFIG_NVGPU_HAL_NON_FUSA) && defined(CONFIG_NVGPU_NON_FUSA)
	{ .compatible = "nvidia,ga10b",
		.data = &ga10b_tegra_platform},
#ifdef CONFIG_NVGPU_GR_VIRTUALIZATION
	{ .compatible = "nvidia,ga10b-vgpu",
		.data = &ga10b_vgpu_tegra_platform},
#endif
#endif
#endif

#ifdef CONFIG_NVGPU_NEXT
#if defined(CONFIG_NVGPU_HAL_NON_FUSA) && defined(CONFIG_NVGPU_NON_FUSA)
	NVGPU_NEXT_COMPATIBLE_CHIPS
#endif
#endif
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_gk20a_of_match);

#ifdef CONFIG_PM
/* Caller of this API can assume the following return values
 *  1) -EBUSY indicates failure of the API, no locks are held. (Failure)
 *  2) 1 indicates pm_runtime_status_suspended without any locks held
 *     and g->probe_done = false. (Success)
 *  3) 0 indicates function successfully idles the driver and prevents
 *     further jobs. Following steps are executed,
 *	  a) Hold back Deterministic Submits
 *	  b) Down-Write Busy lock
 *	  c) Acquire platform->railgate lock.
 */
int gk20a_block_new_jobs_and_idle(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct nvgpu_timeout timeout;
	int ref_cnt;
	int target_ref_cnt = 0;

	if (!g->probe_done) {
		/*
		 * Note that autosuspend delay is 0 at this point hence the device
		 * will suspend immediately. Deterministic channels, gk20a_busy and
		 * unrailgate don't intervene during probe so no need to hold the
		 * locks below.
		 */
		pm_runtime_put_sync_autosuspend(dev);
		if (pm_runtime_status_suspended(dev)) {
			return 1;
		} else {
			nvgpu_err(g, "failed to idle");
			return -EBUSY;
		}
	}

	/*
	 * Hold back deterministic submits and changes to deterministic
	 * channels - this must be outside the power busy locks.
	 */
	nvgpu_channel_deterministic_idle(g);

	/* acquire busy lock to block other busy() calls */
	down_write(&l->busy_lock);

	/* acquire railgate lock to prevent unrailgate in midst of do_idle() */
	nvgpu_mutex_acquire(&platform->railgate_lock);

	/* check if it is already railgated ? */
	if (platform->is_railgated(dev))
		return 0;

	/*
	 * release railgate_lock, prevent suspend by incrementing usage counter,
	 * re-acquire railgate_lock
	 */
	nvgpu_mutex_release(&platform->railgate_lock);
	pm_runtime_get_sync(dev);

	/*
	 * One refcount taken in this API
	 * If User disables rail gating, we take one more
	 * extra refcount
	 */
	if (nvgpu_is_enabled(g, NVGPU_CAN_RAILGATE))
		target_ref_cnt = 1;
	else
		target_ref_cnt = 2;

	nvgpu_mutex_acquire(&platform->railgate_lock);

	nvgpu_timeout_init_cpu_timer(g, &timeout, GK20A_WAIT_FOR_IDLE_MS);

	/* check and wait until GPU is idle (with a timeout) */
	do {
		nvgpu_usleep_range(1000, 1100);
		ref_cnt = atomic_read(&dev->power.usage_count);
	} while (ref_cnt != target_ref_cnt && !nvgpu_timeout_expired(&timeout));

	if (ref_cnt != target_ref_cnt) {
		nvgpu_err(g, "failed to idle - refcount %d != target_ref_cnt",
			ref_cnt);

		pm_runtime_put_noidle(dev);

		nvgpu_mutex_release(&platform->railgate_lock);
		up_write(&l->busy_lock);
		nvgpu_channel_deterministic_unidle(g);
		return -EBUSY;
	}

	return 0;
}

int gk20a_block_new_jobs_and_poweroff(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret;

	ret = gk20a_block_new_jobs_and_idle(g);
	if (ret == -EBUSY) {
		return ret;
	}

	if (ret == 1) {
		return 0;
	}

	/* check if it is already railgated ? */
	if (platform->is_railgated(dev)) {
		nvgpu_mutex_release(&platform->railgate_lock);
		return 0;
	}

	nvgpu_mutex_release(&platform->railgate_lock);

	/* For joint_xpu_rail platforms, This will decrement the
	 * extra refcount taken by us.
	 */
	if (!nvgpu_is_enabled(g, NVGPU_CAN_RAILGATE)) {
		pm_runtime_dont_use_autosuspend(dev);
	}

	pm_runtime_put_sync_suspend(dev);

	nvgpu_log_info(g, "power usage_count = %d", atomic_read(&dev->power.usage_count));

	return 0;
}

static void gk20a_unblock_jobs(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct device *dev = dev_from_gk20a(g);

	/*  For joint_xpu_rail, its unsafe to keep the rail gated. */
	if (!nvgpu_is_enabled(g, NVGPU_CAN_RAILGATE)) {
		pm_runtime_set_autosuspend_delay(dev, -1);
		pm_runtime_use_autosuspend(dev);
	}

	/* release the lock and open up all other busy() calls */
	up_write(&l->busy_lock);

	nvgpu_channel_deterministic_unidle(g);
}

/**
 * gk20a_do_idle() - force the GPU to idle and railgate
 *
 * In success, this call MUST be balanced by caller with gk20a_do_unidle()
 *
 * Acquires two locks : &l->busy_lock and &platform->railgate_lock
 * In success, we hold these locks and return
 * In failure, we release these locks and return
 */
int gk20a_do_idle(void *_g)
{
	struct gk20a *g = (struct gk20a *)_g;
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret;

	ret = gk20a_block_new_jobs_and_idle(g);
	if (ret == -EBUSY) {
		return ret;
	}

	if (ret == 1) {
		return 0;
	}

	/* check if it is already railgated ? */
	if (platform->is_railgated(dev)) {
		return 0;
	}
	/*
	 * If railgating is enabled, autosuspend delay will be > 0. Set it to
	 * 0 to suspend immediately. If railgating is disabled setting it to
	 * 0 will reduce the usage count. pm_runtime_put_sync_autosuspend
	 * will then suspend immediately.
	 */
	pm_runtime_set_autosuspend_delay(dev, 0);

	pm_runtime_put_sync_autosuspend(dev);

	if (pm_runtime_status_suspended(dev)) {
		return 0;
	} else {
		nvgpu_err(g, "failed to idle in timeout");
		/*
		 * gk20a_do_unidle will release the locks and reset the
		 * autosuspend delay.
		 */
		(void) gk20a_do_unidle(g);
		return -EBUSY;
	}

	return 0;
}

/**
 * gk20a_do_unidle() - unblock all the tasks blocked by gk20a_do_idle()
 */
int gk20a_do_unidle(void *_g)
{
	struct gk20a *g = (struct gk20a *)_g;
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = dev_get_drvdata(dev);

	if (!g->probe_done) {
		pm_runtime_get_sync(dev);
		if (pm_runtime_active(dev)) {
			return 0;
		} else {
			nvgpu_err(g, "failed to unidle");
			return -EBUSY;
		}
	}

	/*
	 * Release the railgate_lock here as setting autosuspend_delay to -1
	 * resumes the device that needs this lock.
	 */
	nvgpu_mutex_release(&platform->railgate_lock);

	if (g->railgate_delay && nvgpu_is_enabled(g, NVGPU_CAN_RAILGATE))
		pm_runtime_set_autosuspend_delay(dev, g->railgate_delay);
	else
		pm_runtime_set_autosuspend_delay(dev, -1);

	/* release the lock and open up all other busy() calls */
	up_write(&l->busy_lock);

	nvgpu_channel_deterministic_unidle(g);

	return 0;
}
#endif

void __iomem *nvgpu_devm_ioremap_resource(struct platform_device *dev, int i,
					  struct resource **out)
{
	struct resource *r = platform_get_resource(dev, IORESOURCE_MEM, i);

	if (!r)
		return NULL;
	if (out)
		*out = r;
	return devm_ioremap_resource(&dev->dev, r);
}

void __iomem *nvgpu_devm_ioremap(struct device *dev, resource_size_t offset,
				 resource_size_t size)
{
	return devm_ioremap(dev, offset, size);
}

u64 nvgpu_resource_addr(struct platform_device *dev, int i)
{
	struct resource *r = platform_get_resource(dev, IORESOURCE_MEM, i);

	if (!r)
		return 0;

	return r->start;
}

static irqreturn_t gk20a_intr_isr_stall(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;
	u32 err = nvgpu_cic_mon_intr_stall_isr(g);

	return err == NVGPU_CIC_INTR_HANDLE ? IRQ_WAKE_THREAD : IRQ_NONE;
}

static irqreturn_t gk20a_intr_thread_isr_stall(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;

	nvgpu_cic_mon_intr_stall_handle(g);
	return IRQ_HANDLED;
}

static irqreturn_t gk20a_intr_isr_nonstall(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;
	u32 err = nvgpu_cic_mon_intr_nonstall_isr(g);

	return err == NVGPU_CIC_INTR_HANDLE ? IRQ_WAKE_THREAD : IRQ_NONE;
}

static irqreturn_t gk20a_intr_thread_isr_nonstall(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;

	nvgpu_cic_mon_intr_nonstall_handle(g);
	return IRQ_HANDLED;
}

void gk20a_remove_support(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct sim_nvgpu_linux *sim_linux;

#ifdef CONFIG_TEGRA_L1SS_SUPPORT
	nvgpu_l1ss_deinit_reporting(g);
#endif

#if NVGPU_VPR_RESIZE_SUPPORTED
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_VPR)) {
		tegra_unregister_idle_unidle(gk20a_do_idle);
	}
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_kfree(g, g->dbg_regops_tmp_buf);
#endif

	nvgpu_channel_remove_support_linux(l);

	if (g->sec2.remove_support != NULL) {
		g->sec2.remove_support(&g->sec2);
	}

	nvgpu_gr_remove_support(g);

#ifdef CONFIG_NVGPU_DGPU
	if (g->mm.remove_ce_support)
		g->mm.remove_ce_support(&g->mm);
#endif

	nvgpu_nvs_remove_support(g);

	if (g->fifo.remove_support)
		g->fifo.remove_support(&g->fifo);

#if defined(CONFIG_NVGPU_NON_FUSA)
	if (nvgpu_fb_vab_teardown_hal(g) != 0) {
		nvgpu_err(g, "failed to teardown VAB");
	}
#endif

	if (g->ops.mm.mmu_fault.info_mem_destroy != NULL) {
		g->ops.mm.mmu_fault.info_mem_destroy(g);
	}

	nvgpu_pmu_remove_support(g, g->pmu);

	if (g->mm.remove_support)
		g->mm.remove_support(&g->mm);

	if (g->sim) {
		sim_linux = container_of(g->sim, struct sim_nvgpu_linux, sim);
		if (g->sim->remove_support)
			g->sim->remove_support(g);
		if (sim_linux->remove_support_linux)
			sim_linux->remove_support_linux(g);
	}

#if defined(CONFIG_NVGPU_CYCLESTATS)
	nvgpu_free_cyclestats_snapshot_data(g);
#endif

#ifndef CONFIG_NVGPU_DGPU
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
	nvgpu_gsp_sched_sw_deinit(g);
#endif
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
	nvgpu_gsp_test_sw_deinit(g);
#endif
#endif

	nvgpu_fbp_remove_support(g);

	nvgpu_remove_usermode_support(g);

	nvgpu_free_enabled_flags(g);
	nvgpu_free_errata_flags(g);

	gk20a_lockout_registers(g);
}

static int gk20a_init_support(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gk20a *g = get_gk20a(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	void __iomem *addr;
	int err = -ENOMEM;

#if NVGPU_VPR_RESIZE_SUPPORTED
	tegra_register_idle_unidle(gk20a_do_idle, gk20a_do_unidle, g);
#endif

	addr = nvgpu_devm_ioremap_resource(pdev,
					      GK20A_BAR0_IORESOURCE_MEM,
					      &l->reg_mem);
	if (IS_ERR(addr)) {
		nvgpu_err(g, "failed to remap gk20a registers");
		err = PTR_ERR(addr);
		goto fail;
	}
	g->regs = (uintptr_t)addr;
	g->regs_size = resource_size(l->reg_mem);

	g->regs_bus_addr = nvgpu_resource_addr(pdev,
			GK20A_BAR0_IORESOURCE_MEM);
	if (!g->regs_bus_addr) {
		nvgpu_err(g, "failed to read register bus offset");
		err = -ENODEV;
		goto fail;
	}

	addr = nvgpu_devm_ioremap_resource(pdev,
					      GK20A_BAR1_IORESOURCE_MEM,
					      &l->bar1_mem);
	if (IS_ERR(addr)) {
		nvgpu_err(g, "failed to remap gk20a bar1");
		err = PTR_ERR(addr);
		goto fail;
	}
	g->bar1 = (uintptr_t)addr;

	err = nvgpu_init_sim_support_linux(g, pdev);
	if (err)
		goto fail;
	err = nvgpu_init_sim_support(g);
	if (err)
		goto fail_sim;

	nvgpu_init_usermode_support(g);
	return 0;

fail_sim:
	nvgpu_remove_sim_support_linux(g);
fail:
	if (g->regs)
		g->regs = 0U;

	if (g->bar1)
		g->bar1 = 0U;

	return err;
}

static int gk20a_pm_railgate(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;
	struct gk20a *g = get_gk20a(dev);

	/* return early if platform didn't implement railgate */
	if (!platform->railgate)
		return 0;

	/* if platform is already railgated, then just return */
	if (platform->is_railgated && platform->is_railgated(dev))
		return ret;

#ifdef CONFIG_DEBUG_FS
	g->pstats.last_rail_gate_start = jiffies;

	if (g->pstats.railgating_cycle_count >= 1)
		g->pstats.total_rail_ungate_time_ms =
			g->pstats.total_rail_ungate_time_ms +
			jiffies_to_msecs(g->pstats.last_rail_gate_start -
					g->pstats.last_rail_ungate_complete);
#endif

	nvgpu_mutex_acquire(&g->static_pg_lock);

	ret = platform->railgate(dev);
	if (ret) {
		nvgpu_err(g, "failed to railgate platform, err=%d", ret);
		nvgpu_mutex_release(&g->static_pg_lock);
		return ret;
	}

	nvgpu_mutex_release(&g->static_pg_lock);

#ifdef CONFIG_DEBUG_FS
	g->pstats.last_rail_gate_complete = jiffies;
#endif

	return ret;
}

static int gk20a_pm_unrailgate(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;
#ifdef CONFIG_DEBUG_FS
	struct gk20a *g = get_gk20a(dev);
#endif

	/* return early if platform didn't implement unrailgate */
	if (!platform->unrailgate)
		return 0;

#ifdef CONFIG_DEBUG_FS
	g->pstats.last_rail_ungate_start = jiffies;
	if (g->pstats.railgating_cycle_count >= 1)
		g->pstats.total_rail_gate_time_ms =
			g->pstats.total_rail_gate_time_ms +
			jiffies_to_msecs(g->pstats.last_rail_ungate_start -
				g->pstats.last_rail_gate_complete);

	g->pstats.railgating_cycle_count++;
#endif

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_pm_unrailgate(dev_name(dev));
#endif

	nvgpu_mutex_acquire(&platform->railgate_lock);
	ret = platform->unrailgate(dev);
	nvgpu_mutex_release(&platform->railgate_lock);

#ifdef CONFIG_DEBUG_FS
	g->pstats.last_rail_ungate_complete = jiffies;
#endif

	return ret;
}

/*
 * Remove association of the driver with OS interrupt handler
 */
void nvgpu_free_irq(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	u32 i;

	for (i = 0U; i < l->interrupts.stall_size; i++) {
		devm_free_irq(dev, l->interrupts.stall_lines[i], g);
	}
	if (l->interrupts.nonstall_size > 0) {
		devm_free_irq(dev, l->interrupts.nonstall_line, g);
	}
}

/*
 * Idle the GPU in preparation of shutdown/remove.
 * gk20a_driver_start_unload() does not idle the GPU, but instead changes the SW
 * state to prevent further activity on the driver SW side.
 * On driver removal quiesce() should be called after start_unload()
 */
int nvgpu_quiesce(struct gk20a *g)
{
	int err;
	struct device *dev = dev_from_gk20a(g);

	if (nvgpu_is_powered_on(g)) {
		err = nvgpu_wait_for_idle(g);
		if (err) {
			nvgpu_err(g, "failed to idle GPU, err=%d", err);
			return err;
		}

		err = nvgpu_engine_disable_activity_all(g, true);
		if (err) {
			nvgpu_err(g,
				"failed to disable engine activity, err=%d",
				err);
		return err;
		}

		err = nvgpu_engine_wait_for_idle(g);
		if (err) {
			nvgpu_err(g, "failed to idle engines, err=%d",
				err);
			return err;
		}
	}

	if (gk20a_gpu_is_virtual(dev))
		err = vgpu_pm_prepare_poweroff(dev);
	else
		err = gk20a_pm_prepare_poweroff(dev);

	if (err)
		nvgpu_err(g, "failed to prepare for poweroff, err=%d",
			err);

	return err;
}

static void gk20a_pm_shutdown(struct platform_device *pdev)
{
	struct gk20a_platform *platform = platform_get_drvdata(pdev);
	struct gk20a *g = platform->g;
	int err;

	nvgpu_info(g, "shutting down");

	/* vgpu has nothing to clean up currently */
	if (gk20a_gpu_is_virtual(&pdev->dev))
		return;

	if (nvgpu_is_powered_off(g))
		goto finish;

	gk20a_driver_start_unload(g);

	/* If GPU is already railgated,
	 * just prevent more requests, and return */
	if (platform->is_railgated && platform->is_railgated(&pdev->dev)) {
		__pm_runtime_disable(&pdev->dev, false);
		nvgpu_info(g, "already railgated, shut down complete");
		return;
	}

	/* Prevent more requests by disabling Runtime PM */
	__pm_runtime_disable(&pdev->dev, false);

	err = nvgpu_quiesce(g);
	if (err)
		goto finish;

	err = gk20a_pm_railgate(&pdev->dev);
	if (err)
		nvgpu_err(g, "failed to railgate, err=%d", err);

finish:
	nvgpu_info(g, "shut down complete");
}

#ifdef CONFIG_PM
static int gk20a_pm_runtime_resume(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	int err = 0;

	err = gk20a_pm_unrailgate(dev);
	if (err)
		goto fail;

	if (!g->probe_done) {
		return 0;
	}

	if (gk20a_gpu_is_virtual(dev))
		err = vgpu_pm_finalize_poweron(dev);
	else
		err = gk20a_pm_finalize_poweron(dev);
	if (err)
		goto fail_poweron;

	return 0;

fail_poweron:
	gk20a_pm_railgate(dev);
fail:
	return err;
}

static int gk20a_pm_runtime_suspend(struct device *dev)
{
	int err = 0;
	struct gk20a *g = get_gk20a(dev);

	if (!g)
		return 0;

	if (!g->probe_done) {
		err = gk20a_pm_railgate(dev);
		if (err)
			pm_runtime_mark_last_busy(dev);
		return err;
	}

	if (gk20a_gpu_is_virtual(dev))
		err = vgpu_pm_prepare_poweroff(dev);
	else
		err = gk20a_pm_prepare_poweroff(dev);
	if (err) {
		nvgpu_err(g, "failed to power off, err=%d", err);
		goto fail;
	}

	err = gk20a_pm_railgate(dev);
	if (err)
		goto fail;

	return 0;

fail:
	gk20a_pm_finalize_poweron(dev);
	pm_runtime_mark_last_busy(dev);
	return err;
}

static int gk20a_pm_suspend(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = get_gk20a(dev);
	int ret = 0;
	int usage_count;
	struct nvgpu_timeout timeout;

	if (nvgpu_is_powered_off(g)) {
		if (platform->suspend)
			ret = platform->suspend(dev);

		if (ret)
			return ret;

		if (!pm_runtime_enabled(dev))
			ret = gk20a_pm_railgate(dev);

		return ret;
	}

	nvgpu_timeout_init_cpu_timer(g, &timeout, GK20A_WAIT_FOR_IDLE_MS);
	/*
	 * Hold back deterministic submits and changes to deterministic
	 * channels - this must be outside the power busy locks.
	 */
	nvgpu_channel_deterministic_idle(g);

	/* check and wait until GPU is idle (with a timeout) */
	do {
		nvgpu_usleep_range(1000, 1100);
		usage_count = nvgpu_atomic_read(&g->usage_count);
	} while (usage_count != 0 && !nvgpu_timeout_expired(&timeout));

	if (usage_count != 0) {
		nvgpu_err(g, "failed to idle - usage_count %d", usage_count);
		ret = -EINVAL;
		goto fail_idle;
	}

	/* For cases where we don't have railgate enabled,
	 * we acquire an extra refcount in PM framework.
	 *
	 * Release it here to unblock device suspend.
	 * The below method releases the extra refcount taken
	 * above and disables auto suspend.
	 */
	if (!nvgpu_is_enabled(g, NVGPU_CAN_RAILGATE))
		pm_runtime_dont_use_autosuspend(dev);

	ret = gk20a_pm_runtime_suspend(dev);
	if (ret)
		goto fail_idle;

	if (platform->suspend)
		ret = platform->suspend(dev);
	if (ret)
		goto fail_suspend;

	g->suspended = true;

	return 0;

fail_suspend:
	gk20a_pm_runtime_resume(dev);
fail_idle:
	nvgpu_channel_deterministic_unidle(g);
	return ret;
}

static int gk20a_pm_resume(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = get_gk20a(dev);
	int ret = 0;

	if (!g->suspended) {
		if (platform->resume)
			ret = platform->resume(dev);
		if (ret)
			return ret;

		if (!pm_runtime_enabled(dev))
			ret = gk20a_pm_unrailgate(dev);

		return ret;
	}

	if (platform->resume)
		ret = platform->resume(dev);
	if (ret)
		return ret;

	ret = gk20a_pm_runtime_resume(dev);
	if (ret)
		return ret;

	/* For cases where we don't have railgate enabled,
	 * acquire extra reference in PM framework to prevent
	 * runtime suspend/resume.
	 */
	if (!nvgpu_is_enabled(g, NVGPU_CAN_RAILGATE)) {
		pm_runtime_set_autosuspend_delay(dev, -1);
		pm_runtime_use_autosuspend(dev);
	}

	g->suspended = false;

	nvgpu_channel_deterministic_unidle(g);

	return ret;
}

static const struct dev_pm_ops gk20a_pm_ops = {
	.runtime_resume = gk20a_pm_runtime_resume,
	.runtime_suspend = gk20a_pm_runtime_suspend,
	.resume = gk20a_pm_resume,
	.suspend = gk20a_pm_suspend,
};
#endif

static int gk20a_pm_init(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	int err = 0;

	nvgpu_log_fn(g, " ");

	/*
	 * runtime PM is enabled here. Irrespective of the device power state,
	 * it is resumed and suspended as part of nvgpu_probe due to dependency
	 * on clocks setup. From there onwards runtime PM is truly enabled.
	 */
	pm_runtime_enable(dev);

	return err;
}

static int gk20a_pm_late_init(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	int err = 0;

	nvgpu_log_fn(g, " ");

	pm_runtime_disable(dev);

	/*
	 * For railgate disable case, set autosuspend delay to negative which
	 * will avoid runtime pm suspend.
	 */
	if (g->railgate_delay && nvgpu_is_enabled(g, NVGPU_CAN_RAILGATE))
		pm_runtime_set_autosuspend_delay(dev,
				 g->railgate_delay);
	else
		pm_runtime_set_autosuspend_delay(dev, -1);

	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);

	return err;
}

static int gk20a_pm_deinit(struct device *dev)
{
	pm_runtime_dont_use_autosuspend(dev);
	pm_runtime_disable(dev);
	return 0;
}

void nvgpu_start_gpu_idle(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	down_write(&l->busy_lock);
	nvgpu_set_enabled(g, NVGPU_DRIVER_IS_DYING, true);
	/*
	 * In rmmod path, when the kernel or GPU driver is
	 * dying signal the gr wait queue so that the wait
	 * queue wakes up and further processing happens.
	 * This is needed to prevent other threads, like
	 * pmu_pg_task, to go into un-interruptible state.
	 */
	if (gr != NULL) {
		nvgpu_cond_signal(&gr->init_wq);
	}
	/*
	 * GR SW ready needs to be invalidated at this time with the busy lock
	 * held to prevent a racing condition on the gr/mm code
	 */
	nvgpu_gr_sw_ready(g, false);
	g->sw_ready = false;
	up_write(&l->busy_lock);
}

int nvgpu_wait_for_gpu_idle(struct gk20a *g)
{
	int ret = 0;

	ret = nvgpu_wait_for_idle(g);
	if (ret) {
		nvgpu_err(g, "failed in wait for idle");
		goto out;
	}

	nvgpu_cic_rm_wait_for_deferred_interrupts(g);
out:
	return ret;
}

/*
 * Start the process for unloading the driver. Set NVGPU_DRIVER_IS_DYING.
 */
void gk20a_driver_start_unload(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_shutdown, "Driver is now going down!\n");

	nvgpu_start_gpu_idle(g);

	if (g->is_virtual)
		return;

	nvgpu_wait_for_idle(g);

	nvgpu_cic_rm_wait_for_deferred_interrupts(g);
}

static inline void set_gk20a(struct platform_device *pdev, struct gk20a *gk20a)
{
	gk20a_get_platform(&pdev->dev)->g = gk20a;
}

static int nvgpu_read_fuse_overrides(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_TEGRA_FUSE
	struct device_node *np = nvgpu_get_node(g);
	struct gk20a_platform *platform = dev_get_drvdata(dev_from_gk20a(g));
	u32 *fuses;
	int count, i;
	int ret = 0;

	if (!np) /* may be pcie device */
		return 0;

	count = of_property_count_elems_of_size(np, "fuse-overrides", 8);
	if (count <= 0)
		return count;

	fuses = nvgpu_kmalloc(g, sizeof(u32) * count * 2);
	if (!fuses)
		return -ENOMEM;
	of_property_read_u32_array(np, "fuse-overrides", fuses, count * 2);
	for (i = 0; i < count; i++) {
		u32 fuse, value;

		fuse = fuses[2 * i];
		value = fuses[2 * i + 1];
		switch (fuse) {
		case GM20B_FUSE_OPT_TPC_DISABLE:
			g->tpc_fs_mask_user = ~value;
			break;
		case GP10B_FUSE_OPT_ECC_EN:
			g->fecs_feature_override_ecc_val = value;
			break;
#ifdef CONFIG_NVGPU_STATIC_POWERGATE
		case GV11B_FUSE_OPT_TPC_DISABLE:
			if (platform->set_tpc_pg_mask != NULL) {
				ret = platform->set_tpc_pg_mask(dev_from_gk20a(g),
								value);
				if (ret != 0) {
					return -EINVAL;
				}
			}
			break;
		case GA10B_FUSE_OPT_TPC_DISABLE:
			if (platform->set_tpc_pg_mask != NULL) {
				ret = platform->set_tpc_pg_mask(dev_from_gk20a(g),
								value);
				if (ret != 0) {
					return -EINVAL;
				}
			}
			break;
		case GA10B_FUSE_OPT_GPC_DISABLE:
			if (platform->set_gpc_pg_mask != NULL) {
				ret = platform->set_gpc_pg_mask(dev_from_gk20a(g),
								value);
				if (ret != 0) {
					return -EINVAL;
				}
			}
			break;
		case GA10B_FUSE_OPT_FBP_DISABLE:
			if (platform->set_fbp_pg_mask != NULL) {
				ret = platform->set_fbp_pg_mask(dev_from_gk20a(g),
								value);
				if (ret != 0) {
					return -EINVAL;
				}
			}
			break;
#endif
		default:
			nvgpu_err(g, "ignore unknown fuse override %08x", fuse);
			break;
		}
	}

	nvgpu_kfree(g, fuses);
#endif
	return 0;
}

static int gk20a_probe(struct platform_device *dev)
{
	struct nvgpu_os_linux *l = NULL;
	struct gk20a *gk20a;
	int err;
	struct gk20a_platform *platform = NULL;
	struct device_node *np;
	u32 i, intr_size, irq_idx;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_gk20a_of_match, &dev->dev);
		if (match)
			platform = (struct gk20a_platform *)match->data;
	} else
		platform = (struct gk20a_platform *)dev->dev.platform_data;

	if (!platform) {
		dev_err(&dev->dev, "no platform data\n");
		return -ENODATA;
	}

	platform_set_drvdata(dev, platform);

	if (gk20a_gpu_is_virtual(&dev->dev))
		return vgpu_probe(dev);

	l = kzalloc(sizeof(*l), GFP_KERNEL);
	if (!l) {
		dev_err(&dev->dev, "couldn't allocate gk20a support");
		return -ENOMEM;
	}

	gk20a = &l->g;

	nvgpu_log_fn(gk20a, " ");

	nvgpu_init_gk20a(gk20a);
	set_gk20a(dev, gk20a);
	gk20a->probe_done = false;
	l->dev = &dev->dev;
	gk20a->log_mask = NVGPU_DEFAULT_DBG_MASK;

	nvgpu_kmem_init(gk20a);

	err = nvgpu_init_errata_flags(gk20a);
	if (err)
		goto return_err_platform;

	err = nvgpu_init_enabled_flags(gk20a);
	if (err)
		goto return_err_errata;

	np = nvgpu_get_node(gk20a);
	if (of_dma_is_coherent(np)) {
		nvgpu_set_enabled(gk20a, NVGPU_USE_COHERENT_SYSMEM, true);
		nvgpu_set_enabled(gk20a, NVGPU_SUPPORT_IO_COHERENCE, true);
	}

	if (nvgpu_platform_is_simulation(gk20a))
		nvgpu_set_enabled(gk20a, NVGPU_IS_FMODEL, true);

	err = nvgpu_cic_mon_setup(gk20a);
	if (err != 0) {
		nvgpu_err(gk20a, "CIC-MON setup failed");
		goto return_err_cic_mon;
	}

	intr_size = platform_irq_count(dev);
	if (intr_size > 0U && intr_size <= NVGPU_MAX_INTERRUPTS) {
		irq_idx = 0U;

		/* Single interrupt line could be a stall line*/
		l->interrupts.nonstall_size = intr_size == 1U ? 0U : 1U;
		l->interrupts.stall_size = intr_size - l->interrupts.nonstall_size;

		for (i = 0U; i < l->interrupts.stall_size; i++) {
			l->interrupts.stall_lines[i] = platform_get_irq(dev, i);
			if ((int)l->interrupts.stall_lines[i] < 0) {
				err = -ENXIO;
				goto return_err;
			}
		}
		if (l->interrupts.nonstall_size > 0U) {
			l->interrupts.nonstall_line = platform_get_irq(dev, i);
			if ((int)l->interrupts.nonstall_line < 0) {
				err = -ENXIO;
				goto return_err;
			}
		}
	} else {
		dev_err(&dev->dev, "Invalid intr lines\n");
		err = -ENXIO;
		goto return_err;
	}

	for (i = 0U; i < l->interrupts.stall_size; i++) {
		err = devm_request_threaded_irq(&dev->dev,
			l->interrupts.stall_lines[i],
			gk20a_intr_isr_stall,
			gk20a_intr_thread_isr_stall,
			0, "gk20a_stall", gk20a);
		if (err) {
			dev_err(&dev->dev,
			"failed to request stall intr irq @ %d\n",
				l->interrupts.stall_lines[i]);
			goto return_err;

		}
	}
	if (l->interrupts.nonstall_size > 0) {
		err = devm_request_threaded_irq(&dev->dev,
			l->interrupts.nonstall_line,
			gk20a_intr_isr_nonstall,
			gk20a_intr_thread_isr_nonstall,
				0, "gk20a_nonstall", gk20a);
		if (err) {
			dev_err(&dev->dev,
				"failed to request non-stall intr irq @ %d\n",
					l->interrupts.nonstall_line);
			goto return_err;
		}
	}

	nvgpu_disable_irqs(gk20a);

	err = gk20a_init_support(dev);
	if (err)
		goto return_err;

	err = nvgpu_read_fuse_overrides(gk20a);

#ifdef CONFIG_RESET_CONTROLLER
	platform->reset_control = devm_reset_control_get(&dev->dev, NULL);
	if (IS_ERR(platform->reset_control))
		platform->reset_control = NULL;
#endif

	err = gk20a_pm_init(&dev->dev);
	if (err) {
		dev_err(&dev->dev, "pm init failed");
		goto return_err;
	}

	err = nvgpu_probe(gk20a, "gpu.0");
	if (err)
		goto return_err;

	err = gk20a_pm_late_init(&dev->dev);
	if (err) {
		dev_err(&dev->dev, "pm late_init failed");
		goto return_err;
	}

	l->nvgpu_reboot_nb.notifier_call =
		nvgpu_kernel_shutdown_notification;
	err = register_reboot_notifier(&l->nvgpu_reboot_nb);
	if (err)
		goto return_err;

#ifdef CONFIG_TEGRA_L1SS_SUPPORT
	nvgpu_l1ss_init_reporting(gk20a);
#endif

#ifdef CONFIG_NVGPU_COMPRESSION
	nvgpu_mutex_init(&l->dmabuf_priv_list_lock);
	nvgpu_init_list_node(&l->dmabuf_priv_list);
#endif

	gk20a->probe_done = true;

	return 0;

return_err:
	nvgpu_cic_mon_remove(gk20a);
return_err_cic_mon:
	nvgpu_free_enabled_flags(gk20a);
return_err_errata:
	nvgpu_free_errata_flags(gk20a);
return_err_platform:

	/*
	 * Last since the above allocs may use data structures in here.
	 */
	nvgpu_kmem_fini(gk20a, NVGPU_KMEM_FINI_FORCE_CLEANUP);
	kfree(l);

	return err;
}

int gk20a_driver_force_power_off(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	int err = 0;

	if (!capable(CAP_SYS_NICE)) {
		nvgpu_err(g, "User doesn't have the permission for this operation");
		return -EPERM;
	}

#ifdef CONFIG_NVGPU_DGPU
	if (g->pci_class) {
		nvgpu_err(g, "Poweroff is not supported for device yet.");
		return -EINVAL;
	}
#endif

	err = gk20a_block_new_jobs_and_poweroff(g);
	if (err != 0)
		goto done;

	nvgpu_gr_remove_support(g);

	/*
	 * This is a WAR.
	 * For T210, powernode must not allow device nodes to be powered off
	 * even during a force poweroff. Once the WAR for T210 is removed,
	 * this will hold true for all chips.
	 */
	if (platform->platform_chip_id != TEGRA_210)
		gk20a_user_nodes_deinit(dev);

	gk20a_unblock_jobs(g);

done:
	if (err != 0) {
		nvgpu_err(g, "failed to poweroff");
	}

	return err;
}

int nvgpu_remove(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
#ifdef CONFIG_NVGPU_SUPPORT_CDE
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
#endif
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	int err;

	nvgpu_log_fn(g, " ");

	nvgpu_start_gpu_idle(g);
	err = nvgpu_quiesce(g);
	WARN(err, "gpu failed to idle during driver removal");

	if (nvgpu_mem_is_valid(&g->syncpt_mem))
		nvgpu_dma_free(g, &g->syncpt_mem);

#ifdef CONFIG_NVGPU_SUPPORT_CDE
	if (platform->has_cde)
		gk20a_cde_destroy(l);
#endif

#ifdef CONFIG_NVGPU_FECS_TRACE
	gk20a_ctxsw_trace_cleanup(g);
#endif

	gk20a_sched_ctrl_cleanup(g);

	if (IS_ENABLED(CONFIG_GK20A_DEVFREQ))
		gk20a_scale_exit(dev);

	nvgpu_clk_arb_cleanup_arbiter(g);

	gk20a_user_nodes_deinit(dev_from_gk20a(g));

	gk20a_power_node_deinit(dev_from_gk20a(g));

	nvgpu_remove_sysfs(dev);

	if (platform->secure_buffer.destroy)
		platform->secure_buffer.destroy(g,
				&platform->secure_buffer);

	if (platform->remove)
		platform->remove(dev);

	nvgpu_mutex_destroy(&g->clk_arb_enable_lock);

	nvgpu_log_fn(g, "removed");

	return err;
}

static int __exit gk20a_remove(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;
	struct gk20a *g = get_gk20a(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	if (gk20a_gpu_is_virtual(dev))
		return vgpu_remove(pdev);

	err = nvgpu_remove(dev);

#ifdef CONFIG_NVGPU_COMPRESSION
	gk20a_dma_buf_priv_list_clear(l);
	nvgpu_mutex_destroy(&l->dmabuf_priv_list_lock);
#endif

	unregister_reboot_notifier(&l->nvgpu_reboot_nb);

	set_gk20a(pdev, NULL);

	nvgpu_put(g);

	gk20a_pm_deinit(dev);

	return err;
}

static struct platform_driver gk20a_driver = {
	.probe = gk20a_probe,
	.remove = __exit_p(gk20a_remove),
	.shutdown = gk20a_pm_shutdown,
	.driver = {
		.owner = THIS_MODULE,
		.name = "gk20a",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#ifdef CONFIG_OF
		.of_match_table = tegra_gk20a_of_match,
#endif
#ifdef CONFIG_PM
		.pm = &gk20a_pm_ops,
#endif
		.suppress_bind_attrs = true,
	}
};

static int __init gk20a_init(void)
{

	int ret;

	ret = nvgpu_pci_init();
	if (ret)
		return ret;

	return platform_driver_register(&gk20a_driver);
}

static void __exit gk20a_exit(void)
{
	nvgpu_pci_exit();
	platform_driver_unregister(&gk20a_driver);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0)
MODULE_IMPORT_NS(DMA_BUF);
#endif
#ifdef CONFIG_GK20A_DEVFREQ
MODULE_SOFTDEP("pre: governor_pod_scaling_v2");
#endif
MODULE_LICENSE("GPL v2");
module_init(gk20a_init);
module_exit(gk20a_exit);
