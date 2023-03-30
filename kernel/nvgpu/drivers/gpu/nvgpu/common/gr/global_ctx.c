/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/kmem.h>
#include <nvgpu/bug.h>
#include <nvgpu/dma.h>
#ifdef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
#include <nvgpu/static_analysis.h>
#include <nvgpu/string.h>
#endif

#include <nvgpu/gr/global_ctx.h>
#include <nvgpu/power_features/pg.h>

#include "global_ctx_priv.h"

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
#include <nvgpu/posix/posix-fault-injection.h>

struct nvgpu_posix_fault_inj *nvgpu_golden_ctx_verif_get_fault_injection(void)
{
	struct nvgpu_posix_fault_inj_container *c =
		nvgpu_posix_fault_injection_get_container();

	return &c->golden_ctx_verif_fi;
}

struct nvgpu_posix_fault_inj *nvgpu_local_golden_image_get_fault_injection(void)
{
	struct nvgpu_posix_fault_inj_container *c =
		nvgpu_posix_fault_injection_get_container();

	return &c->local_golden_image_fi;
}
#endif

struct nvgpu_gr_global_ctx_buffer_desc *
nvgpu_gr_global_ctx_desc_alloc(struct gk20a *g)
{
	struct nvgpu_gr_global_ctx_buffer_desc *desc =
		nvgpu_kzalloc(g, sizeof(*desc) *
					U64(NVGPU_GR_GLOBAL_CTX_COUNT));
	return desc;
}

void nvgpu_gr_global_ctx_desc_free(struct gk20a *g,
	struct nvgpu_gr_global_ctx_buffer_desc *desc)
{
	if (desc != NULL) {
		nvgpu_kfree(g, desc);
	}
}


void nvgpu_gr_global_ctx_set_size(struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index, size_t size)
{
	nvgpu_assert(index < NVGPU_GR_GLOBAL_CTX_COUNT);
	desc[index].size = size;
}

size_t nvgpu_gr_global_ctx_get_size(struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index)
{
	return desc[index].size;
}

static void nvgpu_gr_global_ctx_buffer_destroy(struct gk20a *g,
		struct nvgpu_mem *mem)
{
	nvgpu_dma_free(g, mem);
}

void nvgpu_gr_global_ctx_buffer_free(struct gk20a *g,
	struct nvgpu_gr_global_ctx_buffer_desc *desc)
{
	u32 i;

	if (desc == NULL) {
		return;
	}

	for (i = 0; i < NVGPU_GR_GLOBAL_CTX_COUNT; i++) {
		if (desc[i].destroy != NULL) {
			desc[i].destroy(g, &desc[i].mem);
			desc[i].destroy = NULL;
		}
	}

	nvgpu_log_fn(g, "done");
}

static int nvgpu_gr_global_ctx_buffer_alloc_sys(struct gk20a *g,
	struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (nvgpu_mem_is_valid(&desc[index].mem)) {
		return 0;
	}

	err = nvgpu_dma_alloc_sys(g, desc[index].size,
			&desc[index].mem);
	if (err != 0) {
		return err;
	}

	desc[index].destroy = nvgpu_gr_global_ctx_buffer_destroy;

	return err;
}

#ifdef CONFIG_NVGPU_VPR
static int nvgpu_gr_global_ctx_buffer_alloc_vpr(struct gk20a *g,
	struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (nvgpu_mem_is_valid(&desc[index].mem)) {
		return 0;
	}

	if (g->ops.secure_alloc != NULL) {
		err = g->ops.secure_alloc(g,
				&desc[index].mem, desc[index].size,
				&desc[index].destroy);
		if (err != 0) {
			return err;
		}
	}

	return err;
}
#endif

static bool nvgpu_gr_global_ctx_buffer_sizes_are_valid(struct gk20a *g,
				struct nvgpu_gr_global_ctx_buffer_desc *desc)
{

	if (desc[NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP].size == 0U) {
		return false;
	}

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		if ((desc[NVGPU_GR_GLOBAL_CTX_CIRCULAR].size == 0U) ||
			(desc[NVGPU_GR_GLOBAL_CTX_PAGEPOOL].size == 0U) ||
			(desc[NVGPU_GR_GLOBAL_CTX_ATTRIBUTE].size == 0U)) {
			return false;
		}
#ifdef CONFIG_NVGPU_VPR
		if ((desc[NVGPU_GR_GLOBAL_CTX_CIRCULAR_VPR].size == 0U) ||
			(desc[NVGPU_GR_GLOBAL_CTX_PAGEPOOL_VPR].size == 0U) ||
			(desc[NVGPU_GR_GLOBAL_CTX_ATTRIBUTE_VPR].size == 0U)) {
			return false;
		}
#endif
	}

	return true;
}

#ifdef CONFIG_NVGPU_VPR
static int nvgpu_gr_global_ctx_buffer_vpr_alloc(struct gk20a *g,
				struct nvgpu_gr_global_ctx_buffer_desc *desc)
{
	int err = 0;

	/*
	 * MIG supports only compute class.
	 * Allocate BUNDLE_CB, PAGEPOOL, ATTRIBUTE_CB and RTV_CB
	 * if 2D/3D/I2M classes(graphics) are supported.
	 */
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		nvgpu_log(g, gpu_dbg_gr | gpu_dbg_mig,
			"2D class is not supported "
				"skip BUNDLE_CB, PAGEPOOL, ATTRIBUTE_CB "
				"and RTV_CB");
		return 0;
	}

	err = nvgpu_gr_global_ctx_buffer_alloc_vpr(g, desc,
		NVGPU_GR_GLOBAL_CTX_CIRCULAR_VPR);
	if (err != 0) {
		goto fail;
	}

	err = nvgpu_gr_global_ctx_buffer_alloc_vpr(g, desc,
		NVGPU_GR_GLOBAL_CTX_PAGEPOOL_VPR);
	if (err != 0) {
		goto fail;
	}

	err = nvgpu_gr_global_ctx_buffer_alloc_vpr(g, desc,
		NVGPU_GR_GLOBAL_CTX_ATTRIBUTE_VPR);
	if (err != 0) {
		goto fail;
	}
fail:
	return err;
}
#endif

static int nvgpu_gr_global_ctx_buffer_sys_alloc(struct gk20a *g,
				struct nvgpu_gr_global_ctx_buffer_desc *desc)
{
	int err = 0;

	/*
	 * MIG supports only compute class.
	 * Allocate BUNDLE_CB, PAGEPOOL, ATTRIBUTE_CB and RTV_CB
	 * if 2D/3D/I2M classes(graphics) are supported.
	 */
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		err = nvgpu_gr_global_ctx_buffer_alloc_sys(g, desc,
			NVGPU_GR_GLOBAL_CTX_CIRCULAR);
		if (err != 0) {
			goto fail;
		}

		err = nvgpu_gr_global_ctx_buffer_alloc_sys(g, desc,
			NVGPU_GR_GLOBAL_CTX_PAGEPOOL);
		if (err != 0) {
			goto fail;
		}

		err = nvgpu_gr_global_ctx_buffer_alloc_sys(g, desc,
			NVGPU_GR_GLOBAL_CTX_ATTRIBUTE);
		if (err != 0) {
			goto fail;
		}
	}

	err = nvgpu_gr_global_ctx_buffer_alloc_sys(g, desc,
		NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP);

fail:
	return err;
}


int nvgpu_gr_global_ctx_buffer_alloc(struct gk20a *g,
	struct nvgpu_gr_global_ctx_buffer_desc *desc)
{
	int err = 0;

	if (nvgpu_gr_global_ctx_buffer_sizes_are_valid(g, desc) != true) {
		return -EINVAL;
	}

	err = nvgpu_gr_global_ctx_buffer_sys_alloc(g, desc);
	if (err != 0) {
		goto clean_up;
	}

#ifdef CONFIG_NVGPU_FECS_TRACE
	if (desc[NVGPU_GR_GLOBAL_CTX_FECS_TRACE_BUFFER].size != 0U) {
		err = nvgpu_gr_global_ctx_buffer_alloc_sys(g, desc,
			NVGPU_GR_GLOBAL_CTX_FECS_TRACE_BUFFER);
		if (err != 0) {
			goto clean_up;
		}
	}
#endif

#ifdef CONFIG_NVGPU_GRAPHICS
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		if (desc[NVGPU_GR_GLOBAL_CTX_RTV_CIRCULAR_BUFFER].size != 0U) {
			err = nvgpu_gr_global_ctx_buffer_alloc_sys(g, desc,
				NVGPU_GR_GLOBAL_CTX_RTV_CIRCULAR_BUFFER);
			if (err != 0) {
				goto clean_up;
			}
		}
	}
#endif

#ifdef CONFIG_NVGPU_VPR
	if (nvgpu_gr_global_ctx_buffer_vpr_alloc(g, desc) != 0) {
			goto clean_up;
	}
#endif

	return err;

clean_up:
	nvgpu_gr_global_ctx_buffer_free(g, desc);
	return err;
}

u64 nvgpu_gr_global_ctx_buffer_map(struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index,
	struct vm_gk20a *vm, u32 flags, bool priv)
{
	u64 gpu_va;

	if (!nvgpu_mem_is_valid(&desc[index].mem)) {
		return 0;
	}

	gpu_va = nvgpu_gmmu_map(vm, &desc[index].mem,
			flags, gk20a_mem_flag_none, priv,
			desc[index].mem.aperture);
	return gpu_va;
}

void nvgpu_gr_global_ctx_buffer_unmap(
	struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index,
	struct vm_gk20a *vm, u64 gpu_va)
{
	if (nvgpu_mem_is_valid(&desc[index].mem)) {
		nvgpu_gmmu_unmap_addr(vm, &desc[index].mem, gpu_va);
	}
}

struct nvgpu_mem *nvgpu_gr_global_ctx_buffer_get_mem(
	struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index)
{
	if (nvgpu_mem_is_valid(&desc[index].mem)) {
		return &desc[index].mem;
	}
	return NULL;
}

bool nvgpu_gr_global_ctx_buffer_ready(
	struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index)
{
	if (nvgpu_mem_is_valid(&desc[index].mem)) {
		return true;
	}
	return false;
}

int nvgpu_gr_global_ctx_alloc_local_golden_image(struct gk20a *g,
		struct nvgpu_gr_global_ctx_local_golden_image **img,
		size_t size)
{
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image;

	local_golden_image = nvgpu_kzalloc(g, sizeof(*local_golden_image));
	if (local_golden_image == NULL) {
		return -ENOMEM;
	}

	local_golden_image->context = nvgpu_vzalloc(g, size);
	if (local_golden_image->context == NULL) {
		nvgpu_kfree(g, local_golden_image);
		return -ENOMEM;
	}

	local_golden_image->size = size;

	*img = local_golden_image;
	return 0;
}

void nvgpu_gr_global_ctx_init_local_golden_image(struct gk20a *g,
		struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image,
		struct nvgpu_mem *source_mem, size_t size)
{
	(void)size;
	nvgpu_mem_rd_n(g, source_mem, 0, local_golden_image->context,
		nvgpu_safe_cast_u64_to_u32(local_golden_image->size));
}

#ifdef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
bool nvgpu_gr_global_ctx_compare_golden_images(struct gk20a *g,
	bool is_sysmem,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image1,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image2,
	size_t size)
{
	bool is_identical = true;
	u32 *data1 = local_golden_image1->context;
	u32 *data2 = local_golden_image2->context;
#ifdef CONFIG_NVGPU_DGPU
	u32 i;
#endif

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
			nvgpu_golden_ctx_verif_get_fault_injection())) {
		return false;
	}
#endif

	/*
	 * In case of sysmem, direct mem compare can be used.
	 * For vidmem, word by word comparison only works and
	 * it is too early to use ce engine for read operations.
	 */
	if (is_sysmem) {
		if (nvgpu_memcmp((u8 *)data1, (u8 *)data2, size) != 0) {
			is_identical = false;
		}
	}
	else {
#ifdef CONFIG_NVGPU_DGPU
		for( i = 0U; i < nvgpu_safe_cast_u64_to_u32(size/sizeof(u32));
					i = nvgpu_safe_add_u32(i, 1U)) {
			if (*(data1 + i) != *(data2 + i)) {
				is_identical = false;
				nvgpu_log_info(g,
				"mismatch i = %u golden1: %u golden2 %u",
				i, *(data1 + i), *(data2 + i));
				break;
			}
		}
#else
		is_identical = false;
#endif
	}

	nvgpu_log_info(g, "%s result %u", __func__, is_identical);
	return is_identical;
}
#endif

void nvgpu_gr_global_ctx_load_local_golden_image(struct gk20a *g,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image,
	struct nvgpu_mem *target_mem)
{
	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	if (nvgpu_pg_elpg_ms_protected_call(g,
				g->ops.mm.cache.l2_flush(g, true)) != 0) {
		nvgpu_err(g, "l2_flush failed");
	}

	nvgpu_mem_wr_n(g, target_mem, 0, local_golden_image->context,
		nvgpu_safe_cast_u64_to_u32(local_golden_image->size));

	nvgpu_log(g, gpu_dbg_gr, "loaded saved golden image into gr_ctx");
}

void nvgpu_gr_global_ctx_deinit_local_golden_image(struct gk20a *g,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image)
{
	nvgpu_vfree(g, local_golden_image->context);
	nvgpu_kfree(g, local_golden_image);
}

#ifdef CONFIG_NVGPU_DEBUGGER
u32 *nvgpu_gr_global_ctx_get_local_golden_image_ptr(
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image)
{
	return local_golden_image->context;
}
#endif
