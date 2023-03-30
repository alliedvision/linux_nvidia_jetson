/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/vgpu/tegra_vgpu.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/gr/subctx.h>

#include "common/gr/subctx_priv.h"

#include "subctx_vgpu.h"
#include "common/vgpu/ivc/comm_vgpu.h"

void vgpu_gr_setup_free_subctx(struct nvgpu_channel *c)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_free_ctx_header_params *p =
				&msg.params.free_ctx_header;
	struct gk20a *g = c->g;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_FREE_CTX_HEADER;
	msg.handle = vgpu_get_handle(g);
	p->ch_handle = c->virt_ctx;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	if (unlikely(err != 0)) {
		nvgpu_err(g, "free ctx_header failed err %d", err);
	}

	if (c->subctx != NULL) {
		nvgpu_kfree(g, c->subctx);
		c->subctx = NULL;
	}
}
