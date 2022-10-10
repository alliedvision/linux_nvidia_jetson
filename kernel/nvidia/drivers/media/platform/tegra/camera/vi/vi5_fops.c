/*
 * Tegra Video Input 5 device common APIs
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Frank Chen <frank@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/nvhost.h>
#include <linux/semaphore.h>
#include <linux/version.h>
#include <media/tegra_camera_platform.h>
#include <media/mc_common.h>
#include <media/tegra-v4l2-camera.h>
#include <media/fusa-capture/capture-vi-channel.h>
#include <media/fusa-capture/capture-vi.h>
#include <soc/tegra/camrtc-capture.h>
#include <uapi/linux/nvhost_ioctl.h>
#include "nvhost_acm.h"
#include "vi5_formats.h"
#include "vi5_fops.h"
#include <trace/events/camera_common.h>

#define DEFAULT_FRAMERATE	30
#define BPP_MEM			2
#define VI_CSI_CLK_SCALE	110
#define PG_BITRATE		32
#define SLVSEC_STREAM_MAIN	0U

static const struct vi_capture_setup default_setup = {
	.channel_flags = 0
	| CAPTURE_CHANNEL_FLAG_VIDEO
	| CAPTURE_CHANNEL_FLAG_RAW
	| CAPTURE_CHANNEL_FLAG_EMBDATA
	| CAPTURE_CHANNEL_FLAG_LINETIMER
	,

	.vi_channel_mask = ~0ULL,
	.vi2_channel_mask = ~0ULL,

	.queue_depth = CAPTURE_MIN_BUFFERS,
	.request_size = sizeof(struct capture_descriptor),
	.mem = 0,  /* fill in later */
};

static const struct capture_descriptor capture_template = {
	.sequence = 0,

	.capture_flags = 0
	| CAPTURE_FLAG_STATUS_REPORT_ENABLE
	| CAPTURE_FLAG_ERROR_REPORT_ENABLE
	,

	.ch_cfg = {
		.pixfmt_enable = 0,		/* no output */
		.match = {
			.stream = 0,		/* one-hot bit encoding */
			.stream_mask = 0x3f,
			.vc = (1u << 0),	/* one-hot bit encoding */
			.vc_mask = 0xffff,
		},
	},
};

static void vi5_init_video_formats(struct tegra_channel *chan)
{
	int i;

	chan->num_video_formats = ARRAY_SIZE(vi5_video_formats);
	for (i = 0; i < chan->num_video_formats; i++)
		chan->video_formats[i] = &vi5_video_formats[i];
}

static int tegra_vi5_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct tegra_channel *chan = container_of(ctrl->handler,
				struct tegra_channel, ctrl_handler);
	struct v4l2_subdev *sd = chan->subdev_on_csi;
	struct camera_common_data *s_data =
				to_camera_common_data(sd->dev);
	struct tegracam_ctrl_handler *handler;
	struct tegracam_sensor_data *sensor_data;

	if (!s_data)
		return -EINVAL;
	handler = s_data->tegracam_ctrl_hdl;
	if (!handler)
		return -EINVAL;
	sensor_data = &handler->sensor_data;

	/* TODO: Support reading blobs for multiple devices */
	switch (ctrl->id) {
	case TEGRA_CAMERA_CID_SENSOR_CONFIG: {
		struct sensor_cfg *cfg = &s_data->sensor_props.cfg;

		memcpy(ctrl->p_new.p, cfg, sizeof(struct sensor_cfg));
		break;
	}
	case TEGRA_CAMERA_CID_SENSOR_MODE_BLOB: {
		struct sensor_blob *blob = &sensor_data->mode_blob;

		memcpy(ctrl->p_new.p, blob, sizeof(struct sensor_blob));
		break;
	}
	case TEGRA_CAMERA_CID_SENSOR_CONTROL_BLOB: {
		struct sensor_blob *blob = &sensor_data->ctrls_blob;

		memcpy(ctrl->p_new.p, blob, sizeof(struct sensor_blob));
		break;
	}
	default:
		pr_err("%s: unknown ctrl id.\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int tegra_vi5_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct tegra_channel *chan = container_of(ctrl->handler,
				struct tegra_channel, ctrl_handler);
	int err = 0;

	switch (ctrl->id) {
	case TEGRA_CAMERA_CID_WRITE_ISPFORMAT:
		chan->write_ispformat = ctrl->val;
		break;
	default:
		dev_err(&chan->video->dev, "%s:Not valid ctrl\n", __func__);
		return -EINVAL;
	}

	return err;
}

static const struct v4l2_ctrl_ops vi5_ctrl_ops = {
	.s_ctrl	= tegra_vi5_s_ctrl,
	.g_volatile_ctrl = tegra_vi5_g_volatile_ctrl,
};

static const struct v4l2_ctrl_config vi5_custom_ctrls[] = {
	{
		.ops = &vi5_ctrl_ops,
		.id = TEGRA_CAMERA_CID_WRITE_ISPFORMAT,
		.name = "Write ISP format",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 1,
		.min = 1,
		.max = 1,
		.step = 1,
	},
	{
		.ops = &vi5_ctrl_ops,
		.id = TEGRA_CAMERA_CID_SENSOR_CONFIG,
		.name = "Sensor configuration",
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_READ_ONLY |
			V4L2_CTRL_FLAG_HAS_PAYLOAD |
			V4L2_CTRL_FLAG_VOLATILE,
		.min = 0,
		.max = 0xFFFFFFFF,
		.def = 0,
		.step = 1,
		.dims = { SENSOR_CONFIG_SIZE },
	},
	{
		.ops = &vi5_ctrl_ops,
		.id = TEGRA_CAMERA_CID_SENSOR_MODE_BLOB,
		.name = "Sensor mode I2C packet",
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_READ_ONLY |
			V4L2_CTRL_FLAG_HAS_PAYLOAD |
			V4L2_CTRL_FLAG_VOLATILE,
		.min = 0,
		.max = 0xFFFFFFFF,
		.def = 0,
		.step = 1,
		.dims = { SENSOR_MODE_BLOB_SIZE },
	},
	{
		.ops = &vi5_ctrl_ops,
		.id = TEGRA_CAMERA_CID_SENSOR_CONTROL_BLOB,
		.name = "Sensor control I2C packet",
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_READ_ONLY |
			V4L2_CTRL_FLAG_HAS_PAYLOAD |
			V4L2_CTRL_FLAG_VOLATILE,
		.min = 0,
		.max = 0xFFFFFFFF,
		.def = 0,
		.step = 1,
		.dims = { SENSOR_CTRL_BLOB_SIZE },
	},
};

static int vi5_add_ctrls(struct tegra_channel *chan)
{
	//int i;

	/* Add vi5 custom controls */
	/*for (i = 0; i < ARRAY_SIZE(vi5_custom_ctrls); i++) {
		v4l2_ctrl_new_custom(&chan->ctrl_handler,
			&vi5_custom_ctrls[i], NULL);
		if (chan->ctrl_handler.error) {
			dev_err(chan->vi->dev,
				"Failed to add %s ctrl\n",
				vi5_custom_ctrls[i].name);
			return chan->ctrl_handler.error;
		}
	}*/

	return 0;
}

static int vi5_channel_setup_queue(struct tegra_channel *chan,
	unsigned int *nbuffers)
{
	int ret = 0;

	*nbuffers = clamp(*nbuffers, CAPTURE_MIN_BUFFERS, CAPTURE_MAX_BUFFERS);

	ret = tegra_channel_alloc_buffer_queue(chan, *nbuffers);
	if (ret < 0)
		goto done;

	chan->capture_reqs_enqueued = 0;

done:
	return ret;
}
static void vi5_bypass_datatype(struct tegra_channel *chan,
	struct capture_descriptor *desc)
{
	u32 data_type = chan->fmtinfo->img_dt;

	if(chan->bypass_dt) {
		desc->ch_cfg.match.datatype = 0x0;
		desc->ch_cfg.match.datatype_mask = 0x0;
		desc->ch_cfg.dt_enable = 1;
		desc->ch_cfg.dt_override = data_type;
	} else {
		desc->ch_cfg.match.datatype = data_type;
		desc->ch_cfg.match.datatype_mask = 0x3f;
		desc->ch_cfg.dt_enable = 0;
	}
}

static struct tegra_csi_channel *find_linked_csi_channel(
	struct tegra_channel *chan)
{
	struct tegra_csi_channel *csi_it;
	struct tegra_csi_channel *csi_chan = NULL;
	int i;

	struct tegra_csi_device *csi = tegra_get_mc_csi();
	if (csi == NULL)
	{
		dev_err(chan->vi->dev, "csi mc not found");
		return NULL;
	}
	/* Find connected csi_channel */
	list_for_each_entry(csi_it, &csi->csi_chans, list) {
		for (i = 0; i < chan->num_subdevs; i++) {
			if (chan->subdev[i] == &csi_it->subdev) {
				csi_chan = csi_it;
				break;
			}
		}
	}
	return csi_chan;
}

static int tegra_channel_capture_setup(struct tegra_channel *chan, unsigned int vi_port)
{
	struct vi_capture_setup setup = default_setup;
	long err;

	setup.queue_depth = chan->capture_queue_depth;

	trace_tegra_channel_capture_setup(chan, 0);

	chan->request[vi_port] = dma_alloc_coherent(chan->tegra_vi_channel[vi_port]->rtcpu_dev,
					setup.queue_depth * setup.request_size,
					&setup.iova, GFP_KERNEL);
	if (chan->request[vi_port] == NULL) {
		dev_err(chan->vi->dev, "dma_alloc_coherent failed\n");
		return -ENOMEM;
	}

	if (chan->is_slvsec) {
		setup.channel_flags |= CAPTURE_CHANNEL_FLAG_SLVSEC;
		setup.slvsec_stream_main = SLVSEC_STREAM_MAIN;
		setup.slvsec_stream_sub = SLVSEC_STREAM_DISABLED;
	}

	/* Set the NVCSI PixelParser index (Stream ID) and VC ID*/
	setup.csi_stream_id = chan->port[vi_port];
	setup.virtual_channel_id = chan->virtual_channel;
	/* Set CSI port info */
	if (chan->pg_mode) {
		setup.csi_port = NVCSI_PORT_UNSPECIFIED;
	} else {
		struct tegra_csi_channel *csi_chan = find_linked_csi_channel(chan);

		if (csi_chan == NULL)
		{
			dev_err(chan->vi->dev, "csi_chan not found");
			return -EINVAL;
		}

		setup.csi_port = csi_chan->ports[vi_port].csi_port;
	}

	err = vi_capture_setup(chan->tegra_vi_channel[vi_port], &setup);
	if (err) {
		dev_err(chan->vi->dev, "vi capture setup failed\n");
		dma_free_coherent(chan->tegra_vi_channel[vi_port]->rtcpu_dev,
				setup.queue_depth * setup.request_size,
				chan->request, setup.iova);
		return err;
	}

	return 0;
}

static void vi5_setup_surface(struct tegra_channel *chan,
	struct tegra_channel_buffer *buf, unsigned int descr_index, unsigned int vi_port)
{
	dma_addr_t offset = buf->addr + chan->buffer_offset[vi_port];
	u32 height = chan->format.height;
	u32 width = chan->format.width;
	u32 format = chan->fmtinfo->img_fmt;
	u32 bpl = chan->format.bytesperline;

	u32 nvcsi_stream = chan->port[vi_port];
	struct capture_descriptor_memoryinfo *desc_memoryinfo =
		&chan->tegra_vi_channel[vi_port]->
		capture_data->requests_memoryinfo[descr_index];
	struct capture_descriptor *desc = &chan->request[vi_port][descr_index];

	if (chan->valid_ports > NVCSI_STREAM_1) {
		height = chan->gang_height;
		width = chan->gang_width;
		offset = buf->addr + chan->buffer_offset[1 - vi_port];
	}

	memcpy(desc, &capture_template, sizeof(capture_template));
	memset(desc_memoryinfo, 0, sizeof(*desc_memoryinfo));

	desc->sequence = chan->capture_descr_sequence;
	desc->ch_cfg.match.stream = (1u << nvcsi_stream); /* one-hot bit encoding */
	desc->ch_cfg.match.vc = (1u << chan->virtual_channel); /* one-hot bit encoding */
	desc->ch_cfg.frame.frame_x = width;
	desc->ch_cfg.frame.frame_y = height;
	desc->ch_cfg.pixfmt_enable = 1;
	desc->ch_cfg.pixfmt.format = format;

	vi5_bypass_datatype(chan, desc);

	desc_memoryinfo->surface[0].base_address = offset;
	desc_memoryinfo->surface[0].size = chan->format.bytesperline * height;
	desc->ch_cfg.atomp.surface_stride[0] = bpl;

	if (chan->embedded_data_height > 0) {
		desc->ch_cfg.embdata_enable = 1;
		desc->ch_cfg.frame.embed_x = chan->embedded_data_width * BPP_MEM;
		desc->ch_cfg.frame.embed_y = chan->embedded_data_height;

		desc_memoryinfo->surface[VI_ATOMP_SURFACE_EMBEDDED].base_address
			= chan->emb_buf;
		desc_memoryinfo->surface[VI_ATOMP_SURFACE_EMBEDDED].size
			= desc->ch_cfg.frame.embed_x * desc->ch_cfg.frame.embed_y;

		desc->ch_cfg.atomp.surface_stride[VI_ATOMP_SURFACE_EMBEDDED]
			= chan->embedded_data_width * BPP_MEM;
	}

	chan->capture_descr_sequence += 1;
}

static void vi5_release_buffer(struct tegra_channel *chan,
	struct tegra_channel_buffer *buf)
{
	struct vb2_v4l2_buffer *vbuf = &buf->buf;

	vbuf->sequence = chan->sequence++;
	vbuf->field = V4L2_FIELD_NONE;
	vb2_set_plane_payload(&vbuf->vb2_buf, 0, chan->format.sizeimage);

	vb2_buffer_done(&vbuf->vb2_buf, buf->vb2_state);
}

static void vi5_capture_enqueue(struct tegra_channel *chan,
	struct tegra_channel_buffer *buf)
{
	int err = 0;
	int vi_port;
	unsigned long flags;
	struct tegra_mc_vi *vi = chan->vi;
	struct vi_capture_req request[2] = {{
		.buffer_index = 0,
	}, {
		.buffer_index = 0,
	}};

	for (vi_port = 0; vi_port < chan->valid_ports; vi_port++) {
		vi5_setup_surface(chan, buf, chan->capture_descr_index, vi_port);
		request[vi_port].buffer_index = chan->capture_descr_index;

		err = vi_capture_request(chan->tegra_vi_channel[vi_port], &request[vi_port]);

		if (err) {
			dev_err(vi->dev, "uncorr_err: request dispatch err %d\n", err);
			goto uncorr_err;
		}

		spin_lock_irqsave(&chan->capture_state_lock, flags);
		if (chan->capture_state != CAPTURE_ERROR) {
			chan->capture_state = CAPTURE_GOOD;
			chan->capture_reqs_enqueued += 1;
		}
		spin_unlock_irqrestore(&chan->capture_state_lock, flags);
		buf->capture_descr_index[vi_port] = chan->capture_descr_index;
	}
	chan->capture_descr_index = ((chan->capture_descr_index + 1)
					% (chan->capture_queue_depth));

	spin_lock(&chan->dequeue_lock);
	list_add_tail(&buf->queue, &chan->dequeue);
	spin_unlock(&chan->dequeue_lock);

	wake_up_interruptible(&chan->dequeue_wait);

	return;

uncorr_err:
	spin_lock_irqsave(&chan->capture_state_lock, flags);
	chan->capture_state = CAPTURE_ERROR;
	spin_unlock_irqrestore(&chan->capture_state_lock, flags);
}

static void vi5_capture_dequeue(struct tegra_channel *chan,
	struct tegra_channel_buffer *buf)
{
	int err = 0;
	int vi_port = 0;
	int gang_prev_frame_id = 0;
	unsigned long flags;
	struct tegra_mc_vi *vi = chan->vi;
	struct vb2_v4l2_buffer *vb = &buf->buf;

#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
	struct timespec ts;
#else
	struct timespec64 ts;
#endif

	struct capture_descriptor *descr = NULL;

	for (vi_port = 0; vi_port < chan->valid_ports; vi_port++) {
		descr = &chan->request[vi_port][buf->capture_descr_index[vi_port]];

		if (buf->vb2_state != VB2_BUF_STATE_ACTIVE)
			goto rel_buf;

		/* Dequeue a frame and check its capture status */
		err = vi_capture_status(chan->tegra_vi_channel[vi_port], jiffies_to_msecs(chan->timeout));
		if (err) {
			if (err == -ETIMEDOUT) {
				dev_err(vi->dev,
					"uncorr_err: request timed out after %d ms\n",
					jiffies_to_msecs(chan->timeout));
			} else {
				dev_err(vi->dev, "uncorr_err: request err %d\n", err);
			}
			goto uncorr_err;
		} else if (descr->status.status != CAPTURE_STATUS_SUCCESS) {
			if ((descr->status.flags
					& CAPTURE_STATUS_FLAG_CHANNEL_IN_ERROR) != 0) {
				chan->queue_error = true;
				dev_err(vi->dev, "uncorr_err: flags %d, err_data %d\n",
					descr->status.flags, descr->status.err_data);
			} else {
				dev_warn(vi->dev,
					"corr_err: discarding frame %d, flags: %d, "
					"err_data %d\n",
					descr->status.frame_id, descr->status.flags,
					descr->status.err_data);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
					buf->vb2_state = VB2_BUF_STATE_REQUEUEING;
#else
					buf->vb2_state = VB2_BUF_STATE_ERROR;
#endif
			goto done;
			}
		} else if (!vi_port) {
			gang_prev_frame_id = descr->status.frame_id;
		} else if (descr->status.frame_id != gang_prev_frame_id) {
			dev_err(vi->dev, "frame_id out of sync: ch2 %d vs ch1 %d\n",
					gang_prev_frame_id, descr->status.frame_id);
			goto uncorr_err;
		}

		spin_lock_irqsave(&chan->capture_state_lock, flags);
		if (chan->capture_state != CAPTURE_ERROR) {
			chan->capture_reqs_enqueued -= 1;
			chan->capture_state = CAPTURE_GOOD;
		}
		spin_unlock_irqrestore(&chan->capture_state_lock, flags);
	}

	wake_up_interruptible(&chan->start_wait);
	/* Read SOF from capture descriptor */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	ts = ns_to_timespec((s64)descr->status.sof_timestamp);
#else
	ts = ns_to_timespec64((s64)descr->status.sof_timestamp);
#endif
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
	trace_tegra_channel_capture_frame("sof", ts);
#else
	trace_tegra_channel_capture_frame("sof", &ts);
#endif
	vb->vb2_buf.timestamp = descr->status.sof_timestamp;

	buf->vb2_state = VB2_BUF_STATE_DONE;
	/* Read EOF from capture descriptor */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	ts = ns_to_timespec((s64)descr->status.eof_timestamp);
#else
	ts = ns_to_timespec64((s64)descr->status.eof_timestamp);
#endif
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
	trace_tegra_channel_capture_frame("eof", ts);
#else
	trace_tegra_channel_capture_frame("eof", &ts);
#endif

done:
	goto rel_buf;

uncorr_err:
	spin_lock_irqsave(&chan->capture_state_lock, flags);
	chan->capture_state = CAPTURE_ERROR;
	spin_unlock_irqrestore(&chan->capture_state_lock, flags);

	buf->vb2_state = VB2_BUF_STATE_ERROR;

rel_buf:
	tegra_channel_update_statistics(chan);
	vi5_release_buffer(chan, buf);
}

static int vi5_channel_error_recover(struct tegra_channel *chan,
	bool queue_error)
{
	int err = 0;
	int vi_port = 0;
	struct tegra_channel_buffer *buf;
	struct tegra_mc_vi *vi = chan->vi;
	struct v4l2_subdev *csi_subdev;

	/* stop vi channel */
	for (vi_port = 0; vi_port < chan->valid_ports; vi_port++) {
		err = vi_capture_release(chan->tegra_vi_channel[vi_port],
			CAPTURE_CHANNEL_RESET_FLAG_IMMEDIATE);
		if (err) {
			dev_err(&chan->video->dev, "vi capture release failed\n");
			goto done;
		}
		vi_channel_close_ex(chan->id, chan->tegra_vi_channel[vi_port]);
		chan->tegra_vi_channel[vi_port] = NULL;
	}


	/* release all previously-enqueued capture buffers to v4l2 */
	while (!list_empty(&chan->capture)) {
		buf = dequeue_buffer(chan, false);
		if (!buf)
			break;
		vb2_buffer_done(&buf->buf.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	while (!list_empty(&chan->dequeue)) {
		buf = dequeue_dequeue_buffer(chan);
		if (!buf)
			break;
		buf->vb2_state = VB2_BUF_STATE_ERROR;
		vi5_capture_dequeue(chan, buf);
	}

	/* report queue error to application */
	if (queue_error)
		vb2_queue_error(&chan->queue);

	/* reset nvcsi stream */
	csi_subdev = tegra_channel_find_linked_csi_subdev(chan);
	if (!csi_subdev) {
		dev_err(vi->dev, "unable to find linked csi subdev\n");
		err = -1;
		goto done;
	}

	v4l2_subdev_call(csi_subdev, core, sync,
		V4L2_SYNC_EVENT_SUBDEV_ERROR_RECOVER);

	/* restart vi channel */
	for (vi_port = 0; vi_port < chan->valid_ports; vi_port++) {
		chan->tegra_vi_channel[vi_port] = vi_channel_open_ex(chan->id + vi_port, false);
		if (IS_ERR(chan->tegra_vi_channel[vi_port])) {
			err = PTR_ERR(chan);
			goto done;
		}
		err = tegra_channel_capture_setup(chan, vi_port);
		if (err < 0)
			goto done;
	}

	chan->sequence = 0;
	tegra_channel_init_ring_buffer(chan);

	chan->capture_reqs_enqueued = 0;

	/* clear capture channel error state */
	chan->capture_state = CAPTURE_IDLE;

done:
	return err;
}

static int tegra_channel_kthread_capture_enqueue(void *data)
{
	struct tegra_channel *chan = data;
	struct tegra_channel_buffer *buf;
	unsigned long flags;
	set_freezable();

	while (1) {
		try_to_freeze();

		wait_event_interruptible(chan->start_wait,
			(kthread_should_stop() || !list_empty(&chan->capture)));

		while (!(kthread_should_stop() || list_empty(&chan->capture))) {
			spin_lock_irqsave(&chan->capture_state_lock, flags);
			if ((chan->capture_state == CAPTURE_ERROR)
					|| !(chan->capture_reqs_enqueued
					< (chan->capture_queue_depth * chan->valid_ports))) {
				spin_unlock_irqrestore(
					&chan->capture_state_lock, flags);
				break;
			}
			spin_unlock_irqrestore(&chan->capture_state_lock,
				flags);

			buf = dequeue_buffer(chan, false);
			if (!buf)
				break;

			buf->vb2_state = VB2_BUF_STATE_ACTIVE;

			vi5_capture_enqueue(chan, buf);
		}

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int tegra_channel_kthread_capture_dequeue(void *data)
{
	int err = 0;
	unsigned long flags;
	struct tegra_channel *chan = data;
	struct tegra_channel_buffer *buf;

	set_freezable();

	while (1) {
		try_to_freeze();

		wait_event_interruptible(chan->dequeue_wait,
			(kthread_should_stop()
				|| !list_empty(&chan->dequeue)
				|| (chan->capture_state == CAPTURE_ERROR)));

		while (!(kthread_should_stop() || list_empty(&chan->dequeue)
				|| (chan->capture_state == CAPTURE_ERROR))) {

			buf = dequeue_dequeue_buffer(chan);
			if (!buf)
				break;

			vi5_capture_dequeue(chan, buf);
		}

		spin_lock_irqsave(&chan->capture_state_lock, flags);
		if (chan->capture_state == CAPTURE_ERROR) {
			spin_unlock_irqrestore(&chan->capture_state_lock,
				flags);
			err = tegra_channel_error_recover(chan, false);
			if (err) {
				dev_err(chan->vi->dev,
					"fatal: error recovery failed\n");
				break;
			}
		} else
			spin_unlock_irqrestore(&chan->capture_state_lock,
				flags);
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int vi5_channel_start_kthreads(struct tegra_channel *chan)
{
	int err = 0;

	/* Start the kthread for capture enqueue */
	if (chan->kthread_capture_start) {
		dev_err(chan->vi->dev, "enqueue kthread already initialized\n");
		err = -1;
		goto done;
	}
	chan->kthread_capture_start = kthread_run(
		tegra_channel_kthread_capture_enqueue, chan, chan->video->name);
	if (IS_ERR(chan->kthread_capture_start)) {
		dev_err(&chan->video->dev,
			"failed to run kthread for capture enqueue\n");
		err = PTR_ERR(chan->kthread_capture_start);
		goto done;
	}

	/* Start the kthread for capture dequeue */
	if (chan->kthread_capture_dequeue) {
		dev_err(chan->vi->dev, "dequeue kthread already initialized\n");
		err = -1;
		goto done;
	}
	chan->kthread_capture_dequeue = kthread_run(
		tegra_channel_kthread_capture_dequeue, chan, chan->video->name);
	if (IS_ERR(chan->kthread_capture_dequeue)) {
		dev_err(&chan->video->dev,
			"failed to run kthread for capture dequeue\n");
		err = PTR_ERR(chan->kthread_capture_dequeue);
		goto done;
	}

done:
	return err;
}

static void vi5_channel_stop_kthreads(struct tegra_channel *chan)
{
	mutex_lock(&chan->stop_kthread_lock);

	/* Stop the kthread for capture enqueue */
	if (chan->kthread_capture_start) {
		kthread_stop(chan->kthread_capture_start);
		chan->kthread_capture_start = NULL;
	}

	/* Stop the kthread for capture dequeue */
	if (chan->kthread_capture_dequeue) {
		kthread_stop(chan->kthread_capture_dequeue);
		chan->kthread_capture_dequeue = NULL;
	}

	mutex_unlock(&chan->stop_kthread_lock);
}

static void vi5_unit_get_device_handle(struct platform_device *pdev,
		uint32_t csi_stream_id, struct device **dev)
{
	if (dev)
		*dev = vi_csi_stream_to_nvhost_device(pdev, csi_stream_id);
	else
		dev_err(&pdev->dev, "dev pointer is NULL\n");
}

static int vi5_channel_start_streaming(struct vb2_queue *vq, u32 count)
{
	struct tegra_channel *chan = vb2_get_drv_priv(vq);
	/* WAR: With newer version pipe init has some race condition */
	/* TODO: resolve this issue to block userspace not to cleanup media */
	int ret = 0;
	int vi_port = 0;
	unsigned long flags;
	struct v4l2_subdev *sd;
	struct device_node *node;
	struct sensor_mode_properties *sensor_mode;
	struct camera_common_data *s_data;
	unsigned int emb_buf_size = 0;

	/* Skip in bypass mode */
	if (!chan->bypass) {
		for (vi_port = 0; vi_port < chan->valid_ports; vi_port++) {
			chan->tegra_vi_channel[vi_port] =
					vi_channel_open_ex(chan->id + vi_port, false);
			if (IS_ERR(chan->tegra_vi_channel[vi_port])) {
				ret = PTR_ERR(chan);
				goto err_open_ex;
			}
			spin_lock_irqsave(&chan->capture_state_lock, flags);
			chan->capture_state = CAPTURE_IDLE;
			spin_unlock_irqrestore(&chan->capture_state_lock, flags);

			if (!chan->pg_mode) {
				sd = chan->subdev_on_csi;
				node = sd->dev->of_node;
				s_data = to_camera_common_data(sd->dev);

				/* get sensor properties from DT */
				if (s_data != NULL && node != NULL) {
					int idx = s_data->mode_prop_idx;

					emb_buf_size = 0;
					if (idx < s_data->sensor_props.\
								num_modes) {
						sensor_mode =
							&s_data->sensor_props.\
							sensor_modes[idx];

						chan->embedded_data_width =
							sensor_mode->\
							image_properties.width;
						chan->embedded_data_height =
							sensor_mode->\
							 image_properties.\
						      embedded_metadata_height;
						/* rounding up to page size */
						emb_buf_size =
							round_up(chan->\
							embedded_data_width *
								chan->\
							embedded_data_height *
							BPP_MEM, PAGE_SIZE);
					}
				}
				/* Allocate buffer for Embedded Data if need to*/
				if (emb_buf_size > chan->emb_buf_size) {
					struct device *vi_unit_dev;

					vi5_unit_get_device_handle(\
						chan->vi->ndev, chan->port[0],\
						&vi_unit_dev);
				/*
				 * if old buffer is smaller than what we need,
				 * release the old buffer and re-allocate a
				 * bigger one below.
				 */
					if (chan->emb_buf_size > 0) {
						dma_free_coherent(vi_unit_dev,
							chan->emb_buf_size,
							chan->emb_buf_addr,
							chan->emb_buf);
						chan->emb_buf_size = 0;
					}

					chan->emb_buf_addr =
						dma_alloc_coherent(vi_unit_dev,
							emb_buf_size,
						&chan->emb_buf, GFP_KERNEL);
					if (!chan->emb_buf_addr) {
						dev_err(&chan->video->dev,
							"Can't allocate memory"
							"for embedded data\n");
						goto err_setup;
					}
					chan->emb_buf_size = emb_buf_size;
				}
			}
			ret = tegra_channel_capture_setup(chan, vi_port);
			if (ret < 0)
				goto err_setup;
		}
		chan->sequence = 0;
		tegra_channel_init_ring_buffer(chan);

		ret = vi5_channel_start_kthreads(chan);
		if (ret != 0)
			goto err_start_kthreads;
	}

	/* csi stream/sensor devices should be streamon post vi channel setup */
	ret = tegra_channel_set_stream(chan, true);
	if (ret < 0)
		goto err_set_stream;

	ret = tegra_channel_write_blobs(chan);
	if (ret < 0)
		goto err_write_blobs;

	return 0;

err_write_blobs:
	tegra_channel_set_stream(chan, false);

err_set_stream:
	if (!chan->bypass)
		vi5_channel_stop_kthreads(chan);

err_start_kthreads:
	if (!chan->bypass)
		for (vi_port = 0; vi_port < chan->valid_ports; vi_port++)
			vi_capture_release(chan->tegra_vi_channel[vi_port],
				CAPTURE_CHANNEL_RESET_FLAG_IMMEDIATE);

err_setup:
	if (!chan->bypass)
		for (vi_port = 0; vi_port < chan->valid_ports; vi_port++) {
			vi_channel_close_ex(chan->id, chan->tegra_vi_channel[vi_port]);
			chan->tegra_vi_channel[vi_port] = NULL;
		}

err_open_ex:
	vq->start_streaming_called = 0;
	tegra_channel_queued_buf_done(chan, VB2_BUF_STATE_QUEUED, false);

	return ret;
}

static int vi5_channel_stop_streaming(struct vb2_queue *vq)
{
	struct tegra_channel *chan = vb2_get_drv_priv(vq);
	long err;
	int vi_port = 0;

	if (!chan->bypass) {
        for (vi_port = 0; vi_port < chan->valid_ports; vi_port++) {
            vi_stop_waiting(chan->tegra_vi_channel[vi_port]);
        }
        vi5_channel_stop_kthreads(chan);
    }

	/* csi stream/sensor(s) devices to be closed before vi channel */
	tegra_channel_set_stream(chan, false);

	if (!chan->bypass) {
		for (vi_port = 0; vi_port < chan->valid_ports; vi_port++) {
			err = vi_capture_release(chan->tegra_vi_channel[vi_port],
				CAPTURE_CHANNEL_RESET_FLAG_IMMEDIATE);

			if (err)
				dev_err(&chan->video->dev,
					"vi capture release failed\n");

			vi_channel_close_ex(chan->id + vi_port, chan->tegra_vi_channel[vi_port]);
			chan->tegra_vi_channel[vi_port] = NULL;
		}

		/* release all remaining buffers to v4l2 */
		tegra_channel_queued_buf_done(chan, VB2_BUF_STATE_ERROR, false);
	}

	return 0;
}

int tegra_vi5_enable(struct tegra_mc_vi *vi)
{
	int ret;

	ret = tegra_camera_emc_clk_enable();
	if (ret)
		goto err_emc_enable;

	return 0;

err_emc_enable:
	return ret;
}

void tegra_vi5_disable(struct tegra_mc_vi *vi)
{
	tegra_channel_ec_close(vi);
	tegra_camera_emc_clk_disable();
}

static int vi5_power_on(struct tegra_channel *chan)
{
	int ret = 0;
	struct tegra_mc_vi *vi;
	struct tegra_csi_device *csi;

	vi = chan->vi;
	csi = vi->csi;

	ret = tegra_vi5_enable(vi);
	if (ret < 0)
		return ret;

	ret = tegra_channel_set_power(chan, 1);
	if (ret < 0) {
		dev_err(vi->dev, "Failed to power on subdevices\n");
		return ret;
	}

	return 0;
}

static void vi5_power_off(struct tegra_channel *chan)
{
	int ret = 0;
	struct tegra_mc_vi *vi;
	struct tegra_csi_device *csi;

	vi = chan->vi;
	csi = vi->csi;

	ret = tegra_channel_set_power(chan, 0);
	if (ret < 0)
		dev_err(vi->dev, "Failed to power off subdevices\n");

	tegra_vi5_disable(vi);
}

struct tegra_vi_fops vi5_fops = {
	.vi_power_on = vi5_power_on,
	.vi_power_off = vi5_power_off,
	.vi_start_streaming = vi5_channel_start_streaming,
	.vi_stop_streaming = vi5_channel_stop_streaming,
	.vi_setup_queue = vi5_channel_setup_queue,
	.vi_error_recover = vi5_channel_error_recover,
	.vi_add_ctrls = vi5_add_ctrls,
	.vi_init_video_formats = vi5_init_video_formats,
	.vi_unit_get_device_handle = vi5_unit_get_device_handle,
};
