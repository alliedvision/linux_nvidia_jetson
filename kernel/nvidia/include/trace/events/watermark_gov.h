/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM watermark_gov

#if !defined(_TRACE_WATERMARK_GOV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_WATERMARK_GOV_H

#include <linux/tracepoint.h>

TRACE_EVENT(devfreq_watermark_target_freq,
	TP_PROTO(const char *name, unsigned long long load, unsigned long freq),

	TP_ARGS(name, load, freq),

	TP_STRUCT__entry(
		__string(name,  name)
		__field(unsigned long long, load)
		__field(unsigned long, freq)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->load = load;
		__entry->freq  = freq;
	),

	TP_printk("from=%s load=%llu freq=%lu", __get_str(name), __entry->load, __entry->freq)
);

#endif /*  _TRACE_WATERMARK_GOV_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
