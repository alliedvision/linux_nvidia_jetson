/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/global_ctx.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/vm.h>
#include <nvgpu/io.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/dma.h>
#include <nvgpu/string.h>

#include <nvgpu/power_features/pg.h>
#include "common/gr/ctx_priv.h"

static void nvgpu_gr_ctx_unmap_global_ctx_buffers(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm);

struct nvgpu_gr_ctx_desc *
nvgpu_gr_ctx_desc_alloc(struct gk20a *g)
{
	struct nvgpu_gr_ctx_desc *desc = nvgpu_kzalloc(g, sizeof(*desc));
	return desc;
}

void nvgpu_gr_ctx_desc_free(struct gk20a *g,
	struct nvgpu_gr_ctx_desc *desc)
{
	nvgpu_kfree(g, desc);
}

void nvgpu_gr_ctx_set_size(struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	u32 index, u32 size)
{
	nvgpu_assert(index < NVGPU_GR_CTX_COUNT);
	gr_ctx_desc->size[index] = size;
}

struct nvgpu_gr_ctx *nvgpu_alloc_gr_ctx_struct(struct gk20a *g)
{
	return nvgpu_kzalloc(g, sizeof(struct nvgpu_gr_ctx));
}

void nvgpu_free_gr_ctx_struct(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx)
{
	nvgpu_kfree(g, gr_ctx);
}

int nvgpu_gr_ctx_alloc(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct vm_gk20a *vm)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (gr_ctx_desc->size[NVGPU_GR_CTX_CTX] == 0U) {
		return -EINVAL;
	}

	err = nvgpu_dma_alloc(g, gr_ctx_desc->size[NVGPU_GR_CTX_CTX],
			&gr_ctx->mem);
	if (err != 0) {
		return err;
	}

	gr_ctx->mem.gpu_va = nvgpu_gmmu_map(vm,
					&gr_ctx->mem,
					0, /* not GPU-cacheable */
					gk20a_mem_flag_none, true,
					gr_ctx->mem.aperture);
	if (gr_ctx->mem.gpu_va == 0ULL) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	gr_ctx->ctx_id_valid = false;

	return 0;

err_free_mem:
	nvgpu_dma_free(g, &gr_ctx->mem);

	return err;
}

void nvgpu_gr_ctx_free(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm)
{
	nvgpu_log_fn(g, " ");

	if (gr_ctx != NULL) {
		nvgpu_gr_ctx_unmap_global_ctx_buffers(g, gr_ctx,
			global_ctx_buffer, vm);

#ifdef CONFIG_NVGPU_DEBUGGER
		nvgpu_gr_ctx_free_pm_ctx(g, vm, gr_ctx);
#endif
		nvgpu_gr_ctx_free_patch_ctx(g, vm, gr_ctx);
#ifdef CONFIG_NVGPU_GFXP
		if (nvgpu_mem_is_valid(&gr_ctx->gfxp_rtvcb_ctxsw_buffer)) {
			nvgpu_dma_unmap_free(vm,
				&gr_ctx->gfxp_rtvcb_ctxsw_buffer);
		}
		nvgpu_dma_unmap_free(vm, &gr_ctx->pagepool_ctxsw_buffer);
		nvgpu_dma_unmap_free(vm, &gr_ctx->betacb_ctxsw_buffer);
		nvgpu_dma_unmap_free(vm, &gr_ctx->spill_ctxsw_buffer);
		nvgpu_dma_unmap_free(vm, &gr_ctx->preempt_ctxsw_buffer);
#endif

		nvgpu_dma_unmap_free(vm, &gr_ctx->mem);
		(void) memset(gr_ctx, 0, sizeof(*gr_ctx));
	}
}

int nvgpu_gr_ctx_alloc_patch_ctx(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct vm_gk20a *vm)
{
	struct patch_desc *patch_ctx = &gr_ctx->patch_ctx;
	int err = 0;

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "patch_ctx size = %u",
		gr_ctx_desc->size[NVGPU_GR_CTX_PATCH_CTX]);

	err = nvgpu_dma_alloc_map_sys(vm, gr_ctx_desc->size[NVGPU_GR_CTX_PATCH_CTX],
			&patch_ctx->mem);
	if (err != 0) {
		return err;
	}

	return 0;
}

void nvgpu_gr_ctx_free_patch_ctx(struct gk20a *g, struct vm_gk20a *vm,
	struct nvgpu_gr_ctx *gr_ctx)
{
	struct patch_desc *patch_ctx = &gr_ctx->patch_ctx;

	(void)g;

	if (nvgpu_mem_is_valid(&patch_ctx->mem)) {
		nvgpu_dma_unmap_free(vm, &patch_ctx->mem);
		patch_ctx->data_count = 0;
	}
}

static void nvgpu_gr_ctx_unmap_global_ctx_buffers(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm)
{
	u64 *g_bfr_va = &gr_ctx->global_ctx_buffer_va[0];
	u32 *g_bfr_index = &gr_ctx->global_ctx_buffer_index[0];
	u32 i;

	nvgpu_log_fn(g, " ");

	for (i = 0U; i < NVGPU_GR_CTX_VA_COUNT; i++) {
		if (g_bfr_va[i] != 0ULL) {
			nvgpu_gr_global_ctx_buffer_unmap(global_ctx_buffer,
				g_bfr_index[i], vm, g_bfr_va[i]);
		}
	}

	(void) memset(g_bfr_va, 0, sizeof(gr_ctx->global_ctx_buffer_va));
	(void) memset(g_bfr_index, 0, sizeof(gr_ctx->global_ctx_buffer_index));

	gr_ctx->global_ctx_buffer_mapped = false;
}

static int nvgpu_gr_ctx_map_ctx_circular_buffer(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm, bool vpr)
{
	u64 *g_bfr_va;
	u32 *g_bfr_index;
	u64 gpu_va = 0ULL;

	(void)g;
	(void)vpr;

	g_bfr_va = &gr_ctx->global_ctx_buffer_va[0];
	g_bfr_index = &gr_ctx->global_ctx_buffer_index[0];

#ifdef CONFIG_NVGPU_VPR
	if (vpr && nvgpu_gr_global_ctx_buffer_ready(global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_CIRCULAR_VPR)) {
		gpu_va = nvgpu_gr_global_ctx_buffer_map(global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_CIRCULAR_VPR,
					vm, NVGPU_VM_MAP_CACHEABLE, true);
		g_bfr_index[NVGPU_GR_CTX_CIRCULAR_VA] =
					NVGPU_GR_GLOBAL_CTX_CIRCULAR_VPR;
	} else {
#endif
		gpu_va = nvgpu_gr_global_ctx_buffer_map(global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_CIRCULAR,
					vm, NVGPU_VM_MAP_CACHEABLE, true);
		g_bfr_index[NVGPU_GR_CTX_CIRCULAR_VA] =
					NVGPU_GR_GLOBAL_CTX_CIRCULAR;
#ifdef CONFIG_NVGPU_VPR
	}
#endif
	if (gpu_va == 0ULL) {
		goto clean_up;
	}
	g_bfr_va[NVGPU_GR_CTX_CIRCULAR_VA] = gpu_va;

	return 0;

clean_up:
	return -ENOMEM;
}

static int nvgpu_gr_ctx_map_ctx_attribute_buffer(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm, bool vpr)
{
	u64 *g_bfr_va;
	u32 *g_bfr_index;
	u64 gpu_va = 0ULL;

	(void)g;
	(void)vpr;

	g_bfr_va = &gr_ctx->global_ctx_buffer_va[0];
	g_bfr_index = &gr_ctx->global_ctx_buffer_index[0];

#ifdef CONFIG_NVGPU_VPR
	if (vpr && nvgpu_gr_global_ctx_buffer_ready(global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_ATTRIBUTE_VPR)) {
		gpu_va = nvgpu_gr_global_ctx_buffer_map(global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_ATTRIBUTE_VPR,
					vm, NVGPU_VM_MAP_CACHEABLE, false);
		g_bfr_index[NVGPU_GR_CTX_ATTRIBUTE_VA] =
					NVGPU_GR_GLOBAL_CTX_ATTRIBUTE_VPR;
	} else {
#endif
		gpu_va = nvgpu_gr_global_ctx_buffer_map(global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_ATTRIBUTE,
					vm, NVGPU_VM_MAP_CACHEABLE, false);
		g_bfr_index[NVGPU_GR_CTX_ATTRIBUTE_VA] =
					NVGPU_GR_GLOBAL_CTX_ATTRIBUTE;
#ifdef CONFIG_NVGPU_VPR
	}
#endif
	if (gpu_va == 0ULL) {
		goto clean_up;
	}
	g_bfr_va[NVGPU_GR_CTX_ATTRIBUTE_VA] = gpu_va;

	return 0;

clean_up:
	return -ENOMEM;
}


static int nvgpu_gr_ctx_map_ctx_pagepool_buffer(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm, bool vpr)
{
	u64 *g_bfr_va;
	u32 *g_bfr_index;
	u64 gpu_va = 0ULL;

	(void)g;
	(void)vpr;

	g_bfr_va = &gr_ctx->global_ctx_buffer_va[0];
	g_bfr_index = &gr_ctx->global_ctx_buffer_index[0];

#ifdef CONFIG_NVGPU_VPR
	if (vpr && nvgpu_gr_global_ctx_buffer_ready(global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_PAGEPOOL_VPR)) {
		gpu_va = nvgpu_gr_global_ctx_buffer_map(global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_PAGEPOOL_VPR,
					vm, NVGPU_VM_MAP_CACHEABLE, true);
		g_bfr_index[NVGPU_GR_CTX_PAGEPOOL_VA] =
					NVGPU_GR_GLOBAL_CTX_PAGEPOOL_VPR;
	} else {
#endif
		gpu_va = nvgpu_gr_global_ctx_buffer_map(global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_PAGEPOOL,
					vm, NVGPU_VM_MAP_CACHEABLE, true);
		g_bfr_index[NVGPU_GR_CTX_PAGEPOOL_VA] =
					NVGPU_GR_GLOBAL_CTX_PAGEPOOL;
#ifdef CONFIG_NVGPU_VPR
	}
#endif
	if (gpu_va == 0ULL) {
		goto clean_up;
	}
	g_bfr_va[NVGPU_GR_CTX_PAGEPOOL_VA] = gpu_va;

	return 0;

clean_up:
	return -ENOMEM;
}

static int nvgpu_gr_ctx_map_ctx_buffer(struct gk20a *g,
	u32 buffer_type, u32 va_type,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm)
{
	u64 *g_bfr_va;
	u32 *g_bfr_index;
	u64 gpu_va = 0ULL;

	(void)g;

	g_bfr_va = &gr_ctx->global_ctx_buffer_va[0];
	g_bfr_index = &gr_ctx->global_ctx_buffer_index[0];

	gpu_va = nvgpu_gr_global_ctx_buffer_map(global_ctx_buffer,
			buffer_type, vm, 0, true);
	if (gpu_va == 0ULL) {
		goto clean_up;
	}

	g_bfr_index[va_type] = buffer_type;
	g_bfr_va[va_type] = gpu_va;

	return 0;

clean_up:
	return -ENOMEM;
}

int nvgpu_gr_ctx_map_global_ctx_buffers(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm, bool vpr)
{
	int err;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	/*
	 * MIG supports only compute class.
	 * Allocate BUNDLE_CB, PAGEPOOL, ATTRIBUTE_CB and RTV_CB
	 * if 2D/3D/I2M classes(graphics) are supported.
	 */
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		/* Circular Buffer */
		err = nvgpu_gr_ctx_map_ctx_circular_buffer(g, gr_ctx,
						global_ctx_buffer, vm, vpr);
		if (err != 0) {
			nvgpu_err(g, "cannot map ctx circular buffer");
			goto fail;
		}

		/* Attribute Buffer */
		err = nvgpu_gr_ctx_map_ctx_attribute_buffer(g, gr_ctx,
						global_ctx_buffer, vm, vpr);
		if (err != 0) {
			nvgpu_err(g, "cannot map ctx attribute buffer");
			goto fail;
		}

		/* Page Pool */
		err = nvgpu_gr_ctx_map_ctx_pagepool_buffer(g, gr_ctx,
						global_ctx_buffer, vm, vpr);
		if (err != 0) {
			nvgpu_err(g, "cannot map ctx pagepool buffer");
			goto fail;
		}
#ifdef CONFIG_NVGPU_GRAPHICS
		/* RTV circular buffer */
		if (nvgpu_gr_global_ctx_buffer_ready(global_ctx_buffer,
				NVGPU_GR_GLOBAL_CTX_RTV_CIRCULAR_BUFFER)) {
			err  = nvgpu_gr_ctx_map_ctx_buffer(g,
					NVGPU_GR_GLOBAL_CTX_RTV_CIRCULAR_BUFFER,
					NVGPU_GR_CTX_RTV_CIRCULAR_BUFFER_VA,
					gr_ctx, global_ctx_buffer, vm);
			if (err != 0) {
				nvgpu_err(g,
					"cannot map ctx rtv circular buffer");
				goto fail;
			}
		}
#endif
	}

	/* Priv register Access Map */
	err  = nvgpu_gr_ctx_map_ctx_buffer(g,
			NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP,
			NVGPU_GR_CTX_PRIV_ACCESS_MAP_VA,
			gr_ctx, global_ctx_buffer, vm);
	if (err != 0) {
		nvgpu_err(g, "cannot map ctx priv access buffer");
		goto fail;
	}

#ifdef CONFIG_NVGPU_FECS_TRACE
	/* FECS trace buffer */
	if (nvgpu_is_enabled(g, NVGPU_FECS_TRACE_VA)) {
		err  = nvgpu_gr_ctx_map_ctx_buffer(g,
			NVGPU_GR_GLOBAL_CTX_FECS_TRACE_BUFFER,
			NVGPU_GR_CTX_FECS_TRACE_BUFFER_VA,
			gr_ctx, global_ctx_buffer, vm);
		if (err != 0) {
			nvgpu_err(g, "cannot map ctx fecs trace buffer");
			goto fail;
		}
	}
#endif

	gr_ctx->global_ctx_buffer_mapped = true;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
	return 0;

fail:
	nvgpu_gr_ctx_unmap_global_ctx_buffers(g, gr_ctx, global_ctx_buffer, vm);
	return err;
}

u64 nvgpu_gr_ctx_get_global_ctx_va(struct nvgpu_gr_ctx *gr_ctx,
	u32 index)
{
	return gr_ctx->global_ctx_buffer_va[index];
}

struct nvgpu_mem *nvgpu_gr_ctx_get_patch_ctx_mem(struct nvgpu_gr_ctx *gr_ctx)
{
	return &gr_ctx->patch_ctx.mem;
}

void nvgpu_gr_ctx_set_patch_ctx_data_count(struct nvgpu_gr_ctx *gr_ctx,
	u32 data_count)
{
	gr_ctx->patch_ctx.data_count = data_count;
}

struct nvgpu_mem *nvgpu_gr_ctx_get_ctx_mem(struct nvgpu_gr_ctx *gr_ctx)
{
	return &gr_ctx->mem;
}

#ifdef CONFIG_NVGPU_SM_DIVERSITY
void nvgpu_gr_ctx_set_sm_diversity_config(struct nvgpu_gr_ctx *gr_ctx,
	u32 sm_diversity_config)
{
	gr_ctx->sm_diversity_config = sm_diversity_config;
}

u32 nvgpu_gr_ctx_get_sm_diversity_config(struct nvgpu_gr_ctx *gr_ctx)
{
	return gr_ctx->sm_diversity_config;
}
#endif

/* load saved fresh copy of gloden image into channel gr_ctx */
void nvgpu_gr_ctx_load_golden_ctx_image(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image,
	bool cde)
{
	struct nvgpu_mem *mem;
#ifdef CONFIG_NVGPU_DEBUGGER
	u64 virt_addr = 0;
#endif

	(void)cde;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	mem = &gr_ctx->mem;

	nvgpu_gr_global_ctx_load_local_golden_image(g,
		local_golden_image, mem);

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	g->ops.gr.ctxsw_prog.init_ctxsw_hdr_data(g, mem);
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
	if ((g->ops.gr.ctxsw_prog.set_cde_enabled != NULL) && cde) {
		g->ops.gr.ctxsw_prog.set_cde_enabled(g, mem);
	}
#endif

#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
	/* set priv access map */
	g->ops.gr.ctxsw_prog.set_priv_access_map_config_mode(g, mem,
		g->allow_all);
	g->ops.gr.ctxsw_prog.set_priv_access_map_addr(g, mem,
		nvgpu_gr_ctx_get_global_ctx_va(gr_ctx,
			NVGPU_GR_CTX_PRIV_ACCESS_MAP_VA));
#endif

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	/* disable verif features */
	g->ops.gr.ctxsw_prog.disable_verif_features(g, mem);
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
	if (g->ops.gr.ctxsw_prog.set_pmu_options_boost_clock_frequencies !=
			NULL) {
		g->ops.gr.ctxsw_prog.set_pmu_options_boost_clock_frequencies(g,
			mem, nvgpu_safe_cast_bool_to_u32(gr_ctx->boosted_ctx));
	}
#endif

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_gr, "write patch count = %d",
			gr_ctx->patch_ctx.data_count);
	g->ops.gr.ctxsw_prog.set_patch_count(g, mem,
		gr_ctx->patch_ctx.data_count);
	g->ops.gr.ctxsw_prog.set_patch_addr(g, mem,
		gr_ctx->patch_ctx.mem.gpu_va);

#ifdef CONFIG_NVGPU_DEBUGGER
	/* PM ctxt switch is off by default */
	gr_ctx->pm_ctx.pm_mode =
		g->ops.gr.ctxsw_prog.hw_get_pm_mode_no_ctxsw();
	virt_addr = 0;

	g->ops.gr.ctxsw_prog.set_pm_mode(g, mem, gr_ctx->pm_ctx.pm_mode);
	g->ops.gr.ctxsw_prog.set_pm_ptr(g, mem, virt_addr);
#endif

	nvgpu_log(g, gpu_dbg_gr, "done");
}

/*
 * Context state can be written directly, or "patched" at times. So that code
 * can be used in either situation it is written using a series of
 * _ctx_patch_write(..., patch) statements. However any necessary map overhead
 * should be minimized; thus, bundle the sequence of these writes together, and
 * set them up and close with _ctx_patch_write_begin/_ctx_patch_write_end.
 */
void nvgpu_gr_ctx_patch_write_begin(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	bool update_patch_count)
{
	if (update_patch_count) {
		/* reset patch count if ucode has already processed it */
		gr_ctx->patch_ctx.data_count =
			g->ops.gr.ctxsw_prog.get_patch_count(g, &gr_ctx->mem);
		nvgpu_log(g, gpu_dbg_info, "patch count reset to %d",
					gr_ctx->patch_ctx.data_count);
	}
}

void nvgpu_gr_ctx_patch_write_end(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	bool update_patch_count)
{
	/* Write context count to context image if it is mapped */
	if (update_patch_count) {
		g->ops.gr.ctxsw_prog.set_patch_count(g, &gr_ctx->mem,
			     gr_ctx->patch_ctx.data_count);
		nvgpu_log(g, gpu_dbg_info, "write patch count %d",
			gr_ctx->patch_ctx.data_count);
	}
}

void nvgpu_gr_ctx_patch_write(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	u32 addr, u32 data, bool patch)
{
	if (patch) {
		u32 patch_slot;
		u64 patch_slot_max;

		if (gr_ctx == NULL) {
			nvgpu_err(g,
				"failed to access gr_ctx[NULL] but patch true");
			return;
		}

		patch_slot =
			nvgpu_safe_mult_u32(gr_ctx->patch_ctx.data_count,
					PATCH_CTX_SLOTS_REQUIRED_PER_ENTRY);
		patch_slot_max =
			nvgpu_safe_sub_u64(
				PATCH_CTX_ENTRIES_FROM_SIZE(
					gr_ctx->patch_ctx.mem.size),
					PATCH_CTX_SLOTS_REQUIRED_PER_ENTRY);

		if (patch_slot > patch_slot_max) {
			nvgpu_err(g, "failed to access patch_slot %d",
				patch_slot);
			return;
		}

		nvgpu_mem_wr32(g, &gr_ctx->patch_ctx.mem,
						(u64)patch_slot, addr);
		nvgpu_mem_wr32(g, &gr_ctx->patch_ctx.mem,
						(u64)patch_slot + 1ULL, data);
		gr_ctx->patch_ctx.data_count = nvgpu_safe_add_u32(
						gr_ctx->patch_ctx.data_count, 1U);
		nvgpu_log(g, gpu_dbg_info,
			"patch addr = 0x%x data = 0x%x data_count %d",
			addr, data, gr_ctx->patch_ctx.data_count);
	} else {
		nvgpu_writel(g, addr, data);
	}
}

void nvgpu_gr_ctx_init_compute_preemption_mode(struct nvgpu_gr_ctx *gr_ctx,
	u32 compute_preempt_mode)
{
	gr_ctx->compute_preempt_mode = compute_preempt_mode;
}

u32 nvgpu_gr_ctx_get_compute_preemption_mode(struct nvgpu_gr_ctx *gr_ctx)
{
	return gr_ctx->compute_preempt_mode;
}

bool nvgpu_gr_ctx_check_valid_preemption_mode(struct gk20a *g,
		struct nvgpu_gr_ctx *gr_ctx,
		u32 graphics_preempt_mode, u32 compute_preempt_mode)
{
	u32 supported_graphics_preempt_mode = 0U;
	u32 supported_compute_preempt_mode = 0U;

	if ((graphics_preempt_mode == 0U) && (compute_preempt_mode == 0U)) {
		return false;
	}

	g->ops.gr.init.get_supported__preemption_modes(
			&supported_graphics_preempt_mode,
			&supported_compute_preempt_mode);

	if (graphics_preempt_mode != 0U) {
		if ((graphics_preempt_mode & supported_graphics_preempt_mode) == 0U) {
			return false;
		}

		/* Do not allow lower preemption modes than current ones */
		if (graphics_preempt_mode < gr_ctx->graphics_preempt_mode) {
			return false;
		}
	}

	if (compute_preempt_mode != 0U) {
		if ((compute_preempt_mode & supported_compute_preempt_mode) == 0U) {
			return false;
		}

		/* Do not allow lower preemption modes than current ones */
		if (compute_preempt_mode < gr_ctx->compute_preempt_mode) {
			return false;
		}
	}

#if defined(CONFIG_NVGPU_CILP) && defined(CONFIG_NVGPU_GFXP)
	/* Invalid combination */
	if ((graphics_preempt_mode == NVGPU_PREEMPTION_MODE_GRAPHICS_GFXP) &&
		   (compute_preempt_mode == NVGPU_PREEMPTION_MODE_COMPUTE_CILP)) {
		return false;
	}
#endif

	return true;
}

void nvgpu_gr_ctx_set_preemption_modes(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx)
{
#ifdef CONFIG_NVGPU_GFXP
	if (gr_ctx->graphics_preempt_mode == NVGPU_PREEMPTION_MODE_GRAPHICS_GFXP) {
		g->ops.gr.ctxsw_prog.set_graphics_preemption_mode_gfxp(g,
			&gr_ctx->mem);
	}
#endif

#ifdef CONFIG_NVGPU_CILP
	if (gr_ctx->compute_preempt_mode == NVGPU_PREEMPTION_MODE_COMPUTE_CILP) {
		g->ops.gr.ctxsw_prog.set_compute_preemption_mode_cilp(g,
			&gr_ctx->mem);
	}
#endif

	if (gr_ctx->compute_preempt_mode == NVGPU_PREEMPTION_MODE_COMPUTE_CTA) {
		g->ops.gr.ctxsw_prog.set_compute_preemption_mode_cta(g,
			&gr_ctx->mem);
	}

}

void nvgpu_gr_ctx_set_tsgid(struct nvgpu_gr_ctx *gr_ctx, u32 tsgid)
{
	gr_ctx->tsgid = tsgid;
}

u32 nvgpu_gr_ctx_get_tsgid(struct nvgpu_gr_ctx *gr_ctx)
{
	return gr_ctx->tsgid;
}

#ifdef CONFIG_NVGPU_GRAPHICS
void nvgpu_gr_ctx_init_graphics_preemption_mode(struct nvgpu_gr_ctx *gr_ctx,
	u32 graphics_preempt_mode)
{
	gr_ctx->graphics_preempt_mode = graphics_preempt_mode;
}

u32 nvgpu_gr_ctx_get_graphics_preemption_mode(struct nvgpu_gr_ctx *gr_ctx)
{
	return gr_ctx->graphics_preempt_mode;
}

void nvgpu_gr_ctx_set_zcull_ctx(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	u32 mode, u64 gpu_va)
{
	struct zcull_ctx_desc *zcull_ctx = &gr_ctx->zcull_ctx;

	(void)g;

	zcull_ctx->ctx_sw_mode = mode;
	zcull_ctx->gpu_va = gpu_va;
}

u64 nvgpu_gr_ctx_get_zcull_ctx_va(struct nvgpu_gr_ctx *gr_ctx)
{
	return gr_ctx->zcull_ctx.gpu_va;
}

int nvgpu_gr_ctx_init_zcull(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx)
{
	int err;

	nvgpu_log(g, gpu_dbg_gr, " ");

	err = nvgpu_pg_elpg_ms_protected_call(g, g->ops.mm.cache.l2_flush(g, true));
	if (err != 0) {
		nvgpu_err(g, "l2_flush failed");
		return err;
	}

	g->ops.gr.ctxsw_prog.set_zcull_mode_no_ctxsw(g, &gr_ctx->mem);
	g->ops.gr.ctxsw_prog.set_zcull_ptr(g, &gr_ctx->mem, 0);

	return err;
}

int nvgpu_gr_ctx_zcull_setup(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	bool set_zcull_ptr)
{
	nvgpu_log_fn(g, " ");

	if (gr_ctx->zcull_ctx.gpu_va == 0ULL &&
	    g->ops.gr.ctxsw_prog.is_zcull_mode_separate_buffer(
			gr_ctx->zcull_ctx.ctx_sw_mode)) {
		return -EINVAL;
	}

	g->ops.gr.ctxsw_prog.set_zcull(g, &gr_ctx->mem,
		gr_ctx->zcull_ctx.ctx_sw_mode);

	if (set_zcull_ptr) {
		g->ops.gr.ctxsw_prog.set_zcull_ptr(g, &gr_ctx->mem,
			gr_ctx->zcull_ctx.gpu_va);
	}

	return 0;
}
#endif /* CONFIG_NVGPU_GRAPHICS */

#ifdef CONFIG_NVGPU_GFXP
void nvgpu_gr_ctx_set_preemption_buffer_va(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx)
{
	g->ops.gr.ctxsw_prog.set_full_preemption_ptr(g, &gr_ctx->mem,
		gr_ctx->preempt_ctxsw_buffer.gpu_va);

	if (g->ops.gr.ctxsw_prog.set_full_preemption_ptr_veid0 != NULL) {
		g->ops.gr.ctxsw_prog.set_full_preemption_ptr_veid0(g,
			&gr_ctx->mem, gr_ctx->preempt_ctxsw_buffer.gpu_va);
	}
}

bool nvgpu_gr_ctx_desc_force_preemption_gfxp(struct nvgpu_gr_ctx_desc *gr_ctx_desc)
{
	return gr_ctx_desc->force_preemption_gfxp;
}

static int nvgpu_gr_ctx_alloc_ctxsw_buffer(struct vm_gk20a *vm, size_t size,
	struct nvgpu_mem *mem)
{
	int err;

	err = nvgpu_dma_alloc_sys(vm->mm->g, size, mem);
	if (err != 0) {
		return err;
	}

	mem->gpu_va = nvgpu_gmmu_map_partial(vm,
				mem,
				mem->aligned_size,
				NVGPU_VM_MAP_CACHEABLE,
				gk20a_mem_flag_none,
				false,
				mem->aperture);
	if (mem->gpu_va == 0ULL) {
		nvgpu_dma_free(vm->mm->g, mem);
		return -ENOMEM;
	}

	return 0;
}

static int nvgpu_gr_ctx_alloc_preemption_buffers(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct vm_gk20a *vm)
{
	int err = 0;

	err = nvgpu_gr_ctx_alloc_ctxsw_buffer(vm,
			gr_ctx_desc->size[NVGPU_GR_CTX_PREEMPT_CTXSW],
			&gr_ctx->preempt_ctxsw_buffer);
	if (err != 0) {
		nvgpu_err(g, "cannot allocate preempt buffer");
		goto fail;
	}

	err = nvgpu_gr_ctx_alloc_ctxsw_buffer(vm,
			gr_ctx_desc->size[NVGPU_GR_CTX_SPILL_CTXSW],
			&gr_ctx->spill_ctxsw_buffer);
	if (err != 0) {
		nvgpu_err(g, "cannot allocate spill buffer");
		goto fail_free_preempt;
	}

	err = nvgpu_gr_ctx_alloc_ctxsw_buffer(vm,
			gr_ctx_desc->size[NVGPU_GR_CTX_BETACB_CTXSW],
			&gr_ctx->betacb_ctxsw_buffer);
	if (err != 0) {
		nvgpu_err(g, "cannot allocate beta buffer");
		goto fail_free_spill;
	}

	if (gr_ctx_desc->size[NVGPU_GR_CTX_GFXP_RTVCB_CTXSW] != 0U) {
		err = nvgpu_gr_ctx_alloc_ctxsw_buffer(vm,
			gr_ctx_desc->size[NVGPU_GR_CTX_GFXP_RTVCB_CTXSW],
			&gr_ctx->gfxp_rtvcb_ctxsw_buffer);
		if (err != 0) {
			nvgpu_err(g, "cannot allocate gfxp rtvcb");
			goto fail_free_betacb;
		}
	}
	return 0;

fail_free_betacb:
	nvgpu_dma_unmap_free(vm, &gr_ctx->betacb_ctxsw_buffer);
fail_free_spill:
	nvgpu_dma_unmap_free(vm, &gr_ctx->spill_ctxsw_buffer);
fail_free_preempt:
	nvgpu_dma_unmap_free(vm, &gr_ctx->preempt_ctxsw_buffer);
fail:
	return err;
}

int nvgpu_gr_ctx_alloc_ctxsw_buffers(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct vm_gk20a *vm)
{
	int err = 0;

	/* nothing to do if already initialized */
	if (nvgpu_mem_is_valid(&gr_ctx->preempt_ctxsw_buffer)) {
		return 0;
	}

	if (gr_ctx_desc->size[NVGPU_GR_CTX_PREEMPT_CTXSW] == 0U ||
	    gr_ctx_desc->size[NVGPU_GR_CTX_SPILL_CTXSW] == 0U ||
	    gr_ctx_desc->size[NVGPU_GR_CTX_BETACB_CTXSW] == 0U ||
	    gr_ctx_desc->size[NVGPU_GR_CTX_PAGEPOOL_CTXSW] == 0U) {
		return -EINVAL;
	}

	err = nvgpu_gr_ctx_alloc_preemption_buffers(g, gr_ctx,
		gr_ctx_desc, vm);

	if (err != 0) {
		nvgpu_err(g, "cannot allocate preemption buffers");
		goto fail;
	}

	err = nvgpu_gr_ctx_alloc_ctxsw_buffer(vm,
			gr_ctx_desc->size[NVGPU_GR_CTX_PAGEPOOL_CTXSW],
			&gr_ctx->pagepool_ctxsw_buffer);
	if (err != 0) {
		nvgpu_err(g, "cannot allocate page pool");
		goto fail;
	}

	return 0;

fail:
	return err;
}

struct nvgpu_mem *nvgpu_gr_ctx_get_preempt_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx)
{
	return &gr_ctx->preempt_ctxsw_buffer;
}

struct nvgpu_mem *nvgpu_gr_ctx_get_spill_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx)
{
	return &gr_ctx->spill_ctxsw_buffer;
}

struct nvgpu_mem *nvgpu_gr_ctx_get_betacb_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx)
{
	return &gr_ctx->betacb_ctxsw_buffer;
}

struct nvgpu_mem *nvgpu_gr_ctx_get_pagepool_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx)
{
	return &gr_ctx->pagepool_ctxsw_buffer;
}

struct nvgpu_mem *nvgpu_gr_ctx_get_gfxp_rtvcb_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx)
{
	return &gr_ctx->gfxp_rtvcb_ctxsw_buffer;
}
#endif /* CONFIG_NVGPU_GFXP */

#ifdef CONFIG_NVGPU_CILP
bool nvgpu_gr_ctx_desc_force_preemption_cilp(struct nvgpu_gr_ctx_desc *gr_ctx_desc)
{
	return gr_ctx_desc->force_preemption_cilp;
}

bool nvgpu_gr_ctx_get_cilp_preempt_pending(struct nvgpu_gr_ctx *gr_ctx)
{
	return gr_ctx->cilp_preempt_pending;
}

void nvgpu_gr_ctx_set_cilp_preempt_pending(struct nvgpu_gr_ctx *gr_ctx,
	bool cilp_preempt_pending)
{
	gr_ctx->cilp_preempt_pending = cilp_preempt_pending;
}
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
void nvgpu_gr_ctx_reset_patch_count(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx)
{
	u32 tmp;

	tmp = g->ops.gr.ctxsw_prog.get_patch_count(g, &gr_ctx->mem);
	if (tmp == 0U) {
		gr_ctx->patch_ctx.data_count = 0;
	}
}

void nvgpu_gr_ctx_set_patch_ctx(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	bool set_patch_addr)
{
	g->ops.gr.ctxsw_prog.set_patch_count(g, &gr_ctx->mem,
		gr_ctx->patch_ctx.data_count);
	if (set_patch_addr) {
		g->ops.gr.ctxsw_prog.set_patch_addr(g, &gr_ctx->mem,
			gr_ctx->patch_ctx.mem.gpu_va);
	}
}

int nvgpu_gr_ctx_alloc_pm_ctx(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct vm_gk20a *vm)
{
	struct pm_ctx_desc *pm_ctx = &gr_ctx->pm_ctx;
	int err;

	if (pm_ctx->mem.gpu_va != 0ULL) {
		return 0;
	}

	err = nvgpu_dma_alloc_sys(g, gr_ctx_desc->size[NVGPU_GR_CTX_PM_CTX],
			&pm_ctx->mem);
	if (err != 0) {
		nvgpu_err(g,
			"failed to allocate pm ctx buffer");
		return err;
	}

	pm_ctx->mem.gpu_va = nvgpu_gmmu_map(vm,
					&pm_ctx->mem,
					NVGPU_VM_MAP_CACHEABLE,
					gk20a_mem_flag_none, true,
					pm_ctx->mem.aperture);
	if (pm_ctx->mem.gpu_va == 0ULL) {
		nvgpu_err(g,
			"failed to map pm ctxt buffer");
		nvgpu_dma_free(g, &pm_ctx->mem);
		return -ENOMEM;
	}

	return 0;
}

void nvgpu_gr_ctx_free_pm_ctx(struct gk20a *g, struct vm_gk20a *vm,
	struct nvgpu_gr_ctx *gr_ctx)
{
	struct pm_ctx_desc *pm_ctx = &gr_ctx->pm_ctx;

	if (pm_ctx->mem.gpu_va != 0ULL) {
		nvgpu_dma_unmap_free(vm, &pm_ctx->mem);
	}

	(void)g;
}

struct nvgpu_mem *nvgpu_gr_ctx_get_pm_ctx_mem(struct nvgpu_gr_ctx *gr_ctx)
{
	return &gr_ctx->pm_ctx.mem;
}

void nvgpu_gr_ctx_set_pm_ctx_pm_mode(struct nvgpu_gr_ctx *gr_ctx, u32 pm_mode)
{
	gr_ctx->pm_ctx.pm_mode = pm_mode;
}

u32 nvgpu_gr_ctx_get_pm_ctx_pm_mode(struct nvgpu_gr_ctx *gr_ctx)
{
	return gr_ctx->pm_ctx.pm_mode;
}

u32 nvgpu_gr_ctx_get_ctx_id(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx)
{
	if (!gr_ctx->ctx_id_valid) {
		/* Channel gr_ctx buffer is gpu cacheable.
		   Flush and invalidate before cpu update. */
		if (nvgpu_pg_elpg_ms_protected_call(g,
					g->ops.mm.cache.l2_flush(g, true)) != 0) {
			nvgpu_err(g, "l2_flush failed");
		}

		gr_ctx->ctx_id = g->ops.gr.ctxsw_prog.get_main_image_ctx_id(g,
					&gr_ctx->mem);
		gr_ctx->ctx_id_valid = true;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_intr, "ctx_id: 0x%x", gr_ctx->ctx_id);

	return gr_ctx->ctx_id;
}

u32 nvgpu_gr_ctx_read_ctx_id(struct nvgpu_gr_ctx *gr_ctx)
{
	return gr_ctx->ctx_id;
}

#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
void nvgpu_gr_ctx_set_boosted_ctx(struct nvgpu_gr_ctx *gr_ctx, bool boost)
{
	gr_ctx->boosted_ctx = boost;
}

bool nvgpu_gr_ctx_get_boosted_ctx(struct nvgpu_gr_ctx *gr_ctx)
{
	return gr_ctx->boosted_ctx;
}
#endif

#ifdef CONFIG_DEBUG_FS
bool nvgpu_gr_ctx_desc_dump_ctxsw_stats_on_channel_close(
		struct nvgpu_gr_ctx_desc *gr_ctx_desc)
{
	return gr_ctx_desc->dump_ctxsw_stats_on_channel_close;
}
#endif

int nvgpu_gr_ctx_set_smpc_mode(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	bool enable)
{
	int err;

	if (!nvgpu_mem_is_valid(&gr_ctx->mem)) {
		nvgpu_err(g, "no graphics context allocated");
		return -EFAULT;
	}

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	err = nvgpu_pg_elpg_ms_protected_call(g, g->ops.mm.cache.l2_flush(g, true));
	if (err != 0) {
		nvgpu_err(g, "l2_flush failed");
		return err;
	}

	g->ops.gr.ctxsw_prog.set_pm_smpc_mode(g, &gr_ctx->mem, enable);

	return err;
}

int nvgpu_gr_ctx_prepare_hwpm_mode(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	u32 mode, bool *skip_update)
{
	struct pm_ctx_desc *pm_ctx = &gr_ctx->pm_ctx;
	int ret = 0;

	*skip_update = false;

	if (!nvgpu_mem_is_valid(&gr_ctx->mem)) {
		nvgpu_err(g, "no graphics context allocated");
		return -EFAULT;
	}

	if ((mode == NVGPU_GR_CTX_HWPM_CTXSW_MODE_STREAM_OUT_CTXSW) &&
	    (g->ops.gr.ctxsw_prog.hw_get_pm_mode_stream_out_ctxsw == NULL)) {
		nvgpu_err(g,
			"Mode-E hwpm context switch mode is not supported");
		return -EINVAL;
	}

	switch (mode) {
	case NVGPU_GR_CTX_HWPM_CTXSW_MODE_CTXSW:
		if (pm_ctx->pm_mode ==
		    g->ops.gr.ctxsw_prog.hw_get_pm_mode_ctxsw()) {
			*skip_update = true;
			return 0;
		}
		pm_ctx->pm_mode = g->ops.gr.ctxsw_prog.hw_get_pm_mode_ctxsw();
		pm_ctx->gpu_va = pm_ctx->mem.gpu_va;
		break;
	case  NVGPU_GR_CTX_HWPM_CTXSW_MODE_NO_CTXSW:
		if (pm_ctx->pm_mode ==
		    g->ops.gr.ctxsw_prog.hw_get_pm_mode_no_ctxsw()) {
			*skip_update = true;
			return 0;
		}
		pm_ctx->pm_mode =
			g->ops.gr.ctxsw_prog.hw_get_pm_mode_no_ctxsw();
		pm_ctx->gpu_va = 0;
		break;
	case NVGPU_GR_CTX_HWPM_CTXSW_MODE_STREAM_OUT_CTXSW:
		if (pm_ctx->pm_mode ==
		    g->ops.gr.ctxsw_prog.hw_get_pm_mode_stream_out_ctxsw()) {
			*skip_update = true;
			return 0;
		}
		pm_ctx->pm_mode =
			g->ops.gr.ctxsw_prog.hw_get_pm_mode_stream_out_ctxsw();
		pm_ctx->gpu_va = pm_ctx->mem.gpu_va;
		break;
	default:
		nvgpu_err(g, "invalid hwpm context switch mode");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int nvgpu_gr_ctx_set_hwpm_mode(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	bool set_pm_ptr)
{
	int err;

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	err = nvgpu_pg_elpg_ms_protected_call(g, g->ops.mm.cache.l2_flush(g, true));
	if (err != 0) {
		nvgpu_err(g, "l2_flush failed");
		return err;
	}

	g->ops.gr.ctxsw_prog.set_pm_mode(g, &gr_ctx->mem,
			gr_ctx->pm_ctx.pm_mode);
	if (set_pm_ptr) {
		g->ops.gr.ctxsw_prog.set_pm_ptr(g, &gr_ctx->mem,
			gr_ctx->pm_ctx.gpu_va);
	}

	return err;
}
#endif /* CONFIG_NVGPU_DEBUGGER */
