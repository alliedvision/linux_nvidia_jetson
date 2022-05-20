/*
 * drivers/cpuidle/cpuidle-tegra19x.c
 *
 * Copyright (C) 2017-2021, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <linux/tegra-mce.h>
#include <linux/t194_nvg.h>
#include <linux/suspend.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/tick.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include "../../kernel/irq/internals.h"
#include <linux/pm_qos.h>
#include <linux/cpu_pm.h>
#include <linux/psci.h>
#include <linux/version.h>
#include <linux/cpuhotplug.h>
#include <linux/atomic.h>
#include <linux/platform/tegra/t19x-cpuidle.h>

#define CREATE_TRACE_POINTS
#include <trace/events/cpuidle_t19x.h>

#include <linux/of_gpio.h>
#include <asm/cpuidle.h>
#include <asm/suspend.h>
#include <asm/cputype.h> /* cpuid */
#include <asm/cpu.h>
#include <asm/arch_timer.h>
#include "../../drivers/cpuidle/dt_idle_states.h"
#include "../../kernel/time/tick-internal.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 8, 0)
#include "../../drivers/cpuidle/cpuidle-psci.h"
#endif

#define PSCI_STATE_ID_WKTIM_MASK	(~0xf000000f)
#define PSCI_STATE_ID_WKTIM_SHIFT	4
#define CORE_WAKE_MASK			0x180C
#define T19x_CPUIDLE_C7_STATE		2
#define T19x_CPUIDLE_C6_STATE		1
#define MCE_STAT_ID_SHIFT		16UL

/*
 * BG_TIME is margin added to target_residency so that actual HW
 * has better chance entering deep idle state instead of getting
 * back to shallower one.
 */
#define BG_TIME				2000 /* in unit of us */

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 8, 0)
struct psci_cpuidle_data {
	u32 *psci_states;
	struct device *dev;
};

static DEFINE_PER_CPU_READ_MOSTLY(struct psci_cpuidle_data, psci_cpuidle_data);
#endif

/* per CPU sleep_time holds target_residency for next expected idle state */
static DEFINE_PER_CPU(u32, sleep_time);

static u32 read_cluster_info(struct device_node *of_states);
static u32 deepest_cc_state;
static u64 forced_idle_state;
static u64 forced_cluster_idle_state;
static u64 test_c6_exit_latency;
static atomic_t entered_c6_cpu_count = ATOMIC_INIT(0);
static u32 testmode;
static struct cpuidle_driver t19x_cpu_idle_driver;
static int crossover_init(void);
static void program_cc_state(void *data);
static u32 tsc_per_sec, nsec_per_tsc_tick;
static u32 tsc_per_usec;

/* saved hotplug state */
static enum cpuhp_state hp_state;

#define T19x_NVG_CROSSOVER_C6	TEGRA_NVG_CHANNEL_CROSSOVER_C6_LOWER_BOUND
#define T19x_NVG_CROSSOVER_CC6	TEGRA_NVG_CHANNEL_CROSSOVER_CC6_LOWER_BOUND

static bool check_mce_version(void)
{
	u32 mce_version_major, mce_version_minor;
	int err;

	err = tegra_mce_read_versions(&mce_version_major, &mce_version_minor);
	if (!err && (mce_version_major >= TEGRA_NVG_VERSION_MAJOR))
		return true;
	else
		return false;
}

int read_cpu_counter(void)
{
	return atomic_read(&entered_c6_cpu_count);
}
EXPORT_SYMBOL(read_cpu_counter);

void clear_cpu_counter(void)
{
	atomic_set(&entered_c6_cpu_count, 0);
}
EXPORT_SYMBOL(clear_cpu_counter);

static void t19x_cpu_enter_c6(int index)
{
	int cpu = smp_processor_id();
	struct cpuidle_driver *drv = &t19x_cpu_idle_driver;
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 8, 0)
	u32 *state = __this_cpu_read(psci_cpuidle_data.psci_states);
#endif

	per_cpu(sleep_time, cpu) = drv->states[index].target_residency;

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 8, 0)
	psci_cpu_suspend_enter(state[index]);
#else
	arm_cpuidle_suspend(index);
#endif
}

/*enter C6 function used in measuring C6 latency*/
static void test_t19x_cpu_enter_c6(u32 wake_time)
{
	int cpu;
	u64 val;
	u32 mce_index;

	cpu = smp_processor_id();
	mce_index = (NVG_STAT_QUERY_C6_ENTRIES << MCE_STAT_ID_SHIFT)
					+ (u32)cpu;
	tegra_mce_read_cstate_stats(mce_index, &val);
	trace_cpuidle_t19x_c6_count(cpu, val, "C6_COUNT_BEFORE");

	atomic_inc(&entered_c6_cpu_count);

	t19x_cpu_enter_c6(T19x_CPUIDLE_C6_STATE);
	trace_cpuidle_t19x_print("Exiting C6");

	tegra_mce_read_cstate_stats(mce_index, &val);
	trace_cpuidle_t19x_c6_count(cpu, val, "C6_COUNT_AFTER");
}

static void t19x_cpu_enter_c7(int index)
{
	int cpu = smp_processor_id();
	struct cpuidle_driver *drv = &t19x_cpu_idle_driver;
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 8, 0)
	u32 *state = __this_cpu_read(psci_cpuidle_data.psci_states);
#endif

	cpu_pm_enter(); /* power down notifier */
	per_cpu(sleep_time, cpu) = drv->states[index].target_residency;

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 8, 0)
	psci_cpu_suspend_enter(state[index]);
#else
	arm_cpuidle_suspend(index);
#endif

	cpu_pm_exit();
}

static int t19x_cpu_enter_state(
		struct cpuidle_device *dev,
		struct cpuidle_driver *drv,
		int index)
{
	if (tegra_platform_is_vdk()) {
		asm volatile("wfi\n");
		return index;
	}

	if (testmode) {
		tegra_mce_update_cstate_info(forced_cluster_idle_state,
				0, 0, 0, 0, 0);
		if (forced_idle_state >= t19x_cpu_idle_driver.state_count) {
			pr_err("%s: Requested invalid forced idle state\n",
				__func__);
			index = t19x_cpu_idle_driver.state_count;
		} else
			index = forced_idle_state;
	}

	if (index == T19x_CPUIDLE_C7_STATE)
		t19x_cpu_enter_c7(index);
	else if (index == T19x_CPUIDLE_C6_STATE)
		t19x_cpu_enter_c6(index);
	else
		asm volatile("wfi\n");

	return index;
}

static u32 t19x_make_power_state(u32 state)
{
	int cpu = smp_processor_id();

	u32 wake_time = (per_cpu(sleep_time, cpu) + BG_TIME) * tsc_per_usec;

	if (testmode || test_c6_exit_latency)
		wake_time = 0xFFFFEEEE;

	/* The 8-LSB bits of wake time is lost and only 24 MSB bits
	of wake time can fit into the additional state id bits */
	state = state | ((wake_time >> PSCI_STATE_ID_WKTIM_SHIFT)
				& PSCI_STATE_ID_WKTIM_MASK);

	return state;
}

static struct cpuidle_driver t19x_cpu_idle_driver = {
	.name = "tegra19x_cpuidle_driver",
	.owner = THIS_MODULE,
	/*
	 * State at index 0 is standby wfi and considered standard
	 * on all ARM platforms. If in some platforms simple wfi
	 * can't be used as "state 0", DT bindings must be implemented
	 * to work around this issue and allow installing a special
	 * handler for idle state index 0.
	 */
	.states[0] = {
		.enter			= t19x_cpu_enter_state,
		.exit_latency		= 1,
		.target_residency	= 1,
		.power_usage		= UINT_MAX,
		.flags			= 0,
		.name			= "C1",
		.desc			= "c1-cpu-clockgated",
	}
};

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

	for_each_irq_desc(irq, desc) {
		if (is_timer_irq(desc))
			continue;
		synchronize_irq(irq);
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

static u64 dbg_gpio;

static struct dentry *cpuidle_debugfs_node;

static int forced_idle_write(void *data, u64 val)
{
	unsigned long timer_interval_us = (ulong)val;
	ktime_t time, interval, sleep;
	u32 pmstate;
	u32 wake_time;

	val = (val * 1000) / nsec_per_tsc_tick;
	if (val > 0xffffffff)
		val = 0xffffffff;
	wake_time = val;

	if (forced_idle_state >= t19x_cpu_idle_driver.state_count) {
		pr_err("%s: Requested invalid forced idle state\n", __func__);
		return -EINVAL;
	}

	suspend_all_device_irqs();
	preempt_disable();
	tick_nohz_idle_enter();
	stop_critical_timings();
	local_irq_disable();

	interval = ktime_set(0, (NSEC_PER_USEC * timer_interval_us));

	time = ktime_get();
	sleep = ktime_add(time, interval);
	tick_program_event(sleep, true);

	pmstate = forced_idle_state;
	if (dbg_gpio)
		gpio_set_value(dbg_gpio, 1);

	tegra_mce_update_cstate_info(forced_cluster_idle_state, 0, 0, 0, 0, 0);

	if (pmstate == T19x_CPUIDLE_C7_STATE)
		t19x_cpu_enter_c7(pmstate);
	else if (pmstate == T19x_CPUIDLE_C6_STATE) {
		if (test_c6_exit_latency)
			test_t19x_cpu_enter_c6(wake_time);
		else
			t19x_cpu_enter_c6(pmstate);
	}
	else
		asm volatile("wfi\n");

	sleep = ktime_sub(ktime_get(), time);
	time = ktime_sub(sleep, interval);
	if (dbg_gpio)
		gpio_set_value(dbg_gpio, 0);

	pr_info("idle: %lld, exit latency: %lld\n",
				ktime_to_ns(sleep), ktime_to_ns(time));

	local_irq_enable();
	start_critical_timings();
	tick_nohz_idle_exit();
	preempt_enable_no_resched();
	resume_all_device_irqs();

	return 0;
}

void force_idle_c6(u64 delay)
{
	forced_idle_write(NULL, delay);
}
EXPORT_SYMBOL(force_idle_c6);

struct xover_smp_call_data {
	int index;
	int value;
};

static void program_single_crossover(void *data)
{
	struct xover_smp_call_data *xover_data =
		(struct xover_smp_call_data *)data;
	tegra_mce_update_crossover_time(xover_data->index,
					xover_data->value * tsc_per_usec);
}

static int setup_crossover(int index, int value)
{
	struct xover_smp_call_data xover_data;

	xover_data.index = index;
	xover_data.value = value;

	on_each_cpu_mask(cpu_online_mask, program_single_crossover,
			&xover_data, 1);
	return 0;
}

static int c6_xover_write(void *data, u64 val)
{
	return setup_crossover(T19x_NVG_CROSSOVER_C6, (u32) val);
}

static int cc6_xover_write(void *data, u64 val)
{
	return setup_crossover(T19x_NVG_CROSSOVER_CC6, (u32) val);
}

static int set_testmode(void *data, u64 val)
{
	testmode = (u32)val;
	if (testmode) {
		setup_crossover(T19x_NVG_CROSSOVER_C6, 0);
		setup_crossover(T19x_NVG_CROSSOVER_CC6, 0);
	} else {
		/* Restore the cluster state */
		on_each_cpu_mask(cpu_online_mask,
			program_cc_state, &deepest_cc_state, 1);
		/* Restore the crossover values */
		crossover_init();
	}
	return 0;
}

static int cc_state_set(void *data, u64 val)
{
	deepest_cc_state = (u32)val;
	on_each_cpu_mask(cpu_online_mask, program_cc_state,
			&deepest_cc_state, 1);
	return 0;
}

static int cc_state_get(void *data, u64 *val)
{
	*val = (u64) deepest_cc_state;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(duration_us_fops, NULL, forced_idle_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(xover_c6_fops, NULL, c6_xover_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(xover_cc6_fops, NULL,
						cc6_xover_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(cc_state_fops, cc_state_get,
						cc_state_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(testmode_fops, NULL, set_testmode, "%llu\n");

static int cpuidle_debugfs_init(void)
{
	struct dentry *dfs_file;

	cpuidle_debugfs_node = debugfs_create_dir("tegra_cpuidle", NULL);
	if (!cpuidle_debugfs_node)
		goto err_out;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	dfs_file = debugfs_create_u64("forced_idle_state", 0644,
		cpuidle_debugfs_node, &forced_idle_state);

	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_u64("test_c6_exit_latency", 0644,
		cpuidle_debugfs_node, &test_c6_exit_latency);

	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_u64("forced_cluster_idle_state", 0644,
		cpuidle_debugfs_node, &forced_cluster_idle_state);

	if (!dfs_file)
		goto err_out;
#else
	debugfs_create_u64("forced_idle_state", 0644,
		cpuidle_debugfs_node, &forced_idle_state);

	debugfs_create_u64("test_c6_exit_latency", 0644,
		cpuidle_debugfs_node, &test_c6_exit_latency);

	debugfs_create_u64("forced_cluster_idle_state", 0644,
		cpuidle_debugfs_node, &forced_cluster_idle_state);
#endif

	dfs_file = debugfs_create_file("forced_idle_duration_us", 0200,
		cpuidle_debugfs_node, NULL, &duration_us_fops);

	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("testmode", 0200,
		cpuidle_debugfs_node, NULL, &testmode_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("crossover_c1_c6", 0200,
		cpuidle_debugfs_node, NULL, &xover_c6_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("crossover_cc1_cc6", 0200,
		cpuidle_debugfs_node, NULL, &xover_cc6_fops);
	if (!dfs_file)
		goto err_out;

	dfs_file = debugfs_create_file("deepest_cc_state", 0644,
		cpuidle_debugfs_node, NULL, &cc_state_fops);
	if (!dfs_file)
		goto err_out;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	dfs_file = debugfs_create_u64("dbg_gpio", 0644, cpuidle_debugfs_node,
					&dbg_gpio);
	if (!dfs_file)
		goto err_out;
#else
	debugfs_create_u64("dbg_gpio", 0644, cpuidle_debugfs_node,
					&dbg_gpio);
#endif

	return 0;

err_out:
	pr_err("%s: Couldn't create debugfs node for cpuidle\n", __func__);
	debugfs_remove_recursive(cpuidle_debugfs_node);
	return -ENOMEM;
}

static const struct of_device_id t19x_idle_of_match[] = {
	{ .compatible = "nvidia,tegra194-cpuidle-core",
	  .data = t19x_cpu_enter_state },
	{ },
};

static u32 read_cluster_info(struct device_node *of_states)
{
	u32 power = UINT_MAX;
	u32 value, pmstate, deepest_pmstate = 0;
	struct device_node *child;
	int err;

	for_each_child_of_node(of_states, child) {
		if (of_property_match_string(child, "status", "okay"))
			continue;
		err = of_property_read_u32(child, "power", &value);
		if (err) {
			pr_warn(" %s missing power property\n",
				child->full_name);
			continue;
		}
		err = of_property_read_u32(child, "pmstate", &pmstate);
		if (err) {
			pr_warn(" %s missing pmstate property\n",
				child->full_name);
			continue;
		}
		/* Enable the deepest power state */
		if (value > power)
			continue;
		power = value;
		deepest_pmstate = pmstate;
	}
	return deepest_pmstate;
}

struct xover_table {
	char *name;
	int index;
};

static void send_crossover(void *data)
{
	struct device_node *child;
	struct device_node *of_states = (struct device_node *)data;
	u32 value;
	int i;

	struct xover_table table1[] = {
		{"crossover_c1_c6", T19x_NVG_CROSSOVER_C6},
		{"crossover_cc1_cc6", T19x_NVG_CROSSOVER_CC6},
	};

	for_each_child_of_node(of_states, child)
		for (i = 0; i < sizeof(table1)/sizeof(table1[0]); i++) {
			if (of_property_read_u32(child,
				table1[i].name, &value) == 0)
				tegra_mce_update_crossover_time
					(table1[i].index, value * tsc_per_usec);
	}
}

static int crossover_init(void)
{
	struct device_node *cpu_xover;

	cpu_xover = of_find_node_by_name(NULL,
			"cpu_crossover_thresholds");

	pr_debug("cpuidle: Init Power Crossover thresholds.\n");

	if (!cpu_xover) {
		pr_err("WARNING: cpuidle: %s: DT entry ", __func__);
		pr_err("missing for Crossover thresholds\n");
	} else
		on_each_cpu_mask(cpu_online_mask, send_crossover,
			cpu_xover, 1);

	return 0;
}

static void program_cc_state(void *data)
{
	u32 *cc_state = (u32 *)data;

	tegra_mce_update_cstate_info(*cc_state, 0, 0, 0, 0, 0);
}

static int tegra_suspend_notify_callback(struct notifier_block *nb,
	unsigned long action, void *pcpu)
{
	switch (action) {
	case PM_POST_SUSPEND:
	/*
	 * Re-program deepest allowed cluster and cluster group power state
	 * after system resumes from SC7
	 */
		on_each_cpu_mask(cpu_online_mask, program_cc_state,
			&deepest_cc_state, 1);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block suspend_notifier = {
	.notifier_call = tegra_suspend_notify_callback,
};

static int tegra_cpu_online(unsigned int cpu)
{
	/*
	 * Re-program deepest allowed cluster and cluster group power state
	 * after a core in that cluster is onlined.
	 */
	smp_call_function_single(cpu, program_cc_state,
		&deepest_cc_state, 1);

	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 8, 0)
static int psci_dt_cpu_init_idle(struct device *dev, struct cpuidle_driver *drv,
				 struct device_node *cpu_node,
				 unsigned int state_count, int cpu)
{
	int i, ret = 0;
	u32 *psci_states;
	struct device_node *state_node;
	struct psci_cpuidle_data *data = per_cpu_ptr(&psci_cpuidle_data, cpu);

	state_count++; /* Add WFI state too */
	psci_states = devm_kcalloc(dev, state_count, sizeof(*psci_states),
					GFP_KERNEL);
	if (!psci_states)
		return -ENOMEM;

	for (i = 1; i < state_count; i++) {
		state_node = of_get_cpu_state_node(cpu_node, i - 1);
		if (!state_node)
			break;

		ret = psci_dt_parse_state_node(state_node, &psci_states[i]);
		of_node_put(state_node);

		if (ret)
			return ret;

		pr_debug("psci-power-state %#x index %d\n", psci_states[i], i);
	}

	if (i != state_count)
		return -ENODEV;

	/* Idle states parsed correctly, store them in the per-cpu struct. */
	data->psci_states = psci_states;
	return 0;
}

static int psci_cpu_init_idle(struct device *dev, struct cpuidle_driver *drv,
			      unsigned int cpu, unsigned int state_count)
{
	struct device_node *cpu_node;
	int ret;

	/*
	 * If the PSCI cpu_suspend function hook has not been initialized
	 * idle states must not be enabled, so bail out
	 */
	if (!psci_ops.cpu_suspend)
		return -EOPNOTSUPP;

	cpu_node = of_cpu_device_node_get(cpu);
	if (!cpu_node)
		return -ENODEV;

	ret = psci_dt_cpu_init_idle(dev, drv, cpu_node, state_count, cpu);

	of_node_put(cpu_node);

	return ret;
}

static int psci_idle_init_cpu(struct device *dev, int cpu,
				unsigned int state_count)
{
	int ret = 0;

	/*
	 * Initialize PSCI idle states.
	 */
	ret = psci_cpu_init_idle(dev, NULL, cpu, state_count);
	if (ret) {
		pr_err("CPU %d failed to PSCI idle\n", cpu);
		return ret;
	}

	return 0;
}

static int psci_idle_node_init(struct device *dev, unsigned int state_count)
{
	int cpu;
	int ret = 0;

	for_each_possible_cpu(cpu) {
		ret = psci_idle_init_cpu(dev, cpu, state_count);
		if (ret)
			goto out_fail;
	}

	return 0;

out_fail:
	return ret;
}

#endif

static int __init tegra19x_cpuidle_probe(struct platform_device *pdev)
{
	int cpu_number;
	struct device_node *cpu_cc_states;
	int err;
	struct cpumask *cpumask;

	if (!check_mce_version()) {
		pr_err("cpuidle: Incompatible MCE version. Not registering\n");
		return -ENODEV;
	}

	tsc_per_sec = arch_timer_get_cntfrq();
	nsec_per_tsc_tick = 1000000000/tsc_per_sec;
	tsc_per_usec = tsc_per_sec / 1000000;

	cpumask = kmalloc(sizeof(struct cpumask), GFP_KERNEL);
	cpumask_clear(cpumask);

	for_each_online_cpu(cpu_number) {
		cpumask_set_cpu(cpu_number, cpumask);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
		err = arm_cpuidle_init(cpu_number);
		if (err) {
			pr_err("cpuidle: failed to init ops for cpu %d \n",
				cpu_number);
			goto probe_exit;
		}
#endif
	}

	crossover_init();

	cpu_cc_states =
		of_find_node_by_name(NULL, "cpu_cluster_power_states");

	pr_info("cpuidle: Initializing cpuidle driver\n");
	extended_ops.make_power_state = t19x_make_power_state;

	/* read cluster state info from DT */
	deepest_cc_state = read_cluster_info(cpu_cc_states);
	on_each_cpu_mask(cpu_online_mask, program_cc_state,
			&deepest_cc_state, 1);

	t19x_cpu_idle_driver.cpumask = cpumask;
	err = dt_init_idle_driver(&t19x_cpu_idle_driver,
		t19x_idle_of_match, 1);
	if (err <= 0) {
		pr_err("cpuidle: failed to init idle driver states \n");
		err = -ENODEV;
		goto probe_exit;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 8, 0)
	/*
	 * Initialize PSCI idle states.
	 *
	 * dt_init_idle_driver() returns number of valid DT idle states parsed
	 * on successful, pass it to psci_idle_node_init()
	 */
	err = psci_idle_node_init(&pdev->dev, err);
	if (err)
		goto probe_exit;
#endif

	err = cpuidle_register(&t19x_cpu_idle_driver, NULL);

	if (err) {
		pr_err("cpuidle: failed to register cpuidle driver \n");
		goto probe_exit;
	}

	err = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
				  "tegra_cpu:online",
				  tegra_cpu_online,
				  NULL);
	if (err < 0) {
		pr_err("unable to register cpuhp state\n");
		goto cpuhp_error;
	}

	hp_state = err;

	cpuidle_debugfs_init();

	register_pm_notifier(&suspend_notifier);
	return 0;

cpuhp_error:
	cpuidle_unregister(&t19x_cpu_idle_driver);
probe_exit:
	kfree(cpumask);
	pr_err("cpuidle: failed to register cpuidle driver\n");
	return err;
}

static int tegra19x_cpuidle_remove(struct platform_device *pdev)
{
	cpuidle_unregister(&t19x_cpu_idle_driver);
	kfree(t19x_cpu_idle_driver.cpumask);
	cpuhp_remove_state(hp_state);
	unregister_pm_notifier(&suspend_notifier);
	return 0;
}

static const struct of_device_id tegra19x_cpuidle_of[] = {
	{ .compatible = "nvidia,tegra19x-cpuidle" },
	{}
};

static struct platform_driver tegra19x_cpuidle_driver __refdata = {
	.probe	= tegra19x_cpuidle_probe,
	.remove	= tegra19x_cpuidle_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "cpuidle-tegra19x",
		.of_match_table = of_match_ptr(tegra19x_cpuidle_of)
	}
};

module_platform_driver(tegra19x_cpuidle_driver);
