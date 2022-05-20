/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/runlist.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/vgpu/vgpu_ivc.h>
#include <nvgpu/vgpu/vgpu.h>

#include "common/vgpu/ivc/comm_vgpu.h"
#include "channel_vgpu.h"

void vgpu_channel_bind(struct nvgpu_channel *ch)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_config_params *p =
			&msg.params.channel_config;
	int err;
	struct gk20a *g = ch->g;

	nvgpu_log_info(g, "bind channel %d", ch->chid);

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_BIND;
	msg.handle = vgpu_get_handle(ch->g);
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);

	nvgpu_smp_wmb();
	nvgpu_atomic_set(&ch->bound, true);
}

void vgpu_channel_unbind(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;

	nvgpu_log_fn(g, " ");

	if (nvgpu_atomic_cmpxchg(&ch->bound, true, false)) {
		struct tegra_vgpu_cmd_msg msg;
		struct tegra_vgpu_channel_config_params *p =
				&msg.params.channel_config;
		int err;

		msg.cmd = TEGRA_VGPU_CMD_CHANNEL_UNBIND;
		msg.handle = vgpu_get_handle(ch->g);
		p->handle = ch->virt_ctx;
		err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
		WARN_ON(err || msg.ret);
	}

}

int vgpu_channel_alloc_inst(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_hwctx_params *p = &msg.params.channel_hwctx;
	int err;

	nvgpu_log_fn(g, " ");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_ALLOC_HWCTX;
	msg.handle = vgpu_get_handle(g);
	p->id = ch->chid;
	p->runlist_id = ch->runlist->id;
	p->pid = (u64)ch->pid;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	if (err || msg.ret) {
		nvgpu_err(g, "fail");
		return -ENOMEM;
	}

	ch->virt_ctx = p->handle;
	nvgpu_log_fn(g, "done");
	return 0;
}

void vgpu_channel_free_inst(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_hwctx_params *p = &msg.params.channel_hwctx;
	int err;

	nvgpu_log_fn(g, " ");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_FREE_HWCTX;
	msg.handle = vgpu_get_handle(g);
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}

void vgpu_channel_enable(struct nvgpu_channel *ch)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_config_params *p =
			&msg.params.channel_config;
	int err;
	struct gk20a *g = ch->g;

	nvgpu_log_fn(g, " ");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_ENABLE;
	msg.handle = vgpu_get_handle(ch->g);
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}

void vgpu_channel_disable(struct nvgpu_channel *ch)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_channel_config_params *p =
			&msg.params.channel_config;
	int err;
	struct gk20a *g = ch->g;

	nvgpu_log_fn(g, " ");

	msg.cmd = TEGRA_VGPU_CMD_CHANNEL_DISABLE;
	msg.handle = vgpu_get_handle(ch->g);
	p->handle = ch->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}

u32 vgpu_channel_count(struct gk20a *g)
{
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);

	return priv->constants.num_channels;
}

void vgpu_channel_set_ctx_mmu_error(struct gk20a *g, struct nvgpu_channel *ch)
{
	/*
	 * If error code is already set, this mmu fault
	 * was triggered as part of recovery from other
	 * error condition.
	 * Don't overwrite error flag.
	 */
	g->ops.channel.set_error_notifier(ch,
		NVGPU_ERR_NOTIFIER_FIFO_ERROR_MMU_ERR_FLT);

	/* mark channel as faulted */
	nvgpu_channel_set_unserviceable(ch);

	/* unblock pending waits */
	nvgpu_cond_broadcast_interruptible(&ch->semaphore_wq);
	nvgpu_cond_broadcast_interruptible(&ch->notifier_wq);
}

void vgpu_channel_set_error_notifier(struct gk20a *g,
				struct tegra_vgpu_channel_set_error_notifier *p)
{
	struct nvgpu_channel *ch;

	if (p->chid >= g->fifo.num_channels) {
		nvgpu_err(g, "invalid chid %d", p->chid);
		return;
	}

	ch = &g->fifo.channel[p->chid];
	g->ops.channel.set_error_notifier(ch, p->error);
}

void vgpu_channel_abort_cleanup(struct gk20a *g, u32 chid)
{
	struct nvgpu_channel *ch = nvgpu_channel_from_id(g, chid);

	if (ch == NULL) {
		nvgpu_err(g, "invalid channel id %d", chid);
		return;
	}

	nvgpu_channel_set_unserviceable(ch);
	g->ops.channel.abort_clean_up(ch);
	nvgpu_channel_put(ch);
}
