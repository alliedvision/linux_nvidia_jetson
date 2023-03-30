/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_SGT_H
#define NVGPU_SGT_H

/**
 * @file
 *
 * Abstract interface for interacting with the scatter gather list entries.
 */

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_mem;
struct nvgpu_gmmu_attrs;

struct nvgpu_sgt;

/**
 * This structure holds the necessary operations required for
 * interacting with the underlying scatter gather list entries.
 */
struct nvgpu_sgt_ops {
	/**
	 * Used to get the next scatter gather list entry in the
	 * scatter gather list entries.
	 */
	void *(*sgl_next)(void *sgl);
	/**
	 * Used to get the physical address associated with the
	 * scatter gather list entry.
	 */
	u64   (*sgl_phys)(struct gk20a *g, void *sgl);
	/**
	 * Used to get the intermediate physical address associated with the
	 * scatter gather list entry.
	 */
	u64   (*sgl_ipa)(struct gk20a *g, void *sgl);
	/**
	 * Used to get the physical address from the intermediate
	 * physical address.
	 */
	u64   (*sgl_ipa_to_pa)(struct gk20a *g, void *sgl,
				u64 ipa, u64 *pa_len);
	/**
	 * Used to get the iommuable virtual address associated with the
	 * scatter gather list entry.
	 */
	u64   (*sgl_dma)(void *sgl);
	/**
	 * Used to get the length associated with the scatter gather list
	 * entry.
	 */
	u64   (*sgl_length)(void *sgl);
	/**
	 * Used to get the gpu understandable physical address from the
	 * soc physical address.
	 */
	u64   (*sgl_gpu_addr)(struct gk20a *g, void *sgl,
			      struct nvgpu_gmmu_attrs *attrs);
	/**
	 * Used to get the iommu on/off status.
	 * If left NULL then iommuable is assumed to be false.
	 */
	bool  (*sgt_iommuable)(struct gk20a *g, struct nvgpu_sgt *sgt);
	/**
	 * Function ptr to free the entire scatter gather table.
	 * Note: this operates on the whole scatter gather table not a
	 * specific scatter gather list entry.
	 */
	void  (*sgt_free)(struct gk20a *g, struct nvgpu_sgt *sgt);
};

/**
 * Scatter gather table: this is a list of scatter list entries and the ops for
 * interacting with those entries.
 */
struct nvgpu_sgt {
	/**
	 * Ops for interacting with the underlying scatter gather list entries.
	 */
	const struct nvgpu_sgt_ops *ops;

	/**
	 * The first node in the scatter gather list.
	 */
	void *sgl;
};

/**
 * This struct holds the necessary information for describing a struct
 * nvgpu_mem's scatter gather list.
 *
 * This is one underlying implementation for nvgpu_sgl. Not all nvgpu_sgt's use
 * this particular implementation. Nor is a given OS required to use this at
 * all.
 */
struct nvgpu_mem_sgl {
	/**
	 * Pointer to a next scatter gather list entry in this scatter
	 * gather list entries. Internally this is implemented as a singly
	 * linked list.
	 */
	struct nvgpu_mem_sgl	*next;
	/**
	 * Physical address associated with this scatter gather list entry.
	 */
	u64			 phys;
	/**
	 * Iommuable virtual address associated with this scatter gather
	 * list entry.
	 */
	u64			 dma;
	/**
	 * Length associated with this scatter gather list entry.
	 */
	u64			 length;
};

/**
 * Iterate over the SGL entries in an SGT.
 */
#define nvgpu_sgt_for_each_sgl(sgl, sgt)		\
	for ((sgl) = (sgt)->sgl;			\
	     (sgl) != NULL;				\
	     (sgl) = nvgpu_sgt_get_next(sgt, sgl))

/**
 * nvgpu_mem_sgt_create_from_mem - Create a scatter list from an nvgpu_mem.
 *
 * @param g   [in]	The GPU.
 * @param mem [in]	Pointer to nvgpu_mem object.
 *
 * Create a scatter gather table from the passed @mem struct. This list lets
 * the calling code iterate across each chunk of a DMA allocation for when
 * that DMA allocation is not completely contiguous.
 * Since a DMA allocation may well be discontiguous nvgpu requires that a
 * table describe the chunks of memory that make up the DMA allocation. This
 * scatter gather table, SGT, must be created from an nvgpu_mem. Since the
 * representation of a DMA allocation varies wildly from OS to OS.
 *
 * The nvgpu_sgt object returned by this function must be freed by the
 * nvgpu_sgt_free() function.
 *
 * @return nvgpu_sgt object on success.
 * @return NULL when there's an error.
 * Possible failure case:
 * - Insufficient system memory.
 */
struct nvgpu_sgt *nvgpu_sgt_create_from_mem(struct gk20a *g,
					    struct nvgpu_mem *mem);

/**
 * @brief Get the next scatter gather list structure from the given
 *        scatter gather list.
 *
 * @param sgt [in]	Pointer to scatter gather table object.
 * @param sgl [in]	Pointer to scatter gather list structure.
 *
 * - Get the #nvgpu_sgt_ops->sgl_next associated with the given
 *   scatter gather table object.
 * - Check the input scatter gather list is NULL or not.
 * - If the input list is not null, return the field next associated with
 *   the list.
 *
 * @return Pointer to a scatter gather list.
 * @return NULL if there is no next scatter gather list is found.
 */
void *nvgpu_sgt_get_next(struct nvgpu_sgt *sgt, void *sgl);

/**
 * @brief Get the intermediate physical address from given scatter
 *        gather list.
 *
 * @param g   [in]	The GPU.
 * @param sgt [in]	Pointer to scatter gather table object.
 * @param sgl [in]	Pointer to struct scatter gather list.
 *
 * - Get the #nvgpu_sgt_ops->sgl_ipa associated with the given
 *   scatter gather table object.
 * - Call nvgpu_sgl_ipa() to get the intermediate physical address associated
 *   with the scatter gather list.
 *   -- nvgpu_sgl_ipa() will check whether given list is NULL or not. If it is
 *      not null it will return the field phys associated with the scatter
 *      gather list.
 *
 * @return Intermediate physical address associated with the
 *         given scatter gather list.
 */
u64 nvgpu_sgt_get_ipa(struct gk20a *g, struct nvgpu_sgt *sgt, void *sgl);

/**
 * @brief Get the physical address from the intermediate physical address
 *        associated with the scatter gather list.
 *
 * @param g      [in]   The GPU.
 * @param sgt    [in]   Pointer to scatter gather table object.
 * @param sgl    [in]   Pointer to scatter gather list.
 * @param ipa    [in]   Intermediate physical address to be converted.
 * @param pa_len [out]  Offset associated with the physical address.
 *
 * - Get the #nvgpu_sgt_ops->sgl_ipa_to_pa associated with the given
 *   scatter gather table object.
 * - Get the module descriptor from the given struct GPU.
 * - Query the hypervisor to convert given ipa to pa by calling the
 *   function associated with module descriptor.
 *
 * @return Physical address to the input intermediate physical
 *         address.
 */
u64 nvgpu_sgt_ipa_to_pa(struct gk20a *g, struct nvgpu_sgt *sgt,
				void *sgl, u64 ipa, u64 *pa_len);

/**
 * @brief Get the physical address associated with the scatter gather list.
 *
 * @param g   [in]	The GPU.
 * @param sgt [in]	Pointer to scatter gather table object.
 * @param sgl [in]	Pointer to scatter gather list.
 *
 * - Get the #nvgpu_sgt_ops->sgl_phys associated with the given
 *   scatter gather table object.
 * - Call nvgpu_sgl_phys() to get the physical address associated with the
 *   sgl.
 *   -- nvgpu_sgl_phys() will get the module descriptor from the given struct
 *      GPU. Query the hypervisor to convert given ipa to pa by calling the
 *      function associated with module descriptor.
 *
 * @return Physical address associated with the input sgl.
 */
u64 nvgpu_sgt_get_phys(struct gk20a *g, struct nvgpu_sgt *sgt, void *sgl);

/**
 * @brief Get the io virtual address associated with the scatter
 *        gather list.
 *
 * @param sgt [in]	Pointer to scatter gather table object.
 * @param sgl [in]	Pointer to struct sgl.
 *
 * - Get the #nvgpu_sgt_ops->sgl_dma associated with the given
 *   scatter gather table object.
 * - Return the dma associated with the given scatter gather list, if
 *   the scatter gather list is not null.
 *
 * @return Intermediate physical address.
 */
u64 nvgpu_sgt_get_dma(struct nvgpu_sgt *sgt, void *sgl);

/**
 * @brief Get the length associated with given scatter gather list.
 *
 * @param sgt [in]	Pointer to scatter gather table object.
 * @param sgl [in]	Pointer to scatter gather table struct.
 *
 * - Get the #nvgpu_sgt_ops->sgl_length associated with the given
 *   scatter gather table object.
 * - Return the length associated with the given scatter gather list, if
 *   the scatter gather list is not null.
 *
 * @return Length associated with the input sgl.
 */
u64 nvgpu_sgt_get_length(struct nvgpu_sgt *sgt, void *sgl);

/**
 * @brief Get the physical address/intermediate physical address
 *        associated with the given sgl.
 *
 * @param g     [in]	The GPU.
 * @param sgt   [in]	Pointer to scatter gather table object.
 * @param sgl   [in]	Pointer to scatter gather table struct.
 * @param attrs [in]	Attributes describes about the GPU
 *                      memory management unit.
 *
 * - Get the #nvgpu_sgt_ops->sgl_gpu_addr associated with the given
 *   scatter gather table object.
 * - Check the address associated with given scatter list is io virtual address
 *   or intermediate physical address(ipa).
 * - If the address is ipa, Query hypervisor to convert to physical address.
 * - If the address is iova, call nvgpu_mem_iommu_translate() to get physical
 *   address.
 *
 * @return Address associated with the given sgl.
 */
u64 nvgpu_sgt_get_gpu_addr(struct gk20a *g, struct nvgpu_sgt *sgt,
			void*sgl, struct nvgpu_gmmu_attrs *attrs);

/**
 * @brief Free the scatter gather table object.
 *
 * @param g   [in]	The GPU.
 * @param sgt [in]	Pointer to scatter gather table object.
 *
 * - Get the #nvgpu_sgt_ops->sgt_free associated with the given
 *   scatter gather table object.
 * - Walk through the given sgl list by sgl.next and free the sgl.next.
 */
void nvgpu_sgt_free(struct gk20a *g, struct nvgpu_sgt *sgt);

/**
 * @brief Check the given scatter gather table is iommu supported or not.
 *
 * @param g   [in]	The GPU.
 * @param sgt [in]	Pointer to scatter gather table object.
 *
 * - Get the #nvgpu_sgt_ops->sgt_iommuable associated with the given
 *   scatter gather table object.
 * - Call nvgpu_iommuable().
 *   -- As the GPU is not iommuable, nvgpu_iommuable() will alway return
 *      FALSE.
 *
 * @return TRUE, if sgt is iommuable.
 * @return FALSE, if sgt is not iommuable.
 */
bool nvgpu_sgt_iommuable(struct gk20a *g, struct nvgpu_sgt *sgt);

/**
 * @brief Determine alignment for a scatter gather table object.
 *
 * @param g   [in]	The GPU.
 * @param sgt [in]	Pointer to scatter gather table object.
 *
 * Determine alignment for a passed buffer. This is necessary
 * since the buffer may appear big enough to be mapped with large pages.
 * However, the SGL may have chunks that are not aligned on 64/128kB large
 * page boundary. There's also a possibility that chunks are of odd sizes
 * which will necessitate small page mappings to correctly glue them
 * together into a contiguous virtual mapping.
 * If this SGT is iommuable and we want to use the IOMMU address, then
 * the SGT's first entry has the IOMMU address. We will align on this
 * and double check the length of the buffer later.
 * In addition, we know that this DMA address is contiguous
 * since there's an IOMMU.
 *
 * @return The minimum expected alignment in bytes.
 */
u64 nvgpu_sgt_alignment(struct gk20a *g, struct nvgpu_sgt *sgt);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#if defined(__NVGPU_POSIX__)
void *nvgpu_mem_sgl_next(void *sgl);
u64 nvgpu_mem_sgl_phys(struct gk20a *g, void *sgl);
u64 nvgpu_mem_sgl_ipa_to_pa(struct gk20a *g, void *sgl, u64 ipa,
				u64 *pa_len);
u64 nvgpu_mem_sgl_dma(void *sgl);
u64 nvgpu_mem_sgl_length(void *sgl);
u64 nvgpu_mem_sgl_gpu_addr(struct gk20a *g, void *sgl,
				  struct nvgpu_gmmu_attrs *attrs);
bool nvgpu_mem_sgt_iommuable(struct gk20a *g, struct nvgpu_sgt *sgt);
void nvgpu_mem_sgt_free(struct gk20a *g, struct nvgpu_sgt *sgt);
#endif
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif /* NVGPU_SGT_H */
