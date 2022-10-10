/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h> /* printk() */
#include <linux/pm.h>
#include <linux/slab.h>   /* kmalloc() */
#include <linux/fs.h>   /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/hdreg.h> /* HDIO_GETGEO */
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm-generic/bug.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <linux/version.h>
#include "tegra_vblk.h"

static int vblk_major;

/**
 * vblk_get_req: Get a handle to free vsc request.
 */
static struct vsc_request *vblk_get_req(struct vblk_dev *vblkdev)
{
	struct vsc_request *req = NULL;
	unsigned long bit;

	mutex_lock(&vblkdev->req_lock);

	if (vblkdev->queue_state != VBLK_QUEUE_ACTIVE)
		goto exit;

	bit = find_first_zero_bit(vblkdev->pending_reqs, vblkdev->max_requests);
	if (bit < vblkdev->max_requests) {
		req = &vblkdev->reqs[bit];
		req->vs_req.req_id = bit;
		set_bit(bit, vblkdev->pending_reqs);
		vblkdev->inflight_reqs++;
	}

exit:
	mutex_unlock(&vblkdev->req_lock);
	return req;
}

static struct vsc_request *vblk_get_req_by_sr_num(struct vblk_dev *vblkdev,
		uint32_t num)
{
	struct vsc_request *req;

	if (num >= vblkdev->max_requests)
		return NULL;

	mutex_lock(&vblkdev->req_lock);
	req = &vblkdev->reqs[num];
	if (test_bit(req->id, vblkdev->pending_reqs) == 0) {
		dev_err(vblkdev->device,
			"sr_num: Request index %d is not active!\n",
			req->id);
		req = NULL;
	}
	mutex_unlock(&vblkdev->req_lock);

	/* Assuming serial number is same as index into request array */
	return req;
}

/**
 * vblk_put_req: Free an active vsc request.
 */
static void vblk_put_req(struct vsc_request *req)
{
	struct vblk_dev *vblkdev;

	vblkdev = req->vblkdev;
	if (vblkdev == NULL) {
		pr_err("Request %d does not have valid vblkdev!\n",
				req->id);
		return;
	}

	if (req->id >= vblkdev->max_requests) {
		dev_err(vblkdev->device, "Request Index %d out of range!\n",
				req->id);
		return;
	}

	mutex_lock(&vblkdev->req_lock);
	if (req != &vblkdev->reqs[req->id]) {
		dev_err(vblkdev->device,
			"Request Index %d does not match with the request!\n",
				req->id);
		goto exit;
	}

	if (test_bit(req->id, vblkdev->pending_reqs) == 0) {
		dev_err(vblkdev->device,
			"Request index %d is not active!\n",
			req->id);
	} else {
		clear_bit(req->id, vblkdev->pending_reqs);
		memset(&req->vs_req, 0, sizeof(struct vs_request));
		req->req = NULL;
		memset(&req->iter, 0, sizeof(struct req_iterator));
		vblkdev->inflight_reqs--;

		if ((vblkdev->inflight_reqs == 0) &&
			(vblkdev->queue_state == VBLK_QUEUE_SUSPENDED)) {
			complete(&vblkdev->req_queue_empty);
		}
	}
exit:
	mutex_unlock(&vblkdev->req_lock);
}

static int vblk_send_config_cmd(struct vblk_dev *vblkdev)
{
	struct vs_request *vs_req;
	int i = 0;

	/* This while loop exits as long as the remote endpoint cooperates. */
	if (tegra_hv_ivc_channel_notified(vblkdev->ivck) != 0) {
		pr_notice("vblk: send_config wait for ivc channel reset\n");
		while (tegra_hv_ivc_channel_notified(vblkdev->ivck) != 0) {
			if (i++ > IVC_RESET_RETRIES) {
				dev_err(vblkdev->device, "ivc reset timeout\n");
				return -EIO;
			}
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(usecs_to_jiffies(1));
		}
	}
	vs_req = (struct vs_request *)
		tegra_hv_ivc_write_get_next_frame(vblkdev->ivck);
	if (IS_ERR_OR_NULL(vs_req)) {
		dev_err(vblkdev->device, "no empty frame for write\n");
		return -EIO;
	}

	vs_req->type = VS_CONFIGINFO_REQ;

	dev_info(vblkdev->device, "send config cmd to ivc #%d\n",
		vblkdev->ivc_id);

	if (tegra_hv_ivc_write_advance(vblkdev->ivck)) {
		dev_err(vblkdev->device, "ivc write failed\n");
		return -EIO;
	}

	return 0;
}

static int vblk_get_configinfo(struct vblk_dev *vblkdev)
{
	struct vs_request *req;
	int32_t status;

	dev_info(vblkdev->device, "get config data from ivc #%d\n",
		vblkdev->ivc_id);

	req = (struct vs_request *)
		tegra_hv_ivc_read_get_next_frame(vblkdev->ivck);
	if (IS_ERR_OR_NULL(req)) {
		dev_err(vblkdev->device, "no empty frame for read\n");
		return -EIO;
	}

	status = req->status;
	vblkdev->config = req->config_info;

	if (tegra_hv_ivc_read_advance(vblkdev->ivck)) {
		dev_err(vblkdev->device, "ivc read failed\n");
		return -EIO;
	}

	if (status != 0)
		return -EINVAL;

	if (vblkdev->config.type != VS_BLK_DEV) {
		dev_err(vblkdev->device, "Non Blk dev config not supported!\n");
		return -EINVAL;
	}

	if (vblkdev->config.blk_config.num_blks == 0) {
		dev_err(vblkdev->device, "controller init failed\n");
		return -EINVAL;
	}

	return 0;
}

static void req_error_handler(struct vblk_dev *vblkdev, struct request *breq)
{
	dev_err(vblkdev->device,
		"Error for request pos %llx type %llx size %x\n",
		(blk_rq_pos(breq) * (uint64_t)SECTOR_SIZE),
		(uint64_t)req_op(breq),
		blk_rq_bytes(breq));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	blk_mq_end_request(breq, BLK_STS_IOERR);
#else
	blk_end_request_all(breq, -EIO);
#endif
}



static void handle_non_ioctl_resp(struct vblk_dev *vblkdev,
		struct vsc_request *vsc_req,
		struct vs_blk_response *blk_resp)
{
	struct bio_vec bvec;
	void *buffer;
	size_t size;
	size_t total_size = 0;
	bool invoke_req_err_hand = false;
	struct request *const bio_req = vsc_req->req;
	struct vs_blk_request *const blk_req =
		&(vsc_req->vs_req.blkdev_req.blk_req);

	if (blk_resp->status != 0) {
		invoke_req_err_hand = true;
		goto end;
	}

	if (req_op(bio_req) != REQ_OP_FLUSH) {
		if (blk_req->num_blks !=
		    blk_resp->num_blks) {
			invoke_req_err_hand = true;
			goto end;
		}
	}

	if (req_op(bio_req) == REQ_OP_READ) {
		rq_for_each_segment(bvec, bio_req, vsc_req->iter) {
			size = bvec.bv_len;
			buffer = page_address(bvec.bv_page) +
				bvec.bv_offset;

			if ((total_size + size) >
				(blk_req->num_blks *
				vblkdev->config.blk_config.hardblk_size)) {
				size =
				(blk_req->num_blks *
				vblkdev->config.blk_config.hardblk_size) -
					total_size;
			}

			if (!vblkdev->config.blk_config.use_vm_address) {
				memcpy(buffer,
					vsc_req->mempool_virt +
					total_size,
					size);
			}

			total_size += size;
			if (total_size ==
				(blk_req->num_blks *
				vblkdev->config.blk_config.hardblk_size))
				break;
		}
	}

end:
	if (vblkdev->config.blk_config.use_vm_address) {
		if ((req_op(bio_req) == REQ_OP_READ) ||
			(req_op(bio_req) == REQ_OP_WRITE)) {
			dma_unmap_sg(vblkdev->device,
				vsc_req->sg_lst,
				vsc_req->sg_num_ents,
				DMA_BIDIRECTIONAL);
			devm_kfree(vblkdev->device, vsc_req->sg_lst);
		}
	}

	if (!invoke_req_err_hand) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
			blk_mq_end_request(bio_req, BLK_STS_OK);
#else
			if (blk_end_request(bio_req, 0,
				blk_req->num_blks *
				vblkdev->config.blk_config.hardblk_size)) {
				dev_err(vblkdev->device,
					"Error completing fs request!\n");
			}
#endif
	} else {

		req_error_handler(vblkdev, bio_req);
	}
}

/**
 * complete_bio_req: Complete a bio request after server is
 *		done processing the request.
 */

static bool complete_bio_req(struct vblk_dev *vblkdev)
{
	int status = 0;
	struct vsc_request *vsc_req = NULL;
	struct vs_request *vs_req;
	struct vs_request req_resp;
	struct request *bio_req;

	/* First check if ivc read queue is empty */
	if (!tegra_hv_ivc_can_read(vblkdev->ivck))
		goto no_valid_io;

	/* Copy the data and advance to next frame */
	if ((tegra_hv_ivc_read(vblkdev->ivck, &req_resp,
				sizeof(struct vs_request)) <= 0)) {
		dev_err(vblkdev->device,
				"Couldn't increment read frame pointer!\n");
		goto no_valid_io;
	}

	status = req_resp.status;
	if (status != 0) {
		dev_err(vblkdev->device, "IO request error = %d\n",
				status);
	}

	vsc_req = vblk_get_req_by_sr_num(vblkdev, req_resp.req_id);
	if (vsc_req == NULL) {
		dev_err(vblkdev->device, "serial_number mismatch num %d!\n",
				req_resp.req_id);
		goto complete_bio_exit;
	}

	bio_req = vsc_req->req;
	vs_req = &vsc_req->vs_req;

	if ((bio_req != NULL) && (status == 0)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		if (req_op(bio_req) == REQ_OP_DRV_IN) {
#else
		if (bio_req->cmd_type == REQ_TYPE_DRV_PRIV) {
#endif
			vblk_complete_ioctl_req(vblkdev, vsc_req,
					req_resp.blkdev_resp.
					ioctl_resp.status);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
				blk_mq_end_request(bio_req, BLK_STS_OK);
#else
				if (blk_end_request(bio_req, 0, 0)) {
					dev_err(vblkdev->device,
						"Error completing private request!\n");
				}
#endif
		}  else {
			handle_non_ioctl_resp(vblkdev, vsc_req,
				&(req_resp.blkdev_resp.blk_resp));
		}

	} else if ((bio_req != NULL) && (status != 0)) {
		req_error_handler(vblkdev, bio_req);
	} else {
		dev_err(vblkdev->device,
			"VSC request %d has null bio request!\n",
			vsc_req->id);
	}

	vblk_put_req(vsc_req);

complete_bio_exit:
	return true;

no_valid_io:
	return false;
}

static bool bio_req_sanity_check(struct vblk_dev *vblkdev,
		struct request *bio_req,
		struct vsc_request *vsc_req)
{
	uint64_t start_offset = (blk_rq_pos(bio_req) * (uint64_t)SECTOR_SIZE);
	uint64_t req_bytes = blk_rq_bytes(bio_req);


	if ((start_offset >= vblkdev->size) || (req_bytes > vblkdev->size) ||
		((start_offset + req_bytes) > vblkdev->size))
	{
		dev_err(vblkdev->device,
			"Invalid I/O limit start 0x%llx size 0x%llx > 0x%llx\n",
			start_offset,
			req_bytes, vblkdev->size);
		return false;
	}

	if ((start_offset % vblkdev->config.blk_config.hardblk_size) != 0) {
		dev_err(vblkdev->device, "Unaligned block offset (%lld %d)\n",
			start_offset, vblkdev->config.blk_config.hardblk_size);
		return false;
	}

	if ((req_bytes % vblkdev->config.blk_config.hardblk_size) != 0) {
		dev_err(vblkdev->device, "Unaligned io length (%lld %d)\n",
			req_bytes, vblkdev->config.blk_config.hardblk_size);
		return false;
	}

	if (req_bytes > (uint64_t)vsc_req->mempool_len) {
		dev_err(vblkdev->device, "Req bytes %llx greater than %x!\n",
			req_bytes, vsc_req->mempool_len);
		return false;
	}

	return true;
}

/**
 * submit_bio_req: Fetch a bio request and submit it to
 * server for processing.
 */
static bool submit_bio_req(struct vblk_dev *vblkdev)
{
	struct vsc_request *vsc_req = NULL;
	struct request *bio_req = NULL;
	struct vs_request *vs_req;
	struct bio_vec bvec;
	size_t size;
	size_t total_size = 0;
	void *buffer;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	struct req_entry *entry = NULL;
#endif
	size_t sz;
	uint32_t sg_cnt;
	dma_addr_t  sg_dma_addr = 0;

	/* Check if ivc queue is full */
	if (!tegra_hv_ivc_can_write(vblkdev->ivck))
		goto bio_exit;

	if (vblkdev->queue == NULL)
		goto bio_exit;

	vsc_req = vblk_get_req(vblkdev);
	if (vsc_req == NULL)
		goto bio_exit;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	spin_lock(&vblkdev->queue_lock);
	if(!list_empty(&vblkdev->req_list)) {
		entry = list_first_entry(&vblkdev->req_list, struct req_entry,
						list_entry);
		list_del(&entry->list_entry);
		bio_req = entry->req;
		kfree(entry);
	}
	spin_unlock(&vblkdev->queue_lock);
#else
	spin_lock(vblkdev->queue->queue_lock);
	bio_req = blk_fetch_request(vblkdev->queue);
	spin_unlock(vblkdev->queue->queue_lock);
#endif

	if (bio_req == NULL)
		goto bio_exit;

	if ((vblkdev->config.blk_config.use_vm_address) &&
		((req_op(bio_req) == REQ_OP_READ) ||
		(req_op(bio_req) == REQ_OP_WRITE))) {
		sz = (sizeof(struct scatterlist)
			* bio_req->nr_phys_segments);
		vsc_req->sg_lst =  devm_kzalloc(vblkdev->device, sz,
					GFP_KERNEL);
		if (vsc_req->sg_lst == NULL) {
			dev_err(vblkdev->device,
				"SG mem allocation failed\n");
			goto bio_exit;
		}
		sg_init_table(vsc_req->sg_lst,
			bio_req->nr_phys_segments);
		sg_cnt = blk_rq_map_sg(vblkdev->queue, bio_req,
				vsc_req->sg_lst);
		vsc_req->sg_num_ents = sg_nents(vsc_req->sg_lst);
		if (dma_map_sg(vblkdev->device, vsc_req->sg_lst,
			vsc_req->sg_num_ents, DMA_BIDIRECTIONAL) == 0) {
			dev_err(vblkdev->device, "dma_map_sg failed\n");
			goto bio_exit;
		}
		sg_dma_addr = sg_dma_address(vsc_req->sg_lst);
	}

	vsc_req->req = bio_req;
	vs_req = &vsc_req->vs_req;

	vs_req->type = VS_DATA_REQ;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	if (req_op(bio_req) != REQ_OP_DRV_IN) {
#else
	if (bio_req->cmd_type == REQ_TYPE_FS) {
#endif
		if (req_op(bio_req) == REQ_OP_READ) {
			vs_req->blkdev_req.req_op = VS_BLK_READ;
		} else if (req_op(bio_req) == REQ_OP_WRITE) {
			vs_req->blkdev_req.req_op = VS_BLK_WRITE;
		} else if (req_op(bio_req) == REQ_OP_FLUSH) {
			vs_req->blkdev_req.req_op = VS_BLK_FLUSH;
		} else if (req_op(bio_req) == REQ_OP_DISCARD) {
			vs_req->blkdev_req.req_op = VS_BLK_DISCARD;
		} else if (req_op(bio_req) == REQ_OP_SECURE_ERASE) {
			vs_req->blkdev_req.req_op = VS_BLK_SECURE_ERASE;
		} else {
			dev_err(vblkdev->device,
				"Request direction is not read/write!\n");
			goto bio_exit;
		}

		vsc_req->iter.bio = NULL;
		if (req_op(bio_req) == REQ_OP_FLUSH) {
			vs_req->blkdev_req.blk_req.blk_offset = 0;
			vs_req->blkdev_req.blk_req.num_blks =
				vblkdev->config.blk_config.num_blks;
		} else {
			if (!bio_req_sanity_check(vblkdev, bio_req, vsc_req)) {
				goto bio_exit;
			}

			vs_req->blkdev_req.blk_req.blk_offset = ((blk_rq_pos(bio_req) *
				(uint64_t)SECTOR_SIZE)
				/ vblkdev->config.blk_config.hardblk_size);
			vs_req->blkdev_req.blk_req.num_blks = ((blk_rq_sectors(bio_req) *
				SECTOR_SIZE) /
				vblkdev->config.blk_config.hardblk_size);

			if (!vblkdev->config.blk_config.use_vm_address) {
				vs_req->blkdev_req.blk_req.data_offset =
							vsc_req->mempool_offset;
			} else {
				vs_req->blkdev_req.blk_req.data_offset = 0;
				/* Provide IOVA  as part of request */
				vs_req->blkdev_req.blk_req.iova_addr =
							(uint64_t)sg_dma_addr;
			}
		}

		if (req_op(bio_req) == REQ_OP_WRITE) {
			rq_for_each_segment(bvec, bio_req, vsc_req->iter) {
				size = bvec.bv_len;
				buffer = page_address(bvec.bv_page) +
						bvec.bv_offset;

				if ((total_size + size) >
					(vs_req->blkdev_req.blk_req.num_blks *
					vblkdev->config.blk_config.hardblk_size))
				{
					size = (vs_req->blkdev_req.blk_req.num_blks *
						vblkdev->config.blk_config.hardblk_size) -
						total_size;
				}

				/* memcpy to mempool not needed as VM IOVA is
				 * provided
				 */
				if (!vblkdev->config.blk_config.use_vm_address) {
					memcpy(
					vsc_req->mempool_virt + total_size,
					buffer, size);
				}

				total_size += size;
				if (total_size == (vs_req->blkdev_req.blk_req.num_blks *
					vblkdev->config.blk_config.hardblk_size)) {
					break;
				}
			}
		}
	} else {
		if (vblk_prep_ioctl_req(vblkdev,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
			(struct vblk_ioctl_req *)bio_req->completion_data,
#else
			(struct vblk_ioctl_req *)bio_req->special,
#endif
			vsc_req)) {
			dev_err(vblkdev->device,
				"Failed to prepare ioctl request!\n");
			goto bio_exit;
		}
	}

	if (!tegra_hv_ivc_write(vblkdev->ivck, vs_req,
				sizeof(struct vs_request))) {
		dev_err(vblkdev->device,
			"Request Id %d IVC write failed!\n",
				vsc_req->id);
		goto bio_exit;
	}

	return true;

bio_exit:
	if (vsc_req != NULL) {
		vblk_put_req(vsc_req);
	}

	if (bio_req != NULL) {
		req_error_handler(vblkdev, bio_req);
		return true;
	}

	return false;
}

static void vblk_request_work(struct work_struct *ws)
{
	struct vblk_dev *vblkdev =
		container_of(ws, struct vblk_dev, work);
	bool req_submitted, req_completed;

	/* Taking ivc lock before performing IVC read/write */
	mutex_lock(&vblkdev->ivc_lock);
	if (tegra_hv_ivc_channel_notified(vblkdev->ivck) != 0) {
		mutex_unlock(&vblkdev->ivc_lock);
		return;
	}

	req_submitted = true;
	req_completed = true;
	while (req_submitted || req_completed) {
		req_completed = complete_bio_req(vblkdev);

		req_submitted = submit_bio_req(vblkdev);
	}
	mutex_unlock(&vblkdev->ivc_lock);
}

/* The simple form of the request function. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
static blk_status_t vblk_request(struct blk_mq_hw_ctx *hctx,
			const struct blk_mq_queue_data *bd)
{
	struct req_entry *entry;
	struct request *req = bd->rq;
	struct vblk_dev *vblkdev = hctx->queue->queuedata;

	blk_mq_start_request(req);

	/* malloc for req list entry */
	entry = kmalloc(sizeof(struct req_entry), GFP_ATOMIC);
	if (entry == NULL) {
		dev_err(vblkdev->device, "Failed to allocate memory\n");
		return BLK_STS_IOERR;
	}

	/* Initialise the entry */
	entry->req = req;
	INIT_LIST_HEAD(&entry->list_entry);

	/* Insert the req to list */
	spin_lock(&vblkdev->queue_lock);
	list_add_tail(&entry->list_entry, &vblkdev->req_list);
	spin_unlock(&vblkdev->queue_lock);

	/* Now invoke the queue to handle data inserted in queue */
	queue_work_on(WORK_CPU_UNBOUND, vblkdev->wq, &vblkdev->work);

	return BLK_STS_OK;
}
#else
static void vblk_request(struct request_queue *q)
{
	struct vblk_dev *vblkdev = q->queuedata;

	queue_work_on(WORK_CPU_UNBOUND, vblkdev->wq, &vblkdev->work);
}
#endif

/* Open and release */
static int vblk_open(struct block_device *device, fmode_t mode)
{
	struct vblk_dev *vblkdev = device->bd_disk->private_data;

	spin_lock(&vblkdev->lock);
	if (!vblkdev->users) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
		check_disk_change(device);
#else
		bdev_check_media_change(device);
#endif
	}
	vblkdev->users++;

	spin_unlock(&vblkdev->lock);
	return 0;
}

static void vblk_release(struct gendisk *disk, fmode_t mode)
{
	struct vblk_dev *vblkdev = disk->private_data;

	spin_lock(&vblkdev->lock);

	vblkdev->users--;

	spin_unlock(&vblkdev->lock);
}

static int vblk_getgeo(struct block_device *device, struct hd_geometry *geo)
{
	geo->heads = VS_LOG_HEADS;
	geo->sectors = VS_LOG_SECTS;
	geo->cylinders = get_capacity(device->bd_disk) /
		(geo->heads * geo->sectors);

	return 0;
}

/* The device operations structure. */
static const struct block_device_operations vblk_ops = {
	.owner           = THIS_MODULE,
	.open            = vblk_open,
	.release         = vblk_release,
	.getgeo          = vblk_getgeo,
	.ioctl           = vblk_ioctl
};

static ssize_t
vblk_phys_dev_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct gendisk *disk = dev_to_disk(dev);
	struct vblk_dev *vblk = disk->private_data;

	if (vblk->config.phys_dev == VSC_DEV_EMMC)
		return snprintf(buf, 16, "EMMC\n");
	else if (vblk->config.phys_dev == VSC_DEV_UFS)
		return snprintf(buf, 16, "UFS\n");
	else
		return snprintf(buf, 16, "Unknown\n");
}

static ssize_t
vblk_phys_base_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct gendisk *disk = dev_to_disk(dev);
	struct vblk_dev *vblk = disk->private_data;

	return snprintf(buf, 16, "0x%x\n", vblk->config.phys_base);
}

static ssize_t
vblk_storage_type_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct gendisk *disk = dev_to_disk(dev);
	struct vblk_dev *vblk = disk->private_data;

	switch (vblk->config.storage_type) {
	case VSC_STORAGE_RPMB:
		return snprintf(buf, 16, "RPMB\n");
	case VSC_STORAGE_BOOT:
		return snprintf(buf, 16, "BOOT\n");
	case VSC_STORAGE_LUN0:
		return snprintf(buf, 16, "LUN0\n");
	case VSC_STORAGE_LUN1:
		return snprintf(buf, 16, "LUN1\n");
	case VSC_STORAGE_LUN2:
		return snprintf(buf, 16, "LUN2\n");
	case VSC_STORAGE_LUN3:
		return snprintf(buf, 16, "LUN3\n");
	case VSC_STORAGE_LUN4:
		return snprintf(buf, 16, "LUN4\n");
	case VSC_STORAGE_LUN5:
		return snprintf(buf, 16, "LUN5\n");
	case VSC_STORAGE_LUN6:
		return snprintf(buf, 16, "LUN6\n");
	case VSC_STORAGE_LUN7:
		return snprintf(buf, 16, "LUN7\n");
	default:
		break;
	}

	return snprintf(buf, 16, "Unknown\n");
}

static ssize_t
vblk_speed_mode_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct gendisk *disk = dev_to_disk(dev);
	struct vblk_dev *vblk = disk->private_data;

	return snprintf(buf, 32, "%s\n", vblk->config.speed_mode);
}

static const struct device_attribute dev_attr_phys_dev_ro =
	__ATTR(phys_dev, 0444,
	       vblk_phys_dev_show, NULL);

static const struct device_attribute dev_attr_phys_base_ro =
	__ATTR(phys_base, 0444,
	       vblk_phys_base_show, NULL);

static const struct device_attribute dev_attr_storage_type_ro =
	__ATTR(storage_type, 0444,
	       vblk_storage_type_show, NULL);

static const struct device_attribute dev_attr_speed_mode_ro =
	__ATTR(speed_mode, 0444,
	       vblk_speed_mode_show, NULL);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
static const struct blk_mq_ops vblk_mq_ops = {
	.queue_rq	= vblk_request,
};
#endif
/* Set up virtual device. */
static void setup_device(struct vblk_dev *vblkdev)
{
	uint32_t max_io_bytes;
	uint32_t req_id;
	uint32_t max_requests;
	struct vsc_request *req;

	vblkdev->size =
		vblkdev->config.blk_config.num_blks *
			vblkdev->config.blk_config.hardblk_size;

	spin_lock_init(&vblkdev->lock);
	spin_lock_init(&vblkdev->queue_lock);
	mutex_init(&vblkdev->ioctl_lock);
	mutex_init(&vblkdev->ivc_lock);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	vblkdev->queue = blk_mq_init_sq_queue(&vblkdev->tag_set, &vblk_mq_ops, 16,
						BLK_MQ_F_SHOULD_MERGE);
#else
	vblkdev->queue = blk_init_queue(vblk_request, &vblkdev->queue_lock);
#endif
	if (vblkdev->queue == NULL) {
		dev_err(vblkdev->device, "failed to init blk queue\n");
		return;
	}

	vblkdev->queue->queuedata = vblkdev;

	blk_queue_logical_block_size(vblkdev->queue,
		vblkdev->config.blk_config.hardblk_size);
	blk_queue_physical_block_size(vblkdev->queue,
		vblkdev->config.blk_config.hardblk_size);

	if (vblkdev->config.blk_config.req_ops_supported & VS_BLK_FLUSH_OP_F) {
		blk_queue_write_cache(vblkdev->queue, true, false);
	}

	if (vblkdev->config.blk_config.max_read_blks_per_io !=
		vblkdev->config.blk_config.max_write_blks_per_io) {
		dev_err(vblkdev->device,
			"Different read/write blks not supported!\n");
		return;
	}

	/* Set the maximum number of requests possible using
	 * server returned information */
	max_io_bytes = (vblkdev->config.blk_config.hardblk_size *
			vblkdev->config.blk_config.max_read_blks_per_io);
	if (max_io_bytes == 0) {
		dev_err(vblkdev->device, "Maximum io bytes value is 0!\n");
		return;
	}

	max_requests = ((vblkdev->ivmk->size) / max_io_bytes);

	if (max_requests < MAX_VSC_REQS) {
		/* Warn if the virtual storage device supports
		 * normal read write operations */
		if (vblkdev->config.blk_config.req_ops_supported &
				(VS_BLK_READ_OP_F |
				 VS_BLK_WRITE_OP_F)) {
			dev_warn(vblkdev->device,
				"Setting Max requests to %d, consider "
				"increasing mempool size !\n",
				max_requests);
		}
	} else if (max_requests > MAX_VSC_REQS) {
		max_requests = MAX_VSC_REQS;
		dev_warn(vblkdev->device,
			"Reducing the max requests to %d, consider"
			" supporting more requests for the vblkdev!\n",
			MAX_VSC_REQS);
	}

	/* if the number of ivc frames is lesser than th  maximum requests that
	 * can be supported(calculated based on mempool size above), treat this
	 * as critical error and panic.
	 *
	 *if (num_of_ivc_frames < max_supported_requests)
	 *   PANIC
	 * Ideally, these 2 should be equal for below reasons
	 *   1. Each ivc frame is a request should have a backing data memory
	 *      for transfers. So, number of requests supported by message
	 *      request memory should be <= number of frames in
	 *      IVC queue. The read/write logic depends on this.
	 *   2. If number of requests supported by message request memory is
	 *	more than IVC frame count, then thats a wastage of memory space
	 *      and it introduces a race condition in submit_bio_req().
	 *      The race condition happens when there is only one empty slot in
	 *      IVC write queue and 2 threads enter submit_bio_req(). Both will
	 *      compete for IVC write(After calling ivc_can_write) and one of
	 *      the write will fail. But with vblk_get_req() this race can be
	 *      avoided if num_of_ivc_frames >= max_supported_requests
	 *      holds true.
	 *
	 *  In short, the optimal setting is when both of these are equal
	 */
	if (vblkdev->ivck->nframes < max_requests) {
		/* Error if the virtual storage device supports
		 * read, write and ioctl operations
		 */
		if (vblkdev->config.blk_config.req_ops_supported &
				(VS_BLK_READ_OP_F |
				 VS_BLK_WRITE_OP_F |
				 VS_BLK_IOCTL_OP_F)) {
			panic("hv_vblk: IVC Channel:%u IVC frames %d less than possible max requests %d!\n",
				vblkdev->ivc_id, vblkdev->ivck->nframes,
				max_requests);
			return;
		}
	}

	for (req_id = 0; req_id < max_requests; req_id++){
		req = &vblkdev->reqs[req_id];
		req->mempool_virt = (void *)((uintptr_t)vblkdev->shared_buffer +
			(uintptr_t)(req_id * max_io_bytes));
		req->mempool_offset = (req_id * max_io_bytes);
		req->mempool_len = max_io_bytes;
		req->id = req_id;
		req->vblkdev = vblkdev;
	}

	if (max_requests == 0) {
		dev_err(vblkdev->device,
			"maximum requests set to 0!\n");
		return;
	}
	mutex_init(&vblkdev->req_lock);

	vblkdev->max_requests = max_requests;
	blk_queue_max_hw_sectors(vblkdev->queue, max_io_bytes / SECTOR_SIZE);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	blk_queue_flag_set(QUEUE_FLAG_NONROT, vblkdev->queue);
#else
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, vblkdev->queue);
#endif

	if (vblkdev->config.blk_config.req_ops_supported
		& VS_BLK_DISCARD_OP_F) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
		blk_queue_flag_set(QUEUE_FLAG_DISCARD, vblkdev->queue);
#else
		queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, vblkdev->queue);
#endif
		blk_queue_max_discard_sectors(vblkdev->queue,
			vblkdev->config.blk_config.max_erase_blks_per_io);
		vblkdev->queue->limits.discard_granularity =
			vblkdev->config.blk_config.hardblk_size;
		if (vblkdev->config.blk_config.req_ops_supported &
			VS_BLK_SECURE_ERASE_OP_F)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
			blk_queue_flag_set(QUEUE_FLAG_SECERASE, vblkdev->queue);
#else
			queue_flag_set_unlocked(QUEUE_FLAG_SECERASE, vblkdev->queue);
#endif
	}

	/* And the gendisk structure. */
	vblkdev->gd = alloc_disk(VBLK_MINORS);
	if (!vblkdev->gd) {
		dev_err(vblkdev->device, "alloc_disk failure\n");
		return;
	}
	vblkdev->gd->major = vblk_major;
	vblkdev->gd->first_minor = vblkdev->devnum * VBLK_MINORS;
	vblkdev->gd->fops = &vblk_ops;
	vblkdev->gd->queue = vblkdev->queue;
	vblkdev->gd->private_data = vblkdev;
	vblkdev->gd->flags |= GENHD_FL_EXT_DEVT;

	/* Don't allow scanning of the device when block
	 * requests are not supported */
	if (!(vblkdev->config.blk_config.req_ops_supported &
				VS_BLK_READ_OP_F)) {
		vblkdev->gd->flags |= GENHD_FL_NO_PART_SCAN;
	}

	/* Set disk read-only if config response say so */
	if (!(vblkdev->config.blk_config.req_ops_supported &
				VS_BLK_READ_ONLY_MASK)) {
		dev_info(vblkdev->device, "setting device read-only\n");
		set_disk_ro(vblkdev->gd, 1);
	}

	if (vblkdev->config.storage_type == VSC_STORAGE_RPMB) {
		if (snprintf(vblkdev->gd->disk_name, 32, "vblkrpmb%d",
				vblkdev->devnum) < 0) {
			dev_err(vblkdev->device, "Error while updating disk_name!\n");
			return;
		}
	} else {
		if (snprintf(vblkdev->gd->disk_name, 32, "vblkdev%d",
				vblkdev->devnum) < 0) {
			dev_err(vblkdev->device, "Error while updating disk_name!\n");
			return;
		}
	}

	set_capacity(vblkdev->gd, (vblkdev->size / SECTOR_SIZE));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	device_add_disk(vblkdev->device, vblkdev->gd, NULL);
#else
	device_add_disk(vblkdev->device, vblkdev->gd);
#endif

	if (device_create_file(disk_to_dev(vblkdev->gd),
		&dev_attr_phys_dev_ro)) {
		dev_warn(vblkdev->device, "Error adding phys dev file!\n");
		return;
	}

	if (device_create_file(disk_to_dev(vblkdev->gd),
		&dev_attr_phys_base_ro)) {
		dev_warn(vblkdev->device, "Error adding phys base file!\n");
		return;
	}

	if (device_create_file(disk_to_dev(vblkdev->gd),
		&dev_attr_storage_type_ro)) {
		dev_warn(vblkdev->device, "Error adding storage type file!\n");
		return;
	}

	if (device_create_file(disk_to_dev(vblkdev->gd),
		&dev_attr_speed_mode_ro)) {
		dev_warn(vblkdev->device, "Error adding speed_mode file!\n");
		return;
	}
}

static void vblk_init_device(struct work_struct *ws)
{
	struct vblk_dev *vblkdev = container_of(ws, struct vblk_dev, init);

	/* wait for ivc channel reset to finish */
	if (tegra_hv_ivc_channel_notified(vblkdev->ivck) != 0)
		return;	/* this will be rescheduled by irq handler */

	if (tegra_hv_ivc_can_read(vblkdev->ivck) && !vblkdev->initialized) {
		if (vblk_get_configinfo(vblkdev))
			return;

		vblkdev->initialized = true;
		setup_device(vblkdev);
	}
}

static irqreturn_t ivc_irq_handler(int irq, void *data)
{
	struct vblk_dev *vblkdev = (struct vblk_dev *)data;

	if (vblkdev->initialized)
		queue_work_on(WORK_CPU_UNBOUND, vblkdev->wq, &vblkdev->work);
	else
		schedule_work(&vblkdev->init);

	return IRQ_HANDLED;
}

static int tegra_hv_vblk_probe(struct platform_device *pdev)
{
	static struct device_node *vblk_node;
	struct vblk_dev *vblkdev;
	struct device *dev = &pdev->dev;
	int ret;
	struct tegra_hv_ivm_cookie *ivmk;

	if (!is_tegra_hypervisor_mode()) {
		dev_err(dev, "Hypervisor is not present\n");
		return -ENODEV;
	}

	if (vblk_major == 0) {
		dev_err(dev, "major number is invalid\n");
		return -ENODEV;
	}

	vblk_node = dev->of_node;
	if (vblk_node == NULL) {
		dev_err(dev, "No of_node data\n");
		return -ENODEV;
	}

	dev_info(dev, "allocate drvdata buffer\n");
	vblkdev = devm_kzalloc(dev, sizeof(struct vblk_dev), GFP_KERNEL);
	if (vblkdev == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, vblkdev);
	vblkdev->device = dev;

	/* Get properties of instance and ivc channel id */
	if (of_property_read_u32(vblk_node, "instance", &(vblkdev->devnum))) {
		dev_err(dev, "Failed to read instance property\n");
		ret = -ENODEV;
		goto fail;
	} else {
		if (of_property_read_u32_index(vblk_node, "ivc", 1,
			&(vblkdev->ivc_id))) {
			dev_err(dev, "Failed to read ivc property\n");
			ret = -ENODEV;
			goto fail;
		}
		if (of_property_read_u32_index(vblk_node, "mempool", 0,
			&(vblkdev->ivm_id))) {
			dev_err(dev, "Failed to read mempool property\n");
			ret = -ENODEV;
			goto fail;
		}
	}

	vblkdev->ivck = tegra_hv_ivc_reserve(NULL, vblkdev->ivc_id, NULL);
	if (IS_ERR_OR_NULL(vblkdev->ivck)) {
		dev_err(dev, "Failed to reserve IVC channel %d\n",
			vblkdev->ivc_id);
		vblkdev->ivck = NULL;
		ret = -ENODEV;
		goto fail;
	}

	ivmk = tegra_hv_mempool_reserve(vblkdev->ivm_id);
	if (IS_ERR_OR_NULL(ivmk)) {
		dev_err(dev, "Failed to reserve IVM channel %d\n",
			vblkdev->ivm_id);
		ivmk = NULL;
		ret = -ENODEV;
		goto free_ivc;
	}
	vblkdev->ivmk = ivmk;

	vblkdev->shared_buffer = devm_memremap(vblkdev->device,
			ivmk->ipa, ivmk->size, MEMREMAP_WB);
	if (IS_ERR_OR_NULL(vblkdev->shared_buffer)) {
		dev_err(dev, "Failed to map mempool area %d\n",
				vblkdev->ivm_id);
		ret = -ENOMEM;
		goto free_mempool;
	}

	vblkdev->initialized = false;

	vblkdev->wq = alloc_workqueue("vblk_req_wq%d",
		WQ_UNBOUND | WQ_MEM_RECLAIM,
		1, vblkdev->devnum);
	if (vblkdev->wq == NULL) {
		dev_err(dev, "Failed to allocate workqueue\n");
		ret = -ENOMEM;
		goto free_mempool;
	}

	init_completion(&vblkdev->req_queue_empty);
	vblkdev->queue_state = VBLK_QUEUE_ACTIVE;

	INIT_WORK(&vblkdev->init, vblk_init_device);
	INIT_WORK(&vblkdev->work, vblk_request_work);
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 15, 0)
	/* creating and initializing the an internal request list */
	INIT_LIST_HEAD(&vblkdev->req_list);
#endif

	if (devm_request_irq(vblkdev->device, vblkdev->ivck->irq,
		ivc_irq_handler, 0, "vblk", vblkdev)) {
		dev_err(dev, "Failed to request irq %d\n", vblkdev->ivck->irq);
		ret = -EINVAL;
		goto free_wq;
	}

	tegra_hv_ivc_channel_reset(vblkdev->ivck);
	if (vblk_send_config_cmd(vblkdev)) {
		dev_err(dev, "Failed to send config cmd\n");
		ret = -EACCES;
		goto free_wq;
	}

	return 0;

free_wq:
	destroy_workqueue(vblkdev->wq);

free_mempool:
	tegra_hv_mempool_unreserve(vblkdev->ivmk);

free_ivc:
	tegra_hv_ivc_unreserve(vblkdev->ivck);

fail:
	return ret;
}

static int tegra_hv_vblk_remove(struct platform_device *pdev)
{
	struct vblk_dev *vblkdev = platform_get_drvdata(pdev);

	if (vblkdev->gd) {
		del_gendisk(vblkdev->gd);
		put_disk(vblkdev->gd);
	}

	if (vblkdev->queue)
		blk_cleanup_queue(vblkdev->queue);

	destroy_workqueue(vblkdev->wq);
	tegra_hv_ivc_unreserve(vblkdev->ivck);
	tegra_hv_mempool_unreserve(vblkdev->ivmk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_hv_vblk_suspend(struct device *dev)
{
	struct vblk_dev *vblkdev = dev_get_drvdata(dev);
	unsigned long flags;

	if (vblkdev->queue) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
		spin_lock_irqsave(vblkdev->queue->queue_lock, flags);
		blk_stop_queue(vblkdev->queue);
		spin_unlock_irqrestore(vblkdev->queue->queue_lock, flags);
#else
		spin_lock_irqsave(&vblkdev->queue->queue_lock, flags);
		blk_mq_stop_hw_queues(vblkdev->queue);
		spin_unlock_irqrestore(&vblkdev->queue->queue_lock, flags);
#endif

		mutex_lock(&vblkdev->req_lock);
		vblkdev->queue_state = VBLK_QUEUE_SUSPENDED;

		/* Mark the queue as empty if inflight requests are 0 */
		if (vblkdev->inflight_reqs == 0)
			complete(&vblkdev->req_queue_empty);
		mutex_unlock(&vblkdev->req_lock);

		wait_for_completion(&vblkdev->req_queue_empty);
		disable_irq(vblkdev->ivck->irq);

		flush_workqueue(vblkdev->wq);

		/* Reset the channel */
		mutex_lock(&vblkdev->ivc_lock);
		tegra_hv_ivc_channel_reset(vblkdev->ivck);
		mutex_unlock(&vblkdev->ivc_lock);
	}

	return 0;
}

static int tegra_hv_vblk_resume(struct device *dev)
{
	struct vblk_dev *vblkdev = dev_get_drvdata(dev);
	unsigned long flags;

	if (vblkdev->queue) {
		mutex_lock(&vblkdev->req_lock);
		vblkdev->queue_state = VBLK_QUEUE_ACTIVE;
		reinit_completion(&vblkdev->req_queue_empty);
		mutex_unlock(&vblkdev->req_lock);

		enable_irq(vblkdev->ivck->irq);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
		spin_lock_irqsave(vblkdev->queue->queue_lock, flags);
		blk_start_queue(vblkdev->queue);
		spin_unlock_irqrestore(vblkdev->queue->queue_lock, flags);
#else
		spin_lock_irqsave(&vblkdev->queue->queue_lock, flags);
		blk_mq_start_hw_queues(vblkdev->queue);
		spin_unlock_irqrestore(&vblkdev->queue->queue_lock, flags);
#endif

		queue_work_on(WORK_CPU_UNBOUND, vblkdev->wq, &vblkdev->work);
	}

	return 0;
}

static const struct dev_pm_ops tegra_hv_vblk_pm_ops = {
	.suspend = tegra_hv_vblk_suspend,
	.resume = tegra_hv_vblk_resume,
};
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_OF
static struct of_device_id tegra_hv_vblk_match[] = {
	{ .compatible = "nvidia,tegra-hv-storage", },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_hv_vblk_match);
#endif /* CONFIG_OF */

static struct platform_driver tegra_hv_vblk_driver = {
	.probe	= tegra_hv_vblk_probe,
	.remove	= tegra_hv_vblk_remove,
	.driver	= {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_hv_vblk_match),
#ifdef CONFIG_PM_SLEEP
		.pm = &tegra_hv_vblk_pm_ops,
#endif
	},
};

static int __init tegra_hv_vblk_driver_init(void)
{
	vblk_major = 0;
	vblk_major = register_blkdev(vblk_major, "vblk");
	if (vblk_major <= 0) {
		pr_err("vblk: unable to get major number\n");
		return -ENODEV;
	}

	return platform_driver_register(&tegra_hv_vblk_driver);
}
module_init(tegra_hv_vblk_driver_init);

static void __exit tegra_hv_vblk_driver_exit(void)
{
	unregister_blkdev(vblk_major, "vblk");
	platform_driver_unregister(&tegra_hv_vblk_driver);
}
module_exit(tegra_hv_vblk_driver_exit);

MODULE_AUTHOR("Dilan Lee <dilee@nvidia.com>");
MODULE_DESCRIPTION("Virtual storage device over Tegra Hypervisor IVC channel");
MODULE_LICENSE("GPL");

