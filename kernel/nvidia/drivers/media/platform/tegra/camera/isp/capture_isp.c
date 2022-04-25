/*
 * Tegra ISP capture operations
 *
 * Tegra NvCapture ISP KMD
 *
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Sudhir Vyas <svyas@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/completion.h>
#include <linux/nospec.h>
#include <linux/nvhost.h>
#include <linux/of_platform.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/tegra-capture-ivc.h>
#include <asm/arch_timer.h>
#include <media/capture_common.h>
#include <media/capture_isp.h>
#include <media/isp_channel.h>
#include <media/mc_common.h>

#include "soc/tegra/camrtc-capture.h"
#include "soc/tegra/camrtc-capture-messages.h"

#include <soc/tegra/chip-id.h>

#define CAPTURE_CHANNEL_UNKNOWN_RESP 0xFFFFFFFF
#define CAPTURE_CHANNEL_ISP_INVALID_ID 0xFFFF

struct isp_desc_rec {
	struct capture_common_buf requests;
	size_t request_buf_size;
	uint32_t queue_depth;
	uint32_t request_size;
	void *requests_memoryinfo;
		/**< memory info ringbuffer */
	uint64_t requests_memoryinfo_iova;
		/**< memory info ringbuffer rtcpu iova */

	uint32_t progress_status_buffer_depth;

	struct mutex unpins_list_lock;
	struct capture_common_unpins *unpins_list;
};

/* ISP capture context per channel */
struct isp_capture {
	uint16_t channel_id;
	struct device *rtcpu_dev;
	struct tegra_isp_channel *isp_channel;
	struct capture_buffer_table *buffer_ctx;

	/* isp capture desc and it's ring buffer related details */
	struct isp_desc_rec capture_desc_ctx;

	/* isp program desc and it's ring buffer related details */
	struct isp_desc_rec program_desc_ctx;

	struct capture_common_status_notifier progress_status_notifier;
	bool is_progress_status_notifier_set;

#ifdef HAVE_ISP_GOS_TABLES
	uint32_t num_gos_tables;
	const dma_addr_t *gos_tables;
#endif

	struct syncpoint_info progress_sp;
	struct syncpoint_info stats_progress_sp;

	struct completion control_resp;
	struct completion capture_resp;
	struct completion capture_program_resp;

	struct mutex control_msg_lock;
	struct CAPTURE_CONTROL_MSG control_resp_msg;

	struct mutex reset_lock;
	bool reset_capture_program_flag;
	bool reset_capture_flag;
};

static void isp_capture_ivc_control_callback(const void *ivc_resp,
		const void *pcontext)
{
	const struct CAPTURE_CONTROL_MSG *control_msg = ivc_resp;
	struct isp_capture *capture = (struct isp_capture *)pcontext;
	struct tegra_isp_channel *chan = capture->isp_channel;

	if (unlikely(capture == NULL)) {
		dev_err(chan->isp_dev, "%s: invalid context", __func__);
		return;
	}

	if (unlikely(control_msg == NULL)) {
		dev_err(chan->isp_dev, "%s: invalid response", __func__);
		return;
	}

	switch (control_msg->header.msg_id) {
	case CAPTURE_CHANNEL_ISP_SETUP_RESP:
	case CAPTURE_CHANNEL_ISP_RESET_RESP:
	case CAPTURE_CHANNEL_ISP_RELEASE_RESP:
		memcpy(&capture->control_resp_msg, control_msg,
				sizeof(*control_msg));
		complete(&capture->control_resp);
		break;
	default:
		dev_err(chan->isp_dev,
			"%s: unknown capture isp control resp", __func__);
		break;
	}
}

static void isp_capture_request_unpin(struct tegra_isp_channel *chan,
		uint32_t buffer_index);

static void isp_capture_program_request_unpin(struct tegra_isp_channel *chan,
		uint32_t buffer_index);

static void isp_capture_ivc_status_callback(const void *ivc_resp,
		const void *pcontext)
{
	struct CAPTURE_MSG *status_msg = (struct CAPTURE_MSG *)ivc_resp;
	struct isp_capture *capture = (struct isp_capture *)pcontext;
	struct tegra_isp_channel *chan = capture->isp_channel;
	uint32_t buffer_index;

	if (unlikely(capture == NULL)) {
		dev_err(chan->isp_dev, "%s: invalid context", __func__);
		return;
	}

	if (unlikely(status_msg == NULL)) {
		dev_err(chan->isp_dev, "%s: invalid response", __func__);
		return;
	}

	switch (status_msg->header.msg_id) {
	case CAPTURE_ISP_STATUS_IND:
		buffer_index = status_msg->capture_isp_status_ind.buffer_index;
		isp_capture_request_unpin(chan, buffer_index);
		dma_sync_single_range_for_cpu(capture->rtcpu_dev,
			capture->capture_desc_ctx.requests.iova,
			buffer_index * capture->capture_desc_ctx.request_size,
			capture->capture_desc_ctx.request_size,
			DMA_FROM_DEVICE);

		if (capture->is_progress_status_notifier_set) {
			capture_common_set_progress_status(
				&capture->progress_status_notifier,
				buffer_index,
				capture->capture_desc_ctx.progress_status_buffer_depth,
				PROGRESS_STATUS_DONE);
		} else {
			/*
			 * Only fire completions if not using
			 * the new progress status buffer mechanism
			 */
			complete(&capture->capture_resp);
		}

		dev_dbg(chan->isp_dev, "%s: status chan_id %u msg_id %u\n",
			__func__, status_msg->header.channel_id,
			status_msg->header.msg_id);
		break;
	case CAPTURE_ISP_PROGRAM_STATUS_IND:
		buffer_index =
			status_msg->capture_isp_program_status_ind.buffer_index;
		isp_capture_program_request_unpin(chan, buffer_index);
		dma_sync_single_range_for_cpu(capture->rtcpu_dev,
			capture->program_desc_ctx.requests.iova,
			buffer_index * capture->program_desc_ctx.request_size,
			capture->program_desc_ctx.request_size,
			DMA_FROM_DEVICE);

		if (capture->is_progress_status_notifier_set) {
			/*
			 * Program status notifiers are after the process status notifiers;
			 * add the process status buffer depth as an offset.
			 */
			capture_common_set_progress_status(
				&capture->progress_status_notifier,
				buffer_index +
					capture->capture_desc_ctx.progress_status_buffer_depth,
				capture->program_desc_ctx.progress_status_buffer_depth +
					capture->capture_desc_ctx.progress_status_buffer_depth,
				PROGRESS_STATUS_DONE);
		} else {
			/*
			 * Only fire completions if not using
			 * the new progress status buffer mechanism
			 */
			complete(&capture->capture_program_resp);
		}

		dev_dbg(chan->isp_dev,
			"%s: isp_ program status chan_id %u msg_id %u\n",
			__func__, status_msg->header.channel_id,
			status_msg->header.msg_id);
		break;
	default:
		dev_err(chan->isp_dev,
			"%s: unknown capture resp", __func__);
		break;
	}
}

int isp_capture_init(struct tegra_isp_channel *chan)
{
	struct isp_capture *capture;
	struct device_node *dn;
	struct platform_device *rtc_pdev;

	dev_dbg(chan->isp_dev, "%s++\n", __func__);
	dn = of_find_node_by_path("tegra-camera-rtcpu");
	if (of_device_is_available(dn) == 0) {
		dev_err(chan->isp_dev, "failed to find rtcpu device node\n");
		return -ENODEV;
	}
	rtc_pdev = of_find_device_by_node(dn);
	if (rtc_pdev == NULL) {
		dev_err(chan->isp_dev, "failed to find rtcpu platform\n");
		return -ENODEV;
	}

	capture = kzalloc(sizeof(*capture), GFP_KERNEL);
	if (unlikely(capture == NULL)) {
		dev_err(chan->isp_dev, "failed to allocate capture channel\n");
		return -ENOMEM;
	}

	capture->rtcpu_dev = &rtc_pdev->dev;

	init_completion(&capture->control_resp);
	init_completion(&capture->capture_resp);
	init_completion(&capture->capture_program_resp);

	mutex_init(&capture->control_msg_lock);
	mutex_init(&capture->capture_desc_ctx.unpins_list_lock);
	mutex_init(&capture->program_desc_ctx.unpins_list_lock);
	mutex_init(&capture->reset_lock);

	capture->isp_channel = chan;
	chan->capture_data = capture;

	capture->channel_id = CAPTURE_CHANNEL_ISP_INVALID_ID;

	capture->reset_capture_program_flag = false;
	capture->reset_capture_flag = false;

	return 0;
}

void isp_capture_shutdown(struct tegra_isp_channel *chan)
{
	struct isp_capture *capture = chan->capture_data;

	dev_dbg(chan->isp_dev, "%s--\n", __func__);
	if (capture == NULL)
		return;

	if (capture->channel_id != CAPTURE_CHANNEL_ISP_INVALID_ID) {
		/* No valid ISP reset flags defined now, use zero */
		isp_capture_reset(chan, 0);
		isp_capture_release(chan, 0);
	}

	kfree(capture);
	chan->capture_data = NULL;
}

static int isp_capture_ivc_send_control(struct tegra_isp_channel *chan,
		const struct CAPTURE_CONTROL_MSG *msg, size_t size,
		uint32_t resp_id)
{
	struct isp_capture *capture = chan->capture_data;
	struct CAPTURE_MSG_HEADER resp_header = msg->header;
	uint32_t timeout = HZ;
	int err = 0;

	dev_dbg(chan->isp_dev, "%s: sending chan_id %u msg_id %u\n",
			__func__, resp_header.channel_id, resp_header.msg_id);

	resp_header.msg_id = resp_id;

	/* Send capture control IVC message */
	mutex_lock(&capture->control_msg_lock);
	err = tegra_capture_ivc_control_submit(msg, size);
	if (err < 0) {
		dev_err(chan->isp_dev, "IVC control submit failed\n");
		goto fail;
	}

	timeout = wait_for_completion_timeout(&capture->control_resp, timeout);
	if (timeout <= 0) {
		dev_err(chan->isp_dev,
			"no reply from camera processor\n");
		err = -ETIMEDOUT;
		goto fail;
	}

	if (memcmp(&resp_header, &capture->control_resp_msg.header,
			sizeof(resp_header)) != 0) {
		dev_err(chan->isp_dev,
			"unexpected response from camera processor\n");
		err = -EINVAL;
		goto fail;
	}
	mutex_unlock(&capture->control_msg_lock);

	dev_dbg(chan->isp_dev, "%s: response chan_id %u msg_id %u\n",
			__func__, capture->control_resp_msg.header.channel_id,
			capture->control_resp_msg.header.msg_id);
	return 0;

fail:
	mutex_unlock(&capture->control_msg_lock);
	return err;
}

static int isp_capture_setup_syncpts(struct tegra_isp_channel *chan);
static void isp_capture_release_syncpts(struct tegra_isp_channel *chan);

static int isp_capture_setup_syncpt(struct tegra_isp_channel *chan,
				const char *name, bool enable,
				struct syncpoint_info *sp)
{
	struct platform_device *pdev = chan->ndev;
	uint32_t gos_index = GOS_INDEX_INVALID;
	uint32_t gos_offset = 0;
	int err;

	memset(sp, 0, sizeof(*sp));

	if (!enable)
		return 0;


	err = chan->ops->alloc_syncpt(pdev, name, &sp->id);
	if (err)
		return err;

	err = nvhost_syncpt_read_ext_check(pdev, sp->id, &sp->threshold);
	if (err)
		goto cleanup;

	err = chan->ops->get_syncpt_gos_backing(pdev, sp->id, &sp->shim_addr,
				&gos_index, &gos_offset);
	if (err)
		goto cleanup;

	sp->gos_index = gos_index;
	sp->gos_offset = gos_offset;

	return 0;

cleanup:
	chan->ops->release_syncpt(pdev, sp->id);
	memset(sp, 0, sizeof(*sp));

	return err;
}

static int isp_capture_setup_syncpts(struct tegra_isp_channel *chan)
{
	struct isp_capture *capture = chan->capture_data;
	int err = 0;

#ifdef HAVE_ISP_GOS_TABLES
	capture->num_gos_tables = chan->ops->get_gos_table(chan->ndev,
							&capture->gos_tables);
#endif

	err = isp_capture_setup_syncpt(chan, "progress", true,
			&capture->progress_sp);
	if (err < 0)
		goto fail;

	err = isp_capture_setup_syncpt(chan, "stats_progress",
				true,
				&capture->stats_progress_sp);
	if (err < 0)
		goto fail;

	return 0;

fail:
	isp_capture_release_syncpts(chan);
	return err;
}

static void isp_capture_release_syncpt(struct tegra_isp_channel *chan,
				struct syncpoint_info *sp)
{
	if (sp->id)
		chan->ops->release_syncpt(chan->ndev, sp->id);

	memset(sp, 0, sizeof(*sp));
}

static void isp_capture_release_syncpts(struct tegra_isp_channel *chan)
{
	struct isp_capture *capture = chan->capture_data;

	isp_capture_release_syncpt(chan, &capture->progress_sp);
	isp_capture_release_syncpt(chan, &capture->stats_progress_sp);
}

int isp_capture_setup(struct tegra_isp_channel *chan,
		struct isp_capture_setup *setup)
{
	struct capture_buffer_table *buffer_ctx;
	struct isp_capture *capture = chan->capture_data;
	uint32_t transaction;
	struct CAPTURE_CONTROL_MSG control_msg;
	struct CAPTURE_CONTROL_MSG *resp_msg = &capture->control_resp_msg;
	struct capture_channel_isp_config *config =
		&control_msg.channel_isp_setup_req.channel_config;
	int err = 0;
#ifdef HAVE_ISP_GOS_TABLES
	int i;
#endif

	if (capture == NULL) {
		dev_err(chan->isp_dev,
			 "%s: isp capture uninitialized\n", __func__);
		return -ENODEV;
	}

	if (capture->channel_id != CAPTURE_CHANNEL_ISP_INVALID_ID) {
		dev_err(chan->isp_dev,
			"%s: already setup, release first\n", __func__);
		return -EEXIST;
	}

	dev_dbg(chan->isp_dev, "chan flags %u\n", setup->channel_flags);
	dev_dbg(chan->isp_dev, "queue depth %u\n", setup->queue_depth);
	dev_dbg(chan->isp_dev, "request size %u\n", setup->request_size);

	if (setup->channel_flags == 0 ||
			setup->queue_depth == 0 ||
			setup->request_size == 0)
		return -EINVAL;

	buffer_ctx = create_buffer_table(chan->isp_dev);
	if (unlikely(buffer_ctx == NULL)) {
		dev_err(chan->isp_dev, "cannot setup buffer context");
		return -ENOMEM;
	}

	/* pin the capture descriptor ring buffer to RTCPU */
	dev_dbg(chan->isp_dev, "%s: descr buffer handle 0x%x\n",
			__func__, setup->mem);
	err = capture_common_pin_memory(capture->rtcpu_dev,
			setup->mem, &capture->capture_desc_ctx.requests);
	if (err < 0) {
		dev_err(chan->isp_dev, "%s: memory setup failed\n", __func__);
		goto pin_fail;
	}

	/* pin the capture descriptor ring buffer to ISP */
	err = capture_buffer_add(buffer_ctx, setup->mem);
	if (err < 0) {
		dev_err(chan->isp_dev, "%s: memory setup failed\n", __func__);
		goto pin_fail;
	}

	/* cache isp capture desc ring buffer details */
	capture->capture_desc_ctx.queue_depth = setup->queue_depth;
	capture->capture_desc_ctx.request_size = setup->request_size;
	capture->capture_desc_ctx.request_buf_size = setup->request_size *
							setup->queue_depth;

	/* allocate isp capture desc unpin list based on queue depth */
	capture->capture_desc_ctx.unpins_list = vzalloc(
		capture->capture_desc_ctx.queue_depth *
			sizeof(*capture->capture_desc_ctx.unpins_list));

	if (unlikely(capture->capture_desc_ctx.unpins_list == NULL)) {
		dev_err(chan->isp_dev, "failed to allocate unpins array\n");
		goto unpins_list_fail;
	}

	/* Allocate memory info ring buffer for isp capture descriptors */
	capture->capture_desc_ctx.requests_memoryinfo =
		dma_alloc_coherent(capture->rtcpu_dev,
			capture->capture_desc_ctx.queue_depth *
			sizeof(struct isp_capture_descriptor_memoryinfo),
			&capture->capture_desc_ctx.requests_memoryinfo_iova,
			GFP_KERNEL);

	if (!capture->capture_desc_ctx.requests_memoryinfo) {
		dev_err(chan->isp_dev,
			"%s: capture_desc_ctx meminfo alloc failed\n",
			__func__);
		goto capture_meminfo_alloc_fail;
	}

	/* pin the isp program descriptor ring buffer */
	dev_dbg(chan->isp_dev, "%s: descr buffer handle %u\n",
			__func__, setup->isp_program_mem);
	err = capture_common_pin_memory(capture->rtcpu_dev,
				setup->isp_program_mem,
				&capture->program_desc_ctx.requests);
	if (err < 0) {
		dev_err(chan->isp_dev,
			"%s: isp_program memory setup failed\n", __func__);
		goto prog_pin_fail;
	}

	/* pin the isp program descriptor ring buffer to ISP */
	err = capture_buffer_add(buffer_ctx, setup->isp_program_mem);
	if (err < 0) {
		dev_err(chan->isp_dev,
			"%s: isp_program memory setup failed\n", __func__);
		goto prog_pin_fail;
	}

	/* cache isp program desc ring buffer details */
	capture->program_desc_ctx.queue_depth = setup->isp_program_queue_depth;
	capture->program_desc_ctx.request_size =
					setup->isp_program_request_size;
	capture->program_desc_ctx.request_buf_size =
					setup->isp_program_request_size *
						setup->isp_program_queue_depth;

	/* allocate isp program unpin list based on queue depth */
	capture->program_desc_ctx.unpins_list = vzalloc(
			capture->program_desc_ctx.queue_depth *
			 sizeof(*capture->program_desc_ctx.unpins_list));

	if (unlikely(capture->program_desc_ctx.unpins_list == NULL)) {
		dev_err(chan->isp_dev,
			"failed to allocate isp program unpins array\n");
		goto prog_unpins_list_fail;
	}

	/* Allocate memory info ring buffer for program descriptors */
	capture->program_desc_ctx.requests_memoryinfo =
		dma_alloc_coherent(capture->rtcpu_dev,
			capture->program_desc_ctx.queue_depth *
			sizeof(struct memoryinfo_surface),
			&capture->program_desc_ctx.requests_memoryinfo_iova,
			GFP_KERNEL);

	if (!capture->program_desc_ctx.requests_memoryinfo) {
		dev_err(chan->isp_dev,
			"%s: program_desc_ctx meminfo alloc failed\n",
			__func__);
		goto program_meminfo_alloc_fail;
	}

	err = isp_capture_setup_syncpts(chan);
	if (err < 0) {
		dev_err(chan->isp_dev, "%s: syncpt setup failed\n", __func__);
		goto syncpt_fail;
	}

	err = tegra_capture_ivc_register_control_cb(
			&isp_capture_ivc_control_callback,
			&transaction, capture);
	if (err < 0) {
		dev_err(chan->isp_dev, "failed to register control callback\n");
		goto control_cb_fail;
	}

	/* Fill in control config msg to be sent over ctrl ivc chan to RTCPU */
	memset(&control_msg, 0, sizeof(control_msg));

	control_msg.header.msg_id = CAPTURE_CHANNEL_ISP_SETUP_REQ;
	control_msg.header.transaction = transaction;

	config->channel_flags = setup->channel_flags;

	config->request_queue_depth = setup->queue_depth;
	config->request_size = setup->request_size;
	config->requests = capture->capture_desc_ctx.requests.iova;
	config->requests_memoryinfo =
		capture->capture_desc_ctx.requests_memoryinfo_iova;
	config->request_memoryinfo_size =
		sizeof(struct isp_capture_descriptor_memoryinfo);

	config->program_queue_depth = setup->isp_program_queue_depth;
	config->program_size = setup->isp_program_request_size;
	config->programs = capture->program_desc_ctx.requests.iova;
	config->programs_memoryinfo =
		capture->program_desc_ctx.requests_memoryinfo_iova;
	config->program_memoryinfo_size =
		sizeof(struct memoryinfo_surface);

	config->progress_sp = capture->progress_sp;
	config->stats_progress_sp = capture->stats_progress_sp;

#ifdef HAVE_ISP_GOS_TABLES
	dev_dbg(chan->isp_dev, "%u GoS tables configured.\n",
		capture->num_gos_tables);
	for (i = 0; i < capture->num_gos_tables; i++) {
		config->isp_gos_tables[i] = (iova_t)capture->gos_tables[i];
		dev_dbg(chan->isp_dev, "gos[%d] = 0x%08llx\n",
			i, (u64)capture->gos_tables[i]);
	}
	config->num_isp_gos_tables = capture->num_gos_tables;
#endif

	err = isp_capture_ivc_send_control(chan, &control_msg,
			sizeof(control_msg), CAPTURE_CHANNEL_ISP_SETUP_RESP);
	if (err < 0)
		goto submit_fail;

	if (resp_msg->channel_isp_setup_resp.result != CAPTURE_OK) {
		dev_err(chan->isp_dev, "%s: control failed, errno %d", __func__,
			resp_msg->channel_setup_resp.result);
		err = -EIO;
		goto submit_fail;
	}

	capture->channel_id = resp_msg->channel_isp_setup_resp.channel_id;

	err = tegra_capture_ivc_notify_chan_id(capture->channel_id,
			transaction);
	if (err < 0) {
		dev_err(chan->isp_dev, "failed to update control callback\n");
		goto cb_fail;
	}

	err = tegra_capture_ivc_register_capture_cb(
			&isp_capture_ivc_status_callback,
			capture->channel_id, capture);
	if (err < 0) {
		dev_err(chan->isp_dev, "failed to register capture callback\n");
		goto cb_fail;
	}

	capture->buffer_ctx = buffer_ctx;

	return 0;

cb_fail:
	isp_capture_release(chan, CAPTURE_CHANNEL_RESET_FLAG_IMMEDIATE);
	return err;
submit_fail:
	tegra_capture_ivc_unregister_control_cb(transaction);
control_cb_fail:
	isp_capture_release_syncpts(chan);
syncpt_fail:
	dma_free_coherent(capture->rtcpu_dev,
		capture->program_desc_ctx.queue_depth *
		sizeof(struct memoryinfo_surface),
		capture->program_desc_ctx.requests_memoryinfo,
		capture->program_desc_ctx.requests_memoryinfo_iova);
program_meminfo_alloc_fail:
	vfree(capture->program_desc_ctx.unpins_list);
prog_unpins_list_fail:
	capture_common_unpin_memory(&capture->program_desc_ctx.requests);
prog_pin_fail:
	dma_free_coherent(capture->rtcpu_dev,
		capture->capture_desc_ctx.queue_depth *
			sizeof(struct isp_capture_descriptor_memoryinfo),
		capture->capture_desc_ctx.requests_memoryinfo,
		capture->capture_desc_ctx.requests_memoryinfo_iova);
capture_meminfo_alloc_fail:
	vfree(capture->capture_desc_ctx.unpins_list);
unpins_list_fail:
	capture_common_unpin_memory(&capture->capture_desc_ctx.requests);
pin_fail:
	destroy_buffer_table(buffer_ctx);
	return err;
}

int isp_capture_reset(struct tegra_isp_channel *chan,
		uint32_t reset_flags)
{
	struct isp_capture *capture = chan->capture_data;
#ifdef CAPTURE_ISP_RESET_BARRIER_IND
	struct CAPTURE_MSG capture_msg;
#endif
	struct CAPTURE_CONTROL_MSG control_msg;
	struct CAPTURE_CONTROL_MSG *resp_msg = &capture->control_resp_msg;
	int i;
	int err = 0;

	if (capture == NULL) {
		dev_err(chan->isp_dev,
			 "%s: isp capture uninitialized\n", __func__);
		return -ENODEV;
	}

	if (capture->channel_id == CAPTURE_CHANNEL_ISP_INVALID_ID) {
		dev_err(chan->isp_dev,
			"%s: setup channel first\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&capture->reset_lock);
	capture->reset_capture_program_flag = true;
	capture->reset_capture_flag = true;

#ifdef CAPTURE_ISP_RESET_BARRIER_IND
	memset(&capture_msg, 0, sizeof(capture_msg));
	capture_msg.header.msg_id = CAPTURE_ISP_RESET_BARRIER_IND;
	capture_msg.header.channel_id = capture->channel_id;

	err = tegra_capture_ivc_capture_submit(&capture_msg,
			sizeof(capture_msg));
	if (err < 0) {
		dev_err(chan->isp_dev, "IVC capture submit failed\n");
		goto error;
	}
#endif

	memset(&control_msg, 0, sizeof(control_msg));
	control_msg.header.msg_id = CAPTURE_CHANNEL_ISP_RESET_REQ;
	control_msg.header.channel_id = capture->channel_id;
	control_msg.channel_isp_reset_req.reset_flags = reset_flags;

	err = isp_capture_ivc_send_control(chan, &control_msg,
			sizeof(control_msg), CAPTURE_CHANNEL_ISP_RESET_RESP);
	if (err < 0)
		goto error;

#ifdef CAPTURE_ISP_RESET_BARRIER_IND
	if (resp_msg->channel_isp_reset_resp.result == CAPTURE_ERROR_TIMEOUT) {
		dev_dbg(chan->isp_dev, "%s: isp reset timedout\n", __func__);
		err = -EAGAIN;
		goto error;
	}
#endif

	if (resp_msg->channel_isp_reset_resp.result != CAPTURE_OK) {
		dev_err(chan->isp_dev, "%s: control failed, errno %d", __func__,
			resp_msg->channel_isp_reset_resp.result);
		err = -EINVAL;
		goto error;
	}

	for (i = 0; i < capture->program_desc_ctx.queue_depth; i++) {
		isp_capture_program_request_unpin(chan, i);
		complete(&capture->capture_program_resp);
	}

	for (i = 0; i < capture->capture_desc_ctx.queue_depth; i++) {
		isp_capture_request_unpin(chan, i);
		complete(&capture->capture_resp);
	}

	mutex_unlock(&capture->reset_lock);

	return 0;

error:
	mutex_unlock(&capture->reset_lock);
	return err;
}

int isp_capture_release(struct tegra_isp_channel *chan,
		uint32_t reset_flags)
{
	struct isp_capture *capture = chan->capture_data;
	struct CAPTURE_CONTROL_MSG control_msg;
	struct CAPTURE_CONTROL_MSG *resp_msg = &capture->control_resp_msg;
	int i;
	int err = 0;

	if (capture == NULL) {
		dev_err(chan->isp_dev,
			 "%s: isp capture uninitialized\n", __func__);
		return -ENODEV;
	}

	if (capture->channel_id == CAPTURE_CHANNEL_ISP_INVALID_ID) {
		dev_err(chan->isp_dev,
			"%s: setup channel first\n", __func__);
		return -ENODEV;
	}

	memset(&control_msg, 0, sizeof(control_msg));

	control_msg.header.msg_id = CAPTURE_CHANNEL_ISP_RELEASE_REQ;
	control_msg.header.channel_id = capture->channel_id;
	control_msg.channel_release_req.reset_flags = reset_flags;

	err = isp_capture_ivc_send_control(chan, &control_msg,
			sizeof(control_msg), CAPTURE_CHANNEL_ISP_RELEASE_RESP);
	if (err < 0)
		goto error;

	if (resp_msg->channel_isp_release_resp.result != CAPTURE_OK) {
		dev_err(chan->isp_dev, "%s: control failed, errno %d", __func__,
			resp_msg->channel_release_resp.result);
		err = -EINVAL;
		goto error;
	}

	err = tegra_capture_ivc_unregister_capture_cb(capture->channel_id);
	if (err < 0) {
		dev_err(chan->isp_dev,
			"failed to unregister capture callback\n");
		goto error;
	}

	err = tegra_capture_ivc_unregister_control_cb(capture->channel_id);
	if (err < 0) {
		dev_err(chan->isp_dev,
			"failed to unregister control callback\n");
		goto error;
	}

	for (i = 0; i < capture->program_desc_ctx.queue_depth; i++) {
		complete(&capture->capture_program_resp);
		isp_capture_program_request_unpin(chan, i);
	}
	speculation_barrier();

	capture_common_unpin_memory(&capture->program_desc_ctx.requests);

	for (i = 0; i < capture->capture_desc_ctx.queue_depth; i++) {
		complete(&capture->capture_resp);
		isp_capture_request_unpin(chan, i);
	}

	isp_capture_release_syncpts(chan);

	capture_common_unpin_memory(&capture->capture_desc_ctx.requests);

	vfree(capture->program_desc_ctx.unpins_list);
	capture->program_desc_ctx.unpins_list = NULL;
	vfree(capture->capture_desc_ctx.unpins_list);
	capture->capture_desc_ctx.unpins_list = NULL;

	dma_free_coherent(capture->rtcpu_dev,
		capture->program_desc_ctx.queue_depth *
			sizeof(struct memoryinfo_surface),
		capture->program_desc_ctx.requests_memoryinfo,
		capture->program_desc_ctx.requests_memoryinfo_iova);

	dma_free_coherent(capture->rtcpu_dev,
		capture->capture_desc_ctx.queue_depth *
			sizeof(struct isp_capture_descriptor_memoryinfo),
		capture->capture_desc_ctx.requests_memoryinfo,
		capture->capture_desc_ctx.requests_memoryinfo_iova);

	if (capture->is_progress_status_notifier_set)
		capture_common_release_progress_status_notifier(
			&capture->progress_status_notifier);

	destroy_buffer_table(capture->buffer_ctx);

	capture->channel_id = CAPTURE_CHANNEL_ISP_INVALID_ID;

	return 0;

error:
	return err;
}

static int isp_capture_read_syncpt(struct tegra_isp_channel *chan,
		struct syncpoint_info *sp, uint32_t *val)
{
	int err;

	if (sp->id) {
		err = nvhost_syncpt_read_ext_check(chan->ndev,
						sp->id, val);
		if (err < 0) {
			dev_err(chan->isp_dev,
				"%s: get syncpt %i val failed\n", __func__,
				sp->id);
			return -EINVAL;
		}
	}

	return 0;
}

static int isp_capture_populate_fence_info(struct tegra_isp_channel *chan,
		int fence_offset, uint32_t gos_relative,
		uint32_t sp_relative)
{
	struct isp_capture *capture = chan->capture_data;
	void *reloc_page_addr = NULL;
	int err = 0;
	uint64_t sp_raw;
	uint32_t sp_id;
	dma_addr_t syncpt_addr;
	uint32_t gos_index;
	uint32_t gos_offset;
	uint64_t gos_info = 0;

	reloc_page_addr = dma_buf_kmap(capture->capture_desc_ctx.requests.buf,
				fence_offset >> PAGE_SHIFT);

	if (unlikely(reloc_page_addr == NULL)) {
		dev_err(chan->isp_dev, "%s: couldn't map request\n", __func__);
		return -ENOMEM;
	}

	sp_raw = __raw_readq(
			(void __iomem *)(reloc_page_addr +
			(fence_offset & ~PAGE_MASK)));
	sp_id = sp_raw & 0xFFFFFFFF;

	err = chan->ops->get_syncpt_gos_backing(chan->ndev, sp_id, &syncpt_addr,
			&gos_index, &gos_offset);
	if (err) {
		dev_err(chan->isp_dev,
			"%s: get GoS backing failed\n", __func__);
		goto ret;
	}

	gos_info = ((((uint16_t)gos_offset << 16) | ((uint8_t)gos_index) << 8)
			& 0xFFFFFFFF);

	__raw_writeq(gos_info, (void __iomem *)(reloc_page_addr +
			((fence_offset + gos_relative) & ~PAGE_MASK)));

	__raw_writeq((uint64_t)syncpt_addr, (void __iomem *)(reloc_page_addr +
			((fence_offset + sp_relative) & ~PAGE_MASK)));

ret:
	dma_buf_kunmap(capture->capture_desc_ctx.requests.buf,
			fence_offset >> PAGE_SHIFT, reloc_page_addr);
	return err;
}

static int isp_capture_setup_inputfences(struct tegra_isp_channel *chan,
		struct isp_capture_req *req, int request_offset)
{
	uint32_t __user *inpfences_reloc_user;
	uint32_t *inpfences_relocs = NULL;
	uint32_t inputfences_offset = 0;
	int i = 0;
	int err = 0;

	/* It is valid not to have inputfences for given frame capture */
	if (!req->inputfences_relocs.num_relocs)
		return 0;

	inpfences_reloc_user = (uint32_t __user *)
			(uintptr_t)req->inputfences_relocs.reloc_relatives;

	inpfences_relocs = kcalloc(req->inputfences_relocs.num_relocs,
				sizeof(uint32_t), GFP_KERNEL);
	if (unlikely(inpfences_relocs == NULL)) {
		dev_err(chan->isp_dev,
			"failed to allocate inputfences reloc array\n");
		return -ENOMEM;
	}

	err = copy_from_user(inpfences_relocs, inpfences_reloc_user,
		req->inputfences_relocs.num_relocs * sizeof(uint32_t)) ?
			-EFAULT : 0;
	if (err < 0) {
		dev_err(chan->isp_dev, "failed to copy inputfences relocs\n");
		goto fail;
	}

	for (i = 0; i < req->inputfences_relocs.num_relocs; i++) {
		inputfences_offset = request_offset +
					inpfences_relocs[i];
		err = isp_capture_populate_fence_info(chan, inputfences_offset,
				req->gos_relative, req->sp_relative);
		if (err < 0) {
			dev_err(chan->isp_dev, "Populate inputfences info failed\n");
			goto fail;
		}
	}
	speculation_barrier(); /* break_spec_p#5_1 */

fail:
	kfree(inpfences_relocs);
	return err;
}

static int isp_capture_setup_prefences(struct tegra_isp_channel *chan,
		struct isp_capture_req *req, int request_offset)
{
	uint32_t __user *prefence_reloc_user;
	uint32_t *prefence_relocs = NULL;
	uint32_t prefence_offset = 0;
	int i = 0;
	int err = 0;

	/* It is valid not to have prefences for given frame capture */
	if (!req->prefences_relocs.num_relocs)
		return 0;

	prefence_reloc_user = (uint32_t __user *)
			(uintptr_t)req->prefences_relocs.reloc_relatives;

	prefence_relocs = kcalloc(req->prefences_relocs.num_relocs,
		sizeof(uint32_t), GFP_KERNEL);
	if (unlikely(prefence_relocs == NULL)) {
		dev_err(chan->isp_dev,
			"failed to allocate prefences reloc array\n");
		return -ENOMEM;
	}

	err = copy_from_user(prefence_relocs, prefence_reloc_user,
		req->prefences_relocs.num_relocs * sizeof(uint32_t)) ?
			-EFAULT : 0;
	if (err < 0) {
		dev_err(chan->isp_dev, "failed to copy prefences relocs\n");
		goto fail;
	}

	for (i = 0; i < req->prefences_relocs.num_relocs; i++) {
		prefence_offset = request_offset +
					prefence_relocs[i];
		err = isp_capture_populate_fence_info(chan, prefence_offset,
				req->gos_relative, req->sp_relative);
		if (err < 0) {
			dev_err(chan->isp_dev, "Populate prefences info failed\n");
			goto fail;
		}
	}

fail:
	kfree(prefence_relocs);
	return err;
}

int isp_capture_get_info(struct tegra_isp_channel *chan,
		struct isp_capture_info *info)
{
	struct isp_capture *capture = chan->capture_data;
	int err;

	if (capture == NULL) {
		dev_err(chan->isp_dev,
			 "%s: isp capture uninitialized\n", __func__);
		return -ENODEV;
	}

	if (capture->channel_id == CAPTURE_CHANNEL_ISP_INVALID_ID) {
		dev_err(chan->isp_dev,
			"%s: setup channel first\n", __func__);
		return -ENODEV;
	}

	if (info == NULL) {
		dev_err(chan->isp_dev,
			"%s: Invalid user parameter\n", __func__);
		return -EINVAL;
	}

	info->syncpts.progress_syncpt = capture->progress_sp.id;
	info->syncpts.stats_progress_syncpt =
			capture->stats_progress_sp.id;

	err = isp_capture_read_syncpt(chan, &capture->progress_sp,
			&info->syncpts.progress_syncpt_val);
	if (err < 0)
		return err;

	err = isp_capture_read_syncpt(chan, &capture->stats_progress_sp,
			&info->syncpts.stats_progress_syncpt_val);
	if (err < 0)
		return err;

	return 0;
}

static void isp_capture_request_unpin(struct tegra_isp_channel *chan,
		uint32_t buffer_index)
{
	struct isp_capture *capture = chan->capture_data;
	struct capture_common_unpins *unpins;
	int i = 0;

	mutex_lock(&capture->capture_desc_ctx.unpins_list_lock);
	unpins = &capture->capture_desc_ctx.unpins_list[buffer_index];
	if (unpins->num_unpins != 0U) {
		for (i = 0; i < unpins->num_unpins; i++)
			put_mapping(capture->buffer_ctx, unpins->data[i]);
		(void)memset(unpins, 0U, sizeof(*unpins));
	}
	mutex_unlock(&capture->capture_desc_ctx.unpins_list_lock);
}

static void isp_capture_program_request_unpin(struct tegra_isp_channel *chan,
		uint32_t buffer_index)
{
	struct isp_capture *capture = chan->capture_data;
	struct capture_common_unpins *unpins;
	int i = 0;

	mutex_lock(&capture->program_desc_ctx.unpins_list_lock);
	unpins = &capture->program_desc_ctx.unpins_list[buffer_index];
	if (unpins->num_unpins != 0U) {
		for (i = 0; i < unpins->num_unpins; i++)
			put_mapping(capture->buffer_ctx, unpins->data[i]);
		(void)memset(unpins, 0U, sizeof(*unpins));
	}
	mutex_unlock(&capture->program_desc_ctx.unpins_list_lock);
}

int isp_capture_program_request(struct tegra_isp_channel *chan,
		struct isp_program_req *req)
{
	struct isp_capture *capture = chan->capture_data;
	struct CAPTURE_MSG capture_msg;
	int err = 0;
	struct memoryinfo_surface *meminfo;
	struct isp_program_descriptor *desc;
	uint32_t request_offset;

	if (capture == NULL) {
		dev_err(chan->isp_dev,
			"%s: isp capture uninitialized\n", __func__);
		return -ENODEV;
	}

	if (capture->channel_id == CAPTURE_CHANNEL_ISP_INVALID_ID) {
		dev_err(chan->isp_dev,
			"%s: setup channel first\n", __func__);
		return -ENODEV;
	}

	if (req == NULL) {
		dev_err(chan->isp_dev,
			"%s: Invalid program req\n", __func__);
		return -EINVAL;
	}

	if (capture->program_desc_ctx.unpins_list == NULL) {
		dev_err(chan->isp_dev, "Channel setup incomplete\n");
		return -EINVAL;
	}

	if (req->buffer_index >= capture->program_desc_ctx.queue_depth) {
		dev_err(chan->isp_dev, "buffer index is out of bound\n");
		return -EINVAL;
	}

	speculation_barrier();

	mutex_lock(&capture->reset_lock);
	if (capture->reset_capture_program_flag) {
		/* consume any pending completions when coming out of reset */
		while (try_wait_for_completion(&capture->capture_program_resp))
			; /* do nothing */
	}
	capture->reset_capture_program_flag = false;
	mutex_unlock(&capture->reset_lock);

	mutex_lock(&capture->program_desc_ctx.unpins_list_lock);
	if (capture->program_desc_ctx.unpins_list[req->buffer_index].num_unpins != 0) {
		dev_err(chan->isp_dev,
			"%s: program request is still in use by rtcpu\n",
			__func__);
		mutex_unlock(&capture->program_desc_ctx.unpins_list_lock);
		return -EBUSY;
	}

	meminfo = &((struct memoryinfo_surface *)
			capture->program_desc_ctx.requests_memoryinfo)
				[req->buffer_index];

	desc = (struct isp_program_descriptor *)
		(capture->program_desc_ctx.requests.va + req->buffer_index *
				capture->program_desc_ctx.request_size);

	/* Pushbuffer 1 is located after program desc in same ringbuffer */
	request_offset = req->buffer_index *
			capture->program_desc_ctx.request_size;

	err = capture_common_pin_and_get_iova(chan->capture_data->buffer_ctx,
		(uint32_t)(desc->isp_pb1_mem >> 32U), /* mem handle */
		((uint32_t)desc->isp_pb1_mem) + request_offset, /* offset */
		&meminfo->base_address,
		&meminfo->size,
		&capture->program_desc_ctx.unpins_list[req->buffer_index]);

	mutex_unlock(&capture->program_desc_ctx.unpins_list_lock);

	memset(&capture_msg, 0, sizeof(capture_msg));
	capture_msg.header.msg_id = CAPTURE_ISP_PROGRAM_REQUEST_REQ;
	capture_msg.header.channel_id = capture->channel_id;
	capture_msg.capture_isp_program_request_req.buffer_index =
				req->buffer_index;

	err = tegra_capture_ivc_capture_submit(&capture_msg,
			sizeof(capture_msg));
	if (err < 0) {
		dev_err(chan->isp_dev, "IVC program submit failed\n");
		goto fail;
	}
	mutex_unlock(&capture->reset_lock);

	return 0;

fail:
	mutex_unlock(&capture->reset_lock);
	isp_capture_program_request_unpin(chan, req->buffer_index);
	return err;
}

int isp_capture_program_status(struct tegra_isp_channel *chan)
{
	struct isp_capture *capture = chan->capture_data;
	int err = 0;

	if (capture == NULL) {
		dev_err(chan->isp_dev,
			 "%s: isp capture uninitialized\n", __func__);
		return -ENODEV;
	}

	if (capture->channel_id == CAPTURE_CHANNEL_ISP_INVALID_ID) {
		dev_err(chan->isp_dev,
			"%s: setup channel first\n", __func__);
		return -ENODEV;
	}

	dev_dbg(chan->isp_dev, "%s: waiting for isp program status\n",
		__func__);

	/* no timeout as an isp_program may get used for mutliple frames */
	err = wait_for_completion_killable(&capture->capture_program_resp);
	if (err < 0) {
		dev_err(chan->isp_dev,
			"no reply from camera processor\n");
		return err;
	}

	mutex_lock(&capture->reset_lock);
	if (capture->reset_capture_program_flag) {
		mutex_unlock(&capture->reset_lock);
		return -EIO;
	}
	mutex_unlock(&capture->reset_lock);

	return 0;
}

/**
 * Pin/map buffers and save iova boundaries into corresponding
 * memoryinfo struct.
 */
static int pin_isp_capture_request_buffers_locked(
		struct tegra_isp_channel *chan,
		struct isp_capture_req *req,
		struct capture_common_unpins *request_unpins)
{
	struct isp_desc_rec *capture_desc_ctx =
			&chan->capture_data->capture_desc_ctx;
	struct isp_capture_descriptor *desc = (struct isp_capture_descriptor *)
		(capture_desc_ctx->requests.va +
			req->buffer_index * capture_desc_ctx->request_size);

	struct isp_capture_descriptor_memoryinfo *desc_mem =
		&((struct isp_capture_descriptor_memoryinfo *)
			capture_desc_ctx->requests_memoryinfo)
				[req->buffer_index];

	struct capture_buffer_table *buffer_ctx =
			chan->capture_data->buffer_ctx;
	int i, j;
	int err = 0;

	/* Pushbuffer 2 is located after isp desc, in same ringbuffer */
	uint32_t request_offset = req->buffer_index *
			capture_desc_ctx->request_size;

	err = capture_common_pin_and_get_iova(buffer_ctx,
			(uint32_t)(desc->isp_pb2_mem >> 32U),
			((uint32_t)desc->isp_pb2_mem) + request_offset,
			&desc_mem->isp_pb2_mem.base_address,
			&desc_mem->isp_pb2_mem.size,
			request_unpins);

	if (err) {
		dev_err(chan->isp_dev, "%s: get pushbuffer2 iova failed\n",
			__func__);
		goto fail;
	}

	for (i = 0; i < ISP_MAX_INPUT_SURFACES; i++) {
		err = capture_common_pin_and_get_iova(buffer_ctx,
			desc->input_mr_surfaces[i].offset_hi,
			desc->input_mr_surfaces[i].offset,
			&desc_mem->input_mr_surfaces[i].base_address,
			&desc_mem->input_mr_surfaces[i].size,
			request_unpins);

		if (err) {
			dev_err(chan->isp_dev,
				"%s: get input_mr_surfaces iova failed\n",
				__func__);
			goto fail;
		}
	}

	for (i = 0; i < ISP_MAX_OUTPUTS; i++) {
		for (j = 0; j < ISP_MAX_OUTPUT_SURFACES; j++) {
			err = capture_common_pin_and_get_iova(buffer_ctx,
				desc->outputs_mw[i].surfaces[j].offset_hi,
				desc->outputs_mw[i].surfaces[j].offset,
				&desc_mem->outputs_mw[i].surfaces[j].base_address,
				&desc_mem->outputs_mw[i].surfaces[j].size,
				request_unpins);

			if (err) {
				dev_err(chan->isp_dev,
					"%s: get outputs_mw iova failed\n",
					__func__);
				goto fail;
			}
		}
	}

	/* Pin stats surfaces */
	{
		struct stats_surface *stats_surfaces[] = {
			&desc->fb_surface,	&desc->fm_surface,
			&desc->afm_surface,	&desc->lac0_surface,
			&desc->lac1_surface,	&desc->h0_surface,
			&desc->h1_surface,
			&desc->pru_bad_surface,	&desc->ltm_surface,
		};

		struct memoryinfo_surface *meminfo_surfaces[] = {
			&desc_mem->fb_surface,	&desc_mem->fm_surface,
			&desc_mem->afm_surface,	&desc_mem->lac0_surface,
			&desc_mem->lac1_surface,	&desc_mem->h0_surface,
			&desc_mem->h1_surface,
			&desc_mem->pru_bad_surface,	&desc_mem->ltm_surface,
		};

		BUILD_BUG_ON(ARRAY_SIZE(stats_surfaces) !=
				ARRAY_SIZE(meminfo_surfaces));

		for (i = 0; i < ARRAY_SIZE(stats_surfaces); i++) {
			err = capture_common_pin_and_get_iova(buffer_ctx,
					stats_surfaces[i]->offset_hi,
					stats_surfaces[i]->offset,
					&meminfo_surfaces[i]->base_address,
					&meminfo_surfaces[i]->size,
					request_unpins);
			if (err)
				goto fail;
		}
	}

	/* pin engine status surface */
	err = capture_common_pin_and_get_iova(buffer_ctx,
			desc->engine_status.offset_hi,
			desc->engine_status.offset,
			&desc_mem->engine_status.base_address,
			&desc_mem->engine_status.size,
			request_unpins);
fail:
	/* Unpin cleanup is done in isp_capture_request_unpin() */
	return err;
}

int isp_capture_request(struct tegra_isp_channel *chan,
		struct isp_capture_req *req)
{
	struct isp_capture *capture = chan->capture_data;
	struct CAPTURE_MSG capture_msg;
	uint32_t request_offset;
	int err = 0;

	if (capture == NULL) {
		dev_err(chan->isp_dev,
			"%s: isp capture uninitialized\n", __func__);
		return -ENODEV;
	}

	if (capture->channel_id == CAPTURE_CHANNEL_ISP_INVALID_ID) {
		dev_err(chan->isp_dev,
			"%s: setup channel first\n", __func__);
		return -ENODEV;
	}

	if (req == NULL) {
		dev_err(chan->isp_dev,
			"%s: Invalid req\n", __func__);
		return -EINVAL;
	}

	if (capture->capture_desc_ctx.unpins_list == NULL) {
		dev_err(chan->isp_dev, "Channel setup incomplete\n");
		return -EINVAL;
	}

	if (req->buffer_index >= capture->capture_desc_ctx.queue_depth) {
		dev_err(chan->isp_dev, "buffer index is out of bound\n");
		return -EINVAL;
	}

	speculation_barrier();

	mutex_lock(&capture->reset_lock);
	if (capture->reset_capture_flag) {
		/* consume any pending completions when coming out of reset */
		while (try_wait_for_completion(&capture->capture_resp))
			; /* do nothing */
	}
	capture->reset_capture_flag = false;
	mutex_unlock(&capture->reset_lock);

	memset(&capture_msg, 0, sizeof(capture_msg));
	capture_msg.header.msg_id = CAPTURE_ISP_REQUEST_REQ;
	capture_msg.header.channel_id = capture->channel_id;
	capture_msg.capture_isp_request_req.buffer_index = req->buffer_index;

	request_offset = req->buffer_index *
			capture->capture_desc_ctx.request_size;

	err = isp_capture_setup_inputfences(chan, req, request_offset);
	if (err < 0) {
		dev_err(chan->isp_dev, "failed to setup inputfences\n");
		goto fail;
	}

	err = isp_capture_setup_prefences(chan, req, request_offset);
	if (err < 0) {
		dev_err(chan->isp_dev, "failed to setup prefences\n");
		goto fail;
	}

	mutex_lock(&capture->capture_desc_ctx.unpins_list_lock);
	if (capture->capture_desc_ctx.unpins_list[req->buffer_index].num_unpins != 0U) {
		dev_err(chan->isp_dev,
			"%s: descriptor is still in use by rtcpu\n",
			__func__);
		mutex_unlock(&capture->capture_desc_ctx.unpins_list_lock);
		return -EBUSY;
	}

	err = pin_isp_capture_request_buffers_locked(chan, req,
		&capture->capture_desc_ctx.unpins_list[req->buffer_index]);

	mutex_unlock(&capture->capture_desc_ctx.unpins_list_lock);

	if (err < 0) {
		dev_err(chan->isp_dev, "%s failed to pin request buffers\n",
			__func__);
		goto fail;
	}

	nvhost_eventlib_log_submit(
			chan->ndev,
			capture->progress_sp.id,
			capture->progress_sp.threshold,
			arch_counter_get_cntvct());

	dev_dbg(chan->isp_dev, "%s: sending chan_id %u msg_id %u buf:%u\n",
			__func__, capture_msg.header.channel_id,
			capture_msg.header.msg_id, req->buffer_index);


	err = tegra_capture_ivc_capture_submit(&capture_msg,
			sizeof(capture_msg));
	if (err < 0) {
		dev_err(chan->isp_dev, "IVC capture submit failed\n");
		goto fail;
	}

	return 0;

fail:
	isp_capture_request_unpin(chan, req->buffer_index);
	return err;
}

int isp_capture_status(struct tegra_isp_channel *chan,
		int32_t timeout_ms)
{
	struct isp_capture *capture = chan->capture_data;
	int err = 0;

	if (capture == NULL) {
		dev_err(chan->isp_dev,
			 "%s: isp capture uninitialized\n", __func__);
		return -ENODEV;
	}

	if (capture->channel_id == CAPTURE_CHANNEL_ISP_INVALID_ID) {
		dev_err(chan->isp_dev,
			"%s: setup channel first\n", __func__);
		return -ENODEV;
	}

	if (tegra_platform_is_sim() && timeout_ms > 0) {
		dev_dbg(chan->isp_dev, "%s timeout : %d extended by 10x on VDK",
			__func__, timeout_ms);
		timeout_ms *= 10;
	}

	/* negative timeout means wait forever */
	if (timeout_ms < 0) {
		err = wait_for_completion_killable(&capture->capture_resp);
	} else {
		err = wait_for_completion_killable_timeout(
				&capture->capture_resp,
				msecs_to_jiffies(timeout_ms));
		if (err == 0) {
			dev_err(chan->isp_dev,
				"no reply from camera processor\n");
			return -ETIMEDOUT;
		}
	}

	if (err < 0) {
		dev_err(chan->isp_dev,
			"wait for capture status failed\n");
		return err;
	}

	mutex_lock(&capture->reset_lock);
	if (capture->reset_capture_flag) {
		mutex_unlock(&capture->reset_lock);
		return -EIO;
	}
	mutex_unlock(&capture->reset_lock);

	return 0;
}

int isp_capture_request_ex(struct tegra_isp_channel *chan,
		struct isp_capture_req_ex *capture_req_ex)
{
	int ret = isp_capture_request(chan, &capture_req_ex->capture_req);

	/* Handle program request if process request is successful */
	if (ret == 0 && capture_req_ex->program_req.buffer_index != U32_MAX) {
		ret = isp_capture_program_request(chan,
				&capture_req_ex->program_req);
	}

	return ret;
}

int isp_capture_set_progress_status_notifier(struct tegra_isp_channel *chan,
		struct isp_capture_progress_status_req *req)
{
	int err = 0;
	struct isp_capture *capture = chan->capture_data;

	if (req->mem == 0 ||
		req->process_buffer_depth == 0) {
		dev_err(chan->isp_dev,
				"%s: process request buffer is invalid\n",
				__func__);
		return -EINVAL;
	}

	if (req->mem == 0 ||
		req->program_buffer_depth == 0) {
		dev_err(chan->isp_dev,
				"%s: program request buffer is invalid\n",
				__func__);
		return -EINVAL;
	}

	if (capture == NULL) {
		dev_err(chan->isp_dev,
				"%s: isp capture uninitialized\n", __func__);
		return -ENODEV;
	}

	if (req->process_buffer_depth < capture->capture_desc_ctx.queue_depth) {
		dev_err(chan->isp_dev,
				"%s: Process progress status buffer smaller than queue depth\n",
				__func__);
		return -EINVAL;
	}

	if (req->program_buffer_depth < capture->program_desc_ctx.queue_depth) {
		dev_err(chan->isp_dev,
				"%s: Program progress status buffer smaller than queue depth\n",
				__func__);
		return -EINVAL;
	}

	if (req->process_buffer_depth > (U32_MAX - req->program_buffer_depth)) {
		dev_err(chan->isp_dev,
			"%s: Process and Program status buffer larger than expected\n",
			__func__);
		return -EINVAL;
	}

	if ((req->process_buffer_depth + req->program_buffer_depth) >
		(U32_MAX / sizeof(uint32_t))) {
		dev_err(chan->isp_dev,
			"%s: Process and Program status buffer larger than expected\n",
			__func__);
		return -EINVAL;
	}

	/* Setup the progress status buffer */
	err = capture_common_setup_progress_status_notifier(
			&capture->progress_status_notifier,
			req->mem,
			(req->process_buffer_depth + req->program_buffer_depth) *
				sizeof(uint32_t),
			req->mem_offset);

	if (err < 0) {
		dev_err(chan->isp_dev,
				"%s: Process progress status setup failed\n",
				__func__);
		return -EFAULT;
	}

	dev_dbg(chan->isp_dev, "Progress status mem offset %u\n",
			req->mem_offset);
	dev_dbg(chan->isp_dev, "Process buffer depth %u\n",
			req->process_buffer_depth);
	dev_dbg(chan->isp_dev, "Program buffer depth %u\n",
			req->program_buffer_depth);

	capture->capture_desc_ctx.progress_status_buffer_depth =
			req->process_buffer_depth;
	capture->program_desc_ctx.progress_status_buffer_depth =
			req->program_buffer_depth;

	capture->is_progress_status_notifier_set = true;
	return err;
}

int isp_capture_buffer_request(
	struct tegra_isp_channel *chan, struct isp_buffer_req *req)
{
	struct isp_capture *capture = chan->capture_data;
	int err;

	err = capture_buffer_request(
		capture->buffer_ctx, req->mem, req->flag);
	return err;
}
