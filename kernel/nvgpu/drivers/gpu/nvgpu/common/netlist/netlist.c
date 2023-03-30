/*
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/nvgpu_common.h>
#include <nvgpu/sim.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/firmware.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/netlist.h>
#include <nvgpu/string.h>
#include <nvgpu/netlist_defs.h>
#include <nvgpu/static_analysis.h>

#include "netlist_priv.h"

/*
 * Need to support multiple ARCH in same GPU family
 * then need to provide path like ARCH/NETIMAGE to
 * point to correct netimage within GPU family,
 * Example, gm20x can support gm204 or gm206,so path
 * for netimage is gm204/NETC_img.bin, and '/' char
 * will inserted at null terminator char of "GAxxx"
 * to get complete path like gm204/NETC_img.bin
 */

#define MAX_NETLIST_NAME (sizeof("GAxxx/") + sizeof("NET?_img.bin"))

struct netlist_av *nvgpu_netlist_alloc_av_list(struct gk20a *g,
						struct netlist_av_list *avl)
{
	avl->l = nvgpu_kzalloc(g, nvgpu_safe_mult_u64(avl->count,
						      sizeof(*avl->l)));
	return avl->l;
}

struct netlist_av64 *nvgpu_netlist_alloc_av64_list(struct gk20a *g,
						struct netlist_av64_list *av64l)
{
	av64l->l = nvgpu_kzalloc(g, nvgpu_safe_mult_u64(av64l->count,
						      sizeof(*av64l->l)));
	return av64l->l;
}

struct netlist_aiv *nvgpu_netlist_alloc_aiv_list(struct gk20a *g,
				       struct netlist_aiv_list *aivl)
{
	aivl->l = nvgpu_kzalloc(g, nvgpu_safe_mult_u64(aivl->count,
						       sizeof(*aivl->l)));
	return aivl->l;
}

u32 *nvgpu_netlist_alloc_u32_list(struct gk20a *g,
					struct netlist_u32_list *u32l)
{
	u32l->l = nvgpu_kzalloc(g, nvgpu_safe_mult_u64(u32l->count,
						       sizeof(*u32l->l)));
	return u32l->l;
}

static int nvgpu_netlist_alloc_load_u32_list(struct gk20a *g, u8 *src, u32 len,
			struct netlist_u32_list *u32_list)
{
	u32_list->count = nvgpu_safe_add_u32(len,
				nvgpu_safe_cast_u64_to_u32(sizeof(u32) - 1UL))
							/ U32(sizeof(u32));
	if (nvgpu_netlist_alloc_u32_list(g, u32_list) == NULL) {
		return -ENOMEM;
	}

	nvgpu_memcpy((u8 *)u32_list->l, src, len);

	return 0;
}

static int nvgpu_netlist_alloc_load_av_list(struct gk20a *g, u8 *src, u32 len,
			struct netlist_av_list *av_list)
{
	av_list->count = len / U32(sizeof(struct netlist_av));
	if (nvgpu_netlist_alloc_av_list(g, av_list) == NULL) {
		return -ENOMEM;
	}

	nvgpu_memcpy((u8 *)av_list->l, src, len);

	return 0;
}

static int nvgpu_netlist_alloc_load_av_list64(struct gk20a *g, u8 *src, u32 len,
			struct netlist_av64_list *av64_list)
{
	av64_list->count = len / U32(sizeof(struct netlist_av64));
	if (nvgpu_netlist_alloc_av64_list(g, av64_list) == NULL) {
		return -ENOMEM;
	}

	nvgpu_memcpy((u8 *)av64_list->l, src, len);

	return 0;
}

static int nvgpu_netlist_alloc_load_aiv_list(struct gk20a *g, u8 *src, u32 len,
			struct netlist_aiv_list *aiv_list)
{
	aiv_list->count = len / U32(sizeof(struct netlist_aiv));
	if (nvgpu_netlist_alloc_aiv_list(g, aiv_list) == NULL) {
		return -ENOMEM;
	}

	nvgpu_memcpy((u8 *)aiv_list->l, src, len);

	return 0;
}

static bool nvgpu_netlist_handle_ucode_region_id(struct gk20a *g,
			u32 region_id, u8 *src, u32 size,
			struct nvgpu_netlist_vars *netlist_vars, int *err_code)
{
	int err = 0;
	bool handled = true;

	switch (region_id) {
	case NETLIST_REGIONID_FECS_UCODE_DATA:
		nvgpu_log_info(g, "NETLIST_REGIONID_FECS_UCODE_DATA");
		err = nvgpu_netlist_alloc_load_u32_list(g,
			src, size, &netlist_vars->ucode.fecs.data);
		break;
	case NETLIST_REGIONID_FECS_UCODE_INST:
		nvgpu_log_info(g, "NETLIST_REGIONID_FECS_UCODE_INST");
		err = nvgpu_netlist_alloc_load_u32_list(g,
			src, size, &netlist_vars->ucode.fecs.inst);
		break;
	case NETLIST_REGIONID_GPCCS_UCODE_DATA:
		nvgpu_log_info(g, "NETLIST_REGIONID_GPCCS_UCODE_DATA");
		err = nvgpu_netlist_alloc_load_u32_list(g,
			src, size, &netlist_vars->ucode.gpccs.data);
		break;
	case NETLIST_REGIONID_GPCCS_UCODE_INST:
		nvgpu_log_info(g, "NETLIST_REGIONID_GPCCS_UCODE_INST");
		err = nvgpu_netlist_alloc_load_u32_list(g,
			src, size, &netlist_vars->ucode.gpccs.inst);
		break;
	default:
		handled = false;
		break;
	}

	*err_code = err;

	return handled;
}

static bool nvgpu_netlist_handle_sw_bundles_region_id(struct gk20a *g,
			u32 region_id, u8 *src, u32 size,
			struct nvgpu_netlist_vars *netlist_vars, int *err_code)
{
	int err = 0;
	bool handled = true;

	switch (region_id) {
	case NETLIST_REGIONID_SW_BUNDLE_INIT:
		nvgpu_log_info(g, "NETLIST_REGIONID_SW_BUNDLE_INIT");
		err = nvgpu_netlist_alloc_load_av_list(g,
			src, size, &netlist_vars->sw_bundle_init);
		break;
	case NETLIST_REGIONID_SW_METHOD_INIT:
		nvgpu_log_info(g, "NETLIST_REGIONID_SW_METHOD_INIT");
		err = nvgpu_netlist_alloc_load_av_list(g,
			src, size, &netlist_vars->sw_method_init);
		break;
	case NETLIST_REGIONID_SW_CTX_LOAD:
		nvgpu_log_info(g, "NETLIST_REGIONID_SW_CTX_LOAD");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->sw_ctx_load);
		break;
	case NETLIST_REGIONID_SW_NON_CTX_LOAD:
		nvgpu_log_info(g, "NETLIST_REGIONID_SW_NON_CTX_LOAD");
		err = nvgpu_netlist_alloc_load_av_list(g,
			src, size, &netlist_vars->sw_non_ctx_load);
		break;
	case NETLIST_REGIONID_SWVEIDBUNDLEINIT:
		nvgpu_log_info(g, "NETLIST_REGIONID_SW_VEID_BUNDLE_INIT");
		err = nvgpu_netlist_alloc_load_av_list(g,
			src, size, &netlist_vars->sw_veid_bundle_init);
		break;
	case NETLIST_REGIONID_SW_BUNDLE64_INIT:
		nvgpu_log_info(g, "NETLIST_REGIONID_SW_BUNDLE64_INIT");
		err = nvgpu_netlist_alloc_load_av_list64(g,
			src, size, &netlist_vars->sw_bundle64_init);
		break;

#if defined(CONFIG_NVGPU_NON_FUSA)
	case NETLIST_REGIONID_SW_NON_CTX_LOCAL_COMPUTE_LOAD:
		nvgpu_log_info(g, "NETLIST_REGIONID_SW_NON_CTX_LOCAL_COMPUTE_LOAD");
		err = nvgpu_netlist_alloc_load_av_list(g, src, size,
			&netlist_vars->sw_non_ctx_local_compute_load);
		break;
	case NETLIST_REGIONID_SW_NON_CTX_GLOBAL_COMPUTE_LOAD:
		nvgpu_log_info(g, "NETLIST_REGIONID_SW_NON_CTX_GLOBAL_COMPUTE_LOAD");
		err = nvgpu_netlist_alloc_load_av_list(g, src, size,
			&netlist_vars->sw_non_ctx_global_compute_load);
		break;
#endif

	default:
		handled = false;
		break;
	}

#if defined(CONFIG_NVGPU_NON_FUSA)
	if ((handled == false) && (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG))) {
		handled = true;
		switch (region_id) {
#ifdef CONFIG_NVGPU_GRAPHICS
		case NETLIST_REGIONID_SW_NON_CTX_LOCAL_GFX_LOAD:
			nvgpu_log_info(g, "NETLIST_REGIONID_SW_NON_CTX_LOCAL_GFX_LOAD");
			err = nvgpu_netlist_alloc_load_av_list(g, src, size,
				&netlist_vars->sw_non_ctx_local_gfx_load);
			break;
		case NETLIST_REGIONID_SW_NON_CTX_GLOBAL_GFX_LOAD:
			nvgpu_log_info(g, "NETLIST_REGIONID_SW_NON_CTX_GLOBAL_GFX_LOAD");
			err = nvgpu_netlist_alloc_load_av_list(g, src, size,
				&netlist_vars->sw_non_ctx_global_gfx_load);
			break;
#endif
		default:
			handled = false;
			break;
		}
	}
#endif

	*err_code = err;

	return handled;
}

static bool nvgpu_netlist_handle_generic_region_id(struct gk20a *g,
			u32 region_id, u8 *src, u32 size,
			u32 *major_v, u32 *netlist_num,
			struct nvgpu_netlist_vars *netlist_vars)
{
	bool handled = true;

	(void)size;

	switch (region_id) {
	case NETLIST_REGIONID_BUFFER_SIZE:
		nvgpu_memcpy((u8 *)&netlist_vars->buffer_size,
							src, sizeof(u32));
		nvgpu_log_info(g, "NETLIST_REGIONID_BUFFER_SIZE : %d",
			netlist_vars->buffer_size);
		break;
	case NETLIST_REGIONID_CTXSW_REG_BASE_INDEX:
		nvgpu_memcpy((u8 *)&netlist_vars->regs_base_index,
							src, sizeof(u32));
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXSW_REG_BASE_INDEX : %u",
			netlist_vars->regs_base_index);
		break;
	case NETLIST_REGIONID_MAJORV:
		nvgpu_memcpy((u8 *)major_v, src, sizeof(u32));
		nvgpu_log_info(g, "NETLIST_REGIONID_MAJORV : %d", *major_v);
		break;
	case NETLIST_REGIONID_NETLIST_NUM:
		nvgpu_memcpy((u8 *)netlist_num, src, sizeof(u32));
		nvgpu_log_info(g, "NETLIST_REGIONID_NETLIST_NUM : %d",
					*netlist_num);
		break;
	default:
		handled = false;
		break;
	}

	return handled;
}

#ifdef CONFIG_NVGPU_DEBUGGER
static bool nvgpu_netlist_handle_debugger_region_id(struct gk20a *g,
			u32 region_id, u8 *src, u32 size,
			struct nvgpu_netlist_vars *netlist_vars, int *err_code)
{
	int err = 0;
	bool handled = true;

	switch (region_id) {
	case NETLIST_REGIONID_CTXREG_PM_SYS:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PM_SYS");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.pm_sys);
		break;
	case NETLIST_REGIONID_CTXREG_PM_GPC:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PM_GPC");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.pm_gpc);
		break;
	case NETLIST_REGIONID_CTXREG_PM_TPC:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PM_TPC");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.pm_tpc);
		break;
	case NETLIST_REGIONID_NVPERF_CTXREG_SYS:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_CTXREG_SYS");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.perf_sys);
		break;
	case NETLIST_REGIONID_NVPERF_FBP_CTXREGS:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_FBP_CTXREGS");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.fbp);
		break;
	case NETLIST_REGIONID_NVPERF_CTXREG_GPC:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_CTXREG_GPC");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.perf_gpc);
		break;
	case NETLIST_REGIONID_NVPERF_FBP_ROUTER:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_FBP_ROUTER");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.fbp_router);
		break;
	case NETLIST_REGIONID_NVPERF_GPC_ROUTER:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_GPC_ROUTER");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.gpc_router);
		break;
	case NETLIST_REGIONID_CTXREG_PMLTC:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PMLTC");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.pm_ltc);
		break;
	case NETLIST_REGIONID_CTXREG_PMFBPA:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PMFBPA");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.pm_fbpa);
		break;
	case NETLIST_REGIONID_NVPERF_SYS_ROUTER:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_SYS_ROUTER");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.perf_sys_router);
		break;
	case NETLIST_REGIONID_NVPERF_PMA:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_PMA");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.perf_pma);
		break;
	case NETLIST_REGIONID_CTXREG_PMUCGPC:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PMUCGPC");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.pm_ucgpc);
		break;
	case NETLIST_REGIONID_NVPERF_PMCAU:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_PMCAU");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.pm_cau);
		break;
	case NETLIST_REGIONID_NVPERF_SYS_CONTROL:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_SYS_CONTROL");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.perf_sys_control);
		break;
	case NETLIST_REGIONID_NVPERF_FBP_CONTROL:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_FBP_CONTROL");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.perf_fbp_control);
		break;
	case NETLIST_REGIONID_NVPERF_GPC_CONTROL:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_GPC_CONTROL");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.perf_gpc_control);
		break;
	case NETLIST_REGIONID_NVPERF_PMA_CONTROL:
		nvgpu_log_info(g, "NETLIST_REGIONID_NVPERF_PMA_CONTROL");
		err = nvgpu_netlist_alloc_load_aiv_list(g,
			src, size, &netlist_vars->ctxsw_regs.perf_pma_control);
		break;

#if defined(CONFIG_NVGPU_NON_FUSA)
	case NETLIST_REGIONID_CTXREG_SYS_COMPUTE:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_SYS_COMPUTE");
		err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
			&netlist_vars->ctxsw_regs.sys_compute);
		break;

	case NETLIST_REGIONID_CTXREG_GPC_COMPUTE:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_GPC_COMPUTE");
		err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
			&netlist_vars->ctxsw_regs.gpc_compute);
		break;

	case NETLIST_REGIONID_CTXREG_TPC_COMPUTE:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_TPC_COMPUTE");
		err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
			&netlist_vars->ctxsw_regs.tpc_compute);
		break;

	case NETLIST_REGIONID_CTXREG_PPC_COMPUTE:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PPC_COMPUTE");
		err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
			&netlist_vars->ctxsw_regs.ppc_compute);
		break;

	case NETLIST_REGIONID_CTXREG_ETPC_COMPUTE:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_ETPC_COMPUTE");
		err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
			&netlist_vars->ctxsw_regs.etpc_compute);
		break;
	case NETLIST_REGIONID_CTXREG_LTS_BC:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_LTS_BC");
		err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
			&netlist_vars->ctxsw_regs.lts_bc);
		break;

	case NETLIST_REGIONID_CTXREG_LTS_UC:
		nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_LTS_UC");
		err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
			&netlist_vars->ctxsw_regs.lts_uc);
		break;
#endif

	default:
		handled = false;
		break;
	}

	if ((handled == false) && (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG))) {
		handled = true;
		switch (region_id) {
		case NETLIST_REGIONID_CTXREG_SYS:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_SYS");
			err = nvgpu_netlist_alloc_load_aiv_list(g,
				src, size, &netlist_vars->ctxsw_regs.sys);
			break;
		case NETLIST_REGIONID_CTXREG_GPC:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_GPC");
			err = nvgpu_netlist_alloc_load_aiv_list(g,
				src, size, &netlist_vars->ctxsw_regs.gpc);
			break;
		case NETLIST_REGIONID_CTXREG_TPC:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_TPC");
			err = nvgpu_netlist_alloc_load_aiv_list(g,
				src, size, &netlist_vars->ctxsw_regs.tpc);
			break;
#ifdef CONFIG_NVGPU_GRAPHICS
		case NETLIST_REGIONID_CTXREG_ZCULL_GPC:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_ZCULL_GPC");
			err = nvgpu_netlist_alloc_load_aiv_list(g,
				src, size, &netlist_vars->ctxsw_regs.zcull_gpc);
			break;
		case NETLIST_REGIONID_CTXREG_SYS_GFX:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_SYS_GFX");
			err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
				&netlist_vars->ctxsw_regs.sys_gfx);
			break;
		case NETLIST_REGIONID_CTXREG_GPC_GFX:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_GPC_GFX");
			err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
				&netlist_vars->ctxsw_regs.gpc_gfx);
			break;
		case NETLIST_REGIONID_CTXREG_TPC_GFX:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_TPC_GFX");
			err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
				&netlist_vars->ctxsw_regs.tpc_gfx);
			break;
		case NETLIST_REGIONID_CTXREG_PPC_GFX:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PPC_GFX");
			err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
				&netlist_vars->ctxsw_regs.ppc_gfx);
			break;
		case NETLIST_REGIONID_CTXREG_ETPC_GFX:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_ETPC_GFX");
			err = nvgpu_netlist_alloc_load_aiv_list(g, src, size,
				&netlist_vars->ctxsw_regs.etpc_gfx);
			break;
#endif
		case NETLIST_REGIONID_CTXREG_PPC:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PPC");
			err = nvgpu_netlist_alloc_load_aiv_list(g,
				src, size, &netlist_vars->ctxsw_regs.ppc);
			break;
		case NETLIST_REGIONID_CTXREG_PMPPC:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PMPPC");
			err = nvgpu_netlist_alloc_load_aiv_list(g,
				src, size, &netlist_vars->ctxsw_regs.pm_ppc);
			break;
		case NETLIST_REGIONID_CTXREG_PMROP:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_PMROP");
			err = nvgpu_netlist_alloc_load_aiv_list(g,
				src, size, &netlist_vars->ctxsw_regs.pm_rop);
			break;
		case NETLIST_REGIONID_CTXREG_ETPC:
			nvgpu_log_info(g, "NETLIST_REGIONID_CTXREG_ETPC");
			err = nvgpu_netlist_alloc_load_aiv_list(g,
				src, size, &netlist_vars->ctxsw_regs.etpc);
			break;
		default:
			handled = false;
			break;
		}
	}
	*err_code = err;

	return handled;
}
#endif /* CONFIG_NVGPU_DEBUGGER */

static int nvgpu_netlist_handle_region_id(struct gk20a *g,
			u32 region_id, u8 *src, u32 size,
			u32 *major_v, u32 *netlist_num,
			struct nvgpu_netlist_vars *netlist_vars)
{
	bool handled;
	int err = 0;

	handled = nvgpu_netlist_handle_ucode_region_id(g, region_id,
				src, size, netlist_vars, &err);
	if ((err != 0) || handled) {
		goto clean_up;
	}
	handled = nvgpu_netlist_handle_sw_bundles_region_id(g, region_id,
				src, size, netlist_vars, &err);
	if ((err != 0) || handled) {
		goto clean_up;
	}
	handled = nvgpu_netlist_handle_generic_region_id(g, region_id,
				src, size, major_v, netlist_num,
				netlist_vars);
	if (handled) {
		goto clean_up;
	}
#ifdef CONFIG_NVGPU_DEBUGGER
	handled  = nvgpu_netlist_handle_debugger_region_id(g, region_id,
				src, size, netlist_vars, &err);
	if ((err != 0) || handled) {
		goto clean_up;
	}
#endif /* CONFIG_NVGPU_DEBUGGER */

	/* region id command not handled */
	nvgpu_log_info(g, "unrecognized region %d skipped", region_id);

clean_up:
	return err;
}

static bool nvgpu_netlist_is_valid(int net, u32 major_v, u32 major_v_hw)
{
	if ((net != NETLIST_FINAL) && (major_v != major_v_hw)) {
		return false;
	}
	return true;
}

static int nvgpu_netlist_init_ctx_vars_fw(struct gk20a *g)
{
	struct nvgpu_netlist_vars *netlist_vars = g->netlist_vars;
	struct nvgpu_firmware *netlist_fw;
	struct netlist_image *netlist = NULL;
	char name[MAX_NETLIST_NAME];
	u32 i, major_v = ~U32(0U), major_v_hw, netlist_num;
	int net, max_netlist_num, err = -ENOENT;

	nvgpu_log_fn(g, " ");

	if (g->ops.netlist.is_fw_defined()) {
		net = NETLIST_FINAL;
		max_netlist_num = 0;
		major_v_hw = ~U32(0U);
		netlist_vars->dynamic = false;
	} else {
		net = NETLIST_SLOT_A;
		max_netlist_num = MAX_NETLIST;
		major_v_hw =
		g->ops.gr.falcon.get_fecs_ctx_state_store_major_rev_id(g);
		netlist_vars->dynamic = true;
	}

	for (; net < max_netlist_num; net++) {
		if (g->ops.netlist.get_netlist_name(g, net, name) != 0) {
			nvgpu_warn(g, "invalid netlist index %d", net);
			continue;
		}

		netlist_fw = nvgpu_request_firmware(g, name, 0);
		if (netlist_fw == NULL) {
			nvgpu_warn(g, "failed to load netlist %s", name);
			continue;
		}

		netlist = (struct netlist_image *)(uintptr_t)netlist_fw->data;

		for (i = 0; i < netlist->header.regions; i++) {
			u8 *src = ((u8 *)netlist + netlist->regions[i].data_offset);
			u32 size = netlist->regions[i].data_size;

			 err = nvgpu_netlist_handle_region_id(g,
					netlist->regions[i].region_id,
					src, size, &major_v, &netlist_num,
					netlist_vars);
			if (err != 0) {
				goto clean_up;
			}
		}

		if (!nvgpu_netlist_is_valid(net, major_v, major_v_hw)) {
			nvgpu_log_info(g, "skip %s: major_v 0x%08x doesn't match hw 0x%08x",
				name, major_v, major_v_hw);
			goto clean_up;
		}

		g->netlist_valid = true;

		nvgpu_release_firmware(g, netlist_fw);
		nvgpu_log_fn(g, "done");
		goto done;

clean_up:
		g->netlist_valid = false;
		nvgpu_kfree(g, netlist_vars->ucode.fecs.inst.l);
		nvgpu_kfree(g, netlist_vars->ucode.fecs.data.l);
		nvgpu_kfree(g, netlist_vars->ucode.gpccs.inst.l);
		nvgpu_kfree(g, netlist_vars->ucode.gpccs.data.l);
		nvgpu_kfree(g, netlist_vars->sw_bundle_init.l);
		nvgpu_kfree(g, netlist_vars->sw_bundle64_init.l);
		nvgpu_kfree(g, netlist_vars->sw_veid_bundle_init.l);
		nvgpu_kfree(g, netlist_vars->sw_method_init.l);
		nvgpu_kfree(g, netlist_vars->sw_ctx_load.l);
		nvgpu_kfree(g, netlist_vars->sw_non_ctx_load.l);
#if defined(CONFIG_NVGPU_NON_FUSA)
		nvgpu_kfree(g, netlist_vars->sw_non_ctx_local_compute_load.l);
		nvgpu_kfree(g, netlist_vars->sw_non_ctx_global_compute_load.l);
#ifdef CONFIG_NVGPU_GRAPHICS
		nvgpu_kfree(g, netlist_vars->sw_non_ctx_local_gfx_load.l);
		nvgpu_kfree(g, netlist_vars->sw_non_ctx_global_gfx_load.l);
#endif
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.sys.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.gpc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.tpc.l);
#ifdef CONFIG_NVGPU_GRAPHICS
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.zcull_gpc.l);
#endif
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.ppc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_sys.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_gpc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_tpc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_ppc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_sys.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.fbp.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_gpc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.fbp_router.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.gpc_router.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_ltc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_fbpa.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_sys_router.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_pma.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_rop.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_ucgpc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.etpc.l);
#if defined(CONFIG_NVGPU_NON_FUSA)
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.sys_compute.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.gpc_compute.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.tpc_compute.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.ppc_compute.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.etpc_compute.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.lts_bc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.lts_uc.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.sys_gfx.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.gpc_gfx.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.tpc_gfx.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.ppc_gfx.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.etpc_gfx.l);
#endif
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_cau.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_sys_control.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_fbp_control.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_gpc_control.l);
		nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_pma_control.l);
#endif /* CONFIG_NVGPU_DEBUGGER */
		nvgpu_release_firmware(g, netlist_fw);
		err = -ENOENT;
	}

done:
	if (g->netlist_valid) {
		nvgpu_log_info(g, "netlist image %s loaded", name);
		return 0;
	} else {
		nvgpu_err(g, "failed to load netlist image!!");
		return err;
	}
}

int nvgpu_netlist_init_ctx_vars(struct gk20a *g)
{
	int err;

	if (g->netlist_valid == true) {
		return 0;
	}

	g->netlist_vars = nvgpu_kzalloc(g, sizeof(*g->netlist_vars));
	if (g->netlist_vars == NULL) {
		return -ENOMEM;
	}

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		err = nvgpu_init_sim_netlist_ctx_vars(g);
		if (err != 0) {
			nvgpu_err(g, "nvgpu_init_sim_netlist_ctx_vars failed!");
		}
	} else
#endif
	{
		err = nvgpu_netlist_init_ctx_vars_fw(g);
		if (err != 0) {
			nvgpu_err(g, "nvgpu_netlist_init_ctx_vars_fw failed!");
		}
	}
#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_netlist_print_ctxsw_reg_info(g);
#endif

	return err;
}

void nvgpu_netlist_deinit_ctx_vars(struct gk20a *g)
{
	struct nvgpu_netlist_vars *netlist_vars = g->netlist_vars;

	if (netlist_vars == NULL) {
		return;
	}

	g->netlist_valid = false;
	nvgpu_kfree(g, netlist_vars->ucode.fecs.inst.l);
	nvgpu_kfree(g, netlist_vars->ucode.fecs.data.l);
	nvgpu_kfree(g, netlist_vars->ucode.gpccs.inst.l);
	nvgpu_kfree(g, netlist_vars->ucode.gpccs.data.l);
	nvgpu_kfree(g, netlist_vars->sw_bundle_init.l);
	nvgpu_kfree(g, netlist_vars->sw_bundle64_init.l);
	nvgpu_kfree(g, netlist_vars->sw_veid_bundle_init.l);
	nvgpu_kfree(g, netlist_vars->sw_method_init.l);
	nvgpu_kfree(g, netlist_vars->sw_ctx_load.l);
	nvgpu_kfree(g, netlist_vars->sw_non_ctx_load.l);
#if defined(CONFIG_NVGPU_NON_FUSA)
	nvgpu_kfree(g, netlist_vars->sw_non_ctx_local_compute_load.l);
	nvgpu_kfree(g, netlist_vars->sw_non_ctx_global_compute_load.l);
#ifdef CONFIG_NVGPU_GRAPHICS
	nvgpu_kfree(g, netlist_vars->sw_non_ctx_local_gfx_load.l);
	nvgpu_kfree(g, netlist_vars->sw_non_ctx_global_gfx_load.l);
#endif
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.sys.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.gpc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.tpc.l);
#ifdef CONFIG_NVGPU_GRAPHICS
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.zcull_gpc.l);
#endif
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.ppc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_sys.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_gpc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_tpc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_ppc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_sys.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.fbp.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_gpc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.fbp_router.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.gpc_router.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_ltc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_fbpa.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_sys_router.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_pma.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_rop.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_ucgpc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.etpc.l);

#if defined(CONFIG_NVGPU_NON_FUSA)
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.sys_compute.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.gpc_compute.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.tpc_compute.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.ppc_compute.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.etpc_compute.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.lts_bc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.lts_uc.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.sys_gfx.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.gpc_gfx.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.tpc_gfx.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.ppc_gfx.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.etpc_gfx.l);
#endif
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.pm_cau.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_sys_control.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_fbp_control.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_gpc_control.l);
	nvgpu_kfree(g, netlist_vars->ctxsw_regs.perf_pma_control.l);
#endif /* CONFIG_NVGPU_DEBUGGER */

	nvgpu_kfree(g, netlist_vars);
	g->netlist_vars = NULL;
}

struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_load_av_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_non_ctx_load;
}

struct netlist_aiv_list *nvgpu_netlist_get_sw_ctx_load_aiv_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_ctx_load;
}

struct netlist_av_list *nvgpu_netlist_get_sw_method_init_av_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_method_init;
}

struct netlist_av_list *nvgpu_netlist_get_sw_bundle_init_av_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_bundle_init;
}

struct netlist_av_list *nvgpu_netlist_get_sw_veid_bundle_init_av_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_veid_bundle_init;
}

struct netlist_av64_list *nvgpu_netlist_get_sw_bundle64_init_av64_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_bundle64_init;
}

u32 nvgpu_netlist_get_fecs_inst_count(struct gk20a *g)
{
	return g->netlist_vars->ucode.fecs.inst.count;
}

u32 nvgpu_netlist_get_fecs_data_count(struct gk20a *g)
{
	return g->netlist_vars->ucode.fecs.data.count;
}

u32 nvgpu_netlist_get_gpccs_inst_count(struct gk20a *g)
{
	return g->netlist_vars->ucode.gpccs.inst.count;
}

u32 nvgpu_netlist_get_gpccs_data_count(struct gk20a *g)
{
	return g->netlist_vars->ucode.gpccs.data.count;
}

u32 *nvgpu_netlist_get_fecs_inst_list(struct gk20a *g)
{
	return g->netlist_vars->ucode.fecs.inst.l;
}

u32 *nvgpu_netlist_get_fecs_data_list(struct gk20a *g)
{
	return g->netlist_vars->ucode.fecs.data.l;
}

u32 *nvgpu_netlist_get_gpccs_inst_list(struct gk20a *g)
{
	return g->netlist_vars->ucode.gpccs.inst.l;
}

u32 *nvgpu_netlist_get_gpccs_data_list(struct gk20a *g)
{
	return g->netlist_vars->ucode.gpccs.data.l;
}

struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_local_compute_load_av_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_non_ctx_local_compute_load;
}

struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_global_compute_load_av_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_non_ctx_global_compute_load;
}

#ifdef CONFIG_NVGPU_GRAPHICS
struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_local_gfx_load_av_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_non_ctx_local_gfx_load;
}

struct netlist_av_list *nvgpu_netlist_get_sw_non_ctx_global_gfx_load_av_list(
							struct gk20a *g)
{
	return &g->netlist_vars->sw_non_ctx_global_gfx_load;
}
#endif /* CONFIG_NVGPU_GRAPHICS */

#ifdef CONFIG_NVGPU_DEBUGGER
struct netlist_aiv_list *nvgpu_netlist_get_sys_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.sys;
}

struct netlist_aiv_list *nvgpu_netlist_get_gpc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.gpc;
}

struct netlist_aiv_list *nvgpu_netlist_get_tpc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.tpc;
}

#ifdef CONFIG_NVGPU_GRAPHICS
struct netlist_aiv_list *nvgpu_netlist_get_zcull_gpc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.zcull_gpc;
}
#endif

struct netlist_aiv_list *nvgpu_netlist_get_ppc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.ppc;
}

struct netlist_aiv_list *nvgpu_netlist_get_pm_sys_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.pm_sys;
}

struct netlist_aiv_list *nvgpu_netlist_get_pm_gpc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.pm_gpc;
}

struct netlist_aiv_list *nvgpu_netlist_get_pm_tpc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.pm_tpc;
}

struct netlist_aiv_list *nvgpu_netlist_get_pm_ppc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.pm_ppc;
}

struct netlist_aiv_list *nvgpu_netlist_get_perf_sys_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.perf_sys;
}

struct netlist_aiv_list *nvgpu_netlist_get_perf_gpc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.perf_gpc;
}

struct netlist_aiv_list *nvgpu_netlist_get_fbp_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.fbp;
}

struct netlist_aiv_list *nvgpu_netlist_get_fbp_router_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.fbp_router;
}

struct netlist_aiv_list *nvgpu_netlist_get_gpc_router_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.gpc_router;
}

struct netlist_aiv_list *nvgpu_netlist_get_pm_ltc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.pm_ltc;
}

struct netlist_aiv_list *nvgpu_netlist_get_pm_fbpa_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.pm_fbpa;
}

struct netlist_aiv_list *nvgpu_netlist_get_perf_sys_router_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.perf_sys_router;
}

struct netlist_aiv_list *nvgpu_netlist_get_perf_pma_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.perf_pma;
}

struct netlist_aiv_list *nvgpu_netlist_get_pm_rop_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.pm_rop;
}

struct netlist_aiv_list *nvgpu_netlist_get_pm_ucgpc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.pm_ucgpc;
}

struct netlist_aiv_list *nvgpu_netlist_get_etpc_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.etpc;
}

struct netlist_aiv_list *nvgpu_netlist_get_pm_cau_ctxsw_regs(struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.pm_cau;
}

struct netlist_aiv_list *nvgpu_netlist_get_perf_sys_control_ctxsw_regs(
		struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.perf_sys_control;
}

struct netlist_aiv_list *nvgpu_netlist_get_perf_fbp_control_ctxsw_regs(
		struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.perf_fbp_control;
}

struct netlist_aiv_list *nvgpu_netlist_get_perf_gpc_control_ctxsw_regs(
		struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.perf_gpc_control;
}

struct netlist_aiv_list *nvgpu_netlist_get_perf_pma_control_ctxsw_regs(
		struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.perf_pma_control;
}

u32 nvgpu_netlist_get_ppc_ctxsw_regs_count(struct gk20a *g)
{
	u32 count = nvgpu_netlist_get_ppc_ctxsw_regs(g)->count;

#if defined(CONFIG_NVGPU_NON_FUSA)
	if (count == 0U) {
		count = nvgpu_netlist_get_ppc_compute_ctxsw_regs(g)->count;
		count = nvgpu_safe_add_u32(count,
			nvgpu_netlist_get_ppc_gfx_ctxsw_regs(g)->count);
	}
#endif
	return count;
}

u32 nvgpu_netlist_get_gpc_ctxsw_regs_count(struct gk20a *g)
{
	u32 count = nvgpu_netlist_get_gpc_ctxsw_regs(g)->count;

#if defined(CONFIG_NVGPU_NON_FUSA)
	if (count == 0U) {
		count = nvgpu_netlist_get_gpc_compute_ctxsw_regs(g)->count;
		count = nvgpu_safe_add_u32(count,
			nvgpu_netlist_get_gpc_gfx_ctxsw_regs(g)->count);
	}
#endif
	return count;
}

u32 nvgpu_netlist_get_tpc_ctxsw_regs_count(struct gk20a *g)
{
	u32 count = nvgpu_netlist_get_tpc_ctxsw_regs(g)->count;

#if defined(CONFIG_NVGPU_NON_FUSA)
	if (count == 0U) {
		count = nvgpu_netlist_get_tpc_compute_ctxsw_regs(g)->count;
		count = nvgpu_safe_add_u32(count,
			nvgpu_netlist_get_tpc_gfx_ctxsw_regs(g)->count);
	}
#endif
	return count;
}

u32 nvgpu_netlist_get_etpc_ctxsw_regs_count(struct gk20a *g)
{
	u32 count = nvgpu_netlist_get_etpc_ctxsw_regs(g)->count;

#if defined(CONFIG_NVGPU_NON_FUSA)
	if (count == 0U) {
		count = nvgpu_netlist_get_etpc_compute_ctxsw_regs(g)->count;
		count = nvgpu_safe_add_u32(count,
			nvgpu_netlist_get_etpc_gfx_ctxsw_regs(g)->count);
	}
#endif
	return count;
}

void nvgpu_netlist_print_ctxsw_reg_info(struct gk20a *g)
{
	nvgpu_log_info(g, "<<<<---------- CTXSW'ed register info ---------->>>>");
	nvgpu_log_info(g, "GRCTX_REG_LIST_SYS_COUNT                     :%d",
			nvgpu_netlist_get_sys_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_GPC_COUNT                     :%d",
			nvgpu_netlist_get_gpc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_TPC_COUNT                     :%d",
			nvgpu_netlist_get_tpc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_ZCULL_GPC_COUNT               :%d",
			nvgpu_netlist_get_zcull_gpc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PM_SYS_COUNT                  :%d",
			nvgpu_netlist_get_pm_sys_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PM_GPC_COUNT                  :%d",
			nvgpu_netlist_get_pm_gpc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PM_TPC_COUNT                  :%d",
			nvgpu_netlist_get_pm_tpc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PPC_COUNT                     :%d",
			nvgpu_netlist_get_ppc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_ETPC_COUNT                    :%d",
			nvgpu_netlist_get_etpc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PM_PPC_COUNT                  :%d",
			nvgpu_netlist_get_pm_ppc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_SYS_COUNT                :%d",
			nvgpu_netlist_get_perf_sys_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_SYSROUTER_COUNT          :%d",
			nvgpu_netlist_get_perf_sys_router_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_SYS_CONTROL_COUNT        :%d",
			nvgpu_netlist_get_perf_sys_control_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_PMA_COUNT                :%d",
			nvgpu_netlist_get_perf_pma_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_FBP_COUNT                :%d",
			nvgpu_netlist_get_fbp_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_FBPROUTER_COUNT          :%d",
			nvgpu_netlist_get_fbp_router_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_GPC_COUNT                :%d",
			nvgpu_netlist_get_perf_gpc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_GPCROUTER_COUNT          :%d",
			nvgpu_netlist_get_gpc_router_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PM_LTC_COUNT                  :%d",
			nvgpu_netlist_get_pm_ltc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PM_ROP_COUNT                  :%d",
			nvgpu_netlist_get_pm_rop_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PM_UNICAST_GPC_COUNT          :%d",
			nvgpu_netlist_get_pm_ucgpc_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PM_CAU_COUNT                  :%d",
			nvgpu_netlist_get_pm_cau_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PM_FBPA_COUNT                 :%d",
			nvgpu_netlist_get_pm_fbpa_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_FBP_CONTROL_COUNT        :%d",
			nvgpu_netlist_get_perf_fbp_control_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_GPC_CONTROL_COUNT        :%d",
			nvgpu_netlist_get_perf_gpc_control_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PERF_PMA_CONTROL_COUNT        :%d",
			nvgpu_netlist_get_perf_pma_control_ctxsw_regs(g)->count);
#if defined(CONFIG_NVGPU_NON_FUSA)
	nvgpu_log_info(g, "GRCTX_REG_LIST_SYS_(COMPUTE/GRAPICS)_COUNT   :%d %d",
		nvgpu_netlist_get_sys_compute_ctxsw_regs(g)->count,
		nvgpu_netlist_get_sys_gfx_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_GPC_(COMPUTE/GRAPHICS)_COUNT  :%d %d",
		nvgpu_netlist_get_gpc_compute_ctxsw_regs(g)->count,
		nvgpu_netlist_get_gpc_gfx_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_TPC_(COMPUTE/GRAPHICS)_COUNT  :%d %d",
		nvgpu_netlist_get_tpc_compute_ctxsw_regs(g)->count,
		nvgpu_netlist_get_tpc_gfx_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_PPC_(COMPUTE/GRAHPICS)_COUNT  :%d %d",
		nvgpu_netlist_get_ppc_compute_ctxsw_regs(g)->count,
		nvgpu_netlist_get_ppc_gfx_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_ETPC_(COMPUTE/GRAPHICS)_COUNT :%d %d",
		nvgpu_netlist_get_etpc_compute_ctxsw_regs(g)->count,
		nvgpu_netlist_get_etpc_gfx_ctxsw_regs(g)->count);
	nvgpu_log_info(g, "GRCTX_REG_LIST_LTS_BC_COUNT                  :%d",
		nvgpu_netlist_get_lts_ctxsw_regs(g)->count);
#endif
}

#endif /* CONFIG_NVGPU_DEBUGGER */

#ifdef CONFIG_NVGPU_NON_FUSA
void nvgpu_netlist_set_fecs_inst_count(struct gk20a *g, u32 count)
{
	g->netlist_vars->ucode.fecs.inst.count = count;
}

void nvgpu_netlist_set_fecs_data_count(struct gk20a *g, u32 count)
{
	g->netlist_vars->ucode.fecs.data.count = count;
}

void nvgpu_netlist_set_gpccs_inst_count(struct gk20a *g, u32 count)
{
	g->netlist_vars->ucode.gpccs.inst.count = count;
}

void nvgpu_netlist_set_gpccs_data_count(struct gk20a *g, u32 count)
{
	g->netlist_vars->ucode.gpccs.data.count = count;
}

struct netlist_u32_list *nvgpu_netlist_get_fecs_inst(struct gk20a *g)
{
	return &g->netlist_vars->ucode.fecs.inst;
}

struct netlist_u32_list *nvgpu_netlist_get_fecs_data(struct gk20a *g)
{
	return &g->netlist_vars->ucode.fecs.data;
}

struct netlist_u32_list *nvgpu_netlist_get_gpccs_inst(struct gk20a *g)
{
	return &g->netlist_vars->ucode.gpccs.inst;
}

struct netlist_u32_list *nvgpu_netlist_get_gpccs_data(struct gk20a *g)
{
	return &g->netlist_vars->ucode.gpccs.data;
}


void nvgpu_netlist_vars_set_dynamic(struct gk20a *g, bool set)
{
	g->netlist_vars->dynamic = set;
}

void nvgpu_netlist_vars_set_buffer_size(struct gk20a *g, u32 size)
{
	g->netlist_vars->buffer_size = size;
}

void nvgpu_netlist_vars_set_regs_base_index(struct gk20a *g, u32 index)
{
	g->netlist_vars->regs_base_index = index;
}

#ifdef CONFIG_NVGPU_DEBUGGER
struct netlist_aiv_list *nvgpu_netlist_get_sys_compute_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.sys_compute;
}

struct netlist_aiv_list *nvgpu_netlist_get_gpc_compute_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.gpc_compute;
}

struct netlist_aiv_list *nvgpu_netlist_get_tpc_compute_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.tpc_compute;
}

struct netlist_aiv_list *nvgpu_netlist_get_ppc_compute_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.ppc_compute;
}

struct netlist_aiv_list *nvgpu_netlist_get_etpc_compute_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.etpc_compute;
}

struct netlist_aiv_list *nvgpu_netlist_get_lts_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.lts_bc;
}

struct netlist_aiv_list *nvgpu_netlist_get_sys_gfx_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.sys_gfx;
}

struct netlist_aiv_list *nvgpu_netlist_get_gpc_gfx_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.gpc_gfx;
}

struct netlist_aiv_list *nvgpu_netlist_get_tpc_gfx_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.tpc_gfx;
}

struct netlist_aiv_list *nvgpu_netlist_get_ppc_gfx_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.ppc_gfx;
}

struct netlist_aiv_list *nvgpu_netlist_get_etpc_gfx_ctxsw_regs(
							struct gk20a *g)
{
	return &g->netlist_vars->ctxsw_regs.etpc_gfx;
}

u32 nvgpu_netlist_get_sys_ctxsw_regs_count(struct gk20a *g)
{
	u32 count = nvgpu_netlist_get_sys_compute_ctxsw_regs(g)->count;

	count = nvgpu_safe_add_u32(count,
		nvgpu_netlist_get_sys_gfx_ctxsw_regs(g)->count);
	return count;
}
#endif /* CONFIG_NVGPU_DEBUGGER */
#endif
