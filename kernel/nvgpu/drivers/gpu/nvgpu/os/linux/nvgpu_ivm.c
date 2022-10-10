/*
 * Copyright (c) 2022, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <nvgpu/nvgpu_ivm.h>

#include <linux/tegra-ivc.h>

#include "os/linux/os_linux.h"

struct tegra_hv_ivm_cookie *nvgpu_ivm_mempool_reserve(unsigned int id)
{
	return tegra_hv_mempool_reserve(id);
}

int nvgpu_ivm_mempool_unreserve(struct tegra_hv_ivm_cookie *cookie)
{
	return tegra_hv_mempool_unreserve(cookie);
}

u64 nvgpu_ivm_get_ipa(struct tegra_hv_ivm_cookie *cookie)
{
	return cookie->ipa;
}

u64 nvgpu_ivm_get_size(struct tegra_hv_ivm_cookie *cookie)
{
	return cookie->size;
}

void *nvgpu_ivm_mempool_map(struct tegra_hv_ivm_cookie *cookie)
{
	return ioremap_cache(nvgpu_ivm_get_ipa(cookie),
				nvgpu_ivm_get_size(cookie));
}

void nvgpu_ivm_mempool_unmap(struct tegra_hv_ivm_cookie *cookie,
		void *addr)
{
	iounmap(addr);
}
