/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PROFILER_VGPU_H
#define NVGPU_PROFILER_VGPU_H

int vgpu_profiler_bind_hwpm(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg);

int vgpu_profiler_unbind_hwpm(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg);

int vgpu_profiler_bind_hwpm_streamout(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg,
		u64 pma_buffer_va,
		u32 pma_buffer_size,
		u64 pma_bytes_available_buffer_va);

int vgpu_profiler_unbind_hwpm_streamout(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg,
		void *pma_bytes_available_buffer_cpuva,
		bool smpc_reserved);

int vgpu_profiler_bind_smpc(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg);

int vgpu_profiler_unbind_smpc(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg);

#endif
