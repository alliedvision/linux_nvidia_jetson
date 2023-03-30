/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_KMEM_H
#define NVGPU_KMEM_H

/**
 * @file
 *
 * Kmem cache support
 * ------------------
 *
 * In Linux there is support for the notion of a kmem_cache. It gives better
 * memory usage characteristics for lots of allocations of the same size. Think
 * structs that get allocated over and over. Normal kmalloc() type routines
 * typically round to the next power-of-2 since that's easy.
 *
 * But if we know the size ahead of time the packing for the allocations can be
 * much better. This is the benefit of a slab allocator. This type hides the
 * underlying kmem_cache (or absense thereof).
 */

#include <nvgpu/types.h>
#include <nvgpu/utils.h>

struct gk20a;

/**
 * When there's other implementations make sure they are included instead of
 * Linux (i.e Qnx) when not compiling on Linux!
 */
#ifdef __KERNEL__
#include <nvgpu/linux/kmem.h>
#else
#include <nvgpu/posix/kmem.h>
#endif

struct nvgpu_kmem_cache;

#ifdef CONFIG_NVGPU_TRACK_MEM_USAGE
/**
 * Uncomment this if you want to enable stack traces in the memory profiling.
 * Since this is a fairly high overhead operation and is only necessary for
 * debugging actual bugs it's left here for developers to enable.
 */

#if 0
#define NVGPU_SAVE_KALLOC_STACK_TRACES
#endif

/**
 * Defined per-OS.
 */
struct nvgpu_mem_alloc_tracker;
#endif


/**
 * @brief Create an nvgpu memory cache.
 *
 * The internal implementation of the function is OS specific. In Posix
 * implementation, the function would just allocate a normal malloc memory for
 * the cache structure using \a malloc and populates the variables \a g and
 * \a size in struct #nvgpu_kmem_cache. This cache can be used to allocate
 * objects of size \a size. Common usage would be for a struct that gets
 * allocated a lot. In that case \a size should be sizeof(struct my_struct).
 * A given implementation of this need not do anything special. The allocation
 * routines can simply be passed on to #nvgpu_kzalloc() if desired, so packing
 * and alignment of the structs cannot be assumed. In Posix implementation, the
 * allocation from this cache would just return a normal malloc memory of size
 * \a size. Function does not perform any validation of the input parameters.
 *
 * @param g [in] The GPU driver struct using this cache.
 * @param size [in] Size of the object allocated by the cache.
 *
 * @return Pointer to #nvgpu_kmem_cache in case of success, else NULL.
 *
 * @retval NULL in case of failure.
 */
struct nvgpu_kmem_cache *nvgpu_kmem_cache_create(struct gk20a *g, size_t size);

/**
 * @brief Destroy a cache created by #nvgpu_kmem_cache_create().
 *
 * Destroy the allocated OS specific internal structure to avoid memory leak.
 * Uses the function \a free() with \a cache as parameter. Function does not
 * perform any validation of the parameter.
 *
 * @param cache [in] The cache to destroy.
 */
void nvgpu_kmem_cache_destroy(struct nvgpu_kmem_cache *cache);

/**
 * @brief Allocate an object from the cache
 *
 * Allocate an object from a cache created using #nvgpu_kmem_cache_create().
 * In Posix implementation, this function would just return a normal malloc
 * memory allocated using the function \a malloc with \a size in struct
 * #nvgpu_kmem_cache as parameter. Function does not perform any validation of
 * the parameter.
 *
 * @param cache [in] The cache to alloc from.
 *
 * @return Pointer to an object in case of success, else NULL.
 *
 * @retval NULL in case of failure.
 */
void *nvgpu_kmem_cache_alloc(struct nvgpu_kmem_cache *cache);

/**
 * @brief Free an object back to a cache
 *
 * Free an object back to a cache allocated using #nvgpu_kmem_cache_alloc().
 * Uses the function \a free with \a ptr as argument to free the allocated
 * memory. Function does not perform any validation of the input parameters.
 *
 * @param cache [in] The cache to return the object to.
 * @param ptr [in] Pointer to the object to free.
 */
void nvgpu_kmem_cache_free(struct nvgpu_kmem_cache *cache, void *ptr);

/**
 * @brief Macro to allocate memory.
 *
 * Allocate a chunk of system memory from the process address space.
 * This function may sleep so cannot be used in IRQs. Invokes the function
 * #nvgpu_kmalloc_impl() internally with \a g, \a size and #NVGPU_GET_IP as
 * parameters to allocate the required size of memory. Refer to the function
 * #nvgpu_kmalloc_impl() for further details. Macro does not perform any
 * validation of the parameters.
 *
 * @param g [in] Current GPU.
 * @param size [in] Size of the allocation.
 *
 * @return Pointer to an object in case of success, else NULL.
 *
 * @retval NULL in case of failure.
 */
#define nvgpu_kmalloc(g, size)	nvgpu_kmalloc_impl(g, size, NVGPU_GET_IP)

/**
 * @brief Allocate from the OS allocator.
 *
 * Identical to #nvgpu_kalloc() except the memory will be zeroed before being
 * returned. Invokes the function #nvgpu_kzalloc_impl() internally with \a g,
 * \a size and #NVGPU_GET_IP as parameters to allocate the required size of
 * memory. Refer to the function #nvgpu_kzalloc_impl() for further details.
 * Macro does not perform any validation of the parameters.
 *
 * @param g [in] Current GPU.
 * @param size [in] Size of the allocation.
 *
 * @return Pointer to an object in case of success, else NULL.
 *
 * @retval NULL in case of failure.
 */
#define nvgpu_kzalloc(g, size)	nvgpu_kzalloc_impl(g, size, NVGPU_GET_IP)

/**
 * @brief Allocate from the OS allocator.
 *
 * Identical to nvgpu_kalloc() except the size of the memory chunk returned is
 * \a n * \a size. Invokes the function #nvgpu_kcalloc_impl() internally
 * with \a g, \a n, \a size and #NVGPU_GET_IP as parameters to allocate the
 * required size of memory. Refer to the function #nvgpu_kcalloc_impl() for
 * further details. Macro does not perform any validation of the parameters.
 *
 * @param g [in] Current GPU.
 * @param n [in] Number of objects.
 * @param size [in] Size of each object.
 *
 * @return Pointer to an object in case of success, else NULL.
 *
 * @retval NULL in case of failure.
 */
#define nvgpu_kcalloc(g, n, size)	\
	nvgpu_kcalloc_impl(g, n, size, NVGPU_GET_IP)

/**
 * @brief Allocate memory and return a map to it.
 *
 * Allocate some memory and return a pointer to a virtual memory mapping of
 * that memory. The underlying physical memory is not guaranteed to be
 * contiguous. This allows for much larger allocations to be done without
 * worrying as much about physical memory fragmentation. This function may
 * sleep. Invokes the function #nvgpu_vmalloc_impl() with \a g, \a size and
 * #NVGPU_GET_IP as parameters to allocate the required memory. Refer to
 * #nvgpu_vmalloc_impl() for further details. Macro does not perform any
 * validation of the parameters.
 *
 * @param g [in] Current GPU.
 * @param size [in] Size of the allocation.
 *
 * @return Pointer to an object in case of success, else NULL.
 *
 * @retval NULL in case of failure.
 */
#define nvgpu_vmalloc(g, size)	nvgpu_vmalloc_impl(g, size, NVGPU_GET_IP)

/**
 * @brief Allocate memory and return a map to it.
 *
 * Identical to #nvgpu_vmalloc(), except, this macro will return zero
 * initialized memory. Invokes the function #nvgpu_vxalloc_impl() with \a g,
 * \a size and #NVGPU_GET_IP as parameters to allocate the required memory.
 * Refer to #nvgpu_vzalloc_impl() for further details. Macro does not perform
 * any validation of the parameters.
 *
 * @param g [in] Current GPU.
 * @param size [in] Size of the allocation.
 *
 * @return Pointer to an object in case of success, else NULL.
 *
 * @retval NULL in case of failure.
 */
#define nvgpu_vzalloc(g, size)	nvgpu_vzalloc_impl(g, size, NVGPU_GET_IP)

/**
 * @brief Macro to free an allocated chunk of memory.
 *
 * Invokes the function #nvgpu_kfree_impl() with \a g and \a addr as parameters
 * to free the memory. Macro does not perform any validation of the parameters.
 *
 * @param g [in] Current GPU.
 * @param addr [in] Address of object to free.
 */
#define nvgpu_kfree(g, addr)	nvgpu_kfree_impl(g, addr)

/**
 * @brief Frees an allocated chunk of virtual memory.
 *
 * Invokes the function #nvgpu_vfree_impl() with \a g and \a addr as parameters
 * to free the memory. Macro does not perform any validation of the parameters.
 *
 * @param g [in] Current GPU.
 * @param addr [in] Address of object to free.
 */
#define nvgpu_vfree(g, addr)	nvgpu_vfree_impl(g, addr)

#define kmem_dbg(g, fmt, args...)		\
	nvgpu_log(g, gpu_dbg_kmem, fmt, ##args)

/**
 * @brief Initialize the kmem tracking stuff.
 *
 * Initialize the kmem tracking internal structure. Internal implementation
 * is specific to OS. In Posix implementation, the function just returns 0.
 * Function does not perform any validation of the parameter.
 *
 * @param g [in] GPU structure.
 *
 * @return 0 in case of success, non-zero in case of failure. Posix
 * implementation is a dummy function which just returns 0.
 *
 * @retval 0 for Posix implementation.
 */
int nvgpu_kmem_init(struct gk20a *g);

/**
 * @brief Finalize the kmem tracking code
 *
 * The implementation of the function is OS specific. In Posix implementation
 * the function does not perform any operation.
 *
 * @param g [in] The GPU.
 * @param flags [in] Flags that control operation of this finalization.
 */
void nvgpu_kmem_fini(struct gk20a *g, int flags);

/**
 * These will simply be ignored if CONFIG_NVGPU_TRACK_MEM_USAGE is not defined.
 */
#define NVGPU_KMEM_FINI_DO_NOTHING		0
#define NVGPU_KMEM_FINI_FORCE_CLEANUP		(1 << 0)
#define NVGPU_KMEM_FINI_DUMP_ALLOCS		(1 << 1)
#define NVGPU_KMEM_FINI_WARN			(1 << 2)
#define NVGPU_KMEM_FINI_BUG			(1 << 3)

/**
 * @brief Wrapper for memory allocation functions.
 *
 * The internal implementation of this function is OS specific. For Posix
 * implementation, the requested \a size of memory is allocated and returns
 * a pointer to that memory. The parameter \a clear is used to decide if the
 * allocated memory has to be zero filled or not. Based on \a clear value,
 * #nvgpu_kzalloc or #nvgpu_kmalloc is used internally with \a g and \a size
 * as parameters. Function does not perform any validation of the parameters.
 *
 * @param g [in] The GPU.
 * @param size [in] Size of the allocation.
 * @param clear [in] Flag indicates the need of zeroed memory.
 *
 * @return upon successful allocation a pointer to a virtual address range
 * that the nvgpu can access is returned, else NULL.
 *
 * @retval NULL in case of failure.
 */
void *nvgpu_big_alloc_impl(struct gk20a *g, size_t size, bool clear);

/**
 * @brief Macro to allocate memory
 *
 * Invokes the function #nvgpu_big_alloc_impl() internally with \a g, \a size
 * and \a false as parameters. Refer to #nvgpu_big_alloc_impl() for further
 * details. Function does not perform any validation of the input parameters.
 *
 * @param g [in] The GPU.
 * @param size [in] Size of the allocation.
 *
 * @return upon successful allocation a pointer to a virtual address range
 * that the nvgpu can access is returned, else NULL.
 *
 * @retval NULL in case of failure.
 */
static inline void *nvgpu_big_malloc(struct gk20a *g, size_t size)
{
	return nvgpu_big_alloc_impl(g, size, false);
}

/**
 * @brief Macro to allocate a zero initialised memory.
 *
 * Invokes the function #nvgpu_big_alloc_impl() internally with \a g, \a size
 * and \a true as parameters to allocated zero initialized memory. Refer to
 * #nvgpu_big_alloc_impl() for further details. Function does not perform any
 * validation of the input parameters.
 *
 * @param g [in] The GPU.
 * @param size [in] Size of the allocation.
 *
 * @return upon successful allocation a pointer to a virtual address range
 * that the nvgpu can access is returned, else NULL.
 *
 * @retval NULL in case of failure.
 */
static inline void *nvgpu_big_zalloc(struct gk20a *g, size_t size)
{
	return nvgpu_big_alloc_impl(g, size, true);
}

/**
 * @brief Free any allocated memory.
 *
 * Invokes the function #nvgpu_kfree_impl() with \a g and \a p as parameters
 * internally to free any allocated memory. Function does not perform any
 * validation of the parameters.
 *
 * @param g [in] The GPU.
 * @param p [in] Pointer to allocated memory.
 */
void nvgpu_big_free(struct gk20a *g, void *p);

#endif /* NVGPU_KMEM_H */
