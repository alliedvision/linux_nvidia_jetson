/*
 * T23x SCF Uncore PMU support
 *
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)

/*
* perf events refactored include structure starting with 4.9
* This driver is only valid with kernel version 4.9 and greater
*/
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/sysfs.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#include <linux/sched/clock.h>
#else
#include <linux/sched_clock.h>
#endif

#include <asm/irq_regs.h>
#include <asm/sysreg.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include <linux/tegra-mce.h>
#include <dmce_perfmon.h>

/* NV_PMSELR group and unit selection register */
#define PMSELR_GROUP_SCF	0x0
#define PMSELR_UNIT_SCF_SCF	0x0

/* T23x SCF uncore perfmon supports 6 counters per unit */
#define UNIT_CTRS	0x6

/* All uncore counters are 32 bit */
#define COUNTER_MASK 0xFFFFFFFF
#define MAX_COUNTER (1ULL << 32)

/*
 * Format for raw events is 0xEEU
 * EE: event number
 * U:  unit (0 for SCF)
 * eg. SCF BUS_ACCESS: perf stat -e r190
 */
#define CONFIG_UNIT(_config)	(_config & 0xf)
#define CONFIG_EVENT(_config)	(_config >> 4)

#define BUS_ACCESS						0x19
#define BUS_CYCLES						0x1D
#define BUS_ACCESS_RD					0x60
#define BUS_ACCESS_WR					0x61
#define BUS_ACCESS_SHARED			0x62
#define BUS_ACCESS_NOT_SHARED	0x63
#define BUS_ACCESS_NORMAL			0x64
#define BUS_ACCESS_PERIPH			0x65

#define SCF_CACHE_ALLOCATE		0xF0
#define SCF_CACHE_REFILL			0xF1
#define SCF_CACHE							0xF2
#define SCF_CACHE_WB					0xF3

#define NV_INT_SNOC_START			0xD000
#define NV_INT_SNOC_END				0xD0FF

#define NV_INT_SCFc_START			0xD100
#define NV_INT_SCFc_END				0xD1FF

#define NV_INT_ACI_START			0xD200
#define NV_INT_ACI_END				0xD2FF

static ssize_t scf_uncore_event_sysfs_show(struct device *dev,
											  struct device_attribute *attr, char *page)
{
	struct perf_pmu_events_attr *pmu_attr;
	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);
	return sprintf(page, "event=0x%03llx\n", pmu_attr->id);
}

#define SCF_EVENT_ATTR(name, config) \
	PMU_EVENT_ATTR(name, scf_event_attr_##name, \
				   config, scf_uncore_event_sysfs_show)

SCF_EVENT_ATTR(bus_access, BUS_ACCESS);
SCF_EVENT_ATTR(bus_cycles, BUS_CYCLES);
SCF_EVENT_ATTR(bus_access_rd, BUS_ACCESS_RD);
SCF_EVENT_ATTR(bus_access_wr, BUS_ACCESS_WR);
SCF_EVENT_ATTR(bus_access_shared, BUS_ACCESS_SHARED);
SCF_EVENT_ATTR(bus_access_not_shared, BUS_ACCESS_NOT_SHARED);
SCF_EVENT_ATTR(bus_access_normal, BUS_ACCESS_NORMAL);
SCF_EVENT_ATTR(bus_access_periph, BUS_ACCESS_PERIPH);
SCF_EVENT_ATTR(scf_cache_allocate, SCF_CACHE_ALLOCATE);
SCF_EVENT_ATTR(scf_cache_refill, SCF_CACHE_REFILL);
SCF_EVENT_ATTR(scf_cache, SCF_CACHE);
SCF_EVENT_ATTR(scf_cache_wb, SCF_CACHE_WB);

static struct attribute *scf_uncore_pmu_events[] = {
	&scf_event_attr_bus_access.attr.attr,
	&scf_event_attr_bus_cycles.attr.attr,
	&scf_event_attr_bus_access_rd.attr.attr,
	&scf_event_attr_bus_access_wr.attr.attr,
	&scf_event_attr_bus_access_shared.attr.attr,
	&scf_event_attr_bus_access_not_shared.attr.attr,
	&scf_event_attr_bus_access_normal.attr.attr,
	&scf_event_attr_bus_access_periph.attr.attr,
	&scf_event_attr_scf_cache_allocate.attr.attr,
	&scf_event_attr_scf_cache_refill.attr.attr,
	&scf_event_attr_scf_cache.attr.attr,
	&scf_event_attr_scf_cache_wb.attr.attr,
	NULL,
};

static struct attribute_group scf_uncore_pmu_events_group = {
	.name = "events",
	.attrs = scf_uncore_pmu_events,
};

PMU_FORMAT_ATTR(unit,	"config:0-3");
PMU_FORMAT_ATTR(event,	"config:4-15");

static struct attribute *scf_uncore_pmu_formats[] = {
	&format_attr_event.attr,
	&format_attr_unit.attr,
	NULL,
};

static struct attribute_group scf_uncore_pmu_format_group = {
	.name = "format",
	.attrs = scf_uncore_pmu_formats,
};

static const struct attribute_group *scf_uncore_pmu_attr_grps[] = {
	&scf_uncore_pmu_events_group,
	&scf_uncore_pmu_format_group,
	NULL,
};

struct uncore_unit {
	u32 nv_group_id;
	u32 nv_unit_id;
	struct perf_event *events[UNIT_CTRS];
	DECLARE_BITMAP(used_ctrs, UNIT_CTRS);
};

struct uncore_pmu {
	struct platform_device *pdev;
	struct pmu pmu;
	struct uncore_unit scf;
};

static inline struct uncore_pmu *to_uncore_pmu(struct pmu *pmu)
{
	return container_of(pmu, struct uncore_pmu, pmu);
}

static inline struct uncore_unit *get_unit(
		struct uncore_pmu *uncore_pmu, u32 unit_id)
{
	struct platform_device *pdev;
	pdev = uncore_pmu->pdev;

	switch (unit_id) {
		case PMSELR_UNIT_SCF_SCF:
			return &uncore_pmu->scf;
		default:
			dev_dbg(&pdev->dev, "Error invalid unit id: %u\n", unit_id);
			return NULL;
	}
}

static void mce_perfmon_rw(uint8_t command, uint8_t group, uint8_t unit,
		   uint8_t reg, uint8_t counter, u32 *data)
{
	union dmce_perfmon_ari_request_hi_t r;
	u32 status = -1;

	r.bits.command = command;
	r.bits.group = group;
	r.bits.unit = unit;
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

static u32 mce_perfmon_read(struct uncore_unit* unit, uint8_t reg,
		uint8_t counter)
{
	u32 data = 0;
	mce_perfmon_rw(DMCE_PERFMON_COMMAND_READ, unit->nv_group_id, unit->nv_unit_id,
			reg, counter, &data);
	return data;
}

static void mce_perfmon_write(struct uncore_unit* unit, uint8_t reg,
		uint8_t counter, u32 value)
{
	mce_perfmon_rw(DMCE_PERFMON_COMMAND_WRITE, unit->nv_group_id, unit->nv_unit_id,
			reg, counter, &value);
}

/*
 * Enable the SCF counters.
 */
static void scf_uncore_pmu_enable(struct pmu *pmu)
{
	int enabled;

	struct uncore_pmu *uncore_pmu = to_uncore_pmu(pmu);
	struct uncore_unit *uncore_unit = &uncore_pmu->scf;

	union dmce_perfmon_pmcr_t pmcr = {0};

	enabled = bitmap_weight(uncore_unit->used_ctrs, UNIT_CTRS);

	if (!enabled)
		return;

	pmcr.bits.e = 1;
	mce_perfmon_write(uncore_unit, NV_PMCR, 0, pmcr.flat);
}

/*
 * Disable the SCF counters.
 */
static void scf_uncore_pmu_disable(struct pmu *pmu)
{
	struct uncore_pmu *uncore_pmu = to_uncore_pmu(pmu);
	struct uncore_unit *uncore_unit = &uncore_pmu->scf;

	union dmce_perfmon_pmcr_t pmcr = {0};

	int enabled = bitmap_weight(uncore_unit->used_ctrs, UNIT_CTRS);

	if (!enabled)
		return;

	pmcr.bits.e = 0;
	mce_perfmon_write(uncore_unit, NV_PMCR, 0, pmcr.flat);
}

/*
 * To handle cases of extreme interrupt latency, we program
 * the counter with half of the max count for the counters.
 */
static void scf_uncore_event_set_period(
		struct uncore_unit *uncore_unit, struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	u32 val = COUNTER_MASK >> 1;
	local64_set(&hwc->prev_count, val);

	mce_perfmon_write(uncore_unit, NV_PMEVCNTR, idx, val);
}

static void scf_uncore_event_start(struct perf_event *event, int flags) {

	struct uncore_pmu *uncore_pmu;
	struct uncore_unit *uncore_unit;
	struct hw_perf_event *hwc = &event->hw;
	u32 idx = hwc->idx;
	u32 unit_id;
	u32 event_id;

	/* CPU0 does all uncore counting */
	if (event->cpu != 0)
		return;

	/* We always reprogram the counter */
	if (flags & PERF_EF_RELOAD)
		WARN_ON(!(hwc->state & PERF_HES_UPTODATE));

	unit_id = CONFIG_UNIT(event->attr.config);
	uncore_pmu = to_uncore_pmu(event->pmu);
	uncore_unit = get_unit(uncore_pmu, unit_id);

	if (unlikely(uncore_unit == NULL))
		return;

	hwc->state = 0;

	scf_uncore_event_set_period(uncore_unit, event);

	/* Program unit's event register */
	event_id = CONFIG_EVENT(event->attr.config);
	mce_perfmon_write(uncore_unit, NV_PMEVTYPER, idx, event_id);

	/* Enable interrupt and start counter */
	mce_perfmon_write(uncore_unit, NV_PMINTENSET, 0, BIT(idx));
	mce_perfmon_write(uncore_unit, NV_PMCNTENSET, 0, BIT(idx));
}

static void scf_uncore_event_update(
		struct uncore_unit *uncore_unit, struct perf_event *event, bool ovf)
{
	struct hw_perf_event *hwc = &event->hw;
	u32 idx = hwc->idx;
	u64 delta = 0;
	u64 prev = 0;
	u64 now = 0;

	do {
		prev = local64_read(&hwc->prev_count);
		now = mce_perfmon_read(uncore_unit, NV_PMEVCNTR, idx);
	} while (local64_cmpxchg(&hwc->prev_count, prev, now) != prev);

	if (prev > now)
		delta = MAX_COUNTER - prev + now;
	else
		/* Either an incremental read, or fielding an IRQ from a ctr overflow */
		delta = now - prev + (ovf ? MAX_COUNTER : 0);

	local64_add(delta, &event->count);
}

static void scf_uncore_event_stop(struct perf_event *event, int flags)
{
	struct uncore_pmu *uncore_pmu;
	struct uncore_unit *uncore_unit;
	struct hw_perf_event *hwc = &event->hw;
	u32 idx = hwc->idx;
	u32 unit_id;

	/* CPU0 does all uncore counting */
	if (event->cpu != 0)
		return;

	if (event->hw.state & PERF_HES_STOPPED)
		return;

	unit_id = CONFIG_UNIT(event->attr.config);
	uncore_pmu = to_uncore_pmu(event->pmu);
	uncore_unit = get_unit(uncore_pmu, unit_id);

	if (unlikely(uncore_unit == NULL))
		return;

	/* Stop counter and disable interrupt */
	mce_perfmon_write(uncore_unit, NV_PMCNTENCLR, 0, BIT(idx));
	mce_perfmon_write(uncore_unit, NV_PMINTENCLR, 0, BIT(idx));

	if (flags & PERF_EF_UPDATE)
		scf_uncore_event_update(uncore_unit, event, false);

	event->hw.state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

/*
 * Allocates hardware resouces required to service the event
 * (and optionally starts counting)
 */
static int scf_uncore_event_add(struct perf_event *event, int flags)
{
	struct uncore_pmu *uncore_pmu;
	struct uncore_unit *uncore_unit;
	struct platform_device *pdev;
	struct hw_perf_event *hwc = &event->hw;
	u32 unit_id;
	u32 idx;

	/* CPU0 does all uncore counting */
	if (event->cpu != 0)
		return 0;

	unit_id = CONFIG_UNIT(event->attr.config);
	uncore_pmu = to_uncore_pmu(event->pmu);
	uncore_unit = get_unit(uncore_pmu, unit_id);
	pdev = uncore_pmu->pdev;

	if (!uncore_unit) {
		dev_err(&pdev->dev, "Unsupported unit id: %u\n", unit_id);
		return -EINVAL;
	}

	idx = find_first_zero_bit(uncore_unit->used_ctrs, UNIT_CTRS);
	/* All counters are in use */
	if (idx == UNIT_CTRS)
		return -EOPNOTSUPP;

	set_bit(idx, uncore_unit->used_ctrs);
	uncore_unit->events[idx] = event;
	hwc->idx = idx;
	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;

	if (flags & PERF_EF_START)
		scf_uncore_event_start(event, PERF_EF_RELOAD);

	perf_event_update_userpage(event);
	return 0;
}

static void scf_uncore_event_del(struct perf_event *event, int flags)
{
	struct uncore_pmu *uncore_pmu;
	struct uncore_unit *uncore_unit;
	struct hw_perf_event *hwc = &event->hw;
	u32 unit_id;
	u32 idx = hwc->idx;

	/* CPU0 does all uncore counting */
	if (event->cpu != 0)
		return;

	unit_id = CONFIG_UNIT(event->attr.config);
	uncore_pmu = to_uncore_pmu(event->pmu);
	uncore_unit = get_unit(uncore_pmu, unit_id);

	if (unlikely(uncore_unit == NULL))
		return;

	scf_uncore_event_stop(event, flags | PERF_EF_UPDATE);

	clear_bit(idx, uncore_unit->used_ctrs);
	uncore_unit->events[idx] = NULL;

	perf_event_update_userpage(event);
}

static void scf_uncore_event_read(struct perf_event *event)
{
	struct uncore_pmu *uncore_pmu;
	struct uncore_unit *uncore_unit;
	u32 unit_id;

	/* CPU0 does all uncore counting */
	if (event->cpu != 0)
		return;

	unit_id = CONFIG_UNIT(event->attr.config);
	uncore_pmu = to_uncore_pmu(event->pmu);
	uncore_unit = get_unit(uncore_pmu, unit_id);

	if (unlikely(uncore_unit == NULL))
		return;

	scf_uncore_event_update(uncore_unit, event, false);
}

/*
 * Handle counter overflows. We have one interrupt for all uncore
 * counters, so iterate through active units to find overflow bits
 */
static irqreturn_t scf_handle_irq(int irq_num, void *data)
{
	struct uncore_pmu *uncore_pmu = data;
	struct uncore_unit *uncore_unit;
	u32 int_en;
	u32 ovf;
	u32 idx;

	uncore_unit = &uncore_pmu->scf;

	int_en = mce_perfmon_read(uncore_unit, NV_PMINTENCLR, 0);
	ovf = mce_perfmon_read(uncore_unit, NV_PMOVSCLR, 0);

	/* Disable the interrupt to prevent another interrupt during the handling */
	mce_perfmon_write(uncore_unit, NV_PMINTENCLR, 0, int_en);

	/* Find the counters that report overflow */
	for_each_set_bit(idx, uncore_unit->used_ctrs, UNIT_CTRS) {
		if (BIT(idx) & int_en & ovf) {
			struct perf_event *event;

			event = uncore_unit->events[idx];
			if (event) {
				scf_uncore_event_update(uncore_unit, event, true);
				scf_uncore_event_set_period(uncore_unit, event);
			}
		}
	}

	/* Clear overflow and reenable the interrupt */
	mce_perfmon_write(uncore_unit, NV_PMOVSCLR, 0, ovf);
	mce_perfmon_write(uncore_unit, NV_PMINTENSET, 0, int_en);

	return IRQ_HANDLED;
}

/*
 * event_init: Verify this PMU can handle the desired event
 */
static int scf_uncore_event_init(struct perf_event *event)
{
	struct uncore_pmu *uncore_pmu;
	struct platform_device *pdev;
	struct hw_perf_event *hwc = &event->hw;
	u32 unit_id;
	u32 event_id;

	uncore_pmu = to_uncore_pmu(event->pmu);
	pdev = uncore_pmu->pdev;

	/*
	 * The uncore counters are shared by all CPU cores. Therefore it does not
	 * support sampling mode or attach to a task (per-process mode).
	 */
	if (is_sampling_event(event) || event->attach_state & PERF_ATTACH_TASK) {
		dev_dbg(&pdev->dev, "Can't support sampling events\n");
		return -EOPNOTSUPP;
	}

	/*
	 * The uncore counters are not specific to any CPU, so cannot support per-task.
	 */
	if (event->cpu < 0) {
		dev_err(&pdev->dev, "Can't support per-task counters\n");
		return -EINVAL;
	}

	unit_id = CONFIG_UNIT(event->attr.config);
	event_id = CONFIG_EVENT(event->attr.config);

	if (!get_unit(uncore_pmu, unit_id)) {
		dev_dbg(&pdev->dev, "Unsupported unit id: %u\n", unit_id);
		return -EINVAL;
	}

	/* Verify event is for this PMU and targets correct unit type */
	switch (event_id) {
		case BUS_ACCESS:
		case BUS_CYCLES:
		case BUS_ACCESS_RD ... BUS_ACCESS_PERIPH:
		case SCF_CACHE_ALLOCATE ... SCF_CACHE_WB:
			if (unit_id != PMSELR_UNIT_SCF_SCF)
				return -ENOENT;
			break;
		case NV_INT_SNOC_START ... NV_INT_SNOC_END:
		case NV_INT_SCFc_START ... NV_INT_SCFc_END:
		case NV_INT_ACI_START ... NV_INT_ACI_END:
			break;
		default:
			return -ENOENT;
			break;
	}

	/* Event is valid, hw not allocated yet */
	hwc->idx = -1;
	hwc->config_base = event->attr.config;

	return 0;
}

static int scf_pmu_device_probe(struct platform_device *pdev)
{
	struct uncore_pmu *uncore_pmu;
	int err;
	int irq;

	uncore_pmu = devm_kzalloc(&pdev->dev, sizeof(*uncore_pmu), GFP_KERNEL);
	if(!uncore_pmu)
		return -ENOMEM;

	uncore_pmu->scf.nv_group_id = PMSELR_GROUP_SCF;
	uncore_pmu->scf.nv_unit_id = PMSELR_UNIT_SCF_SCF;

	platform_set_drvdata(pdev, uncore_pmu);
	uncore_pmu->pmu = (struct pmu) {
		.name			= "scf_pmu",
		.task_ctx_nr	= perf_invalid_context,
		.pmu_enable		= scf_uncore_pmu_enable,
		.pmu_disable	= scf_uncore_pmu_disable,
		.event_init		= scf_uncore_event_init,
		.add			= scf_uncore_event_add,
		.del			= scf_uncore_event_del,
		.start			= scf_uncore_event_start,
		.stop			= scf_uncore_event_stop,
		.read			= scf_uncore_event_read,
		.attr_groups	= scf_uncore_pmu_attr_grps,
		.type			= PERF_TYPE_HARDWARE,
	};

	uncore_pmu->pdev = pdev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to find IRQ for T23x SCF Uncore PMU\n");
		return irq;
	}

	err = devm_request_irq(&pdev->dev, irq, scf_handle_irq,
							IRQF_NOBALANCING, "scf-pmu", uncore_pmu);

	if (err) {
		dev_err(&pdev->dev, "Unable to request IRQ for T23x SCF Uncore PMU\n");
		return err;
	}

	err = perf_pmu_register(&uncore_pmu->pmu, uncore_pmu->pmu.name, -1);
	if (err) {
		dev_err(&pdev->dev, "Error %d registering T23x SCF Uncore PMU\n", err);
		return err;
	}

	dev_info(&pdev->dev, "Registered T23x SCF Uncore PMU\n");

	return 0;
}

static const struct of_device_id scf_pmu_of_device_ids[] = {
	{.compatible = "nvidia,scf-pmu"},
	{},
};

static struct platform_driver scf_pmu_driver = {
	.driver = {
		.name = "scf-pmu-drv",
		.of_match_table = scf_pmu_of_device_ids,
	},
	.probe = scf_pmu_device_probe,
};

static int __init register_pmu_driver(void)
{
	if (tegra_platform_is_silicon() || tegra_platform_is_fpga())
		return platform_driver_register(&scf_pmu_driver);
	return 0;
}
device_initcall(register_pmu_driver);

#endif
