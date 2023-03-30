/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/string.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/vgpu/vgpu_ivc.h>
#include <nvgpu/vgpu/tegra_vgpu.h>

#include "common/vgpu/ivc/comm_vgpu.h"
#include "vab_vgpu.h"

int vgpu_fb_vab_reserve(struct gk20a *g, u32 vab_mode, u32 num_range_checkers,
			struct nvgpu_vab_range_checker *vab_range_checker)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_fb_vab_reserve_params *p = &msg.params.fb_vab_reserve;
	int err;
	void *oob;
	void *oob_handle;
	size_t size, oob_size;

	oob_handle = vgpu_ivc_oob_get_ptr(vgpu_ivc_get_server_vmid(),
			TEGRA_VGPU_QUEUE_CMD, &oob, &oob_size);
	if (!oob_handle) {
		return -EINVAL;
	}

	size = sizeof(*vab_range_checker) * num_range_checkers;
	if (oob_size < size) {
		err = -ENOMEM;
		goto done;
	}

	msg.cmd = TEGRA_VGPU_CMD_FB_VAB_RESERVE;
	msg.handle = vgpu_get_handle(g);
	p->vab_mode = vab_mode;
	p->num_range_checkers = num_range_checkers;

	nvgpu_memcpy((u8 *)oob, (u8 *)vab_range_checker, size);
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));

	err = err != 0 ? err : msg.ret;
	if (err != 0) {
		nvgpu_err(g, "fb vab reserve failed err %d", err);
	}

done:
	vgpu_ivc_oob_put_ptr(oob_handle);
    return err;
}

int vgpu_fb_vab_dump_and_clear(struct gk20a *g, u8 *user_buf,
				u64 user_buf_size)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_fb_vab_dump_and_clear_params *p =
			&msg.params.fb_vab_dump_and_clear;
	int err;
	void *oob;
	void *oob_handle;
	size_t oob_size;

	oob_handle = vgpu_ivc_oob_get_ptr(vgpu_ivc_get_server_vmid(),
			TEGRA_VGPU_QUEUE_CMD, &oob, &oob_size);
	if (!oob_handle) {
		return -EINVAL;
	}

	if (oob_size < user_buf_size) {
		err = -ENOMEM;
		goto done;
	}

	msg.cmd = TEGRA_VGPU_CMD_FB_VAB_DUMP_CLEAR;
	msg.handle = vgpu_get_handle(g);
	p->user_buf_size = user_buf_size;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));

	err = err != 0 ? err : msg.ret;
	if (err == 0) {
		nvgpu_memcpy(user_buf, (u8 *)oob, user_buf_size);
	} else {
		nvgpu_err(g, "fb vab flush state failed err %d", err);
	}

done:
	vgpu_ivc_oob_put_ptr(oob_handle);
	return err;
}

int vgpu_fb_vab_release(struct gk20a *g)
{
	struct tegra_vgpu_cmd_msg msg = {};
	int err;

	msg.cmd = TEGRA_VGPU_CMD_FB_VAB_RELEASE;
	msg.handle = vgpu_get_handle(g);
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err != 0 ? err : msg.ret;
	if (err != 0) {
		nvgpu_err(g, "fb vab release failed err %d", err);
	}

    return err;
}
