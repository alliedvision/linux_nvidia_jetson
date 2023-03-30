/*
 * CBC
 *
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
#include <nvgpu/cbc.h>
#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/string.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/comptags.h>
#include <nvgpu/soc.h>

void nvgpu_cbc_remove_support(struct gk20a *g)
{
	struct nvgpu_cbc *cbc = g->cbc;

	nvgpu_log_fn(g, " ");

	if (cbc == NULL) {
		return;
	}
#ifdef CONFIG_NVGPU_IVM_BUILD
	nvgpu_cbc_contig_deinit(g);
#endif
	if (nvgpu_mem_is_valid(&cbc->compbit_store.mem)) {
		nvgpu_dma_free(g, &cbc->compbit_store.mem);
		(void) memset(&cbc->compbit_store, 0,
			sizeof(struct compbit_store_desc));
	}
	gk20a_comptag_allocator_destroy(g, &cbc->comp_tags);

	nvgpu_kfree(g, cbc);
	g->cbc = NULL;
}

/*
 * This function is triggered during finalize_poweron multiple times.
 * This function should not return if cbc is not NULL.
 * cbc.init(), which re-writes HW registers that are reset during suspend,
 * should be allowed to execute each time.
 */
int nvgpu_cbc_init_support(struct gk20a *g)
{
	int err = 0;
	struct nvgpu_cbc *cbc = g->cbc;
	bool is_resume = true;

	nvgpu_log_fn(g, " ");

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_COMPRESSION)) {
		return 0;
	}

	/*
	 * If cbc == NULL, the device is being powered-on for the first
	 * time and hence nvgpu_cbc_init_support is not called as part of
	 * suspend/resume cycle, so set is_resume to false.
	 */
	if (cbc == NULL) {
		cbc = nvgpu_kzalloc(g, sizeof(*cbc));
		if (cbc == NULL) {
			return -ENOMEM;
		}
		g->cbc = cbc;

		if (g->ops.cbc.alloc_comptags != NULL) {
			err = g->ops.cbc.alloc_comptags(g, g->cbc);
			if (err != 0) {
				nvgpu_err(g, "Failed to allocate comptags");
				nvgpu_kfree(g, cbc);
				g->cbc = NULL;
				return err;
			}
		}
		is_resume = false;
	}

	if (g->ops.cbc.init != NULL) {
		g->ops.cbc.init(g, g->cbc, is_resume);
	}

	return err;
}

#ifdef CONFIG_NVGPU_IVM_BUILD
static int nvgpu_init_cbc_mem(struct gk20a *g, u64 pa, u64 size)
{
	u64 nr_pages;
	int err = 0;
	struct nvgpu_cbc *cbc = g->cbc;

	nr_pages = size / NVGPU_CPU_PAGE_SIZE;
	err = nvgpu_mem_create_from_phys(g, &cbc->compbit_store.mem,
		pa, nr_pages);
	return err;
}

static int nvgpu_get_mem_from_contigpool(struct gk20a *g, size_t size)
{
	struct nvgpu_contig_cbcmempool *contig_pool;
	u64 pa;
	int err = 0;

	contig_pool = g->cbc->cbc_contig_mempool;
	if (contig_pool->size < size) {
		return -ENOMEM;
	}

	pa = contig_pool->base_addr;
	err = nvgpu_init_cbc_mem(g, pa, size);
	if (err != 0) {
		return err;
	}

	return 0;

}
#endif

int nvgpu_cbc_alloc(struct gk20a *g, size_t compbit_backing_size,
			bool vidmem_alloc)
{
	struct nvgpu_cbc *cbc = g->cbc;
#ifdef CONFIG_NVGPU_IVM_BUILD
	int err = 0;
#endif
	(void)vidmem_alloc;

	if (nvgpu_mem_is_valid(&cbc->compbit_store.mem) != 0) {
		return 0;
	}

#ifdef CONFIG_NVGPU_DGPU
	if (vidmem_alloc == true) {
		/*
		 * Backing store MUST be physically contiguous and allocated in
		 * one chunk
		 * Vidmem allocation API does not support FORCE_CONTIGUOUS like
		 * flag to allocate contiguous memory
		 * But this allocation will happen in vidmem bootstrap allocator
		 * which always allocates contiguous memory
		 */
		return nvgpu_dma_alloc_vid(g,
					 compbit_backing_size,
					 &cbc->compbit_store.mem);
	}
#endif
#ifdef CONFIG_NVGPU_IVM_BUILD
	if (nvgpu_is_hypervisor_mode(g) && !g->is_virtual &&
			(g->ops.cbc.use_contig_pool != NULL)) {
		if (cbc->cbc_contig_mempool == NULL) {
			err = nvgpu_cbc_contig_init(g);
			if (err != 0) {
				nvgpu_err(g, "Contig pool initialization failed");
				return -ENOMEM;
			}
		}

		return nvgpu_get_mem_from_contigpool(g,
				compbit_backing_size);
	} else
#endif
	{
		return nvgpu_dma_alloc_flags_sys(g,
				NVGPU_DMA_PHYSICALLY_ADDRESSED,
				compbit_backing_size,
				&cbc->compbit_store.mem);
	}
}
