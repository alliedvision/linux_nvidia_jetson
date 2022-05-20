/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_POSIX_TRACE_GK20A_H
#define NVGPU_POSIX_TRACE_GK20A_H

#include <nvgpu/types.h>

static inline void trace_gk20a_mm_fb_flush(const char *name){}

static inline void trace_gk20a_mm_fb_flush_done(const char *name){}

static inline void trace_gk20a_mm_l2_invalidate(const char *name){}

static inline void trace_gk20a_mm_l2_invalidate_done(const char *name){}

static inline void trace_gk20a_mm_l2_flush(const char *name){}

static inline void trace_gk20a_mm_l2_flush_done(const char *name){}

static inline void trace_nvgpu_channel_open_new(int chid){}
static inline void trace_gk20a_release_used_channel(int chid){}
static inline void trace_nvgpu_channel_get(u32 chid, const char *caller){}
static inline void trace_nvgpu_channel_put(u32 chid, const char *caller){}
static inline void trace_gk20a_free_channel(int chid){}
static inline void trace_nvgpu_channel_update(int chid){}
static inline void trace_gk20a_mmu_fault(u64 fault_addr,
		     u32 fault_type,
		     u32 access_type,
		     u64 inst_ptr,
		     u32 engine_id,
		     const char *client_type_desc,
		     const char *client_id_desc,
		     const char *fault_type_desc){}
#ifdef CONFIG_NVGPU_COMPRESSION
static inline void trace_gk20a_ltc_cbc_ctrl_start(const char *name,
		u32 cbc_ctrl, u32 min_value, u32 max_value) {}
static inline void trace_gk20a_ltc_cbc_ctrl_done(const char *name) {}
#endif
static inline void trace_gk20a_mm_tlb_invalidate(const char *name) {}
static inline void trace_gk20a_mm_tlb_invalidate_done(const char *name) {}
static inline void trace_gk20a_channel_reset(u32 chid, u32 tsgid) {}

static inline void trace_gk20a_channel_submit_gpfifo(const char *name,
		u32 chid,
		u32 num_entries,
		u32 flags,
		u32 wait_id,
		u32 wait_value) {}
static inline void trace_gk20a_channel_submitted_gpfifo(const char *name,
		u32 chid,
		u32 num_entries,
		u32 flags,
		u32 incr_id,
		u32 incr_value) {}
static inline void trace_gk20a_push_cmdbuf(const char *name,
		u32 mem_id,
		u32 words,
		u32 offset,
		void *cmdbuf) {}
#endif
