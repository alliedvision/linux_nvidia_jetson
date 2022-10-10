// SPDX-License-Identifier: GPL-2.0
/*
 * Tegra Virtual Block I/O Driver for OOPS partition
 *
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <soc/tegra/fuse.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm-generic/bug.h>
#include <linux/version.h>
#include <linux/kmsg_dump.h>
#include <linux/pstore_zone.h>
#include "tegra_vblk_oops.h"

static struct vblk_dev *vblkdev_oops;
static struct pstore_zone_info pstore_zone;

#define POPULATE_BLK_REQ(x, req_type, req_opr, opr_offset, num_of_blk, opr_data_offset) \
do { \
	x.type = req_type;\
	x.blkdev_req.req_op = req_opr; \
	x.blkdev_req.blk_req.blk_offset = opr_offset; \
	x.blkdev_req.blk_req.num_blks = num_of_blk; \
	x.blkdev_req.blk_req.data_offset = opr_data_offset; \
} while (0)

static ssize_t vblk_oops_read(char *buf, size_t bytes, loff_t pos)
{
	struct vsc_request *vsc_req;
	struct vs_request req_in;
	struct vs_request req_out;
	uint32_t blocks, block_pos;
	uint32_t block_size = vblkdev_oops->config.blk_config.hardblk_size;
	int32_t retry;

	dev_dbg(vblkdev_oops->device, "%s> pos:%lld, bytes:%lu\n", __func__,
		pos, bytes);

	/*
	 * We expect to be invoked in non-atomic context for read, but let's
	 * make sure that's always the case.
	 */
	if (in_atomic()) {
		dev_warn(vblkdev_oops->device,
			"%s invoked in atomic context..aborting\n", __func__);
		return -EBUSY;
	}

	/*
	 * Read is always from the start of record which is block aligned, but
	 * let's check just to be sure.
	 */
	if (pos & (block_size - 1)) {
		dev_warn(vblkdev_oops->device, "Unaligned start address\n");
		return -ENOMSG;
	}

	mutex_lock(&vblkdev_oops->ivc_lock);
	vsc_req = &vblkdev_oops->reqs[VSC_REQ_RW];

	block_pos = pos/block_size;
	blocks = bytes/block_size;

	/*
	 * For non-block aligned read requests, we can read full block(s) and
	 * return requested bytes.
	 */
	if (bytes & (block_size - 1))
		blocks += 1;

	POPULATE_BLK_REQ(req_in, VS_DATA_REQ, VS_BLK_READ, block_pos, blocks,
			vsc_req->mempool_offset);

	if (!tegra_hv_ivc_write(vblkdev_oops->ivck, &req_in,
				sizeof(struct vs_request))) {
		dev_err(vblkdev_oops->device,
			"%s: IVC write failed!\n", __func__);
		goto fail;
	}

	retry = VSC_RESPONSE_RETRIES;
	while (!tegra_hv_ivc_can_read(vblkdev_oops->ivck) && (retry--)) {
		dev_dbg(vblkdev_oops->device, "Waiting for IVC response\n");
		msleep(VSC_RESPONSE_WAIT_MS);
	}
	if (retry == (-1)) {
		dev_err(vblkdev_oops->device,
			"%s: No response from virtual storage!\n", __func__);
		goto fail;
	}

	/* Copy the data and advance to next frame */
	if ((tegra_hv_ivc_read(vblkdev_oops->ivck, &req_out,
				sizeof(struct vs_request)) <= 0)) {
		dev_err(vblkdev_oops->device,
				"%s: IVC read failed!\n", __func__);
		goto fail;
	}

	if (req_out.status != 0) {
		dev_err(vblkdev_oops->device, "%s: IO request error = %d\n",
				__func__, req_out.status);
	}

	memcpy(buf, vsc_req->mempool_virt, bytes);

	mutex_unlock(&vblkdev_oops->ivc_lock);
	return bytes;

fail:
	mutex_unlock(&vblkdev_oops->ivc_lock);
	return -ENOMSG;
}

static ssize_t vblk_oops_write(const char *buf, size_t bytes,
		loff_t pos)
{
	struct vsc_request *vsc_req;
	struct vs_request req_in;
	struct vs_request req_out;
	uint32_t blocks, block_pos;
	uint32_t block_size = vblkdev_oops->config.blk_config.hardblk_size;
	int32_t retry;

	dev_dbg(vblkdev_oops->device, "%s> pos:%lld, bytes:%lu\n", __func__,
		pos, bytes);

	/*
	 * It is possible for write to be invoked from atomic context.  We
	 * will return EBUSY so pstore_zone will attempt a retry from
	 * workqueue later.
	 */
	if (in_atomic()) {
		dev_warn(vblkdev_oops->device,
			"%s invoked in atomic context..aborting\n", __func__);
		return -EBUSY;
	}

	/*
	 * If write position is misaligned with block size, return EBUSY so
	 * pstore_zone will retry to flush all dirty records (record start
	 * addresses are always block aligned).
	 *
	 * However, this is not expected to happen since pstore always writes
	 * from the start address for record buffer (for KMSG atleast) && we
	 * support only KMSG.
	 */
	if (pos & (block_size - 1)) {
		dev_warn(vblkdev_oops->device, "Unaligned start address\n");
		return -EBUSY;
	}

	if (!bytes)
		return -ENOMSG;

	mutex_lock(&vblkdev_oops->ivc_lock);
	vsc_req = &vblkdev_oops->reqs[VSC_REQ_RW];

	block_pos = pos/block_size;
	blocks = bytes/block_size;

	/*
	 * Only need for unaligned size is when metadata is updated during
	 * pstore erase operation.  It is OK in this case to round up size to
	 * block boundary (corrupting remainder of the block).
	 */
	if (bytes & (block_size - 1))
		blocks += 1;

	POPULATE_BLK_REQ(req_in, VS_DATA_REQ, VS_BLK_WRITE, block_pos, blocks,
			vsc_req->mempool_offset);

	memcpy(vsc_req->mempool_virt, buf, bytes);

	if (!tegra_hv_ivc_write(vblkdev_oops->ivck, &req_in,
				sizeof(struct vs_request))) {
		dev_err(vblkdev_oops->device,
			"%s IVC write failed!\n", __func__);
		goto fail;
	}

	retry = VSC_RESPONSE_RETRIES;
	while (!tegra_hv_ivc_can_read(vblkdev_oops->ivck) && (retry--)) {
		dev_dbg(vblkdev_oops->device, "Waiting for IVC response\n");
		msleep(VSC_RESPONSE_WAIT_MS);
	}
	if (retry == (-1)) {
		dev_err(vblkdev_oops->device,
			"%s: No response from virtual storage!\n", __func__);
		goto fail;
	}

	/* Copy the data and advance to next frame */
	if ((tegra_hv_ivc_read(vblkdev_oops->ivck, &req_out,
				sizeof(struct vs_request)) <= 0)) {
		dev_err(vblkdev_oops->device,
				"%s: IVC read failed!!\n", __func__);
		goto fail;
	}

	if (req_out.status != 0) {
		dev_err(vblkdev_oops->device, "%s: IO request error = %d\n",
				__func__, req_out.status);
	}

	mutex_unlock(&vblkdev_oops->ivc_lock);
	return bytes;

fail:
	mutex_unlock(&vblkdev_oops->ivc_lock);
	return -ENOMSG;
}

/*
 * panic_write is going to mirror what regular write is going to do with some
 * differences:
 * - this is best effort service that can have no assumptions on system state
 * - avoid locks since nobody is executing concurrently .and. system is going
 *   to stop running soon
 * - use VSC_REQ that is reserved for panic
 * - no need to check for VSC response.  Send request and assume it is all good
 *   since the caller is not going to do anything meaningful if we report error
 */
static ssize_t vblk_oops_panic_write(const char *buf, size_t bytes,
		loff_t pos)
{
	struct vsc_request *vsc_req;
	struct vs_request req_in;
	uint32_t blocks, block_pos;
	uint32_t block_size = vblkdev_oops->config.blk_config.hardblk_size;

	dev_dbg(vblkdev_oops->device, "%s> pos:%lld, bytes:%lu\n", __func__,
		pos, bytes);

	/* Not expected to happen for KMSG */
	if (pos & (block_size-1)) {
		dev_warn(vblkdev_oops->device, "Unaligned start address\n");
		return -ENOMSG;
	}

	if (!bytes)
		return -ENOMSG;

	vsc_req = &vblkdev_oops->reqs[VSC_REQ_PANIC];

	block_pos = pos/block_size;
	blocks = bytes/block_size;

	/*
	 * only need for unaligned size is when metadata is updated during
	 * pstore erase operation.  It is OK in this case to round up size to
	 * block boundary.
	 *
	 * For panic_write, however, we expect full records to be written which
	 * means start offset and size are both block aligned.
	 */
	if (bytes & (block_size-1))
		blocks += 1;

	POPULATE_BLK_REQ(req_in, VS_DATA_REQ, VS_BLK_WRITE, block_pos, blocks,
			vsc_req->mempool_offset);

	memcpy(vsc_req->mempool_virt, buf, bytes);

	/*
	 * We are avoiding ivc_lock usage in this path since the assumption is
	 * that in panic flow there is only a single thread/CPU executing which
	 * is currently in vblk_oops_panic_write() and after this, the VM is
	 * going to either reboot or die.  Once we get here,
	 * vblk_oops_read()/vblk_oops_write() is not going to be invoked.
	 *
	 * There is potential for IVC corruption in the event that
	 * vblk_oops_read()/vblk_oops_write() was accessing IVC when panic was
	 * triggered (either as a part of vblk_* flow or outside of it). The
	 * right way avoid corruption would be to use ivc_lock here but we
	 * could potentially deadlock since vblk_oops_read()/vblk_oops_write()
	 * won't be able to run to release the acquired ivc_lock.
	 */
	if (!tegra_hv_ivc_write(vblkdev_oops->ivck, &req_in,
				sizeof(struct vs_request))) {
		dev_err(vblkdev_oops->device,
			"Request IVC write failed!\n");
		return 0;
	}

	/*
	 * VSC will respond at some point but we don't care about the response
	 * since we cannot do anything new to recover/retry (if there is some
	 * error). So we are not going to wait and check the response.
	 *
	 * Also, after panic_write is invoked,the VM is going to stop executing
	 * and the only recovery out of this is a VM (or) tegra reboots.
	 * In both the cases we reset IVC to get it to a clean state.
	 */
	return bytes;
}

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

	mutex_init(&vblkdev->ivc_lock);

	if (vblkdev->config.blk_config.max_read_blks_per_io !=
		vblkdev->config.blk_config.max_write_blks_per_io) {
		dev_err(vblkdev->device,
			"Different read/write blks not supported!\n");
		return;
	}

	/*
	 * Set the maximum number of requests possible using
	 * server returned information
	 */
	max_io_bytes = (vblkdev->config.blk_config.hardblk_size *
			vblkdev->config.blk_config.max_read_blks_per_io);
	if (max_io_bytes == 0) {
		dev_err(vblkdev->device, "Maximum io bytes value is 0!\n");
		return;
	}

	max_requests = ((vblkdev->ivmk->size) / max_io_bytes);

	if (max_requests < MAX_OOPS_VSC_REQS) {
		dev_err(vblkdev->device,
			"Device needs to support %d concurrent requests\n",
			MAX_OOPS_VSC_REQS);
		return;
	} else if (max_requests > MAX_OOPS_VSC_REQS) {
		dev_warn(vblkdev->device,
			"Only %d concurrent requests can be filed, consider reducing mempool size\n",
			MAX_OOPS_VSC_REQS);
		max_requests = MAX_OOPS_VSC_REQS;
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
		panic("hv_vblk: IVC Channel:%u IVC frames %d less than possible max requests %d!\n",
			vblkdev->ivc_id, vblkdev->ivck->nframes,
			max_requests);
		return;
	}

	for (req_id = 0; req_id < max_requests; req_id++) {
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

	vblkdev->max_requests = max_requests;

	if (!(vblkdev->config.blk_config.req_ops_supported &
				VS_BLK_READ_ONLY_MASK)) {
		dev_warn(vblkdev->device, "device partition is read-only ?!\n");
	}

	dev_dbg(vblkdev->device,
		"Size: %lld B, blk_size: %d B, numblocks/IO: %d, maxio: %d B, max_req: %d, phys_dev: %s\n",
		vblkdev->size, vblkdev->config.blk_config.hardblk_size,
		vblkdev->config.blk_config.max_read_blks_per_io, max_io_bytes,
		max_requests, ((vblkdev->config.phys_dev == VSC_DEV_EMMC)?"EMMC":"Other"));

	/*
	 * Ensure that the selected kmsg record size is a multiple of blk_size
	 * and atleast one block size.
	 */
	if ((vblkdev->pstore_kmsg_size < vblkdev->config.blk_config.hardblk_size) ||
		(vblkdev->pstore_kmsg_size & (vblkdev->config.blk_config.hardblk_size - 1))) {
		dev_warn(vblkdev->device,
			"Unsupported pstore_kmsg_size property, assuming %d bytes\n",
			PSTORE_KMSG_RECORD_SIZE);
		vblkdev->pstore_kmsg_size = PSTORE_KMSG_RECORD_SIZE;
	}

	/* Check if the storage is enough for aleast one kmsg record */
	if (vblkdev->pstore_kmsg_size > vblkdev->size) {
		dev_warn(vblkdev->device,
			"pstore_kmsg_size cannot be greater than storage size, reducing to %llu bytes\n",
			vblkdev->size);
		vblkdev->pstore_kmsg_size = vblkdev->size;
	}

	/*
	 * Allow only KMSG (PANIC/OOPS) since pstore_zone doesn't take care of
	 * block restrictions for CONSOLE/FTRACE/PMSG during write (since we
	 * have a block device that we are accessing directly without block layer
	 * support, we cannot handle non-block aligned start offset/size).
	 */
	pstore_zone.name = OOPS_DRV_NAME;
	pstore_zone.total_size = vblkdev->size;
	pstore_zone.kmsg_size = vblkdev->pstore_kmsg_size;
	pstore_zone.max_reason = vblkdev->pstore_max_reason;
	pstore_zone.pmsg_size = 0;
	pstore_zone.console_size = 0;
	pstore_zone.ftrace_size = 0;
	pstore_zone.read = vblk_oops_read;
	pstore_zone.write = vblk_oops_write;
	pstore_zone.panic_write = vblk_oops_panic_write;

	if (register_pstore_zone(&pstore_zone))
		dev_err(vblkdev->device, "Could not register with pstore_zone\n");

}

static int vblk_oops_send_config_cmd(struct vblk_dev *vblkdev)
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

static int vblk_oops_get_configinfo(struct vblk_dev *vblkdev)
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

static void vblk_oops_init_device(struct work_struct *ws)
{
	struct vblk_dev *vblkdev = container_of(ws, struct vblk_dev, init.work);

	dev_info(vblkdev->device, "%s: Check for IVC channel reset\n", __func__);

	/* wait for ivc channel reset to finish */
	if (tegra_hv_ivc_channel_notified(vblkdev->ivck) != 0) {
		dev_warn(vblkdev->device,
			"%s: IVC channel reset not complete...retry\n", __func__);
		schedule_delayed_work(&vblkdev->init,
					msecs_to_jiffies(VSC_RESPONSE_WAIT_MS));
		return;
	}

	if (tegra_hv_ivc_can_read(vblkdev->ivck) && !vblkdev->initialized) {
		if (vblk_oops_get_configinfo(vblkdev)) {
			dev_err(vblkdev->device,
				"unable to get configinfo, giving up\n");
			return;
		}
		vblkdev->initialized = true;
		setup_device(vblkdev);
	}
}

static int tegra_hv_vblk_oops_probe(struct platform_device *pdev)
{
	static struct device_node *vblk_node;
	struct device *dev = &pdev->dev;
	int ret;
	struct tegra_hv_ivm_cookie *ivmk;

	if (!is_tegra_hypervisor_mode()) {
		dev_err(dev, "Hypervisor is not present\n");
		return -ENODEV;
	}

	vblk_node = dev->of_node;
	if (vblk_node == NULL) {
		dev_err(dev, "No of_node data\n");
		return -ENODEV;
	}

	vblkdev_oops = devm_kzalloc(dev, sizeof(struct vblk_dev), GFP_KERNEL);
	if (vblkdev_oops == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, vblkdev_oops);
	vblkdev_oops->device = dev;

	/* Get properties of instance and ivc channel id */
	if (of_property_read_u32(vblk_node, "instance", &(vblkdev_oops->devnum))) {
		dev_err(dev, "Failed to read instance property\n");
		ret = -ENODEV;
		goto fail;
	} else {
		if (of_property_read_u32_index(vblk_node, "ivc", 1,
			&(vblkdev_oops->ivc_id))) {
			dev_err(dev, "Failed to read ivc property\n");
			ret = -ENODEV;
			goto fail;
		}
		if (of_property_read_u32_index(vblk_node, "mempool", 0,
			&(vblkdev_oops->ivm_id))) {
			dev_err(dev, "Failed to read mempool property\n");
			ret = -ENODEV;
			goto fail;
		}
	}

	if (of_property_read_u32(vblk_node, "pstore_max_reason",
				&(vblkdev_oops->pstore_max_reason))) {
		dev_warn(dev,
			"Failed to read pstore_max_reason property, assuming %d\n",
			KMSG_DUMP_OOPS);
		vblkdev_oops->pstore_max_reason = KMSG_DUMP_OOPS;
	} else if (vblkdev_oops->pstore_max_reason != KMSG_DUMP_OOPS) {
		/* currently we support only KMSG_DUMP_OOPS */
		dev_warn(dev, "Unsupported pstore_max_reason property, assuming %d\n",
			KMSG_DUMP_OOPS);
		vblkdev_oops->pstore_max_reason = KMSG_DUMP_OOPS;
	}

	if (of_property_read_u32(vblk_node, "pstore_kmsg_size",
				&(vblkdev_oops->pstore_kmsg_size))) {
		dev_warn(dev, "Failed to read pstore_kmsg_size property, assuming %d bytes\n",
			PSTORE_KMSG_RECORD_SIZE);
		vblkdev_oops->pstore_kmsg_size = PSTORE_KMSG_RECORD_SIZE;
		/* defer alignment and minimum size check for later */
	}

	vblkdev_oops->ivck = tegra_hv_ivc_reserve(NULL, vblkdev_oops->ivc_id, NULL);
	if (IS_ERR_OR_NULL(vblkdev_oops->ivck)) {
		dev_err(dev, "Failed to reserve IVC channel %d\n",
			vblkdev_oops->ivc_id);
		vblkdev_oops->ivck = NULL;
		ret = -ENODEV;
		goto fail;
	}

	ivmk = tegra_hv_mempool_reserve(vblkdev_oops->ivm_id);
	if (IS_ERR_OR_NULL(ivmk)) {
		dev_err(dev, "Failed to reserve IVM channel %d\n",
			vblkdev_oops->ivm_id);
		ivmk = NULL;
		ret = -ENODEV;
		goto free_ivc;
	}
	vblkdev_oops->ivmk = ivmk;

	vblkdev_oops->shared_buffer = devm_memremap(vblkdev_oops->device,
			ivmk->ipa, ivmk->size, MEMREMAP_WB);
	if (IS_ERR_OR_NULL(vblkdev_oops->shared_buffer)) {
		dev_err(dev, "Failed to map mempool area %d\n",
				vblkdev_oops->ivm_id);
		ret = -ENOMEM;
		goto free_mempool;
	}

	vblkdev_oops->initialized = false;

	INIT_DELAYED_WORK(&vblkdev_oops->init, vblk_oops_init_device);

	tegra_hv_ivc_channel_reset(vblkdev_oops->ivck);
	if (vblk_oops_send_config_cmd(vblkdev_oops)) {
		dev_err(dev, "Failed to send config cmd\n");
		ret = -EACCES;
		goto free_mempool;
	}

	/* postpone init work that needs response */
	schedule_delayed_work(&vblkdev_oops->init,
				msecs_to_jiffies(VSC_RESPONSE_WAIT_MS));

	return 0;

free_mempool:
	tegra_hv_mempool_unreserve(vblkdev_oops->ivmk);

free_ivc:
	tegra_hv_ivc_unreserve(vblkdev_oops->ivck);

fail:
	return ret;
}

static int tegra_hv_vblk_oops_remove(struct platform_device *pdev)
{
	struct vblk_dev *vblkdev = platform_get_drvdata(pdev);

	tegra_hv_ivc_unreserve(vblkdev->ivck);
	tegra_hv_mempool_unreserve(vblkdev->ivmk);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tegra_hv_vblk_oops_match[] = {
	{ .compatible = "nvidia,tegra-hv-oops-storage", },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_hv_vblk_oops_match);
#endif /* CONFIG_OF */

static struct platform_driver tegra_hv_vblk_oops_driver = {
	.probe	= tegra_hv_vblk_oops_probe,
	.remove	= tegra_hv_vblk_oops_remove,
	.driver	= {
		.name = OOPS_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_hv_vblk_oops_match),
	},
};

static int __init tegra_hv_vblk_driver_init(void)
{
	return platform_driver_register(&tegra_hv_vblk_oops_driver);
}
module_init(tegra_hv_vblk_driver_init);

static void __exit tegra_hv_vblk_driver_exit(void)
{
	platform_driver_unregister(&tegra_hv_vblk_oops_driver);
}
module_exit(tegra_hv_vblk_driver_exit);

MODULE_AUTHOR("Haribabu Narayanan <hnarayanan@nvidia.com>");
MODULE_DESCRIPTION("Virtual OOPS storage device over Tegra Hypervisor IVC channel");
MODULE_LICENSE("GPL");

