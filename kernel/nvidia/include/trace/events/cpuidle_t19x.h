/*
 * Cpuidle event logging to ftrace.
 *
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cpuidle_t19x

#if !defined(_TRACE_CPUIDLE_T19X_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CPUIDLE_T19X_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(cpuidle_t19x_c6_count,
	TP_PROTO(
		int cpu,
		u64 val,
		const char *str
	),

	TP_ARGS(cpu, val, str),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(u64, val)
		__field(const char *, str)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->val = val;
		__entry->str = str;
	),

	TP_printk("cpu = %d %s = %llu",
		__entry->cpu,
		__entry->str,
		__entry->val
	)
);

TRACE_EVENT(cpuidle_t19x_print,
	TP_PROTO(
		const char *str
	),

	TP_ARGS(str),

	TP_STRUCT__entry(
		__field(const char *, str)
	),

	TP_fast_assign(
		__entry->str = str;
	),

	TP_printk("%s",
		__entry->str
	)
);

#endif /* _TRACE_CPUIDLE_T19X_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
