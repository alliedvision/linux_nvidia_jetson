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

#ifndef NVGPU_VPR_H
#define NVGPU_VPR_H

#include <nvgpu/types.h>

#ifdef __KERNEL__
#include <linux/version.h>

/*
 * VPR resize is enabled only on 4.9 kernel because kernel core mm changes to
 * support it are intrusive and they can't be upstreamed easily. Upstream
 * kernel will have support for static VPR. Note that static VPR is
 * supported on all kernels.
 */
#define NVGPU_VPR_RESIZE_SUPPORTED (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
#endif /* __KERNEL__ */

#ifdef CONFIG_NVGPU_VPR
bool nvgpu_is_vpr_resize_enabled(void);
#else
static inline bool nvgpu_is_vpr_resize_enabled(void)
{
	return false;
}
#endif

#endif /* NVGPU_VPR_H */
