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

#include <nvgpu/vgpu/vgpu_ivc.h>
#include <nvgpu/vgpu/tegra_vgpu.h>
#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/gk20a.h>

#include "perf_vgpu.h"
#include "common/vgpu/ivc/comm_vgpu.h"

static int vgpu_sendrecv_perfbuf_cmd(struct gk20a *g, u64 offset, u32 size)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->perfbuf.vm;
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_perfbuf_mgt_params *p =
						&msg.params.perfbuf_management;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_PERFBUF_MGT;
	msg.handle = vgpu_get_handle(g);

	p->vm_handle = vm->handle;
	p->offset = offset;
	p->size = size;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}

int vgpu_perfbuffer_enable(struct gk20a *g, u64 offset, u32 size)
{
	return vgpu_sendrecv_perfbuf_cmd(g, offset, size);
}

int vgpu_perfbuffer_disable(struct gk20a *g)
{
	return vgpu_sendrecv_perfbuf_cmd(g, 0, 0);
}

static int vgpu_sendrecv_perfbuf_inst_block_cmd(struct gk20a *g, u32 mode)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->perfbuf.vm;
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_perfbuf_inst_block_mgt_params *p =
				&msg.params.perfbuf_inst_block_management;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_PERFBUF_INST_BLOCK_MGT;
	msg.handle = vgpu_get_handle(g);

	p->vm_handle = vm->handle;
	p->mode = mode;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}

int vgpu_perfbuffer_init_inst_block(struct gk20a *g)
{
	return vgpu_sendrecv_perfbuf_inst_block_cmd(g,
			TEGRA_VGPU_PROF_PERFBUF_INST_BLOCK_INIT);
}

void vgpu_perfbuffer_deinit_inst_block(struct gk20a *g)
{
	vgpu_sendrecv_perfbuf_inst_block_cmd(g,
			TEGRA_VGPU_PROF_PERFBUF_INST_BLOCK_DEINIT);
}

int vgpu_perf_update_get_put(struct gk20a *g, u64 bytes_consumed,
		bool update_available_bytes, u64 *put_ptr,
		bool *overflowed)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_perf_update_get_put_params *p =
				&msg.params.perf_updat_get_put;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_PERF_UPDATE_GET_PUT;
	msg.handle = vgpu_get_handle(g);

	p->bytes_consumed = bytes_consumed;
	p->update_available_bytes = (u8)update_available_bytes;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;

	if (err == 0) {
		if (put_ptr != NULL) {
			*put_ptr = p->put_ptr;
		}
		if (overflowed != NULL) {
			*overflowed = (bool)p->overflowed;
		}
	}

	return err;
}
