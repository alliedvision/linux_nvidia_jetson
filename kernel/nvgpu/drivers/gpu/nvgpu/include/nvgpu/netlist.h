/*
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_NETLIST_H
#define NVGPU_NETLIST_H
/**
 * @file
 *
 * common.netlist unit interface
 */
#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_netlist_vars;

/**
 * Description of netlist Address-Value(av) structure.
 */
struct netlist_av {
	/** U32 address in av structure. */
	u32 addr;
	/** U32 value in av structure. */
	u32 value;
};
/**
 * Description of netlist Address-Value64(av64) structure.
 */
struct netlist_av64 {
	/** U32 address in av64 structure. */
	u32 addr;
	/** Lower u32 value in av64 structure. */
	u32 value_lo;
	/** Higher u32 value in av64 structure. */
	u32 value_hi;
};
/**
 * Description of netlist Address-Index-Value(aiv) structure.
 */
struct netlist_aiv {
	/** U32 address in aiv structure. */
	u32 addr;
	/** U32 index in aiv structure. */
	u32 index;
	/** U32 value in aiv structure. */
	u32 value;
};
/**
 * Description of netlist aiv list bundle structure.
 */
struct netlist_aiv_list {
	/** Pointer to netlist aiv bundle. */
	struct netlist_aiv *l;
	/** Number of netlist aiv bundles. */
	u32 count;
};
/**
 * Description of netlist av list bundle structure.
 */
struct netlist_av_list {
	/** Pointer to netlist av bundle. */
	struct netlist_av *l;
	/** Number of netlist av bundles. */
	u32 count;
};
/**
 * Description of netlist av64 list bundle structure.
 */
struct netlist_av64_list {
	/** Pointer to netlist av64 bundle. */
	struct netlist_av64 *l;
	/** Number of netlist av64 bundles. */
	u32 count;
};
/**
 * Description of netlist u32 list bundle structure.
 */
struct netlist_u32_list {
	/** Pointer to u32 value. */
	u32 *l;
	/** Number of u32 elements in the list. */
	u32 count;
};

/**
 * @brief Allocates memory for netlist bundles and populates
 * ctx region info from ctx firmware file.
 *
 * This function allocates memory for netlist unit variables and bundles.
 * Opens netlist ctx firmware file based on firmware definition and loads
 * different bundles into appropriate netlist regions structures.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if memory allocation for netlist variables or bundle fails.
 * @retval -ENOENT if it fails to find required netlist firmware file.
 * @retval -ENOENT if netlist s/w version mismatches with h/w config.
 */
int nvgpu_netlist_init_ctx_vars(struct gk20a *g);
/**
 * @brief Frees memory allocated for netlist bundles.
 *
 * This function frees all allocated memory for netlist bundles and
 * netlist vars.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 */
void nvgpu_netlist_deinit_ctx_vars(struct gk20a *g);
/**
 * @brief Allocates memory for netlist av list bundle.
 *
 * This function allocates memory for netlist av list bundles for the
 * requested size from #netlist_av_list count.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param avl [in]		Pointer to #netlist_av_list struct.
 *
 * @return  Pointer to #netlist_av.
 * @retval NULL if memory allocation for #netlist_av bundles fails.
 */
struct netlist_av *nvgpu_netlist_alloc_av_list(struct gk20a *g,
						struct netlist_av_list *avl);
/**
 * @brief Allocates memory for netlist av64 list bundle.
 *
 * This function allocates memory for netlist av64 list bundles for the
 * requested size from #netlist_av64_list count.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param av64l [in]		Pointer to #netlist_av64_list struct.
 *
 * @return  Pointer to #netlist_av64 struct.
 * @retval NULL if memory allocation for #netlist_av64 bundles fails.
 */
struct netlist_av64 *nvgpu_netlist_alloc_av64_list(struct gk20a *g,
					struct netlist_av64_list *av64l);
/**
 * @brief Allocates memory for netlist aiv list bundle.
 *
 * This function allocates memory for netlist aiv list bundles for the
 * requested size from #netlist_aiv_list count.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param aivl [in]		Pointer to #netlist_avi_list struct.
 *
 * @return  Pointer to #netlist_aiv struct.
 * @retval NULL if memory allocation for #netlist_aiv bundles fails.
 */
struct netlist_aiv *nvgpu_netlist_alloc_aiv_list(struct gk20a *g,
						struct netlist_aiv_list *aivl);
/**
 * @brief Allocates memory for netlist u32 list bundle.
 *
 * This function allocates memory for netlist u32 list bundles for the
 * requested size from #netlist_u32_list count.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param u32l [in]		Pointer to #netlist_u32_list struct.
 *
 * @return  Pointer to u32 list.
 * @retval NULL if memory allocation for u32 list bundles fails.
 */
u32 *nvgpu_netlist_alloc_u32_list(struct gk20a *g,
						struct netlist_u32_list *u32l);
/**
 * @brief Get s/w non-context #netlist_av_list bundles from firmware.
 *
 * This function returns non-context #netlist_av_list bundles from
 * netlist firmware. SW needs to program these bundles to h/w as part
 * of golden context creation.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  Pointer to #netlist_av_list struct.
 */
struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_load_av_list(
							struct gk20a *g);
/**
 * @brief Get s/w context #netlist_aiv_list bundles from firmware.
 *
 * This function returns s/w context #netlist_aiv_list bundles from
 * netlist firmware. SW needs to program these bundles to h/w as part
 * of golden context creation.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  Pointer to #netlist_aiv_list struct.
 */
struct netlist_aiv_list *nvgpu_netlist_get_sw_ctx_load_aiv_list(
							struct gk20a *g);
/**
 * @brief Get s/w method init #netlist_av_list bundles from firmware.
 *
 * This function returns s/w method init #netlist_av_list bundles from
 * netlist firmware. SW needs to program these bundles to h/w as part
 * of golden context creation.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  Pointer to #netlist_av_list struct.
 */
struct netlist_av_list *nvgpu_netlist_get_sw_method_init_av_list(
							struct gk20a *g);
/**
 * @brief Get s/w init #netlist_av_list bundles from firmware.
 *
 * This function returns s/w bundles #netlist_av_list bundles from
 * netlist firmware. SW needs to program these bundles to h/w as part
 * of golden context creation.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  Pointer to #netlist_av_list struct.
 */
struct netlist_av_list *nvgpu_netlist_get_sw_bundle_init_av_list(
							struct gk20a *g);
/**
 * @brief Get s/w veid init #netlist_av_list bundles from firmware.
 *
 * This function returns s/w veid #netlist_av_list bundles from
 * netlist firmware. SW needs to program these bundles to h/w as part
 * of golden context creation.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  Pointer to #netlist_av_list struct.
 */
struct netlist_av_list *nvgpu_netlist_get_sw_veid_bundle_init_av_list(
							struct gk20a *g);
/**
 * @brief Get s/w u64 init #netlist_av64_list bundles from firmware.
 *
 * This function returns s/w u64 #netlist_av64_list bundles from
 * netlist firmware. SW needs to program these bundles to h/w as part
 * of golden context creation.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  Pointer to #netlist_av64_list struct.
 */
struct netlist_av64_list *nvgpu_netlist_get_sw_bundle64_init_av64_list(
							struct gk20a *g);
/**
 * @brief Get FECS imem ucode size from firmware.
 *
 * This function returns h/w fecs imem ucode size from netlist firmware.
 * SW needs this information for falcon FECS ucode programming.
 *
 * @return  FECS imem ucode size.
 * @param g [in]		Pointer to GPU driver struct.
 *
 */
u32 nvgpu_netlist_get_fecs_inst_count(struct gk20a *g);
/**
 * @brief Get FECS dmem ucode size from firmware.
 *
 * This function returns h/w fecs dmem ucode size from netlist firmware.
 * SW needs this information for falcon FECS ucode programming.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  FECS dmem ucode size.
 */
u32 nvgpu_netlist_get_fecs_data_count(struct gk20a *g);
/**
 * @brief Get GPCCS imem ucode size from firmware.
 *
 * This function returns h/w gpccs imem ucode size from netlist firmware.
 * SW needs this information for falcon GPCCS ucode programming.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  GPCCS imem ucode size.
 */
u32 nvgpu_netlist_get_gpccs_inst_count(struct gk20a *g);
/**
 * @brief Get GPCCS dmem ucode size from firmware.
 *
 * This function returns h/w gpccs dmem ucode size from netlist firmware.
 * SW needs this information for falcon GPCCS ucode programming.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  GPCCS dmem ucode size.
 */
u32 nvgpu_netlist_get_gpccs_data_count(struct gk20a *g);
/**
 * @brief Get pointer to FECS imem ucode from firmware.
 *
 * This function returns pointer to fecs imem ucode from netlist firmware.
 * SW will use this information for falcon FECS imem ucode programming.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  U32 pointer to FECS imem ucode.
 */
u32 *nvgpu_netlist_get_fecs_inst_list(struct gk20a *g);
/**
 * @brief Get pointer to FECS dmem ucode from firmware.
 *
 * This function returns pointer to fecs dmem ucode from netlist firmware.
 * SW will use this information for falcon FECS ucode programming.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  U32 pointer to FECS dmem ucode.
 */
u32 *nvgpu_netlist_get_fecs_data_list(struct gk20a *g);
/**
 * @brief Get pointer Io GPCCS imem ucode from firmware.
 *
 * This function returns pointer to gpccs imem ucode from netlist firmware.
 * SW will use this information for falcon GPCCS ucode programming.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  U32 pointer to GPCCS imem ucode.
 */
u32 *nvgpu_netlist_get_gpccs_inst_list(struct gk20a *g);
/**
 * @brief Get pointer to GPCCS dmem ucode from firmware.
 *
 * This function returns pointer to gpccs dmem ucode from netlist firmware.
 * SW will use this information for falcon GPCCS ucode programming.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * @return  U32 pointer to GPCCS dmem ucode.
 */
u32 *nvgpu_netlist_get_gpccs_data_list(struct gk20a *g);

struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_local_compute_load_av_list(
							struct gk20a *g);
struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_global_compute_load_av_list(
							struct gk20a *g);

#ifdef CONFIG_NVGPU_GRAPHICS
struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_local_gfx_load_av_list(
							struct gk20a *g);
struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_global_gfx_load_av_list(
							struct gk20a *g);
#endif /* CONFIG_NVGPU_GRAPHICS */

#ifdef CONFIG_NVGPU_DEBUGGER
struct netlist_aiv_list *nvgpu_netlist_get_sys_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_gpc_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_tpc_ctxsw_regs(struct gk20a *g);
#ifdef CONFIG_NVGPU_GRAPHICS
struct netlist_aiv_list *nvgpu_netlist_get_zcull_gpc_ctxsw_regs(
							struct gk20a *g);
#endif
struct netlist_aiv_list *nvgpu_netlist_get_ppc_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_pm_sys_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_pm_gpc_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_pm_tpc_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_pm_ppc_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_perf_sys_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_perf_gpc_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_fbp_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_fbp_router_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_gpc_router_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_pm_ltc_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_pm_fbpa_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_perf_sys_router_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_perf_pma_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_pm_rop_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_pm_ucgpc_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_etpc_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_pm_cau_ctxsw_regs(struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_perf_sys_control_ctxsw_regs(
		struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_perf_fbp_control_ctxsw_regs(
		struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_perf_gpc_control_ctxsw_regs(
		struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_perf_pma_control_ctxsw_regs(
		struct gk20a *g);
u32 nvgpu_netlist_get_ppc_ctxsw_regs_count(struct gk20a *g);
u32 nvgpu_netlist_get_gpc_ctxsw_regs_count(struct gk20a *g);
u32 nvgpu_netlist_get_tpc_ctxsw_regs_count(struct gk20a *g);
u32 nvgpu_netlist_get_etpc_ctxsw_regs_count(struct gk20a *g);
u32 nvgpu_netlist_get_sys_ctxsw_regs_count(struct gk20a *g);
void nvgpu_netlist_print_ctxsw_reg_info(struct gk20a *g);
#endif /* CONFIG_NVGPU_DEBUGGER */

#ifdef CONFIG_NVGPU_NON_FUSA
void nvgpu_netlist_set_fecs_inst_count(struct gk20a *g, u32 count);
void nvgpu_netlist_set_fecs_data_count(struct gk20a *g, u32 count);
void nvgpu_netlist_set_gpccs_inst_count(struct gk20a *g, u32 count);
void nvgpu_netlist_set_gpccs_data_count(struct gk20a *g, u32 count);

struct netlist_u32_list *nvgpu_netlist_get_fecs_inst(struct gk20a *g);
struct netlist_u32_list *nvgpu_netlist_get_fecs_data(struct gk20a *g);
struct netlist_u32_list *nvgpu_netlist_get_gpccs_inst(struct gk20a *g);
struct netlist_u32_list *nvgpu_netlist_get_gpccs_data(struct gk20a *g);

void nvgpu_netlist_vars_set_dynamic(struct gk20a *g, bool set);
void nvgpu_netlist_vars_set_buffer_size(struct gk20a *g, u32 size);
void nvgpu_netlist_vars_set_regs_base_index(struct gk20a *g, u32 index);

#ifdef CONFIG_NVGPU_DEBUGGER
struct netlist_aiv_list *nvgpu_netlist_get_sys_compute_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_gpc_compute_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_tpc_compute_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_ppc_compute_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_etpc_compute_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_lts_ctxsw_regs(
							struct gk20a *g);
#ifdef CONFIG_NVGPU_GRAPHICS
struct netlist_aiv_list *nvgpu_netlist_get_sys_gfx_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_gpc_gfx_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_tpc_gfx_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_ppc_gfx_ctxsw_regs(
							struct gk20a *g);
struct netlist_aiv_list *nvgpu_netlist_get_etpc_gfx_ctxsw_regs(
							struct gk20a *g);
#endif /* CONFIG_NVGPU_GRAPHICS */
#endif /* CONFIG_NVGPU_DEBUGGER */
#endif /* CONFIG_NVGPU_NON_FUSA */
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif /* NVGPU_NETLIST_H */
