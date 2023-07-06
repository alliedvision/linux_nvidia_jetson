/*
 * drivers/misc/tegra-profiler/tegra23x_pmu_dsu.c
 *
 * Copyright (c) 2022-2023, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/list.h>
#include <linux/bitmap.h>
#include <linux/errno.h>
#include <linux/topology.h>
#include <linux/of.h>
#include <linux/version.h>

#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include <asm/sysreg.h>
#include <asm/arm_dsu_pmu.h>

#include "tegra23x_pmu_dsu.h"
#include "quadd.h"


#define CPU_CYCLES		0x11
#define BUS_ACCESS		0x19
#define MEMORY_ERROR		0x1A
#define BUS_CYCLES		0x1D

#define L3D_CACHE_ALLOCATE	0x29
#define L3D_CACHE_REFILL	0x2A
#define L3D_CACHE		0x2B
#define L3D_CACHE_WB		0x2C

#define DSU_IDCODE_DSU_AE	0x42

#define DSU_MAX_EVENTS		64

#define CLUSTERPMCR_E			BIT(0)
#define CLUSTERPMCR_P			BIT(1)
#define CLUSTERPMCR_C			BIT(2)
#define CLUSTERPMCR_N_SHIFT		11
#define CLUSTERPMCR_N_MASK		0x1f
#define CLUSTERPMCR_IDCODE_SHIFT	16
#define CLUSTERPMCR_IDCODE_MASK		0xff
#define CLUSTERPMCR_IMP_SHIFT		24
#define CLUSTERPMCR_IMP_MASK		0xff

#define DSU_PMU_IDX_CYCLE_CNTR		31
#define DSU_MAX_CLUSTER_CNTRS		(DSU_PMU_IDX_CYCLE_CNTR + 1)

#define DSU_MAX_CLUSTERS		16

#define TEGRA23X_UNIT(__id)	(__id & 0xf)
#define TEGRA23X_EVENT(__id)	(__id >> 4)

struct cntr_info {
	u32 prev_val;
	u32 id_raw;
	u32 id_hw;
	size_t out_idx;
};

struct dsu_unit {
	unsigned int id;
	cpumask_t associated_cpus;

	bool is_used;
	bool is_available;

	unsigned long nr_cntrs;

	struct cntr_info cntrs[DSU_MAX_CLUSTER_CNTRS];
	DECLARE_BITMAP(used_cntrs, DSU_MAX_CLUSTER_CNTRS);

	DECLARE_BITMAP(pmceid_bitmap, DSU_MAX_EVENTS);
};

struct dsu_cpu_context {
	struct dsu_unit *unit;
};

static struct tegra23x_pmu_dsu_ctx {
	struct dsu_unit units[DSU_MAX_CLUSTERS];
	DECLARE_BITMAP(used_units, DSU_MAX_CLUSTERS);

	struct dsu_cpu_context __percpu *cpu_ctx;
} ctx;

static int tegra23x_pmu_dsu_enable(void)
{
	u32 pmcr;
	struct dsu_unit *unit;
	struct dsu_cpu_context *cpu_ctx = this_cpu_ptr(ctx.cpu_ctx);

	unit = cpu_ctx->unit;
	if (!unit->is_used)
		return 0;

	pmcr = __dsu_pmu_read_pmcr();
	pmcr |= CLUSTERPMCR_E | CLUSTERPMCR_P;
	__dsu_pmu_write_pmcr(pmcr);

	return 0;
}

static void tegra23x_pmu_dsu_disable(void)
{
	u32 pmcr;
	struct dsu_unit *unit;
	struct dsu_cpu_context *cpu_ctx = this_cpu_ptr(ctx.cpu_ctx);

	unit = cpu_ctx->unit;
	if (!unit->is_used)
		return;

	pmcr = __dsu_pmu_read_pmcr();
	pmcr &= ~((u32)CLUSTERPMCR_E);
	__dsu_pmu_write_pmcr(pmcr);

	memset(unit->cntrs, 0, sizeof(unit->cntrs));
	bitmap_zero(unit->used_cntrs, DSU_MAX_CLUSTER_CNTRS);
	unit->is_used = false;
}

static void tegra23x_pmu_dsu_start(void)
{
	struct dsu_unit *unit;
	struct cntr_info *cntr;
	unsigned long idx = 0, nr_cntrs;
	struct dsu_cpu_context *cpu_ctx = this_cpu_ptr(ctx.cpu_ctx);

	unit = cpu_ctx->unit;
	if (!unit->is_used)
		return;

	nr_cntrs = unit->nr_cntrs;

	while (idx < nr_cntrs) {
		idx = find_next_bit(unit->used_cntrs, nr_cntrs, idx);
		if (idx != nr_cntrs) {
			cntr = &unit->cntrs[idx];
			cntr->prev_val = 0;

			if (idx != DSU_PMU_IDX_CYCLE_CNTR)
				__dsu_pmu_set_event(idx, cntr->id_hw);

			__dsu_pmu_enable_counter(idx);
		}
		idx++;
	}
}

static void tegra23x_pmu_dsu_stop(void)
{
	struct dsu_unit *unit;
	unsigned long idx = 0, nr_cntrs;
	struct dsu_cpu_context *cpu_ctx = this_cpu_ptr(ctx.cpu_ctx);

	unit = cpu_ctx->unit;
	if (!unit->is_used)
		return;

	nr_cntrs = unit->nr_cntrs;

	while (idx < nr_cntrs) {
		idx = find_next_bit(unit->used_cntrs, nr_cntrs, idx);
		if (idx != nr_cntrs)
			__dsu_pmu_disable_counter(idx);
		idx++;
	}
}

static int
tegra23x_pmu_dsu_read(struct quadd_event_data *events, int max)
{
	struct cntr_info *cntr;
	struct dsu_unit *unit;
	u64 val, prev_val, delta, max_count;
	struct quadd_event_data *curr, *end;
	unsigned long idx = 0, nr_cntrs;
	struct dsu_cpu_context *cpu_ctx = this_cpu_ptr(ctx.cpu_ctx);

	unit = cpu_ctx->unit;
	if (!unit->is_used)
		return 0;

	nr_cntrs = unit->nr_cntrs;

	curr = events;
	end = events + max;

	while (idx < nr_cntrs) {
		idx = find_next_bit(unit->used_cntrs, nr_cntrs, idx);
		if (idx != nr_cntrs) {
			cntr = &unit->cntrs[idx];

			val = __dsu_pmu_read_counter(idx);

			curr->event_source =
				QUADD_EVENT_SOURCE_T23X_UNCORE_PMU_DSU;

			max_count = (idx == DSU_PMU_IDX_CYCLE_CNTR ?
					U64_MAX : U32_MAX);
			curr->max_count = max_count;

			curr->event.type = QUADD_EVENT_TYPE_RAW_T23X_UNCORE_DSU;
			curr->event.id = cntr->id_raw;

			curr->out_idx = cntr->out_idx;

			prev_val = cntr->prev_val;

			if (prev_val <= val)
				delta = val - prev_val;
			else
				delta = max_count - prev_val + val;

			curr->val = val;
			curr->prev_val = prev_val;
			curr->delta = delta;

			cntr->prev_val = val;
			curr++;
		}
		idx++;
	}

	return curr - events;
}

static void clean_units(void)
{
	int i;
	struct dsu_unit *unit;

	bitmap_zero(ctx.used_units, DSU_MAX_CLUSTERS);

	for (i = 0; i < DSU_MAX_CLUSTERS; i++) {
		unit = &ctx.units[i];

		if (unit->is_available) {
			memset(unit->cntrs, 0, sizeof(unit->cntrs));
			bitmap_zero(unit->used_cntrs, DSU_MAX_CLUSTER_CNTRS);
			unit->is_used = false;
		}
	}
}

static int add_event(const struct quadd_event *event)
{
	struct dsu_unit *unit;
	struct cntr_info *cntr;
	unsigned long idx, nr_cntrs;
	u32 unit_id, event_raw, event_hw;

	event_raw = event->id;

	unit_id = TEGRA23X_UNIT(event_raw);
	event_hw = TEGRA23X_EVENT(event_raw);

	if (unit_id >= DSU_MAX_CLUSTERS)
		return -EINVAL;

	unit = &ctx.units[unit_id];
	if (!unit->is_available)
		return -ENOENT;

	nr_cntrs = unit->nr_cntrs;

	idx = find_first_zero_bit(unit->used_cntrs, nr_cntrs);
	if (idx >= nr_cntrs)
		return -ENOSPC;

	cntr = &unit->cntrs[idx];
	cntr->id_raw = event_raw;
	cntr->id_hw = event_hw;

	set_bit(idx, unit->used_cntrs);
	set_bit(unit_id, ctx.used_units);

	unit->is_used = true;

	return 0;
}

static void fill_output_indexes(size_t base_idx)
{
	unsigned long cntr_id, unit_id;
	size_t out_idx = base_idx;
	struct dsu_unit *unit;

	unit_id = 0;
	while (true) {
		unit_id = find_next_bit(ctx.used_units,
					DSU_MAX_CLUSTERS, unit_id);
		if (unit_id >= DSU_MAX_CLUSTERS)
			break;
		unit = &ctx.units[unit_id];

		cntr_id = 0;
		while (true) {
			cntr_id = find_next_bit(unit->used_cntrs,
					DSU_MAX_CLUSTER_CNTRS, cntr_id);
			if (cntr_id >= DSU_MAX_CLUSTER_CNTRS)
				break;

			unit->cntrs[cntr_id++].out_idx = out_idx++;
		}
		unit_id++;
	}
}

static int
tegra23x_pmu_dsu_set_events(int cpuid, const struct quadd_event *events,
			    int size, size_t base_idx)
{
	int i, err, nr_events = 0;

	clean_units();

	for (i = 0; i < size; i++) {
		const struct quadd_event *event = &events[i];

		if (event->type == QUADD_EVENT_TYPE_RAW_T23X_UNCORE_DSU) {
			err = add_event(event);
			if (err < 0) {
				clean_units();
				return err;
			}
			nr_events++;
		}
	}
	fill_output_indexes(base_idx);

	return nr_events;
}

static int
supported_events(int cpuid, struct quadd_event *events,
		 int max, unsigned int *raw_event_mask, int *nr_cntrs)
{
	struct dsu_unit *unit = &ctx.units[0];

	*raw_event_mask = 0x0fff;
	*nr_cntrs = unit->nr_cntrs;

	return 0;
}

static int
current_events(int cpuid, struct quadd_event *events, int max)
{
	unsigned long cntr_id, unit_id;
	struct dsu_unit *unit;
	struct quadd_event *curr, *end;

	curr = events;
	end = curr + max;

	unit_id = 0;
	while (curr < end) {
		unit_id = find_next_bit(ctx.used_units,
					DSU_MAX_CLUSTERS, unit_id);
		if (unit_id >= DSU_MAX_CLUSTERS)
			break;
		unit = &ctx.units[unit_id];

		cntr_id = 0;
		while (curr < end) {
			cntr_id = find_next_bit(unit->used_cntrs,
					DSU_MAX_CLUSTER_CNTRS, cntr_id);
			if (cntr_id >= DSU_MAX_CLUSTER_CNTRS)
				break;

			curr->type = QUADD_EVENT_TYPE_RAW_T23X_UNCORE_DSU;
			curr->id = unit->cntrs[cntr_id++].id_raw;
			curr++;
		}
		unit_id++;
	}

	return curr - events;
}

QUADD_PMU_CNTR_INFO(cpu_cycles, CPU_CYCLES);
QUADD_PMU_CNTR_INFO(bus_access, BUS_ACCESS);
QUADD_PMU_CNTR_INFO(memory_error, MEMORY_ERROR);
QUADD_PMU_CNTR_INFO(bus_cycles, BUS_CYCLES);
QUADD_PMU_CNTR_INFO(l3d_cache_allocate, L3D_CACHE_ALLOCATE);
QUADD_PMU_CNTR_INFO(l3d_cache_refill, L3D_CACHE_REFILL);
QUADD_PMU_CNTR_INFO(l3d_cache, L3D_CACHE);
QUADD_PMU_CNTR_INFO(l3d_cache_wb, L3D_CACHE_WB);

static const struct quadd_pmu_cntr_info *dsu_cntrs[] = {
	&quadd_pmu_cntr_cpu_cycles,
	&quadd_pmu_cntr_bus_access,
	&quadd_pmu_cntr_memory_error,
	&quadd_pmu_cntr_bus_cycles,
	&quadd_pmu_cntr_l3d_cache_allocate,
	&quadd_pmu_cntr_l3d_cache_refill,
	&quadd_pmu_cntr_l3d_cache,
	&quadd_pmu_cntr_l3d_cache_wb,
	NULL,
};

static struct
quadd_event_source tegra23x_pmu_dsu_int = {
	.name			= "tegra23x_pmu_dsu",
	.description		= "T23X Uncore PMU DSU",
	.enable			= tegra23x_pmu_dsu_enable,
	.disable		= tegra23x_pmu_dsu_disable,
	.start			= tegra23x_pmu_dsu_start,
	.stop			= tegra23x_pmu_dsu_stop,
	.read			= tegra23x_pmu_dsu_read,
	.set_events		= tegra23x_pmu_dsu_set_events,
	.supported_events	= supported_events,
	.current_events		= current_events,
	.pmu_cntrs		= dsu_cntrs,
};

static void dsu_get_associated_cpus(int cluster_id, cpumask_t *mask)
{
	int cpu;

	cpumask_clear(mask);

	for_each_possible_cpu(cpu) {
		if (topology_physical_package_id(cpu) == cluster_id)
			cpumask_set_cpu(cpu, mask);
	}
}

static bool is_cluster_available(int cluster_id)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		if (topology_physical_package_id(cpu) == cluster_id)
			return true;
	}

	return false;
}

static bool dsu_get_clusters_info(void)
{
	u32 pmcr = 0;
	struct dsu_unit *unit;
	struct dsu_cpu_context *cpu_ctx;
	unsigned int pmceid[2];
	unsigned int nr_cntrs, idcode, i, cpu;
	DECLARE_BITMAP(pmceid_bitmap, DSU_MAX_EVENTS);

	pmcr = __dsu_pmu_read_pmcr();

	nr_cntrs = (pmcr >> CLUSTERPMCR_N_SHIFT) & CLUSTERPMCR_N_MASK;
	idcode = (pmcr >> CLUSTERPMCR_IDCODE_SHIFT) & CLUSTERPMCR_IDCODE_MASK;

	if (idcode != DSU_IDCODE_DSU_AE)
		return false;

	if (nr_cntrs == 0)
		return false;

	pmceid[0] = __dsu_pmu_read_pmceid(0);
	pmceid[1] = __dsu_pmu_read_pmceid(1);
	bitmap_from_arr32(pmceid_bitmap, pmceid, DSU_MAX_EVENTS);

	for (i = 0; i < DSU_MAX_CLUSTERS; i++) {
		unit = &ctx.units[i];
		unit->id = i;
		unit->is_used = false;
		unit->is_available = is_cluster_available(i);

		unit->nr_cntrs = nr_cntrs;

		bitmap_copy(unit->pmceid_bitmap, pmceid_bitmap, DSU_MAX_EVENTS);
		bitmap_zero(unit->used_cntrs, DSU_MAX_CLUSTER_CNTRS);

		dsu_get_associated_cpus(i, &unit->associated_cpus);

		for_each_cpu(cpu, &unit->associated_cpus) {
			cpu_ctx = per_cpu_ptr(ctx.cpu_ctx, cpu);
			cpu_ctx->unit = unit;
		}
	}

	return true;
}

static bool is_device_available(const char *path)
{
	struct device_node *node;

	node = of_find_node_by_path(path);
	if (!node)
		return false;

	return of_device_is_available(node);
}

struct quadd_event_source *
quadd_tegra23x_pmu_dsu_init(void)
{
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
	if (tegra_get_chipid() != TEGRA_CHIPID_TEGRA23)
#else
	if (tegra_get_chip_id() != TEGRA234)
#endif
		return NULL;

	if (!is_device_available("/dsu-pmu-0"))
		return NULL;

	ctx.cpu_ctx = alloc_percpu(struct dsu_cpu_context);
	if (!ctx.cpu_ctx)
		return ERR_PTR(-ENOMEM);

	bitmap_zero(ctx.used_units, DSU_MAX_CLUSTERS);

	if (!dsu_get_clusters_info())
		return NULL;

	return &tegra23x_pmu_dsu_int;
}

void quadd_tegra23x_pmu_dsu_deinit(void)
{
	free_percpu(ctx.cpu_ctx);
}
