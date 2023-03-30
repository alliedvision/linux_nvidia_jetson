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

#ifndef NVGPU_CBC_H
#define NVGPU_CBC_H

#ifdef CONFIG_NVGPU_COMPRESSION

#include <nvgpu/types.h>
#include <nvgpu/comptags.h>
#include <nvgpu/nvgpu_mem.h>

struct gk20a;

enum nvgpu_cbc_op {
	nvgpu_cbc_op_clear,
	nvgpu_cbc_op_clean,
	nvgpu_cbc_op_invalidate,
};

struct compbit_store_desc {
	struct nvgpu_mem mem;

	/*
	 * The value that is written to the hardware. This depends on
	 * on the number of ltcs and is not an address.
	 */
	u64 base_hw;
};

struct nvgpu_contig_cbcmempool {
	struct gk20a *g;
	/*
	 * cookie to hold the information about the IVM.
	 */
	struct tegra_hv_ivm_cookie *cookie;
	/*
	 * base physical address of the contig pool.
	 */
	u64 base_addr;
	/* size of the contig_pool */
	u64 size;
	/* Cpu mapped address for the given pool. */
	void *cbc_cpuva;
	/* Mutex to protect the allocation requests. */
	struct nvgpu_mutex contigmem_mutex;
};

struct nvgpu_cbc {
	u32 compbit_backing_size;
	u32 comptags_per_cacheline;
	u32 gobs_per_comptagline_per_slice;
	u32 max_comptag_lines;
	struct gk20a_comptag_allocator comp_tags;
	struct compbit_store_desc compbit_store;
	struct nvgpu_contig_cbcmempool *cbc_contig_mempool;
};

int nvgpu_cbc_init_support(struct gk20a *g);
void nvgpu_cbc_remove_support(struct gk20a *g);
int nvgpu_cbc_alloc(struct gk20a *g, size_t compbit_backing_size,
			bool vidmem_alloc);
int  nvgpu_cbc_contig_init(struct gk20a *g);
void nvgpu_cbc_contig_deinit(struct gk20a *g);
#endif
#endif /* NVGPU_CBC_H */
