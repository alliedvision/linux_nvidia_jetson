/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/device.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/soc.h>

#include "fb_ga10b.h"

#include <nvgpu/hw/ga10b/hw_fb_ga10b.h>

#ifdef CONFIG_NVGPU_MIG
#include <nvgpu/grmgr.h>
#endif

#ifdef CONFIG_NVGPU_COMPRESSION
void ga10b_fb_cbc_configure(struct gk20a *g, struct nvgpu_cbc *cbc)
{
	u64 base_divisor = 0ULL;
	u64 top_divisor = 0ULL;
	u64 compbit_store_base = 0ULL;
	u64 compbit_start_pa = 0ULL;
	u64 compbit_store_pa = 0ULL;
	u64 combit_top_size = 0ULL;
	u64 combit_top = 0ULL;
	u32 cbc_max_rval = 0U;

	g->ops.fb.cbc_get_alignment(g, &base_divisor, &top_divisor);

	/*
	 * Update CBC registers
	 * Note: CBC Base value should be updated after CBC MAX
	 */
	combit_top_size = cbc->compbit_backing_size;
	combit_top = combit_top_size / top_divisor;
	nvgpu_assert(combit_top < U64(U32_MAX));
	nvgpu_writel(g, fb_mmu_cbc_top_r(),
		fb_mmu_cbc_top_size_f(u64_lo32(combit_top)));

	cbc_max_rval = nvgpu_readl(g, fb_mmu_cbc_max_r());
	cbc_max_rval = set_field(cbc_max_rval,
		  fb_mmu_cbc_max_comptagline_m(),
		  fb_mmu_cbc_max_comptagline_f(cbc->max_comptag_lines));
	nvgpu_writel(g, fb_mmu_cbc_max_r(), cbc_max_rval);

	if (nvgpu_is_hypervisor_mode(g) &&
			(g->ops.cbc.use_contig_pool != NULL)) {
		/*
		 * As the nvgpu_mem in ga10b holds the physical sgt, call
		 * nvgpu_mem_phys_get_addr to get the physical address.
		 */
		compbit_store_pa = nvgpu_mem_phys_get_addr(g, &cbc->compbit_store.mem);
	} else {
		compbit_store_pa = nvgpu_mem_get_addr(g, &cbc->compbit_store.mem);
	}
	/* must be a multiple of 64KB within allocated memory */
	compbit_store_base = round_up(compbit_store_pa, SZ_64K);
	/* Calculate post-divide cbc address */
	compbit_store_base = compbit_store_base / base_divisor;

	/*
	 * CBC start address is calculated from the CBC_BASE register value
	 * Check that CBC start address lies within cbc allocated memory.
	 */
	compbit_start_pa = compbit_store_base * base_divisor;
	nvgpu_assert(compbit_start_pa >= compbit_store_pa);

	nvgpu_assert(compbit_store_base < U64(U32_MAX));
	nvgpu_writel(g, fb_mmu_cbc_base_r(),
		fb_mmu_cbc_base_address_f(u64_lo32(compbit_store_base)));

	if (nvgpu_platform_is_silicon(g)) {
		/* Make sure cbc is marked safe by MMU */
		cbc_max_rval = nvgpu_readl(g, fb_mmu_cbc_max_r());
		if ((cbc_max_rval & fb_mmu_cbc_max_safe_m()) !=
			fb_mmu_cbc_max_safe_true_f()) {
			nvgpu_err(g,
				"CBC marked unsafe by MMU, check cbc config");
		}
	}

	cbc->compbit_store.base_hw = compbit_store_base;

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_map_v | gpu_dbg_pte,
		"compbit top size: 0x%x,%08x \n",
		(u32)(combit_top_size >> 32),
		(u32)(combit_top_size & 0xffffffffU));

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_map_v | gpu_dbg_pte,
		"compbit mem.pa: 0x%x,%08x cbc_base:0x%llx\n",
		(u32)(compbit_store_pa >> 32),
		(u32)(compbit_store_pa & 0xffffffffU),
		compbit_store_base);
}
#endif

#ifdef CONFIG_NVGPU_MIG
int ga10b_fb_config_veid_smc_map(struct gk20a *g, bool enable)
{
	u32 reg_val;
	u32 gpu_instance_id;
	struct nvgpu_gpu_instance *gpu_instance;
	struct nvgpu_gr_syspipe *gr_syspipe;
	u32 veid_enable_mask = fb_mmu_hypervisor_ctl_use_smc_veid_tables_f(
		fb_mmu_hypervisor_ctl_use_smc_veid_tables_disable_v());
	u32 default_remote_swizid = 0U;

	if (enable) {
		for (gpu_instance_id = 0U;
				gpu_instance_id < g->mig.num_gpu_instances;
				++gpu_instance_id) {

			if (!nvgpu_grmgr_is_mig_type_gpu_instance(
					&g->mig.gpu_instance[gpu_instance_id])) {
				nvgpu_log(g, gpu_dbg_mig, "skip physical instance[%u]",
					gpu_instance_id);
				continue;
			}

			gpu_instance =
				&g->mig.gpu_instance[gpu_instance_id];
			gr_syspipe = &gpu_instance->gr_syspipe;

			reg_val = nvgpu_readl(g,
				fb_mmu_smc_eng_cfg_0_r(gr_syspipe->gr_syspipe_id));

			if (gpu_instance->is_memory_partition_supported) {
				default_remote_swizid = gpu_instance->gpu_instance_id;
			}

			reg_val = set_field(reg_val,
				fb_mmu_smc_eng_cfg_0_remote_swizid_m(),
				fb_mmu_smc_eng_cfg_0_remote_swizid_f(
					default_remote_swizid));

			reg_val = set_field(reg_val,
				fb_mmu_smc_eng_cfg_0_mmu_eng_veid_offset_m(),
				fb_mmu_smc_eng_cfg_0_mmu_eng_veid_offset_f(
					gr_syspipe->veid_start_offset));

			reg_val = set_field(reg_val,
				fb_mmu_smc_eng_cfg_0_veid_max_m(),
				fb_mmu_smc_eng_cfg_0_veid_max_f(
					nvgpu_safe_sub_u32(
						gr_syspipe->max_veid_count_per_tsg, 1U)));

			nvgpu_writel(g,
				fb_mmu_smc_eng_cfg_0_r(gr_syspipe->gr_syspipe_id),
				reg_val);
			nvgpu_log(g, gpu_dbg_mig,
				"[%d] gpu_instance_id[%u] default_remote_swizid[%u] "
					"gr_instance_id[%u] gr_syspipe_id[%u] "
					"veid_start_offset[%u] veid_end_offset[%u] "
					"reg_val[%x] ",
				gpu_instance_id,
				g->mig.gpu_instance[gpu_instance_id].gpu_instance_id,
				default_remote_swizid,
				gr_syspipe->gr_instance_id,
				gr_syspipe->gr_syspipe_id,
				gr_syspipe->veid_start_offset,
				nvgpu_safe_sub_u32(
					nvgpu_safe_add_u32(gr_syspipe->veid_start_offset,
						gr_syspipe->max_veid_count_per_tsg), 1U),
				reg_val);
		}
		veid_enable_mask = fb_mmu_hypervisor_ctl_use_smc_veid_tables_f(
			fb_mmu_hypervisor_ctl_use_smc_veid_tables_enable_v());
	}
	reg_val = nvgpu_readl(g, fb_mmu_hypervisor_ctl_r());
	reg_val &= ~fb_mmu_hypervisor_ctl_use_smc_veid_tables_m();

	reg_val |= veid_enable_mask;

	nvgpu_writel(g, fb_mmu_hypervisor_ctl_r(), reg_val);

	nvgpu_log(g, gpu_dbg_mig,
		"state[%d] reg_val[%x] ",
		enable, reg_val);
	return 0;
}

int ga10b_fb_set_smc_eng_config(struct gk20a *g, bool enable)
{
	u32 reg_val;
	u32 index;
	u32 local_id;
	u32 logical_gpc_id_mask;
	struct nvgpu_gr_syspipe *gr_syspipe;

	for (index = 0U; index < g->mig.num_gpu_instances; index++) {
		if (!nvgpu_grmgr_is_mig_type_gpu_instance(
				&g->mig.gpu_instance[index])) {
			nvgpu_log(g, gpu_dbg_mig, "skip physical instance[%u]",
				index);
			continue;
		}
		gr_syspipe = &g->mig.gpu_instance[index].gr_syspipe;
		logical_gpc_id_mask = 0U;

		if (enable) {
			for (local_id = 0U; local_id < gr_syspipe->num_gpc;
					local_id++) {
				logical_gpc_id_mask |= BIT32(
					gr_syspipe->gpcs[local_id].logical_id);
			}
		}
		reg_val = nvgpu_readl(g, fb_mmu_smc_eng_cfg_1_r(
			gr_syspipe->gr_syspipe_id));
		reg_val = set_field(reg_val,
			fb_mmu_smc_eng_cfg_1_gpc_mask_m(),
			fb_mmu_smc_eng_cfg_1_gpc_mask_f(
				logical_gpc_id_mask));

		nvgpu_writel(g, fb_mmu_smc_eng_cfg_1_r(
			gr_syspipe->gr_syspipe_id), reg_val);

		nvgpu_log(g, gpu_dbg_mig,
			"[%d] gpu_instance_id[%u] gr_syspipe_id[%u] "
				"gr_instance_id[%u] logical_gpc_id_mask[%x] "
				"reg_val[%x] enable[%d] ",
			index,
			g->mig.gpu_instance[index].gpu_instance_id,
			gr_syspipe->gr_syspipe_id,
			gr_syspipe->gr_instance_id,
			logical_gpc_id_mask,
			reg_val,
			enable);
	}

	return 0;
}

int ga10b_fb_set_remote_swizid(struct gk20a *g, bool enable)
{
	u32 reg_val;
	u32 index;
	u32 lce_id;
	struct nvgpu_gr_syspipe *gr_syspipe;
	u32 default_remote_swizid = 0U;
	struct nvgpu_gpu_instance *gpu_instance;
	u32 pbdma_id_mask;
	struct nvgpu_pbdma_info pbdma_info;
	u32 pbdma_index;

	for (index = 0U; index < g->mig.num_gpu_instances; index++) {
		const struct nvgpu_device *gr_dev =
			g->mig.gpu_instance[index].gr_syspipe.gr_dev;
		gpu_instance = &g->mig.gpu_instance[index];
		gr_syspipe = &gpu_instance->gr_syspipe;
		pbdma_id_mask = 0U;

		if (!nvgpu_grmgr_is_mig_type_gpu_instance(
				&g->mig.gpu_instance[index])) {
			nvgpu_log(g, gpu_dbg_mig, "skip physical instance[%u]",
				index);
			continue;
		}

		/* Set remote swizid for gr */
		reg_val = nvgpu_readl(g,
			fb_mmu_smc_eng_cfg_0_r(gr_syspipe->gr_syspipe_id));
		reg_val &= ~fb_mmu_smc_eng_cfg_0_remote_swizid_m();

		if (enable) {
			if (gpu_instance->is_memory_partition_supported) {
				default_remote_swizid =
					g->mig.gpu_instance[index].gpu_instance_id;
			}
			reg_val |= fb_mmu_smc_eng_cfg_0_remote_swizid_f(
				default_remote_swizid);
		}

		nvgpu_writel(g,
			fb_mmu_smc_eng_cfg_0_r(gr_syspipe->gr_syspipe_id),
			reg_val);

		g->ops.runlist.get_pbdma_info(g,
				gr_dev->rl_pri_base,
				&pbdma_info);

		for (pbdma_index = 0U; pbdma_index < PBDMA_PER_RUNLIST_SIZE;
				++pbdma_index) {
			if (pbdma_info.pbdma_id[pbdma_index] !=
					NVGPU_INVALID_PBDMA_ID) {
				pbdma_id_mask |=
					BIT32(pbdma_info.pbdma_id[pbdma_index]);

				nvgpu_log(g, gpu_dbg_mig,
					"gr-[%d %d] gpu_instance_id[%u] gr_syspipe_id[%u] "
						"pbdma_id[%u] pbdma_id_mask[%x] enable[%d] ",
					index,
					pbdma_index,
					g->mig.gpu_instance[index].gpu_instance_id,
					gr_syspipe->gr_syspipe_id,
					pbdma_info.pbdma_id[pbdma_index],
					pbdma_id_mask,
					enable);
			}
		}

		nvgpu_log(g, gpu_dbg_mig,
			"gr-[%d] gpu_instance_id[%u] gr_syspipe_id[%u] "
				"gr_instance_id[%u] pbdma_id_mask[%x] reg_val[%x] "
				 "enable[%d] ",
			index,
			g->mig.gpu_instance[index].gpu_instance_id,
			gr_syspipe->gr_syspipe_id,
			gr_syspipe->gr_instance_id,
			pbdma_id_mask, reg_val, enable);

		/* Set remote swizid for lces */
		for (lce_id = 0U; lce_id < gpu_instance->num_lce;
				lce_id++) {
			const struct nvgpu_device *lce =
				gpu_instance->lce_devs[lce_id];

			reg_val = nvgpu_readl(g,
				fb_mmu_mmu_eng_id_cfg_r(lce->fault_id));
			reg_val &= ~fb_mmu_mmu_eng_id_cfg_remote_swizid_m();

			if (enable) {
				reg_val |= fb_mmu_mmu_eng_id_cfg_remote_swizid_f(
					default_remote_swizid);
			}

			g->ops.runlist.get_pbdma_info(g,
					lce->rl_pri_base,
					&pbdma_info);

			for (pbdma_index = 0U; pbdma_index < PBDMA_PER_RUNLIST_SIZE;
					++pbdma_index) {
				if (pbdma_info.pbdma_id[pbdma_index] !=
						NVGPU_INVALID_PBDMA_ID) {
					pbdma_id_mask |=
						BIT32(pbdma_info.pbdma_id[pbdma_index]);

					nvgpu_log(g, gpu_dbg_mig,
						"lce-[%d %d] gpu_instance_id[%u] gr_syspipe_id[%u] "
							"pbdma_id[%u] pbdma_id_mask[%x] enable[%d] ",
						index,
						pbdma_index,
						g->mig.gpu_instance[index].gpu_instance_id,
						gr_syspipe->gr_syspipe_id,
						pbdma_info.pbdma_id[pbdma_index],
						pbdma_id_mask, enable);
				}
			}

			nvgpu_writel(g,
				fb_mmu_mmu_eng_id_cfg_r(lce->fault_id),
				reg_val);

			nvgpu_log(g, gpu_dbg_mig,
				"lce-[%d] gpu_instance_id[%u] gr_syspipe_id[%u] "
					"gr_instance_id[%u] engine_id[%u] inst_id[%u] "
					"fault_id[%u] pbdma_id_mask[%x] reg_val[%x] "
					"enable[%d] ",
				index,
				g->mig.gpu_instance[index].gpu_instance_id,
				gr_syspipe->gr_syspipe_id,
				gr_syspipe->gr_instance_id,
				lce->engine_id, lce->inst_id,
				lce->fault_id, pbdma_id_mask,
				reg_val, enable);
		}

		/* Set remote swizid for its pbdma */
		while (pbdma_id_mask != 0U) {
			u32 fault_id;
			u32 pbdma_id = nvgpu_safe_sub_u32(
				(u32)nvgpu_ffs(pbdma_id_mask), 1UL);

			fault_id =
				g->ops.pbdma.get_mmu_fault_id(g, pbdma_id);

			reg_val = nvgpu_readl(g,
				fb_mmu_mmu_eng_id_cfg_r(fault_id));
			reg_val &= ~fb_mmu_mmu_eng_id_cfg_remote_swizid_m();

			if (enable) {
				reg_val |= fb_mmu_mmu_eng_id_cfg_remote_swizid_f(
					default_remote_swizid);
			}

			nvgpu_writel(g,
				fb_mmu_mmu_eng_id_cfg_r(fault_id),
				reg_val);

			nvgpu_log(g, gpu_dbg_mig,
				"gpu_instance_id[%u] gr_syspipe_id[%u] "
					"pbdma_id[%u] fault_id[%u] pbdma_id_mask[%x] "
					"reg_val[%x] enable[%d] ",
				g->mig.gpu_instance[index].gpu_instance_id,
				gr_syspipe->gr_syspipe_id,
				pbdma_id, fault_id, pbdma_id_mask,
				reg_val, enable);

			pbdma_id_mask ^= BIT32(pbdma_id);
		}
	}

	return 0;
}
#endif

