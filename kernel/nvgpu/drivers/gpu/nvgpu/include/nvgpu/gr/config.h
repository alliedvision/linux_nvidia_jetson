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

#ifndef NVGPU_GR_CONFIG_H
#define NVGPU_GR_CONFIG_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * common.gr.config unit interface
 */
struct gk20a;
struct nvgpu_sm_info;
struct nvgpu_gr_config;

/**
 * @brief Initialize GR engine configuration information.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function reads GR engine configuration from GPU h/w and stores
 * it in #nvgpu_gr_config struct.
 *
 * GR engine configuration data includes max count, available count and mask
 * for below h/w units:
 * - GPC (Graphics Processing Cluster)
 * - TPC (Texture Processor Cluster)
 * - SM (Streaming Multiprocessor)
 * - PPC (Primitive Processor Cluster)
 * - PES (Primitive Engine Shared)
 *
 * This unit also exposes APIs to query each of above configuration data.
 *
 * If incorrect or unexpected GR engine configuration is read (examples
 * below) initialization is aborted and NULL is returned.
 * - GPC count is 0.
 * - SM count per TPC is 0.
 * - PES count per GPC is more than #GK20A_GR_MAX_PES_PER_GPC.
 *
 * @return pointer to nvgpu_gr_config struct in case of success,
 *         NULL in case of failure.
 */
struct nvgpu_gr_config *nvgpu_gr_config_init(struct gk20a *g);

/**
 * @brief Deinitialize GR engine configuration.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function will free memory allocated to hold GR engine
 * configuration information in #nvgpu_gr_config_init().
 */
void nvgpu_gr_config_deinit(struct gk20a *g, struct nvgpu_gr_config *config);

/**
 * @brief Get max GPC count.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns maximum number of GPCs available in a GPU chip
 * family.
 *
 * @return max GPC count.
 */
u32 nvgpu_gr_config_get_max_gpc_count(struct nvgpu_gr_config *config);

/**
 * @brief Get max TPC per GPC count.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns maximum number of TPCs available per GPC in a
 * GPU chip family.
 *
 * @return max TPC per GPC count.
 */
u32 nvgpu_gr_config_get_max_tpc_per_gpc_count(struct nvgpu_gr_config *config);

/**
 * @brief Get max PES per GPC count.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns maximum number of PESs available per GPC in a
 * GPU chip family.
 *
 * @return max PES per GPC count.
 */
u32 nvgpu_gr_config_get_max_pes_per_gpc_count(struct nvgpu_gr_config *config);

/**
 * @brief Get max ROP per GPC count.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns maximum number of ROPs available per GPC in a
 * GPU chip family.
 *
 * @return max ROP per GPC count.
 */
u32 nvgpu_gr_config_get_max_rop_per_gpc_count(struct nvgpu_gr_config *config);

/**
 * @brief Get max TPC count.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns maximum number of TPCs available in a GPU chip
 * family.
 *
 * @return max TPC count.
 */
u32 nvgpu_gr_config_get_max_tpc_count(struct nvgpu_gr_config *config);

/**
 * @brief Get available GPC count.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns number of available GPCs in GR engine.
 * Note that other GPCs are floorswept or not available.
 *
 * @return number of available GPCs.
 */
u32 nvgpu_gr_config_get_gpc_count(struct nvgpu_gr_config *config);

/**
 * @brief Get available TPC count.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns number of available TPCs in GR engine.
 * Note that other TPCs are floorswept or not available.
 *
 * @return number of available TPCs.
 */
u32 nvgpu_gr_config_get_tpc_count(struct nvgpu_gr_config *config);

/**
 * @brief Get available PPC count.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns number of available PPCs in GR engine.
 *
 * @return number of available PPCs.
 */
u32 nvgpu_gr_config_get_ppc_count(struct nvgpu_gr_config *config);

/**
 * @brief Get PES count per GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns number of PES units per GPC.
 *
 * @return number of PES per GPC.
 */
u32 nvgpu_gr_config_get_pe_count_per_gpc(struct nvgpu_gr_config *config);

/**
 * @brief Get SM count per TPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns number of SMs per TPC.
 *
 * @return number of SMs per TPC.
 */
u32 nvgpu_gr_config_get_sm_count_per_tpc(struct nvgpu_gr_config *config);

/**
 * @brief Get PPC count for given GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc_index [in]	Valid GPC index.
 *
 * This function returns number of PPCs for given GPC index.
 * GPC index must be less than value returned by
 * #nvgpu_gr_config_get_gpc_count(), otherwise an assert is raised.
 *
 * @return number of PPCs for given GPC.
 */
u32 nvgpu_gr_config_get_gpc_ppc_count(struct nvgpu_gr_config *config,
	u32 gpc_index);

/**
 * @brief Get base address of array that stores number of TPCs in GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * Number of TPCs per GPC are stored in an array indexed by GPC index.
 * This function returns base address of this array.
 *
 * @return base address of array that stores number of TPCs in GPC.
 */
u32 *nvgpu_gr_config_get_base_count_gpc_tpc(struct nvgpu_gr_config *config);

/**
 * @brief Get TPC count for given GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc_index [in]	Valid GPC index.
 *
 * This function returns number of TPCs for given GPC index.
 * GPC index must be less than value returned by
 * #nvgpu_gr_config_get_gpc_count(), otherwise value of 0 is returned.
 *
 * @return number of TPCs for given GPC for valid GPC index.
 */
u32 nvgpu_gr_config_get_gpc_tpc_count(struct nvgpu_gr_config *config,
	u32 gpc_index);

/**
 * @brief Get TPC count for given PES/GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc_index [in]	Valid GPC index.
 * @param pes_index [in]	Valid PES index.
 *
 * A GPC includes multiple TPC and PES units. A PES unit has multiple
 * TPC units connected to it within same GPC.
 * This function returns number of TPCs attached to PES with index
 * pes_index in a GPC with index gpc_index.
 *
 * GPC index must be less than value returned by
 * #nvgpu_gr_config_get_gpc_count() and PES index must be less than value
 * returned by #nvgpu_gr_config_get_pe_count_per_gpc(),
 * otherwise an assert is raised.
 *
 * @return number of TPCs for given PES/GPC.
 */
u32 nvgpu_gr_config_get_pes_tpc_count(struct nvgpu_gr_config *config,
	u32 gpc_index, u32 pes_index);

/**
 * @brief Get base address of array that stores mask of TPCs in GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * Masks of TPCs per GPC are stored in an array indexed by GPC index.
 * This function returns base address of this array.
 *
 * @return base address of array that stores mask of TPCs in GPC.
 */
u32 *nvgpu_gr_config_get_base_mask_gpc_tpc(struct nvgpu_gr_config *config);

/**
 * @brief Get base address of array that stores mask of TPCs in GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * Get base address of array that stores mask of TPCs in GPC, ordered
 * in physical-id when in non-MIG(legacy) mode and by logical-id when in
 * MIG mode.
 *
 * @return base address of array that stores mask of TPCs in GPC.
 */
u32 *nvgpu_gr_config_get_gpc_tpc_mask_physical_base(
		struct nvgpu_gr_config *config);

/**
 * @brief Get TPC mask for given logical GPC index.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc_index [in]	Valid GPC index.
 *
 * This function returns mask of TPCs for given GPC index.
 * Each set bit indicates TPC with that index is available, otherwise
 * the TPC is considered floorswept.
 * GPC index must be less than value returned by
 * #nvgpu_gr_config_get_max_gpc_count(), otherwise an assert is raised.
 *
 * @return mask of TPCs for given GPC.
 */
u32 nvgpu_gr_config_get_gpc_tpc_mask(struct nvgpu_gr_config *config,
	u32 gpc_index);

/**
 * @brief Get TPC mask for given physical GPC index.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc_index [in]	Valid GPC physical id.
 *
 * This function returns mask of TPCs for given GPC index, which will be
 * the physical-id in non-MIG(legacy) mode and logical-id in MIG mode.
 * Each set bit indicates TPC with that index is available, otherwise
 * the TPC is considered floorswept.
 * GPC index must be less than value returned by
 * #nvgpu_gr_config_get_max_gpc_count(), otherwise an assert is raised.
 *
 * @return mask of TPCs for given GPC.
 */
u32 nvgpu_gr_config_get_gpc_tpc_mask_physical(struct nvgpu_gr_config *config,
	u32 gpc_index);

/**
 * @brief Set TPC mask for given GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc_index [in]	Valid GPC index.
 * @param val [in]		Mask value to be set.
 *
 * This function sets the TPC mask in #nvgpu_gr_config struct
 * for given GPC index.
 * GPC index must be less than value returned by
 * #nvgpu_gr_config_get_gpc_count(), otherwise an assert is raised.
 */
void nvgpu_gr_config_set_gpc_tpc_mask(struct nvgpu_gr_config *config,
	u32 gpc_index, u32 val);

/**
 * @brief Get TPC skip mask for given GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc_index [in]	Valid GPC index.
 *
 * This function returns skip mask of TPCs for given GPC index.
 * This mask will be used to skip certain TPC during load balancing
 * if any of the PES is overloaded.
 * GPC index must be less than value returned by
 * #nvgpu_gr_config_get_gpc_count(), otherwise value of 0 is returned.
 *
 * @return skip mask of TPCs for given GPC.
 */
u32 nvgpu_gr_config_get_gpc_skip_mask(struct nvgpu_gr_config *config,
	u32 gpc_index);

/**
 * @brief Get TPC mask for given PES/GPC.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc_index [in]	Valid GPC index.
 * @param pes_index [in]	Valid PES index.
 *
 * A GPC includes multiple TPC and PES units. A PES unit has multiple
 * TPC units connected to it within same GPC.
 * This function returns mask of TPCs attached to PES with index
 * pes_index in a GPC with index gpc_index
 *
 * GPC index must be less than value returned by
 * #nvgpu_gr_config_get_gpc_count() and PES index must be less than value
 * returned by #nvgpu_gr_config_get_pe_count_per_gpc(),
 * otherwise an assert is raised.
 *
 * @return mask of TPCs for given PES/GPC.
 */
u32 nvgpu_gr_config_get_pes_tpc_mask(struct nvgpu_gr_config *config,
	u32 gpc_index, u32 pes_index);

/**
 * @brief Get mask of GPCs.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns mask of GPCs in GR engine. Each set bit indicates
 * GPC with that index is available, otherwise the GPC is considered
 * floorswept.
 *
 * @return GPC mask.
 */
u32 nvgpu_gr_config_get_gpc_mask(struct nvgpu_gr_config *config);

/**
 * @brief Get number of SMs.
 *
 * @param config [in]		Pointer to GR configuration struct.
 *
 * This function returns number of SMs in GR engine.
 *
 * @return number of SMs.
 */
u32 nvgpu_gr_config_get_no_of_sm(struct nvgpu_gr_config *config);

/**
 * @brief Set number of SMs.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param no_of_sm [in]		SM count to be set.
 *
 * This function sets number of SMs in #nvgpu_gr_config struct.
 */
void nvgpu_gr_config_set_no_of_sm(struct nvgpu_gr_config *config, u32 no_of_sm);

/**
 * @brief Get information of given SM.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param sm_id [in]		Valid SM index.
 *
 * common.gr unit stores information of each SM into an array of struct
 * #nvgpu_sm_info. This information includes GPC/TPC indexes for
 * particular SM, and index of SM within TPC.
 *
 * This function will return pointer to #nvgpu_sm_info struct for SM with
 * requested index if requested index is valid.
 * Valid SM index range is 0 to (SM count - 1) where SM count is received
 * with #nvgpu_gr_config_get_no_of_sm().
 *
 * @return pointer to struct #nvgpu_sm_info if requested sm_id is valid,
 *         NULL otherwise.
 */
struct nvgpu_sm_info *nvgpu_gr_config_get_sm_info(struct nvgpu_gr_config *config,
	u32 sm_id);

#ifdef CONFIG_NVGPU_SM_DIVERSITY
/**
 * @brief Get information of given SM.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param sm_id [in]		SM index.
 *
 * common.gr unit stores redundant execution config information of each SM
 * into an array of struct #nvgpu_sm_info. This information includes GPC/TPC
 * indexes for particular SM, and index of SM within TPC.
 *
 * This config is valid only if NVGPU_SUPPORT_SM_DIVERSITY support is true.
 *
 * This function will return pointer to #nvgpu_sm_info struct for SM with
 * requested index.
 *
 * @return pointer to struct #nvgpu_sm_info
 */
struct nvgpu_sm_info *nvgpu_gr_config_get_redex_sm_info(
	struct nvgpu_gr_config *config, u32 sm_id);
#endif

/**
 * @brief Get GPC index of SM.
 *
 * @param sm_info [in]		Pointer to SM information struct.
 *
 * This function returns GPC index of SM from given #nvgpu_sm_info struct.
 *
 * @return GPC index of SM.
 */
u32 nvgpu_gr_config_get_sm_info_gpc_index(struct nvgpu_sm_info *sm_info);

/**
 * @brief Set GPC index of SM.
 *
 * @param sm_info [in]		Pointer to SM information struct.
 * @param gpc_index [in]	GPC index to be set.
 *
 * This function sets GPC index of SM into given #nvgpu_sm_info struct.
 */
void nvgpu_gr_config_set_sm_info_gpc_index(struct nvgpu_sm_info *sm_info,
	u32 gpc_index);

/**
 * @brief Get TPC index of SM.
 *
 * @param sm_info [in]		Pointer to SM information struct.
 *
 * This function returns TPC index of SM from given #nvgpu_sm_info struct.
 *
 * @return TPC index of SM.
 */
u32 nvgpu_gr_config_get_sm_info_tpc_index(struct nvgpu_sm_info *sm_info);

/**
 * @brief Set TPC index of SM.
 *
 * @param sm_info [in]		Pointer to SM information struct.
 * @param tpc_index [in]	TPC index to be set.
 *
 * This function sets TPC index of SM into given #nvgpu_sm_info struct.
 */
void nvgpu_gr_config_set_sm_info_tpc_index(struct nvgpu_sm_info *sm_info,
	u32 tpc_index);

/**
 * @brief Get global TPC index of SM.
 *
 * @param sm_info [in]		Pointer to SM information struct.
 *
 * This function returns global TPC index of SM from given #nvgpu_sm_info
 * struct. Global index is assigned to TPC considering all TPCs in all GPCs.
 *
 * @return global TPC index of SM.
 */
u32 nvgpu_gr_config_get_sm_info_global_tpc_index(struct nvgpu_sm_info *sm_info);

/**
 * @brief Set global TPC index of SM.
 *
 * @param sm_info [in]		Pointer to SM information struct.
 * @param global_tpc_index [in]	Global TPC index to be set.
 *
 * This function sets global TPC index of SM into given #nvgpu_sm_info struct.
 * Global index is assigned to TPC considering all TPCs in all GPCs.
 */
void nvgpu_gr_config_set_sm_info_global_tpc_index(struct nvgpu_sm_info *sm_info,
	u32 global_tpc_index);

/**
 * @brief Get index of SM within TPC.
 *
 * @param sm_info [in]		Pointer to SM information struct.
 *
 * This function returns index of SM within TPC from given #nvgpu_sm_info
 * struct. e.g. GV11B GPU has 2 SMs in a TPC. So this function will return
 * 0 or 1 as appropriate.
 *
 * @return index of SM within TPC.
 */
u32 nvgpu_gr_config_get_sm_info_sm_index(struct nvgpu_sm_info *sm_info);

/**
 * @brief Set index of SM within TPC.
 *
 * @param sm_info [in]		Pointer to SM information struct.
 * @param sm_index [in]		SM index.
 *
 * This function sets index of SM within TPC into given #nvgpu_sm_info
 * struct.
 */
void nvgpu_gr_config_set_sm_info_sm_index(struct nvgpu_sm_info *sm_info,
	u32 sm_index);

/**
 * @brief Get the physical to logical id map for all the ROPs in the specified
 *        gpc.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc [in]		GPC logical id.
 *
 * @return pointer to u32 array.
 */
const u32 *gr_config_get_gpc_rop_logical_id_map(struct nvgpu_gr_config *config,
		u32 gpc);

/**
 * @brief Get the physical to logical id map for all the PESs in the specified
 *        gpc.
 *
 * @param config [in]		Pointer to GR configuration struct.
 * @param gpc [in]		GPC logical id.
 *
 * @return pointer to u32 array.
 */
const u32 *gr_config_get_gpc_pes_logical_id_map(struct nvgpu_gr_config *config,
		u32 gpc);

#ifdef CONFIG_NVGPU_GRAPHICS
int nvgpu_gr_config_init_map_tiles(struct gk20a *g,
				   struct nvgpu_gr_config *config);
u32 nvgpu_gr_config_get_map_row_offset(struct nvgpu_gr_config *config);
u32 nvgpu_gr_config_get_map_tile_count(struct nvgpu_gr_config *config,
	u32 index);
u8 *nvgpu_gr_config_get_map_tiles(struct nvgpu_gr_config *config);
u32 nvgpu_gr_config_get_max_zcull_per_gpc_count(struct nvgpu_gr_config *config);
u32 nvgpu_gr_config_get_zcb_count(struct nvgpu_gr_config *config);
u32 nvgpu_gr_config_get_gpc_zcb_count(struct nvgpu_gr_config *config,
	u32 gpc_index);
#endif /* CONFIG_NVGPU_GRAPHICS */

#endif /* NVGPU_GR_CONFIG_H */
