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

#include <nvgpu/types.h>
#include <nvgpu/utils.h>
#include <nvgpu/bug.h>
#include <nvgpu/string.h>
#include <nvgpu/vgpu/vgpu_ivc.h>
#include <nvgpu/vgpu/tegra_vgpu.h>

#include "comm_vgpu.h"

int vgpu_comm_init(struct gk20a *g)
{
	size_t queue_sizes[] = { TEGRA_VGPU_QUEUE_SIZES };

	return vgpu_ivc_init(g, 3, queue_sizes, TEGRA_VGPU_QUEUE_CMD,
			ARRAY_SIZE(queue_sizes));
}

void vgpu_comm_deinit(void)
{
	size_t queue_sizes[] = { TEGRA_VGPU_QUEUE_SIZES };

	vgpu_ivc_deinit(TEGRA_VGPU_QUEUE_CMD, ARRAY_SIZE(queue_sizes));
}

int vgpu_comm_sendrecv(struct tegra_vgpu_cmd_msg *msg, size_t size_in,
		size_t size_out)
{
	void *handle;
	size_t size = size_in;
	void *data = msg;
	int err;

	err = vgpu_ivc_sendrecv(vgpu_ivc_get_server_vmid(),
				TEGRA_VGPU_QUEUE_CMD, &handle, &data, &size);
	if (err == 0) {
		WARN_ON(size < size_out);
		nvgpu_memcpy((u8 *)msg, (u8 *)data, size_out);
		vgpu_ivc_release(handle);
	}

	return err;
}
