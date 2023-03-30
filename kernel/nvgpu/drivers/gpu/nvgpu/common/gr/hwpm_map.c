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

#include <nvgpu/gk20a.h>
#include <nvgpu/netlist.h>
#include <nvgpu/log.h>
#include <nvgpu/sort.h>
#include <nvgpu/kmem.h>
#include <nvgpu/bsearch.h>
#include <nvgpu/fbp.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/hwpm_map.h>

/* needed for pri_is_ppc_addr_shared */
#include "hal/gr/gr/gr_pri_gk20a.h"

#define NV_PCFG_BASE		0x00088000U
#define NV_PERF_PMM_FBP_ROUTER_STRIDE 0x0200U
#define NV_PERF_PMMGPCROUTER_STRIDE	0x0200U
#define NV_XBAR_MXBAR_PRI_GPC_GNIC_STRIDE	0x0020U

/* Dummy address for ctxsw'ed pri reg checksum. */
#define CTXSW_PRI_CHECKSUM_DUMMY_REG  0x00ffffffU

int nvgpu_gr_hwpm_map_init(struct gk20a *g, struct nvgpu_gr_hwpm_map **hwpm_map,
	u32 size)
{
	struct nvgpu_gr_hwpm_map *tmp_map;

	nvgpu_log(g, gpu_dbg_gr, "size = %u", size);

	if (size == 0U) {
		return -EINVAL;
	}

	tmp_map = nvgpu_kzalloc(g, sizeof(*tmp_map));
	if (tmp_map == NULL) {
		return -ENOMEM;
	}

	tmp_map->pm_ctxsw_image_size = size;
	tmp_map->init = false;

	*hwpm_map = tmp_map;

	return 0;
}

void nvgpu_gr_hwpm_map_deinit(struct gk20a *g,
	struct nvgpu_gr_hwpm_map *hwpm_map)
{
	if (hwpm_map->init) {
		nvgpu_big_free(g, hwpm_map->map);
	}

	nvgpu_kfree(g, hwpm_map);
}

u32 nvgpu_gr_hwpm_map_get_size(struct nvgpu_gr_hwpm_map *hwpm_map)
{
	return hwpm_map->pm_ctxsw_image_size;
}

static int map_cmp(const void *a, const void *b)
{
	const struct ctxsw_buf_offset_map_entry *e1;
	const struct ctxsw_buf_offset_map_entry *e2;

	e1 = (const struct ctxsw_buf_offset_map_entry *)a;
	e2 = (const struct ctxsw_buf_offset_map_entry *)b;

	if (e1->addr < e2->addr) {
		return -1;
	}

	if (e1->addr > e2->addr) {
		return 1;
	}
	return 0;
}

static int add_ctxsw_buffer_map_entries_pmsys(
	struct ctxsw_buf_offset_map_entry *map,
	struct netlist_aiv_list *regs,	u32 *count, u32 *offset,
	u32 max_cnt, u32 base, u32 mask)
{
	u32 idx;
	u32 cnt = *count;
	u32 off = *offset;

	if ((cnt + regs->count) > max_cnt) {
		return -EINVAL;
	}

	for (idx = 0; idx < regs->count; idx++) {
		if ((base + (regs->l[idx].addr & mask)) < 0xFFFU) {
			map[cnt].addr = base + (regs->l[idx].addr & mask)
					+ NV_PCFG_BASE;
		} else {
			map[cnt].addr = base + (regs->l[idx].addr & mask);
		}
		map[cnt++].offset = off;
		off += 4U;
	}
	*count = cnt;
	*offset = off;
	return 0;
}

static int add_ctxsw_buffer_map_entries_pmgpc(struct gk20a *g,
					struct ctxsw_buf_offset_map_entry *map,
					struct netlist_aiv_list *regs,
					u32 *count, u32 *offset,
					u32 max_cnt, u32 base, u32 mask)
{
	u32 idx;
	u32 cnt = *count;
	u32 off = *offset;

	if ((cnt + regs->count) > max_cnt) {
		return -EINVAL;
	}

	/* NOTE: The PPC offsets get added to the pm_gpc list if numPpc <= 1
	 * To handle the case of PPC registers getting added into GPC, the below
	 * code specifically checks for any PPC offsets and adds them using
	 * proper mask
	 */
	for (idx = 0; idx < regs->count; idx++) {
		/* Check if the address is PPC address */
		if (pri_is_ppc_addr_shared(g, regs->l[idx].addr & mask)) {
			u32 ppc_in_gpc_base = nvgpu_get_litter_value(g,
						GPU_LIT_PPC_IN_GPC_BASE);
			u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g,
						GPU_LIT_PPC_IN_GPC_STRIDE);
			/* Use PPC mask instead of the GPC mask provided */
			u32 ppcmask = ppc_in_gpc_stride - 1U;

			map[cnt].addr = base + ppc_in_gpc_base
					+ (regs->l[idx].addr & ppcmask);
		} else {
			map[cnt].addr = base + (regs->l[idx].addr & mask);
		}
		map[cnt++].offset = off;
		off += 4U;
	}
	*count = cnt;
	*offset = off;
	return 0;
}

static int add_ctxsw_buffer_map_entries(struct ctxsw_buf_offset_map_entry *map,
					struct netlist_aiv_list *regs,
					u32 *count, u32 *offset,
					u32 max_cnt, u32 base, u32 mask)
{
	u32 idx;
	u32 cnt = *count;
	u32 off = *offset;

	if ((cnt + regs->count) > max_cnt) {
		return -EINVAL;
	}

	for (idx = 0; idx < regs->count; idx++) {
		map[cnt].addr = base + (regs->l[idx].addr & mask);
		map[cnt++].offset = off;
		off += 4U;
	}
	*count = cnt;
	*offset = off;
	return 0;
}

/* Helper function to add register entries to the register map for all
 * subunits
 */
static int add_ctxsw_buffer_map_entries_subunits(
				struct ctxsw_buf_offset_map_entry *map,
				struct netlist_aiv_list *regs,
				u32 *count, u32 *offset,
				u32 max_cnt, u32 base, u32 num_units,
				u32 active_unit_mask, u32 stride, u32 mask)
{
	u32 unit;
	u32 idx;
	u32 cnt = *count;
	u32 off = *offset;

	if ((cnt + (regs->count * num_units)) > max_cnt) {
		return -EINVAL;
	}

	/* Data is interleaved for units in ctxsw buffer */
	for (idx = 0; idx < regs->count; idx++) {
		for (unit = 0; unit < num_units; unit++) {
			if ((active_unit_mask & BIT32(unit)) != 0U) {
				map[cnt].addr = base +
						(regs->l[idx].addr & mask) +
						(unit * stride);
				map[cnt++].offset = off;
				off += 4U;

				/*
				 * The ucode computes and saves the checksum of
				 * all ctxsw'ed register values within a list.
				 * Entries with addr=0x00ffffff are placeholder
				 * for these checksums.
				 *
				 * There is only one checksum for a list
				 * even if it contains multiple subunits. Hence,
				 * skip iterating over all subunits for this
				 * entry.
				 */
				if (regs->l[idx].addr ==
						CTXSW_PRI_CHECKSUM_DUMMY_REG) {
					break;
				}
			}
		}
	}
	*count = cnt;
	*offset = off;
	return 0;
}

static int add_ctxsw_buffer_map_entries_gpcs(struct gk20a *g,
					struct ctxsw_buf_offset_map_entry *map,
					u32 *count, u32 *offset, u32 max_cnt,
					struct nvgpu_gr_config *config)
{
	u32 num_gpcs = nvgpu_gr_config_get_gpc_count(config);
	u32 num_ppcs, num_tpcs, gpc_num, base;
	u32 gpc_base = nvgpu_get_litter_value(g, GPU_LIT_GPC_BASE);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_BASE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	for (gpc_num = 0; gpc_num < num_gpcs; gpc_num++) {
		num_tpcs = nvgpu_gr_config_get_gpc_tpc_count(config, gpc_num);
		base = gpc_base + (gpc_stride * gpc_num) + tpc_in_gpc_base;
		if (add_ctxsw_buffer_map_entries_subunits(map,
					nvgpu_netlist_get_pm_tpc_ctxsw_regs(g),
					count, offset, max_cnt, base,
					num_tpcs, ~U32(0U), tpc_in_gpc_stride,
					(tpc_in_gpc_stride - 1U)) != 0) {
			return -EINVAL;
		}

		num_ppcs = nvgpu_gr_config_get_gpc_ppc_count(config, gpc_num);
		base = gpc_base + (gpc_stride * gpc_num) + ppc_in_gpc_base;
		if (add_ctxsw_buffer_map_entries_subunits(map,
					nvgpu_netlist_get_pm_ppc_ctxsw_regs(g),
					count, offset, max_cnt, base, num_ppcs,
					~U32(0U), ppc_in_gpc_stride,
					(ppc_in_gpc_stride - 1U)) != 0) {
			return -EINVAL;
		}

		base = gpc_base + (gpc_stride * gpc_num);
		if (add_ctxsw_buffer_map_entries_pmgpc(g, map,
					nvgpu_netlist_get_pm_gpc_ctxsw_regs(g),
					count, offset, max_cnt, base,
					(gpc_stride - 1U)) != 0) {
			return -EINVAL;
		}

		base = (g->ops.gr.ctxsw_prog.hw_get_pm_gpc_gnic_stride(g)) * gpc_num;
		if (add_ctxsw_buffer_map_entries(map,
				nvgpu_netlist_get_pm_ucgpc_ctxsw_regs(g),
				count, offset, max_cnt, base, ~U32(0U)) != 0) {
			return -EINVAL;
		}

		base = (g->ops.perf.get_pmmgpc_per_chiplet_offset() * gpc_num);
		if (add_ctxsw_buffer_map_entries(map,
				nvgpu_netlist_get_perf_gpc_ctxsw_regs(g),
				count, offset, max_cnt, base, ~U32(0U)) != 0) {
			return -EINVAL;
		}

		base = (NV_PERF_PMMGPCROUTER_STRIDE * gpc_num);
		if (add_ctxsw_buffer_map_entries(map,
				nvgpu_netlist_get_gpc_router_ctxsw_regs(g),
				count, offset, max_cnt, base, ~U32(0U)) != 0) {
			return -EINVAL;
		}

		/* Counter Aggregation Unit, if available */
		if (nvgpu_netlist_get_pm_cau_ctxsw_regs(g)->count != 0U) {
			base = gpc_base + (gpc_stride * gpc_num)
					+ tpc_in_gpc_base;
			if (add_ctxsw_buffer_map_entries_subunits(map,
					nvgpu_netlist_get_pm_cau_ctxsw_regs(g),
					count, offset, max_cnt, base, num_tpcs,
					~U32(0U), tpc_in_gpc_stride,
					(tpc_in_gpc_stride - 1U)) != 0) {
				return -EINVAL;
			}
		}

		*offset = NVGPU_ALIGN(*offset, 256U);

		base = (g->ops.perf.get_pmmgpc_per_chiplet_offset() * gpc_num);
		if (add_ctxsw_buffer_map_entries(map,
				nvgpu_netlist_get_perf_gpc_control_ctxsw_regs(g),
				count, offset, max_cnt, base, ~U32(0U)) != 0) {
			return -EINVAL;
		}

		*offset = NVGPU_ALIGN(*offset, 256U);
	}
	return 0;
}

/*
 * PM CTXSW BUFFER LAYOUT:
 *|=============================================|0x00 <----PM CTXSW BUFFER BASE
 *|        LIST_compressed_pm_ctx_reg_SYS       |Space allocated: numRegs words
 *|    LIST_compressed_nv_perf_ctx_reg_SYS      |Space allocated: numRegs words
 *|  LIST_compressed_nv_perf_ctx_reg_sysrouter  |Space allocated: numRegs words
 *|  PADDING for 256 byte alignment on Maxwell+ |
 *|=============================================|<----256 byte aligned on Maxwell and later
 *| LIST_compressed_nv_perf_sys_control_ctx_regs|Space allocated: numRegs words (+ padding)
 *|        PADDING for 256 byte alignment       |(If reg list is empty, 0 bytes allocated.)
 *|=============================================|<----256 byte aligned
 *|    LIST_compressed_nv_perf_ctx_reg_PMA      |Space allocated: numRegs words (+ padding)
 *|        PADDING for 256 byte alignment       |
 *|=============================================|<----256 byte aligned (if prev segment exists)
 *| LIST_compressed_nv_perf_pma_control_ctx_regs|Space allocated: numRegs words (+ padding)
 *|        PADDING for 256 byte alignment       |(If reg list is empty, 0 bytes allocated.)
 *|=============================================|<----256 byte aligned
 *|    LIST_compressed_nv_perf_fbp_ctx_regs     |Space allocated: numRegs * n words (for n FB units)
 *| LIST_compressed_nv_perf_fbprouter_ctx_regs  |Space allocated: numRegs * n words (for n FB units)
 *|    LIST_compressed_pm_fbpa_ctx_regs         |Space allocated: numRegs * n words (for n FB units)
 *|    LIST_compressed_pm_rop_ctx_regs          |Space allocated: numRegs * n words (for n FB units)
 *|    LIST_compressed_pm_ltc_ctx_regs          |
 *|                                  LTC0 LTS0  |
 *|                                  LTC1 LTS0  |Space allocated: numRegs * n words (for n LTC units)
 *|                                  LTCn LTS0  |
 *|                                  LTC0 LTS1  |
 *|                                  LTC1 LTS1  |
 *|                                  LTCn LTS1  |
 *|                                  LTC0 LTSn  |
 *|                                  LTC1 LTSn  |
 *|                                  LTCn LTSn  |
 *|        PADDING for 256 byte alignment       |
 *|=============================================|<----256 byte aligned on Maxwell and later
 *| LIST_compressed_nv_perf_fbp_control_ctx_regs|Space allocated: numRegs words + padding
 *|        PADDING for 256 byte alignment       |(If reg list is empty, 0 bytes allocated.)
 *|=============================================|<----256 byte aligned on Maxwell and later
 *
 * Each "GPCn PRI register" segment above has this layout:
 *|=============================================|<----256 byte aligned
 *|                            GPC0  REG0 TPC0  |Each GPC has space allocated to accomodate
 *|                                  REG0 TPC1  |    all the GPC/TPC register lists
 *| Lists in each GPC region:        REG0 TPCn  |Per GPC allocated space is always 256 byte aligned
 *|  LIST_pm_ctx_reg_TPC             REG1 TPC0  |
 *|             * numTpcs            REG1 TPC1  |
 *|  LIST_pm_ctx_reg_PPC             REG1 TPCn  |
 *|             * numPpcs            REGn TPC0  |
 *|  LIST_pm_ctx_reg_GPC             REGn TPC1  |
 *|  List_pm_ctx_reg_uc_GPC          REGn TPCn  |
 *|  LIST_nv_perf_ctx_reg_GPC                   |
 *|  LIST_nv_perf_gpcrouter_ctx_reg             |
 *|  LIST_nv_perf_ctx_reg_CAU (Tur)             |
 *|=============================================|
 *| LIST_compressed_nv_perf_gpc_control_ctx_regs|Space allocated: numRegs words + padding
 *|        PADDING for 256 byte alignment       |(If reg list is empty, 0 bytes allocated.)
 *|=============================================|<----256 byte aligned on Maxwell and later
 */

static int nvgpu_gr_hwpm_map_create(struct gk20a *g,
	struct nvgpu_gr_hwpm_map *hwpm_map, struct nvgpu_gr_config *config)
{
	u32 hwpm_ctxsw_buffer_size = hwpm_map->pm_ctxsw_image_size;
	struct ctxsw_buf_offset_map_entry *map;
	u32 hwpm_ctxsw_reg_count_max;
	u32 map_size;
	u32 i, count = 0;
	u32 offset = 0;
	int ret;
	u32 active_fbpa_mask;
	u32 num_fbps = nvgpu_fbp_get_num_fbps(g->fbp);
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 num_fbpas = nvgpu_get_litter_value(g, GPU_LIT_NUM_FBPAS);
	u32 fbpa_stride = nvgpu_get_litter_value(g, GPU_LIT_FBPA_STRIDE);
	u32 num_ltc = g->ops.top.get_max_ltc_per_fbp(g) *
		      g->ops.priv_ring.get_fbp_count(g);

	if (hwpm_ctxsw_buffer_size == 0U) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"no PM Ctxsw buffer memory in context buffer");
		return -EINVAL;
	}

	hwpm_ctxsw_reg_count_max = hwpm_ctxsw_buffer_size >> 2;
	map_size = hwpm_ctxsw_reg_count_max * (u32)sizeof(*map);

	map = nvgpu_big_zalloc(g, map_size);
	if (map == NULL) {
		return -ENOMEM;
	}

	/* Add entries from _LIST_pm_ctx_reg_SYS */
	if (add_ctxsw_buffer_map_entries_pmsys(map,
		nvgpu_netlist_get_pm_sys_ctxsw_regs(g),
		&count, &offset, hwpm_ctxsw_reg_count_max, 0, ~U32(0U)) != 0) {
		goto cleanup;
	}

	/* Add entries from _LIST_nv_perf_ctx_reg_SYS */
	if (add_ctxsw_buffer_map_entries(map,
		nvgpu_netlist_get_perf_sys_ctxsw_regs(g),
		&count, &offset, hwpm_ctxsw_reg_count_max, 0, ~U32(0U)) != 0) {
		goto cleanup;
	}

	/* Add entries from _LIST_nv_perf_sysrouter_ctx_reg*/
	if (add_ctxsw_buffer_map_entries(map,
		nvgpu_netlist_get_perf_sys_router_ctxsw_regs(g),
		&count, &offset, hwpm_ctxsw_reg_count_max, 0, ~U32(0U)) != 0) {
		goto cleanup;
	}

	/* Add entries from _LIST_nv_perf_sys_control_ctx_reg*/
	if (nvgpu_netlist_get_perf_sys_control_ctxsw_regs(g)->count > 0U) {
		offset = NVGPU_ALIGN(offset, 256U);

		ret = add_ctxsw_buffer_map_entries(map,
			nvgpu_netlist_get_perf_sys_control_ctxsw_regs(g),
			&count, &offset,
			hwpm_ctxsw_reg_count_max, 0, ~U32(0U));
		if (ret != 0) {
			goto cleanup;
		}
	}

	if (g->ops.gr.hwpm_map.align_regs_perf_pma) {
		g->ops.gr.hwpm_map.align_regs_perf_pma(&offset);
	}

	/* Add entries from _LIST_nv_perf_pma_ctx_reg*/
	ret = add_ctxsw_buffer_map_entries(map,
		nvgpu_netlist_get_perf_pma_ctxsw_regs(g), &count, &offset,
			hwpm_ctxsw_reg_count_max, 0, ~U32(0U));
	if (ret != 0) {
		goto cleanup;
	}

	offset = NVGPU_ALIGN(offset, 256U);

	/* Add entries from _LIST_nv_perf_pma_control_ctx_reg*/
	ret = add_ctxsw_buffer_map_entries(map,
		nvgpu_netlist_get_perf_pma_control_ctxsw_regs(g), &count, &offset,
			hwpm_ctxsw_reg_count_max, 0, ~U32(0U));
	if (ret != 0) {
		goto cleanup;
	}

	offset = NVGPU_ALIGN(offset, 256U);

	/* Add entries from _LIST_nv_perf_fbp_ctx_regs */
	if (add_ctxsw_buffer_map_entries_subunits(map,
		nvgpu_netlist_get_fbp_ctxsw_regs(g), &count, &offset,
			hwpm_ctxsw_reg_count_max, 0, num_fbps, ~U32(0U),
			g->ops.perf.get_pmmfbp_per_chiplet_offset(),
			~U32(0U)) != 0) {
		goto cleanup;
	}

	/* Add entries from _LIST_nv_perf_fbprouter_ctx_regs */
	if (add_ctxsw_buffer_map_entries_subunits(map,
			nvgpu_netlist_get_fbp_router_ctxsw_regs(g),
			&count, &offset, hwpm_ctxsw_reg_count_max, 0,
			num_fbps, ~U32(0U), NV_PERF_PMM_FBP_ROUTER_STRIDE,
			~U32(0U)) != 0) {
		goto cleanup;
	}

	if (g->ops.gr.hwpm_map.get_active_fbpa_mask) {
		active_fbpa_mask = g->ops.gr.hwpm_map.get_active_fbpa_mask(g);
	} else {
		active_fbpa_mask = ~U32(0U);
	}

	/* Add entries from _LIST_nv_pm_fbpa_ctx_regs */
	if (add_ctxsw_buffer_map_entries_subunits(map,
			nvgpu_netlist_get_pm_fbpa_ctxsw_regs(g),
			&count, &offset, hwpm_ctxsw_reg_count_max, 0,
			num_fbpas, active_fbpa_mask, fbpa_stride, ~U32(0U))
				!= 0) {
		goto cleanup;
	}

	/* Add entries from _LIST_nv_pm_rop_ctx_regs */
	if (add_ctxsw_buffer_map_entries(map,
		nvgpu_netlist_get_pm_rop_ctxsw_regs(g), &count, &offset,
			hwpm_ctxsw_reg_count_max, 0, ~U32(0U)) != 0) {
		goto cleanup;
	}

	/* Add entries from _LIST_compressed_nv_pm_ltc_ctx_regs */
	if (add_ctxsw_buffer_map_entries_subunits(map,
			nvgpu_netlist_get_pm_ltc_ctxsw_regs(g), &count, &offset,
			hwpm_ctxsw_reg_count_max, 0, num_ltc, ~U32(0U),
			ltc_stride, ~U32(0U)) != 0) {
		goto cleanup;
	}

	offset = NVGPU_ALIGN(offset, 256U);

	/* Add entries from _LIST_nv_perf_fbp_control_ctx_regs */
	if (add_ctxsw_buffer_map_entries_subunits(map,
			nvgpu_netlist_get_perf_fbp_control_ctxsw_regs(g),
			&count, &offset, hwpm_ctxsw_reg_count_max, 0,
			num_fbps, ~U32(0U),
			g->ops.perf.get_pmmfbp_per_chiplet_offset(),
			~U32(0U)) != 0) {
		goto cleanup;
	}

	offset = NVGPU_ALIGN(offset, 256U);

	/* Add GPC entries */
	if (add_ctxsw_buffer_map_entries_gpcs(g, map, &count, &offset,
			hwpm_ctxsw_reg_count_max, config) != 0) {
		goto cleanup;
	}

	if (offset > hwpm_ctxsw_buffer_size) {
		nvgpu_err(g, "offset > buffer size");
		goto cleanup;
	}

	sort(map, count, sizeof(*map), map_cmp, NULL);

	hwpm_map->map = map;
	hwpm_map->count = count;
	hwpm_map->init = true;

	nvgpu_log(g, gpu_dbg_hwpm,
		"Reg Addr => HWPM Ctxt switch buffer offset");

	for (i = 0; i < count; i++) {
		nvgpu_log(g, gpu_dbg_hwpm, "%08x => %08x",
			map[i].addr, map[i].offset);
	}

	return 0;

cleanup:
	nvgpu_err(g, "Failed to create HWPM buffer offset map");
	nvgpu_big_free(g, map);
	return -EINVAL;
}

/*
 *  This function will return the 32 bit offset for a priv register if it is
 *  present in the PM context buffer.
 */
int nvgpu_gr_hwmp_map_find_priv_offset(struct gk20a *g,
	struct nvgpu_gr_hwpm_map *hwpm_map,
	u32 addr, u32 *priv_offset, struct nvgpu_gr_config *config)
{
	struct ctxsw_buf_offset_map_entry *map, *result, map_key;
	int err = 0;
	u32 count;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	/* Create map of pri address and pm offset if necessary */
	if (!hwpm_map->init) {
		err = nvgpu_gr_hwpm_map_create(g, hwpm_map, config);
		if (err != 0) {
			return err;
		}
	}

	*priv_offset = 0;

	map = hwpm_map->map;
	count = hwpm_map->count;

	map_key.addr = addr;
	result = nvgpu_bsearch(&map_key, map, count, sizeof(*map), map_cmp);

	if (result != NULL) {
		*priv_offset = result->offset;
	} else {
		err = -EINVAL;
	}

	return err;
}
