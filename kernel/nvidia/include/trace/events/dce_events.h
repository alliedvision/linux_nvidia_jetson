/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION, All rights reserved.
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM dce_events

#if !defined(_TRACE_DCE_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DCE_EVENTS_H

#include <dce.h>
#include <dce-ipc.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(dce_ipc_events_notifier,
	TP_PROTO(struct tegra_dce *d, struct dce_ipc_channel *ch),
	TP_ARGS(d, ch),
	TP_STRUCT__entry(
		__field_struct(struct dce_ipc_channel *,	ch)
		__field_struct(struct tegra_dce *,			d)
	),
	TP_fast_assign(
		__entry->ch = ch;
		__entry->d = d;
	),
	TP_printk("Channel Type = [%u], Flags = [0x%x], Wait Type = [%u], Write Pos = [%u], Read Pos = [%u], Frame Size = [%u], "
		  "No of Frames = [%u], Rx Iova = [0x%llx], Tx Iova = [0x%llx], Region Current Offset = [%u], Region Iova Base = [0x%llx], "
		  "Region Size = [%lu] Region Base Address = [0x%p]",
		__entry->ch->ch_type, __entry->ch->flags, __entry->ch->w_type,
		__entry->ch->d_ivc.tx.position, __entry->ch->d_ivc.rx.position,
		__entry->ch->q_info.frame_sz, __entry->ch->q_info.nframes,
		__entry->ch->q_info.rx_iova, __entry->ch->q_info.tx_iova,
		__entry->d->d_ipc.region.s_offset, __entry->d->d_ipc.region.iova,
		__entry->d->d_ipc.region.size, __entry->d->d_ipc.region.base)
);

DEFINE_EVENT(dce_ipc_events_notifier, ivc_channel_init_complete,
		TP_PROTO(struct tegra_dce *d, struct dce_ipc_channel *ch),
		TP_ARGS(d, ch)
);

DEFINE_EVENT(dce_ipc_events_notifier, ivc_channel_reset_triggered,
		TP_PROTO(struct tegra_dce *d, struct dce_ipc_channel *ch),
		TP_ARGS(d, ch)
);

DEFINE_EVENT(dce_ipc_events_notifier, ivc_channel_reset_complete,
		TP_PROTO(struct tegra_dce *d, struct dce_ipc_channel *ch),
		TP_ARGS(d, ch)
);

DEFINE_EVENT(dce_ipc_events_notifier, ivc_send_req_received,
		TP_PROTO(struct tegra_dce *d, struct dce_ipc_channel *ch),
		TP_ARGS(d, ch)
);

DEFINE_EVENT(dce_ipc_events_notifier, ivc_send_complete,
		TP_PROTO(struct tegra_dce *d, struct dce_ipc_channel *ch),
		TP_ARGS(d, ch)
);

DEFINE_EVENT(dce_ipc_events_notifier, ivc_wait_complete,
		TP_PROTO(struct tegra_dce *d, struct dce_ipc_channel *ch),
		TP_ARGS(d, ch)
);

DEFINE_EVENT(dce_ipc_events_notifier, ivc_receive_req_received,
		TP_PROTO(struct tegra_dce *d, struct dce_ipc_channel *ch),
		TP_ARGS(d, ch)
);

DEFINE_EVENT(dce_ipc_events_notifier, ivc_receive_req_complete,
		TP_PROTO(struct tegra_dce *d, struct dce_ipc_channel *ch),
		TP_ARGS(d, ch)
);

#endif /* _TRACE_DCE_EVENTS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
