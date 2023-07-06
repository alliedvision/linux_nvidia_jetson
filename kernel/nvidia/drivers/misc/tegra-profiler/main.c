/*
 * drivers/misc/tegra-profiler/main.c
 *
 * Copyright (c) 2013-2023, NVIDIA CORPORATION.  All rights reserved.
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
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/sched.h>

#include <linux/tegra_profiler.h>

#include "quadd.h"
#include "arm_pmu.h"
#include "hrt.h"
#include "comm.h"
#include "mmap.h"
#include "tegra.h"
#include "power_clk.h"
#include "auth.h"
#include "version.h"
#include "quadd_proc.h"
#include "eh_unwind.h"
#include "uncore_events.h"

#if defined(CONFIG_ARCH_TEGRA_19x_SOC) || defined(CONFIG_ARCH_TEGRA_194_SOC)
#include "carmel_pmu.h"
#endif

#if defined(CONFIG_ARCH_TEGRA_23x_SOC) || defined(CONFIG_ARCH_TEGRA_234_SOC)
#include "tegra23x_pmu_scf.h"
#include "tegra23x_pmu_dsu.h"
#endif

#ifdef CONFIG_ARM64
#include "armv8_pmu.h"
#else
#include "armv7_pmu.h"
#endif

static struct quadd_ctx ctx;
static DEFINE_PER_CPU(struct source_info, ctx_pmu_info);
static DEFINE_PER_CPU(struct quadd_comm_cap_for_cpu, per_cpu_caps);

static struct source_info *get_pmu_info_for_current_cpu(void)
{
	return this_cpu_ptr(&ctx_pmu_info);
}

static struct quadd_comm_cap_for_cpu *get_capabilities_for_cpu_int(int cpuid)
{
	return &per_cpu(per_cpu_caps, cpuid);
}

int tegra_profiler_try_lock(void)
{
	return atomic_cmpxchg(&ctx.tegra_profiler_lock, 0, 1);
}
EXPORT_SYMBOL_GPL(tegra_profiler_try_lock);

void tegra_profiler_unlock(void)
{
	atomic_set(&ctx.tegra_profiler_lock, 0);
}
EXPORT_SYMBOL_GPL(tegra_profiler_unlock);

static int start(void)
{
	int err;

	if (tegra_profiler_try_lock()) {
		pr_err("Error: tegra_profiler lock\n");
		return -EBUSY;
	}

	if (!atomic_cmpxchg(&ctx.started, 0, 1)) {
		if (quadd_mode_is_sampling(&ctx)) {
			if (ctx.pmu) {
				err = ctx.pmu->enable();
				if (err) {
					pr_err("error: pmu enable\n");
					goto out_err;
				}
			}
		}

		ctx.comm->reset();

		err = quadd_hrt_start();
		if (err) {
			pr_err("error: hrt start\n");
			goto out_err;
		}

		err = quadd_uncore_start();
		if (err) {
			pr_err("error: uncore start\n");
			goto out_err_hrt;
		}

		err = quadd_power_clk_start();
		if (err < 0) {
			pr_err("error: power_clk start\n");
			goto out_err_uncore;
		}
	}

	return 0;

out_err_uncore:
	quadd_uncore_stop();
out_err_hrt:
	quadd_hrt_stop();
out_err:
	atomic_set(&ctx.started, 0);
	tegra_profiler_unlock();

	return err;
}

static void stop(void)
{
	int cpu;

	if (atomic_cmpxchg(&ctx.started, 1, 0)) {
		quadd_hrt_stop();
		quadd_uncore_stop();
		quadd_power_clk_stop();

		ctx.comm->reset();
		quadd_unwind_stop();

		if (ctx.pmu) {
			ctx.pmu->disable();
			for_each_possible_cpu(cpu)
				per_cpu(ctx_pmu_info, cpu).active = 0;
		}

		if (ctx.carmel_pmu)
			ctx.carmel_pmu_info.active = 0;
		if (ctx.tegra23x_pmu_scf)
			ctx.tegra23x_pmu_scf_info.active = 0;
		if (ctx.tegra23x_pmu_dsu)
			ctx.tegra23x_pmu_dsu_info.active = 0;

		tegra_profiler_unlock();
	}
}

static inline int
is_event_supported(struct source_info *si, const struct quadd_event *event)
{
	unsigned int type, id;
	int i, nr = si->nr_supp_events;
	struct quadd_event *events = si->supp_events;

	type = event->type;
	id = event->id;

	if (type == QUADD_EVENT_TYPE_RAW ||
	    type == QUADD_EVENT_TYPE_RAW_CARMEL_UNCORE ||
	    type == QUADD_EVENT_TYPE_RAW_T23X_UNCORE_SCF ||
	    type == QUADD_EVENT_TYPE_RAW_T23X_UNCORE_DSU)
		return (id & ~si->raw_event_mask) == 0;

	if (type == QUADD_EVENT_TYPE_HARDWARE) {
		for (i = 0; i < nr; i++) {
			if (id == events[i].id)
				return 1;
		}
	}

	return 0;
}

static inline bool
validate_freq(unsigned int freq)
{
	return freq >= 100 && freq <= 100000;
}

static inline bool
validate_clk_freq(unsigned int freq)
{
	return freq > 0 && freq <= 1000;
}

static int
set_parameters_for_cpu(struct quadd_pmu_setup_for_cpu *params)
{
	unsigned int i, nr_pmu = 0;
	int err, cpuid = params->cpuid;

	struct source_info *pmu_info = &per_cpu(ctx_pmu_info, cpuid);
	struct quadd_event pmu_events[QUADD_MAX_COUNTERS];

	if (!ctx.mode_is_sampling)
		return -EINVAL;

	if (!pmu_info->is_present)
		return -ENODEV;

	if (pmu_info->nr_supp_events == 0)
		return -ENODEV;

	if (params->nr_events > QUADD_MAX_COUNTERS)
		return -EINVAL;

	for (i = 0; i < params->nr_events; i++) {
		struct quadd_event *event = &params->events[i];

		if (is_event_supported(pmu_info, event)) {
			pmu_events[nr_pmu++] = *event;
			pr_debug("[%d] PMU active event: %#x (%s)\n",
				 cpuid, event->id,
				 event->type == QUADD_EVENT_TYPE_RAW ?
				 "raw" : "hw");
		} else {
			pr_err("[%d] Bad event: %#x (%s)\n", cpuid, event->id,
			       event->type == QUADD_EVENT_TYPE_RAW ?
			       "raw" : "hw");
			return -EINVAL;
		}
	}

	err = ctx.pmu->set_events(cpuid, pmu_events, nr_pmu, 0);
	if (err < 0) {
		pr_err("PMU set parameters: error\n");
		per_cpu(ctx_pmu_info, cpuid).active = 0;
		return err;
	}
	per_cpu(ctx_pmu_info, cpuid).active = 1;

	return 0;
}

static int verify_app(struct quadd_parameters *p, uid_t task_uid)
{
	int err;
	uid_t uid = 0;

	err = quadd_auth_is_debuggable((char *)p->package_name, &uid);
	if (err < 0) {
		pr_err("error: app either non-debuggable or not found: %s\n",
		       p->package_name);
		return err;
	}

	pr_info("app \"%s\" is debuggable, uid: %u\n",
		p->package_name, (unsigned int)uid);

	if (task_uid != uid) {
		pr_err("error: uids are not matched: %u, %u\n",
		       (unsigned int)task_uid, (unsigned int)uid);
		return -EACCES;
	}

	return 0;
}

static inline bool
is_carmel_events(const struct quadd_event *events, int nr)
{
	int i;

	for (i = 0; i < nr; i++) {
		if (events[i].type == QUADD_EVENT_TYPE_RAW_CARMEL_UNCORE)
			return true;
	}
	return false;
}

static inline bool
is_tegra23x_pmu_scf_events(const struct quadd_event *events, int nr)
{
	int i;

	for (i = 0; i < nr; i++) {
		if (events[i].type == QUADD_EVENT_TYPE_RAW_T23X_UNCORE_SCF)
			return true;
	}
	return false;
}

static inline bool
is_tegra23x_pmu_dsu_events(const struct quadd_event *events, int nr)
{
	int i;

	for (i = 0; i < nr; i++) {
		if (events[i].type == QUADD_EVENT_TYPE_RAW_T23X_UNCORE_DSU)
			return true;
	}
	return false;
}

static int
set_parameters(struct quadd_parameters *p)
{
	int err = 0;
	uid_t task_uid, current_uid;
	struct task_struct *task = NULL;
	u64 *low_addr_p;
	u32 extra, uncore_freq;
#if defined(CONFIG_ARCH_TEGRA_19x_SOC) || defined(CONFIG_ARCH_TEGRA_194_SOC) || \
    defined(CONFIG_ARCH_TEGRA_23x_SOC) || defined(CONFIG_ARCH_TEGRA_234_SOC)
	int nr;
	size_t base_idx = 0;
#endif

	extra = p->reserved[QUADD_PARAM_IDX_EXTRA];

	ctx.mode_is_sampling =
		extra & QUADD_PARAM_EXTRA_SAMPLING ? 1 : 0;
	ctx.mode_is_tracing =
		extra & QUADD_PARAM_EXTRA_TRACING ? 1 : 0;
	ctx.mode_is_sample_all =
		extra & QUADD_PARAM_EXTRA_SAMPLE_ALL_TASKS ? 1 : 0;
	ctx.mode_is_trace_all = p->trace_all_tasks;
	ctx.mode_is_sample_tree =
		extra & QUADD_PARAM_EXTRA_SAMPLE_TREE ? 1 : 0;
	ctx.mode_is_trace_tree =
		extra & QUADD_PARAM_EXTRA_TRACE_TREE ? 1 : 0;

	ctx.mode_is_sampling_timer =
		extra & QUADD_PARAM_EXTRA_SAMPLING_TIMER ? 1 : 0;
	ctx.mode_is_sampling_sched =
		extra & QUADD_PARAM_EXTRA_SAMPLING_SCHED_OUT ? 1 : 0;

	if (!ctx.mode_is_sampling_timer && !ctx.mode_is_sampling_sched)
		ctx.mode_is_sampling = 0;

	if (ctx.mode_is_sample_all)
		ctx.mode_is_sample_tree = 0;
	if (ctx.mode_is_trace_all)
		ctx.mode_is_trace_tree = 0;

	pr_info("flags: s/t/sa/ta/st/tt: %u/%u/%u/%u/%u/%u, st/ss: %u/%u\n",
		ctx.mode_is_sampling,
		ctx.mode_is_tracing,
		ctx.mode_is_sample_all,
		ctx.mode_is_trace_all,
		ctx.mode_is_sample_tree,
		ctx.mode_is_trace_tree,
		ctx.mode_is_sampling_timer,
		ctx.mode_is_sampling_sched);

	if ((ctx.mode_is_trace_all || ctx.mode_is_sample_all) &&
	    !capable(CAP_SYS_ADMIN)) {
		pr_err("error: \"all tasks\" modes are allowed only for root\n");
		return -EACCES;
	}

	if ((ctx.mode_is_trace_all && !ctx.mode_is_tracing) ||
	    (ctx.mode_is_sample_all && !ctx.mode_is_sampling))
		return -EINVAL;

	if (ctx.mode_is_sampling && !validate_freq(p->freq))
		return -EINVAL;

	if (p->power_rate_freq != 0 && !validate_clk_freq(p->power_rate_freq))
		return -EINVAL;
	if (p->ma_freq != 0 && !validate_clk_freq(p->ma_freq))
		return -EINVAL;

	ctx.exclude_user =
		extra & QUADD_PARAM_EXTRA_EXCLUDE_USER ? 1 : 0;
	ctx.exclude_kernel =
		extra & QUADD_PARAM_EXTRA_EXCLUDE_KERNEL ? 1 : 0;
	ctx.exclude_hv =
		extra & QUADD_PARAM_EXTRA_EXCLUDE_HV ? 1 : 0;
	pr_info("exclude user/kernel/hv: %u/%u/%u\n",
		ctx.exclude_user, ctx.exclude_kernel, ctx.exclude_hv);

	uncore_freq = p->reserved[QUADD_PARAM_IDX_UNCORE_FREQ];
	if (uncore_freq != 0 && !validate_freq(uncore_freq))
		return -EINVAL;

	p->package_name[sizeof(p->package_name) - 1] = '\0';
	ctx.param = *p;

	current_uid = from_kuid(&init_user_ns, current_fsuid());
	pr_info("owner uid: %u\n", current_uid);

	if ((ctx.mode_is_tracing && !ctx.mode_is_trace_all) ||
	    (ctx.mode_is_sampling && !ctx.mode_is_sample_all)) {
		/* Currently only first process */
		if (p->nr_pids != 1 || p->pids[0] == 0)
			return -EINVAL;

		rcu_read_lock();
		task = get_pid_task(find_vpid(p->pids[0]), PIDTYPE_PID);
		rcu_read_unlock();
		if (!task) {
			pr_err("error: process not found: %u\n", p->pids[0]);
			return -ESRCH;
		}

		task_uid = from_kuid(&init_user_ns, task_uid(task));
		pr_info("task uid: %u\n", task_uid);

		if (!capable(CAP_SYS_ADMIN)) {
			if (current_uid != task_uid) {
				err = verify_app(p, task_uid);
				if (err < 0)
					goto out_put_task;
			}
			ctx.collect_kernel_ips = 0;
		} else {
			ctx.collect_kernel_ips = 1;
		}
	}

	low_addr_p =
		(u64 *)&p->reserved[QUADD_PARAM_IDX_BT_LOWER_BOUND];
	ctx.hrt->low_addr = (unsigned long)*low_addr_p;

	err = quadd_unwind_start(task);
	if (err)
		goto out_put_task;

#if defined(CONFIG_ARCH_TEGRA_19x_SOC) || defined(CONFIG_ARCH_TEGRA_194_SOC) || \
    defined(CONFIG_ARCH_TEGRA_23x_SOC) || defined(CONFIG_ARCH_TEGRA_234_SOC)
	nr = p->nr_events;

	if (nr > QUADD_MAX_COUNTERS) {
		err = -EINVAL;
		goto out_put_task;
	}

#if defined(CONFIG_ARCH_TEGRA_19x_SOC) || defined(CONFIG_ARCH_TEGRA_194_SOC)
	if (ctx.carmel_pmu && is_carmel_events(p->events, nr)) {
		if (!capable(CAP_SYS_ADMIN)) {
			pr_err("error: Carmel PMU: allowed only for root\n");
			err = -EACCES;
			goto out_put_task;
		}

		if (uncore_freq == 0) {
			err = -EINVAL;
			goto out_put_task;
		}

		err = ctx.carmel_pmu->set_events(-1, p->events, nr, base_idx);
		if (err < 0) {
			pr_err("Carmel Uncore PMU set parameters: error\n");
			ctx.carmel_pmu_info.active = 0;
			goto out_put_task;
		}
		base_idx += err;
		err = 0;
		ctx.carmel_pmu_info.active = 1;
	}
#endif /* CONFIG_ARCH_TEGRA_19x_SOC || defined(CONFIG_ARCH_TEGRA_194_SOC */

#if defined(CONFIG_ARCH_TEGRA_23x_SOC) || defined(CONFIG_ARCH_TEGRA_234_SOC)
	if (ctx.tegra23x_pmu_scf && is_tegra23x_pmu_scf_events(p->events, nr)) {
		if (!capable(CAP_SYS_ADMIN)) {
			pr_err("error: T23X PMU SCF: allowed only for root\n");
			err = -EACCES;
			goto out_put_task;
		}

		if (uncore_freq == 0) {
			err = -EINVAL;
			goto out_put_task;
		}

		err = ctx.tegra23x_pmu_scf->set_events(-1, p->events, nr, base_idx);
		if (err < 0) {
			pr_err("T23X Uncore PMU SCF set parameters: error\n");
			ctx.tegra23x_pmu_scf_info.active = 0;
			goto out_put_task;
		}
		base_idx += err;
		err = 0;
		ctx.tegra23x_pmu_scf_info.active = 1;
	}

#ifdef CONFIG_ARM_DSU_PMU
	if (ctx.tegra23x_pmu_dsu && is_tegra23x_pmu_dsu_events(p->events, nr)) {
		if (!capable(CAP_SYS_ADMIN)) {
			pr_err("error: T23X PMU DSU: allowed only for root\n");
			err = -EACCES;
			goto out_put_task;
		}

		if (uncore_freq == 0) {
			err = -EINVAL;
			goto out_put_task;
		}

		err = ctx.tegra23x_pmu_dsu->set_events(-1, p->events, nr, base_idx);
		if (err < 0) {
			pr_err("T23X Uncore PMU DSU set parameters: error\n");
			ctx.tegra23x_pmu_dsu_info.active = 0;
			goto out_put_task;
		}
		base_idx += err;
		err = 0;
		ctx.tegra23x_pmu_dsu_info.active = 1;
	}
#endif /* CONFIG_ARM_DSU_PMU */
#endif /* CONFIG_ARCH_TEGRA_23x_SOC || defined(CONFIG_ARCH_TEGRA_234_SOC */

#endif
	pr_info("New parameters have been applied\n");

out_put_task:
	if (task)
		put_task_struct(task);

	return err;
}

static void
get_capabilities_for_cpu(int cpuid, struct quadd_comm_cap_for_cpu *cap)
{
	int i, id;
	struct quadd_events_cap *events_cap;
	struct source_info *s = &per_cpu(ctx_pmu_info, cpuid);

	if (!s->is_present)
		return;

	cap->cpuid = cpuid;
	cap->l2_cache = 0;
	cap->l2_multiple_events = 0;

	events_cap = &cap->events_cap;

	events_cap->raw_event_mask = s->raw_event_mask;

	events_cap->cpu_cycles = 0;
	events_cap->l1_dcache_read_misses = 0;
	events_cap->l1_dcache_write_misses = 0;
	events_cap->l1_icache_misses = 0;

	events_cap->instructions = 0;
	events_cap->branch_instructions = 0;
	events_cap->branch_misses = 0;
	events_cap->bus_cycles = 0;

	events_cap->l2_dcache_read_misses = 0;
	events_cap->l2_dcache_write_misses = 0;
	events_cap->l2_icache_misses = 0;

	for (i = 0; i < s->nr_supp_events; i++) {
		struct quadd_event *event = &s->supp_events[i];

		id = event->id;

		if (id == QUADD_EVENT_HW_L2_DCACHE_READ_MISSES ||
		    id == QUADD_EVENT_HW_L2_DCACHE_WRITE_MISSES ||
		    id == QUADD_EVENT_HW_L2_ICACHE_MISSES) {
			cap->l2_cache = 1;
			cap->l2_multiple_events = 1;
		}

		switch (id) {
		case QUADD_EVENT_HW_CPU_CYCLES:
			events_cap->cpu_cycles = 1;
			break;
		case QUADD_EVENT_HW_INSTRUCTIONS:
			events_cap->instructions = 1;
			break;
		case QUADD_EVENT_HW_BRANCH_INSTRUCTIONS:
			events_cap->branch_instructions = 1;
			break;
		case QUADD_EVENT_HW_BRANCH_MISSES:
			events_cap->branch_misses = 1;
			break;
		case QUADD_EVENT_HW_BUS_CYCLES:
			events_cap->bus_cycles = 1;
			break;

		case QUADD_EVENT_HW_L1_DCACHE_READ_MISSES:
			events_cap->l1_dcache_read_misses = 1;
			break;
		case QUADD_EVENT_HW_L1_DCACHE_WRITE_MISSES:
			events_cap->l1_dcache_write_misses = 1;
			break;
		case QUADD_EVENT_HW_L1_ICACHE_MISSES:
			events_cap->l1_icache_misses = 1;
			break;

		case QUADD_EVENT_HW_L2_DCACHE_READ_MISSES:
			events_cap->l2_dcache_read_misses = 1;
			break;
		case QUADD_EVENT_HW_L2_DCACHE_WRITE_MISSES:
			events_cap->l2_dcache_write_misses = 1;
			break;
		case QUADD_EVENT_HW_L2_ICACHE_MISSES:
			events_cap->l2_icache_misses = 1;
			break;

		default:
			pr_err_once("%s: error: invalid event\n",
						__func__);
			return;
		}
	}
}

static u32 get_possible_cpu(void)
{
	int cpu;
	u32 mask = 0;
	struct source_info *s;

	if (ctx.pmu) {
		for_each_possible_cpu(cpu) {
			/* since we don't support more than 32 CPUs */
			if (cpu >= BITS_PER_BYTE * sizeof(mask))
				break;

			s = &per_cpu(ctx_pmu_info, cpu);
			if (s->is_present)
				mask |= (1U << cpu);
		}
	}

	return mask;
}

static void
get_capabilities(struct quadd_comm_cap *cap)
{
	unsigned int extra = 0;
	struct quadd_events_cap *events_cap = &cap->events_cap;

	cap->pmu = ctx.pmu ? 1 : 0;

	cap->l2_cache = 0;

	events_cap->cpu_cycles = 0;
	events_cap->l1_dcache_read_misses = 0;
	events_cap->l1_dcache_write_misses = 0;
	events_cap->l1_icache_misses = 0;

	events_cap->instructions = 0;
	events_cap->branch_instructions = 0;
	events_cap->branch_misses = 0;
	events_cap->bus_cycles = 0;

	events_cap->l2_dcache_read_misses = 0;
	events_cap->l2_dcache_write_misses = 0;
	events_cap->l2_icache_misses = 0;

	cap->tegra_lp_cluster = quadd_is_cpu_with_lp_cluster();
	cap->power_rate = 1;
	cap->blocked_read = 1;

	extra |= QUADD_COMM_CAP_EXTRA_BT_KERNEL_CTX;
	extra |= QUADD_COMM_CAP_EXTRA_GET_MMAP;
	extra |= QUADD_COMM_CAP_EXTRA_GROUP_SAMPLES;
	extra |= QUADD_COMM_CAP_EXTRA_BT_UNWIND_TABLES;
	extra |= QUADD_COMM_CAP_EXTRA_SUPPORT_AARCH64;
	extra |= QUADD_COMM_CAP_EXTRA_SPECIAL_ARCH_MMAP;
	extra |= QUADD_COMM_CAP_EXTRA_UNWIND_MIXED;
	extra |= QUADD_COMM_CAP_EXTRA_UNW_ENTRY_TYPE;
	extra |= QUADD_COMM_CAP_EXTRA_RB_MMAP_OP;
	extra |= QUADD_COMM_CAP_EXTRA_CPU_MASK;

	if (ctx.hrt->tc) {
		extra |= QUADD_COMM_CAP_EXTRA_ARCH_TIMER;
		if (ctx.hrt->arch_timer_user_access)
			extra |= QUADD_COMM_CAP_EXTRA_ARCH_TIMER_USR;
	}

	if (ctx.pclk_cpufreq)
		extra |= QUADD_COMM_CAP_EXTRA_CPUFREQ;

	cap->reserved[QUADD_COMM_CAP_IDX_EXTRA] = extra;
	cap->reserved[QUADD_COMM_CAP_IDX_CPU_MASK] = get_possible_cpu();
}

void quadd_get_state(struct quadd_module_state *state)
{
	unsigned int status = 0;

	quadd_hrt_get_state(state);

	if (ctx.comm->is_active())
		status |= QUADD_MOD_STATE_STATUS_IS_ACTIVE;

	if (quadd_auth_is_auth_open())
		status |= QUADD_MOD_STATE_STATUS_IS_AUTH_OPEN;

	state->reserved[QUADD_MOD_STATE_IDX_STATUS] = status;
}

static int
set_extab(struct quadd_sections *extabs,
	  struct quadd_mmap_area *mmap)
{
	return quadd_unwind_set_extab(extabs, mmap);
}

static void
delete_mmap(struct quadd_mmap_area *mmap)
{
	quadd_unwind_clean_mmap(mmap);
}

static int
is_cpu_present(int cpuid)
{
	struct source_info *s = &per_cpu(ctx_pmu_info, cpuid);

	return s->is_present;
}

static struct quadd_comm_control_interface control = {
	.start			= start,
	.stop			= stop,
	.set_parameters		= set_parameters,
	.set_parameters_for_cpu = set_parameters_for_cpu,
	.get_capabilities	= get_capabilities,
	.get_capabilities_for_cpu = get_capabilities_for_cpu,
	.get_state		= quadd_get_state,
	.set_extab		= set_extab,
	.delete_mmap		= delete_mmap,
	.is_cpu_present		= is_cpu_present,
};

static inline
struct quadd_event_source *pmu_init(void)
{
#ifdef CONFIG_ARM64
	return quadd_armv8_pmu_init(&ctx);
#else
	return quadd_armv7_pmu_init();
#endif
}

static inline void pmu_deinit(void)
{
#ifdef CONFIG_ARM64
	quadd_armv8_pmu_deinit();
#else
	quadd_armv7_pmu_deinit();
#endif
}

int quadd_late_init(void)
{
	int nr_events, nr_ctrs, err;
	unsigned int raw_event_mask;
	struct quadd_event *events;
	struct source_info *pmu_info;
	struct quadd_event_source *src;
	int cpuid;

	if (unlikely(!ctx.early_initialized))
		return -ENODEV;

	if (likely(ctx.initialized))
		return 0;

	src = pmu_init();
	ctx.pmu = IS_ERR(src) ? NULL : src;

	if (ctx.pmu) {
		for_each_possible_cpu(cpuid) {
			const struct quadd_arch_info *arch;

			arch = ctx.pmu->get_arch(cpuid);
			if (!arch)
				continue;

			pmu_info = &per_cpu(ctx_pmu_info, cpuid);
			pmu_info->is_present = 1;

			events = pmu_info->supp_events;
			nr_events =
				ctx.pmu->supported_events(cpuid, events,
							  QUADD_MAX_COUNTERS,
							  &raw_event_mask, &nr_ctrs);

			pmu_info->nr_supp_events = nr_events;
			pmu_info->raw_event_mask = raw_event_mask;
			pmu_info->nr_ctrs = nr_ctrs;
		}
	}

#if defined(CONFIG_ARCH_TEGRA_19x_SOC) || defined(CONFIG_ARCH_TEGRA_194_SOC)
	ctx.carmel_pmu = quadd_carmel_uncore_pmu_init();
	if (IS_ERR(ctx.carmel_pmu)) {
		pr_err("Carmel Uncore PMU init failed\n");
		err = PTR_ERR(ctx.carmel_pmu);
		goto out_err_pmu;
	}

	if (ctx.carmel_pmu) {
		pmu_info = &ctx.carmel_pmu_info;
		events = pmu_info->supp_events;

		nr_events =
			ctx.carmel_pmu->supported_events(0, events,
							 QUADD_MAX_COUNTERS,
							 &raw_event_mask, &nr_ctrs);

		pmu_info->is_present = 1;
		pmu_info->nr_supp_events = nr_events;
		pmu_info->raw_event_mask = raw_event_mask;
		pmu_info->nr_ctrs = nr_ctrs;
	}
#endif

#if defined(CONFIG_ARCH_TEGRA_23x_SOC) || defined(CONFIG_ARCH_TEGRA_234_SOC)
	ctx.tegra23x_pmu_scf = quadd_tegra23x_pmu_scf_init();
	if (IS_ERR(ctx.tegra23x_pmu_scf)) {
		pr_err("T23X Uncore PMU SCF init failed\n");
		err = PTR_ERR(ctx.tegra23x_pmu_scf);
		goto out_err_pmu;
	}

	if (ctx.tegra23x_pmu_scf) {
		pmu_info = &ctx.tegra23x_pmu_scf_info;
		events = pmu_info->supp_events;

		nr_events =
			ctx.tegra23x_pmu_scf->supported_events(0, events,
							QUADD_MAX_COUNTERS,
							&raw_event_mask, &nr_ctrs);

		pmu_info->is_present = 1;
		pmu_info->nr_supp_events = nr_events;
		pmu_info->raw_event_mask = raw_event_mask;
		pmu_info->nr_ctrs = nr_ctrs;
	}

#ifdef CONFIG_ARM_DSU_PMU
	ctx.tegra23x_pmu_dsu = quadd_tegra23x_pmu_dsu_init();
	if (IS_ERR(ctx.tegra23x_pmu_dsu)) {
		pr_err("T23X Uncore PMU DSU init failed\n");
		err = PTR_ERR(ctx.tegra23x_pmu_dsu);
		goto out_err_uncore_pmu_scf;
	}

	if (ctx.tegra23x_pmu_dsu) {
		pmu_info = &ctx.tegra23x_pmu_dsu_info;
		events = pmu_info->supp_events;

		nr_events =
			ctx.tegra23x_pmu_dsu->supported_events(0, events,
							QUADD_MAX_COUNTERS,
							&raw_event_mask, &nr_ctrs);

		pmu_info->is_present = 1;
		pmu_info->nr_supp_events = nr_events;
		pmu_info->raw_event_mask = raw_event_mask;
		pmu_info->nr_ctrs = nr_ctrs;
	}
#endif /* CONFIG_ARM_DSU_PMU */
#endif /* CONFIG_ARCH_TEGRA_23x_SOC || CONFIG_ARCH_TEGRA_234_SOC */

	ctx.hrt = quadd_hrt_init(&ctx);
	if (IS_ERR(ctx.hrt)) {
		pr_err("error: HRT init failed\n");
		err = PTR_ERR(ctx.hrt);
		goto out_err_uncore_pmu;
	}

	err = quadd_uncore_init(&ctx);
	if (err < 0) {
		pr_err("error: uncore events init failed\n");
		goto out_err_hrt;
	}

	err = quadd_power_clk_init(&ctx);
	if (err < 0) {
		pr_err("error: POWER CLK init failed\n");
		goto out_err_uncore;
	}

	err = quadd_unwind_init(&ctx);
	if (err < 0) {
		pr_err("error: EH unwinding init failed\n");
		goto out_err_power_clk;
	}

	get_capabilities(&ctx.cap);

	for_each_possible_cpu(cpuid)
		get_capabilities_for_cpu(cpuid, &per_cpu(per_cpu_caps, cpuid));

	ctx.initialized = 1;

	return 0;

out_err_power_clk:
	quadd_power_clk_deinit();
out_err_uncore:
	quadd_uncore_deinit();
out_err_hrt:
	quadd_hrt_deinit();

out_err_uncore_pmu:
#if defined(CONFIG_ARCH_TEGRA_19x_SOC) || defined(CONFIG_ARCH_TEGRA_194_SOC)
	quadd_carmel_uncore_pmu_deinit();
#endif
#if defined(CONFIG_ARCH_TEGRA_23x_SOC) || defined(CONFIG_ARCH_TEGRA_234_SOC)
#ifdef CONFIG_ARM_DSU_PMU
	quadd_tegra23x_pmu_dsu_deinit();
out_err_uncore_pmu_scf:
#endif /* CONFIG_ARM_DSU_PMU */
	quadd_tegra23x_pmu_scf_deinit();
#endif /* CONFIG_ARCH_TEGRA_23x_SOC || CONFIG_ARCH_TEGRA_234_SOC */

out_err_pmu:
	pmu_deinit();

	return err;
}

static int __init quadd_early_init(void)
{
	int cpuid, err;

	pr_info("version: %s, samples/io: %d/%d\n",
		QUADD_MODULE_VERSION,
		QUADD_SAMPLES_VERSION,
		QUADD_IO_VERSION);

	atomic_set(&ctx.started, 0);

	ctx.early_initialized = 0;
	ctx.initialized = 0;

#ifndef MODULE
	atomic_set(&ctx.tegra_profiler_lock, 0);
#endif

	ctx.get_capabilities_for_cpu = get_capabilities_for_cpu_int;
	ctx.get_pmu_info = get_pmu_info_for_current_cpu;

	ctx.pmu = NULL;
	for_each_possible_cpu(cpuid) {
		struct source_info *pmu_info = &per_cpu(ctx_pmu_info, cpuid);

		pmu_info->active = 0;
		pmu_info->is_present = 0;
	}

	ctx.carmel_pmu = NULL;
	ctx.carmel_pmu_info.active = 0;

	ctx.comm = quadd_comm_init(&ctx, &control);
	if (IS_ERR(ctx.comm)) {
		err = PTR_ERR(ctx.comm);
		goto out_err;
	}

	err = quadd_auth_init(&ctx);
	if (err < 0)
		goto out_err_comm;

	quadd_proc_init(&ctx);
	ctx.early_initialized = 1;

	return 0;

out_err_comm:
	quadd_comm_exit();

out_err:
	return err;
}

static void deinit(void)
{
	if (ctx.initialized) {
		quadd_unwind_deinit();
		quadd_power_clk_deinit();
		quadd_uncore_deinit();
		quadd_hrt_deinit();
#if defined(CONFIG_ARCH_TEGRA_19x_SOC) || defined(CONFIG_ARCH_TEGRA_194_SOC)
		quadd_carmel_uncore_pmu_deinit();
#endif
		pmu_deinit();

		ctx.initialized = 0;
	}

	if (ctx.early_initialized) {
		quadd_proc_deinit();
		quadd_auth_deinit();
		quadd_comm_exit();

		ctx.early_initialized = 0;
	}
}

static int __init quadd_module_init(void)
{
	return quadd_early_init();
}

static void __exit quadd_module_exit(void)
{
	deinit();
}

module_init(quadd_module_init);
module_exit(quadd_module_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR("Nvidia Ltd");
MODULE_DESCRIPTION("Tegra profiler");
