/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/bug.h>
#include <nvgpu/dma.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/kmem.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/nvgpu_sgt_os.h>
#include <nvgpu/gk20a.h>
#include <os/posix/os_posix.h>

#define DMA_ERROR_CODE	(~(u64)0x0)

/*
 * This function (and the get_addr() and get_phys_addr() functions are somewhat
 * meaningless in userspace.
 *
 * There is no GPU in the loop here, so defining a "GPU physical" address is
 * difficult. What we do here is simple but limited. We'll treat the GPU physical
 * address as just the bottom 32 bits of the CPU virtual address. Since the driver
 * shouldn't be dereferencing these pointers in the first place that's sufficient
 * to make most tests work. The reason we truncate the CPU VA is because the
 * address returned from this is programmed into the GMMU PTEs/PDEs. That code
 * asserts that the address is a valid GPU physical address (i.e less than some
 * number of bits, depending on chip).
 *
 * However, this does lead to some potential quirks: GPU addresses of different
 * CPU virtual addresses could alias (e.g B and B + 4GB will both result in the
 * same value when ANDing with 0xFFFFFFFF.
 *
 * If there is a buffer with an address range that crosses a 4GB boundary it'll
 * be detected here. A more sophisticated buffer to GPU virtual address approach
 * could be taken, but for now this is probably sufficient. At least for one run
 * through the unit test framework, the CPU malloc() address range seemed to be
 * 0x555555000000 - this is a long way away from any 4GB boundary.
 *
 * For invalid nvgpu_mems and nvgpu_mems with no cpu_va, just return NULL.
 * There's little else we can do. In many cases in the unit test FW we wind up
 * getting essentially uninitialized nvgpu_mems.
 */
static u64 nvgpu_mem_userspace_get_addr(struct gk20a *g, struct nvgpu_mem *mem)
{
	u64 hi_front = ((u64)(uintptr_t)mem->cpu_va) & ~0xffffffffUL;
	u64 hi_back  = ((u64)(uintptr_t)mem->cpu_va + mem->size - 1U) & ~0xffffffffUL;

	if (!nvgpu_mem_is_valid(mem) || mem->cpu_va == NULL) {
		return 0x0UL;
	}

	if (hi_front != hi_back) {
		nvgpu_err(g, "Mismatching cpu_va calc.");
		nvgpu_err(g, "  valid = %s", nvgpu_mem_is_valid(mem) ? "yes" : "no");
		nvgpu_err(g, "  cpu_va = %p", mem->cpu_va);
		nvgpu_err(g, "  size   = %lx", mem->size);
		nvgpu_err(g, "  hi_front = 0x%llx", hi_front);
		nvgpu_err(g, "  hi_back  = 0x%llx", hi_back);
	}

	nvgpu_assert(hi_front == hi_back);

	return ((u64)(uintptr_t)mem->cpu_va) & 0xffffffffUL;
}

u64 nvgpu_mem_get_addr(struct gk20a *g, struct nvgpu_mem *mem)
{
	return nvgpu_mem_userspace_get_addr(g, mem);
}

u64 nvgpu_mem_get_phys_addr(struct gk20a *g, struct nvgpu_mem *mem)
{
	return nvgpu_mem_userspace_get_addr(g, mem);
}

void *nvgpu_mem_sgl_next(void *sgl)
{
	struct nvgpu_mem_sgl *mem = (struct nvgpu_mem_sgl *)sgl;

	return (void *) mem->next;
}

u64 nvgpu_mem_sgl_phys(struct gk20a *g, void *sgl)
{
	struct nvgpu_mem_sgl *mem = (struct nvgpu_mem_sgl *)sgl;

	return (u64)(uintptr_t)mem->phys;
}

u64 nvgpu_mem_sgl_ipa_to_pa(struct gk20a *g, void *sgl, u64 ipa, u64 *pa_len)
{
	return nvgpu_mem_sgl_phys(g, sgl);
}

u64 nvgpu_mem_sgl_dma(void *sgl)
{
	struct nvgpu_mem_sgl *mem = (struct nvgpu_mem_sgl *)sgl;

	return (u64)(uintptr_t)mem->dma;
}

u64 nvgpu_mem_sgl_length(void *sgl)
{
	struct nvgpu_mem_sgl *mem = (struct nvgpu_mem_sgl *)sgl;

	return (u64)mem->length;
}

u64 nvgpu_mem_sgl_gpu_addr(struct gk20a *g, void *sgl,
				  struct nvgpu_gmmu_attrs *attrs)
{
	struct nvgpu_mem_sgl *mem = (struct nvgpu_mem_sgl *)sgl;

	if (mem->dma == 0U) {
		return g->ops.mm.gmmu.gpu_phys_addr(g, attrs, mem->phys);
	}

	if (mem->dma == DMA_ERROR_CODE) {
		return 0x0;
	}

	return nvgpu_mem_iommu_translate(g, mem->dma);
}

bool nvgpu_mem_sgt_iommuable(struct gk20a *g, struct nvgpu_sgt *sgt)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	return p->mm_sgt_is_iommuable;
}

void nvgpu_mem_sgl_free(struct gk20a *g, struct nvgpu_mem_sgl *sgl)
{
	struct nvgpu_mem_sgl *tptr;

	while (sgl != NULL) {
		tptr = sgl->next;
		nvgpu_kfree(g, sgl);
		sgl = tptr;
	}
}

void nvgpu_mem_sgt_free(struct gk20a *g, struct nvgpu_sgt *sgt)
{
	nvgpu_mem_sgl_free(g, (struct nvgpu_mem_sgl *)sgt->sgl);
	nvgpu_kfree(g, sgt);
}

static struct nvgpu_sgt_ops nvgpu_sgt_posix_ops = {
	.sgl_next	= nvgpu_mem_sgl_next,
	.sgl_phys	= nvgpu_mem_sgl_phys,
	.sgl_ipa	= nvgpu_mem_sgl_phys,
	.sgl_ipa_to_pa	= nvgpu_mem_sgl_ipa_to_pa,
	.sgl_dma	= nvgpu_mem_sgl_dma,
	.sgl_length	= nvgpu_mem_sgl_length,
	.sgl_gpu_addr	= nvgpu_mem_sgl_gpu_addr,
	.sgt_iommuable	= nvgpu_mem_sgt_iommuable,
	.sgt_free	= nvgpu_mem_sgt_free,
};

struct nvgpu_mem_sgl *nvgpu_mem_sgl_posix_create_from_list(struct gk20a *g,
				struct nvgpu_mem_sgl *sgl_list, u32 nr_sgls,
				u64 *total_size)
{
	struct nvgpu_mem_sgl *sgl_ptr, *tptr, *head = NULL;
	u32 i;

	*total_size = 0;
	for (i = 0; i < nr_sgls; i++) {
		tptr = (struct nvgpu_mem_sgl *)nvgpu_kzalloc(g,
						sizeof(struct nvgpu_mem_sgl));
		if (tptr == NULL) {
			goto err;
		}

		if (i == 0U) {
			sgl_ptr = tptr;
			head = sgl_ptr;
		} else {
			sgl_ptr->next = tptr;
			sgl_ptr = sgl_ptr->next;
		}
		sgl_ptr->next = NULL;
		sgl_ptr->phys = sgl_list[i].phys;
		sgl_ptr->dma = sgl_list[i].dma;
		sgl_ptr->length = sgl_list[i].length;
		*total_size += sgl_list[i].length;
	}

	return head;

err:
	while (head != NULL) {
		struct nvgpu_mem_sgl *tmp = head;
		head = tmp->next;
		nvgpu_kfree(g, tmp);
	}
	return NULL;
}

struct nvgpu_sgt *nvgpu_mem_sgt_posix_create_from_list(struct gk20a *g,
				struct nvgpu_mem_sgl *sgl_list, u32 nr_sgls,
				u64 *total_size)
{
	struct nvgpu_sgt *sgt = nvgpu_kzalloc(g, sizeof(struct nvgpu_sgt));
	struct nvgpu_mem_sgl *sgl;

	if (sgt == NULL) {
		return NULL;
	}

	sgl = nvgpu_mem_sgl_posix_create_from_list(g, sgl_list, nr_sgls,
				total_size);
	if (sgl == NULL) {
		nvgpu_kfree(g, sgt);
		return NULL;
	}
	sgt->sgl = (void *)sgl;
	sgt->ops = &nvgpu_sgt_posix_ops;

	return sgt;
}

int nvgpu_mem_posix_create_from_list(struct gk20a *g, struct nvgpu_mem *mem,
				struct nvgpu_mem_sgl *sgl_list, u32 nr_sgls)
{
	u64 sgl_size;

	mem->priv.sgt = nvgpu_mem_sgt_posix_create_from_list(g, sgl_list,
				nr_sgls, &sgl_size);
	if (mem->priv.sgt == NULL) {
		return -ENOMEM;
	}

	mem->aperture = APERTURE_SYSMEM;
	mem->aligned_size = PAGE_ALIGN(sgl_size);
	mem->size = sgl_size;

	return 0;
}

struct nvgpu_sgt *nvgpu_sgt_os_create_from_mem(struct gk20a *g,
					       struct nvgpu_mem *mem)
{
	struct nvgpu_mem_sgl *sgl;
	struct nvgpu_sgt *sgt;

	if (mem->priv.sgt != NULL) {
		return mem->priv.sgt;
	}

	sgt = nvgpu_kzalloc(g, sizeof(*sgt));
	if (sgt == NULL) {
		return NULL;
	}

	sgt->ops = &nvgpu_sgt_posix_ops;

	/*
	 * The userspace implementation is simple: a single 'entry' (which we
	 * only need the nvgpu_mem_sgl struct to describe). A unit test can
	 * easily replace it if needed.
	 */
	sgl = (struct nvgpu_mem_sgl *) nvgpu_kzalloc(g, sizeof(
		struct nvgpu_mem_sgl));
	if (sgl == NULL) {
		nvgpu_kfree(g, sgt);
		return NULL;
	}

	sgl->length = mem->size;
	sgl->phys   = (u64) mem->cpu_va;
	sgt->sgl    = (void *) sgl;

	return sgt;
}

int nvgpu_mem_create_from_mem(struct gk20a *g,
			      struct nvgpu_mem *dest, struct nvgpu_mem *src,
			      u64 start_page, size_t nr_pages)
{
	u64 start = start_page * U64(NVGPU_CPU_PAGE_SIZE);
	u64 size = U64(nr_pages) * U64(NVGPU_CPU_PAGE_SIZE);

	if (src->aperture != APERTURE_SYSMEM) {
		return -EINVAL;
	}

	/* Some silly things a caller might do... */
	if (size > src->size) {
		return -EINVAL;
	}
	if ((start + size) > src->size) {
		return -EINVAL;
	}

	(void) memset(dest, 0, sizeof(*dest));

	dest->cpu_va    = ((char *)src->cpu_va) + start;
	dest->mem_flags = src->mem_flags | NVGPU_MEM_FLAG_SHADOW_COPY;
	dest->aperture  = src->aperture;
	dest->skip_wmb  = src->skip_wmb;
	dest->size      = size;

	return 0;
}

int __nvgpu_mem_create_from_phys(struct gk20a *g, struct nvgpu_mem *dest,
				 u64 src_phys, int nr_pages)
{
	BUG();
	return 0;
}
