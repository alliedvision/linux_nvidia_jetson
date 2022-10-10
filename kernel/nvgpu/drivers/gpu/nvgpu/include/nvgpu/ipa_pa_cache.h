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

#ifndef NVGPU_IPAPACACHE_H
#define NVGPU_IPAPACACHE_H

#include <nvgpu/rwsem.h>

struct nvgpu_hyp_ipa_pa_info {
	u64 base;
	u64 offset;
	u64 size;
};

struct gk20a;

#define MAX_IPA_PA_CACHE 256U

struct nvgpu_ipa_desc {
	u64 ipa_base;
	u64 ipa_size;
	u64 pa_base;
};

struct nvgpu_ipa_pa_cache {
	struct nvgpu_rwsem ipa_pa_rw_lock;
	struct nvgpu_ipa_desc ipa[MAX_IPA_PA_CACHE];
	u32 num_ipa_desc;
};

u64 nvgpu_ipa_to_pa_cache_lookup_locked(struct gk20a *g, u64 ipa,
		u64 *pa_len);

void nvgpu_ipa_to_pa_add_to_cache(struct gk20a *g, u64 ipa,
		u64 pa, struct nvgpu_hyp_ipa_pa_info *info);
#endif /* NVGPU_IPAPACACHE_H */
