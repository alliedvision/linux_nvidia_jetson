/*
 * drivers/misc/tegra-profiler/tegra23x_pmu_scf.c
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

#include <linux/tegra-mce.h>
#include <dmce_perfmon.h>

#include "tegra23x_pmu_scf.h"
#include "quadd.h"

#define BUS_ACCESS		0x19
#define BUS_CYCLES		0x1D
#define BUS_ACCESS_RD		0x60
#define BUS_ACCESS_WR		0x61
#define BUS_ACCESS_SHARED	0x62
#define BUS_ACCESS_NOT_SHARED	0x63
#define BUS_ACCESS_NORMAL	0x64
#define BUS_ACCESS_PERIPH	0x65

#define SCF_CACHE_ALLOCATE	0xF0
#define SCF_CACHE_REFILL	0xF1
#define SCF_CACHE		0xF2
#define SCF_CACHE_WB		0xF3

/* NV_PMSELR group and unit selection register */
#define PMSELR_GROUP_SCF	0x0
#define PMSELR_UNIT_SCF_SCF	0x0

/* T23x SCF uncore perfmon - maximum possible counters per unit */
#define UNIT_MAX_CTRS	0x10

#define TEGRA23X_UNIT(__id)	(__id & 0xf)
#define TEGRA23X_EVENT(__id)	(__id >> 4)

struct cntr_info {
	u32 prev_val;
	u32 id_raw;
	u32 id_hw;
	size_t out_idx;
};

struct uncore_unit {
	u32 group_id;
	u32 unit_id;

	bool is_used;
	bool is_available;

	unsigned long nr_ctrs;

	struct cntr_info cntrs[UNIT_MAX_CTRS];
	DECLARE_BITMAP(used_ctrs, UNIT_MAX_CTRS);
};

static struct tegra23x_pmu_scf_ctx {
	struct uncore_unit scf;
} ctx;

static void
mce_perfmon_rw(uint8_t command, uint8_t group_id, uint8_t unit_id,
	uint8_t reg, uint8_t counter, u32 *data)
{
	union dmce_perfmon_ari_request_hi_t r;
	u32 status = -1;

	r.bits.command = command;
	r.bits.group = group_id;
	r.bits.unit = unit_id;
	r.bits.reg = reg;
	r.bits.counter = counter;

	if (command == DMCE_PERFMON_COMMAND_WRITE)
		status = tegra_mce_write_uncore_perfmon(r.flat, *data);
	else if (command == DMCE_PERFMON_COMMAND_READ)
		status = tegra_mce_read_uncore_perfmon(r.flat, data);
	else
		pr_err("perfmon command not recognized");

	if (status != DMCE_PERFMON_STATUS_SUCCESS) {
		pr_err("perfmon status error: %u", status);
		pr_info("ARI CMD:%x REG:%x CTR:%x Data:%x\n", command, reg,
			counter, *data);
	}
}

static u32
mce_perfmon_read(uint8_t group_id, uint8_t unit_id, uint8_t reg, uint8_t counter)
{
	u32 data = 0;

	mce_perfmon_rw(DMCE_PERFMON_COMMAND_READ, group_id, unit_id, reg, counter, &data);
	return data;
}

static void
mce_perfmon_write(uint8_t group_id, uint8_t unit_id, uint8_t reg, uint8_t counter, u32 value)
{
	mce_perfmon_rw(DMCE_PERFMON_COMMAND_WRITE, group_id, unit_id, reg, counter, &value);
}

static inline u32 scf_pmcr_read(struct uncore_unit *unit)
{
	u32 value;

	value = mce_perfmon_read(unit->group_id, unit->unit_id, NV_PMCR, 0);
	return value;
}

static inline void scf_pmcr_write(struct uncore_unit *unit, u32 value)
{
	mce_perfmon_write(unit->group_id, unit->unit_id, NV_PMCR, 0, value);
}

static inline u32 scf_pmevcntr_read(struct uncore_unit *unit, u32 counter)
{
	u32 value;

	value = mce_perfmon_read(unit->group_id, unit->unit_id, NV_PMEVCNTR, counter);
	return value;
}

static inline void scf_pmevtyper_write(struct uncore_unit *unit, u32 counter, u32 event_id)
{
	mce_perfmon_write(unit->group_id, unit->unit_id, NV_PMEVTYPER, counter, event_id & 0xffff);
}

static inline void scf_pmcntenset_write(struct uncore_unit *unit, u32 bitmask)
{
	mce_perfmon_write(unit->group_id, unit->unit_id, NV_PMCNTENSET, 0, bitmask);
}

static inline void scf_pmcntenclr_write(struct uncore_unit *unit, u32 bitmask)
{
	mce_perfmon_write(unit->group_id, unit->unit_id, NV_PMCNTENCLR, 0, bitmask);
}

static void clean_units(void)
{
	struct uncore_unit *unit = &ctx.scf;

	if (unit->is_used) {
		memset(unit->cntrs, 0, sizeof(unit->cntrs));
		bitmap_zero(unit->used_ctrs, UNIT_MAX_CTRS);
		unit->is_used = false;
	}
}

static int add_event(const struct quadd_event *event, size_t out_idx)
{
	unsigned long idx, nr_ctrs;
	struct cntr_info *cntr;
	u32 event_raw, event_hw;
	struct uncore_unit *unit = &ctx.scf;

	nr_ctrs = unit->nr_ctrs;

	idx = find_first_zero_bit(unit->used_ctrs, nr_ctrs);
	if (idx >= nr_ctrs)
		return -EOPNOTSUPP;

	event_raw = event->id;
	event_hw = TEGRA23X_EVENT(event_raw);

	cntr = &unit->cntrs[idx];
	cntr->id_raw = event_raw;
	cntr->id_hw = event_hw;
	cntr->out_idx = out_idx;

	set_bit(idx, unit->used_ctrs);

	return 0;
}

static int
read_counters(struct uncore_unit *unit,
	      struct quadd_event_data *events, int max)
{
	unsigned long nr_ctrs, idx = 0;
	u32 val, prev_val, delta;
	struct cntr_info *cntr;
	struct quadd_event_data *curr, *end;

	nr_ctrs = unit->nr_ctrs;

	if (unlikely(bitmap_empty(unit->used_ctrs, nr_ctrs)))
		return 0;

	curr = events;
	end = events + max;

	while (idx < nr_ctrs && curr < end) {
		idx = find_next_bit(unit->used_ctrs, nr_ctrs, idx);
		if (idx != nr_ctrs) {
			cntr = &unit->cntrs[idx];

			val = scf_pmevcntr_read(unit, idx);

			curr->event_source =
				QUADD_EVENT_SOURCE_T23X_UNCORE_PMU_SCF;
			curr->max_count = U32_MAX;

			curr->event.type = QUADD_EVENT_TYPE_RAW_T23X_UNCORE_SCF;
			curr->event.id = cntr->id_raw;

			curr->out_idx = cntr->out_idx;

			prev_val = cntr->prev_val;

			if (prev_val <= val)
				delta = val - prev_val;
			else
				delta = U32_MAX - prev_val + val;

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

static int tegra23x_pmu_scf_enable(void)
{
	union dmce_perfmon_pmcr_t pmcr = {0};

	struct uncore_unit *unit = &ctx.scf;

	pmcr.flat = scf_pmcr_read(unit);

	pmcr.bits.e = 1;
	pmcr.bits.p = 1;
	scf_pmcr_write(unit, pmcr.flat);

	return 0;
}

static void tegra23x_pmu_scf_disable(void)
{
	struct uncore_unit *unit = &ctx.scf;
	union dmce_perfmon_pmcr_t pmcr = {0};

	pmcr.bits.e = 0;
	pmcr.bits.p = 1;
	scf_pmcr_write(unit, pmcr.flat);

	clean_units();
}

static void start_unit(struct uncore_unit *unit)
{
	u32 bitmask = 0;
	struct cntr_info *cntr;
	unsigned long nr_ctrs, idx = 0;

	nr_ctrs = unit->nr_ctrs;

	if (bitmap_empty(unit->used_ctrs, nr_ctrs))
		return;

	while (idx < nr_ctrs) {
		idx = find_next_bit(unit->used_ctrs, nr_ctrs, idx);
		if (idx != nr_ctrs) {
			cntr = &unit->cntrs[idx];
			cntr->prev_val = 0;
			scf_pmevtyper_write(unit, idx, cntr->id_hw);
			bitmask |= BIT(idx);
		}
		idx++;
	}

	scf_pmcntenset_write(unit, bitmask);
	tegra23x_pmu_scf_enable();
}

static void stop_unit(struct uncore_unit *unit)
{
	u32 bitmask = 0;
	unsigned long nr_ctrs, idx = 0;

	nr_ctrs = unit->nr_ctrs;

	if (bitmap_empty(unit->used_ctrs, nr_ctrs))
		return;

	while (idx < nr_ctrs) {
		idx = find_next_bit(unit->used_ctrs, nr_ctrs, idx);
		if (idx != nr_ctrs)
			bitmask |= BIT(idx);
		idx++;
	}

	scf_pmcntenclr_write(unit, bitmask);
}

static void tegra23x_pmu_scf_start(void)
{
	struct uncore_unit *unit = &ctx.scf;

	if (unit->is_used)
		start_unit(unit);
}

static void tegra23x_pmu_scf_stop(void)
{
	struct uncore_unit *unit = &ctx.scf;

	if (unit->is_used)
		stop_unit(unit);
}

static int tegra23x_pmu_scf_read(struct quadd_event_data *events, int max)
{
	int result = 0;
	struct uncore_unit *unit = &ctx.scf;

	if (unit->is_used)
		result = read_counters(unit, events, max);

	return result;
}

static int
tegra23x_pmu_scf_set_events(int cpuid, const struct quadd_event *events,
			    int size, size_t base_idx)
{
	size_t out_idx = base_idx;
	int i, err, nr_events = 0;
	struct uncore_unit *unit = &ctx.scf;

	clean_units();

	for (i = 0; i < size; i++) {
		const struct quadd_event *event = &events[i];

		if (event->type == QUADD_EVENT_TYPE_RAW_T23X_UNCORE_SCF) {
			err = add_event(event, out_idx++);
			if (err < 0) {
				clean_units();
				return err;
			}
			nr_events++;
		}
	}
	unit->is_used = true;

	return nr_events;
}

static int
supported_events(int cpuid, struct quadd_event *events,
		 int max, unsigned int *raw_event_mask, int *nr_ctrs)
{
	struct uncore_unit *unit = &ctx.scf;

	*raw_event_mask = 0x0fff;
	*nr_ctrs = unit->nr_ctrs;

	return 0;
}

static int
current_events(int cpuid, struct quadd_event *events, int max)
{
	unsigned long nr_ctrs, cntr_id = 0;
	struct quadd_event *curr, *end;
	struct uncore_unit *unit = &ctx.scf;

	curr = events;
	end = curr + max;

	if (unit->is_used) {
		nr_ctrs = unit->nr_ctrs;
		while (curr < end) {
			cntr_id = find_next_bit(unit->used_ctrs, nr_ctrs, cntr_id);
			if (cntr_id >= nr_ctrs)
				break;

			curr->type = QUADD_EVENT_TYPE_RAW_T23X_UNCORE_SCF;
			curr->id = unit->cntrs[cntr_id++].id_raw;
			curr++;
		}
	}

	return curr - events;
}

static void scf_get_unit_info(struct uncore_unit *unit)
{
	union dmce_perfmon_pmcr_t pmcr = {0};

	pmcr.flat = scf_pmcr_read(unit);
	unit->nr_ctrs = min_t(u32, pmcr.bits.n, UNIT_MAX_CTRS);
}

static bool is_device_available(const char *path)
{
	struct device_node *node;

	node = of_find_node_by_path(path);
	if (!node)
		return false;

	return of_device_is_available(node);
}

QUADD_PMU_CNTR_INFO(bus_access, BUS_ACCESS);
QUADD_PMU_CNTR_INFO(bus_cycles, BUS_CYCLES);
QUADD_PMU_CNTR_INFO(bus_access_rd, BUS_ACCESS_RD);
QUADD_PMU_CNTR_INFO(bus_access_wr, BUS_ACCESS_WR);
QUADD_PMU_CNTR_INFO(bus_access_shared, BUS_ACCESS_SHARED);
QUADD_PMU_CNTR_INFO(bus_access_not_shared, BUS_ACCESS_NOT_SHARED);
QUADD_PMU_CNTR_INFO(bus_access_normal, BUS_ACCESS_NORMAL);
QUADD_PMU_CNTR_INFO(bus_access_periph, BUS_ACCESS_PERIPH);
QUADD_PMU_CNTR_INFO(scf_cache_allocate, SCF_CACHE_ALLOCATE);
QUADD_PMU_CNTR_INFO(scf_cache_refill, SCF_CACHE_REFILL);
QUADD_PMU_CNTR_INFO(scf_cache, SCF_CACHE);
QUADD_PMU_CNTR_INFO(scf_cache_wb, SCF_CACHE_WB);

static const struct quadd_pmu_cntr_info *scf_cntrs[] = {
	&quadd_pmu_cntr_bus_access,
	&quadd_pmu_cntr_bus_cycles,
	&quadd_pmu_cntr_bus_access_rd,
	&quadd_pmu_cntr_bus_access_wr,
	&quadd_pmu_cntr_bus_access_shared,
	&quadd_pmu_cntr_bus_access_not_shared,
	&quadd_pmu_cntr_bus_access_normal,
	&quadd_pmu_cntr_bus_access_periph,
	&quadd_pmu_cntr_scf_cache_allocate,
	&quadd_pmu_cntr_scf_cache_refill,
	&quadd_pmu_cntr_scf_cache,
	&quadd_pmu_cntr_scf_cache_wb,
	NULL,
};

static struct
quadd_event_source tegra23x_pmu_scf_int = {
	.name			= "tegra23x_pmu_scf",
	.description		= "T23X Uncore PMU SCF",
	.enable			= tegra23x_pmu_scf_enable,
	.disable		= tegra23x_pmu_scf_disable,
	.start			= tegra23x_pmu_scf_start,
	.stop			= tegra23x_pmu_scf_stop,
	.read			= tegra23x_pmu_scf_read,
	.set_events		= tegra23x_pmu_scf_set_events,
	.supported_events	= supported_events,
	.current_events		= current_events,
	.pmu_cntrs		= scf_cntrs,
};

struct quadd_event_source *
quadd_tegra23x_pmu_scf_init(void)
{
	struct uncore_unit *unit;

#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
	if (tegra_get_chipid() != TEGRA_CHIPID_TEGRA23)
#else
	if (tegra_get_chip_id() != TEGRA234)
#endif
		return NULL;

	if (!is_device_available("/scf-pmu"))
		return NULL;

	unit = &ctx.scf;
	unit->group_id = PMSELR_GROUP_SCF;
	unit->unit_id = PMSELR_UNIT_SCF_SCF;

	unit->is_used = false;

	bitmap_zero(unit->used_ctrs, UNIT_MAX_CTRS);

	scf_get_unit_info(unit);
	if (unit->nr_ctrs == 0) {
		unit->is_available = false;
		return NULL;
	}
	unit->is_available = true;

	return &tegra23x_pmu_scf_int;
}

void quadd_tegra23x_pmu_scf_deinit(void)
{
}
