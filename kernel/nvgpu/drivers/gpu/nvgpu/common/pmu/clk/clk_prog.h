/*
* Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_CLK_PROG_H
#define NVGPU_CLK_PROG_H

#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/boardobjgrp_e255.h>
#include <nvgpu/boardobjgrpmask.h>
#include <nvgpu/pmu/clk/clk.h>

struct clk_prog_1x_master;

typedef int vf_flatten(struct gk20a *g, struct nvgpu_clk_pmupstate *pclk,
			struct clk_prog_1x_master *p1xmaster,
			u8 clk_domain_idx, u16 *pfreqmaxlastmhz);

typedef int vf_lookup(struct gk20a *g, struct nvgpu_clk_pmupstate *pclk,
			struct clk_prog_1x_master *p1xmaster,
			u8 *slave_clk_domain_idx, u16 *pclkmhz,
			u32 *pvoltuv, u8 rail);

typedef int get_slaveclk(struct gk20a *g, struct nvgpu_clk_pmupstate *pclk,
			struct clk_prog_1x_master *p1xmaster,
			u8 slave_clk_domain_idx, u16 *pclkmhz,
			u16 masterclkmhz, u8 *ratio);

typedef int get_fpoints(struct gk20a *g, struct nvgpu_clk_pmupstate *pclk,
			struct clk_prog_1x_master *p1xmaster,
			u32 *pfpointscount,
			u16 **ppfreqpointsinmhz, u8 rail);


struct clk_prog {
	struct pmu_board_obj super;
};

struct clk_prog_1x {
	struct clk_prog super;
	u8  source;
	u16 freq_max_mhz;
	union ctrl_clk_clk_prog_1x_source_data source_data;
};

struct clk_prog_1x_master {
	struct clk_prog_1x super;
	bool b_o_c_o_v_enabled;
	struct ctrl_clk_clk_prog_1x_master_vf_entry *p_vf_entries;
	struct ctrl_clk_clk_delta deltas;
	union ctrl_clk_clk_prog_1x_master_source_data source_data;
	vf_flatten *vfflatten;
	vf_lookup *vflookup;
	get_fpoints *getfpoints;
	get_slaveclk *getslaveclk;
};

struct clk_prog_1x_master_ratio {
	struct clk_prog_1x_master super;
	struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry *p_slave_entries;
};

struct clk_prog_1x_master_table {
	struct clk_prog_1x_master super;
	struct ctrl_clk_clk_prog_1x_master_table_slave_entry *p_slave_entries;
};

struct clk_prog_3x_master {
	bool b_o_c_o_v_enabled;
	struct ctrl_clk_clk_prog_1x_master_vf_entry *p_vf_entries;
	struct ctrl_clk_clk_delta deltas;
	union ctrl_clk_clk_prog_1x_master_source_data source_data;
	vf_flatten *vfflatten;
	vf_lookup *vflookup;
	get_fpoints *getfpoints;
	get_slaveclk *getslaveclk;
};

struct clk_prog_3x_master_ratio {
	struct ctrl_clk_clk_prog_1x_master_ratio_slave_entry *p_slave_entries;
};

struct clk_prog_3x_master_table {
	struct ctrl_clk_clk_prog_1x_master_table_slave_entry *p_slave_entries;
};

struct clk_prog_35_master {
	struct clk_prog_1x super;
	struct clk_prog_3x_master master;
	struct ctrl_clk_clk_prog_35_master_sec_vf_entry_voltrail
			*p_voltrail_sec_vf_entries;
};

struct clk_prog_35_master_ratio {
	struct clk_prog_35_master super;
	struct clk_prog_3x_master_ratio ratio;
};

struct clk_prog_35_master_table {
	struct clk_prog_35_master super;
	struct clk_prog_3x_master_table table;
};

struct nvgpu_clk_progs {
	struct boardobjgrp_e255 super;
	u8 slave_entry_count;
	u8 vf_entry_count;
	u8 vf_sec_entry_count;
};

#define CLK_CLK_PROG_GET(pclk, idx)\
	((struct clk_prog *)(void *)BOARDOBJGRP_OBJ_GET_BY_IDX(\
		&pclk->clk_progobjs->super.super, (u8)(idx)))


int clk_prog_init_pmupstate(struct gk20a *g);
void clk_prog_free_pmupstate(struct gk20a *g);
int clk_prog_sw_setup(struct gk20a *g);
int clk_prog_pmu_setup(struct gk20a *g);
#endif /* NVGPU_CLK_PROG_H */
