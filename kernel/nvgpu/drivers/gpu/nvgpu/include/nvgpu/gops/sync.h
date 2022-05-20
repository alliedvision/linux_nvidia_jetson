/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_SYNC_H
#define NVGPU_GOPS_SYNC_H

#include <nvgpu/types.h>


/**
 * @file
 *
 * Sync HAL interface.
 */
struct gk20a;
struct nvgpu_channel;
struct nvgpu_mem;
struct vm_gk20a;
struct priv_cmd_entry;
struct nvgpu_semaphore;

struct gops_sync_syncpt {
	/**
	 * @brief Map syncpoint aperture as read-only.
	 *
	 * @param vm [in]		VM area for channel.
	 * @param base_gpu [out]	Base GPU VA for mapped
	 * 				syncpoint aperture.
	 * @param sync_size [out]	Size per syncpoint in bytes.
	 * @param num_syncpoints [out]	Number of syncpoints in the
	 * 				aperture.
	 *
	 * Map syncpoint aperture in GPU virtual memory as read-only:
	 * - Acquire syncpoint read-only map lock.
	 * - Map syncpoint aperture in sysmem to GPU virtual memory,
	 *   if not already mapped. Map as read-only.
	 * - Release syncpoint read-only map lock.
	 *
	 * The syncpoint shim mapping size is sync_size * num_syncpoints.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ENOMEM if syncpoint aperture could not be
	 *         mapped to GPU virtual memory.
	 */
	int (*get_sync_ro_map)(struct vm_gk20a *vm,
			u64 *base_gpuva, u32 *sync_size, u32 *num_syncpoints);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	int (*alloc_buf)(struct nvgpu_channel *c,
			u32 syncpt_id,
			struct nvgpu_mem *syncpt_buf);
	void (*free_buf)(struct nvgpu_channel *c,
			struct nvgpu_mem *syncpt_buf);
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	void (*add_wait_cmd)(struct gk20a *g,
			struct priv_cmd_entry *cmd,
			u32 id, u32 thresh, u64 gpu_va_base);
	u32 (*get_wait_cmd_size)(void);
	void (*add_incr_cmd)(struct gk20a *g,
			struct priv_cmd_entry *cmd,
			u32 id, u64 gpu_va,
			bool wfi);
	u32 (*get_incr_cmd_size)(bool wfi_cmd);
	u32 (*get_incr_per_release)(void);
#endif
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

#if defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT) && \
	defined(CONFIG_NVGPU_SW_SEMAPHORE)
/** @cond DOXYGEN_SHOULD_SKIP_THIS */
struct gops_sync_sema {
	u32 (*get_wait_cmd_size)(void);
	u32 (*get_incr_cmd_size)(void);
	void (*add_wait_cmd)(struct gk20a *g,
		struct priv_cmd_entry *cmd,
		struct nvgpu_semaphore *s, u64 sema_va);
	void (*add_incr_cmd)(struct gk20a *g,
		struct priv_cmd_entry *cmd,
		struct nvgpu_semaphore *s, u64 sema_va,
		bool wfi);
};
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
#endif

/**
 * Sync HAL operations.
 *
 * @see gpu_ops
 */
struct gops_sync {
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	struct gops_sync_syncpt syncpt;
#endif /* CONFIG_TEGRA_GK20A_NVHOST */
#if defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT) && \
	defined(CONFIG_NVGPU_SW_SEMAPHORE)
/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	struct gops_sync_sema sema;
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
#endif
};

#endif
