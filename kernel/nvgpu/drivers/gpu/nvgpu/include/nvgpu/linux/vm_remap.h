/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __COMMON_LINUX_VM_REMAP_H__
#define __COMMON_LINUX_VM_REMAP_H__

#include <nvgpu/vm.h>
#include <nvgpu/types.h>

struct nvgpu_vm_remap_op;
struct nvgpu_as_remap_op;

/* struct tracking os-specific remap memory buffer state */
struct nvgpu_vm_remap_os_buffer {
	struct nvgpu_os_buffer os_buf;
	struct nvgpu_mapped_buf_priv os_priv;

	struct nvgpu_sgt *nv_sgt;
	u64 aperture;
};

int nvgpu_vm_remap_translate_as_op(struct vm_gk20a *vm,
				struct nvgpu_vm_remap_op *vm_op,
				struct nvgpu_as_remap_op *as_op);
void nvgpu_vm_remap_translate_vm_op(struct nvgpu_as_remap_op *as_op,
				struct nvgpu_vm_remap_op *vm_op);

u64 nvgpu_vm_remap_get_handle(struct nvgpu_vm_remap_os_buffer *remap_os_buf);
int nvgpu_vm_remap_os_buf_get(struct vm_gk20a *vm,
			struct nvgpu_vm_remap_op *op,
			struct nvgpu_vm_remap_os_buffer *remap_os_buf);
void nvgpu_vm_remap_os_buf_put(struct vm_gk20a *vm,
			struct nvgpu_vm_remap_os_buffer *remap_os_buf);

#endif /* __COMMON_LINUX_VM_REMAP_H__ */
