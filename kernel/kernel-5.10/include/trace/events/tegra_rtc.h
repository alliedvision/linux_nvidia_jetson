/* SPDX-License-Identifier: GPL-2.0 */
/*
 * include/trace/events/tegra_rtc.h
 *
 * NVIDIA Tegra specific power events.
 *
 * Copyright (c) 2010-2020, NVIDIA CORPORATION.  All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tegra_rtc

#if !defined(_TRACE_TEGRA_RTC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEGRA_RTC_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(tegra_rtc_set_alarm,

	TP_PROTO(unsigned long now, unsigned long target),

	TP_ARGS(now, target),

	TP_STRUCT__entry(
		__field(unsigned long, now)
		__field(unsigned long, target)
	),

	TP_fast_assign(
		__entry->now = now;
		__entry->target = target;
	),

	TP_printk("now %lu, target %lu\n", __entry->now, __entry->target)
);

TRACE_EVENT(tegra_rtc_irq_handler,

	TP_PROTO(const char *s, unsigned long target),

	TP_ARGS(s, target),

	TP_STRUCT__entry(
		__string(s, s)
		__field(unsigned long, target)
	),

	TP_fast_assign(
		__assign_str(s, s);
		__entry->target = target;
	),

	TP_printk("%s: irq time %lu\n", __get_str(s), __entry->target)
);

#endif /* _TRACE_TEGRA_RTC_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
