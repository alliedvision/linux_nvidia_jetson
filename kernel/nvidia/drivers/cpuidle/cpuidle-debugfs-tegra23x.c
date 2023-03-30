// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2021, NVIDIA Corporation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/psci.h>
#include <linux/cpu_pm.h>
#include <linux/t23x_ari.h>
#include <linux/tegra-mce.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/tick.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <asm/arch_timer.h>
#include <asm/cpuidle.h>
#include "../../drivers/cpuidle/dt_idle_states.h"
#include "../../kernel/irq/internals.h"
#include "../../kernel/time/tick-internal.h"

#define T23x_CPUIDLE_C1_STATE		1
#define T23x_CPUIDLE_C7_STATE		7
#define T23x_OIST_STATE			8
#define C7_PSCI_PARAM			0x40000007
#define POWER_STATE_TYPE_MASK		(0x1 << 30)
#define TIMER_CTL_CTX			0x3U

static u64 forced_idle_state;

static bool check_mce_version(void)
{
	u32 mce_version_major, mce_version_minor;
	int err;

	err = tegra_mce_read_versions(&mce_version_major, &mce_version_minor);
	if (!err && (mce_version_major >= TEGRA_ARI_VERSION_MAJOR))
		return true;
	else
		return false;
}

static bool is_timer_irq(struct irq_desc *desc)
{
	return desc && desc->action && (desc->action->flags & IRQF_TIMER);
}

static void suspend_all_device_irqs(void)
{
	struct irq_desc *desc;
	int irq;

	for_each_irq_desc(irq, desc) {
		unsigned long flags;

		/* Don't disable the 'wakeup' interrupt */
		if (is_timer_irq(desc))
			continue;

		raw_spin_lock_irqsave(&desc->lock, flags);
		__disable_irq(desc);
		raw_spin_unlock_irqrestore(&desc->lock, flags);
	}
}

static void resume_all_device_irqs(void)
{
	struct irq_desc *desc;
	int irq;

	for_each_irq_desc(irq, desc) {
		unsigned long flags;

		if (is_timer_irq(desc))
			continue;

		raw_spin_lock_irqsave(&desc->lock, flags);
		__enable_irq(desc);
		raw_spin_unlock_irqrestore(&desc->lock, flags);
	}
}

static int forced_idle_write(void *data, u64 val)
{
	unsigned long timer_interval_us = (ulong)val;
	ktime_t time, interval, sleep;
	int ret = 0;
	uint32_t psci_state = 0;
	uint32_t idle_state = forced_idle_state & 0xF;

	if (idle_state == T23x_CPUIDLE_C1_STATE)
		pr_debug("forcing C1\n");
	else if (idle_state == T23x_CPUIDLE_C7_STATE)
		psci_state = C7_PSCI_PARAM;
	else if (idle_state == T23x_OIST_STATE) {
		psci_state = forced_idle_state & 0xFFFFFFFF;
		if (!(psci_state & POWER_STATE_TYPE_MASK)) {
			pr_err("%s: EXT_POWER_STATE_TYPE bit not set\n", __func__);
			return -EINVAL;
		}
	} else {
		pr_err("%s: Requested invalid forced idle state\n", __func__);
		return -EINVAL;
	}

	suspend_all_device_irqs();
	preempt_disable();
	tick_nohz_idle_enter();
	stop_critical_timings();
	local_irq_disable();

	/* Program timer for C1/C7 wake IRQ */
	if (idle_state == T23x_CPUIDLE_C7_STATE ||
			idle_state == T23x_CPUIDLE_C1_STATE) {
		interval = ktime_set(0, (NSEC_PER_USEC * timer_interval_us));
		time = ktime_get();
		sleep = ktime_add(time, interval);
		tick_program_event(sleep, true);
	}

	if (idle_state == T23x_CPUIDLE_C1_STATE)
		asm volatile("wfi\n");
	else {
#if KERNEL_VERSION(4, 15, 0) < LINUX_VERSION_CODE
		ret = cpu_pm_enter();
		if (!ret) {
			ret = psci_cpu_suspend_enter(psci_state);
			cpu_pm_exit();
		}
#endif
	}

	local_irq_enable();
	start_critical_timings();
	tick_nohz_idle_exit();
	preempt_enable_no_resched();
	resume_all_device_irqs();

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(duration_us_fops, NULL, forced_idle_write, "%llu\n");

static struct dentry *cpuidle_debugfs_node;

static int cpuidle_debugfs_init(void)
{
	cpuidle_debugfs_node = debugfs_create_dir("tegra_cpuidle", NULL);
	if (!cpuidle_debugfs_node)
		goto err_out;

	debugfs_create_u64("forced_idle_state", 0644,
		cpuidle_debugfs_node, &forced_idle_state);

	debugfs_create_file("forced_idle_duration_us", 0200,
		cpuidle_debugfs_node, NULL, &duration_us_fops);

	return 0;

err_out:
	pr_err("%s: Couldn't create debugfs node for cpuidle\n", __func__);
	debugfs_remove_recursive(cpuidle_debugfs_node);
	return -ENOMEM;
}

#ifdef CONFIG_CPU_PM
static DEFINE_PER_CPU(uint32_t, saved_timer_ctl_reg);
static int t23x_cpuidle_cpu_pm_notify(struct notifier_block *self,
				    unsigned long action, void *hcpu)
{
	if (action == CPU_PM_ENTER)
		__this_cpu_write(saved_timer_ctl_reg,
			read_sysreg(cnthp_ctl_el2) & TIMER_CTL_CTX);
	else if (action == CPU_PM_ENTER_FAILED || action == CPU_PM_EXIT)
		write_sysreg(__this_cpu_read(saved_timer_ctl_reg),
							cnthp_ctl_el2);

	return NOTIFY_OK;
}

static struct notifier_block t23x_cpuidle_cpu_pm_notifier = {
	.notifier_call = t23x_cpuidle_cpu_pm_notify,
};

static int __init t23x_cpuidle_cpu_pm_init(void)
{
	return cpu_pm_register_notifier(&t23x_cpuidle_cpu_pm_notifier);
}
#else
static int __init t23x_cpuidle_cpu_pm_init(void)
{
	return 0;
}
#endif

static int tegra23x_cpuidle_debug_probe(struct platform_device *pdev)
{
	int ret;

	if (!check_mce_version()) {
		pr_err("Incompatible MCE version\n");
		return -ENODEV;
	}

	ret = t23x_cpuidle_cpu_pm_init();
	if (ret) {
		pr_err("Error registering PM notifiers\n");
		return ret;
	}

	ret = cpuidle_debugfs_init();
	if (ret) {
		pr_err("Initializing cpuidle debugfs failed\n");
		return ret;
	}

	return 0;

}

static int tegra23x_cpuidle_debug_remove(struct platform_device *pdev)
{
	debugfs_remove_recursive(cpuidle_debugfs_node);
	cpu_pm_unregister_notifier(&t23x_cpuidle_cpu_pm_notifier);
	return 0;
}

static const struct of_device_id tegra23x_cpuidle_debug_of[] = {
	{ .compatible = "nvidia,tegra23x-cpuidle-debugfs" },
	{}
};

static struct platform_driver tegra23x_cpuidle_debug_driver = {
	.probe	= tegra23x_cpuidle_debug_probe,
	.remove	= tegra23x_cpuidle_debug_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "cpuidle-debug-tegra23x",
		.of_match_table = of_match_ptr(tegra23x_cpuidle_debug_of)
	}
};

static int __init tegra_cpuidle_debug_init(void)
{
	return platform_driver_register(&tegra23x_cpuidle_debug_driver);
}
subsys_initcall(tegra_cpuidle_debug_init);
