/*
 * GK20A Address Spaces
 *
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_AS_H
#define NVGPU_AS_H

#include <nvgpu/types.h>

struct vm_gk20a;
struct gk20a;

/**
 * Basic structure to identify an address space (AS).
 */
struct gk20a_as {
	/**
	 * Incrementing id to identify the AS, dummy allocator for now.
	 */
	int last_share_id;
};

/**
 * Basic structure to share an AS.
 */
struct gk20a_as_share {
	/**
	 * The AS to share.
	 */
	struct gk20a_as *as;

	/**
	 * The VM used by the AS.
	 */
	struct vm_gk20a *vm;

	/**
	 * Simple incrementing id to identify the share.
	 */
	int id;
};

/**
 * AS allocation flag for userspace managed
 */
#define NVGPU_AS_ALLOC_USERSPACE_MANAGED	BIT32(0)

/**
 * AS allocation flag for unified VA
 */
#define NVGPU_AS_ALLOC_UNIFIED_VA		BIT32(1)

/**
 * @brief Release an AS share.
 *
 * @param as_share [in] The address space share to release.
 *
 * Release the address space share \a as_share that is created
 * by gk20a_as_alloc_share().
 *
 * @return	 EOK in case of success, < 0 in case of failure.
 *
 * @retval	-ENODEV For struct g is NULL.
 * @retval	-EINVAL For the power contxt associated with struct nvgpu_os_rmos
 * 		is NULL.
 * @retval	-EINVAL For the power function pointer associated with struct
 * 		nvgpu_module is NULL.
 * @retval	-EIO For setting clock related failures.
 *
 */
int gk20a_as_release_share(struct gk20a_as_share *as_share);

/**
 * @brief Set internal pointers to NULL and decrement reference count on the VM.
 *
 * @param as_share [in] The address space share to release.
 *
 * Release the \a as_share provided in argument by marking the internal pointers
 * between the share and its VM as NULL, and freeing it.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int gk20a_vm_release_share(struct gk20a_as_share *as_share);

/**
 * @brief Allocate an AS share.
 *
 * @param g [in]			The GPU
 * @param big_page_size [in]		Big page size to use for the VM,
 *					set 0 for 64K big page size.
 * @param flags [in]			NVGPU_AS_ALLOC_* flags. The flags are
 * 					NVGPU_AS_ALLOC_USERSPACE_MANAGED and
 * 					NVGPU_AS_ALLOC_UNIFIED_VA.
 * @param va_range_start [in]		Requested user managed memory start
 *					address, used to map buffers, save data
 *					should be aligned by PDE
 * @param va_range_end [in]		Requested user managed va end address
 *					should be aligned by PDE
 * @param va_range_split [in]		Requested small/big page split
 *					should be aligned by PDE,
 *					ignored if UNIFIED_VA is set
 * @params out [out]			The resulting, allocated, gk20a_as_share
 *					structure
 *
 * Allocate the gk20a_as_share structure and the VM associated with it, based
 *  on the provided \a big_page_size and NVGPU_AS_ALLOC_* \a flags.
 * Check the validity of \a big_page_size by the big_page_size should be power
 *  of two and it should be in the range supported big page sizes supported by the GPU.
 *
 * @note	if \a big_page_size == 0, the default big page size(64K) is used.
 * @note	The \a flags is always set as NVGPU_AS_ALLOC_USERSPACE_MANAGED(AS
 * 		allocation flag for userspace managed)
 *
 * @return	0 in case of success, < 0 in case of failure.
 *
 * @retval	-ENODEV For struct GPU is NULL.
 * @retval	-EIO For setting clock related failures.
 * @retval	-ENOMEM For memory allocation failures.
 * @retval	-EINVAL For any parameter compute failures from gk20a_vm_alloc_share().
 * @retval	-ENOMEM For allocated VM is NULL.
 *
 */
int gk20a_as_alloc_share(struct gk20a *g, u32 big_page_size,
			u32 flags, u64 va_range_start,
			u64 va_range_end, u64 va_range_split,
			struct gk20a_as_share **out);

/**
 * @brief Retrieve the instance of gk20a from a gk20a_as instance.
 *
 * @param as [in] The address space
 *
 * Given an instance of gk20a_as, retrieve a pointer to the underlying gk20a
 * instance.
 *
 * @return pointer to the underlying GPU (gk20a).
 */
struct gk20a *gk20a_from_as(struct gk20a_as *as);
#endif /* NVGPU_AS_H */
