/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <nvgpu/trace.h>

#include <nvgpu/gk20a.h>


void nvgpu_trace_intr_thread_stall_start(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_TRACE
	trace_mc_gk20a_intr_thread_stall(g->name);
#endif
}

void nvgpu_trace_intr_thread_stall_done(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_TRACE
	trace_mc_gk20a_intr_thread_stall_done(g->name);
#endif
}

void nvgpu_trace_intr_stall_start(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_TRACE
	trace_mc_gk20a_intr_stall(g->name);
#endif
}

void nvgpu_trace_intr_stall_done(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_TRACE
	trace_mc_gk20a_intr_stall_done(g->name);
#endif
}
