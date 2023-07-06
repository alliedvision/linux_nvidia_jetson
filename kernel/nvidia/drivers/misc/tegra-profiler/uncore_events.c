/*
 * drivers/misc/tegra-profiler/uncore_events.c
 *
 * Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>

#include <linux/tegra_profiler.h>

#include "uncore_events.h"
#include "hrt.h"
#include "comm.h"
#include "tegra.h"
#include "quadd.h"

enum {
	QUADD_UNCORE_STATE_ACTIVE = 0,
	QUADD_UNCORE_STATE_STOPPING,
	QUADD_UNCORE_STATE_INACTIVE,
};

#define TEGRA23X_UNIT(__id)	(__id & 0xf)

struct uncore_cpu_context {
	struct hrtimer hrtimer;
	bool is_uncore_cpu;

	atomic_t ref_count;
	raw_spinlock_t state_lock;

	struct quadd_event_data events[QUADD_MAX_COUNTERS];
};

struct uncore_ctx {
	struct quadd_event_source *carmel_pmu;
	struct source_info *carmel_info;

	struct quadd_event_source *tegra23x_pmu_scf;
	struct source_info *tegra23x_pmu_scf_info;

	struct quadd_event_source *tegra23x_pmu_dsu;
	struct source_info *tegra23x_pmu_dsu_info;

	u64 sample_period;
	atomic_t state;

	cpumask_t on_cpus;
	int uncore_cpu;

	struct uncore_cpu_context __percpu *cpu_ctx;
	struct quadd_ctx *quadd_ctx;
};

static struct uncore_ctx ctx = {
	.state = ATOMIC_INIT(QUADD_UNCORE_STATE_INACTIVE),
};

static inline bool is_source_active(struct source_info *si)
{
	return si->active != 0;
}

static inline bool is_uncore_active(void)
{
	bool is_carmel_pmu =
		ctx.carmel_pmu && is_source_active(ctx.carmel_info);
	bool is_tegra23x_pmu_scf =
		ctx.tegra23x_pmu_scf && is_source_active(ctx.tegra23x_pmu_scf_info);
	bool is_tegra23x_pmu_dsu =
		ctx.tegra23x_pmu_dsu && is_source_active(ctx.tegra23x_pmu_dsu_info);

	return is_carmel_pmu || is_tegra23x_pmu_scf || is_tegra23x_pmu_dsu;
}

static void
put_sample(const struct quadd_event_data *events, int nr_events, u64 ts)
{
	unsigned int flags;
	int i, nr_positive = 0, vec_idx = 0;
	struct quadd_iovec vec[3];
	u32 extra_data = 0, ts_delta = 0, events_extra[QUADD_MAX_COUNTERS];
	struct quadd_record_data record;
	struct quadd_sample_data *s = &record.sample;

	record.record_type = QUADD_RECORD_TYPE_SAMPLE;

	s->time = ts;
	s->flags = 0;
	s->flags |= QUADD_SAMPLE_FLAG_UNCORE;

	s->cpu_id = quadd_get_processor_id(NULL, &flags);
	s->pid = s->tgid = U32_MAX;

	s->ip = 0;
	s->callchain_nr = 0;

	s->events_flags = 0;
	for (i = 0; i < nr_events; i++) {
		u32 value = (u32)events[i].delta;

		if (value > 0) {
			s->events_flags |= 1U << events[i].out_idx;
			events_extra[nr_positive++] = value;
		}
	}

	if (nr_positive > 0) {
		vec[vec_idx].base = &extra_data;
		vec[vec_idx].len = sizeof(extra_data);
		vec_idx++;

		vec[vec_idx].base = events_extra;
		vec[vec_idx].len = nr_positive * sizeof(events_extra[0]);
		vec_idx++;

		vec[vec_idx].base = &ts_delta;
		vec[vec_idx].len = sizeof(ts_delta);
		vec_idx++;

		quadd_put_sample(&record, vec, vec_idx);
	}
}

static inline bool get_uncore_sources(struct uncore_cpu_context *cpu_ctx)
{
	bool res = true;

	raw_spin_lock(&cpu_ctx->state_lock);
	if (unlikely(atomic_read(&ctx.state) != QUADD_UNCORE_STATE_ACTIVE)) {
		res = false;
		goto out;
	}
	atomic_inc(&cpu_ctx->ref_count);
out:
	raw_spin_unlock(&cpu_ctx->state_lock);
	return res;
}

static inline void put_uncore_sources(struct uncore_cpu_context *cpu_ctx)
{
	atomic_dec(&cpu_ctx->ref_count);
}

static void wait_for_close(struct uncore_cpu_context *cpu_ctx)
{
	raw_spin_lock(&cpu_ctx->state_lock);
	atomic_set(&ctx.state, QUADD_UNCORE_STATE_STOPPING);
	raw_spin_unlock(&cpu_ctx->state_lock);

	while (atomic_read(&cpu_ctx->ref_count) > 0)
		cpu_relax();
}

static void read_uncore_sources(u64 ts)
{
	bool is_carmel_pmu, is_tegra23x_pmu_scf, is_tegra23x_pmu_dsu;
	struct quadd_event_data *events, *e, *e_end;
	struct uncore_cpu_context *cpu_ctx = this_cpu_ptr(ctx.cpu_ctx);

	is_carmel_pmu = ctx.carmel_pmu && is_source_active(ctx.carmel_info);
	is_tegra23x_pmu_scf = ctx.tegra23x_pmu_scf && is_source_active(ctx.tegra23x_pmu_scf_info);
	is_tegra23x_pmu_dsu = ctx.tegra23x_pmu_dsu && is_source_active(ctx.tegra23x_pmu_dsu_info);

	if (!is_carmel_pmu && !is_tegra23x_pmu_scf && !is_tegra23x_pmu_dsu)
		return;

	e = events = cpu_ctx->events;
	e_end = events + ARRAY_SIZE(cpu_ctx->events);

	if (!get_uncore_sources(cpu_ctx))
		return;

	if (cpu_ctx->is_uncore_cpu) {
		if (is_carmel_pmu)
			e += ctx.carmel_pmu->read(e, e_end - e);

		if (is_tegra23x_pmu_scf)
			e += ctx.tegra23x_pmu_scf->read(e, e_end - e);
	}
	if (is_tegra23x_pmu_dsu)
		e += ctx.tegra23x_pmu_dsu->read(e, e_end - e);

	put_uncore_sources(cpu_ctx);
	put_sample(events, e - events, ts);
}

static enum hrtimer_restart hrtimer_handler(struct hrtimer *hrtimer)
{
	u64 ts = quadd_get_time();

	if (unlikely(atomic_read(&ctx.state) != QUADD_UNCORE_STATE_ACTIVE))
		return HRTIMER_NORESTART;

	read_uncore_sources(ts);
	hrtimer_forward_now(hrtimer, ns_to_ktime(ctx.sample_period));

	return HRTIMER_RESTART;
}

static void start_hrtimer(struct hrtimer *hrtimer)
{
#if ((defined(CONFIG_PREEMPT_RT) || defined(CONFIG_PREEMPT_RT_FULL)) && \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)))
	hrtimer_start(hrtimer, ns_to_ktime(ctx.sample_period),
		      HRTIMER_MODE_REL_PINNED_HARD);
#else
	hrtimer_start(hrtimer, ns_to_ktime(ctx.sample_period),
		      HRTIMER_MODE_REL_PINNED);
#endif
}

static void cancel_hrtimer(struct hrtimer *timer)
{
	hrtimer_cancel(timer);
}

static void init_hrtimer(struct hrtimer *timer)
{
#if ((defined(CONFIG_PREEMPT_RT) || defined(CONFIG_PREEMPT_RT_FULL)) && \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)))
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_HARD);
#else
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
#endif
	timer->function = hrtimer_handler;
}

static void get_cluster_online_cpus(int cluster_id, cpumask_t *cpumask)
{
	int cpu;

	cpumask_clear(cpumask);

	for_each_online_cpu(cpu) {
		if (topology_physical_package_id(cpu) == cluster_id)
			cpumask_set_cpu(cpu, cpumask);
	}
}

static void dsu_enable_on_cpu(void)
{
	struct quadd_event_source *source = ctx.tegra23x_pmu_dsu;

	source->enable();
	source->start();
}

static void dsu_disable_on_cpu(void *arg)
{
	struct quadd_event_source *source = ctx.tegra23x_pmu_dsu;

	source->stop();
	source->disable();
}

static void start_on_cpu(void *is_dsu)
{
	struct uncore_cpu_context *cpu_ctx = this_cpu_ptr(ctx.cpu_ctx);

	if (is_dsu)
		dsu_enable_on_cpu();

	start_hrtimer(&cpu_ctx->hrtimer);
}

int quadd_uncore_start(void)
{
	long freq;
	void *arg;
	bool is_dsu;
	cpumask_t cpumask;
	int cpu, uncore_cpu, nr_events, i, cluster_id;
	struct quadd_event events[QUADD_MAX_COUNTERS];
	struct quadd_event_source *source;
	struct uncore_cpu_context *cpu_ctx;
	struct quadd_parameters *p = &ctx.quadd_ctx->param;

	if (atomic_read(&ctx.state) == QUADD_UNCORE_STATE_ACTIVE)
		return 0;

	if (!is_uncore_active())
		return 0;

	freq = p->reserved[QUADD_PARAM_IDX_UNCORE_FREQ];
	if (freq == 0)
		return 0;

	ctx.sample_period = NSEC_PER_SEC / freq;

	atomic_set(&ctx.state, QUADD_UNCORE_STATE_ACTIVE);
	cpumask_clear(&ctx.on_cpus);

	source = ctx.carmel_pmu;
	if (source && is_source_active(ctx.carmel_info)) {
		source->enable();
		source->start();
	}

	source = ctx.tegra23x_pmu_scf;
	if (source && is_source_active(ctx.tegra23x_pmu_scf_info)) {
		source->enable();
		source->start();
	}

	source = ctx.tegra23x_pmu_dsu;
	is_dsu = source && is_source_active(ctx.tegra23x_pmu_dsu_info);

	cpu = get_cpu();

	if (is_dsu) {
		nr_events = source->current_events(0, events, QUADD_MAX_COUNTERS);
		for (i = 0; i < nr_events; i++) {
			cluster_id = TEGRA23X_UNIT(events[i].id);
			get_cluster_online_cpus(cluster_id, &cpumask);
			cpu = cpumask_first(&cpumask);
			if (cpu < nr_cpu_ids)
				cpumask_set_cpu(cpu, &ctx.on_cpus);
		}
		uncore_cpu = cpumask_first(&ctx.on_cpus);
	} else {
		uncore_cpu = cpumask_first(cpu_online_mask);
		cpumask_set_cpu(uncore_cpu, &ctx.on_cpus);
	}

	cpu_ctx = per_cpu_ptr(ctx.cpu_ctx, uncore_cpu);
	cpu_ctx->is_uncore_cpu = true;

	ctx.uncore_cpu = uncore_cpu;

	arg = is_dsu ? (void *)1 : NULL;
	on_each_cpu_mask(&ctx.on_cpus, start_on_cpu, arg, true);

	put_cpu();

	return 0;
}

void quadd_uncore_stop(void)
{
	int cpu_id;
	struct uncore_cpu_context *cpu_ctx;

	if (atomic_read(&ctx.state) != QUADD_UNCORE_STATE_ACTIVE)
		return;

	for_each_possible_cpu(cpu_id) {
		cpu_ctx = per_cpu_ptr(ctx.cpu_ctx, cpu_id);
		cancel_hrtimer(&cpu_ctx->hrtimer);
		cpu_ctx->is_uncore_cpu = false;
		wait_for_close(cpu_ctx);
	}

	if (ctx.carmel_pmu && is_source_active(ctx.carmel_info)) {
		ctx.carmel_pmu->stop();
		ctx.carmel_pmu->disable();
	}
	if (ctx.tegra23x_pmu_scf && is_source_active(ctx.tegra23x_pmu_scf_info)) {
		ctx.tegra23x_pmu_scf->stop();
		ctx.tegra23x_pmu_scf->disable();
	}

	if (ctx.tegra23x_pmu_dsu && is_source_active(ctx.tegra23x_pmu_dsu_info))
		on_each_cpu_mask(&ctx.on_cpus, dsu_disable_on_cpu, NULL, true);

	atomic_set(&ctx.state, QUADD_UNCORE_STATE_INACTIVE);
}

int quadd_uncore_init(struct quadd_ctx *quadd_ctx)
{
	int cpu_id;
	struct uncore_cpu_context *cpu_ctx;

	ctx.quadd_ctx = quadd_ctx;

	ctx.carmel_pmu = quadd_ctx->carmel_pmu;
	ctx.carmel_info = &quadd_ctx->carmel_pmu_info;

	ctx.tegra23x_pmu_scf = quadd_ctx->tegra23x_pmu_scf;
	ctx.tegra23x_pmu_scf_info = &quadd_ctx->tegra23x_pmu_scf_info;

	ctx.tegra23x_pmu_dsu = quadd_ctx->tegra23x_pmu_dsu;
	ctx.tegra23x_pmu_dsu_info = &quadd_ctx->tegra23x_pmu_dsu_info;

	atomic_set(&ctx.state, QUADD_UNCORE_STATE_INACTIVE);

	ctx.cpu_ctx = alloc_percpu(struct uncore_cpu_context);
	if (!ctx.cpu_ctx)
		return -ENOMEM;

	for_each_possible_cpu(cpu_id) {
		cpu_ctx = per_cpu_ptr(ctx.cpu_ctx, cpu_id);
		init_hrtimer(&cpu_ctx->hrtimer);
		cpu_ctx->is_uncore_cpu = false;

		atomic_set(&cpu_ctx->ref_count, 0);
		raw_spin_lock_init(&cpu_ctx->state_lock);
	}

	return 0;
}

void quadd_uncore_deinit(void)
{
	free_percpu(ctx.cpu_ctx);
}
