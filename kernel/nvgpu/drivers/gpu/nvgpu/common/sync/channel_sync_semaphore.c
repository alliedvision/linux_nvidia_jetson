/*
 * GK20A Channel Synchronization Abstraction
 *
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/atomic.h>
#include <nvgpu/bug.h>
#include <nvgpu/list.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/semaphore.h>
#include <nvgpu/os_fence.h>
#include <nvgpu/os_fence_semas.h>
#include <nvgpu/channel.h>
#include <nvgpu/channel_sync.h>
#include <nvgpu/channel_sync_semaphore.h>
#include <nvgpu/priv_cmdbuf.h>
#include <nvgpu/fence.h>
#include <nvgpu/fence_sema.h>

#include "channel_sync_priv.h"

struct nvgpu_channel_sync_semaphore {
	struct nvgpu_channel_sync base;
	struct nvgpu_channel *c;
	struct nvgpu_hw_semaphore *hw_sema;
};

static struct nvgpu_channel_sync_semaphore *
nvgpu_channel_sync_semaphore_from_base(struct nvgpu_channel_sync *base)
{
	return (struct nvgpu_channel_sync_semaphore *)
		((uintptr_t)base -
			offsetof(struct nvgpu_channel_sync_semaphore, base));
}

#ifndef CONFIG_NVGPU_SYNCFD_NONE
static void add_sema_wait_cmd(struct gk20a *g, struct nvgpu_channel *c,
			 struct nvgpu_semaphore *s, struct priv_cmd_entry *cmd)
{
	int ch = c->chid;
	u64 va;

	/* acquire just needs to read the mem. */
	va = nvgpu_semaphore_gpu_ro_va(s);

	g->ops.sync.sema.add_wait_cmd(g, cmd, s, va);
	gpu_sema_verbose_dbg(g, "(A) c=%d ACQ_GE %-4u pool=%-3llu"
			     "va=0x%llx cmd=%p",
			     ch, nvgpu_semaphore_get_value(s),
			     nvgpu_semaphore_get_hw_pool_page_idx(s),
			     va, cmd);
}

static void channel_sync_semaphore_gen_wait_cmd(struct nvgpu_channel *c,
	struct nvgpu_semaphore *sema, struct priv_cmd_entry *wait_cmd,
	u32 wait_cmd_size)
{
	bool has_incremented;

	if (sema == NULL) {
		/* came from an expired sync fence */
		nvgpu_priv_cmdbuf_append_zeros(c->g, wait_cmd, wait_cmd_size);
	} else {
		has_incremented = nvgpu_semaphore_can_wait(sema);
		nvgpu_assert(has_incremented);
		add_sema_wait_cmd(c->g, c, sema, wait_cmd);
		nvgpu_semaphore_put(sema);
	}
}
#endif

static void add_sema_incr_cmd(struct gk20a *g, struct nvgpu_channel *c,
			 struct nvgpu_semaphore *s, struct priv_cmd_entry *cmd,
			 bool wfi, struct nvgpu_hw_semaphore *hw_sema)
{
	int ch = c->chid;
	u64 va;

	/* release will need to write back to the semaphore memory. */
	va = nvgpu_semaphore_gpu_rw_va(s);

	/* find the right sema next_value to write (like syncpt's max). */
	nvgpu_semaphore_prepare(s, hw_sema);

	g->ops.sync.sema.add_incr_cmd(g, cmd, s, va, wfi);
	gpu_sema_verbose_dbg(g, "(R) c=%d INCR %u (%u) pool=%-3llu"
			     "va=0x%llx entry=%p",
			     ch, nvgpu_semaphore_get_value(s),
			     nvgpu_semaphore_read(s),
			     nvgpu_semaphore_get_hw_pool_page_idx(s),
			     va, cmd);
}

static int channel_sync_semaphore_wait_fd(
		struct nvgpu_channel_sync *s, int fd,
		struct priv_cmd_entry **entry, u32 max_wait_cmds)
{
#ifndef CONFIG_NVGPU_SYNCFD_NONE
	struct nvgpu_channel_sync_semaphore *sema =
		nvgpu_channel_sync_semaphore_from_base(s);
	struct nvgpu_channel *c = sema->c;

	struct nvgpu_os_fence os_fence = {0};
	struct nvgpu_os_fence_sema os_fence_sema = {0};
	int err;
	u32 wait_cmd_size, i, num_fences;
	struct nvgpu_semaphore *semaphore = NULL;

	err = nvgpu_os_fence_fdget(&os_fence, c, fd);
	if (err != 0) {
		return err;
	}

	err = nvgpu_os_fence_get_semas(&os_fence_sema, &os_fence);
	if (err != 0) {
		goto cleanup;
	}

	num_fences = nvgpu_os_fence_sema_get_num_semaphores(&os_fence_sema);

	if (num_fences == 0U) {
		goto cleanup;
	}

	if ((max_wait_cmds != 0U) && (num_fences > max_wait_cmds)) {
		err = -EINVAL;
		goto cleanup;
	}

	wait_cmd_size = c->g->ops.sync.sema.get_wait_cmd_size();
	err = nvgpu_priv_cmdbuf_alloc(c->priv_cmd_q,
		wait_cmd_size * num_fences, entry);
	if (err != 0) {
		goto cleanup;
	}

	for (i = 0; i < num_fences; i++) {
		nvgpu_os_fence_sema_extract_nth_semaphore(
			&os_fence_sema, i, &semaphore);
		channel_sync_semaphore_gen_wait_cmd(c, semaphore, *entry,
				wait_cmd_size);
	}

cleanup:
	os_fence.ops->drop_ref(&os_fence);
	return err;
#else
	struct nvgpu_channel_sync_semaphore *sema =
		nvgpu_channel_sync_semaphore_from_base(s);

	nvgpu_err(sema->c->g,
		  "trying to use sync fds with CONFIG_NVGPU_SYNCFD_NONE");
	return -ENODEV;
#endif
}

static int channel_sync_semaphore_incr_common(
		struct nvgpu_channel_sync *s, bool wfi_cmd,
		struct priv_cmd_entry **incr_cmd,
		struct nvgpu_fence_type *fence,
		bool need_sync_fence)
{
	u32 incr_cmd_size;
	struct nvgpu_channel_sync_semaphore *sp =
		nvgpu_channel_sync_semaphore_from_base(s);
	struct nvgpu_channel *c = sp->c;
	struct nvgpu_semaphore *semaphore;
	int err = 0;
	struct nvgpu_os_fence os_fence = {0};

	semaphore = nvgpu_semaphore_alloc(sp->hw_sema);
	if (semaphore == NULL) {
		nvgpu_err(c->g,
				"ran out of semaphores");
		return -ENOMEM;
	}

	incr_cmd_size = c->g->ops.sync.sema.get_incr_cmd_size();
	err = nvgpu_priv_cmdbuf_alloc(c->priv_cmd_q, incr_cmd_size, incr_cmd);
	if (err != 0) {
		goto clean_up_sema;
	}

	/* Release the completion semaphore. */
	add_sema_incr_cmd(c->g, c, semaphore, *incr_cmd, wfi_cmd, sp->hw_sema);

	if (need_sync_fence) {
		err = nvgpu_os_fence_sema_create(&os_fence, c, semaphore);

		if (err != 0) {
			goto clean_up_cmdbuf;
		}
	}

	nvgpu_fence_from_semaphore(fence, semaphore, &c->semaphore_wq, os_fence);

	return 0;

clean_up_cmdbuf:
	nvgpu_priv_cmdbuf_rollback(c->priv_cmd_q, *incr_cmd);
clean_up_sema:
	nvgpu_semaphore_put(semaphore);
	return err;
}

static int channel_sync_semaphore_incr(
		struct nvgpu_channel_sync *s,
		struct priv_cmd_entry **entry,
		struct nvgpu_fence_type *fence,
		bool need_sync_fence)
{
	/* Don't put wfi cmd to this one since we're not returning
	 * a fence to user space. */
	return channel_sync_semaphore_incr_common(s,
			false /* no wfi */,
			entry, fence, need_sync_fence);
}

static int channel_sync_semaphore_incr_user(
		struct nvgpu_channel_sync *s,
		struct priv_cmd_entry **entry,
		struct nvgpu_fence_type *fence,
		bool wfi,
		bool need_sync_fence)
{
#ifndef CONFIG_NVGPU_SYNCFD_NONE
	int err;

	err = channel_sync_semaphore_incr_common(s, wfi, entry, fence,
			need_sync_fence);
	if (err != 0) {
		return err;
	}

	return 0;
#else
	struct nvgpu_channel_sync_semaphore *sema =
		nvgpu_channel_sync_semaphore_from_base(s);

	nvgpu_err(sema->c->g,
		  "trying to use sync fds with CONFIG_NVGPU_SYNCFD_NONE");
	return -ENODEV;
#endif
}

static void channel_sync_semaphore_mark_progress(struct nvgpu_channel_sync *s,
		bool register_irq)
{
	struct nvgpu_channel_sync_semaphore *sp =
		nvgpu_channel_sync_semaphore_from_base(s);

	(void)nvgpu_hw_semaphore_update_next(sp->hw_sema);
	/*
	 * register_irq is ignored: there is only one semaphore interrupt that
	 * triggers nvgpu_channel_update() and it's always active.
	 */
}

static void channel_sync_semaphore_set_min_eq_max(struct nvgpu_channel_sync *s)
{
	struct nvgpu_channel_sync_semaphore *sp =
		nvgpu_channel_sync_semaphore_from_base(s);
	struct nvgpu_channel *c = sp->c;
	bool updated;

	updated = nvgpu_hw_semaphore_reset(sp->hw_sema);

	if (updated) {
		nvgpu_cond_broadcast_interruptible(&c->semaphore_wq);
	}
}

static void channel_sync_semaphore_destroy(struct nvgpu_channel_sync *s)
{
	struct nvgpu_channel_sync_semaphore *sema =
		nvgpu_channel_sync_semaphore_from_base(s);

	struct nvgpu_channel *c = sema->c;
	struct gk20a *g = c->g;

	if (c->has_os_fence_framework_support &&
		g->os_channel.os_fence_framework_inst_exists(c)) {
			g->os_channel.destroy_os_fence_framework(c);
	}
	nvgpu_hw_semaphore_free(sema->hw_sema);

	nvgpu_kfree(g, sema);
}

static const struct nvgpu_channel_sync_ops channel_sync_semaphore_ops = {
	.wait_fence_fd		= channel_sync_semaphore_wait_fd,
	.incr			= channel_sync_semaphore_incr,
	.incr_user		= channel_sync_semaphore_incr_user,
	.mark_progress		= channel_sync_semaphore_mark_progress,
	.set_min_eq_max		= channel_sync_semaphore_set_min_eq_max,
	.destroy		= channel_sync_semaphore_destroy,
};

/* Converts a valid struct nvgpu_channel_sync ptr to
 * struct nvgpu_channel_sync_syncpt ptr else return NULL.
 */
struct nvgpu_channel_sync_semaphore *
	nvgpu_channel_sync_to_semaphore(struct nvgpu_channel_sync *sync)
{
	struct nvgpu_channel_sync_semaphore *sema = NULL;
	if (sync->ops == &channel_sync_semaphore_ops) {
		sema = nvgpu_channel_sync_semaphore_from_base(sync);
	}

	return sema;
}

struct nvgpu_hw_semaphore *
nvgpu_channel_sync_semaphore_hw_sema(struct nvgpu_channel_sync_semaphore *sema)
{
	return sema->hw_sema;
}

struct nvgpu_channel_sync *
nvgpu_channel_sync_semaphore_create(struct nvgpu_channel *c)
{
	struct nvgpu_channel_sync_semaphore *sema;
	struct gk20a *g = c->g;
	int asid = -1;
	int err;

	if (c->vm == NULL) {
		nvgpu_do_assert();
		return NULL;
	}

	sema = nvgpu_kzalloc(c->g, sizeof(*sema));
	if (sema == NULL) {
		return NULL;
	}
	sema->c = c;

	err = nvgpu_hw_semaphore_init(c->vm, c->chid, &sema->hw_sema);
	if (err != 0) {
		goto err_free_sema;
	}

	if (c->vm->as_share != NULL) {
		asid = c->vm->as_share->id;
	}

	if (c->has_os_fence_framework_support) {
		/*Init the sync_timeline for this channel */
		err = g->os_channel.init_os_fence_framework(c,
			"gk20a_ch%d_as%d", c->chid, asid);

		if (err != 0) {
			goto err_free_hw_sema;
		}
	}

	nvgpu_atomic_set(&sema->base.refcount, 0);
	sema->base.ops = &channel_sync_semaphore_ops;

	return &sema->base;

err_free_hw_sema:
	nvgpu_hw_semaphore_free(sema->hw_sema);
err_free_sema:
	nvgpu_kfree(g, sema);
	return NULL;
}
