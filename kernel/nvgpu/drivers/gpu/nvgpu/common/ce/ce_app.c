/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/os_sched.h>
#include <nvgpu/channel.h>
#include <nvgpu/watchdog.h>
#include <nvgpu/dma.h>
#include <nvgpu/utils.h>
#include <nvgpu/fence.h>
#include <nvgpu/ce_app.h>
#include <nvgpu/power_features/cg.h>

#include "common/ce/ce_priv.h"

static inline u32 nvgpu_ce_get_valid_launch_flags(struct gk20a *g,
		u32 launch_flags)
{
#ifdef CONFIG_NVGPU_DGPU
	/*
	 * there is no local memory available,
	 * don't allow local memory related CE flags
	 */
	if (g->mm.vidmem.size == 0ULL) {
		launch_flags &= ~(NVGPU_CE_SRC_LOCATION_LOCAL_FB |
			NVGPU_CE_DST_LOCATION_LOCAL_FB);
	}
#endif
	return launch_flags;
}

int nvgpu_ce_execute_ops(struct gk20a *g,
		u32 ce_ctx_id,
		u64 src_paddr,
		u64 dst_paddr,
		u64 size,
		u32 payload,
		u32 launch_flags,
		u32 request_operation,
		u32 submit_flags,
		struct nvgpu_fence_type **fence_out)
{
	int ret = -EPERM;
	struct nvgpu_ce_app *ce_app = g->ce_app;
	struct nvgpu_ce_gpu_ctx *ce_ctx, *ce_ctx_save;
	bool found = false;
	u32 *cmd_buf_cpu_va;
	u64 cmd_buf_gpu_va = 0UL;
	u32 method_size;
	u32 cmd_buf_read_offset;
	u32 dma_copy_class;
	struct nvgpu_gpfifo_entry gpfifo;
	struct nvgpu_channel_fence fence = {0U, 0U};
	struct nvgpu_fence_type *ce_cmd_buf_fence_out = NULL;

	if (!ce_app->initialised || ce_app->app_state != NVGPU_CE_ACTIVE) {
		goto end;
	}

	/* This shouldn't happen */
	if (size == 0ULL) {
		ret = -EINVAL;
		goto end;
	}

	if (request_operation != NVGPU_CE_PHYS_MODE_TRANSFER &&
	    request_operation != NVGPU_CE_MEMSET) {
		ret = -EINVAL;
		goto end;
	}

	if (src_paddr > NVGPU_CE_MAX_ADDRESS) {
		ret = -EINVAL;
		goto end;
	}

	if (dst_paddr > NVGPU_CE_MAX_ADDRESS) {
		ret = -EINVAL;
		goto end;
	}

	nvgpu_mutex_acquire(&ce_app->app_mutex);

	nvgpu_list_for_each_entry_safe(ce_ctx, ce_ctx_save,
			&ce_app->allocated_contexts, nvgpu_ce_gpu_ctx, list) {
		if (ce_ctx->ctx_id == ce_ctx_id) {
			found = true;
			break;
		}
	}

	nvgpu_mutex_release(&ce_app->app_mutex);

	if (!found) {
		ret = -EINVAL;
		goto end;
	}

	if (ce_ctx->gpu_ctx_state != NVGPU_CE_GPU_CTX_ALLOCATED) {
		ret = -ENODEV;
		goto end;
	}

	nvgpu_mutex_acquire(&ce_ctx->gpu_ctx_mutex);

	ce_ctx->cmd_buf_read_queue_offset %= NVGPU_CE_MAX_INFLIGHT_JOBS;

	cmd_buf_read_offset = (ce_ctx->cmd_buf_read_queue_offset *
			(NVGPU_CE_MAX_COMMAND_BUFF_BYTES_PER_SUBMIT /
			U32(sizeof(u32))));

	cmd_buf_cpu_va = (u32 *)ce_ctx->cmd_buf_mem.cpu_va;

	if (ce_ctx->postfences[ce_ctx->cmd_buf_read_queue_offset] != NULL) {
		struct nvgpu_fence_type **prev_post_fence =
			&ce_ctx->postfences[ce_ctx->cmd_buf_read_queue_offset];

		ret = nvgpu_fence_wait(g, *prev_post_fence,
				       nvgpu_get_poll_timeout(g));

		nvgpu_fence_put(*prev_post_fence);
		*prev_post_fence = NULL;
		if (ret != 0) {
			goto noop;
		}
	}

	cmd_buf_gpu_va = (ce_ctx->cmd_buf_mem.gpu_va +
			(u64)(cmd_buf_read_offset * sizeof(u32)));

	dma_copy_class = g->ops.get_litter_value(g, GPU_LIT_DMA_COPY_CLASS);
	method_size = nvgpu_ce_prepare_submit(src_paddr,
			dst_paddr,
			size,
			&cmd_buf_cpu_va[cmd_buf_read_offset],
			payload,
			nvgpu_ce_get_valid_launch_flags(g, launch_flags),
			request_operation,
			dma_copy_class);
	nvgpu_assert(method_size <= NVGPU_CE_MAX_COMMAND_BUFF_BYTES_PER_SUBMIT);

	if (method_size != 0U) {
		/* store the element into gpfifo */
		g->ops.pbdma.format_gpfifo_entry(g, &gpfifo,
				cmd_buf_gpu_va, method_size);

		/*
		 * take always the postfence as it is needed for protecting the
		 * ce context
		 */
		submit_flags |= NVGPU_SUBMIT_FLAGS_FENCE_GET;

		nvgpu_smp_wmb();

		ret = nvgpu_submit_channel_gpfifo_kernel(ce_ctx->ch, &gpfifo,
				1, submit_flags, &fence, &ce_cmd_buf_fence_out);

		if (ret == 0) {
			ce_ctx->postfences[ce_ctx->cmd_buf_read_queue_offset] =
				ce_cmd_buf_fence_out;
			if (fence_out != NULL) {
				nvgpu_fence_get(ce_cmd_buf_fence_out);
				*fence_out = ce_cmd_buf_fence_out;
			}

			/* Next available command buffer queue Index */
			++ce_ctx->cmd_buf_read_queue_offset;
		}
	} else {
		ret = -ENOMEM;
	}
noop:
	nvgpu_mutex_release(&ce_ctx->gpu_ctx_mutex);
end:
	return ret;
}

/* static CE app api */
static void nvgpu_ce_put_fences(struct nvgpu_ce_gpu_ctx *ce_ctx)
{
	u32 i;

	for (i = 0U; i < NVGPU_CE_MAX_INFLIGHT_JOBS; i++) {
		struct nvgpu_fence_type **fence = &ce_ctx->postfences[i];

		if (*fence != NULL) {
			nvgpu_fence_put(*fence);
		}
		*fence = NULL;
	}
}

/* caller must hold ce_app->app_mutex */
static void nvgpu_ce_delete_gpu_context_locked(struct nvgpu_ce_gpu_ctx *ce_ctx)
{
	struct nvgpu_list_node *list = &ce_ctx->list;

	ce_ctx->gpu_ctx_state = NVGPU_CE_GPU_CTX_DELETED;
	ce_ctx->tsg->abortable = true;

	nvgpu_mutex_acquire(&ce_ctx->gpu_ctx_mutex);

	if (nvgpu_mem_is_valid(&ce_ctx->cmd_buf_mem)) {
		nvgpu_ce_put_fences(ce_ctx);
		nvgpu_dma_unmap_free(ce_ctx->vm, &ce_ctx->cmd_buf_mem);
	}

	/*
	 * free the channel
	 * nvgpu_channel_close() will also unbind the channel from TSG
	 */
	nvgpu_channel_close(ce_ctx->ch);
	nvgpu_ref_put(&ce_ctx->tsg->refcount, nvgpu_tsg_release);

	/* housekeeping on app */
	if ((list->prev != NULL) && (list->next != NULL)) {
		nvgpu_list_del(list);
	}

	nvgpu_mutex_release(&ce_ctx->gpu_ctx_mutex);
	nvgpu_mutex_destroy(&ce_ctx->gpu_ctx_mutex);

	nvgpu_kfree(ce_ctx->g, ce_ctx);
}

static u32 nvgpu_prepare_ce_op(u32 *cmd_buf_cpu_va,
		u64 src_paddr, u64 dst_paddr,
		u32 width, u32 height, u32 payload,
		bool mode_transfer, u32 launch_flags)
{
	u32 launch = 0U;
	u32 methodSize = 0U;

	if (mode_transfer) {
		/* setup the source */
		cmd_buf_cpu_va[methodSize++] = 0x20028100;
		cmd_buf_cpu_va[methodSize++] = (u64_hi32(src_paddr) &
			NVGPU_CE_UPPER_ADDRESS_OFFSET_MASK);
		cmd_buf_cpu_va[methodSize++] = (u64_lo32(src_paddr) &
			NVGPU_CE_LOWER_ADDRESS_OFFSET_MASK);

		cmd_buf_cpu_va[methodSize++] = 0x20018098;
		if ((launch_flags &
		     NVGPU_CE_SRC_LOCATION_LOCAL_FB) != 0U) {
			cmd_buf_cpu_va[methodSize++] = 0x00000000;
		} else if ((launch_flags &
		     NVGPU_CE_SRC_LOCATION_NONCOHERENT_SYSMEM) != 0U) {
			cmd_buf_cpu_va[methodSize++] = 0x00000002;
		} else {
			cmd_buf_cpu_va[methodSize++] = 0x00000001;
		}

		launch |= 0x00001000U;
	} else { /* memset */
		/* Remap from component A on 1 byte wide pixels */
		cmd_buf_cpu_va[methodSize++] = 0x200181c2;
		cmd_buf_cpu_va[methodSize++] = 0x00000004;

		cmd_buf_cpu_va[methodSize++] = 0x200181c0;
		cmd_buf_cpu_va[methodSize++] = payload;

		launch |= 0x00000400U;
	}

	/* setup the destination/output */
	cmd_buf_cpu_va[methodSize++] = 0x20068102;
	cmd_buf_cpu_va[methodSize++] = (u64_hi32(dst_paddr) &
		NVGPU_CE_UPPER_ADDRESS_OFFSET_MASK);
	cmd_buf_cpu_va[methodSize++] = (u64_lo32(dst_paddr) &
		NVGPU_CE_LOWER_ADDRESS_OFFSET_MASK);
	/* Pitch in/out */
	cmd_buf_cpu_va[methodSize++] = width;
	cmd_buf_cpu_va[methodSize++] = width;
	/* width and line count */
	cmd_buf_cpu_va[methodSize++] = width;
	cmd_buf_cpu_va[methodSize++] = height;

	cmd_buf_cpu_va[methodSize++] = 0x20018099;
	if ((launch_flags & NVGPU_CE_DST_LOCATION_LOCAL_FB) != 0U) {
		cmd_buf_cpu_va[methodSize++] = 0x00000000;
	} else if ((launch_flags &
		   NVGPU_CE_DST_LOCATION_NONCOHERENT_SYSMEM) != 0U) {
		cmd_buf_cpu_va[methodSize++] = 0x00000002;
	} else {
		cmd_buf_cpu_va[methodSize++] = 0x00000001;
	}

	launch |= 0x00002005U;

	if ((launch_flags &
	     NVGPU_CE_SRC_MEMORY_LAYOUT_BLOCKLINEAR) != 0U) {
		launch |= 0x00000000U;
	} else {
		launch |= 0x00000080U;
	}

	if ((launch_flags &
	     NVGPU_CE_DST_MEMORY_LAYOUT_BLOCKLINEAR) != 0U) {
		launch |= 0x00000000U;
	} else {
		launch |= 0x00000100U;
	}

	cmd_buf_cpu_va[methodSize++] = 0x200180c0;
	cmd_buf_cpu_va[methodSize++] = launch;

	return methodSize;
}

u32 nvgpu_ce_prepare_submit(u64 src_paddr,
		u64 dst_paddr,
		u64 size,
		u32 *cmd_buf_cpu_va,
		u32 payload,
		u32 launch_flags,
		u32 request_operation,
		u32 dma_copy_class)
{
	u32 methodSize = 0;
	u64 low, hi;
	bool mode_transfer = (request_operation == NVGPU_CE_PHYS_MODE_TRANSFER);

	/* set the channel object */
	cmd_buf_cpu_va[methodSize++] = 0x20018000;
	cmd_buf_cpu_va[methodSize++] = dma_copy_class;

	/*
	 * The CE can work with 2D rectangles of at most 0xffffffff or 4G-1
	 * pixels per line. Exactly 2G is a more round number, so we'll use
	 * that as the base unit to clear large amounts of memory. If the
	 * requested size is not a multiple of 2G, we'll do one clear first to
	 * deal with the low bits, followed by another in units of 2G.
	 *
	 * We'll use 1 bytes per pixel to do byte aligned sets/copies. The
	 * maximum number of lines is also 4G-1, so (4G-1) * 2 GB is enough for
	 * whole vidmem.
	 */

	/* Lower 2GB */
	low = size & 0x7fffffffULL;
	/* Over 2GB */
	hi  = size >> 31U;

	/*
	 * Unable to fit this in one submit, but no device should have this
	 * much memory anyway.
	 */
	if (hi > 0xffffffffULL) {
		/* zero size means error */
		return 0;
	}

	if (low != 0U) {
		/* do the low bytes in one long line */
		methodSize += nvgpu_prepare_ce_op(&cmd_buf_cpu_va[methodSize],
				src_paddr, dst_paddr,
				nvgpu_safe_cast_u64_to_u32(low), 1,
				payload, mode_transfer, launch_flags);
	}
	if (hi != 0U) {
		/* do the high bytes in many 2G lines */
		methodSize += nvgpu_prepare_ce_op(&cmd_buf_cpu_va[methodSize],
				src_paddr + low, dst_paddr + low,
				0x80000000ULL, nvgpu_safe_cast_u64_to_u32(hi),
				payload, mode_transfer, launch_flags);
	}

	return methodSize;
}

/* global CE app related apis */
int nvgpu_ce_app_init_support(struct gk20a *g)
{
	struct nvgpu_ce_app *ce_app = g->ce_app;

	if (unlikely(ce_app == NULL)) {
		ce_app = nvgpu_kzalloc(g, sizeof(*ce_app));
		if (ce_app == NULL) {
			return -ENOMEM;
		}
		g->ce_app = ce_app;
	}

	if (ce_app->initialised) {
		/* assume this happen during poweron/poweroff GPU sequence */
		ce_app->app_state = NVGPU_CE_ACTIVE;
		return 0;
	}

	nvgpu_log(g, gpu_dbg_fn, "ce: init");

	nvgpu_mutex_init(&ce_app->app_mutex);

	nvgpu_mutex_acquire(&ce_app->app_mutex);

	nvgpu_init_list_node(&ce_app->allocated_contexts);
	ce_app->ctx_count = 0;
	ce_app->next_ctx_id = 0;
	ce_app->initialised = true;
	ce_app->app_state = NVGPU_CE_ACTIVE;

	nvgpu_mutex_release(&ce_app->app_mutex);

	nvgpu_log(g, gpu_dbg_cde_ctx, "ce: init finished");

	return 0;
}

void nvgpu_ce_app_destroy(struct gk20a *g)
{
	struct nvgpu_ce_app *ce_app = g->ce_app;
	struct nvgpu_ce_gpu_ctx *ce_ctx, *ce_ctx_save;

	if (ce_app == NULL) {
		return;
	}

	if (ce_app->initialised == false) {
		goto free;
	}

	ce_app->app_state = NVGPU_CE_SUSPEND;
	ce_app->initialised = false;

	nvgpu_mutex_acquire(&ce_app->app_mutex);

	nvgpu_list_for_each_entry_safe(ce_ctx, ce_ctx_save,
			&ce_app->allocated_contexts, nvgpu_ce_gpu_ctx, list) {
		nvgpu_ce_delete_gpu_context_locked(ce_ctx);
	}

	nvgpu_init_list_node(&ce_app->allocated_contexts);
	ce_app->ctx_count = 0;
	ce_app->next_ctx_id = 0;

	nvgpu_mutex_release(&ce_app->app_mutex);

	nvgpu_mutex_destroy(&ce_app->app_mutex);
free:
	nvgpu_kfree(g, ce_app);
	g->ce_app = NULL;
}

void nvgpu_ce_app_suspend(struct gk20a *g)
{
	struct nvgpu_ce_app *ce_app = g->ce_app;

	if (ce_app == NULL || !ce_app->initialised) {
		return;
	}

	ce_app->app_state = NVGPU_CE_SUSPEND;
}

/* CE app utility functions */
u32 nvgpu_ce_app_create_context(struct gk20a *g,
		u32 runlist_id,
		int timeslice,
		int runlist_level)
{
	struct nvgpu_ce_gpu_ctx *ce_ctx;
	struct nvgpu_ce_app *ce_app = g->ce_app;
	struct nvgpu_setup_bind_args setup_bind_args;
	u32 ctx_id = NVGPU_CE_INVAL_CTX_ID;
	int err = 0;

	if (!ce_app->initialised || ce_app->app_state != NVGPU_CE_ACTIVE) {
		return ctx_id;
	}

	ce_ctx = nvgpu_kzalloc(g, sizeof(*ce_ctx));
	if (ce_ctx == NULL) {
		return ctx_id;
	}

	nvgpu_mutex_init(&ce_ctx->gpu_ctx_mutex);

	ce_ctx->g = g;
	ce_ctx->cmd_buf_read_queue_offset = 0;
	ce_ctx->vm = g->mm.ce.vm;

	/* allocate a tsg if needed */
	ce_ctx->tsg = nvgpu_tsg_open(g, nvgpu_current_pid(g));
	if (ce_ctx->tsg == NULL) {
		nvgpu_err(g, "ce: gk20a tsg not available");
		goto end;
	}

	/* this TSG should never be aborted */
	ce_ctx->tsg->abortable = false;

	/* always kernel client needs privileged channel */
	ce_ctx->ch = nvgpu_channel_open_new(g, runlist_id, true,
				nvgpu_current_pid(g), nvgpu_current_tid(g));
	if (ce_ctx->ch == NULL) {
		nvgpu_err(g, "ce: gk20a channel not available");
		goto end;
	}

	nvgpu_channel_wdt_disable(ce_ctx->ch->wdt);

	/* bind the channel to the vm */
	err = g->ops.mm.vm_bind_channel(g->mm.ce.vm, ce_ctx->ch);
	if (err != 0) {
		nvgpu_err(g, "ce: could not bind vm");
		goto end;
	}

	err = nvgpu_tsg_bind_channel(ce_ctx->tsg, ce_ctx->ch);
	if (err != 0) {
		nvgpu_err(g, "ce: unable to bind to tsg");
		goto end;
	}

	setup_bind_args.num_gpfifo_entries = 1024;
	setup_bind_args.num_inflight_jobs = 0;
	setup_bind_args.flags = 0;
	err = nvgpu_channel_setup_bind(ce_ctx->ch, &setup_bind_args);
	if (err != 0) {
		nvgpu_err(g, "ce: unable to setup and bind channel");
		goto end;
	}

	/* allocate command buffer from sysmem */
	err = nvgpu_dma_alloc_map_sys(ce_ctx->vm,
			NVGPU_CE_MAX_INFLIGHT_JOBS *
			NVGPU_CE_MAX_COMMAND_BUFF_BYTES_PER_SUBMIT,
			&ce_ctx->cmd_buf_mem);
	if (err != 0) {
		nvgpu_err(g,
			"ce: alloc command buffer failed");
		goto end;
	}

	(void) memset(ce_ctx->cmd_buf_mem.cpu_va, 0x00,
		ce_ctx->cmd_buf_mem.size);

#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
	/* -1 means default channel timeslice value */
	if (timeslice != -1) {
		err = g->ops.tsg.set_timeslice(ce_ctx->tsg, timeslice);
		if (err != 0) {
			nvgpu_err(g, "ce: set timesliced failed for CE context");
			goto end;
		}
	}

	/* -1 means default channel runlist level */
	if (runlist_level != -1) {
		err = nvgpu_tsg_set_interleave(ce_ctx->tsg, runlist_level);
		if (err != 0) {
			nvgpu_err(g, "ce: set runlist interleave failed");
			goto end;
		}
	}
#endif

	nvgpu_mutex_acquire(&ce_app->app_mutex);
	ctx_id = ce_ctx->ctx_id = ce_app->next_ctx_id;
	nvgpu_list_add(&ce_ctx->list, &ce_app->allocated_contexts);
	++ce_app->next_ctx_id;
	++ce_app->ctx_count;
	nvgpu_mutex_release(&ce_app->app_mutex);

	ce_ctx->gpu_ctx_state = NVGPU_CE_GPU_CTX_ALLOCATED;

end:
	if (ctx_id == NVGPU_CE_INVAL_CTX_ID) {
		nvgpu_mutex_acquire(&ce_app->app_mutex);
		nvgpu_ce_delete_gpu_context_locked(ce_ctx);
		nvgpu_mutex_release(&ce_app->app_mutex);
	}
	return ctx_id;

}

void nvgpu_ce_app_delete_context(struct gk20a *g,
		u32 ce_ctx_id)
{
	struct nvgpu_ce_app *ce_app = g->ce_app;
	struct nvgpu_ce_gpu_ctx *ce_ctx, *ce_ctx_save;

	if (ce_app == NULL || !ce_app->initialised ||
		ce_app->app_state != NVGPU_CE_ACTIVE) {
		return;
	}

	nvgpu_mutex_acquire(&ce_app->app_mutex);

	nvgpu_list_for_each_entry_safe(ce_ctx, ce_ctx_save,
			&ce_app->allocated_contexts, nvgpu_ce_gpu_ctx, list) {
		if (ce_ctx->ctx_id == ce_ctx_id) {
			nvgpu_ce_delete_gpu_context_locked(ce_ctx);
			--ce_app->ctx_count;
			break;
		}
	}

	nvgpu_mutex_release(&ce_app->app_mutex);
}
