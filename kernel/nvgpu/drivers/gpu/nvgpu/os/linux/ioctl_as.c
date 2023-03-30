/*
 * GK20A Address Spaces
 *
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <nvgpu/trace.h>

#include <uapi/linux/nvgpu.h>

#include <nvgpu/gmmu.h>
#include <nvgpu/mm.h>
#include <nvgpu/vm_area.h>
#include <nvgpu/vm_remap.h>
#include <nvgpu/log2.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/channel.h>
#include <nvgpu/nvhost.h>

#include <nvgpu/linux/vm.h>

#include "platform_gk20a.h"
#include "ioctl_as.h"
#include "ioctl_channel.h"
#include "ioctl.h"
#include "os_linux.h"

static u32 gk20a_as_translate_as_alloc_space_flags(struct gk20a *g, u32 flags)
{
	u32 core_flags = 0;

	if (flags & NVGPU_AS_ALLOC_SPACE_FLAGS_FIXED_OFFSET)
		core_flags |= NVGPU_VM_AREA_ALLOC_FIXED_OFFSET;
	if (flags & NVGPU_AS_ALLOC_SPACE_FLAGS_SPARSE)
		core_flags |= NVGPU_VM_AREA_ALLOC_SPARSE;

	return core_flags;
}

static int gk20a_as_ioctl_bind_channel(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_bind_channel_args *args)
{
	int err = 0;
	struct nvgpu_channel *ch;
	struct gk20a *g = gk20a_from_vm(as_share->vm);

	nvgpu_log_fn(g, " ");

	ch = nvgpu_channel_get_from_file(args->channel_fd);
	if (!ch)
		return -EINVAL;

	if (nvgpu_channel_as_bound(ch)) {
		err = -EINVAL;
		goto out;
	}

	/* this will set nvgpu_channel->vm */
	err = ch->g->ops.mm.vm_bind_channel(as_share->vm, ch);

out:
	nvgpu_channel_put(ch);
	return err;
}

static int gk20a_as_ioctl_alloc_space(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_alloc_space_args *args)
{
	struct gk20a *g = gk20a_from_vm(as_share->vm);

	nvgpu_log_fn(g, " ");
	return nvgpu_vm_area_alloc(as_share->vm, args->pages, args->page_size,
				   &args->o_a.offset,
				   gk20a_as_translate_as_alloc_space_flags(g,
								args->flags));
}

static int gk20a_as_ioctl_free_space(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_free_space_args *args)
{
	struct gk20a *g = gk20a_from_vm(as_share->vm);

	nvgpu_log_fn(g, " ");
	return nvgpu_vm_area_free(as_share->vm, args->offset);
}

static int gk20a_as_ioctl_map_buffer_ex(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_map_buffer_ex_args *args)
{
	struct gk20a *g = gk20a_from_vm(as_share->vm);

	nvgpu_log_fn(g, " ");

	/* unsupported, direct kind control must be used */
	if (!(args->flags & NVGPU_AS_MAP_BUFFER_FLAGS_DIRECT_KIND_CTRL)) {
		struct gk20a *g = as_share->vm->mm->g;
		nvgpu_log_info(g, "Direct kind control must be requested");
		return -EINVAL;
	}

	return nvgpu_vm_map_buffer(as_share->vm, args->dmabuf_fd,
				   &args->offset, args->flags,
				   args->page_size,
				   args->compr_kind,
				   args->incompr_kind,
				   args->buffer_offset,
				   args->mapping_size,
				   NULL);
}

static int gk20a_as_ioctl_unmap_buffer(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_unmap_buffer_args *args)
{
	struct gk20a *g = gk20a_from_vm(as_share->vm);

	nvgpu_log_fn(g, " ");

	nvgpu_vm_unmap(as_share->vm, args->offset, NULL);

	return 0;
}

static int gk20a_as_ioctl_map_buffer_batch(
	struct gk20a_as_share *as_share,
	struct nvgpu_as_map_buffer_batch_args *args)
{
	struct gk20a *g = gk20a_from_vm(as_share->vm);
	u32 i;
	int err = 0;

	struct nvgpu_as_unmap_buffer_args __user *user_unmap_args =
		(struct nvgpu_as_unmap_buffer_args __user *)(uintptr_t)
		args->unmaps;
	struct nvgpu_as_map_buffer_ex_args __user *user_map_args =
		(struct nvgpu_as_map_buffer_ex_args __user *)(uintptr_t)
		args->maps;

	struct vm_gk20a_mapping_batch batch;

	nvgpu_log_fn(g, " ");

	if (args->num_unmaps > NVGPU_IOCTL_AS_MAP_BUFFER_BATCH_LIMIT ||
	    args->num_maps > NVGPU_IOCTL_AS_MAP_BUFFER_BATCH_LIMIT)
		return -EINVAL;

	nvgpu_vm_mapping_batch_start(&batch);

	for (i = 0; i < args->num_unmaps; ++i) {
		struct nvgpu_as_unmap_buffer_args unmap_args;

		if (copy_from_user(&unmap_args, &user_unmap_args[i],
				   sizeof(unmap_args))) {
			err = -EFAULT;
			break;
		}

		nvgpu_vm_unmap(as_share->vm, unmap_args.offset, &batch);
	}

	nvgpu_speculation_barrier();
	if (err) {
		nvgpu_vm_mapping_batch_finish(as_share->vm, &batch);

		args->num_unmaps = i;
		args->num_maps = 0;
		return err;
	}

	for (i = 0; i < args->num_maps; ++i) {
		s16 compressible_kind;
		s16 incompressible_kind;

		struct nvgpu_as_map_buffer_ex_args map_args;
		(void) memset(&map_args, 0, sizeof(map_args));

		if (copy_from_user(&map_args, &user_map_args[i],
				   sizeof(map_args))) {
			err = -EFAULT;
			break;
		}

		if (map_args.flags &
		    NVGPU_AS_MAP_BUFFER_FLAGS_DIRECT_KIND_CTRL) {
			compressible_kind = map_args.compr_kind;
			incompressible_kind = map_args.incompr_kind;
		} else {
			/* direct kind control must be used */
			err = -EINVAL;
			break;
		}

		err = nvgpu_vm_map_buffer(
			as_share->vm, map_args.dmabuf_fd,
			&map_args.offset, map_args.flags, map_args.page_size,
			compressible_kind, incompressible_kind,
			map_args.buffer_offset,
			map_args.mapping_size,
			&batch);
		if (err)
			break;
	}

	nvgpu_vm_mapping_batch_finish(as_share->vm, &batch);

	if (err)
		args->num_maps = i;
	/* note: args->num_unmaps will be unmodified, which is ok
	 * since all unmaps are done */

	return err;
}

static int gk20a_as_ioctl_get_va_regions(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_get_va_regions_args *args)
{
	unsigned int i;
	unsigned int write_entries;
	struct nvgpu_as_va_region __user *user_region_ptr;
	struct vm_gk20a *vm = as_share->vm;
	struct gk20a *g = gk20a_from_vm(vm);
	unsigned int page_sizes = GMMU_PAGE_SIZE_KERNEL;

	nvgpu_log_fn(g, " ");

	if (!vm->big_pages)
		page_sizes--;

	write_entries = args->buf_size / sizeof(struct nvgpu_as_va_region);
	if (write_entries > page_sizes)
		write_entries = page_sizes;

	user_region_ptr =
		(struct nvgpu_as_va_region __user *)(uintptr_t)args->buf_addr;

	for (i = 0; i < write_entries; ++i) {
		struct nvgpu_as_va_region region;
		struct nvgpu_allocator *vma = vm->vma[i];

		(void) memset(&region, 0, sizeof(struct nvgpu_as_va_region));

		region.page_size = vm->gmmu_page_sizes[i];
		/*
		 * The gmmu_page_sizes values are assigned in nvgpu_vm_init_attribute function.
		 * The only value that can possibly be zero is vm->gmmu_page_sizes[1] when big_pages
		 * are not enabled.
		 * The upper bound on page_sizes already handles this issue. When big_pages are not enabled
		 * write_entries will be 1 and hence vm->gmmu_page_sizes[1] will never be accessed.
		 * Hence, an assert would suffice instead of an if check.
		 */
		nvgpu_assert(region.page_size > 0U);

		region.offset = nvgpu_alloc_base(vma);
		/* No __aeabi_uldivmod() on some platforms... */
		region.pages = (nvgpu_alloc_end(vma) -
			nvgpu_alloc_base(vma)) >> nvgpu_ilog2(region.page_size);

		if (copy_to_user(user_region_ptr + i, &region, sizeof(region)))
			return -EFAULT;
	}

	args->buf_size =
		page_sizes * sizeof(struct nvgpu_as_va_region);

	return 0;
}

static int nvgpu_as_ioctl_get_sync_ro_map(
	struct gk20a_as_share *as_share,
	struct nvgpu_as_get_sync_ro_map_args *args)
{
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	struct vm_gk20a *vm = as_share->vm;
	struct gk20a *g = gk20a_from_vm(vm);
	u64 base_gpuva;
	u32 sync_size;
	u32 num_syncpoints;
	int err = 0;

	if (g->ops.sync.syncpt.get_sync_ro_map == NULL)
		return -EINVAL;

	if (!nvgpu_has_syncpoints(g))
		return -EINVAL;

	err = g->ops.sync.syncpt.get_sync_ro_map(vm, &base_gpuva, &sync_size,
						 &num_syncpoints);
	if (err)
		return err;

	args->base_gpuva = base_gpuva;
	args->sync_size = sync_size;
	args->num_syncpoints = num_syncpoints;

	return err;
#else
	return -EINVAL;
#endif
}

static int nvgpu_as_ioctl_mapping_modify(
		struct gk20a_as_share *as_share,
		struct nvgpu_as_mapping_modify_args *args)
{
	struct gk20a *g = gk20a_from_vm(as_share->vm);

	nvgpu_log_fn(g, " ");

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MAPPING_MODIFY)) {
		return -ENOTTY;
	}

	return nvgpu_vm_mapping_modify(as_share->vm,
				args->compr_kind,
				args->incompr_kind,
				args->map_address,
				args->buffer_offset,
				args->buffer_size);
}

static int nvgpu_as_ioctl_remap(
	struct gk20a_as_share *as_share,
	struct nvgpu_as_remap_args *args)
{
	struct gk20a *g = gk20a_from_vm(as_share->vm);
	struct nvgpu_as_remap_op __user *user_remap_ops = NULL;
	struct nvgpu_as_remap_op remap_op;
	struct nvgpu_vm_remap_op *nvgpu_vm_remap_ops = NULL;
	u32 i;
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_REMAP)) {
		return -ENOTTY;
	}

	if (args->num_ops == 0) {
		return 0;
	}

	/* allocate buffer for internal representation of remap ops */
	nvgpu_vm_remap_ops = nvgpu_kzalloc(g, args->num_ops *
				sizeof(struct nvgpu_vm_remap_op));
	if (nvgpu_vm_remap_ops == NULL) {
		err = -ENOMEM;
		goto out;
	}

	user_remap_ops =
		(struct nvgpu_as_remap_op __user *)(uintptr_t)args->ops;

	for (i = 0; i < args->num_ops; i++) {
		if (copy_from_user(&remap_op, &user_remap_ops[i],
					sizeof(remap_op))) {
			err = -EFAULT;
			goto out;
		}

		err = nvgpu_vm_remap_translate_as_op(as_share->vm,
						&nvgpu_vm_remap_ops[i],
						&remap_op);
		if (err != 0) {
			args->num_ops = 0;
			goto out;
		}
	}

	/* execute remap ops */
	err = nvgpu_vm_remap(as_share->vm, nvgpu_vm_remap_ops,
			&args->num_ops);
	if (err != 0) {
		goto out;
	}

	/* update user params */
	for (i = 0; i < args->num_ops; i++) {
		nvgpu_vm_remap_translate_vm_op(&remap_op,
					&nvgpu_vm_remap_ops[i]);

		if (copy_to_user(&user_remap_ops[i], &remap_op,
					sizeof(remap_op))) {
			err = -EFAULT;
			args->num_ops = i;
			goto out;
		}
	}

out:
	if (nvgpu_vm_remap_ops != NULL) {
		nvgpu_kfree(g, nvgpu_vm_remap_ops);
	}

	return err;
}

int gk20a_as_dev_open(struct inode *inode, struct file *filp)
{
	struct gk20a_as_share *as_share;
	struct gk20a *g;
	struct mm_gk20a *mm;
	int err;
	struct nvgpu_cdev *cdev;
	u32 big_page_size;

	cdev = container_of(inode->i_cdev, struct nvgpu_cdev, cdev);
	g = nvgpu_get_gk20a_from_cdev(cdev);
	mm = &g->mm;
	big_page_size = g->ops.mm.gmmu.get_default_big_page_size();

	nvgpu_log_fn(g, " ");

	err = gk20a_as_alloc_share(g,
		big_page_size,
		NVGPU_AS_ALLOC_UNIFIED_VA,
		U64(big_page_size) << U64(10),
		mm->channel.user_size,
		0ULL, &as_share);
	if (err) {
		nvgpu_log_fn(g, "failed to alloc share");
		return err;
	}

	filp->private_data = as_share;
	return 0;
}

int gk20a_as_dev_release(struct inode *inode, struct file *filp)
{
	struct gk20a_as_share *as_share = filp->private_data;

	if (!as_share)
		return 0;

	return gk20a_as_release_share(as_share);
}

long gk20a_as_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct gk20a_as_share *as_share = filp->private_data;
	struct gk20a *g = gk20a_from_as(as_share->as);
	bool always_copy_to_user = false;
	u8 buf[NVGPU_AS_IOCTL_MAX_ARG_SIZE];

	nvgpu_log_fn(g, "start %d", _IOC_NR(cmd));

	if ((_IOC_TYPE(cmd) != NVGPU_AS_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > NVGPU_AS_IOCTL_LAST) ||
		(_IOC_SIZE(cmd) > NVGPU_AS_IOCTL_MAX_ARG_SIZE))
		return -EINVAL;

	(void) memset(buf, 0, sizeof(buf));
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	err = gk20a_busy(g);
	if (err)
		return err;

	nvgpu_speculation_barrier();
	switch (cmd) {
	case NVGPU_AS_IOCTL_BIND_CHANNEL:
#ifdef CONFIG_NVGPU_TRACE
		trace_gk20a_as_ioctl_bind_channel(g->name);
#endif
		err = gk20a_as_ioctl_bind_channel(as_share,
			       (struct nvgpu_as_bind_channel_args *)buf);

		break;
	case NVGPU32_AS_IOCTL_ALLOC_SPACE:
	{
		struct nvgpu32_as_alloc_space_args *args32 =
			(struct nvgpu32_as_alloc_space_args *)buf;
		struct nvgpu_as_alloc_space_args args;

		args.pages = args32->pages;
		args.page_size = args32->page_size;
		args.flags = args32->flags;
		args.o_a.offset = args32->o_a.offset;
#ifdef CONFIG_NVGPU_TRACE
		trace_gk20a_as_ioctl_alloc_space(g->name);
#endif
		err = gk20a_as_ioctl_alloc_space(as_share, &args);
		args32->o_a.offset = args.o_a.offset;
		break;
	}
	case NVGPU_AS_IOCTL_ALLOC_SPACE:
#ifdef CONFIG_NVGPU_TRACE
		trace_gk20a_as_ioctl_alloc_space(g->name);
#endif
		err = gk20a_as_ioctl_alloc_space(as_share,
				(struct nvgpu_as_alloc_space_args *)buf);
		break;
	case NVGPU_AS_IOCTL_FREE_SPACE:
#ifdef CONFIG_NVGPU_TRACE
		trace_gk20a_as_ioctl_free_space(g->name);
#endif
		err = gk20a_as_ioctl_free_space(as_share,
				(struct nvgpu_as_free_space_args *)buf);
		break;
	case NVGPU_AS_IOCTL_MAP_BUFFER_EX:
#ifdef CONFIG_NVGPU_TRACE
		trace_gk20a_as_ioctl_map_buffer(g->name);
#endif
		err = gk20a_as_ioctl_map_buffer_ex(as_share,
				(struct nvgpu_as_map_buffer_ex_args *)buf);
		break;
	case NVGPU_AS_IOCTL_UNMAP_BUFFER:
#ifdef CONFIG_NVGPU_TRACE
		trace_gk20a_as_ioctl_unmap_buffer(g->name);
#endif
		err = gk20a_as_ioctl_unmap_buffer(as_share,
				(struct nvgpu_as_unmap_buffer_args *)buf);
		break;
	case NVGPU_AS_IOCTL_GET_VA_REGIONS:
#ifdef CONFIG_NVGPU_TRACE
		trace_gk20a_as_ioctl_get_va_regions(g->name);
#endif
		err = gk20a_as_ioctl_get_va_regions(as_share,
				(struct nvgpu_as_get_va_regions_args *)buf);
		break;
	case NVGPU_AS_IOCTL_MAP_BUFFER_BATCH:
		err = gk20a_as_ioctl_map_buffer_batch(as_share,
				(struct nvgpu_as_map_buffer_batch_args *)buf);
		break;
	case NVGPU_AS_IOCTL_GET_SYNC_RO_MAP:
		err = nvgpu_as_ioctl_get_sync_ro_map(as_share,
			(struct nvgpu_as_get_sync_ro_map_args *)buf);
		break;
	case NVGPU_AS_IOCTL_MAPPING_MODIFY:
		err = nvgpu_as_ioctl_mapping_modify(as_share,
			(struct nvgpu_as_mapping_modify_args *)buf);
		break;
	case NVGPU_AS_IOCTL_REMAP:
		err = nvgpu_as_ioctl_remap(as_share,
			(struct nvgpu_as_remap_args *)buf);
		always_copy_to_user = true;
		break;
	default:
		err = -ENOTTY;
		break;
	}

	gk20a_idle(g);

	if ((err == 0 || always_copy_to_user) && (_IOC_DIR(cmd) & _IOC_READ))
		if (copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd)))
			err = -EFAULT;

	return err;
}
