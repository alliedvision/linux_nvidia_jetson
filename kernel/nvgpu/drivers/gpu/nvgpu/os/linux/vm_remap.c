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

#include <linux/dma-buf.h>

#include <uapi/linux/nvgpu.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/vm.h>
#include <nvgpu/vm_remap.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/power_features/pg.h>

#include "os_linux.h"
#include "dmabuf_priv.h"

#define dev_from_vm(vm) dev_from_gk20a(vm->mm->g)

u64 nvgpu_vm_remap_get_handle(struct nvgpu_vm_remap_os_buffer *remap_os_buf)
{
	return (u64)(uintptr_t)remap_os_buf->os_priv.dmabuf;
}

int nvgpu_vm_remap_os_buf_get(struct vm_gk20a *vm,
			struct nvgpu_vm_remap_op *op,
			struct nvgpu_vm_remap_os_buffer *remap_os_buf)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct device *dev = dev_from_gk20a(g);
	struct dma_buf *dmabuf;
	struct sg_table *sgt = NULL;
	struct nvgpu_sgt *nv_sgt = NULL;
	struct dma_buf_attachment *attachment;
	enum nvgpu_aperture aperture;
	enum dma_data_direction dmabuf_direction;
	int err = 0;

	/* get ref to the dmabuf fd */
	dmabuf = dma_buf_get(op->mem_handle);
	if (IS_ERR(dmabuf)) {
		nvgpu_warn(g, "mem_handle 0x%x is not a dmabuf",
			op->mem_handle);
		return -EINVAL;
	}

	if (!(dmabuf->file->f_mode & (FMODE_WRITE | FMODE_PWRITE)) &&
		!(op->flags & NVGPU_VM_REMAP_OP_FLAGS_ACCESS_NO_WRITE)) {
		nvgpu_err(g, "RW access requested for RO mapped buffer");
		err = -EINVAL;
		goto clean_up;
	}

	if ((op->flags & NVGPU_VM_REMAP_OP_FLAGS_ACCESS_NO_WRITE) != 0) {
		dmabuf_direction = DMA_TO_DEVICE;
	} else {
		dmabuf_direction = DMA_BIDIRECTIONAL;
	}

	sgt = nvgpu_mm_pin(dev, dmabuf, &attachment, dmabuf_direction);
	if (IS_ERR(sgt)) {
		nvgpu_warn(g, "failed to pin dma_buf");
		err = -ENOMEM;
		goto clean_up;
	}

	aperture = gk20a_dmabuf_aperture(g, dmabuf);
	if (aperture == APERTURE_INVALID) {
		err = -EINVAL;
		goto clean_up;
	}

	nv_sgt = nvgpu_linux_sgt_create(g, sgt);
	if (nv_sgt == NULL) {
		nvgpu_warn(g, "failed to create nv_sgt");
		err = -ENOMEM;
		goto clean_up;
	}

	memset(remap_os_buf, 0, sizeof(*remap_os_buf));

	remap_os_buf->os_priv.dmabuf = dmabuf;
	remap_os_buf->os_priv.attachment = attachment;
	remap_os_buf->os_priv.sgt = sgt;

	remap_os_buf->os_buf.dmabuf = dmabuf;
	remap_os_buf->os_buf.attachment = attachment;
	remap_os_buf->os_buf.dev = dev;

	remap_os_buf->nv_sgt = nv_sgt;
	remap_os_buf->aperture = aperture;

	return 0;

clean_up:
	if (IS_ERR(sgt)) {
		nvgpu_mm_unpin(dev, dmabuf, attachment, sgt);
	}
	dma_buf_put(dmabuf);

	return err;
}

void nvgpu_vm_remap_os_buf_put(struct vm_gk20a *vm,
			struct nvgpu_vm_remap_os_buffer *remap_os_buf)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct device *dev = dev_from_gk20a(g);
#ifdef CONFIG_NVGPU_COMPRESSION
	struct gk20a_comptags comptags;
	int err = 0;
#endif

	nvgpu_mm_unpin(dev, remap_os_buf->os_priv.dmabuf,
		remap_os_buf->os_priv.attachment, remap_os_buf->os_priv.sgt);

#ifdef CONFIG_NVGPU_COMPRESSION
	gk20a_get_comptags(&remap_os_buf->os_buf, &comptags);

	/*
	 * Flush compression bit cache before releasing the physical
	 * memory buffer reference.
	 */
	if (comptags.offset != 0) {
		g->ops.cbc.ctrl(g, nvgpu_cbc_op_clean, 0, 0);
		err = nvgpu_pg_elpg_ms_protected_call(g,
				g->ops.mm.cache.l2_flush(g, true));
		if (err != 0) {
			nvgpu_err(g, "l2 flush failed");
			return;
		}
	}
#endif

	nvgpu_sgt_free(g, remap_os_buf->nv_sgt);

	dma_buf_put(remap_os_buf->os_priv.dmabuf);
}

static int nvgpu_vm_remap_validate_map_op(struct nvgpu_as_remap_op *op)
{
	int err = 0;
	const u32 pagesize_flags = (NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_4K |
			NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_64K |
			NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_128K);
	const u32 valid_flags = (pagesize_flags |
			NVGPU_AS_REMAP_OP_FLAGS_CACHEABLE |
			NVGPU_AS_REMAP_OP_FLAGS_ACCESS_NO_WRITE);
	const u32 pagesize = op->flags & pagesize_flags;

	if ((op->flags & ~valid_flags) != 0) {
		err = -EINVAL;
	}

	/* must be set and to a single pagesize */
	if ((pagesize != NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_4K) &&
		(pagesize != NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_64K) &&
		(pagesize != NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_128K)) {
		err = -EINVAL;
	}

	return err;
}

static int nvgpu_vm_remap_validate_unmap_op(struct nvgpu_as_remap_op *op)
{
	int err = 0;
	const u32 pagesize_flags = (NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_4K |
			NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_64K |
			NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_128K);
	const u32 valid_flags = pagesize_flags;
	const u32 pagesize = op->flags & pagesize_flags;

	if ((op->flags & ~valid_flags) != 0) {
		err = -EINVAL;
	}

	/* must be set and to a single pagesize */
	if ((pagesize != NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_4K) &&
		(pagesize != NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_64K) &&
		(pagesize != NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_128K)) {
		err = -EINVAL;
	}

	if ((op->compr_kind != NVGPU_KIND_INVALID) ||
		(op->incompr_kind != NVGPU_KIND_INVALID) ||
		(op->mem_offset_in_pages != 0)) {
		err = -EINVAL;
	}

	return err;
}

static u32 nvgpu_vm_remap_translate_as_flags(u32 flags)
{
	u32 core_flags = 0;

	if ((flags & NVGPU_AS_REMAP_OP_FLAGS_CACHEABLE) != 0) {
		core_flags |= NVGPU_VM_REMAP_OP_FLAGS_CACHEABLE;
	}
	if ((flags & NVGPU_AS_REMAP_OP_FLAGS_ACCESS_NO_WRITE) != 0) {
		core_flags |= NVGPU_VM_REMAP_OP_FLAGS_ACCESS_NO_WRITE;
	}
	if ((flags & NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_4K) != 0) {
		core_flags |= NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_4K;
	}
	if ((flags & NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_64K) != 0) {
		core_flags |= NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_64K;
	}
	if ((flags & NVGPU_AS_REMAP_OP_FLAGS_PAGESIZE_128K) != 0) {
		core_flags |= NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_128K;
	}
	return core_flags;
}

int nvgpu_vm_remap_translate_as_op(struct vm_gk20a *vm,
				struct nvgpu_vm_remap_op *vm_op,
				struct nvgpu_as_remap_op *as_op)
{
	int err = 0;
	u64 page_size;

	if (as_op->mem_handle == 0) {
		err = nvgpu_vm_remap_validate_unmap_op(as_op);
	} else {
		err = nvgpu_vm_remap_validate_map_op(as_op);
	}

	if (err != 0)
		goto clean_up;

	vm_op->flags = nvgpu_vm_remap_translate_as_flags(as_op->flags);
	page_size = nvgpu_vm_remap_page_size(vm_op);

	if ((as_op->num_pages == 0) || (page_size == 0) ||
		(as_op->num_pages > (vm->va_limit / page_size)) ||
		(as_op->mem_offset_in_pages > (vm->va_limit / page_size)) ||
		(as_op->virt_offset_in_pages > (vm->va_limit / page_size))) {
		err = -EINVAL;
		goto clean_up;
	}

	vm_op->compr_kind = as_op->compr_kind;
	vm_op->incompr_kind = as_op->incompr_kind;
	vm_op->mem_handle = as_op->mem_handle;
	vm_op->mem_offset_in_pages = as_op->mem_offset_in_pages;
	vm_op->virt_offset_in_pages = as_op->virt_offset_in_pages;
	vm_op->num_pages = as_op->num_pages;

	return 0;

clean_up:
	return err;
}

void nvgpu_vm_remap_translate_vm_op(struct nvgpu_as_remap_op *as_op,
				struct nvgpu_vm_remap_op *vm_op)
{
	as_op->compr_kind = vm_op->compr_kind;
	as_op->incompr_kind = vm_op->incompr_kind;
}
