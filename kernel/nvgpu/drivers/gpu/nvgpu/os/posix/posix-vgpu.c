/*
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

#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/vgpu/vgpu_ivc.h>
#include <nvgpu/nvgpu_ivm.h>
#include <nvgpu/vgpu/os_init_hal_vgpu.h>

struct vgpu_priv_data *vgpu_get_priv_data(struct gk20a *g)
{
	(void)g;
	BUG();
	return NULL;
}

int vgpu_ivc_init(struct gk20a *g, u32 elems,
		const size_t *queue_sizes, u32 queue_start, u32 num_queues)
{
	(void)g;
	(void)elems;
	(void)queue_sizes;
	(void)queue_start;
	(void)num_queues;
	BUG();
	return 0;
}

void vgpu_ivc_deinit(u32 queue_start, u32 num_queues)
{
	(void)queue_start;
	(void)num_queues;
	BUG();
}

void vgpu_ivc_release(void *handle)
{
	(void)handle;
	BUG();
}

u32 vgpu_ivc_get_server_vmid(void)
{
	BUG();
	return 0U;
}

int vgpu_ivc_recv(u32 index, void **handle, void **data,
				size_t *size, u32 *sender)
{
	(void)index;
	(void)handle;
	(void)data;
	(void)size;
	(void)sender;
	BUG();
	return 0;
}

int vgpu_ivc_send(u32 peer, u32 index, void *data, size_t size)
{
	(void)peer;
	(void)index;
	(void)data;
	(void)size;
	BUG();
	return 0;
}

int vgpu_ivc_sendrecv(u32 peer, u32 index, void **handle,
				void **data, size_t *size)
{
	(void)peer;
	(void)index;
	(void)handle;
	(void)data;
	(void)size;
	BUG();
	return 0;
}

u32 vgpu_ivc_get_peer_self(void)
{
	BUG();
	return 0U;
}

void *vgpu_ivc_oob_get_ptr(u32 peer, u32 index, void **ptr,
					size_t *size)
{
	(void)peer;
	(void)index;
	(void)ptr;
	(void)size;
	BUG();
	return NULL;
}

void vgpu_ivc_oob_put_ptr(void *handle)
{
	(void)handle;
	BUG();
}


struct tegra_hv_ivm_cookie *nvgpu_ivm_mempool_reserve(unsigned int id)
{
	(void)id;
	BUG();
	return NULL;
}

int nvgpu_ivm_mempool_unreserve(struct tegra_hv_ivm_cookie *cookie)
{
	(void)cookie;
	BUG();
	return 0;
}

u64 nvgpu_ivm_get_ipa(struct tegra_hv_ivm_cookie *cookie)
{
	(void)cookie;
	BUG();
	return 0ULL;
}

u64 nvgpu_ivm_get_size(struct tegra_hv_ivm_cookie *cookie)
{
	(void)cookie;
	BUG();
	return 0ULL;
}

void *nvgpu_ivm_mempool_map(struct tegra_hv_ivm_cookie *cookie)
{
	(void)cookie;
	BUG();
	return NULL;
}

void nvgpu_ivm_mempool_unmap(struct tegra_hv_ivm_cookie *cookie,
		void *addr)
{
	(void)cookie;
	(void)addr;
	BUG();
}
int vgpu_init_hal_os(struct gk20a *g)
{
	(void)g;
	BUG();
	return -ENOSYS;
}
