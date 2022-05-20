/*
 * Copyright (c) 2016-2020 NVIDIA CORPORATION. All rights reserved.
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

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tegra_rtcpu

#if !defined(_TRACE_TEGRA_RTCPU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEGRA_RTCPU_H

#include <linux/tracepoint.h>

/*
 * Classes
 */

DECLARE_EVENT_CLASS(rtcpu__noarg,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp),
	TP_STRUCT__entry(
		__field(u64, tstamp)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
	),
	TP_printk("tstamp:%llu", __entry->tstamp)
);

DECLARE_EVENT_CLASS(rtcpu__arg1,
	TP_PROTO(u64 tstamp, u32 data1),
	TP_ARGS(tstamp, data1),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, data1)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->data1 = data1;
	),
	TP_printk("tstamp:%llu, data:%u", __entry->tstamp,
		__entry->data1)
);

DECLARE_EVENT_CLASS(rtcpu__dump,
	TP_PROTO(u64 tstamp, u32 id, u32 len, void *data),
	TP_ARGS(tstamp, id, len, data),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, id)
		__field(u32, len)
		__dynamic_array(__u8, data, len)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->id = id;
		__entry->len = len;
		memcpy(__get_dynamic_array(data), data, len);
	),
	TP_printk("tstamp:%llu id:0x%08x len:%u data:%s",
		__entry->tstamp, __entry->id, __entry->len,
		__print_hex(__get_dynamic_array(data), __entry->len))
);

/*
 * Unknown events
 */

DEFINE_EVENT(rtcpu__dump, rtcpu_unknown,
	TP_PROTO(u64 tstamp, u32 id, u32 len, void *data),
	TP_ARGS(tstamp, id, len, data)
);

/*
 * Non ARRAY event types
 */

TRACE_EVENT(rtcpu_armv7_exception,
	TP_PROTO(u64 tstamp, u32 type),
	TP_ARGS(tstamp, type),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, type)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->type = type;
	),
	TP_printk("tstamp:%llu type:%u", __entry->tstamp, __entry->type)
);

TRACE_EVENT(rtcpu_start,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp),
	TP_STRUCT__entry(
		__field(u64, tstamp)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
	),
	TP_printk("tstamp:%llu", __entry->tstamp)
);

#ifndef TEGRA_RTCPU_TRACE_STRING_SIZE
#define TEGRA_RTCPU_TRACE_STRING_SIZE 48
#endif

TRACE_EVENT(rtcpu_string,
	TP_PROTO(u64 tstamp, u32 id, u32 len, const char *data),
	TP_ARGS(tstamp, id, len, data),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, id)
		__field(u32, len)
		__array(char, data, TEGRA_RTCPU_TRACE_STRING_SIZE)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->id = id;
		__entry->len = len;
		strncpy(__entry->data, data, sizeof(__entry->data));
	),
	TP_printk("tstamp:%llu id:0x%08x str:\"%.*s\"",
		__entry->tstamp, __entry->id,
		(int)__entry->len, __entry->data)
);

DEFINE_EVENT(rtcpu__dump, rtcpu_bulk,
	TP_PROTO(u64 tstamp, u32 id, u32 len, void *data),
	TP_ARGS(tstamp, id, len, data)
);

/*
 * Base events
 */

DEFINE_EVENT(rtcpu__noarg, rtcpu_target_init,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtcpu__noarg, rtcpu_start_scheduler,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

/*
 * Debug interface
 */

DEFINE_EVENT(rtcpu__arg1, rtcpu_dbg_unknown,
	TP_PROTO(u64 tstamp, u32 data1),
	TP_ARGS(tstamp, data1)
);

DEFINE_EVENT(rtcpu__arg1, rtcpu_dbg_enter,
	TP_PROTO(u64 tstamp, u32 req_type),
	TP_ARGS(tstamp, req_type)
);

DEFINE_EVENT(rtcpu__noarg, rtcpu_dbg_exit,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

TRACE_EVENT(rtcpu_dbg_set_loglevel,
	TP_PROTO(u64 tstamp, u32 old_level, u32 new_level),
	TP_ARGS(tstamp, old_level, new_level),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, old_level)
		__field(u32, new_level)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->old_level = old_level;
		__entry->new_level = new_level;
	),
	TP_printk("tstamp:%llu old:%u new:%u", __entry->tstamp,
		__entry->old_level, __entry->new_level)
);

/*
 * VI Notify events
 */

extern const char * const g_trace_vinotify_tag_strs[];
extern const unsigned int g_trace_vinotify_tag_str_count;

TRACE_EVENT(rtcpu_vinotify_event_ts64,
	TP_PROTO(u64 tstamp, u8 tag, u32 ch_frame, u64 vi_tstamp, u32 data),
	TP_ARGS(tstamp, tag, ch_frame, vi_tstamp, data),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u8, tag)
		__field(u32, ch_frame)
		__field(u64, vi_tstamp)
		__field(u32, data)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->tag = tag;
		__entry->ch_frame = ch_frame;
		__entry->vi_tstamp = vi_tstamp;
		__entry->data = data;
	),
	TP_printk(
		"tstamp:%llu tag:%s channel:0x%02x frame:%u vi_tstamp:%llu data:0x%08x",
		__entry->tstamp,
		(__entry->tag < g_trace_vinotify_tag_str_count) ?
			g_trace_vinotify_tag_strs[__entry->tag] :
			__print_hex(&__entry->tag, 1),
		(__entry->ch_frame >> 8) & 0xff,
		(__entry->ch_frame >> 16) & 0xffff,
		__entry->vi_tstamp, __entry->data)
);

TRACE_EVENT(rtcpu_vinotify_event,
	TP_PROTO(u64 tstamp, u32 channel_id, u32 unit,
		u32 tag, u32 vi_ts_hi, u32 vi_ts_lo, u32 ext_data, u32 data),
	TP_ARGS(tstamp, channel_id, unit, tag, vi_ts_hi, vi_ts_lo, ext_data, data),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, channel_id)
		__field(u32, unit)
		__field(u8, tag_tag)
		__field(u8, tag_channel)
		__field(u16, tag_frame)
		__field(u64, vi_ts)
		__field(u64, data)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->channel_id = channel_id;
		__entry->unit = unit;
		__entry->tag_tag = tag & 0xff;
		__entry->tag_channel = (tag >> 8) & 0xff;
		__entry->tag_frame = (tag >> 16) & 0xffff;
		__entry->vi_ts = ((u64)vi_ts_hi << 32) | vi_ts_lo;
		__entry->data = ((u64)ext_data << 32) | data;
	),
	TP_printk(
		"tstamp:%llu cch:%d vi:%u tag:%s channel:0x%02x frame:%u "
		"vi_tstamp:%llu data:0x%016llx",
		__entry->tstamp,
		__entry->channel_id,
		__entry->unit,
		((__entry->tag_tag >> 1) < g_trace_vinotify_tag_str_count) ?
			g_trace_vinotify_tag_strs[__entry->tag_tag >> 1] :
			__print_hex(&__entry->tag_tag, 1),
		__entry->tag_channel, __entry->tag_frame,
		__entry->vi_ts, __entry->data)
);

TRACE_EVENT(rtcpu_vinotify_error,
	TP_PROTO(u64 tstamp, u32 channel_id, u32 unit,
		u32 tag, u32 vi_ts_hi, u32 vi_ts_lo, u32 ext_data, u32 data),
	TP_ARGS(tstamp, channel_id, unit, tag, vi_ts_hi, vi_ts_lo, ext_data, data),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, channel_id)
		__field(u32, unit)
		__field(u8, tag_tag)
		__field(u8, tag_channel)
		__field(u16, tag_frame)
		__field(u64, vi_ts)
		__field(u64, data)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->channel_id = channel_id;
		__entry->unit = unit;
		__entry->tag_tag = tag & 0xff;
		__entry->tag_channel = (tag >> 8) & 0xff;
		__entry->tag_frame = (tag >> 16) & 0xffff;
		__entry->vi_ts = ((u64)vi_ts_hi << 32) | vi_ts_lo;
		__entry->data = ((u64)ext_data << 32) | data;
	),
	TP_printk(
		"tstamp:%llu cch:%d vi:%u tag:%s channel:0x%02x frame:%u "
		"vi_tstamp:%llu data:0x%016llx",
		__entry->tstamp,
		__entry->channel_id,
		__entry->unit,
		((__entry->tag_tag >> 1) < g_trace_vinotify_tag_str_count) ?
			g_trace_vinotify_tag_strs[__entry->tag_tag >> 1] :
			__print_hex(&__entry->tag_tag, 1),
		__entry->tag_channel, __entry->tag_frame,
		__entry->vi_ts, __entry->data)
);

/*
 * NVCSI events
 */

extern const char * const g_trace_nvcsi_intr_class_strs[];
extern const unsigned int g_trace_nvcsi_intr_class_str_count;

extern const char * const g_trace_nvcsi_intr_type_strs[];
extern const unsigned int g_trace_nvcsi_intr_type_str_count;

TRACE_EVENT(rtcpu_nvcsi_intr,
	TP_PROTO(u64 tstamp, u8 intr_class, u8 intr_type, u32 index,
		u32 status),
	TP_ARGS(tstamp, intr_class, intr_type, index, status),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u8, intr_class)
		__field(u8, intr_type)
		__field(u32, index)
		__field(u32, status)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->intr_class = intr_class;
		__entry->intr_type = intr_type;
		__entry->index = index;
		__entry->status = status;
	),
	TP_printk(
		"tstamp:%llu class:%s type:%s phy:%u cil:%u st:%u vc:%u status:0x%08x",
		__entry->tstamp,
		(__entry->intr_class < g_trace_nvcsi_intr_class_str_count) ?
			g_trace_nvcsi_intr_class_strs[__entry->intr_class] :
			__print_hex(&__entry->intr_class, 1),
		(__entry->intr_type < g_trace_nvcsi_intr_type_str_count) ?
			g_trace_nvcsi_intr_type_strs[__entry->intr_type] :
			__print_hex(&__entry->intr_type, 1),
		(__entry->index >> 24) & 0xff,
		(__entry->index >> 16) & 0xff,
		(__entry->index >> 8) & 0xff,
		__entry->index & 0xff,
		__entry->status)
);

/*
 * ISP events
 */

TRACE_EVENT(rtcpu_isp_falcon,
	TP_PROTO(u8 tag, u8 ch, u8 seq, u32 tstamp, u32 data, u32 ext_data),
	TP_ARGS(tag, ch, seq, tstamp, data, ext_data),
	TP_STRUCT__entry(
		__field(u8, tag)
		__field(u8, ch)
		__field(u8, seq)
		__field(u32, tstamp)
		__field(u32, data)
		__field(u32, ext_data)
	),
	TP_fast_assign(
		__entry->tag = tag;
		__entry->ch = ch;
		__entry->seq = seq;
		__entry->tstamp = tstamp;
		__entry->data = data;
		__entry->ext_data = ext_data;
	),
	TP_printk(
		"tag:0x%x tstamp:%u ch:%u seq:%u data:0x%08x ext_data:0x%08x",
		__entry->tag, __entry->tstamp, __entry->ch, __entry->seq,
		__entry->data, __entry->ext_data
	)
);

extern const char * const g_trace_isp_falcon_task_strs[];
extern const unsigned int g_trace_isp_falcon_task_str_count;

TRACE_EVENT(rtcpu_isp_falcon_task_start,
	TP_PROTO(u8 ch, u32 tstamp, u32 task),
	TP_ARGS(ch, tstamp, task),
	TP_STRUCT__entry(
		__field(u8, ch)
		__field(u32, tstamp)
		__field(u32, task)
	),
	TP_fast_assign(
		__entry->ch = ch;
		__entry->tstamp = tstamp;
		__entry->task = task;
	),
	TP_printk(
		"tstamp:%u ch:%u task:%s",
		__entry->tstamp, __entry->ch,
		(__entry->task < g_trace_isp_falcon_task_str_count) ?
			g_trace_isp_falcon_task_strs[__entry->task] :
			"UNKNOWN"
	)
);

TRACE_EVENT(rtcpu_isp_falcon_task_end,
	TP_PROTO(u32 tstamp, u32 task),
	TP_ARGS(tstamp, task),
	TP_STRUCT__entry(
		__field(u32, tstamp)
		__field(u32, task)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->task = task;
	),
	TP_printk(
		"tstamp:%u task:%s",
		__entry->tstamp,
		(__entry->task < g_trace_isp_falcon_task_str_count) ?
			g_trace_isp_falcon_task_strs[__entry->task] :
			"UNKNOWN"
	)
);


TRACE_EVENT(rtcpu_isp_falcon_tile_start,
	TP_PROTO(
		u8 ch, u8 seq, u32 tstamp,
		u8 tile_x, u8 tile_y,
		u16 tile_w, u16 tile_h),
	TP_ARGS(ch, seq, tstamp, tile_x, tile_y, tile_w, tile_h),
	TP_STRUCT__entry(
		__field(u8, ch)
		__field(u8, seq)
		__field(u32, tstamp)
		__field(u8, tile_x)
		__field(u8, tile_y)
		__field(u16, tile_w)
		__field(u16, tile_h)

	),
	TP_fast_assign(
		__entry->ch = ch;
		__entry->seq = seq;
		__entry->tstamp = tstamp;
		__entry->tile_x = tile_x;
		__entry->tile_y = tile_y;
		__entry->tile_w = tile_w;
		__entry->tile_h = tile_h;
	),
	TP_printk(
		"tstamp:%u ch:%u seq:%u tile_x:%u tile_y:%u tile_w:%u tile_h:%u",
		__entry->tstamp, __entry->ch, __entry->seq,
		__entry->tile_x, __entry->tile_y,
		__entry->tile_w, __entry->tile_h
	)
);

TRACE_EVENT(rtcpu_isp_falcon_tile_end,
	TP_PROTO(u8 ch, u8 seq, u32 tstamp, u8 tile_x, u8 tile_y),
	TP_ARGS(ch, seq, tstamp, tile_x, tile_y),
	TP_STRUCT__entry(
		__field(u8, ch)
		__field(u8, seq)
		__field(u32, tstamp)
		__field(u8, tile_x)
		__field(u8, tile_y)

	),
	TP_fast_assign(
		__entry->ch = ch;
		__entry->seq = seq;
		__entry->tstamp = tstamp;
		__entry->tile_x = tile_x;
		__entry->tile_y = tile_y;
	),
	TP_printk(
		"tstamp:%u ch:%u seq:%u tile_x:%u tile_y:%u",
		__entry->tstamp, __entry->ch, __entry->seq,
		__entry->tile_x, __entry->tile_y
	)
);


#endif /* _TRACE_TEGRA_RTCPU_H */

#include <trace/define_trace.h>
