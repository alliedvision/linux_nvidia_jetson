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
#include <nvgpu/gk20a.h>
#include <nvgpu/timers.h>
#include <nvgpu/ipa_pa_cache.h>

static u64 nvgpu_ipa_to_pa_cache_lookup(struct gk20a *g, u64 ipa,
				u64 *pa_len)
{
	struct nvgpu_ipa_desc *desc;
	struct nvgpu_ipa_pa_cache *ipa_cache;
	u32 i = 0U;
	u64 pa = 0UL;

	ipa_cache = &(g->ipa_pa_cache);
	desc = &(ipa_cache->ipa[0]);

	for (i = 0; i < ipa_cache->num_ipa_desc; ++i, ++desc) {
		if (ipa >= desc->ipa_base &&
				(ipa < (desc->ipa_base + desc->ipa_size))) {
			pa = ipa - desc->ipa_base + desc->pa_base;
			if (pa_len != NULL) {
				*pa_len = desc->ipa_size -(ipa - desc->ipa_base);
			}

			return pa;
		}
	}

	return 0U;
}

u64 nvgpu_ipa_to_pa_cache_lookup_locked(struct gk20a *g, u64 ipa,
		u64 *pa_len)
{
	struct nvgpu_ipa_pa_cache *ipa_cache;
	u64 pa = 0UL;

	ipa_cache = &(g->ipa_pa_cache);
	nvgpu_rwsem_down_read(&(ipa_cache->ipa_pa_rw_lock));
	pa = nvgpu_ipa_to_pa_cache_lookup(g, ipa, pa_len);
	nvgpu_rwsem_up_read(&(ipa_cache->ipa_pa_rw_lock));
	return pa;
}

void nvgpu_ipa_to_pa_add_to_cache(struct gk20a *g, u64 ipa, u64 pa,
				struct nvgpu_hyp_ipa_pa_info *info)
{
	struct nvgpu_ipa_pa_cache *ipa_cache;
	struct nvgpu_ipa_desc *desc = NULL;
	u64 pa_cached = 0U;

	ipa_cache = &(g->ipa_pa_cache);
	nvgpu_rwsem_down_write(&(ipa_cache->ipa_pa_rw_lock));
	pa_cached = nvgpu_ipa_to_pa_cache_lookup(g, ipa, NULL);
	if (pa_cached != 0UL) {
		/* Check any other context insert the translation
		 * already and return.
		 */
		nvgpu_assert(pa_cached == pa);
		nvgpu_rwsem_up_write(&(ipa_cache->ipa_pa_rw_lock));
		return;
	}

        if (ipa_cache->num_ipa_desc >= MAX_IPA_PA_CACHE) {
                desc = &ipa_cache->ipa[nvgpu_current_time_ns() %
			MAX_IPA_PA_CACHE];
        } else {
                desc = &ipa_cache->ipa[ipa_cache->num_ipa_desc++];
        }

	desc->ipa_base = ipa - info->offset;
        desc->ipa_size = info->size;
        desc->pa_base = info->base;
	nvgpu_rwsem_up_write(&(ipa_cache->ipa_pa_rw_lock));

}
