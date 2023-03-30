/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_NETLIST_PRIV_H
#define NVGPU_NETLIST_PRIV_H

#include <nvgpu/types.h>

struct netlist_u32_list;
struct netlist_av_list;
struct netlist_av64_list;
struct netlist_aiv_list;

/* netlist regions */
#define NETLIST_REGIONID_FECS_UCODE_DATA	0
#define NETLIST_REGIONID_FECS_UCODE_INST	1
#define NETLIST_REGIONID_GPCCS_UCODE_DATA	2
#define NETLIST_REGIONID_GPCCS_UCODE_INST	3
#define NETLIST_REGIONID_SW_BUNDLE_INIT		4
#define NETLIST_REGIONID_SW_CTX_LOAD		5
#define NETLIST_REGIONID_SW_NON_CTX_LOAD	6
#define NETLIST_REGIONID_SW_METHOD_INIT		7
#ifdef CONFIG_NVGPU_DEBUGGER
#define NETLIST_REGIONID_CTXREG_SYS		8
#define NETLIST_REGIONID_CTXREG_GPC		9
#define NETLIST_REGIONID_CTXREG_TPC		10
#define NETLIST_REGIONID_CTXREG_ZCULL_GPC	11
#define NETLIST_REGIONID_CTXREG_PM_SYS		12
#define NETLIST_REGIONID_CTXREG_PM_GPC		13
#define NETLIST_REGIONID_CTXREG_PM_TPC		14
#endif
#define NETLIST_REGIONID_MAJORV			15
#define NETLIST_REGIONID_BUFFER_SIZE		16
#define NETLIST_REGIONID_CTXSW_REG_BASE_INDEX	17
#define NETLIST_REGIONID_NETLIST_NUM		18
#ifdef CONFIG_NVGPU_DEBUGGER
#define NETLIST_REGIONID_CTXREG_PPC		19
#define NETLIST_REGIONID_CTXREG_PMPPC		20
#define NETLIST_REGIONID_NVPERF_CTXREG_SYS	21
#define NETLIST_REGIONID_NVPERF_FBP_CTXREGS	22
#define NETLIST_REGIONID_NVPERF_CTXREG_GPC	23
#define NETLIST_REGIONID_NVPERF_FBP_ROUTER	24
#define NETLIST_REGIONID_NVPERF_GPC_ROUTER	25
#define NETLIST_REGIONID_CTXREG_PMLTC		26
#define NETLIST_REGIONID_CTXREG_PMFBPA		27
#endif
#define NETLIST_REGIONID_SWVEIDBUNDLEINIT       28
#ifdef CONFIG_NVGPU_DEBUGGER
#define NETLIST_REGIONID_NVPERF_SYS_ROUTER      29
#define NETLIST_REGIONID_NVPERF_PMA             30
#define NETLIST_REGIONID_CTXREG_PMROP           31
#define NETLIST_REGIONID_CTXREG_PMUCGPC         32
#define NETLIST_REGIONID_CTXREG_ETPC            33
#endif
#define NETLIST_REGIONID_SW_BUNDLE64_INIT	34
#ifdef CONFIG_NVGPU_DEBUGGER
#define NETLIST_REGIONID_NVPERF_PMCAU		35
#define NETLIST_REGIONID_CTXREG_SYS_COMPUTE	36
#define NETLIST_REGIONID_CTXREG_GPC_COMPUTE	38
#define NETLIST_REGIONID_CTXREG_TPC_COMPUTE	40
#define NETLIST_REGIONID_CTXREG_PPC_COMPUTE	42
#define NETLIST_REGIONID_CTXREG_ETPC_COMPUTE	44
#ifdef CONFIG_NVGPU_GRAPHICS
#define NETLIST_REGIONID_CTXREG_SYS_GFX			37
#define NETLIST_REGIONID_CTXREG_GPC_GFX			39
#define NETLIST_REGIONID_CTXREG_TPC_GFX			41
#define NETLIST_REGIONID_CTXREG_PPC_GFX			43
#define NETLIST_REGIONID_CTXREG_ETPC_GFX		45
#endif  /* CONFIG_NVGPU_GRAPHICS */
#define NETLIST_REGIONID_SW_NON_CTX_LOCAL_COMPUTE_LOAD	48
#define NETLIST_REGIONID_SW_NON_CTX_GLOBAL_COMPUTE_LOAD	50
#ifdef CONFIG_NVGPU_GRAPHICS
#define NETLIST_REGIONID_SW_NON_CTX_LOCAL_GFX_LOAD	49
#define NETLIST_REGIONID_SW_NON_CTX_GLOBAL_GFX_LOAD	51
#endif  /* CONFIG_NVGPU_GRAPHICS */
#define NETLIST_REGIONID_NVPERF_SYS_CONTROL	52
#define NETLIST_REGIONID_NVPERF_FBP_CONTROL	53
#define NETLIST_REGIONID_NVPERF_GPC_CONTROL	54
#define NETLIST_REGIONID_NVPERF_PMA_CONTROL	55
#define NETLIST_REGIONID_CTXREG_LTS_BC		57
#define NETLIST_REGIONID_CTXREG_LTS_UC		58
#endif

struct netlist_region {
	u32 region_id;
	u32 data_size;
	u32 data_offset;
};

struct netlist_image_header {
	u32 version;
	u32 regions;
};

struct netlist_image {
	struct netlist_image_header header;
	struct netlist_region regions[1];
};

struct netlist_gr_ucode {
	struct {
		struct netlist_u32_list inst;
		struct netlist_u32_list data;
	} gpccs, fecs;
};

struct nvgpu_netlist_vars {
	bool dynamic;

	u32 regs_base_index;
	u32 buffer_size;

	struct netlist_gr_ucode ucode;

	struct netlist_av_list  sw_bundle_init;
	struct netlist_av64_list sw_bundle64_init;
	struct netlist_av_list  sw_method_init;
	struct netlist_aiv_list sw_ctx_load;
	struct netlist_av_list  sw_non_ctx_load;
	struct netlist_av_list  sw_non_ctx_local_compute_load;
	struct netlist_av_list  sw_non_ctx_global_compute_load;
#ifdef CONFIG_NVGPU_GRAPHICS
	struct netlist_av_list  sw_non_ctx_local_gfx_load;
	struct netlist_av_list  sw_non_ctx_global_gfx_load;
#endif  /* CONFIG_NVGPU_GRAPHICS */
	struct netlist_av_list  sw_veid_bundle_init;
#ifdef CONFIG_NVGPU_DEBUGGER
	struct {
		struct netlist_aiv_list sys;
		struct netlist_aiv_list gpc;
		struct netlist_aiv_list tpc;
#ifdef CONFIG_NVGPU_GRAPHICS
		struct netlist_aiv_list zcull_gpc;
#endif
		struct netlist_aiv_list ppc;
		struct netlist_aiv_list pm_sys;
		struct netlist_aiv_list pm_gpc;
		struct netlist_aiv_list pm_tpc;
		struct netlist_aiv_list pm_ppc;
		struct netlist_aiv_list perf_sys;
		struct netlist_aiv_list perf_gpc;
		struct netlist_aiv_list fbp;
		struct netlist_aiv_list fbp_router;
		struct netlist_aiv_list gpc_router;
		struct netlist_aiv_list pm_ltc;
		struct netlist_aiv_list pm_fbpa;
		struct netlist_aiv_list perf_sys_router;
		struct netlist_aiv_list perf_pma;
		struct netlist_aiv_list pm_rop;
		struct netlist_aiv_list pm_ucgpc;
		struct netlist_aiv_list etpc;
		struct netlist_aiv_list pm_cau;
		struct netlist_aiv_list perf_sys_control;
		struct netlist_aiv_list perf_fbp_control;
		struct netlist_aiv_list perf_gpc_control;
		struct netlist_aiv_list perf_pma_control;
#if defined(CONFIG_NVGPU_NON_FUSA)
		struct netlist_aiv_list sys_compute;
		struct netlist_aiv_list gpc_compute;
		struct netlist_aiv_list tpc_compute;
		struct netlist_aiv_list ppc_compute;
		struct netlist_aiv_list etpc_compute;
		struct netlist_aiv_list lts_bc;
		struct netlist_aiv_list lts_uc;
#ifdef CONFIG_NVGPU_GRAPHICS
		struct netlist_aiv_list sys_gfx;
		struct netlist_aiv_list gpc_gfx;
		struct netlist_aiv_list tpc_gfx;
		struct netlist_aiv_list ppc_gfx;
		struct netlist_aiv_list etpc_gfx;
#endif /* CONFIG_NVGPU_GRAPHICS */
#endif /* CONFIG_NVGPU_NON_FUSA */
	} ctxsw_regs;
#endif /* CONFIG_NVGPU_DEBUGGER */
};

#endif /* NVGPU_NETLIST_PRIV_H */
