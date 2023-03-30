/*
 * Tegra GK20A GPU Debugger Driver
 *
 * Copyright (c) 2013-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef NVGPU_DEBUGGER_H
#define NVGPU_DEBUGGER_H

#ifdef CONFIG_NVGPU_DEBUGGER

#include <nvgpu/cond.h>
#include <nvgpu/lock.h>
#include <nvgpu/list.h>

struct gk20a;
struct nvgpu_channel;
struct dbg_session_gk20a;
struct nvgpu_profiler_object;

struct nvgpu_channel *
nvgpu_dbg_gpu_get_session_channel(struct dbg_session_gk20a *dbg_s);

struct dbg_gpu_session_events {
	struct nvgpu_cond wait_queue;
	bool events_enabled;
	int num_pending_events;
};

struct dbg_session_gk20a {
	/* dbg session id used for trace/prints */
	int id;

	/* profiler session, if any */
	bool is_profiler;

	/* power enabled or disabled */
	bool is_pg_disabled;

	/* timeouts enabled or disabled */
	bool is_timeout_disabled;

	struct gk20a              *g;

	/* list of bound channels, if any */
	struct nvgpu_list_node ch_list;
	struct nvgpu_mutex ch_list_lock;

	/* event support */
	struct dbg_gpu_session_events dbg_events;

	bool broadcast_stop_trigger;

	struct nvgpu_mutex ioctl_lock;

	/*
	 * Dummy profiler object for debug session to synchronize PMA
	 * reservation and HWPM system reset with new context/device
	 * profilers.
	 */
	struct nvgpu_profiler_object *prof;

	/** GPU instance Id */
	u32 gpu_instance_id;
};

struct dbg_session_data {
	struct dbg_session_gk20a *dbg_s;
	struct nvgpu_list_node dbg_s_entry;
};

static inline struct dbg_session_data *
dbg_session_data_from_dbg_s_entry(struct nvgpu_list_node *node)
{
	return (struct dbg_session_data *)
	     ((uintptr_t)node - offsetof(struct dbg_session_data, dbg_s_entry));
};

struct dbg_session_channel_data {
	int channel_fd;
	u32 chid;
	struct nvgpu_list_node ch_entry;
	struct dbg_session_data *session_data;
	int (*unbind_single_channel)(struct dbg_session_gk20a *dbg_s,
			struct dbg_session_channel_data *ch_data);
};

static inline struct dbg_session_channel_data *
dbg_session_channel_data_from_ch_entry(struct nvgpu_list_node *node)
{
	return (struct dbg_session_channel_data *)
	((uintptr_t)node - offsetof(struct dbg_session_channel_data, ch_entry));
};

/* used by the interrupt handler to post events */
void nvgpu_dbg_gpu_post_events(struct nvgpu_channel *ch);

bool nvgpu_dbg_gpu_broadcast_stop_trigger(struct nvgpu_channel *ch);
void nvgpu_dbg_gpu_clear_broadcast_stop_trigger(struct nvgpu_channel *ch);

int nvgpu_dbg_set_powergate(struct dbg_session_gk20a *dbg_s, bool disable_powergate);

void nvgpu_dbg_session_post_event(struct dbg_session_gk20a *dbg_s);
u32 nvgpu_set_powergate_locked(struct dbg_session_gk20a *dbg_s,
				bool mode);

#endif /* CONFIG_NVGPU_DEBUGGER */
#endif /* NVGPU_DEBUGGER_H */
