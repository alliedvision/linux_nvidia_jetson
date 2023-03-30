/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cpu_cooling.h>
#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/smp.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <asm/cpuidle.h>
#include <linux/suspend.h>
#include <linux/wait.h>
#include <soc/tegra/virt/syscalls.h>
#include <soc/tegra/virt/tegra_hv_sysmgr.h>
#include "../../kernel/irq/internals.h"

static struct cpumask cpumask;

static void suspend_all_device_irqs(void)
{
	struct irq_desc *desc;
	int irq;

	for_each_irq_desc(irq, desc) {
		unsigned long flags;

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

		raw_spin_lock_irqsave(&desc->lock, flags);
		__enable_irq(desc);
		raw_spin_unlock_irqrestore(&desc->lock, flags);
	}
}

static int tegra_auto_suspend_notify_callback(struct notifier_block *nb,
	unsigned long action, void *pcpu)
{
	switch (action) {
	case PM_SUSPEND_PREPARE:
		cpumask_clear(&cpumask);
		break;
	case PM_POST_SUSPEND:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block suspend_notifier = {
	.notifier_call = tegra_auto_suspend_notify_callback,
};

/*
 * tegra_auto_enter_idle_state - Programs CPU to enter the specified state
 *
 * dev: cpuidle device
 * drv: cpuidle driver
 * idx: state index
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int tegra_auto_enter_idle_state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	if (idle_should_enter_s2idle() == true) {
		int cpu_id = smp_processor_id();
		int boot_cpu_id = get_boot_cpu_id();

		if (cpu_id == boot_cpu_id) {
			int error = 0;
			int cpu_number = 0;

			suspend_all_device_irqs();

			for_each_online_cpu(cpu_number) {
				if (cpu_number == boot_cpu_id)
					continue;
				while (!cpumask_test_cpu(cpu_number,
					(const struct cpumask *)&cpumask))
					udelay(10);
			}

			/*
			 * Causes the linux guest VM to suspend. After SC7
			 * resume it resumes from this point.
			 */
			error = hyp_guest_reset(GUEST_PAUSE_CMD(0), NULL);
			if (error < 0)
				pr_err("%s: Failed to trigger suspend, %d\n",
						__func__, error);

			resume_all_device_irqs();

			pm_system_wakeup();

		} else {
			preempt_disable();
			local_irq_disable();

			cpumask_test_and_set_cpu(cpu_id, &cpumask);

			do {
				asm volatile("wfi\n");
			} while (idle_should_enter_s2idle() == true);

			local_irq_enable();
			preempt_enable_no_resched();
		}

	} else {
		asm volatile("wfi\n");
	}

	return 0;
}

static struct cpuidle_driver tegra_auto_idle_driver __initdata = {
	.name = "tegra_auto_idle",
	.owner = THIS_MODULE,
	/*
	 * State at index 0 is standby wfi and considered standard
	 * on all ARM platforms.
	 */
	.state_count = 1,
	.states[0] = {
		.enter                  = tegra_auto_enter_idle_state,
		.exit_latency           = 1,
		.target_residency       = 1,
		.power_usage		= UINT_MAX,
		.name                   = "WFI",
		.desc                   = "ARM WFI",
	},
};

/*
 * tegra_auto_idle_init_cpu
 *
 * Registers the tegra_auto specific cpuidle driver with the cpuidle
 * framework.
 */
static int __init tegra_auto_idle_init_cpu(int cpu)
{
	int ret = 0;
	struct cpuidle_driver *drv;

	drv = kmemdup(&tegra_auto_idle_driver, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->cpumask = (struct cpumask *)cpumask_of(cpu);

	ret = cpuidle_register(drv, NULL);
	if (ret) {
		pr_err("cpu register failed\n");
		goto out_kfree_drv;
	}

	return 0;
out_kfree_drv:
	kfree(drv);
	return ret;
}

/*
 * tegra_auto_cpuidle_probe - Initializes cpuidle driver
 *
 * Initializes cpuidle driver for all CPUs, if any CPU fails
 * to register cpuidle driver then rollback to cancel all CPUs
 * registeration.
 */
static int __init tegra_auto_cpuidle_probe(struct platform_device *pdev)
{
	int cpu, ret;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;

	for_each_possible_cpu(cpu) {
		ret = tegra_auto_idle_init_cpu(cpu);
		if (ret)
			goto out_fail;
	}
	register_pm_notifier(&suspend_notifier);

	return 0;
out_fail:
	while (--cpu >= 0) {
		dev = per_cpu(cpuidle_devices, cpu);
		drv = cpuidle_get_cpu_driver(dev);
		cpuidle_unregister(drv);
		kfree(drv);
	}

	return ret;
}

static int tegra_auto_cpuidle_remove(struct platform_device *pdev)
{
	int cpu;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;

	for_each_possible_cpu(cpu) {
		dev = per_cpu(cpuidle_devices, cpu);
		drv = cpuidle_get_cpu_driver(dev);
		cpuidle_unregister(drv);
		kfree(drv);
	}
	return 0;
}

static const struct of_device_id tegra_auto_cpuidle_of[] = {
	{ .compatible = "nvidia,cpuidle-tegra-auto" },
	{ },
};

static struct platform_driver tegra_auto_cpuidle_driver __refdata = {
	.probe	= tegra_auto_cpuidle_probe,
	.remove	= tegra_auto_cpuidle_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "cpuidle_tegra_auto",
		.of_match_table = of_match_ptr(tegra_auto_cpuidle_of)
	}
};

module_platform_driver(tegra_auto_cpuidle_driver);
