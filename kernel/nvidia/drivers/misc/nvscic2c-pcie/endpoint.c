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

#define pr_fmt(fmt)	"nvscic2c-pcie: endpoint: " fmt

#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/dma-iommu.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/nvhost.h>
#include <linux/nvhost_t194.h>
#include <linux/nvhost_interrupt_syncpt.h>
#include <linux/poll.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <uapi/misc/nvscic2c-pcie-ioctl.h>

#include "common.h"
#include "endpoint.h"
#include "module.h"
#include "pci-client.h"
#include "stream-extensions.h"

#define PCIE_STATUS_CHANGE_ACK_TIMEOUT (2000)

/*
 * Masked offsets to return to user, allowing them to mmap
 * different memory segments of endpoints in user-space.
 */
enum mem_mmap_type {
	/* Invalid.*/
	MEM_MMAP_INVALID = 0,
	/* Map Peer PCIe aperture: For Tx across PCIe.*/
	PEER_MEM_MMAP,
	/* Map Self PCIe shared memory: For Rx across PCIe.*/
	SELF_MEM_MMAP,
	/* Map Link memory segment to query link status with Peer.*/
	LINK_MEM_MMAP,
	/* Maximum. */
	MEM_MAX_MMAP,
};

/* syncpoint handling. */
struct syncpt_t {
	/* reference to syncpoint shim.*/
	struct nvhost_interrupt_syncpt *is;

	/* tasklet/worker for reattaching the callback to next syncpoint shim.*/
	struct work_struct reprime_work;

	/* PCIe aperture for writes to peer syncpoint for same the endpoint. */
	struct pci_aper_t peer_mem;

	/* syncpoint physical address for stritching to PCIe BAR backing.*/
	size_t size;
	phys_addr_t phy_addr;

	/* for mapping above physical pages to iova of client choice.*/
	void *iova_block_h;
	u64 iova;
	bool mapped_iova;
};

/* private data structure for each endpoint. */
struct endpoint_t {
	/* properties / attributes of this endpoint.*/
	char name[NAME_MAX];

	/* char device management.*/
	int minor;
	dev_t dev;
	struct cdev cdev;
	struct device *device;

	/* slot/frames this endpoint is divided into honoring alignment.*/
	u32 nframes;
	u32 frame_sz;

	/* allocated physical memory info for mmap.*/
	struct cpu_buff_t self_mem;

	/* mapping physical pages to iova of client choice.*/
	void *iova_block_h;
	u64 iova;
	bool mapped_iova;

	/* PCIe aperture for writes to peer over pcie. */
	struct pci_aper_t peer_mem;

	/* poll/notifications.*/
	wait_queue_head_t waitq;

	/* syncpoint shim for notifications (rx). */
	struct syncpt_t syncpt;

	/* msi irq to x86 RP */
	u16 msi_irq;

	/* book-keeping of peer notifications.*/
	atomic_t dataevent_count;

	/* book-keeping of PCIe link event.*/
	atomic_t linkevent_count;
	u32 linkevent_id;

	/* propagate events when endpoint was initialized.*/
	atomic_t event_handling;

	/* serialise access to fops.*/
	struct mutex fops_lock;
	atomic_t in_use;
	bool link_status_ack_frm_usr;
	wait_queue_head_t ack_waitq;
	wait_queue_head_t close_waitq;

	/* pci client handle.*/
	void *pci_client_h;

	/* stream extensions.*/
	struct stream_ext_params stream_ext_params;
	void *stream_ext_h;
};

/* Overall context for the endpoint sub-module of  nvscic2c-pcie driver.*/
struct endpoint_drv_ctx_t {
	/* entire char device region allocated for all endpoints.*/
	dev_t char_dev;

	/* every endpoint char device will be registered to this class.*/
	struct class *class;

	/* array of nvscic2c-pcie endpoint logical devices.*/
	u8 nr_endpoint;
	struct endpoint_t *endpoints;

	/* nvscic2c-pcie DT node reference, used in getting syncpoint shim. */
	struct device_node *of_node;
};

/*
 * pci-client would raise this callback only when there is change
 * in PCIe link status(up->down OR down->up).
 */
static void
link_event_callback(void *event_type, void *ctx);

/* prototype. */
static int
enable_event_handling(struct endpoint_t *endpoint);

/* prototype. */
static int
disable_event_handling(struct endpoint_t *endpoint);

/* prototype. */
static int
ioctl_notify_remote_impl(struct endpoint_t *endpoint);

static int
link_change_ack(struct endpoint_t *endpoint,
		struct nvscic2c_link_change_ack *ack);

/* prototype. */
static int
ioctl_get_info_impl(struct endpoint_t *endpoint,
		    struct nvscic2c_pcie_endpoint_info *get_info);

/*
 * open() syscall backing for nvscic2c-pcie endpoint devices.
 *
 * Populate the endpoint_device internal data-structure into fops private data
 * for subsequent calls to other fops handlers.
 */
static int
endpoint_fops_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct endpoint_t *endpoint =
		container_of(inode->i_cdev, struct endpoint_t, cdev);

	mutex_lock(&endpoint->fops_lock);
	if (atomic_read(&endpoint->in_use)) {
		/* already in use.*/
		mutex_unlock(&endpoint->fops_lock);
		return -EBUSY;
	}

	/* create stream extension handle.*/
	ret = stream_extension_init(&endpoint->stream_ext_params,
				    &endpoint->stream_ext_h);
	if (ret) {
		pr_err("Failed setting up stream extension handle: (%s)\n",
		       endpoint->name);
		goto err;
	}

	/* start link, data event handling.*/
	ret = enable_event_handling(endpoint);
	if (ret) {
		pr_err("(%s): Failed to enable link, syncpt event handling\n",
		       endpoint->name);
		stream_extension_deinit(&endpoint->stream_ext_h);
		goto err;
	}

	filp->private_data = endpoint;
	endpoint->link_status_ack_frm_usr = true;
	atomic_set(&endpoint->in_use, 1);
err:
	mutex_unlock(&endpoint->fops_lock);
	return ret;
}

/* close() syscall backing for nvscic2c-pcie endpoint devices.*/
static int
endpoint_fops_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct nvscic2c_link_change_ack ack = {0};
	struct endpoint_t *endpoint = filp->private_data;

	if (!endpoint)
		return ret;

	mutex_lock(&endpoint->fops_lock);
	if (atomic_read(&endpoint->in_use)) {
		disable_event_handling(endpoint);
		stream_extension_deinit(&endpoint->stream_ext_h);
		ack.done = false;
		link_change_ack(endpoint, &ack);
		atomic_set(&endpoint->in_use, 0);
		wake_up_interruptible_all(&endpoint->close_waitq);
	}
	filp->private_data = NULL;
	mutex_unlock(&endpoint->fops_lock);

	return ret;
}

/*
 * mmap() syscall backing for nvscic2c-pcie endpoint device.
 *
 * We support mapping following distinct regions of memory:
 * - Peer's memory for same endpoint(used for Tx),
 * - Self's memory (used for Rx),
 * - pci-client link status memory.
 *
 * We map just one segment of memory in each call based on the information
 * (which memory segment) provided by user-space code.
 */
static int
endpoint_fops_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct endpoint_t *endpoint = filp->private_data;
	u64 mmap_type = vma->vm_pgoff;
	u64 memaddr = 0x0;
	u64 memsize = 0x0;
	int ret = 0;

	if (WARN_ON(!endpoint))
		return -EFAULT;

	if (WARN_ON(!(vma)))
		return -EFAULT;

	mutex_lock(&endpoint->fops_lock);

	switch (mmap_type) {
	case PEER_MEM_MMAP:
		vma->vm_page_prot = pgprot_device(vma->vm_page_prot);
		memaddr = endpoint->peer_mem.aper;
		memsize = endpoint->peer_mem.size;
		break;
	case SELF_MEM_MMAP:
		memaddr = endpoint->self_mem.phys_addr;
		memsize = endpoint->self_mem.size;
		break;
	case LINK_MEM_MMAP:
		if (vma->vm_flags & VM_WRITE) {
			ret = -EPERM;
			pr_err("(%s): LINK_MEM_MMAP called with WRITE prot\n",
			       endpoint->name);
			goto exit;
		}
		ret = pci_client_mmap_link_mem(endpoint->pci_client_h, vma);
		goto exit;
	default:
		pr_err("(%s): unrecognised mmap type: (%llu)\n",
		       endpoint->name, mmap_type);
		goto exit;
	}

	if ((vma->vm_end - vma->vm_start) != memsize) {
		pr_err("(%s): mmap type: (%llu), memsize mismatch\n",
		       endpoint->name, mmap_type);
		goto exit;
	}

	vma->vm_pgoff  = 0;
	vma->vm_flags |= (VM_DONTCOPY); // fork() not supported.
	ret = remap_pfn_range(vma, vma->vm_start,
			      PFN_DOWN(memaddr),
			      memsize, vma->vm_page_prot);
	if (ret) {
		pr_err("(%s): mmap() failed, mmap type:(%llu)\n",
		       endpoint->name, mmap_type);
	}
exit:
	mutex_unlock(&endpoint->fops_lock);
	return ret;
}

/*
 * poll() syscall backing for nvscic2c-pcie endpoint devices.
 *
 * user-space code shall call poll with FD on read, write and probably exception
 * for endpoint state changes.
 *
 * If we are able to read(), write() or there is a pending state change event
 * to be serviced, we return letting application call get_event(), otherwise
 * kernel f/w will wait for waitq activity to occur.
 */
static __poll_t
endpoint_fops_poll(struct file *filp, poll_table *wait)
{
	__poll_t mask = 0;
	struct endpoint_t *endpoint = filp->private_data;

	if (WARN_ON(!endpoint))
		return POLLNVAL;

	mutex_lock(&endpoint->fops_lock);

	/* add all waitq if they are different for read, write & link+state.*/
	poll_wait(filp, &endpoint->waitq, wait);

	/*
	 * wake up read, write (& exception - those who want to use) fd on
	 * getting Link + peer notifications.
	 */
	if (atomic_read(&endpoint->linkevent_count)) {
		atomic_dec(&endpoint->linkevent_count);
		mask = (POLLPRI | POLLIN | POLLOUT);
	} else if (atomic_read(&endpoint->dataevent_count)) {
		atomic_dec(&endpoint->dataevent_count);
		mask = (POLLPRI | POLLIN | POLLOUT);
	}

	mutex_unlock(&endpoint->fops_lock);

	return mask;
}

/* ioctl() syscall backing for nvscic2c-pcie endpoint device. */
#define MAX_IOCTL_ARG_SIZE (sizeof(union nvscic2c_pcie_ioctl_arg_max_size))
static long
endpoint_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	u8 buf[MAX_IOCTL_ARG_SIZE] __aligned(sizeof(u64)) = {0};
	struct endpoint_t *endpoint = filp->private_data;

	if (WARN_ON(!endpoint))
		return -EFAULT;

	if (WARN_ON(_IOC_TYPE(cmd) != NVSCIC2C_PCIE_IOCTL_MAGIC ||
		    _IOC_NR(cmd) == 0 ||
		    _IOC_NR(cmd) > NVSCIC2C_PCIE_IOCTL_NUMBER_MAX) ||
		    _IOC_SIZE(cmd) > MAX_IOCTL_ARG_SIZE)
		return -ENOTTY;

	/* copy the cmd if it was meant from user->kernel. */
	(void)memset(buf, 0, sizeof(buf));
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	mutex_lock(&endpoint->fops_lock);
	switch (cmd) {
	case NVSCIC2C_PCIE_IOCTL_GET_INFO:
		ret = ioctl_get_info_impl
			(endpoint, (struct nvscic2c_pcie_endpoint_info *)buf);
		break;
	case NVSCIC2C_PCIE_IOCTL_NOTIFY_REMOTE:
		ret = ioctl_notify_remote_impl(endpoint);
		break;
	case NVSCIC2C_PCIE_LINK_STATUS_CHANGE_ACK:
		link_change_ack(endpoint,
				(struct nvscic2c_link_change_ack *)buf);
		break;
	default:
		ret = stream_extension_ioctl(endpoint->stream_ext_h, cmd, buf);
		break;
	}
	mutex_unlock(&endpoint->fops_lock);

	/* copy the cmd result back to user if it was kernel->user: get_info.*/
	if (ret == 0 && (_IOC_DIR(cmd) & _IOC_READ))
		ret = copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd));
	return ret;
}

/*
 * All important endpoint dev node properites required for user-space
 * to map the channel memory and work without going to LKM for data
 * xfer are exported in this ioctl implementation.
 *
 * Because we export different memory for a single nvscic2c-pcie endpoint,
 * export the memory regions as masked offsets.
 */
static int
ioctl_get_info_impl(struct endpoint_t *endpoint,
		    struct nvscic2c_pcie_endpoint_info *get_info)
{
	get_info->nframes     = endpoint->nframes;
	get_info->frame_size  = endpoint->frame_sz;
	get_info->peer.offset = (PEER_MEM_MMAP << PAGE_SHIFT);
	get_info->peer.size   = endpoint->peer_mem.size;
	get_info->self.offset = (SELF_MEM_MMAP << PAGE_SHIFT);
	get_info->self.size   = endpoint->self_mem.size;
	get_info->link.offset = (LINK_MEM_MMAP << PAGE_SHIFT);
	get_info->link.size   = PAGE_ALIGN(sizeof(enum nvscic2c_pcie_link));

	return 0;
}

/*
 * implement NVSCIC2C_PCIE_IOCTL_NOTIFY_REMOTE ioctl call.
 */
static int
ioctl_notify_remote_impl(struct endpoint_t *endpoint)
{
	int ret = 0;
	enum nvscic2c_pcie_link link = NVSCIC2C_PCIE_LINK_DOWN;
	struct syncpt_t *syncpt = &endpoint->syncpt;
	enum peer_cpu_t peer_cpu = NVCPU_ORIN;

	link = pci_client_query_link_status(endpoint->pci_client_h);
	peer_cpu =  pci_client_get_peer_cpu(endpoint->pci_client_h);

	if (link != NVSCIC2C_PCIE_LINK_UP)
		return -ENOLINK;

	if (peer_cpu == NVCPU_X86_64) {
		ret = pci_client_raise_irq(endpoint->pci_client_h, PCI_EPC_IRQ_MSI,
					   endpoint->msi_irq);
	} else {
	/*
	 * increment peer's syncpoint. Write of any 4-byte value
	 * increments remote's syncpoint shim by 1.
	 */
		writel(0x1, syncpt->peer_mem.pva);
	}

	return ret;
}

static int
link_change_ack(struct endpoint_t *endpoint,
		struct nvscic2c_link_change_ack *ack)
{
	endpoint->link_status_ack_frm_usr = ack->done;
	wake_up_interruptible_all(&endpoint->ack_waitq);

	return 0;
}

static int
enable_event_handling(struct endpoint_t *endpoint)
{
	int ret = 0;

	/*
	 * propagate link and state change events that occur after the device
	 * is opened and not the stale ones.
	 */
	atomic_set(&endpoint->dataevent_count, 0);
	atomic_set(&endpoint->linkevent_count, 0);
	atomic_set(&endpoint->event_handling, 1);

	return ret;
}

static int
disable_event_handling(struct endpoint_t *endpoint)
{
	int ret = 0;

	if (!endpoint)
		return ret;

	atomic_set(&endpoint->event_handling, 0);
	atomic_set(&endpoint->linkevent_count, 0);
	atomic_set(&endpoint->dataevent_count, 0);

	return ret;
}

static void
link_event_callback(void *data, void *ctx)
{
	struct endpoint_t *endpoint = NULL;

	if (!ctx) {
		pr_err("Spurious link event callback\n");
		return;
	}

	endpoint = (struct endpoint_t *)(ctx);

	/* notify only if the endpoint was openend.*/
	if (atomic_read(&endpoint->event_handling)) {
		atomic_inc(&endpoint->linkevent_count);
		wake_up_interruptible_all(&endpoint->waitq);
	}
}

/*
 * tasklet/scheduled work for reattaching to nvhost syncpoint
 * callback with for the next fence value. The increment happens
 * under the hood in the nvhost api.
 */
static void
irqsp_reprime_work(struct work_struct *work)
{
	struct syncpt_t *syncpt =
		container_of(work, struct syncpt_t, reprime_work);

	nvhost_interrupt_syncpt_prime(syncpt->is);
}

/*
 * Callback registered with Syncpoint shim, shall be invoked
 * on expiry of syncpoint shim fence/trigger from remote.
 */
static void
syncpt_callback(void *data)
{
	/* Skip args ceck, trusting host1x. */

	struct endpoint_t *endpoint = (struct endpoint_t *)(data);
	struct syncpt_t *syncpt = &endpoint->syncpt;

	/* notify only if the endpoint was openend - else drain.*/
	if (atomic_read(&endpoint->event_handling)) {
		atomic_inc(&endpoint->dataevent_count);
		wake_up_interruptible_all(&endpoint->waitq);
	}

	/* look for next increment. */
	schedule_work(&syncpt->reprime_work);
}

/*
 * unpin/unmap and free the syncpoints allocated.
 */
static void
free_syncpoint(struct endpoint_drv_ctx_t *eps_ctx,
	       struct endpoint_t *endpoint)
{
	struct syncpt_t *syncpt = NULL;

	if (!eps_ctx || !endpoint)
		return;

	syncpt = &endpoint->syncpt;

	cancel_work_sync(&syncpt->reprime_work);

	if (syncpt->peer_mem.pva) {
		iounmap(syncpt->peer_mem.pva);
		syncpt->peer_mem.pva = NULL;
	}

	if (syncpt->mapped_iova) {
		pci_client_unmap_addr(endpoint->pci_client_h,
				      syncpt->iova, syncpt->size);
		syncpt->mapped_iova = false;
	}

	if (syncpt->iova_block_h) {
		pci_client_free_iova(endpoint->pci_client_h,
				     &syncpt->iova_block_h);
		syncpt->iova_block_h = NULL;
	}

	if (syncpt->is) {
		nvhost_interrupt_syncpt_free(syncpt->is);
		syncpt->is = NULL;
	}
}

/* Allocate syncpoint shim for the endpoint. Subsequently, map/pin
 * them to PCIe BAR backing.
 */
static int
allocate_syncpoint(struct endpoint_drv_ctx_t *eps_ctx,
		   struct endpoint_t *endpoint)
{
	int ret = 0;
	int prot = 0;
	struct syncpt_t *syncpt = NULL;
	size_t offsetof = 0x0;

	syncpt = &endpoint->syncpt;

	/* nvscic2c-pcie device-tree node has host1x phandle.*/
	syncpt->is = nvhost_interrupt_syncpt_get(eps_ctx->of_node,
						 syncpt_callback,
						 endpoint);
	if (IS_ERR(syncpt->is)) {
		syncpt->is = NULL;
		ret = -ENOMEM;
		pr_err("(%s): Failed to reserve syncpt\n", endpoint->name);
		goto err;
	}

	/* physical address of syncpoint shim. */
	syncpt->phy_addr = nvhost_interrupt_syncpt_get_syncpt_addr(syncpt->is);
	syncpt->size = SP_SIZE;

	/* reserve iova with the iova manager.*/
	ret = pci_client_alloc_iova(endpoint->pci_client_h, syncpt->size,
				    &syncpt->iova, &offsetof,
				    &syncpt->iova_block_h);
	if (ret) {
		pr_err("(%s): Err reserving iova region of size(SP): (%lu)\n",
		       endpoint->name, syncpt->size);
		goto err;
	}

	/* map the pages to the reserved iova.*/
	prot = (IOMMU_CACHE | IOMMU_READ | IOMMU_WRITE);
	ret = pci_client_map_addr(endpoint->pci_client_h, syncpt->iova,
				  syncpt->phy_addr, syncpt->size, prot);
	if (ret) {
		pr_err("(%s): Failed to map SP physical addr to reserved iova\n",
		       endpoint->name);
		goto err;
	}
	syncpt->mapped_iova = true;

	pr_debug("(%s): mapped phy:0x%pa[p]+0x%lx to iova:0x%llx\n",
		 endpoint->name, &syncpt->phy_addr, syncpt->size, syncpt->iova);

	/* get peer's aperture offset. Map tx (pcie aper for notif tx.)*/
	syncpt->peer_mem.size = syncpt->size;
	ret = pci_client_get_peer_aper(endpoint->pci_client_h, offsetof,
				       syncpt->peer_mem.size,
				       &syncpt->peer_mem.aper);
	if (ret) {
		pr_err("Failed to get comm peer's syncpt pcie aperture\n");
		goto err;
	}

	syncpt->peer_mem.pva = ioremap(syncpt->peer_mem.aper,
				       syncpt->peer_mem.size);
	if (!syncpt->peer_mem.pva) {
		ret = -ENOMEM;
		pr_err("(%s): Failed to ioremap peer's syncpt pcie aperture\n",
		       endpoint->name);
		goto err;
	}

	/*
	 * every callback will have this scheduled to re-attach the
	 * syncpoint callback with higher fence value. This would have
	 * some latency.
	 */
	INIT_WORK(&syncpt->reprime_work, irqsp_reprime_work);
	nvhost_interrupt_syncpt_prime(syncpt->is);

	return ret;
err:
	free_syncpoint(eps_ctx, endpoint);
	return ret;
}

/* unmap the memory from PCIe BAR iova and free the allocated physical pages. */
static void
free_memory(struct endpoint_drv_ctx_t *eps_ctx, struct endpoint_t *endpoint)
{
	if (!eps_ctx || !endpoint)
		return;

	if (endpoint->mapped_iova) {
		pci_client_unmap_addr(endpoint->pci_client_h,
				      endpoint->iova, endpoint->self_mem.size);
		endpoint->mapped_iova = false;
	}

	if (endpoint->iova_block_h) {
		pci_client_free_iova(endpoint->pci_client_h,
				     &endpoint->iova_block_h);
		endpoint->iova_block_h = NULL;
	}

	if (endpoint->self_mem.pva) {
		free_pages_exact(endpoint->self_mem.pva,
				 endpoint->self_mem.size);
		endpoint->self_mem.pva = NULL;
	}
}

/*
 * allocate coniguous physical memory for endpoint. This shall be mapped
 * to PCIe BAR iova.
 */
static int
allocate_memory(struct endpoint_drv_ctx_t *eps_ctx, struct endpoint_t *ep)
{
	int ret = 0;
	int prot = 0;
	size_t offsetof = 0x0;

	/*
	 * memory size includes space for frames(aligned to PAGE_SIZE) plus
	 * one additional PAGE for frames header (managed/used by user-space).
	 */
	ep->self_mem.size = (ep->nframes * ep->frame_sz);
	ep->self_mem.size = ALIGN(ep->self_mem.size, PAGE_SIZE);
	ep->self_mem.size += PAGE_SIZE;
	ep->self_mem.pva = alloc_pages_exact(ep->self_mem.size,
					     (GFP_KERNEL | __GFP_ZERO));
	if (!ep->self_mem.pva) {
		ret = -ENOMEM;
		pr_err("(%s): Error allocating: (%lu) contiguous pages\n",
		       ep->name, (ep->self_mem.size >> PAGE_SHIFT));
		goto err;
	}
	ep->self_mem.phys_addr = page_to_phys(virt_to_page(ep->self_mem.pva));
	pr_debug("(%s): physical page allocated at:(0x%pa[p]+0x%lx)\n",
		 ep->name, &ep->self_mem.phys_addr, ep->self_mem.size);

	/* reserve iova with the iova manager.*/
	ret = pci_client_alloc_iova(ep->pci_client_h, ep->self_mem.size,
				    &ep->iova, &offsetof, &ep->iova_block_h);
	if (ret) {
		pr_err("(%s): Failed to reserve iova region of size: 0x%lx\n",
		       ep->name, ep->self_mem.size);
		goto err;
	}

	/* map the pages to the reserved iova.*/
	prot = (IOMMU_CACHE | IOMMU_READ | IOMMU_WRITE);
	ret = pci_client_map_addr(ep->pci_client_h, ep->iova,
				  ep->self_mem.phys_addr, ep->self_mem.size,
				  prot);
	if (ret) {
		pr_err("(%s): Failed to map physical page to reserved iova\n",
		       ep->name);
		goto err;
	}
	ep->mapped_iova = true;

	pr_debug("(%s): mapped page:0x%pa[p]+0x%lx to iova:0x%llx\n", ep->name,
		 &ep->self_mem.phys_addr, ep->self_mem.size, ep->iova);

	/* get peer's aperture offset. Used in mmaping tx mem.*/
	ep->peer_mem.size = ep->self_mem.size;
	ret = pci_client_get_peer_aper(ep->pci_client_h, offsetof,
				       ep->peer_mem.size, &ep->peer_mem.aper);
	if (ret) {
		pr_err("Failed to get peer's endpoint pcie aperture\n");
		goto err;
	}

	return ret;
err:
	free_memory(eps_ctx, ep);
	return ret;
}

/*
 * Set of per-endpoint char device file operations. Do not support:
 * read() and write() on nvscic2c-pcie endpoint descriptors.
 */
static const struct file_operations endpoint_fops = {
	.owner          = THIS_MODULE,
	.open           = endpoint_fops_open,
	.release        = endpoint_fops_release,
	.mmap           = endpoint_fops_mmap,
	.unlocked_ioctl = endpoint_fops_ioctl,
	.poll           = endpoint_fops_poll,
	.llseek         = noop_llseek,
};

/* Clean up the endpoint devices. */
static int
remove_endpoint_device(struct endpoint_drv_ctx_t *eps_ctx,
		       struct endpoint_t *endpoint)
{
	int ret = 0;

	if (!eps_ctx || !endpoint)
		return ret;

	wait_event_interruptible(endpoint->close_waitq, !(atomic_read(&endpoint->in_use)));

	pci_client_unregister_for_link_event(endpoint->pci_client_h,
					     endpoint->linkevent_id);
	free_syncpoint(eps_ctx, endpoint);
	free_memory(eps_ctx, endpoint);
	atomic_set(&endpoint->in_use, 0);
	mutex_destroy(&endpoint->fops_lock);

	if (endpoint->device) {
		cdev_del(&endpoint->cdev);
		device_del(endpoint->device);
		endpoint->device = NULL;
	}

	return ret;
}

/* Create the nvscic2c-pcie endpoint devices for the user-space to:
 * - Map the endpoints Self and Peer area.
 * - send notifications to remote/peer.
 * - receive notifications from peer.
 */
static int
create_endpoint_device(struct endpoint_drv_ctx_t *eps_ctx,
		       struct endpoint_t *endpoint)
{
	int ret = 0;
	struct callback_ops ops = {0};

	/* create the nvscic2c endpoint char device.*/
	endpoint->dev = MKDEV(MAJOR(eps_ctx->char_dev), endpoint->minor);
	cdev_init(&endpoint->cdev, &endpoint_fops);
	endpoint->cdev.owner = THIS_MODULE;
	ret = cdev_add(&endpoint->cdev, endpoint->dev, 1);
	if (ret != 0) {
		pr_err("(%s): cdev_add() failed\n", endpoint->name);
		goto err;
	}
	/* parent is this hvd dev */
	endpoint->device = device_create(eps_ctx->class, NULL,
					 endpoint->dev, endpoint,
					 endpoint->name);
	if (IS_ERR(endpoint->device)) {
		ret = PTR_ERR(endpoint->device);
		pr_err("(%s): device_create() failed\n", endpoint->name);
		goto err;
	}
	dev_set_drvdata(endpoint->device, endpoint);

	/* initialise the endpoint internals.*/
	mutex_init(&endpoint->fops_lock);
	atomic_set(&endpoint->in_use, 0);
	init_waitqueue_head(&endpoint->waitq);
	endpoint->link_status_ack_frm_usr = false;
	init_waitqueue_head(&endpoint->ack_waitq);
	init_waitqueue_head(&endpoint->close_waitq);

	/* allocate physical pages for the endpoint PCIe BAR (rx) area.*/
	ret = allocate_memory(eps_ctx, endpoint);
	if (ret) {
		pr_err("(%s): Failed to allocate physical pages\n",
		       endpoint->name);
		goto err;
	}

	/* allocate syncpoint for notification.*/
	ret = allocate_syncpoint(eps_ctx, endpoint);
	if (ret) {
		pr_err("(%s): Failed to allocate syncpt shim for notifications\n",
		       endpoint->name);
		goto err;
	}

	/* Register for link events.*/
	ops.callback = &(link_event_callback);
	ops.ctx = (void *)(endpoint);
	ret = pci_client_register_for_link_event(endpoint->pci_client_h, &ops,
						 &endpoint->linkevent_id);
	if (ret) {
		pr_err("(%s): Failed to register for PCIe link events\n",
		       endpoint->name);
	}

	/* all okay.*/
	return ret;
err:
	remove_endpoint_device(eps_ctx, endpoint);
	return ret;
}

/*
 * Entry point for the endpoint(s) char device sub-module/abstraction.
 *
 * On successful return (0), devices would have been created and ready to
 * accept ioctls from user-space application.
 */
int
endpoints_setup(struct driver_ctx_t *drv_ctx, void **endpoints_h)
{
	u32 i = 0;
	int ret = 0;
	struct endpoint_t *endpoint = NULL;
	struct endpoint_prop_t *ep_prop = NULL;
	struct endpoint_drv_ctx_t *eps_ctx = NULL;
	struct stream_ext_params *stream_ext_params = NULL;

	/* this cannot be initialized again.*/
	if (WARN_ON(!drv_ctx || !endpoints_h || *endpoints_h))
		return -EINVAL;

	if (WARN_ON(drv_ctx->drv_param.nr_endpoint == 0 ||
		    drv_ctx->drv_param.nr_endpoint > MAX_ENDPOINTS))
		return -EINVAL;

	/* start by allocating the endpoint driver (global for all eps) ctx.*/
	eps_ctx = kzalloc(sizeof(*eps_ctx), GFP_KERNEL);
	if (WARN_ON(!eps_ctx))
		return -ENOMEM;

	eps_ctx->nr_endpoint = drv_ctx->drv_param.nr_endpoint;
	eps_ctx->of_node = drv_ctx->drv_param.of_node;

	/* allocate the whole chardev range */
	ret = alloc_chrdev_region(&eps_ctx->char_dev, 0,
				  eps_ctx->nr_endpoint, drv_ctx->drv_name);
	if (ret < 0)
		goto err;

	eps_ctx->class = class_create(THIS_MODULE, drv_ctx->drv_name);
	if (IS_ERR_OR_NULL(eps_ctx->class)) {
		ret = PTR_ERR(eps_ctx->class);
		goto err;
	}

	/* allocate char devices context for supported endpoints.*/
	eps_ctx->endpoints = kzalloc((eps_ctx->nr_endpoint *
				      sizeof(*eps_ctx->endpoints)),
				     GFP_KERNEL);
	if (WARN_ON(!eps_ctx->endpoints)) {
		ret = -ENOMEM;
		goto err;
	}

	/* create char devices for each endpoint.*/
	for (i = 0; i < eps_ctx->nr_endpoint; i++) {
		endpoint = &eps_ctx->endpoints[i];
		ep_prop = &drv_ctx->drv_param.endpoint_props[i];
		stream_ext_params = &endpoint->stream_ext_params;

		/* copy the parameters from nvscic2c-pcie driver ctx.*/
		strcpy(endpoint->name, ep_prop->name);
		endpoint->minor = ep_prop->id;
		endpoint->nframes = ep_prop->nframes;
		endpoint->frame_sz = ep_prop->frame_sz;
		endpoint->pci_client_h = drv_ctx->pci_client_h;
		/* set index of the msi-x interruper vector
		 * where the first one is reserved for comm-channel
		 */
		endpoint->msi_irq = i + 1;
		stream_ext_params->local_node = &drv_ctx->drv_param.local_node;
		stream_ext_params->peer_node = &drv_ctx->drv_param.peer_node;
		stream_ext_params->host1x_pdev = drv_ctx->drv_param.host1x_pdev;
		stream_ext_params->pci_client_h = drv_ctx->pci_client_h;
		stream_ext_params->comm_channel_h = drv_ctx->comm_channel_h;
		stream_ext_params->vmap_h = drv_ctx->vmap_h;
		stream_ext_params->edma_h = drv_ctx->edma_h;
		stream_ext_params->ep_id = ep_prop->id;
		stream_ext_params->ep_name = endpoint->name;
		stream_ext_params->drv_mode = drv_ctx->drv_mode;

		/* create nvscic2c-pcie endpoint device.*/
		ret = create_endpoint_device(eps_ctx, endpoint);
		if (ret)
			goto err;
	}

	*endpoints_h = eps_ctx;
	return ret;
err:
	endpoints_release((void **)&eps_ctx);
	return ret;
}

/* exit point for nvscic2c-pcie endpoints char device sub-module/abstraction.*/
int
endpoints_release(void **endpoints_h)
{
	u32 i = 0;
	int ret = 0;
	struct endpoint_drv_ctx_t *eps_ctx =
				 (struct endpoint_drv_ctx_t *)(*endpoints_h);
	if (!eps_ctx)
		return ret;

	/* remove all the endpoints char devices.*/
	if (eps_ctx->endpoints) {
		for (i = 0; i < eps_ctx->nr_endpoint; i++) {
			struct endpoint_t *endpoint =
						 &eps_ctx->endpoints[i];
			remove_endpoint_device(eps_ctx, endpoint);
		}
		kfree(eps_ctx->endpoints);
		eps_ctx->endpoints = NULL;
	}

	if (eps_ctx->class) {
		class_destroy(eps_ctx->class);
		eps_ctx->class = NULL;
	}

	if (eps_ctx->char_dev) {
		unregister_chrdev_region(eps_ctx->char_dev,
					 eps_ctx->nr_endpoint);
		eps_ctx->char_dev = 0;
	}

	kfree(eps_ctx);
	*endpoints_h = NULL;

	return ret;
}

int
endpoints_core_deinit(void *endpoints_h)
{
	u32 i = 0;
	int ret = 0;
	struct endpoint_drv_ctx_t *eps_ctx =
				 (struct endpoint_drv_ctx_t *)endpoints_h;
	if (!eps_ctx)
		return ret;

	if (eps_ctx->endpoints) {
		for (i = 0; i < eps_ctx->nr_endpoint; i++) {
			struct endpoint_t *endpoint =
						 &eps_ctx->endpoints[i];

			mutex_lock(&endpoint->fops_lock);
			stream_extension_edma_deinit(endpoint->stream_ext_h);
			mutex_unlock(&endpoint->fops_lock);
			(void)wait_event_interruptible_timeout(endpoint->ack_waitq,
							       !endpoint->link_status_ack_frm_usr,
							       msecs_to_jiffies(
							       PCIE_STATUS_CHANGE_ACK_TIMEOUT));

			pci_client_unregister_for_link_event(endpoint->pci_client_h,
							     endpoint->linkevent_id);
		}
	}

	return ret;
}
