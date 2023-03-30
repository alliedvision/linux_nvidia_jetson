/*
 * GK20A Channel Synchronization Abstraction
 *
 * Copyright (c) 2014-2022, NVIDIA CORPORATION.  All rights reserved.
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

#if !defined(CONFIG_NVGPU_SYNCFD_NONE) && !defined(CONFIG_TEGRA_GK20A_NVHOST_HOST1X)
#include <uapi/linux/nvhost_ioctl.h>
#endif

#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/errata.h>
#include <nvgpu/atomic.h>
#include <nvgpu/bug.h>
#include <nvgpu/list.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/os_fence.h>
#include <nvgpu/os_fence_syncpts.h>
#include <nvgpu/channel.h>
#include <nvgpu/channel_sync.h>
#include <nvgpu/channel_sync_syncpt.h>
#include <nvgpu/priv_cmdbuf.h>
#include <nvgpu/fence.h>
#include <nvgpu/fence_syncpt.h>
#include <nvgpu/string.h>

#include "channel_sync_priv.h"

struct nvgpu_channel_sync_syncpt {
	struct nvgpu_channel_sync base;
	struct nvgpu_channel *c;
	struct nvgpu_nvhost_dev *nvhost;
	u32 id;
	struct nvgpu_mem syncpt_buf;
	u32 max_thresh;
};

static struct nvgpu_channel_sync_syncpt *
nvgpu_channel_sync_syncpt_from_base(struct nvgpu_channel_sync *base)
{
	return (struct nvgpu_channel_sync_syncpt *)
		((uintptr_t)base -
			offsetof(struct nvgpu_channel_sync_syncpt, base));
}

static void channel_sync_syncpt_gen_wait_cmd(struct nvgpu_channel *c,
	u32 id, u32 thresh, struct priv_cmd_entry *wait_cmd)
{
	nvgpu_log(c->g, gpu_dbg_info, "sp->id %d gpu va %llx",
			id, c->vm->syncpt_ro_map_gpu_va);
	c->g->ops.sync.syncpt.add_wait_cmd(c->g, wait_cmd, id, thresh,
			c->vm->syncpt_ro_map_gpu_va);
}

static int channel_sync_syncpt_wait_raw(struct nvgpu_channel_sync_syncpt *s,
		u32 id, u32 thresh, struct priv_cmd_entry **wait_cmd)
{
	struct nvgpu_channel *c = s->c;
	int err = 0;
	u32 wait_cmd_size = c->g->ops.sync.syncpt.get_wait_cmd_size();

	if (!nvgpu_nvhost_syncpt_is_valid_pt_ext(s->nvhost, id)) {
		return -EINVAL;
	}

	err = nvgpu_priv_cmdbuf_alloc(c->priv_cmd_q, wait_cmd_size, wait_cmd);
	if (err != 0) {
		return err;
	}

	channel_sync_syncpt_gen_wait_cmd(c, id, thresh, *wait_cmd);

	return 0;
}

#ifndef CONFIG_NVGPU_SYNCFD_NONE
struct gen_wait_cmd_iter_data {
	struct nvgpu_channel *c;
	struct priv_cmd_entry *wait_cmd;
};

static int gen_wait_cmd_iter(struct nvhost_ctrl_sync_fence_info info, void *d)
{
	struct gen_wait_cmd_iter_data *data = d;

	channel_sync_syncpt_gen_wait_cmd(data->c, info.id, info.thresh,
			data->wait_cmd);
	return 0;
}

static int channel_sync_syncpt_wait_fd(struct nvgpu_channel_sync *s, int fd,
	struct priv_cmd_entry **wait_cmd, u32 max_wait_cmds)
{
	struct nvgpu_os_fence os_fence = {0};
	struct nvgpu_os_fence_syncpt os_fence_syncpt = {0};
	struct nvgpu_channel_sync_syncpt *sp =
		nvgpu_channel_sync_syncpt_from_base(s);
	struct nvgpu_channel *c = sp->c;
	struct gen_wait_cmd_iter_data iter_data = {
		.c = c
	};
	u32 num_fences, wait_cmd_size;
	int err = 0;

	err = nvgpu_os_fence_fdget(&os_fence, c, fd);
	if (err != 0) {
		return -EINVAL;
	}

	err = nvgpu_os_fence_get_syncpts(&os_fence_syncpt, &os_fence);
	if (err != 0) {
		goto cleanup;
	}

	num_fences = nvgpu_os_fence_syncpt_get_num_syncpoints(&os_fence_syncpt);

	if (num_fences == 0U) {
		goto cleanup;
	}

	if ((max_wait_cmds != 0U) && (num_fences > max_wait_cmds)) {
		err = -EINVAL;
		goto cleanup;
	}

	wait_cmd_size = c->g->ops.sync.syncpt.get_wait_cmd_size();
	err = nvgpu_priv_cmdbuf_alloc(c->priv_cmd_q,
		wait_cmd_size * num_fences, wait_cmd);
	if (err != 0) {
		goto cleanup;
	}

	iter_data.wait_cmd = *wait_cmd;

	nvgpu_os_fence_syncpt_foreach_pt(&os_fence_syncpt,
			gen_wait_cmd_iter, &iter_data);

cleanup:
	os_fence.ops->drop_ref(&os_fence);
	return err;
}
#else /* CONFIG_NVGPU_SYNCFD_NONE */
static int channel_sync_syncpt_wait_fd(struct nvgpu_channel_sync *s, int fd,
	struct priv_cmd_entry **wait_cmd, u32 max_wait_cmds)
{
	struct nvgpu_channel_sync_syncpt *sp =
		nvgpu_channel_sync_syncpt_from_base(s);
	(void)s;
	(void)fd;
	(void)wait_cmd;
	(void)max_wait_cmds;
	nvgpu_err(sp->c->g,
		  "trying to use sync fds with CONFIG_NVGPU_SYNCFD_NONE");
	return -ENODEV;
}
#endif /* CONFIG_NVGPU_SYNCFD_NONE */

static void channel_sync_syncpt_update(void *priv, int nr_completed)
{
	struct nvgpu_channel *ch = priv;

	(void)nr_completed;

	nvgpu_channel_update(ch);

	/* note: channel_get() is in channel_sync_syncpt_mark_progress() */
	nvgpu_channel_put(ch);
}

static int channel_sync_syncpt_incr_common(struct nvgpu_channel_sync *s,
				       bool wfi_cmd,
				       struct priv_cmd_entry **incr_cmd,
				       struct nvgpu_fence_type *fence,
				       bool need_sync_fence)
{
	u32 thresh;
	int err;
	struct nvgpu_channel_sync_syncpt *sp =
		nvgpu_channel_sync_syncpt_from_base(s);
	struct nvgpu_channel *c = sp->c;
	struct nvgpu_os_fence os_fence = {0};
	struct gk20a *g = c->g;

	err = nvgpu_priv_cmdbuf_alloc(c->priv_cmd_q,
			g->ops.sync.syncpt.get_incr_cmd_size(wfi_cmd),
			incr_cmd);
	if (err != 0) {
		return err;
	}

	nvgpu_log(g, gpu_dbg_info, "sp->id %d gpu va %llx",
				sp->id, sp->syncpt_buf.gpu_va);
	g->ops.sync.syncpt.add_incr_cmd(g, *incr_cmd,
			sp->id, sp->syncpt_buf.gpu_va, wfi_cmd);

	thresh = nvgpu_wrapping_add_u32(sp->max_thresh,
			g->ops.sync.syncpt.get_incr_per_release());

	if (need_sync_fence) {
		err = nvgpu_os_fence_syncpt_create(&os_fence, c, sp->nvhost,
			sp->id, thresh);

		if (err != 0) {
			goto clean_up_priv_cmd;
		}
	}

	nvgpu_fence_from_syncpt(fence, sp->nvhost, sp->id, thresh, os_fence);

	return 0;

clean_up_priv_cmd:
	nvgpu_priv_cmdbuf_rollback(c->priv_cmd_q, *incr_cmd);
	return err;
}

static int channel_sync_syncpt_incr(struct nvgpu_channel_sync *s,
			      struct priv_cmd_entry **entry,
			      struct nvgpu_fence_type *fence,
			      bool need_sync_fence)
{
	/* Don't put wfi cmd to this one since we're not returning
	 * a fence to user space. */
	return channel_sync_syncpt_incr_common(s, false, entry, fence,
			need_sync_fence);
}

static int channel_sync_syncpt_incr_user(struct nvgpu_channel_sync *s,
				   struct priv_cmd_entry **entry,
				   struct nvgpu_fence_type *fence,
				   bool wfi,
				   bool need_sync_fence)
{
	/* Need to do 'wfi + host incr' since we return the fence
	 * to user space. */
	return channel_sync_syncpt_incr_common(s, wfi, entry, fence,
			need_sync_fence);
}

static void channel_sync_syncpt_mark_progress(struct nvgpu_channel_sync *s,
				   bool register_irq)
{
	struct nvgpu_channel_sync_syncpt *sp =
		nvgpu_channel_sync_syncpt_from_base(s);
	struct nvgpu_channel *c = sp->c;
	struct gk20a *g = c->g;

	sp->max_thresh = nvgpu_wrapping_add_u32(sp->max_thresh,
			g->ops.sync.syncpt.get_incr_per_release());

	if (register_irq) {
		struct nvgpu_channel *referenced = nvgpu_channel_get(c);

		WARN_ON(referenced == NULL);

		if (referenced != NULL) {
			/*
			 * note: the matching channel_put() is in
			 * channel_sync_syncpt_update() that gets called when
			 * the job completes.
			 */

			int err = nvgpu_nvhost_intr_register_notifier(
				sp->nvhost,
				sp->id, sp->max_thresh,
				channel_sync_syncpt_update, c);
			if (err != 0) {
				nvgpu_channel_put(referenced);
			}

			/*
			 * This never fails in practice. If it does, we won't
			 * be getting a completion signal to free the job
			 * resources, but maybe this succeeds on a possible
			 * subsequent submit, and the channel closure path will
			 * eventually mark everything completed anyway.
			 */
			WARN(err != 0,
			     "failed to set submit complete interrupt");
		}
	}
}

int nvgpu_channel_sync_wait_syncpt(struct nvgpu_channel_sync_syncpt *s,
	u32 id, u32 thresh, struct priv_cmd_entry **entry)
{
	return channel_sync_syncpt_wait_raw(s, id, thresh, entry);
}

static void channel_sync_syncpt_set_min_eq_max(struct nvgpu_channel_sync *s)
{
	struct nvgpu_channel_sync_syncpt *sp =
		nvgpu_channel_sync_syncpt_from_base(s);

	nvgpu_nvhost_syncpt_set_minval(sp->nvhost, sp->id, sp->max_thresh);
}

static u32 channel_sync_syncpt_get_id(struct nvgpu_channel_sync_syncpt *sp)
{
	return sp->id;
}

static void channel_sync_syncpt_destroy(struct nvgpu_channel_sync *s)
{
	struct nvgpu_channel_sync_syncpt *sp =
		nvgpu_channel_sync_syncpt_from_base(s);


	sp->c->g->ops.sync.syncpt.free_buf(sp->c, &sp->syncpt_buf);

	channel_sync_syncpt_set_min_eq_max(s);
	nvgpu_nvhost_syncpt_put_ref_ext(sp->nvhost, sp->id);
	nvgpu_kfree(sp->c->g, sp);
}

u32 nvgpu_channel_sync_get_syncpt_id(struct nvgpu_channel_sync_syncpt *s)
{
	return channel_sync_syncpt_get_id(s);
}

static const struct nvgpu_channel_sync_ops channel_sync_syncpt_ops = {
	.wait_fence_fd		= channel_sync_syncpt_wait_fd,
	.incr			= channel_sync_syncpt_incr,
	.incr_user		= channel_sync_syncpt_incr_user,
	.mark_progress		= channel_sync_syncpt_mark_progress,
	.set_min_eq_max		= channel_sync_syncpt_set_min_eq_max,
	.destroy		= channel_sync_syncpt_destroy,
};

struct nvgpu_channel_sync_syncpt *
nvgpu_channel_sync_to_syncpt(struct nvgpu_channel_sync *sync)
{
	struct nvgpu_channel_sync_syncpt *syncpt = NULL;

	if (sync->ops == &channel_sync_syncpt_ops) {
		syncpt = nvgpu_channel_sync_syncpt_from_base(sync);
	}

	return syncpt;
}

struct nvgpu_channel_sync *
nvgpu_channel_sync_syncpt_create(struct nvgpu_channel *c)
{
	struct nvgpu_channel_sync_syncpt *sp;
	char syncpt_name[32];
	int err;

	sp = nvgpu_kzalloc(c->g, sizeof(*sp));
	if (sp == NULL) {
		return NULL;
	}

	sp->c = c;
	sp->nvhost = c->g->nvhost;

	snprintf(syncpt_name, sizeof(syncpt_name),
		"%s_%d", c->g->name, c->chid);

	sp->id = nvgpu_nvhost_get_syncpt_client_managed(sp->nvhost,
					syncpt_name);

	/**
	 * This is a fix to handle invalid value of a syncpt.
	 * Once nvhost update the return value as NVGPU_INVALID_SYNCPT_ID,
	 * we can remove the zero check.
	 */
	if ((nvgpu_is_errata_present(c->g, NVGPU_ERRATA_SYNCPT_INVALID_ID_0)) &&
		(sp->id == 0U)) {
		nvgpu_err(c->g, "failed to get free syncpt");
		goto err_free;
	}
	if (sp->id == NVGPU_INVALID_SYNCPT_ID) {
		nvgpu_err(c->g, "failed to get free syncpt");
		goto err_free;
	}

	err = sp->c->g->ops.sync.syncpt.alloc_buf(sp->c, sp->id,
				&sp->syncpt_buf);

	if (err != 0) {
		nvgpu_err(c->g, "failed to allocate syncpoint buffer");
		goto err_put;
	}

	err = nvgpu_nvhost_syncpt_read_ext_check(sp->nvhost, sp->id,
			&sp->max_thresh);

	if (err != 0) {
		goto err_free_buf;
	}

	nvgpu_atomic_set(&sp->base.refcount, 0);
	sp->base.ops = &channel_sync_syncpt_ops;

	return &sp->base;

err_free_buf:
	sp->c->g->ops.sync.syncpt.free_buf(sp->c, &sp->syncpt_buf);
err_put:
	nvgpu_nvhost_syncpt_put_ref_ext(sp->nvhost, sp->id);
err_free:
	nvgpu_kfree(c->g, sp);
	return NULL;
}
