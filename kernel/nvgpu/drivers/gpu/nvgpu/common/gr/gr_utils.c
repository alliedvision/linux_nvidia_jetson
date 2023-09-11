/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/types.h>
#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/gr/gr_instances.h>

#include <nvgpu/gr/config.h>

#include "gr_priv.h"

u32 nvgpu_gr_checksum_u32(u32 a, u32 b)
{
	return nvgpu_safe_cast_u64_to_u32(((u64)a + (u64)b) & (U32_MAX));
}

struct nvgpu_gr_falcon *nvgpu_gr_get_falcon_ptr(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	return gr->falcon;
}

struct nvgpu_gr_config *nvgpu_gr_get_config_ptr(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	return gr->config;
}

struct nvgpu_gr_config *nvgpu_gr_get_gr_instance_config_ptr(struct gk20a *g,
		u32 gr_instance_id)
{
	return g->gr[gr_instance_id].config;
}

struct nvgpu_gr_intr *nvgpu_gr_get_intr_ptr(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	return gr->intr;
}

#ifdef CONFIG_NVGPU_NON_FUSA
u32 nvgpu_gr_get_override_ecc_val(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	return gr->fecs_feature_override_ecc_val;
}

void nvgpu_gr_override_ecc_val(struct nvgpu_gr *gr, u32 ecc_val)
{
	gr->fecs_feature_override_ecc_val = ecc_val;
}
#endif

#ifdef CONFIG_NVGPU_GRAPHICS
struct nvgpu_gr_zcull *nvgpu_gr_get_zcull_ptr(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	return gr->zcull;
}

struct nvgpu_gr_zbc *nvgpu_gr_get_zbc_ptr(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	return gr->zbc;
}
#endif

#ifdef CONFIG_NVGPU_FECS_TRACE
struct nvgpu_gr_global_ctx_buffer_desc *nvgpu_gr_get_global_ctx_buffer_ptr(
							struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	return gr->global_ctx_buffer;
}
#endif

#ifdef CONFIG_NVGPU_CILP
u32 nvgpu_gr_get_cilp_preempt_pending_chid(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	return gr->cilp_preempt_pending_chid;
}

void nvgpu_gr_clear_cilp_preempt_pending_chid(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	gr->cilp_preempt_pending_chid =
				NVGPU_INVALID_CHANNEL_ID;
}
#endif

struct nvgpu_gr_obj_ctx_golden_image *nvgpu_gr_get_golden_image_ptr(
		struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	return gr->golden_image;
}

#ifdef CONFIG_NVGPU_DEBUGGER
struct nvgpu_gr_hwpm_map *nvgpu_gr_get_hwpm_map_ptr(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	return gr->hwpm_map;
}

void nvgpu_gr_reset_falcon_ptr(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	gr->falcon = NULL;
}

void nvgpu_gr_reset_golden_image_ptr(struct gk20a *g)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	gr->golden_image = NULL;
}
#endif
