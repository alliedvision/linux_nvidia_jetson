/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/kmem.h>
#include <nvgpu/enabled.h>
#include <nvgpu/dt.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_ivm.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/soc.h>
#ifdef __KERNEL__
#include <linux/tegra-ivc.h>
#else
#include <tegra-ivc.h>
#endif
#include <nvgpu/nvgpu_sgt.h>

static void nvgpu_init_cbc_contig_pa(struct nvgpu_contig_cbcmempool *contig_pool)
{
	contig_pool->base_addr = nvgpu_get_pa_from_ipa(contig_pool->g,
					contig_pool->cookie->ipa);
	contig_pool->size = contig_pool->cookie->size;
}

int nvgpu_cbc_contig_init(struct gk20a *g)
{
	struct nvgpu_contig_cbcmempool *contig_pool;
	u32 mempool_id;
	int err;

	contig_pool = nvgpu_kzalloc(g, sizeof(*contig_pool));
	if (!contig_pool) {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_COMPRESSION, false);
		return -ENOMEM;
	}

	contig_pool->g = g;
	nvgpu_mutex_init(&contig_pool->contigmem_mutex);
	g->cbc->cbc_contig_mempool = contig_pool;
	err = nvgpu_dt_read_u32_index(g, "phys_contiguous_mempool",
			0, &mempool_id);
	if (err) {
		nvgpu_err(g, "Reading the contig_mempool from dt failed %d", err);
		goto fail;
	}

	contig_pool->cookie = nvgpu_ivm_mempool_reserve(mempool_id);
	if (contig_pool->cookie == NULL) {
		nvgpu_err(g,
			"mempool  %u reserve failed", mempool_id);
		contig_pool->cookie = NULL;
		goto fail;
	}

	contig_pool->cbc_cpuva = nvgpu_ivm_mempool_map(contig_pool->cookie);
	if (contig_pool->cbc_cpuva == NULL) {
		nvgpu_err(g, "nvgpu_ivm_mempool_map failed");
		goto fail;
	}

	nvgpu_init_cbc_contig_pa(contig_pool);

	return 0;
fail:
	nvgpu_cbc_contig_deinit(g);
	err = -ENOMEM;
	nvgpu_set_enabled(g, NVGPU_SUPPORT_COMPRESSION, false);
	return err;
}

void nvgpu_cbc_contig_deinit(struct gk20a *g)
{
	struct nvgpu_contig_cbcmempool *contig_pool;
	struct nvgpu_mem *mem;

	if ((g->cbc == NULL) || (g->cbc->cbc_contig_mempool == NULL)) {
		return;
	}

	contig_pool = g->cbc->cbc_contig_mempool;
	if (contig_pool->cookie != NULL &&
			contig_pool->cbc_cpuva != NULL) {
		nvgpu_ivm_mempool_unmap(contig_pool->cookie,
				contig_pool->cbc_cpuva);
	}

	if (contig_pool->cookie) {
		nvgpu_ivm_mempool_unreserve(contig_pool->cookie);
	}

	nvgpu_kfree(g, contig_pool);
	g->cbc->cbc_contig_mempool = NULL;
	mem = &g->cbc->compbit_store.mem;
	nvgpu_kfree(g, mem->phys_sgt->sgl);
	nvgpu_kfree(g, mem->phys_sgt);
	(void) memset(&g->cbc->compbit_store, 0,
			sizeof(struct compbit_store_desc));
}

