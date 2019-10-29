/*
 * GK20A Channel Synchronization Abstraction
 *
 * Copyright (c) 2014-2018, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/semaphore.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/atomic.h>
#include <nvgpu/bug.h>
#include <nvgpu/list.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/os_fence.h>

#include "channel_sync_gk20a.h"
#include "gk20a.h"
#include "fence_gk20a.h"
#include "mm_gk20a.h"

#ifdef CONFIG_TEGRA_GK20A_NVHOST

struct gk20a_channel_syncpt {
	struct gk20a_channel_sync ops;
	struct channel_gk20a *c;
	struct nvgpu_nvhost_dev *nvhost_dev;
	u32 id;
	struct nvgpu_mem syncpt_buf;
};

int gk20a_channel_gen_syncpt_wait_cmd(struct channel_gk20a *c,
	u32 id, u32 thresh, struct priv_cmd_entry *wait_cmd,
	u32 wait_cmd_size, int pos, bool preallocated)
{
	int err = 0;
	bool is_expired = nvgpu_nvhost_syncpt_is_expired_ext(
		c->g->nvhost_dev, id, thresh);

	if (is_expired) {
		if (preallocated) {
			nvgpu_memset(c->g, wait_cmd->mem,
			(wait_cmd->off + pos * wait_cmd_size) * sizeof(u32),
				0, wait_cmd_size * sizeof(u32));
		}
	} else {
		if (!preallocated) {
			err = gk20a_channel_alloc_priv_cmdbuf(c,
				c->g->ops.fifo.get_syncpt_wait_cmd_size(), wait_cmd);
			if (err) {
				nvgpu_err(c->g, "not enough priv cmd buffer space");
				return err;
			}
		}
		nvgpu_log(c->g, gpu_dbg_info, "sp->id %d gpu va %llx",
				id, c->vm->syncpt_ro_map_gpu_va);
		c->g->ops.fifo.add_syncpt_wait_cmd(c->g, wait_cmd,
			pos * wait_cmd_size, id, thresh,
			c->vm->syncpt_ro_map_gpu_va);
	}

	return 0;
}

static int gk20a_channel_syncpt_wait_syncpt(struct gk20a_channel_sync *s,
		u32 id, u32 thresh, struct priv_cmd_entry *wait_cmd)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	struct channel_gk20a *c = sp->c;
	int err = 0;
	u32 wait_cmd_size = c->g->ops.fifo.get_syncpt_wait_cmd_size();

	if (!nvgpu_nvhost_syncpt_is_valid_pt_ext(sp->nvhost_dev, id))
		return -EINVAL;

	err = gk20a_channel_gen_syncpt_wait_cmd(c, id, thresh,
			wait_cmd, wait_cmd_size, 0, false);

	return err;
}

static int gk20a_channel_syncpt_wait_fd(struct gk20a_channel_sync *s, int fd,
	struct priv_cmd_entry *wait_cmd, int max_wait_cmds)
{
	struct nvgpu_os_fence os_fence = {0};
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	struct channel_gk20a *c = sp->c;
	int err = 0;

	err = nvgpu_os_fence_fdget(&os_fence, c, fd);
	if (err)
		return -EINVAL;

	err = os_fence.ops->program_waits(&os_fence,
		wait_cmd, c, max_wait_cmds);

	os_fence.ops->drop_ref(&os_fence);

	return err;
}

static void gk20a_channel_syncpt_update(void *priv, int nr_completed)
{
	struct channel_gk20a *ch = priv;

	gk20a_channel_update(ch);

	/* note: channel_get() is in __gk20a_channel_syncpt_incr() */
	gk20a_channel_put(ch);
}

static int __gk20a_channel_syncpt_incr(struct gk20a_channel_sync *s,
				       bool wfi_cmd,
				       bool register_irq,
				       struct priv_cmd_entry *incr_cmd,
				       struct gk20a_fence *fence,
				       bool need_sync_fence)
{
	u32 thresh;
	int err;
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	struct channel_gk20a *c = sp->c;
	struct nvgpu_os_fence os_fence = {0};

	err = gk20a_channel_alloc_priv_cmdbuf(c,
			c->g->ops.fifo.get_syncpt_incr_cmd_size(wfi_cmd),
			incr_cmd);
	if (err)
		return err;

	nvgpu_log(c->g, gpu_dbg_info, "sp->id %d gpu va %llx",
				sp->id, sp->syncpt_buf.gpu_va);
	c->g->ops.fifo.add_syncpt_incr_cmd(c->g, wfi_cmd,
			incr_cmd, sp->id, sp->syncpt_buf.gpu_va);

	thresh = nvgpu_nvhost_syncpt_incr_max_ext(sp->nvhost_dev, sp->id,
			c->g->ops.fifo.get_syncpt_incr_per_release());

	if (register_irq) {
		struct channel_gk20a *referenced = gk20a_channel_get(c);

		WARN_ON(!referenced);

		if (referenced) {
			/* note: channel_put() is in
			 * gk20a_channel_syncpt_update() */

			err = nvgpu_nvhost_intr_register_notifier(
				sp->nvhost_dev,
				sp->id, thresh,
				gk20a_channel_syncpt_update, c);
			if (err)
				gk20a_channel_put(referenced);

			/* Adding interrupt action should
			 * never fail. A proper error handling
			 * here would require us to decrement
			 * the syncpt max back to its original
			 * value. */
			WARN(err,
			     "failed to set submit complete interrupt");
		}
	}

	if (need_sync_fence) {
		err = nvgpu_os_fence_syncpt_create(&os_fence, c, sp->nvhost_dev,
			sp->id, thresh);

		if (err)
			goto clean_up_priv_cmd;
	}

	err = gk20a_fence_from_syncpt(fence, sp->nvhost_dev,
	 sp->id, thresh, os_fence);

	if (err) {
		if (nvgpu_os_fence_is_initialized(&os_fence))
			os_fence.ops->drop_ref(&os_fence);
		goto clean_up_priv_cmd;
	}

	return 0;

clean_up_priv_cmd:
	gk20a_free_priv_cmdbuf(c, incr_cmd);
	return err;
}

static int gk20a_channel_syncpt_incr_wfi(struct gk20a_channel_sync *s,
				  struct priv_cmd_entry *entry,
				  struct gk20a_fence *fence)
{
	return __gk20a_channel_syncpt_incr(s,
			true /* wfi */,
			false /* no irq handler */,
			entry, fence, true);
}

static int gk20a_channel_syncpt_incr(struct gk20a_channel_sync *s,
			      struct priv_cmd_entry *entry,
			      struct gk20a_fence *fence,
			      bool need_sync_fence,
			      bool register_irq)
{
	/* Don't put wfi cmd to this one since we're not returning
	 * a fence to user space. */
	return __gk20a_channel_syncpt_incr(s,
			false /* no wfi */,
			register_irq /* register irq */,
			entry, fence, need_sync_fence);
}

static int gk20a_channel_syncpt_incr_user(struct gk20a_channel_sync *s,
				   int wait_fence_fd,
				   struct priv_cmd_entry *entry,
				   struct gk20a_fence *fence,
				   bool wfi,
				   bool need_sync_fence,
				   bool register_irq)
{
	/* Need to do 'wfi + host incr' since we return the fence
	 * to user space. */
	return __gk20a_channel_syncpt_incr(s,
			wfi,
			register_irq /* register irq */,
			entry, fence, need_sync_fence);
}

static void gk20a_channel_syncpt_set_min_eq_max(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	nvgpu_nvhost_syncpt_set_min_eq_max_ext(sp->nvhost_dev, sp->id);
}

static void gk20a_channel_syncpt_set_safe_state(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	nvgpu_nvhost_syncpt_set_safe_state(sp->nvhost_dev, sp->id);
}

static int gk20a_channel_syncpt_id(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	return sp->id;
}

static u64 gk20a_channel_syncpt_address(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);
	return sp->syncpt_buf.gpu_va;
}

static void gk20a_channel_syncpt_destroy(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_syncpt *sp =
		container_of(s, struct gk20a_channel_syncpt, ops);


	sp->c->g->ops.fifo.free_syncpt_buf(sp->c, &sp->syncpt_buf);

	nvgpu_nvhost_syncpt_set_min_eq_max_ext(sp->nvhost_dev, sp->id);
	nvgpu_nvhost_syncpt_put_ref_ext(sp->nvhost_dev, sp->id);
	nvgpu_kfree(sp->c->g, sp);
}

static struct gk20a_channel_sync *
gk20a_channel_syncpt_create(struct channel_gk20a *c, bool user_managed)
{
	struct gk20a_channel_syncpt *sp;
	char syncpt_name[32];

	sp = nvgpu_kzalloc(c->g, sizeof(*sp));
	if (!sp)
		return NULL;

	sp->c = c;
	sp->nvhost_dev = c->g->nvhost_dev;

	if (user_managed) {
		snprintf(syncpt_name, sizeof(syncpt_name),
			"%s_%d_user", c->g->name, c->chid);

		sp->id = nvgpu_nvhost_get_syncpt_client_managed(sp->nvhost_dev,
						syncpt_name);
	} else {
		snprintf(syncpt_name, sizeof(syncpt_name),
			"%s_%d", c->g->name, c->chid);

		sp->id = nvgpu_nvhost_get_syncpt_host_managed(sp->nvhost_dev,
						c->chid, syncpt_name);
	}
	if (!sp->id) {
		nvgpu_kfree(c->g, sp);
		nvgpu_err(c->g, "failed to get free syncpt");
		return NULL;
	}

	sp->c->g->ops.fifo.alloc_syncpt_buf(sp->c, sp->id,
				&sp->syncpt_buf);

	nvgpu_nvhost_syncpt_set_min_eq_max_ext(sp->nvhost_dev, sp->id);

	nvgpu_atomic_set(&sp->ops.refcount, 0);
	sp->ops.wait_syncpt		= gk20a_channel_syncpt_wait_syncpt;
	sp->ops.wait_fd			= gk20a_channel_syncpt_wait_fd;
	sp->ops.incr			= gk20a_channel_syncpt_incr;
	sp->ops.incr_wfi		= gk20a_channel_syncpt_incr_wfi;
	sp->ops.incr_user		= gk20a_channel_syncpt_incr_user;
	sp->ops.set_min_eq_max		= gk20a_channel_syncpt_set_min_eq_max;
	sp->ops.set_safe_state		= gk20a_channel_syncpt_set_safe_state;
	sp->ops.syncpt_id		= gk20a_channel_syncpt_id;
	sp->ops.syncpt_address		= gk20a_channel_syncpt_address;
	sp->ops.destroy			= gk20a_channel_syncpt_destroy;

	return &sp->ops;
}
#endif /* CONFIG_TEGRA_GK20A_NVHOST */

struct gk20a_channel_semaphore {
	struct gk20a_channel_sync ops;
	struct channel_gk20a *c;

	/* A semaphore pool owned by this channel. */
	struct nvgpu_semaphore_pool *pool;
};

static void add_sema_cmd(struct gk20a *g, struct channel_gk20a *c,
			 struct nvgpu_semaphore *s, struct priv_cmd_entry *cmd,
			 u32 offset, bool acquire, bool wfi)
{
	int ch = c->chid;
	u32 ob, off = cmd->off + offset;
	u64 va;

	ob = off;

	/*
	 * RO for acquire (since we just need to read the mem) and RW for
	 * release since we will need to write back to the semaphore memory.
	 */
	va = acquire ? nvgpu_semaphore_gpu_ro_va(s) :
		       nvgpu_semaphore_gpu_rw_va(s);

	/*
	 * If the op is not an acquire (so therefor a release) we should
	 * incr the underlying sema next_value.
	 */
	if (!acquire)
		nvgpu_semaphore_prepare(s, c->hw_sema);

	g->ops.fifo.add_sema_cmd(g, s, va, cmd, off, acquire, wfi);

	if (acquire)
		gpu_sema_verbose_dbg(g, "(A) c=%d ACQ_GE %-4u pool=%-3d"
				     "va=0x%llx cmd_mem=0x%llx b=0x%llx off=%u",
				     ch, nvgpu_semaphore_get_value(s),
				     s->location.pool->page_idx, va, cmd->gva,
				     cmd->mem->gpu_va, ob);
	else
		gpu_sema_verbose_dbg(g, "(R) c=%d INCR %u (%u) pool=%-3d"
				     "va=0x%llx cmd_mem=0x%llx b=0x%llx off=%u",
				     ch, nvgpu_semaphore_get_value(s),
				     nvgpu_semaphore_read(s),
				     s->location.pool->page_idx,
				     va, cmd->gva, cmd->mem->gpu_va, ob);
}

void gk20a_channel_gen_sema_wait_cmd(struct channel_gk20a *c,
	struct nvgpu_semaphore *sema, struct priv_cmd_entry *wait_cmd,
	u32 wait_cmd_size, int pos)
{
	if (!sema) {
		/* expired */
		nvgpu_memset(c->g, wait_cmd->mem,
			(wait_cmd->off + pos * wait_cmd_size) * sizeof(u32),
			0, wait_cmd_size * sizeof(u32));
	} else {
		WARN_ON(!sema->incremented);
		add_sema_cmd(c->g, c, sema, wait_cmd,
			pos * wait_cmd_size, true, false);
		nvgpu_semaphore_put(sema);
	}
}

static int gk20a_channel_semaphore_wait_syncpt(
		struct gk20a_channel_sync *s, u32 id,
		u32 thresh, struct priv_cmd_entry *entry)
{
	struct gk20a_channel_semaphore *sema =
		container_of(s, struct gk20a_channel_semaphore, ops);
	struct gk20a *g = sema->c->g;
	nvgpu_err(g, "trying to use syncpoint synchronization");
	return -ENODEV;
}

static int gk20a_channel_semaphore_wait_fd(
		struct gk20a_channel_sync *s, int fd,
		struct priv_cmd_entry *entry, int max_wait_cmds)
{
	struct gk20a_channel_semaphore *sema =
		container_of(s, struct gk20a_channel_semaphore, ops);
	struct channel_gk20a *c = sema->c;

	struct nvgpu_os_fence os_fence = {0};
	int err;

	err = nvgpu_os_fence_fdget(&os_fence, c, fd);
	if (err)
		return err;

	err = os_fence.ops->program_waits(&os_fence,
		entry, c, max_wait_cmds);

	os_fence.ops->drop_ref(&os_fence);

	return err;
}

static int __gk20a_channel_semaphore_incr(
		struct gk20a_channel_sync *s, bool wfi_cmd,
		struct priv_cmd_entry *incr_cmd,
		struct gk20a_fence *fence,
		bool need_sync_fence)
{
	int incr_cmd_size;
	struct gk20a_channel_semaphore *sp =
		container_of(s, struct gk20a_channel_semaphore, ops);
	struct channel_gk20a *c = sp->c;
	struct nvgpu_semaphore *semaphore;
	int err = 0;
	struct nvgpu_os_fence os_fence = {0};

	semaphore = nvgpu_semaphore_alloc(c);
	if (!semaphore) {
		nvgpu_err(c->g,
				"ran out of semaphores");
		return -ENOMEM;
	}

	incr_cmd_size = c->g->ops.fifo.get_sema_incr_cmd_size();
	err = gk20a_channel_alloc_priv_cmdbuf(c, incr_cmd_size, incr_cmd);
	if (err) {
		nvgpu_err(c->g,
				"not enough priv cmd buffer space");
		goto clean_up_sema;
	}

	/* Release the completion semaphore. */
	add_sema_cmd(c->g, c, semaphore, incr_cmd, 0, false, wfi_cmd);

	if (need_sync_fence) {
		err = nvgpu_os_fence_sema_create(&os_fence, c,
			semaphore);

		if (err)
			goto clean_up_sema;
	}

	err = gk20a_fence_from_semaphore(fence,
		semaphore,
		&c->semaphore_wq,
		os_fence);

	if (err) {
		if (nvgpu_os_fence_is_initialized(&os_fence))
			os_fence.ops->drop_ref(&os_fence);
		goto clean_up_sema;
	}

	return 0;

clean_up_sema:
	nvgpu_semaphore_put(semaphore);
	return err;
}

static int gk20a_channel_semaphore_incr_wfi(
		struct gk20a_channel_sync *s,
		struct priv_cmd_entry *entry,
		struct gk20a_fence *fence)
{
	return __gk20a_channel_semaphore_incr(s,
			true /* wfi */,
			entry, fence, true);
}

static int gk20a_channel_semaphore_incr(
		struct gk20a_channel_sync *s,
		struct priv_cmd_entry *entry,
		struct gk20a_fence *fence,
		bool need_sync_fence,
		bool register_irq)
{
	/* Don't put wfi cmd to this one since we're not returning
	 * a fence to user space. */
	return __gk20a_channel_semaphore_incr(s,
			false /* no wfi */,
			entry, fence, need_sync_fence);
}

static int gk20a_channel_semaphore_incr_user(
		struct gk20a_channel_sync *s,
		int wait_fence_fd,
		struct priv_cmd_entry *entry,
		struct gk20a_fence *fence,
		bool wfi,
		bool need_sync_fence,
		bool register_irq)
{
#ifdef CONFIG_SYNC
	int err;

	err = __gk20a_channel_semaphore_incr(s, wfi, entry, fence,
			need_sync_fence);
	if (err)
		return err;

	return 0;
#else
	struct gk20a_channel_semaphore *sema =
		container_of(s, struct gk20a_channel_semaphore, ops);
	nvgpu_err(sema->c->g,
		  "trying to use sync fds with CONFIG_SYNC disabled");
	return -ENODEV;
#endif
}

static void gk20a_channel_semaphore_set_min_eq_max(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_semaphore *sp =
		container_of(s, struct gk20a_channel_semaphore, ops);
	struct channel_gk20a *c = sp->c;
	bool updated;

	if (!c->hw_sema)
		return;

	updated = nvgpu_semaphore_reset(c->hw_sema);

	if (updated)
		nvgpu_cond_broadcast_interruptible(&c->semaphore_wq);
}

static void gk20a_channel_semaphore_set_safe_state(struct gk20a_channel_sync *s)
{
	/* Nothing to do. */
}

static int gk20a_channel_semaphore_syncpt_id(struct gk20a_channel_sync *s)
{
	return -EINVAL;
}

static u64 gk20a_channel_semaphore_syncpt_address(struct gk20a_channel_sync *s)
{
	return 0;
}

static void gk20a_channel_semaphore_destroy(struct gk20a_channel_sync *s)
{
	struct gk20a_channel_semaphore *sema =
		container_of(s, struct gk20a_channel_semaphore, ops);

	struct channel_gk20a *c = sema->c;
	struct gk20a *g = c->g;

	if (c->has_os_fence_framework_support &&
		g->os_channel.os_fence_framework_inst_exists(c))
			g->os_channel.destroy_os_fence_framework(c);

	/* The sema pool is cleaned up by the VM destroy. */
	sema->pool = NULL;

	nvgpu_kfree(sema->c->g, sema);
}

static struct gk20a_channel_sync *
gk20a_channel_semaphore_create(struct channel_gk20a *c, bool user_managed)
{
	struct gk20a_channel_semaphore *sema;
	struct gk20a *g = c->g;
	char pool_name[20];
	int asid = -1;
	int err;

	if (WARN_ON(!c->vm))
		return NULL;

	sema = nvgpu_kzalloc(c->g, sizeof(*sema));
	if (!sema)
		return NULL;
	sema->c = c;

	sprintf(pool_name, "semaphore_pool-%d", c->chid);
	sema->pool = c->vm->sema_pool;

	if (c->vm->as_share)
		asid = c->vm->as_share->id;

	if (c->has_os_fence_framework_support) {
		/*Init the sync_timeline for this channel */
		err = g->os_channel.init_os_fence_framework(c,
			"gk20a_ch%d_as%d", c->chid, asid);

		if (err) {
			nvgpu_kfree(g, sema);
			return NULL;
		}
	}

	nvgpu_atomic_set(&sema->ops.refcount, 0);
	sema->ops.wait_syncpt	= gk20a_channel_semaphore_wait_syncpt;
	sema->ops.wait_fd	= gk20a_channel_semaphore_wait_fd;
	sema->ops.incr		= gk20a_channel_semaphore_incr;
	sema->ops.incr_wfi	= gk20a_channel_semaphore_incr_wfi;
	sema->ops.incr_user	= gk20a_channel_semaphore_incr_user;
	sema->ops.set_min_eq_max = gk20a_channel_semaphore_set_min_eq_max;
	sema->ops.set_safe_state = gk20a_channel_semaphore_set_safe_state;
	sema->ops.syncpt_id	= gk20a_channel_semaphore_syncpt_id;
	sema->ops.syncpt_address = gk20a_channel_semaphore_syncpt_address;
	sema->ops.destroy	= gk20a_channel_semaphore_destroy;

	return &sema->ops;
}

void gk20a_channel_sync_destroy(struct gk20a_channel_sync *sync,
	bool set_safe_state)
{
	if (set_safe_state)
		sync->set_safe_state(sync);
	sync->destroy(sync);
}

struct gk20a_channel_sync *gk20a_channel_sync_create(struct channel_gk20a *c,
	bool user_managed)
{
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	if (gk20a_platform_has_syncpoints(c->g))
		return gk20a_channel_syncpt_create(c, user_managed);
#endif
	return gk20a_channel_semaphore_create(c, user_managed);
}

bool gk20a_channel_sync_needs_sync_framework(struct gk20a *g)
{
	return !gk20a_platform_has_syncpoints(g);
}
