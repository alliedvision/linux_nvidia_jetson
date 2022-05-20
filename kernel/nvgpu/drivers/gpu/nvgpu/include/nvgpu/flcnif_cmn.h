/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FLCNIF_CMN_H
#define NVGPU_FLCNIF_CMN_H

#include <nvgpu/types.h>
#include <nvgpu/bitops.h>

#define PMU_CMD_SUBMIT_PAYLOAD_PARAMS_FB_SIZE_UNUSED 0U

struct falc_u64 {
	u32 lo;
	u32 hi;
};

static inline void flcn64_set_dma(struct falc_u64 *dma_addr, u64 value)
{
	dma_addr->lo |= u64_lo32(value);
	dma_addr->hi |= u64_hi32(value);
}

struct falc_dma_addr {
	u32 dma_base;
	/*
	 * dma_base1 is 9-bit MSB for FB Base
	 * address for the transfer in FB after
	 * address using 49b FB address
	 */
	u16 dma_base1;
	u8 dma_offset;
};

struct pmu_mem_v1 {
	u32 dma_base;
	u8  dma_offset;
	u8  dma_idx;
	u16 fb_size;
};

struct pmu_mem_desc_v0 {
	struct falc_u64 dma_addr;
	u16       dma_sizemax;
	u8        dma_idx;
};

struct pmu_dmem {
	u16 size;
	u32 offset;
};

struct flcn_mem_desc_v0 {
	struct falc_u64 address;
	u32 params;
};

#define nv_flcn_mem_desc flcn_mem_desc_v0

struct pmu_allocation_v1 {
	struct {
		struct pmu_dmem dmem;
		struct pmu_mem_v1 fb;
	} alloc;
};

struct pmu_allocation_v2 {
	struct {
		struct pmu_dmem dmem;
		struct pmu_mem_desc_v0 fb;
	} alloc;
};

struct pmu_allocation_v3 {
	struct {
		struct pmu_dmem dmem;
		struct flcn_mem_desc_v0 fb;
	} alloc;
};

struct falcon_payload_alloc {
	u16 dmem_size;
	u32 dmem_offset;
};

#define nv_pmu_allocation pmu_allocation_v3

struct pmu_hdr {
	u8 unit_id;
	u8 size;
	u8 ctrl_flags;
	u8 seq_id;
};

#define  NV_FLCN_UNIT_ID_REWIND    (0x00U)

#define PMU_MSG_HDR_SIZE	U32(sizeof(struct pmu_hdr))
#define PMU_CMD_HDR_SIZE	U32(sizeof(struct pmu_hdr))

#define nv_pmu_hdr pmu_hdr
typedef u8 falcon_status;

#define PMU_DMEM_ALLOC_ALIGNMENT	32U
#define PMU_DMEM_ALIGNMENT		4U

#define PMU_CMD_FLAGS_PMU_MASK		U8(0xF0U)

#define PMU_CMD_FLAGS_STATUS		BIT8(0)
#define PMU_CMD_FLAGS_INTR		BIT8(1)
#define PMU_CMD_FLAGS_EVENT		BIT8(2)
#define PMU_CMD_FLAGS_RPC_EVENT		BIT8(3U)

#define ALIGN_UP(v, gran)       (((v) + ((gran) - 1U)) & ~((gran)-1U))

#define NV_UNSIGNED_ROUNDED_DIV(a, b)    (((a) + ((b) / 2U)) / (b))

/* FB queue support interfaces */
/* Header for a FBQ qntry */
struct nv_falcon_fbq_hdr {
	/* Element this CMD will use in the FB CMD Q. */
	u8    element_index;
	/* Pad bytes to keep 4 byte alignment. */
	u8    padding[3];
	/* Size of allocation in nvgpu managed heap. */
	u16   heap_size;
	/* Heap location this CMD will use in the nvgpu managed heap. */
	u16   heap_offset;
};

/* Header for a FB MSG Queue Entry */
struct nv_falcon_fbq_msgq_hdr {
	/* Queue level sequence number. */
	u16   sequence_number;
	/* Negative checksum of entire queue entry. */
	u16   checksum;
};
#endif /* NVGPU_FLCNIF_CMN_H */
