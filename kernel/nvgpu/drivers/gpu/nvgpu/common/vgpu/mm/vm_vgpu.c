/*
 * Virtualized GPU VM
 *
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/dma.h>
#include <nvgpu/bug.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/vm.h>
#include <nvgpu/vm_area.h>

#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/vgpu/vm_vgpu.h>

#include "common/vgpu/ivc/comm_vgpu.h"

/*
 * This is called by the common VM init routine to handle vGPU specifics of
 * intializing a VM on a vGPU. This alone is not enough to init a VM. See
 * nvgpu_vm_init().
 */
int vgpu_vm_as_alloc_share(struct gk20a *g, struct vm_gk20a *vm)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_share_params *p = &msg.params.as_share;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_AS_ALLOC_SHARE;
	msg.handle = vgpu_get_handle(g);
	p->va_start = vm->virtaddr_start;
	p->va_limit = vm->va_limit;
	p->big_page_size = vm->big_page_size;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	if (err || msg.ret) {
		return -ENOMEM;
	}

	vm->handle = p->handle;

	return 0;
}

/*
 * Similar to vgpu_vm_as_alloc_share() this is called as part of the cleanup
 * path for VMs. This alone is not enough to remove a VM -
 * see nvgpu_vm_remove().
 */
void vgpu_vm_as_free_share(struct vm_gk20a *vm)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_as_share_params *p = &msg.params.as_share;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_AS_FREE_SHARE;
	msg.handle = vgpu_get_handle(g);
	p->handle = vm->handle;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	WARN_ON(err || msg.ret);
}
