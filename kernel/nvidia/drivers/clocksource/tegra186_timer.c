/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clockchips.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <linux/tick.h>
#include <linux/vmalloc.h>
#include <linux/syscore_ops.h>
#include <linux/version.h>

#define TMRCR	0x000
#define TMRSR	0x004
#define TMRCSSR	0x008
#define TKEIE	0x100


struct tegra186_tke;

struct tegra186_tmr {
	struct clock_event_device evt;
	u64 tmr_index;
	u64 cpu_index;
	u32 freq;
	char name[20];
	void __iomem *reg_base;
	struct tegra186_tke *tke;
};

struct tegra186_tke {
	void __iomem *reg_base;
	struct tegra186_tmr tegra186_tmr[CONFIG_NR_CPUS];
};

static struct tegra186_tke *tke;

static int tegra186_timer_set_next_event(unsigned long cycles,
					 struct clock_event_device *evt)
{
	struct tegra186_tmr *tmr;
	tmr = container_of(evt, struct tegra186_tmr, evt);
	__raw_writel((1 << 31) /* EN=1, enable timer */
		     | ((cycles > 1) ? (cycles - 1) : 0), /* n+1 scheme */
		     tmr->reg_base + TMRCR);
	return 0;
}

static inline void _shutdown(struct tegra186_tmr *tmr)
{
	__raw_writel(0 << 31, /* EN=0, disable timer */
		     tmr->reg_base + TMRCR);
	__raw_writel(1 << 30, /* INTR_CLR */
		     tmr->reg_base + TMRSR);
}

static int tegra186_timer_shutdown(struct clock_event_device *evt)
{
	struct tegra186_tmr *tmr;
	tmr = container_of(evt, struct tegra186_tmr, evt);

	_shutdown(tmr);
	return 0;
}

static int tegra186_timer_set_periodic(struct clock_event_device *evt)
{
	struct tegra186_tmr *tmr  = container_of(evt, struct tegra186_tmr, evt);

	_shutdown(tmr);
	__raw_writel((1 << 31) /* EN=1, enable timer */
		     | (1 << 30) /* PER=1, periodic mode */
		     | ((tmr->freq / HZ) - 1), /* PTV, preset value*/
		     tmr->reg_base + TMRCR);
	return 0;
}

static irqreturn_t tegra186_timer_isr(int irq, void *dev_id)
{
	struct tegra186_tmr *tmr;

	tmr = (struct tegra186_tmr *) dev_id;
	__raw_writel(1 << 30, /* INTR_CLR */
		     tmr->reg_base + TMRSR);
	tmr->evt.event_handler(&tmr->evt);
	return IRQ_HANDLED;
}

static int tegra186_timer_setup(unsigned int cpu)
{
	struct tegra186_tmr *tmr = &tke->tegra186_tmr[cpu];

	clockevents_config_and_register(&tmr->evt, tmr->freq,
					1, /* min */
					0x1fffffff); /* 29 bits */
#ifdef CONFIG_SMP
	if (irq_force_affinity(tmr->evt.irq, cpumask_of(cpu))) {
		pr_err("%s: cannot set irq %d affinity to CPU%d\n",
		       __func__, tmr->evt.irq, cpu);
		BUG();
	}
#endif
	enable_irq(tmr->evt.irq);
	return 0;
}

static int tegra186_timer_enable_irq(unsigned int cpu)
{
	struct tegra186_tmr *tmr = &tke->tegra186_tmr[cpu];

#ifdef CONFIG_SMP
	if (irq_force_affinity(tmr->evt.irq, cpumask_of(cpu))) {
		pr_err("%s: cannot set irq %d affinity to CPU%d\n",
		       __func__, tmr->evt.irq, cpu);
		BUG();
	}
#endif
	enable_irq(tmr->evt.irq);
	return 0;
}

static int tegra186_timer_stop(unsigned int cpu)
{
	struct tegra186_tmr *tmr = &tke->tegra186_tmr[cpu];
	_shutdown(tmr);
	disable_irq_nosync(tmr->evt.irq);

	return 0;
}

static int tegra186_timer_suspend(void)
{
	int cpu = smp_processor_id();
	struct tegra186_tmr *tmr = &tke->tegra186_tmr[cpu];

	_shutdown(tmr);
	disable_irq_nosync(tmr->evt.irq);

	return 0;
}

static void tegra186_timer_resume(void)
{
	int cpu;
	struct tegra186_tmr *tmr;

	for_each_possible_cpu(cpu) {
		tmr = &tke->tegra186_tmr[cpu];

		/* Configure TSC as the TKE source */
		__raw_writel(2, tmr->reg_base + TMRCSSR);

		__raw_writel(1 << tmr->tmr_index,   tke->reg_base
					     + TKEIE + 4 * tmr->tmr_index);
	}

	cpu = smp_processor_id();
	tegra186_timer_enable_irq(cpu);
}

static struct syscore_ops tegra186_timer_syscore_ops = {
	.suspend = tegra186_timer_suspend,
	.resume = tegra186_timer_resume,
};

static int __init tegra186_timer_init(struct device_node *np)
{
	int cpu;
	struct tegra186_tmr *tmr;
	u32 tmr_count;
	u32 freq;
	int irq_count;
	unsigned long tmr_index, irq_index;

	/* Allocate the driver struct */
	tke = vmalloc(sizeof(*tke));
	BUG_ON(!tke);
	memset(tke, 0, sizeof(*tke));

	/* MAp MMIO */
	tke->reg_base = of_iomap(np, 0);
	if (!tke->reg_base) {
		pr_err("%s: can't map timer registers\n", __func__);
		BUG();
	}

	/* Read the parameters */
	BUG_ON(of_property_read_u32(np, "tmr-count", &tmr_count));
	irq_count = of_irq_count(np);

	BUG_ON(of_property_read_u32(np, "clock-frequency", &freq));

	tmr_index = 0;
	for_each_possible_cpu(cpu) {
		tmr = &tke->tegra186_tmr[cpu];
		tmr->tke = tke;
		tmr->tmr_index = tmr_index;
		tmr->freq = freq;

		/* Allocate a TMR */
		BUG_ON(tmr_index >= tmr_count);
		tmr->reg_base = tke->reg_base + 0x10000 * (tmr_index + 1);

		/* Allocate an IRQ */
		irq_index = tmr_index;
		BUG_ON(irq_index >= irq_count);
		/* Program TKEIE to map TMR to the right IRQ */
		__raw_writel(1 << tmr_index,   tke->reg_base
					     + TKEIE + 4 * irq_index);
		tmr->evt.irq = irq_of_parse_and_map(np, irq_index);
		BUG_ON(!tmr->evt.irq);

		/* Configure OSC as the TKE source */
		__raw_writel(1, tmr->reg_base + TMRCSSR);

		snprintf(tmr->name, sizeof(tmr->name), "tegra186_timer%d", cpu);
		tmr->evt.name = tmr->name;
		tmr->evt.cpumask = cpumask_of(cpu);
		tmr->evt.features = CLOCK_EVT_FEAT_PERIODIC |
			CLOCK_EVT_FEAT_ONESHOT;
		tmr->evt.set_next_event     = tegra186_timer_set_next_event;
		tmr->evt.set_state_shutdown = tegra186_timer_shutdown;
		tmr->evt.set_state_periodic = tegra186_timer_set_periodic;
		tmr->evt.set_state_oneshot  = tegra186_timer_shutdown;
		tmr->evt.tick_resume        = tegra186_timer_shutdown;

		/* want to be preferred over arch timers */
		tmr->evt.rating = 460;
		irq_set_status_flags(tmr->evt.irq, IRQ_NOAUTOEN | IRQ_PER_CPU);
		if (request_irq(tmr->evt.irq, tegra186_timer_isr,
				  IRQF_TIMER | IRQF_TRIGGER_HIGH
				| IRQF_NOBALANCING, tmr->name, tmr)) {
			pr_err("%s: cannot setup irq %d for CPU%d\n",
				__func__, tmr->evt.irq, cpu);
			BUG();
		}
		tmr_index++;
	}

	cpuhp_setup_state(CPUHP_AP_TEGRA_TIMER_STARTING,
			  "AP_TEGRA_TIMER_STARTING", tegra186_timer_setup,
			  tegra186_timer_stop);

	register_syscore_ops(&tegra186_timer_syscore_ops);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
CLOCKSOURCE_OF_DECLARE(tegra186_timer, "nvidia,tegra186-timer",
		       tegra186_timer_init);
#else
TIMER_OF_DECLARE(tegra186_timer, "nvidia,tegra186-timer",
		       tegra186_timer_init);
#endif
