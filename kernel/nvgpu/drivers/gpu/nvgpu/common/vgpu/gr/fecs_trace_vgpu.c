/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/bug.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gr/fecs_trace.h>
#include <nvgpu/dt.h>
#include <nvgpu/vgpu/vgpu_ivm.h>
#include <nvgpu/vgpu/tegra_vgpu.h>
#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/gk20a.h>

#include "fecs_trace_vgpu.h"
#include "common/vgpu/ivc/comm_vgpu.h"

int vgpu_fecs_trace_init(struct gk20a *g)
{
	struct vgpu_fecs_trace *vcst;
	u32 mempool;
	int err;

	nvgpu_log_fn(g, " ");

	if (g->fecs_trace) {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_FECS_CTXSW_TRACE, false);
		return 0;
	}

	vcst = nvgpu_kzalloc(g, sizeof(*vcst));
	if (!vcst) {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_FECS_CTXSW_TRACE, false);
		return -ENOMEM;
	}

	err = nvgpu_dt_read_u32_index(g, "mempool-fecs-trace", 1, &mempool);
	if (err) {
		nvgpu_info(g, "does not support fecs trace");
		nvgpu_set_enabled(g, NVGPU_SUPPORT_FECS_CTXSW_TRACE, false);
		goto fail;
	}

	vcst->cookie = vgpu_ivm_mempool_reserve(mempool);
	if ((vcst->cookie == NULL) ||
		((unsigned long)vcst->cookie >= (unsigned long)-MAX_ERRNO)) {
		nvgpu_info(g,
			"mempool  %u reserve failed", mempool);
		vcst->cookie = NULL;
		err = -EINVAL;
		goto fail;
	}

	vcst->buf = vgpu_ivm_mempool_map(vcst->cookie);
	if (!vcst->buf) {
		nvgpu_info(g, "ioremap_cache failed");
		err = -EINVAL;
		goto fail;
	}
	vcst->header = vcst->buf;
	vcst->num_entries = vcst->header->num_ents;
	if (unlikely(vcst->header->ent_size != sizeof(*vcst->entries))) {
		nvgpu_err(g, "entry size mismatch");
		goto fail;
	}
	vcst->entries = (struct nvgpu_gpu_ctxsw_trace_entry *)(
			(char *)vcst->buf + sizeof(*vcst->header));
	g->fecs_trace = (struct nvgpu_gr_fecs_trace *)vcst;

	return 0;
fail:
	if (vcst->cookie != NULL && vcst->buf != NULL) {
		vgpu_ivm_mempool_unmap(vcst->cookie, vcst->buf);
	}
	if (vcst->cookie) {
		vgpu_ivm_mempool_unreserve(vcst->cookie);
	}
	nvgpu_kfree(g, vcst);
	return err;
}

int vgpu_fecs_trace_deinit(struct gk20a *g)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;

	vgpu_ivm_mempool_unmap(vcst->cookie, vcst->buf);
	vgpu_ivm_mempool_unreserve(vcst->cookie);
	nvgpu_kfree(g, vcst);
	return 0;
}

int vgpu_fecs_trace_enable(struct gk20a *g)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;
	struct tegra_vgpu_cmd_msg msg = {
		.cmd = TEGRA_VGPU_CMD_FECS_TRACE_ENABLE,
		.handle = vgpu_get_handle(g),
	};
	int err;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);
	vcst->enabled = !err;
	return err;
}

int vgpu_fecs_trace_disable(struct gk20a *g)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;
	struct tegra_vgpu_cmd_msg msg = {
		.cmd = TEGRA_VGPU_CMD_FECS_TRACE_DISABLE,
		.handle = vgpu_get_handle(g),
	};
	int err;

	vcst->enabled = false;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);
	return err;
}

bool vgpu_fecs_trace_is_enabled(struct gk20a *g)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;

	return (vcst && vcst->enabled);
}

int vgpu_fecs_trace_poll(struct gk20a *g)
{
	struct tegra_vgpu_cmd_msg msg = {
		.cmd = TEGRA_VGPU_CMD_FECS_TRACE_POLL,
		.handle = vgpu_get_handle(g),
	};
	int err;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);
	return err;
}

int vgpu_free_user_buffer(struct gk20a *g)
{
	return 0;
}


#ifdef CONFIG_NVGPU_FECS_TRACE
int vgpu_fecs_trace_max_entries(struct gk20a *g,
			struct nvgpu_gpu_ctxsw_trace_filter *filter)
{
	struct vgpu_fecs_trace *vcst = (struct vgpu_fecs_trace *)g->fecs_trace;

	return vcst->header->num_ents;
}

int vgpu_fecs_trace_set_filter(struct gk20a *g,
			struct nvgpu_gpu_ctxsw_trace_filter *filter)
{
	struct tegra_vgpu_cmd_msg msg = {
		.cmd = TEGRA_VGPU_CMD_FECS_TRACE_SET_FILTER,
		.handle = vgpu_get_handle(g),
	};
	struct tegra_vgpu_fecs_trace_filter *p = &msg.params.fecs_trace_filter;
	int err;

	(void) memcpy(&p->tag_bits, &filter->tag_bits, sizeof(p->tag_bits));
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	WARN_ON(err);
	return err;
}

#endif /* CONFIG_NVGPU_FECS_TRACE */
