/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_SIM_H
#define NVGPU_SIM_H

#ifdef CONFIG_NVGPU_SIM

#include <nvgpu/types.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/gk20a.h>

/*
 * Size of SIM ring buffers.
 *
 * Although, each buffer is allocated with NVGPU_CPU_PAGE_SIZE bytes.
 * The send and receive interface can only be configured to work with buffers of
 * sizes: 4K, 8K, 12K and 16K. Furthermore, this size should match with size
 * configured in the fmodel chiplib. At present, the agreed buffer size is 4K.
 */
#define SIM_BFR_SIZE	(SZ_4K)

struct sim_nvgpu {
	struct gk20a *g;
	uintptr_t regs;
	u32 send_ring_put;
	u32 recv_ring_get;
	u32 recv_ring_put;
	u32 sequence_base;
	struct nvgpu_mem send_bfr;
	struct nvgpu_mem recv_bfr;
	struct nvgpu_mem msg_bfr;
	int (*sim_init_late)(struct gk20a *g);
	void (*remove_support)(struct gk20a *g);
	void (*esc_readl)(
		struct gk20a *g, const char *path, u32 index, u32 *data);
};
#ifdef __KERNEL__
#include "linux/sim.h"
#include "linux/sim_pci.h"
#elif defined(__NVGPU_POSIX__)
/* Nothing for POSIX-nvgpu. */
#else
#include <nvgpu_rmos/include/sim.h>
#include <nvgpu_rmos/include/sim_pci.h>
#endif
int nvgpu_init_sim_support(struct gk20a *g);
int nvgpu_init_sim_support_pci(struct gk20a *g);
int nvgpu_alloc_sim_buffer(struct gk20a *g, struct nvgpu_mem *mem);
void nvgpu_free_sim_buffer(struct gk20a *g, struct nvgpu_mem *mem);
void nvgpu_free_sim_support(struct gk20a *g);
void nvgpu_remove_sim_support(struct gk20a *g);
void sim_writel(struct sim_nvgpu *sim, u32 r, u32 v);
u32 sim_readl(struct sim_nvgpu *sim, u32 r);
int nvgpu_init_sim_netlist_ctx_vars(struct gk20a *g);
int issue_rpc_and_wait(struct gk20a *g);
void sim_write_hdr(struct gk20a *g, u32 func, u32 size);

static inline u32 sim_escape_read_hdr_size(void)
{
	return 12U; /*TBD: fix NV_VGPU_SIM_ESCAPE_READ_HEADER*/
}

static inline u32 sim_msg_header_size(void)
{
	return 24U;/*TBD: fix the header to gt this from NV_VGPU_MSG_HEADER*/
}

static inline u32 *sim_msg_bfr(struct gk20a *g, u32 byte_offset)
{
	u8 *cpu_va;

	cpu_va = (u8 *)g->sim->msg_bfr.cpu_va;

	return (u32 *)(cpu_va + byte_offset);
}

static inline u32 *sim_msg_hdr(struct gk20a *g, u32 byte_offset)
{
	return sim_msg_bfr(g, byte_offset); /*starts at 0*/
}

static inline u32 *sim_msg_param(struct gk20a *g, u32 byte_offset)
{
	/*starts after msg header/cmn*/
	return sim_msg_bfr(g, byte_offset + sim_msg_header_size());
}

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_NON_FUSA
void nvgpu_init_sim_support2(struct gk20a *g);
#endif /* CONFIG_NVGPU_NON_FUSA */
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif
#endif /* NVGPU_SIM_H */
