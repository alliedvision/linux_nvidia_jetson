/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <nvgpu/atomic.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/fifo.h>
#include <nvgpu/tsg.h>
#include <nvgpu/preempt.h>

#include <nvgpu/vgpu/vgpu_ivc.h>
#include <nvgpu/vgpu/vgpu.h>

#include "preempt_vgpu.h"
#include "common/vgpu/ivc/comm_vgpu.h"

int vgpu_fifo_preempt_channel(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_config_params *p =
			&msg.params.channel_config;
	int err;

	nvgpu_log_fn(g, " ");

	if (!nvgpu_atomic_read(&ch->bound)) {
		return 0;
	}

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_PREEMPT;
	msg.handle = vgpu_get_handle(g);
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));

	if (err || msg.ret) {
		nvgpu_err(g,
			"preempt channel %d failed", ch->chid);
		err = -ENOMEM;
	}

	return err;
}

int vgpu_fifo_preempt_tsg(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_tsg_preempt_params *p =
			&msg.params.tsg_preempt;
	int err;

	nvgpu_log_fn(g, " ");

	msg.cmd = TEGRA_VGPU_CMD_TSG_PREEMPT;
	msg.handle = vgpu_get_handle(g);
	p->tsg_id = tsg->tsgid;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;

	if (err) {
		nvgpu_err(g,
			"preempt tsg %u failed", tsg->tsgid);
	}

	return err;
}
