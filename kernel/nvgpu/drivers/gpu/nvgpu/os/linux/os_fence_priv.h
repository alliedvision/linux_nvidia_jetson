/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_OS_FENCE_PRIV_H
#define NVGPU_OS_FENCE_PRIV_H

#include <nvgpu/os_fence.h>

static inline void nvgpu_os_fence_clear(struct nvgpu_os_fence *fence_out)
{
	fence_out->priv = NULL;
	fence_out->g = NULL;
	fence_out->ops = NULL;
}

static inline void nvgpu_os_fence_init(struct nvgpu_os_fence *fence_out,
		struct gk20a *g, const struct nvgpu_os_fence_ops *fops,
		void *fence)
{
	fence_out->g = g;
	fence_out->ops = fops;
	fence_out->priv = fence;
}

#endif /* NVGPU_OS_FENCE_PRIV_H */
