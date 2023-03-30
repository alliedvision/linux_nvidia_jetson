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

#ifndef NVGPU_LINUX_OS_FENCE_ANDROID_H
#define NVGPU_LINUX_OS_FENCE_ANDROID_H

struct gk20a;
struct nvgpu_os_fence;
struct sync_fence;
struct nvgpu_channel;

struct sync_fence *nvgpu_get_sync_fence(struct nvgpu_os_fence *s);

void nvgpu_os_fence_android_drop_ref(struct nvgpu_os_fence *s);

int nvgpu_os_fence_sema_fdget(struct nvgpu_os_fence *fence_out,
	struct nvgpu_channel *c, int fd);

void nvgpu_os_fence_android_dup(struct nvgpu_os_fence *s);
int nvgpu_os_fence_android_install_fd(struct nvgpu_os_fence *s, int fd);

int nvgpu_os_fence_syncpt_fdget(
	struct nvgpu_os_fence *fence_out,
	struct nvgpu_channel *c, int fd);

#endif /* NVGPU_LINUX_OS_FENCE_ANDROID_H */
