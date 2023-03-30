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

#ifndef NVGPU_GR_CONFIG_PRIV_H
#define NVGPU_GR_CONFIG_PRIV_H

#include <nvgpu/types.h>

/**
 * Max possible PES count per GPC.
 */
#define GK20A_GR_MAX_PES_PER_GPC 3U

struct gk20a;

/**
 * Detailed information of SM indexes in GR engine.
 */
struct nvgpu_sm_info {
	/**
	 * Index of GPC for SM.
	 */
	u32 gpc_index;

	/**
	 * Index of TPC for SM.
	 */
	u32 tpc_index;

	/**
	 * Index of SM within TPC.
	 */
	u32 sm_index;

	/**
	 * Global TPC index for SM.
	 */
	u32 global_tpc_index;
};

/**
 * GR engine configuration data.
 *
 * This data is populated during GR initialization and referred across
 * GPU driver through public APIs.
 */
struct nvgpu_gr_config {
	/**
	 * Pointer to GPU driver struct.
	 */
	struct gk20a *g;

	/**
	 * Max possible number of GPCs in GR engine.
	 */
	u32 max_gpc_count;
	/**
	 * Max possible number of TPCs per GPC in GR engine.
	 */
	u32 max_tpc_per_gpc_count;
	/**
	 * Max possible number of TPCs in GR engine.
	 */
	u32 max_tpc_count;
	/**
	 * Max possible number of PESs in a GPC.
	 */
	u32 max_pes_per_gpc_count;
	/**
	 * Max possible number of ROPs in a GPC.
	 */
	u32 max_rop_per_gpc_count;
	/**
	 * Number of GPCs in GR engine.
	 */
	u32 gpc_count;
	/**
	 * Number of TPCs in GR engine.
	 */
	u32 tpc_count;
	/**
	 * Number of PPCs in GR engine.
	 */
	u32 ppc_count;

	/**
	 * Number of PES per GPC in GR engine.
	 */
	u32 pe_count_per_gpc;
	/**
	 * Number of SMs per TPC in GR engine.
	 */
	u32 sm_count_per_tpc;

	/**
	 * Array to hold number of PPC units per GPC.
	 * Array is indexed by GPC index.
	 */
	u32 *gpc_ppc_count;
	/**
	 * Array to hold number of TPCs per GPC.
	 * Array is indexed by GPC index.
	 */
	u32 *gpc_tpc_count;
	/**
	 * 2-D array to hold number of TPCs attached to a PES unit
	 * in a GPC.
	 */
	u32 *pes_tpc_count[GK20A_GR_MAX_PES_PER_GPC];

	/**
	 * Mask of GPCs. A set bit indicates GPC is available, otherwise
	 * it is not available.
	 */
	u32 gpc_mask;

	/**
	 * Array to hold mask of TPCs per GPC.
	 * Array is indexed by GPC logical index/local IDs when using MIG mode
	 */
	u32 *gpc_tpc_mask;
	/**
	 * Array to hold mask of TPCs per GPC.
	 * Array is indexed by GPC physical-id.
	 */
	u32 *gpc_tpc_mask_physical;
	/**
	 * 2-D array to hold mask of TPCs attached to a PES unit
	 * in a GPC.
	 */
	u32 *pes_tpc_mask[GK20A_GR_MAX_PES_PER_GPC];
	/**
	 * Array to hold skip mask of TPCs per GPC.
	 * Array is indexed by GPC index.
	 */
	u32 *gpc_skip_mask;

	/**
	 * Array to hold mask of PESs per GPC.
	 * Array is indexed by GPC logical index/local IDs when using MIG mode
	 */
	u32 *gpc_pes_mask;

	/**
	 * 2D array to map PES physical id to logical id.
	 */
	u32 **gpc_pes_logical_id_map;


	/**
	 * Array to hold mask of ROPs per GPC.
	 * Array is indexed by GPC logical index/local IDs when using MIG mode
	 */
	u32 *gpc_rop_mask;

	/**
	 * 2D array to map ROP physical id to logical id.
	 */
	u32 **gpc_rop_logical_id_map;

	/**
	 * Number of SMs in GR engine.
	 */
	u32 no_of_sm;
	/**
	 * Pointer to SM information struct.
	 */
	struct nvgpu_sm_info *sm_to_cluster;
#ifdef CONFIG_NVGPU_SM_DIVERSITY
	/**
	 * Pointer to redundant execution config SM information struct.
	 * It is valid only if NVGPU_SUPPORT_SM_DIVERSITY support is true.
	 */
	struct nvgpu_sm_info *sm_to_cluster_redex_config;
#endif

#ifdef CONFIG_NVGPU_GRAPHICS
	u32 max_zcull_per_gpc_count;
	u32 zcb_count;
	u32 *gpc_zcb_count;

	u8 *map_tiles;
	u32 map_tile_count;
	u32 map_row_offset;
#endif
};

#endif /* NVGPU_GR_CONFIG_PRIV_H */
