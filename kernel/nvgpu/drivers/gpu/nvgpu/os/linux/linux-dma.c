/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/iommu.h>

#include <nvgpu/log.h>
#include <nvgpu/dma.h>
#include <nvgpu/lock.h>
#include <nvgpu/bug.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/kmem.h>
#include <nvgpu/enabled.h>
#include <nvgpu/vidmem.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_sgt.h>

#include <nvgpu/linux/dma.h>

#include "platform_gk20a.h"
#include "os_linux.h"
#include "dmabuf_vidmem.h"

/*
 * Enough to hold all the possible flags in string form. When a new flag is
 * added it must be added here as well!!
 */
#define NVGPU_DMA_STR_SIZE					\
	sizeof("NO_KERNEL_MAPPING PHYSICALLY_ADDRESSED")

/*
 * This function can't fail. It will always at minimum memset() the buf which
 * is assumed to be able to hold at least %NVGPU_DMA_STR_SIZE bytes.
 */
void nvgpu_dma_flags_to_str(struct gk20a *g, unsigned long flags, char *buf)
{
	int bytes_available = NVGPU_DMA_STR_SIZE;

	memset(buf, 0, NVGPU_DMA_STR_SIZE);

#define APPEND_FLAG(flag, str_flag)					\
	do {								\
		if (flags & flag) {					\
			strncat(buf, str_flag, bytes_available);	\
			bytes_available -= strlen(str_flag);		\
		}							\
	} while (false)

	APPEND_FLAG(NVGPU_DMA_NO_KERNEL_MAPPING,    "NO_KERNEL_MAPPING ");
	APPEND_FLAG(NVGPU_DMA_PHYSICALLY_ADDRESSED, "PHYSICALLY_ADDRESSED");
#undef APPEND_FLAG
}

/**
 * __dma_dbg - Debug print for DMA allocs and frees.
 *
 * @g     - The GPU.
 * @size  - The requested size of the alloc (size_t).
 * @flags - The flags (unsigned long).
 * @type  - A string describing the type (i.e: sysmem or vidmem).
 * @what  - A string with 'alloc' or 'free'.
 *
 * @flags is the DMA flags. If there are none or it doesn't make sense to print
 * flags just pass 0.
 *
 * Please use dma_dbg_alloc() and dma_dbg_free() instead of this function.
 */
static void __dma_dbg(struct gk20a *g, size_t size, unsigned long flags,
		      const char *type, const char *what,
		      const char *func, int line)
{
	char flags_str[NVGPU_DMA_STR_SIZE];

	/*
	 * Don't bother making the flags_str if debugging is not enabled.
	 */
	if (!nvgpu_log_mask_enabled(g, gpu_dbg_dma))
		return;

	nvgpu_dma_flags_to_str(g, flags, flags_str);

	nvgpu_log_dbg_impl(g, gpu_dbg_dma,
			func, line,
			"DMA %s: [%s] size=%-7zu "
			"aligned=%-7zu total=%-10llukB %s",
			what, type,
			size, PAGE_ALIGN(size),
			g->dma_memory_used >> 10,
			flags_str);
}

static void nvgpu_dma_print_err(struct gk20a *g, size_t size,
				const char *type, const char *what,
				unsigned long flags)
{
	char flags_str[NVGPU_DMA_STR_SIZE];

	nvgpu_dma_flags_to_str(g, flags, flags_str);

	nvgpu_info(g,
		  "DMA %s FAILED: [%s] size=%-7zu "
		  "aligned=%-7zu flags:%s",
		  what, type,
		  size, PAGE_ALIGN(size), flags_str);
}

#define dma_dbg_alloc(g, size, flags, type)				\
	__dma_dbg(g, size, flags, type, "alloc", __func__, __LINE__)
#define dma_dbg_free(g, size, flags, type)				\
	__dma_dbg(g, size, flags, type, "free", __func__, __LINE__)

/*
 * For after the DMA alloc is done.
 */
#define __dma_dbg_done(g, size, type, what)				\
	nvgpu_log(g, gpu_dbg_dma,					\
		  "DMA %s: [%s] size=%-7zu Done!",			\
		  what, type, size);					\

#define dma_dbg_alloc_done(g, size, type)				\
	__dma_dbg_done(g, size, type, "alloc")
#define dma_dbg_free_done(g, size, type)				\
	__dma_dbg_done(g, size, type, "free")

#if defined(CONFIG_NVGPU_DGPU)
static u64 __nvgpu_dma_alloc(struct nvgpu_allocator *allocator, u64 at,
				size_t size)
{
	u64 addr = 0;

	if (at)
		addr = nvgpu_alloc_fixed(allocator, at, size, 0);
	else
		addr = nvgpu_alloc(allocator, size);

	return addr;
}
#endif

/**
 * The nvgpu_dma_alloc_no_iommu/nvgpu_dma_free_no_iommu() are for use
 * cases where memory can be physically non-contiguous even if GPU is
 * not iommuable as GPU uses nvlink to access the memory and lets GMMU
 * fully control it
 */
static void __nvgpu_dma_free_no_iommu(struct page **pages,
				      int max, bool big_array)
{
	int i;

	for (i = 0; i < max; i++)
		if (pages[i])
			__free_pages(pages[i], 0);

	if (big_array)
		vfree(pages);
	else
		kfree(pages);
}

static void *nvgpu_dma_alloc_no_iommu(struct device *dev, size_t size,
				      dma_addr_t *dma_handle, gfp_t gfps)
{
	int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned int array_size = count * sizeof(struct page *);
	struct page **pages;
	int i = 0;

	if (array_size <= NVGPU_CPU_PAGE_SIZE)
		pages = kzalloc(array_size, GFP_KERNEL);
	else
		pages = vzalloc(array_size);
	if (!pages)
		return NULL;

	gfps |= __GFP_HIGHMEM | __GFP_NOWARN;

	while (count) {
		int j, order = __fls(count);

		pages[i] = alloc_pages(gfps, order);
		while (!pages[i] && order)
			pages[i] = alloc_pages(gfps, --order);
		if (!pages[i])
			goto error;

		if (order) {
			split_page(pages[i], order);
			j = 1 << order;
			while (--j)
				pages[i + j] = pages[i] + j;
		}

		memset(page_address(pages[i]), 0, NVGPU_CPU_PAGE_SIZE << order);

		i += 1 << order;
		count -= 1 << order;
	}

	*dma_handle = __pfn_to_phys(page_to_pfn(pages[0]));

	return (void *)pages;

error:
	__nvgpu_dma_free_no_iommu(pages, i, array_size > NVGPU_CPU_PAGE_SIZE);
	return NULL;
}

static void nvgpu_dma_free_no_iommu(size_t size, void *vaddr)
{
	int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned int array_size = count * sizeof(struct page *);
	struct page **pages = vaddr;

	WARN_ON(!pages);

	__nvgpu_dma_free_no_iommu(pages, count, array_size > NVGPU_CPU_PAGE_SIZE);
}

/* Check if IOMMU is available and if GPU uses it */
#define nvgpu_uses_iommu(g) \
	(nvgpu_iommuable(g) && !nvgpu_is_enabled(g, NVGPU_MM_USE_PHYSICAL_SG))

static void nvgpu_dma_flags_to_attrs(struct gk20a *g, unsigned long *attrs,
				     unsigned long flags)
{
	if (flags & NVGPU_DMA_NO_KERNEL_MAPPING)
		*attrs |= DMA_ATTR_NO_KERNEL_MAPPING;
	if (flags & NVGPU_DMA_PHYSICALLY_ADDRESSED && !nvgpu_uses_iommu(g))
		*attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
}

/*
 * When GPU uses nvlink instead of IOMMU, memory can be non-contiguous if
 * no NVGPU_DMA_PHYSICALLY_ADDRESSED flag is assigned. This means the GPU
 * driver will need to map the memory after allocation
 */
#define nvgpu_nvlink_non_contig(g, flags) \
	(nvgpu_is_enabled(g, NVGPU_MM_BYPASSES_IOMMU) && \
	 !(flags & NVGPU_DMA_PHYSICALLY_ADDRESSED))

int nvgpu_dma_alloc_flags_sys(struct gk20a *g, unsigned long flags,
		size_t size, struct nvgpu_mem *mem)
{
	struct device *d = dev_from_gk20a(g);
	gfp_t gfps = GFP_KERNEL|__GFP_ZERO;
	dma_addr_t iova;
	unsigned long dma_attrs = 0;
	void *alloc_ret;
	int err;

	if (nvgpu_mem_is_valid(mem)) {
		nvgpu_warn(g, "memory leak !!");
		WARN_ON(1);
	}

	/*
	 * Before the debug print so we see this in the total. But during
	 * cleanup in the fail path this has to be subtracted.
	 */
	g->dma_memory_used += PAGE_ALIGN(size);

	dma_dbg_alloc(g, size, flags, "sysmem");

	/*
	 * Save the old size but for actual allocation purposes the size is
	 * going to be page aligned.
	 */
	mem->size = size;
	size = PAGE_ALIGN(size);

	nvgpu_dma_flags_to_attrs(g, &dma_attrs, flags);
	if (nvgpu_nvlink_non_contig(g, flags))
		alloc_ret = nvgpu_dma_alloc_no_iommu(d, size, &iova, gfps);
	else
		alloc_ret = dma_alloc_attrs(d, size, &iova, gfps, dma_attrs);
	if (!alloc_ret) {
		err = -ENOMEM;
		goto print_dma_err;
	}

	if (nvgpu_nvlink_non_contig(g, flags) ||
	    flags & NVGPU_DMA_NO_KERNEL_MAPPING) {
		mem->priv.pages = alloc_ret;
		err = nvgpu_get_sgtable_from_pages(g, &mem->priv.sgt,
						   mem->priv.pages,
						   iova, size);
	} else {
		mem->cpu_va = alloc_ret;
		err = nvgpu_get_sgtable_attrs(g, &mem->priv.sgt, mem->cpu_va,
					iova, size, flags);
	}
	if (err)
		goto fail_free_dma;

	/* Map the page list from the non-contiguous allocation */
	if (nvgpu_nvlink_non_contig(g, flags)) {
		mem->cpu_va = vmap(mem->priv.pages, size >> PAGE_SHIFT,
				   0, PAGE_KERNEL);
		if (!mem->cpu_va) {
			err = -ENOMEM;
			goto fail_free_sgt;
		}
	}

	mem->aligned_size = size;
	mem->aperture = APERTURE_SYSMEM;
	mem->priv.flags = flags;

	dma_dbg_alloc_done(g, mem->size, "sysmem");

	return 0;

fail_free_sgt:
	nvgpu_free_sgtable(g, &mem->priv.sgt);
fail_free_dma:
	dma_free_attrs(d, size, alloc_ret, iova, dma_attrs);
	mem->cpu_va = NULL;
	mem->priv.sgt = NULL;
	mem->size = 0;
	g->dma_memory_used -= mem->aligned_size;
print_dma_err:
	nvgpu_dma_print_err(g, size, "sysmem", "alloc", flags);
	return err;
}

#if defined(CONFIG_NVGPU_DGPU)
int nvgpu_dma_alloc_flags_vid_at(struct gk20a *g, unsigned long flags,
		size_t size, struct nvgpu_mem *mem, u64 at)
{
	u64 addr;
	int err;
	struct nvgpu_allocator *vidmem_alloc = g->mm.vidmem.cleared ?
		&g->mm.vidmem.allocator :
		&g->mm.vidmem.bootstrap_allocator;
	u64 before_pending;

	if (nvgpu_mem_is_valid(mem)) {
		nvgpu_warn(g, "memory leak !!");
		WARN_ON(1);
	}

	dma_dbg_alloc(g, size, flags, "vidmem");

	mem->size = size;
	size = PAGE_ALIGN(size);

	if (!nvgpu_alloc_initialized(&g->mm.vidmem.allocator)) {
		err = -ENOSYS;
		goto print_dma_err;
	}

	/*
	 * Our own allocator doesn't have any flags yet, and we can't
	 * kernel-map these, so require explicit flags.
	 */
	WARN_ON(flags != NVGPU_DMA_NO_KERNEL_MAPPING);

	nvgpu_mutex_acquire(&g->mm.vidmem.clear_list_mutex);
	before_pending = atomic64_read(&g->mm.vidmem.bytes_pending.atomic_var);
	addr = __nvgpu_dma_alloc(vidmem_alloc, at, size);
	nvgpu_mutex_release(&g->mm.vidmem.clear_list_mutex);
	if (!addr) {
		/*
		 * If memory is known to be freed soon, let the user know that
		 * it may be available after a while.
		 */
		if (before_pending) {
			return -EAGAIN;
		} else {
			err = -ENOMEM;
			goto print_dma_err;
		}
	}

	if (at)
		mem->mem_flags |= NVGPU_MEM_FLAG_FIXED;

	mem->priv.sgt = nvgpu_kzalloc(g, sizeof(struct sg_table));
	if (!mem->priv.sgt) {
		err = -ENOMEM;
		goto fail_physfree;
	}

	err = sg_alloc_table(mem->priv.sgt, 1, GFP_KERNEL);
	if (err)
		goto fail_kfree;

	nvgpu_vidmem_set_page_alloc(mem->priv.sgt->sgl, addr);
	sg_set_page(mem->priv.sgt->sgl, NULL, size, 0);

	mem->aligned_size = size;
	mem->aperture = APERTURE_VIDMEM;
	mem->vidmem_alloc = (struct nvgpu_page_alloc *)(uintptr_t)addr;
	mem->allocator = vidmem_alloc;
	mem->priv.flags = flags;

	nvgpu_init_list_node(&mem->clear_list_entry);

	dma_dbg_alloc_done(g, mem->size, "vidmem");

	return 0;

fail_kfree:
	nvgpu_kfree(g, mem->priv.sgt);
fail_physfree:
	nvgpu_free(&g->mm.vidmem.allocator, addr);
	mem->size = 0;
print_dma_err:
	nvgpu_dma_print_err(g, size, "vidmem", "alloc", flags);
	return err;
}
#endif

void nvgpu_dma_free_sys(struct gk20a *g, struct nvgpu_mem *mem)
{
	struct device *d = dev_from_gk20a(g);
	unsigned long dma_attrs = 0;

	g->dma_memory_used -= mem->aligned_size;

	dma_dbg_free(g, mem->size, mem->priv.flags, "sysmem");

	if (!(mem->mem_flags & NVGPU_MEM_FLAG_SHADOW_COPY) &&
	    !(mem->mem_flags & NVGPU_MEM_FLAG_NO_DMA) &&
	    (mem->cpu_va || mem->priv.pages)) {
		void *cpu_addr = mem->cpu_va;

		/* These two use pages pointer instead of cpu_va */
		if (nvgpu_nvlink_non_contig(g, mem->priv.flags) ||
		    mem->priv.flags & NVGPU_DMA_NO_KERNEL_MAPPING)
			cpu_addr = mem->priv.pages;

		if (nvgpu_nvlink_non_contig(g, mem->priv.flags)) {
			vunmap(mem->cpu_va);
			nvgpu_dma_free_no_iommu(mem->aligned_size, cpu_addr);
		} else {
			nvgpu_dma_flags_to_attrs(g, &dma_attrs,
						 mem->priv.flags);
			dma_free_attrs(d, mem->aligned_size, cpu_addr,
					sg_dma_address(mem->priv.sgt->sgl),
					dma_attrs);
		}

		mem->cpu_va = NULL;
		mem->priv.pages = NULL;
	}

	/*
	 * When this flag is set this means we are freeing a "phys" nvgpu_mem.
	 * To handle this just nvgpu_kfree() the nvgpu_sgt and nvgpu_sgl.
	 */
	if (mem->mem_flags & NVGPU_MEM_FLAG_NO_DMA) {
		nvgpu_kfree(g, mem->phys_sgt->sgl);
		nvgpu_kfree(g, mem->phys_sgt);
	}

	if ((mem->mem_flags & NVGPU_MEM_FLAG_FOREIGN_SGT) == 0 &&
			mem->priv.sgt != NULL) {
		nvgpu_free_sgtable(g, &mem->priv.sgt);
	}

	dma_dbg_free_done(g, mem->size, "sysmem");

	mem->size = 0;
	mem->aligned_size = 0;
	mem->aperture = APERTURE_INVALID;
}

void nvgpu_dma_free_vid(struct gk20a *g, struct nvgpu_mem *mem)
{
#if defined(CONFIG_NVGPU_DGPU)
	size_t mem_size = mem->size;

	dma_dbg_free(g, mem->size, mem->priv.flags, "vidmem");

	/* Sanity check - only this supported when allocating. */
	WARN_ON(mem->priv.flags != NVGPU_DMA_NO_KERNEL_MAPPING);

	if (mem->mem_flags & NVGPU_MEM_FLAG_USER_MEM) {
		int err = nvgpu_vidmem_clear_list_enqueue(g, mem);

		/*
		 * If there's an error here then that means we can't clear the
		 * vidmem. That's too bad; however, we still own the nvgpu_mem
		 * buf so we have to free that.
		 *
		 * We don't need to worry about the vidmem allocator itself
		 * since when that gets cleaned up in the driver shutdown path
		 * all the outstanding allocs are force freed.
		 */
		if (err)
			nvgpu_kfree(g, mem);
	} else {
		nvgpu_memset(g, mem, 0, 0, mem->aligned_size);
		nvgpu_free(mem->allocator,
			   (u64)nvgpu_vidmem_get_page_alloc(mem->priv.sgt->sgl));
		nvgpu_free_sgtable(g, &mem->priv.sgt);

		mem->size = 0;
		mem->aligned_size = 0;
		mem->aperture = APERTURE_INVALID;
	}

	dma_dbg_free_done(g, mem_size, "vidmem");
#endif
}

int nvgpu_get_sgtable_attrs(struct gk20a *g, struct sg_table **sgt,
		      void *cpuva, u64 iova, size_t size, unsigned long flags)
{
	int err = 0;
	struct sg_table *tbl;
	unsigned long dma_attrs = 0;

	tbl = nvgpu_kzalloc(g, sizeof(struct sg_table));
	if (!tbl) {
		err = -ENOMEM;
		goto fail;
	}

	nvgpu_dma_flags_to_attrs(g, &dma_attrs, flags);
	err = dma_get_sgtable_attrs(dev_from_gk20a(g), tbl, cpuva, iova,
					size, dma_attrs);
	if (err)
		goto fail;

	sg_dma_address(tbl->sgl) = iova;
	*sgt = tbl;

	return 0;

fail:
	if (tbl)
		nvgpu_kfree(g, tbl);

	return err;
}

int nvgpu_get_sgtable(struct gk20a *g, struct sg_table **sgt,
		      void *cpuva, u64 iova, size_t size)
{
	return nvgpu_get_sgtable_attrs(g, sgt, cpuva, iova, size, 0);
}

int nvgpu_get_sgtable_from_pages(struct gk20a *g, struct sg_table **sgt,
				 struct page **pages, u64 iova, size_t size)
{
	int err = 0;
	struct sg_table *tbl;

	tbl = nvgpu_kzalloc(g, sizeof(struct sg_table));
	if (!tbl) {
		err = -ENOMEM;
		goto fail;
	}

	err = sg_alloc_table_from_pages(tbl, pages,
					DIV_ROUND_UP(size, NVGPU_CPU_PAGE_SIZE),
					0, size, GFP_KERNEL);
	if (err)
		goto fail;

	sg_dma_address(tbl->sgl) = iova;
	*sgt = tbl;

	return 0;

fail:
	if (tbl)
		nvgpu_kfree(g, tbl);

	return err;
}

void nvgpu_free_sgtable(struct gk20a *g, struct sg_table **sgt)
{
	sg_free_table(*sgt);
	nvgpu_kfree(g, *sgt);
	*sgt = NULL;
}

bool nvgpu_iommuable(struct gk20a *g)
{
#ifdef CONFIG_TEGRA_GK20A
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct device *dev = l->dev;

	/*
	 * Check against the nvgpu device to see if it's been marked as
	 * IOMMU'able.
	 */
	if (iommu_get_domain_for_dev(dev) == NULL)
		return false;
#endif

	return true;
}
