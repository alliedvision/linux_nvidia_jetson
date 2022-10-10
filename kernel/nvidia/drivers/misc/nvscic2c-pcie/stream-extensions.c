// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#define pr_fmt(fmt)	"nvscic2c-pcie: stream-ext: " fmt

#include <linux/anon_inodes.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/of_platform.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/nvhost.h>
#include <linux/nvhost_t194.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/tegra-pcie-edma.h>

#include <uapi/misc/nvscic2c-pcie-ioctl.h>

#include "comm-channel.h"
#include "common.h"
#include "descriptor.h"
#include "module.h"
#include "pci-client.h"
#include "stream-extensions.h"
#include "vmap.h"

/* forward declaration.*/
struct stream_ext_ctx_t;
struct stream_ext_obj;

/* limits as set for copy requests.*/
struct copy_req_limits {
	u64 max_copy_requests;
	u64 max_flush_ranges;
	u64 max_post_fences;
};

/*
 * Copied submit-copy args from use-space. These are then parsed and validated.
 * This copy is required as args have pointer to user-space area which be copied
 * into kernel-space before using it. On subsequent copy, basic checks are done
 * and shall be used to create a copy request payload for eDMA.
 */
struct copy_req_params {
	u64 num_local_post_fences;
	s32 *local_post_fences;
	u64 num_remote_post_fences;
	s32 *remote_post_fences;
	u64 num_flush_ranges;
	u64 *remote_post_fence_values;
	struct nvscic2c_pcie_flush_range *flush_ranges;
};

/* one copy request.*/
struct copy_request {
	/* book-keeping for copy completion.*/
	struct list_head node;

	/*
	 * back-reference to stream_ext_context, used in eDMA callback.
	 * to add this copy_request back in free_list for reuse. Also,
	 * the host1x_pdev in ctx is used via this ctx in the callback.
	 */
	struct stream_ext_ctx_t *ctx;

	/*
	 * actual number of handles per the submit-copy request.
	 * Shall include ((2 * num_flush_range) + num_local_post_fences
	 * + num_remote_post_fences).
	 * used for refcounting: out of order free and copy.
	 */
	u64 num_handles;
	/*
	 * space for num_handles considering worst-case allocation:
	 * ((2 * max_flush_ranges) + (max_post_fences)).
	 */
	struct stream_ext_obj **handles;

	/*
	 * actual number of edma-desc per the submit-copy request.
	 * Shall include (num_flush_range + num_remote_post_fences (eDMAed))
	 */
	u64 num_edma_desc;
	/*
	 * space for num_edma_desc considering worst-case allocation:
	 * (max_flush_ranges + max_post_fences), assuming submit-copy could have
	 * all the post-fences for remote signalling by eDMA.
	 */
	struct tegra_pcie_edma_desc *edma_desc;

	/*
	 * actual number of local_post-fences per the submit-copy request.
	 * Shall include (num_local_post_fences).
	 */
	u64 num_local_post_fences;
	u64 num_remote_post_fences;
	u64 num_remote_buf_objs;
	/*
	 * space for num_local_post_fences considering worst-case allocation:
	 * max_post_fences, assuming submit-copy could have all post-fences for
	 * local signalling.
	 */
	struct stream_ext_obj **local_post_fences;
	/*
	 * space for num_remote_post_fences considering worst-case allocation:
	 * max_post_fences, assuming submit-copy could have all post-fences for
	 * remote signalling.
	 */
	struct stream_ext_obj **remote_post_fences;
	/*
	 * space for num_remote_buf_objs considering worst-case allocation:
	 * max_flush_ranges, assuming submit-copy could have all flush ranges for
	 * transfers.
	 */
	struct stream_ext_obj **remote_buf_objs;

	/* X86 uses semaphores for fences and it needs to be written with NvSciStream
	 * provided value
	 */
	u64 *remote_post_fence_values;
	enum peer_cpu_t peer_cpu;
};

struct stream_ext_obj {
	/* back-reference to vmap handle, required during free/unmap.*/
	void *vmap_h;

	/* for correctness check. */
	enum nvscic2c_pcie_obj_type type;
	u32 soc_id;
	u32 cntrlr_id;
	u32 ep_id;

	/* for ordering out of order copy and free ops.*/
	bool marked_for_del;
	struct kref refcount;

	/* virtual mapping information.*/
	struct vmap_obj_attributes vmap;

	/*
	 * ImportObj only.
	 * Add offsetof from peer window to local aper base for access
	 * by local eDMA or CPU(mmap) towards peer obj.
	 *  - For PCIe RP.
	 * Add offsetof from peer window to local aper base for access by
	 * CPU(mmap) towards peer obj, eDMA will use the iova direactly.
	 *  - For PCIe EP.
	 */
	u32 import_type;
	phys_addr_t aper;

	/* Mapping for ImportObj for CPU Read/Write.*/
	void __iomem *import_obj_map;
};

/* stream extensions context per NvSciC2cPcie endpoint.*/
struct stream_ext_ctx_t {
	/*
	 * mode - EPC(on PCIe RP) or EPF(on PCIe EP).
	 * Destination address of eDMA descriptor is different for these
	 * two modes.
	 */
	enum drv_mode_t drv_mode;

	u32 ep_id;
	char ep_name[NAME_MAX];

	struct node_info_t local_node;
	struct node_info_t peer_node;

	/* for local post fence increment ops.*/
	struct platform_device *host1x_pdev;

	/* vmap abstraction.*/
	void *vmap_h;

	/* tegra-pcie-edma cookie.*/
	void *edma_h;

	/* comm-channel abstraction. */
	void *comm_channel_h;

	/* pci client abstraction. */
	void *pci_client_h;

	/* max copy request limits as set by user.*/
	struct copy_req_limits cr_limits;

	/* Intermediate validated and copied user-args for submit-copy ioctl.*/
	struct copy_req_params cr_params;

	/* Async copy: book-keeping copy-requests: free and in-progress.*/
	struct list_head free_list;
	/* guard free_list.*/
	struct mutex free_lock;
	atomic_t transfer_count;
	wait_queue_head_t transfer_waitq;
};

static int
cache_copy_request_handles(struct copy_req_params *params,
			   struct copy_request *cr);
static int
release_copy_request_handles(struct copy_request *cr);

static void
signal_local_post_fences(struct copy_request *cr);

static void
signal_remote_post_fences(struct copy_request *cr);

static int
prepare_edma_desc(enum drv_mode_t drv_mode, struct copy_req_params *params,
		  struct tegra_pcie_edma_desc *desc, u64 *num_desc, enum peer_cpu_t);

static edma_xfer_status_t
schedule_edma_xfer(void *edma_h, void *priv, u64 num_desc,
		   struct tegra_pcie_edma_desc *desc);
static void
callback_edma_xfer(void *priv, edma_xfer_status_t status,
		   struct tegra_pcie_edma_desc *desc);
static int
validate_handle(struct stream_ext_ctx_t *ctx, s32 handle,
		enum nvscic2c_pcie_obj_type type);
static int
allocate_handle(struct stream_ext_ctx_t *ctx,
		enum nvscic2c_pcie_obj_type type,
		void *ioctl_args);
static int
copy_args_from_user(struct stream_ext_ctx_t *ctx,
		    struct nvscic2c_pcie_submit_copy_args *args,
		    struct copy_req_params *params);
static int
allocate_copy_request(struct stream_ext_ctx_t *ctx,
		      struct copy_request **copy_request);
static void
free_copy_request(struct copy_request **copy_request);

static int
allocate_copy_req_params(struct stream_ext_ctx_t *ctx,
			 struct copy_req_params *params);
static void
free_copy_req_params(struct copy_req_params *params);

static int
validate_copy_req_params(struct stream_ext_ctx_t *ctx,
			 struct copy_req_params *params);

static int
fops_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int ret = 0;
	u64 memaddr = 0x0;
	u64 memsize = 0x0;
	struct stream_ext_obj *stream_obj = NULL;

	if (WARN_ON(!filep))
		return -EFAULT;

	if (WARN_ON(!(vma)))
		return -EFAULT;

	/* read access of import sync object would mean poll over pcie.*/
	if (WARN_ON(vma->vm_flags & VM_READ))
		return -EINVAL;

	stream_obj = (struct stream_ext_obj *)(filep->private_data);
	if (WARN_ON(stream_obj->type != NVSCIC2C_PCIE_OBJ_TYPE_IMPORT))
		return -EOPNOTSUPP;
	if (WARN_ON(stream_obj->import_type != STREAM_OBJ_TYPE_SYNC))
		return -EOPNOTSUPP;
	if (WARN_ON(stream_obj->marked_for_del))
		return -EINVAL;

	memsize = stream_obj->vmap.size;
	memaddr = stream_obj->aper;

	vma->vm_pgoff  = 0;
	vma->vm_flags |= (VM_DONTCOPY);
	vma->vm_page_prot = pgprot_device(vma->vm_page_prot);
	ret = remap_pfn_range(vma, vma->vm_start,
					PFN_DOWN(memaddr),
					memsize, vma->vm_page_prot);
	if (ret)
		pr_err("mmap() failed for Imported sync object\n");

	return ret;
}

static void
streamobj_free(struct kref *kref)
{
	struct stream_ext_obj *stream_obj = NULL;

	if (!kref)
		return;

	stream_obj = container_of(kref, struct stream_ext_obj, refcount);
	if (stream_obj) {
		if (stream_obj->import_obj_map)
			iounmap(stream_obj->import_obj_map);
		vmap_obj_unmap(stream_obj->vmap_h, stream_obj->vmap.type,
					stream_obj->vmap.id);
		kfree(stream_obj);
	}
}

static int
fops_release(struct inode *inode, struct file *filep)
{
	struct stream_ext_obj *stream_obj =
				(struct stream_ext_obj *)(filep->private_data);

	if (WARN_ON(!stream_obj))
		return -EFAULT;

	/*
	 * actual free happens when the refcount reaches zero. This is done to
	 * accommodate: out of order free while copy isin progress use-case.
	 */
	stream_obj->marked_for_del = true;
	kref_put(&stream_obj->refcount, streamobj_free);

	return 0;
}

/* for all stream objs - Local, remote + Mem, Sync, Import*/
static const struct file_operations fops_default = {
	.owner = THIS_MODULE,
	.release = fops_release,
	.mmap = fops_mmap,
};

/* implement NVSCIC2C_PCIE_IOCTL_FREE ioctl call. */
static int
ioctl_free_obj(struct stream_ext_ctx_t *ctx,
	       struct nvscic2c_pcie_free_obj_args *args)
{
	int ret = 0;

	/* validate the input handle for correctness.*/
	ret = validate_handle(ctx, args->handle, args->obj_type);
	if (ret)
		return ret;

	/* this shall close the handle: resulting in fops_release().*/
	ksys_close(args->handle);

	return 0;
}

/* implement NVSCIC2C_PCIE_IOCTL_GET_AUTH_TOKEN call. */
static int
ioctl_export_obj(struct stream_ext_ctx_t *ctx,
		 struct nvscic2c_pcie_export_obj_args *args)
{
	int ret = 0;
	u64 exp_desc = 0;
	struct comm_msg msg = {0};
	struct file *filep = NULL;
	struct stream_ext_obj *stream_obj = NULL;
	struct node_info_t *peer = &ctx->peer_node;
	enum vmap_obj_type export_type = STREAM_OBJ_TYPE_MEM;

	/* validate the input handle for correctness.*/
	ret = validate_handle(ctx, args->in.handle, args->obj_type);
	if (ret)
		return ret;

	/* only target/remote can be exported.*/
	if (args->obj_type == NVSCIC2C_PCIE_OBJ_TYPE_TARGET_MEM)
		export_type = STREAM_OBJ_TYPE_MEM;
	else if (args->obj_type == NVSCIC2C_PCIE_OBJ_TYPE_REMOTE_SYNC)
		export_type = STREAM_OBJ_TYPE_SYNC;
	else
		return -EINVAL;

	filep = fget(args->in.handle);
	stream_obj = filep->private_data;

	/*
	 * take a reference to the virtual mapping. The reference shall be
	 * released by peer when the peer unregisters it's corresponding
	 * imported obj. This happens via comm-channel.
	 *
	 * reference count of stream_obj is not taken, valid scenario to
	 * free the exported obj from this SoC but the virtual mapping
	 * to continue exist and is released when peer SoC releases it's
	 * corresponding import stream obj.
	 */
	ret = vmap_obj_getref(stream_obj->vmap_h, stream_obj->vmap.type,
				stream_obj->vmap.id);
	if (ret) {
		pr_err("(%s): Failed ref counting an object\n", ctx->ep_name);
		fput(filep);
		return ret;
	}

	/* generate export desc.*/
	peer = &ctx->peer_node;
	exp_desc = gen_desc(peer->board_id, peer->soc_id, peer->cntrlr_id,
				ctx->ep_id, export_type, stream_obj->vmap.id);

	/*share it with peer for peer for corresponding import.*/
	pr_debug("Exporting descriptor = (%llu)\n", exp_desc);
	msg.type = COMM_MSG_TYPE_REGISTER;
	msg.u.reg.export_desc = exp_desc;
	msg.u.reg.iova = stream_obj->vmap.iova;
	msg.u.reg.size = stream_obj->vmap.size;
	msg.u.reg.offsetof = stream_obj->vmap.offsetof;
	ret = comm_channel_msg_send(ctx->comm_channel_h, &msg);
	if (ret)
		vmap_obj_putref(stream_obj->vmap_h, stream_obj->vmap.type,
				stream_obj->vmap.id);
	else
		args->out.desc = exp_desc;

	fput(filep);
	return ret;
}

/* implement NVSCIC2C_PCIE_IOCTL_GET_HANDLE call. */
static int
ioctl_import_obj(struct stream_ext_ctx_t *ctx,
		 struct nvscic2c_pcie_import_obj_args *args)
{
	int ret = 0;
	s32 handle = -1;
	struct file *filep = NULL;
	struct stream_ext_obj *stream_obj = NULL;
	struct node_info_t *local = &ctx->local_node;
	enum peer_cpu_t peer_cpu;

	if (args->obj_type != NVSCIC2C_PCIE_OBJ_TYPE_IMPORT)
		return -EINVAL;

	/* validate the incoming descriptor.*/
	ret = validate_desc(args->in.desc, local->board_id, local->soc_id,
				local->cntrlr_id, ctx->ep_id);
	if (ret) {
		pr_err("(%s): Invalid descriptor: (%llu) received\n",
				ctx->ep_name, args->in.desc);
		return ret;
	}

	/*
	 * Import the desc :- create virt. mapping, bind it to a stream_obj
	 * and create a UMD handle for this stream_obj.
	 */
	handle = allocate_handle(ctx, args->obj_type, (void *)args);
	if (handle < 0)
		return handle;
	pr_debug("Imported descriptor = (%llu)\n", args->in.desc);

	filep = fget(handle);
	if (!filep)
		return -ENOMEM;
	stream_obj = filep->private_data;
	stream_obj->import_type = get_handle_type_from_desc(args->in.desc);
	ret = pci_client_get_peer_aper(ctx->pci_client_h, stream_obj->vmap.offsetof,
				stream_obj->vmap.size, &stream_obj->aper);
	if (ret) {
		pr_err("(%s): PCI Client Get Peer Aper Failed\n", ctx->ep_name);
		fput(filep);
		return ret;
	}

	peer_cpu = pci_client_get_peer_cpu(ctx->pci_client_h);
	if (peer_cpu == NVCPU_X86_64)
		stream_obj->import_obj_map = ioremap(stream_obj->aper, PAGE_SIZE);
	fput(filep);

	args->out.handle = handle;

	return ret;
}

/* implement NVSCIC2C_PCIE_IOCTL_MAP ioctl call. */
static int
ioctl_map_obj(struct stream_ext_ctx_t *ctx,
				struct nvscic2c_pcie_map_obj_args *args)
{
	int ret = 0;
	s32 handle = -1;

	/*
	 * Create virt. mapping for the user primitive objs - Mem or Sync.
	 * Bind it to a stream_obj. Create a UMD handle for this stream_obj.
	 */
	handle = allocate_handle(ctx, args->obj_type, (void *)args);
	if (handle < 0)
		return handle;

	args->out.handle = handle;

	return ret;
}

/* implement NVSCIC2C_PCIE_IOCTL_SUBMIT_COPY_REQUEST ioctl call. */
static int
ioctl_submit_copy_request(struct stream_ext_ctx_t *ctx,
				struct nvscic2c_pcie_submit_copy_args *args)
{
	int ret = 0;
	struct copy_request *cr = NULL;
	edma_xfer_status_t edma_status = EDMA_XFER_FAIL_INVAL_INPUTS;
	enum nvscic2c_pcie_link link = NVSCIC2C_PCIE_LINK_DOWN;

	link = pci_client_query_link_status(ctx->pci_client_h);
	if (link != NVSCIC2C_PCIE_LINK_UP)
		return -ENOLINK;

	/* copy user-supplied submit-copy args.*/
	ret = copy_args_from_user(ctx, args, &ctx->cr_params);
	if (ret)
		return ret;

	/* validate the user-supplied handles in flush_range and post-fence.*/
	ret = validate_copy_req_params(ctx, &ctx->cr_params);
	if (ret)
		return ret;

	/* get one copy-request from the free list.*/
	mutex_lock(&ctx->free_lock);
	if (list_empty(&ctx->free_list)) {
		/*
		 * user supplied more than mentioned in max_copy_requests OR
		 * eDMA async didn't invoke callback when eDMA was done.
		 */
		mutex_unlock(&ctx->free_lock);
		return -EAGAIN;
	}
	cr = list_first_entry(&ctx->free_list, struct copy_request, node);
	list_del(&cr->node);
	mutex_unlock(&ctx->free_lock);

	/*
	 * To support out-of-order free and copy-requets when eDMA is in async
	 * mode, cache all the handles from the copy-submit params and increment
	 * their reference count before eDMA ops. Post eDMA, decrement the
	 * reference, thereby, when during in-progress eDMA, free() is received
	 * for the same set of handles, the handles would be marked for deletion
	 * but doesn't actually get deleted.
	 */
	ret = cache_copy_request_handles(&ctx->cr_params, cr);
	if (ret)
		goto reclaim_cr;

	cr->peer_cpu = pci_client_get_peer_cpu(ctx->pci_client_h);
	/* generate eDMA descriptors from flush_ranges, remote_post_fences.*/
	ret = prepare_edma_desc(ctx->drv_mode, &ctx->cr_params, cr->edma_desc,
				&cr->num_edma_desc, cr->peer_cpu);
	if (ret) {
		release_copy_request_handles(cr);
		goto reclaim_cr;
	}

	/* schedule asynchronous eDMA.*/
	atomic_inc(&ctx->transfer_count);
	edma_status = schedule_edma_xfer(ctx->edma_h, (void *)cr,
					cr->num_edma_desc, cr->edma_desc);
	if (edma_status != EDMA_XFER_SUCCESS) {
		ret = -EIO;
		atomic_dec(&ctx->transfer_count);
		release_copy_request_handles(cr);
		goto reclaim_cr;
	}

	return ret;

reclaim_cr:
	mutex_lock(&ctx->free_lock);
	list_add_tail(&cr->node, &ctx->free_list);
	mutex_unlock(&ctx->free_lock);
	return ret;
}

/* implement NVSCIC2C_PCIE_IOCTL_MAX_COPY_REQUESTS ioctl call. */
static int
ioctl_set_max_copy_requests(struct stream_ext_ctx_t *ctx,
			    struct nvscic2c_pcie_max_copy_args *args)
{
	int ret = 0;
	u32 i = 0;
	struct copy_request *cr = NULL;
	struct list_head *curr = NULL, *next = NULL;

	if (WARN_ON(!args->max_copy_requests ||
			!args->max_flush_ranges ||
			!args->max_post_fences))
		return -EINVAL;

	/* limits already set.*/
	if (WARN_ON(ctx->cr_limits.max_copy_requests ||
			ctx->cr_limits.max_flush_ranges ||
			ctx->cr_limits.max_post_fences))
		return -EINVAL;

	ctx->cr_limits.max_copy_requests = args->max_copy_requests;
	ctx->cr_limits.max_flush_ranges = args->max_flush_ranges;
	ctx->cr_limits.max_post_fences = args->max_post_fences;

	/* allocate one submit-copy params.*/
	ret = allocate_copy_req_params(ctx, &ctx->cr_params);
	if (ret) {
		pr_err("Failed to allocate submit-copy params\n");
		goto clean_up;
	}

	/* allocate the maximum outstanding copy requests we can have.*/
	for (i = 0; i < ctx->cr_limits.max_copy_requests; i++) {
		cr = NULL;
		ret = allocate_copy_request(ctx, &cr);
		if (ret) {
			pr_err("Failed to allocate copy request\n");
			goto clean_up;
		}

		mutex_lock(&ctx->free_lock);
		list_add(&cr->node, &ctx->free_list);
		mutex_unlock(&ctx->free_lock);
	}

	return ret;

clean_up:
	mutex_unlock(&ctx->free_lock);
	list_for_each_safe(curr, next, &ctx->free_list) {
		cr = list_entry(curr, struct copy_request, node);
		list_del(curr);
		free_copy_request(&cr);
	}
	mutex_unlock(&ctx->free_lock);

	free_copy_req_params(&ctx->cr_params);

	return ret;
}

int
stream_extension_ioctl(void *stream_ext_h, unsigned int cmd, void *args)
{
	int ret = 0;
	struct stream_ext_ctx_t *ctx = NULL;

	if (WARN_ON(!stream_ext_h || !args))
		return -EINVAL;

	ctx = (struct stream_ext_ctx_t *)stream_ext_h;
	switch (cmd) {
	case NVSCIC2C_PCIE_IOCTL_MAP:
		ret = ioctl_map_obj
			((struct stream_ext_ctx_t *)ctx,
			 (struct nvscic2c_pcie_map_obj_args *)args);
		break;
	case NVSCIC2C_PCIE_IOCTL_GET_AUTH_TOKEN:
		ret = ioctl_export_obj
			((struct stream_ext_ctx_t *)ctx,
			 (struct nvscic2c_pcie_export_obj_args *)args);
		break;
	case NVSCIC2C_PCIE_IOCTL_GET_HANDLE:
		ret = ioctl_import_obj
			((struct stream_ext_ctx_t *)ctx,
			 (struct nvscic2c_pcie_import_obj_args *)args);
		break;
	case NVSCIC2C_PCIE_IOCTL_FREE:
		ret = ioctl_free_obj
			((struct stream_ext_ctx_t *)ctx,
			 (struct nvscic2c_pcie_free_obj_args *)args);
		break;
	case NVSCIC2C_PCIE_IOCTL_SUBMIT_COPY_REQUEST:
		ret = ioctl_submit_copy_request
			((struct stream_ext_ctx_t *)ctx,
			 (struct nvscic2c_pcie_submit_copy_args *)args);
		break;
	case NVSCIC2C_PCIE_IOCTL_MAX_COPY_REQUESTS:
		ret = ioctl_set_max_copy_requests
			((struct stream_ext_ctx_t *)ctx,
			 (struct nvscic2c_pcie_max_copy_args *)args);
		break;
	default:
		pr_err("(%s): unrecognised nvscic2c-pcie ioclt cmd: 0x%x\n",
		    ctx->ep_name, cmd);
		ret = -ENOTTY;
		break;
	}
	return ret;
}

int
stream_extension_init(struct stream_ext_params *params, void **stream_ext_h)
{
	struct stream_ext_ctx_t *ctx = NULL;

	if (WARN_ON(!params || !stream_ext_h || *stream_ext_h))
		return -EINVAL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (WARN_ON(!ctx))
		return -ENOMEM;

	ctx->drv_mode = params->drv_mode;
	ctx->ep_id = params->ep_id;
	ctx->host1x_pdev = params->host1x_pdev;
	ctx->edma_h = params->edma_h;
	ctx->vmap_h = params->vmap_h;
	ctx->pci_client_h = params->pci_client_h;
	ctx->comm_channel_h = params->comm_channel_h;
	strlcpy(ctx->ep_name, params->ep_name, NAME_MAX);
	memcpy(&ctx->local_node, params->local_node, sizeof(ctx->local_node));
	memcpy(&ctx->peer_node, params->peer_node, sizeof(ctx->peer_node));

	/* copy operations.*/
	mutex_init(&ctx->free_lock);
	INIT_LIST_HEAD(&ctx->free_list);
	atomic_set(&ctx->transfer_count, 0);
	init_waitqueue_head(&ctx->transfer_waitq);

	*stream_ext_h = (void *)ctx;

	return 0;
}

#define MAX_TRANSFER_TIMEOUT_US	(5000000)
void
stream_extension_deinit(void **stream_ext_h)
{
	int ret = 0;
	struct stream_ext_ctx_t *ctx = (struct stream_ext_ctx_t *)*stream_ext_h;
	struct list_head *curr = NULL, *next = NULL;
	struct copy_request *cr = NULL;

	if (!ctx)
		return;

	/* wait for any on-going eDMA/copy(ies). */
	ret = wait_event_interruptible_timeout
			(ctx->transfer_waitq,
			 !(atomic_read(&ctx->transfer_count)),
			 msecs_to_jiffies(MAX_TRANSFER_TIMEOUT_US));
	if (ret <= 0)
		pr_err("eDMA transfers are still in progress\n");

	mutex_lock(&ctx->free_lock);
	list_for_each_safe(curr, next, &ctx->free_list) {
		cr = list_entry(curr, struct copy_request, node);
		list_del(curr);
		free_copy_request(&cr);
	}
	mutex_unlock(&ctx->free_lock);

	free_copy_req_params(&ctx->cr_params);

	mutex_destroy(&ctx->free_lock);

	kfree(ctx);
	*stream_ext_h = NULL;
}

/*
 * Clear edma handle associated with stream extension.
 */
void
stream_extension_edma_deinit(void *stream_ext_h)
{
	struct stream_ext_ctx_t *ctx = (struct stream_ext_ctx_t *)stream_ext_h;

	if (!ctx)
		return;

	ctx->edma_h = NULL;
}

static int
allocate_handle(struct stream_ext_ctx_t *ctx, enum nvscic2c_pcie_obj_type type,
		void *ioctl_args)
{
	int ret = 0;
	s32 handle = -1;
	struct stream_ext_obj *stream_obj = NULL;
	struct vmap_obj_map_params vmap_params = {0};
	struct vmap_obj_attributes vmap_attrib = {0};

	/* one of the two below would apply.*/
	struct nvscic2c_pcie_map_obj_args *map_args =
		(struct nvscic2c_pcie_map_obj_args *)ioctl_args;
	struct nvscic2c_pcie_import_obj_args *import_args =
		(struct nvscic2c_pcie_import_obj_args *)ioctl_args;

	/* create pcie virtual mapping of the obj.*/
	switch (type) {
	case NVSCIC2C_PCIE_OBJ_TYPE_SOURCE_MEM:
		vmap_params.type = VMAP_OBJ_TYPE_MEM;
		vmap_params.u.memobj.mngd = VMAP_MNGD_DEV;
		vmap_params.u.memobj.prot = VMAP_OBJ_PROT_READ;
		vmap_params.u.memobj.fd = map_args->in.fd;
		break;
	case NVSCIC2C_PCIE_OBJ_TYPE_TARGET_MEM:
		vmap_params.type = VMAP_OBJ_TYPE_MEM;
		vmap_params.u.memobj.mngd = VMAP_MNGD_CLIENT;
		vmap_params.u.memobj.prot = VMAP_OBJ_PROT_WRITE;
		vmap_params.u.memobj.fd = map_args->in.fd;
		break;
	case NVSCIC2C_PCIE_OBJ_TYPE_LOCAL_SYNC:
		vmap_params.type = VMAP_OBJ_TYPE_SYNC;
		vmap_params.u.syncobj.pin_reqd = false;
		vmap_params.u.syncobj.fd = map_args->in.fd;
		break;
	case NVSCIC2C_PCIE_OBJ_TYPE_REMOTE_SYNC:
		vmap_params.type = VMAP_OBJ_TYPE_SYNC;
		vmap_params.u.syncobj.pin_reqd = true;
		vmap_params.u.syncobj.mngd = VMAP_MNGD_CLIENT;
		vmap_params.u.syncobj.prot = VMAP_OBJ_PROT_WRITE;
		vmap_params.u.syncobj.fd = map_args->in.fd;
		break;
	case NVSCIC2C_PCIE_OBJ_TYPE_IMPORT:
		vmap_params.type = VMAP_OBJ_TYPE_IMPORT;
		vmap_params.u.importobj.export_desc = import_args->in.desc;
		break;
	default:
		pr_err("Incorrect NVSCIC2C_IOCTL_MAP params\n");
		return -EINVAL;
	}
	ret = vmap_obj_map(ctx->vmap_h, &vmap_params, &vmap_attrib);
	if (ret) {
		if (ret == -EAGAIN)
			pr_info("Failed to map obj of type: (%d)\n", type);
		else
			pr_err("Failed to map obj of type: (%d)\n", type);
		return ret;
	}

	/* bind the pcie virt. mapping to a streaming obj.*/
	stream_obj = kzalloc(sizeof(*stream_obj), GFP_KERNEL);
	if (WARN_ON(!stream_obj)) {
		vmap_obj_unmap(ctx->vmap_h, vmap_attrib.type, vmap_attrib.id);
		return -ENOMEM;
	}

	/*
	 * allocate a UMD handle for this streaming_obj.
	 * O_RDWR is required only for ImportedSyncObjs mmap() from user-space.
	 */
	handle = anon_inode_getfd("nvscic2c-pcie-stream-ext", &fops_default,
				stream_obj, (O_RDWR | O_CLOEXEC));
	if (handle < 0) {
		pr_err("(%s): Failed to get stream obj handle\n", ctx->ep_name);
		vmap_obj_unmap(ctx->vmap_h, vmap_attrib.type, vmap_attrib.id);
		kfree(stream_obj);
		return -EFAULT;
	}

	stream_obj->vmap_h = ctx->vmap_h;
	stream_obj->type = type;
	stream_obj->soc_id = ctx->local_node.soc_id;
	stream_obj->cntrlr_id = ctx->local_node.cntrlr_id;
	stream_obj->ep_id = ctx->ep_id;
	memcpy(&stream_obj->vmap, &vmap_attrib, sizeof(vmap_attrib));
	kref_init(&stream_obj->refcount);

	return handle;
}

static edma_xfer_status_t
schedule_edma_xfer(void *edma_h, void *priv, u64 num_desc,
		   struct tegra_pcie_edma_desc *desc)
{
	struct tegra_pcie_edma_xfer_info info = {0};

	if (WARN_ON(!num_desc || !desc))
		return -EINVAL;

	info.type = EDMA_XFER_WRITE;
	info.channel_num = 0; // no use-case to use all WR channels yet.
	info.desc = desc;
	info.nents = num_desc;
	info.complete = callback_edma_xfer;
	info.priv = priv;

	return tegra_pcie_edma_submit_xfer(edma_h, &info);
}

/* Callback with each async eDMA submit xfer.*/
static void
callback_edma_xfer(void *priv, edma_xfer_status_t status,
			struct tegra_pcie_edma_desc *desc)
{
	struct copy_request *cr = (struct copy_request *)priv;

	/* increment num_local_fences.*/
	if (status == EDMA_XFER_SUCCESS) {
		/* X86 remote end fences are signaled through CPU */
		if (cr->peer_cpu == NVCPU_X86_64)
			signal_remote_post_fences(cr);

		/* Signal local fences for Tegra*/
		signal_local_post_fences(cr);
	}

	/* releases the references of the cubmit-copy handles.*/
	release_copy_request_handles(cr);

	/* reclaim the copy_request for reuse.*/
	mutex_lock(&cr->ctx->free_lock);
	list_add_tail(&cr->node, &cr->ctx->free_list);
	mutex_unlock(&cr->ctx->free_lock);

	atomic_dec(&cr->ctx->transfer_count);
	wake_up_interruptible_all(&cr->ctx->transfer_waitq);
}

static int
prepare_edma_desc(enum drv_mode_t drv_mode, struct copy_req_params *params,
		struct tegra_pcie_edma_desc *desc, u64 *num_desc, enum peer_cpu_t peer_cpu)
{
	u32 i = 0;
	int ret = 0;
	u32 iter = 0;
	s32 handle = -1;
	struct file *filep = NULL;
	struct stream_ext_obj *stream_obj = NULL;
	struct nvscic2c_pcie_flush_range *flush_range = NULL;
	phys_addr_t dummy_addr = 0x0;

	*num_desc = 0;
	for (i = 0; i < params->num_flush_ranges; i++) {
		flush_range = &params->flush_ranges[i];

		filep = fget(flush_range->src_handle);
		stream_obj = filep->private_data;
		desc[iter].src = (stream_obj->vmap.iova + flush_range->offset);
		dummy_addr = stream_obj->vmap.iova;
		fput(filep);

		filep = fget(flush_range->dst_handle);
		stream_obj = filep->private_data;
		if (drv_mode == DRV_MODE_EPC)
			desc[iter].dst = stream_obj->aper;
		else
			desc[iter].dst = stream_obj->vmap.iova;
		desc[iter].dst += flush_range->offset;
		fput(filep);

		desc[iter].sz = flush_range->size;
		iter++;
	}
	/* With Orin as remote end, the remote fence signaling is done using DMA
	 * With X86 as remote end, the remote fence signaling is done using CPU
	 */
	if (peer_cpu == NVCPU_ORIN) {
		for (i = 0; i < params->num_remote_post_fences; i++) {
			handle = params->remote_post_fences[i];
			desc[iter].src = dummy_addr;

			filep = fget(handle);
			stream_obj = filep->private_data;
			if (drv_mode == DRV_MODE_EPC)
				desc[iter].dst = stream_obj->aper;
			else
				desc[iter].dst = stream_obj->vmap.iova;

			fput(filep);

			desc[iter].sz = 4;
			iter++;
		}
	}
	*num_desc += iter;
	return ret;
}

/* this is post eDMA path, must be done with references still taken.*/
static void
signal_local_post_fences(struct copy_request *cr)
{
	u32 i = 0;
	struct stream_ext_obj *stream_obj = NULL;

	for (i = 0; i < cr->num_local_post_fences; i++) {
		stream_obj = cr->local_post_fences[i];
		nvhost_syncpt_cpu_incr_ext(cr->ctx->host1x_pdev,
					   stream_obj->vmap.syncpt_id);
	}
}

static void
signal_remote_post_fences(struct copy_request *cr)
{
	u32 i = 0;
	struct stream_ext_obj *stream_obj = NULL;
	/* Dummy read operation is done on the imported buffer object to ensure
	 * coherence of data on Vidmem of GA100 dGPU, which is connected as an EP to X86.
	 * This is needed as Ampere architecture doesn't support coherence of Write after
	 * Write operation and the dummy read of 4 bytes ensures the data is reconciled in
	 * vid-memory when the consumer waiting on a sysmem semaphore is unblocked.
	 */
	for (i = 0; i < cr->num_remote_buf_objs; i++) {
		stream_obj = cr->remote_buf_objs[i];
		(void)readl(stream_obj->import_obj_map);
	}
	for (i = 0; i < cr->num_remote_post_fences; i++) {
		stream_obj = cr->remote_post_fences[i];
		writeq(cr->remote_post_fence_values[i], stream_obj->import_obj_map);
	}
}

static int
release_copy_request_handles(struct copy_request *cr)
{
	u32 i = 0;
	struct stream_ext_obj *stream_obj = NULL;

	for (i = 0; i < cr->num_handles; i++) {
		stream_obj = cr->handles[i];
		kref_put(&stream_obj->refcount, streamobj_free);
	}

	return 0;
}

static int
cache_copy_request_handles(struct copy_req_params *params,
			   struct copy_request *cr)
{
	u32 i = 0;
	s32 handle = -1;
	struct file *filep = NULL;
	struct stream_ext_obj *stream_obj = NULL;

	cr->num_handles = 0;
	cr->num_local_post_fences = 0;
	cr->num_remote_post_fences = 0;
	cr->num_remote_buf_objs = 0;
	for (i = 0; i < params->num_local_post_fences; i++) {
		handle = params->local_post_fences[i];
		filep = fget(handle);
		stream_obj = filep->private_data;
		kref_get(&stream_obj->refcount);
		cr->handles[cr->num_handles] = stream_obj;
		cr->num_handles++;
		/* collect all local post fences separately for nvhost incr.*/
		cr->local_post_fences[cr->num_local_post_fences] = stream_obj;
		cr->num_local_post_fences++;
		fput(filep);
	}
	for (i = 0; i < params->num_remote_post_fences; i++) {
		handle = params->remote_post_fences[i];
		filep = fget(handle);
		stream_obj = filep->private_data;
		kref_get(&stream_obj->refcount);
		cr->handles[cr->num_handles] = stream_obj;
		cr->num_handles++;
		cr->remote_post_fence_values[i] =  params->remote_post_fence_values[i];
		cr->remote_post_fences[cr->num_remote_post_fences] = stream_obj;
		cr->num_remote_post_fences++;
		fput(filep);
	}
	for (i = 0; i < params->num_flush_ranges; i++) {
		handle = params->flush_ranges[i].src_handle;
		filep = fget(handle);
		stream_obj = filep->private_data;
		kref_get(&stream_obj->refcount);
		cr->handles[cr->num_handles] = stream_obj;
		cr->num_handles++;
		fput(filep);

		handle = params->flush_ranges[i].dst_handle;
		filep = fget(handle);
		stream_obj = filep->private_data;
		kref_get(&stream_obj->refcount);
		cr->handles[cr->num_handles] = stream_obj;
		cr->num_handles++;

		cr->remote_buf_objs[cr->num_remote_buf_objs] = stream_obj;
		cr->num_remote_buf_objs++;
		fput(filep);
	}

	return 0;
}

static int
validate_handle(struct stream_ext_ctx_t *ctx, s32 handle,
		enum nvscic2c_pcie_obj_type type)
{
	int ret = -EINVAL;
	struct stream_ext_obj *stream_obj = NULL;
	struct file *filep = fget(handle);

	if (!filep)
		goto exit;

	if (filep->f_op != &fops_default)
		goto err;

	stream_obj = filep->private_data;
	if (!stream_obj)
		goto err;

	if (stream_obj->marked_for_del)
		goto err;

	if (stream_obj->soc_id != ctx->local_node.soc_id ||
		stream_obj->cntrlr_id != ctx->local_node.cntrlr_id ||
		stream_obj->ep_id != ctx->ep_id)
		goto err;

	if (stream_obj->type != type)
		goto err;

	/* okay.*/
	ret = 0;
err:
	fput(filep);
exit:
	return ret;
}

static int
validate_import_handle(struct stream_ext_ctx_t *ctx, s32 handle,
			u32 import_type)
{
	int ret = 0;
	struct stream_ext_obj *stream_obj = NULL;
	struct file *filep = NULL;

	ret = validate_handle(ctx, handle, NVSCIC2C_PCIE_OBJ_TYPE_IMPORT);
	if (ret)
		return ret;

	filep = fget(handle);
	stream_obj = filep->private_data;
	if (stream_obj->import_type != import_type) {
		fput(filep);
		return -EINVAL;
	}
	fput(filep);

	return ret;
}

static int
validate_flush_range(struct stream_ext_ctx_t *ctx,
		     struct nvscic2c_pcie_flush_range *flush_range)
{
	int ret = 0;
	struct file *filep = NULL;
	struct stream_ext_obj *stream_obj = NULL;

	if (flush_range->size <= 0)
		return -EINVAL;

	if (flush_range->size & 0x3)
		return -EINVAL;

	if (flush_range->offset & 0x3)
		return -EINVAL;

	ret = validate_handle(ctx, flush_range->src_handle,
					NVSCIC2C_PCIE_OBJ_TYPE_SOURCE_MEM);
	if (ret)
		return ret;

	ret = validate_import_handle(ctx, flush_range->dst_handle,
					STREAM_OBJ_TYPE_MEM);
	if (ret)
		return ret;

	filep = fget(flush_range->src_handle);
	stream_obj = filep->private_data;
	if ((flush_range->offset + flush_range->size) > stream_obj->vmap.size) {
		fput(filep);
		return -EINVAL;
	}
	fput(filep);

	filep = fget(flush_range->dst_handle);
	stream_obj = filep->private_data;
	if ((flush_range->offset + flush_range->size) > stream_obj->vmap.size) {
		fput(filep);
		return -EINVAL;
	}
	fput(filep);

	return 0;
}

static int
validate_copy_req_params(struct stream_ext_ctx_t *ctx,
				struct copy_req_params *params)
{
	u32 i = 0;
	int ret = 0;

	/* for each local post-fence.*/
	for (i = 0; i < params->num_local_post_fences; i++) {
		s32 handle = 0;

		handle = params->local_post_fences[i];
		ret = validate_handle(ctx, handle,
					NVSCIC2C_PCIE_OBJ_TYPE_LOCAL_SYNC);
		if (ret)
			return ret;
	}
	/* for each remote post-fence.*/
	for (i = 0; i < params->num_remote_post_fences; i++) {
		s32 handle = 0;

		handle = params->remote_post_fences[i];
		ret = validate_import_handle(ctx, handle, STREAM_OBJ_TYPE_SYNC);
		if (ret)
			return ret;
	}

	/* for each flush-range.*/
	for (i = 0; i < params->num_flush_ranges; i++) {
		struct nvscic2c_pcie_flush_range *flush_range = NULL;

		flush_range = &params->flush_ranges[i];
		ret = validate_flush_range(ctx, flush_range);
		if (ret)
			return ret;
	}

	return ret;
}

static int
copy_args_from_user(struct stream_ext_ctx_t *ctx,
		    struct nvscic2c_pcie_submit_copy_args *args,
		    struct copy_req_params *params)
{
	int ret = 0;

	if (WARN_ON(!args->num_local_post_fences ||
		    !args->num_flush_ranges ||
		    !args->num_remote_post_fences))
		return -EINVAL;

	if ((args->num_local_post_fences + args->num_remote_post_fences) >
	    ctx->cr_limits.max_post_fences)
		return -EINVAL;
	if (args->num_flush_ranges > ctx->cr_limits.max_flush_ranges)
		return -EINVAL;

	params->num_local_post_fences = args->num_local_post_fences;
	params->num_remote_post_fences = args->num_remote_post_fences;
	params->num_flush_ranges = args->num_flush_ranges;

	ret = copy_from_user(params->local_post_fences,
			     (void __user *)args->local_post_fences,
			     (params->num_local_post_fences * sizeof(s32)));
	if (ret < 0)
		return -EFAULT;

	ret = copy_from_user(params->remote_post_fences,
			     (void __user *)args->remote_post_fences,
			     (params->num_remote_post_fences * sizeof(s32)));
	if (ret < 0)
		return -EFAULT;

	ret = copy_from_user(params->remote_post_fence_values,
			     (void __user *)args->remote_post_fence_values,
			     (params->num_remote_post_fences * sizeof(u64)));
	if (ret < 0)
		return -EFAULT;

	ret = copy_from_user(params->flush_ranges,
			     (void __user *)args->flush_ranges,
			     (params->num_flush_ranges *
			      sizeof(struct nvscic2c_pcie_flush_range)));
	if (ret < 0)
		return -EFAULT;

	return 0;
}

static void
free_copy_request(struct copy_request **copy_request)
{
	struct copy_request *cr = *copy_request;

	if (!cr)
		return;

	kfree(cr->local_post_fences);
	kfree(cr->remote_post_fences);
	kfree(cr->remote_buf_objs);
	kfree(cr->remote_post_fence_values);
	kfree(cr->edma_desc);
	kfree(cr->handles);
	kfree(cr);

	*copy_request = NULL;
}

static int
allocate_copy_request(struct stream_ext_ctx_t *ctx,
		      struct copy_request **copy_request)
{
	int ret = 0;
	struct copy_request *cr = NULL;

	/*worst-case allocation for each copy request.*/

	cr = kzalloc(sizeof(*cr), GFP_KERNEL);
	if (WARN_ON(!cr)) {
		ret = -ENOMEM;
		goto err;
	}
	cr->ctx = ctx;

	/* flush range has two handles: src, dst + all possible post_fences.*/
	cr->handles = kzalloc((sizeof(*cr->handles) *
				((2 * ctx->cr_limits.max_flush_ranges) +
				(ctx->cr_limits.max_post_fences))),
				GFP_KERNEL);
	if (WARN_ON(!cr->handles)) {
		ret = -ENOMEM;
		goto err;
	}

	/*
	 * edma_desc shall include flush_range + worst-case all post-fences
	 * (all max_post_fences could be remote_post_fence which need be eDMAd).
	 */
	cr->edma_desc = kzalloc((sizeof(*cr->edma_desc) *
				(ctx->cr_limits.max_flush_ranges +
				ctx->cr_limits.max_post_fences)),
				GFP_KERNEL);
	if (WARN_ON(!cr->edma_desc)) {
		ret = -ENOMEM;
		goto err;
	}

	/* OR all max_post_fences could be local_post_fence. */
	cr->local_post_fences = kzalloc((sizeof(*cr->local_post_fences) *
					ctx->cr_limits.max_post_fences),
					GFP_KERNEL);
	if (WARN_ON(!cr->local_post_fences)) {
		ret = -ENOMEM;
		goto err;
	}
	cr->remote_post_fences = kzalloc((sizeof(*cr->remote_post_fences) *
					ctx->cr_limits.max_post_fences),
					GFP_KERNEL);
	if (WARN_ON(!cr->remote_post_fences)) {
		ret = -ENOMEM;
		goto err;
	}

	cr->remote_buf_objs = kzalloc((sizeof(*cr->remote_buf_objs) *
					ctx->cr_limits.max_flush_ranges),
					GFP_KERNEL);
	if (WARN_ON(!cr->remote_buf_objs)) {
		ret = -ENOMEM;
		goto err;
	}

	cr->remote_post_fence_values =
				kzalloc((sizeof(*cr->remote_post_fence_values) *
				ctx->cr_limits.max_post_fences),
				GFP_KERNEL);
	if (WARN_ON(!cr->remote_post_fence_values)) {
		ret = -ENOMEM;
		goto err;
	}

	*copy_request = cr;
	return ret;
err:
	free_copy_request(&cr);
	return ret;
}

static void
free_copy_req_params(struct copy_req_params *params)
{
	if (!params)
		return;

	kfree(params->flush_ranges);
	params->flush_ranges = NULL;
	kfree(params->local_post_fences);
	params->local_post_fences = NULL;
	kfree(params->remote_post_fences);
	params->remote_post_fences = NULL;
	kfree(params->remote_post_fence_values);
	params->remote_post_fence_values = NULL;
}

static int
allocate_copy_req_params(struct stream_ext_ctx_t *ctx,
			 struct copy_req_params *params)
{
	int ret = 0;

	/*worst-case allocation for each.*/

	params->flush_ranges = kzalloc((sizeof(*params->flush_ranges) *
					ctx->cr_limits.max_flush_ranges),
				       GFP_KERNEL);
	if (WARN_ON(!params->flush_ranges)) {
		ret = -ENOMEM;
		goto err;
	}
	params->local_post_fences =
				kzalloc((sizeof(*params->local_post_fences) *
					 ctx->cr_limits.max_post_fences),
					GFP_KERNEL);
	if (WARN_ON(!params->local_post_fences)) {
		ret = -ENOMEM;
		goto err;
	}
	params->remote_post_fences =
				kzalloc((sizeof(*params->remote_post_fences) *
					 ctx->cr_limits.max_post_fences),
					GFP_KERNEL);
	if (WARN_ON(!params->remote_post_fences)) {
		ret = -ENOMEM;
		goto err;
	}
	params->remote_post_fence_values =
				kzalloc((sizeof(*params->remote_post_fence_values) *
					 ctx->cr_limits.max_post_fences),
					GFP_KERNEL);
	if (WARN_ON(!params->remote_post_fence_values)) {
		ret = -ENOMEM;
		goto err;
	}

	return ret;
err:
	free_copy_req_params(params);
	return ret;
}
