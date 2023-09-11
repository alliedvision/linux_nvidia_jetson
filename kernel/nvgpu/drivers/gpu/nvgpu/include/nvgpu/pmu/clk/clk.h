/*
 * general clock structures & definitions
 *
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

#ifndef NVGPU_PMU_CLK_H
#define NVGPU_PMU_CLK_H

#include <nvgpu/types.h>
#include <nvgpu/boardobjgrpmask.h>
#include <nvgpu/boardobjgrp_e32.h>

/* Following include will be removed in further CL */
#include "../../../../common/pmu/boardobj/ucode_boardobj_inf.h"

/*!
 * Valid global VIN ID values
 */
#define	CTRL_CLK_VIN_ID_SYS			0x00000000U
#define	CTRL_CLK_VIN_ID_LTC			0x00000001U
#define	CTRL_CLK_VIN_ID_XBAR			0x00000002U
#define	CTRL_CLK_VIN_ID_GPC0			0x00000003U
#define	CTRL_CLK_VIN_ID_GPC1			0x00000004U
#define	CTRL_CLK_VIN_ID_GPC2			0x00000005U
#define	CTRL_CLK_VIN_ID_GPC3			0x00000006U
#define	CTRL_CLK_VIN_ID_GPC4			0x00000007U
#define	CTRL_CLK_VIN_ID_GPC5			0x00000008U
#define	CTRL_CLK_VIN_ID_GPCS			0x00000009U
#define	CTRL_CLK_VIN_ID_SRAM			0x0000000AU
#define	CTRL_CLK_VIN_ID_UNDEFINED		0x000000FFU
#define	CTRL_CLK_VIN_TYPE_DISABLED		0x00000000U
#define CTRL_CLK_VIN_TYPE_V20			0x00000002U

/* valid clock domain values */
#define CTRL_CLK_DOMAIN_MCLK			0x00000010U
#define CTRL_CLK_DOMAIN_HOSTCLK			0x00000020U
#define CTRL_CLK_DOMAIN_DISPCLK			0x00000040U
#define CTRL_CLK_DOMAIN_GPC2CLK			0x00010000U
#define CTRL_CLK_DOMAIN_XBAR2CLK		0x00040000U
#define CTRL_CLK_DOMAIN_SYS2CLK			0x00800000U
#define CTRL_CLK_DOMAIN_HUB2CLK			0x01000000U
#define CTRL_CLK_DOMAIN_UTILSCLK		0x00040000U
#define CTRL_CLK_DOMAIN_PWRCLK			0x00080000U
#define CTRL_CLK_DOMAIN_NVDCLK			0x00100000U
#define CTRL_CLK_DOMAIN_PCIEGENCLK		0x00200000U
#define CTRL_CLK_DOMAIN_XCLK			0x04000000U
#define CTRL_CLK_DOMAIN_NVL_COMMON		0x08000000U
#define CTRL_CLK_DOMAIN_PEX_REFCLK		0x10000000U
#define CTRL_CLK_DOMAIN_GPCCLK			0x00000001U
#define CTRL_CLK_DOMAIN_XBARCLK			0x00000002U
#define CTRL_CLK_DOMAIN_SYSCLK			0x00000004U
#define CTRL_CLK_DOMAIN_HUBCLK			0x00000008U

#define CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS	16

/*
 *  Try to get gpc2clk, mclk, sys2clk, xbar2clk work for Pascal
 *
 *  mclk is same for both
 *  gpc2clk is 17 for Pascal and 13 for Volta, making it 17
 *    as volta uses gpcclk
 *  sys2clk is 20 in Pascal and 15 in Volta.
 *    Changing for Pascal would break nvdclk of Volta
 *  xbar2clk is 19 in Pascal and 14 in Volta
 *    Changing for Pascal would break pwrclk of Volta
 */
#define CLKWHICH_GPCCLK				1U
#define CLKWHICH_XBARCLK			2U
#define CLKWHICH_SYSCLK				3U
#define CLKWHICH_HUBCLK				4U
#define CLKWHICH_MCLK				5U
#define CLKWHICH_HOSTCLK			6U
#define CLKWHICH_DISPCLK			7U
#define CLKWHICH_XCLK				12U
#define CLKWHICH_XBAR2CLK			14U
#define CLKWHICH_SYS2CLK			15U
#define CLKWHICH_HUB2CLK			16U
#define CLKWHICH_GPC2CLK			17U
#define CLKWHICH_PWRCLK				19U
#define CLKWHICH_NVDCLK				20U
#define CLKWHICH_PCIEGENCLK			26U


/*
 * Mask of all GPC VIN IDs supported by RM
 */
#define CTRL_CLK_LUT_NUM_ENTRIES_MAX		128U
#define CTRL_CLK_LUT_NUM_ENTRIES_GV10x		128U
#define CTRL_CLK_LUT_NUM_ENTRIES_GP10x		100U

/*
 * The Minimum resolution of frequency which is supported
 */
#define FREQ_STEP_SIZE_MHZ			15U

struct gk20a;
struct clk_avfs_fll_objs;
struct nvgpu_clk_domains;
struct nvgpu_clk_progs;
struct nvgpu_clk_vf_points;
struct nvgpu_clk_mclk_state;
struct nvgpu_clk_slave_freq;
struct nvgpu_pmu_perf_change_input_clk_info;
struct clk_vin_device;
struct nvgpu_clk_domain;
struct nvgpu_clk_arb;
struct nvgpu_clk_pmupstate;

struct ctrl_clk_clk_domain_list_item_v1 {
	u32  clk_domain;
	u32  clk_freq_khz;
	u8   regime_id;
	u8   source;
};

struct ctrl_clk_clk_domain_list {
	u8 num_domains;
	struct ctrl_clk_clk_domain_list_item_v1
		clk_domains[CTRL_BOARDOBJ_MAX_BOARD_OBJECTS];
};

struct nvgpu_clk_slave_freq{
	u16 gpc_mhz;
	u16 sys_mhz;
	u16 xbar_mhz;
	u16 host_mhz;
	u16 nvd_mhz;
};

int clk_get_fll_clks_per_clk_domain(struct gk20a *g,
		struct nvgpu_clk_slave_freq *setfllclk);
int nvgpu_pmu_clk_domain_freq_to_volt(struct gk20a *g, u8 clkdomain_idx,
	u32 *pclkmhz, u32 *pvoltuv, u8 railidx);
int nvgpu_pmu_clk_domain_get_from_index(struct gk20a *g, u32 *domain, u32 index);
int nvgpu_pmu_clk_pmu_setup(struct gk20a *g);
int nvgpu_pmu_clk_sw_setup(struct gk20a *g);
int nvgpu_pmu_clk_init(struct gk20a *g);
void nvgpu_pmu_clk_deinit(struct gk20a *g);
u8 nvgpu_pmu_clk_fll_get_fmargin_idx(struct gk20a *g);
int nvgpu_clk_arb_find_slave_points(struct nvgpu_clk_arb *arb,
	struct nvgpu_clk_slave_freq *vf_point);
int nvgpu_clk_vf_point_cache(struct gk20a *g);
int nvgpu_clk_domain_volt_to_freq(struct gk20a *g, u8 clkdomain_idx,
	u32 *pclkmhz, u32 *pvoltuv, u8 railidx);
u16 nvgpu_pmu_clk_fll_get_min_max_freq(struct gk20a *g);
u32 nvgpu_pmu_clk_fll_get_lut_step_size(struct nvgpu_clk_pmupstate *pclk);
int nvgpu_pmu_clk_domain_get_f_points(struct gk20a *g,
	u32 clkapidomain,
	u32 *pfpointscount,
	u16 *pfreqpointsinmhz);
u8 nvgpu_pmu_clk_domain_update_clk_info(struct gk20a *g,
		struct ctrl_clk_clk_domain_list *clk_list);
void clk_set_p0_clk_per_domain(struct gk20a *g, u8 *gpcclk_domain,
		u32 *gpcclk_clkmhz,
		struct nvgpu_clk_slave_freq *vf_point,
		struct nvgpu_pmu_perf_change_input_clk_info *change_input);
unsigned long nvgpu_pmu_clk_mon_init_domains(struct gk20a *g);
#endif /* NVGPU_PMU_CLK_H */
