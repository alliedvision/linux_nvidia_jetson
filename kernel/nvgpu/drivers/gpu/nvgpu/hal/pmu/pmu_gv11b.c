/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/pmu.h>
#include <nvgpu/falcon.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/mm.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/firmware.h>
#include <nvgpu/bug.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/cmd.h>
#endif

#include "pmu_gk20a.h"
#include "pmu_gv11b.h"

#include <nvgpu/hw/gv11b/hw_pwr_gv11b.h>

#define ALIGN_4KB     12

#ifdef CONFIG_NVGPU_LS_PMU
/* PROD settings for ELPG sequencing registers*/
static struct pg_init_sequence_list _pginitseq_gv11b[] = {
	{0x0010e0a8U, 0x00000000U} ,
	{0x0010e0acU, 0x00000000U} ,
	{0x0010e198U, 0x00000200U} ,
	{0x0010e19cU, 0x00000000U} ,
	{0x0010e19cU, 0x00000000U} ,
	{0x0010e19cU, 0x00000000U} ,
	{0x0010e19cU, 0x00000000U} ,
	{0x0010aba8U, 0x00000200U} ,
	{0x0010abacU, 0x00000000U} ,
	{0x0010abacU, 0x00000000U} ,
	{0x0010abacU, 0x00000000U} ,
	{0x0010e09cU, 0x00000731U} ,
	{0x0010e18cU, 0x00000731U} ,
	{0x0010ab9cU, 0x00000731U} ,
	{0x0010e0a0U, 0x00000200U} ,
	{0x0010e0a4U, 0x00000004U} ,
	{0x0010e0a4U, 0x80000000U} ,
	{0x0010e0a4U, 0x80000009U} ,
	{0x0010e0a4U, 0x8000001AU} ,
	{0x0010e0a4U, 0x8000001EU} ,
	{0x0010e0a4U, 0x8000002AU} ,
	{0x0010e0a4U, 0x8000002EU} ,
	{0x0010e0a4U, 0x80000016U} ,
	{0x0010e0a4U, 0x80000022U} ,
	{0x0010e0a4U, 0x80000026U} ,
	{0x0010e0a4U, 0x00000005U} ,
	{0x0010e0a4U, 0x80000001U} ,
	{0x0010e0a4U, 0x8000000AU} ,
	{0x0010e0a4U, 0x8000001BU} ,
	{0x0010e0a4U, 0x8000001FU} ,
	{0x0010e0a4U, 0x8000002BU} ,
	{0x0010e0a4U, 0x8000002FU} ,
	{0x0010e0a4U, 0x80000017U} ,
	{0x0010e0a4U, 0x80000023U} ,
	{0x0010e0a4U, 0x80000027U} ,
	{0x0010e0a4U, 0x00000006U} ,
	{0x0010e0a4U, 0x80000002U} ,
	{0x0010e0a4U, 0x8000000BU} ,
	{0x0010e0a4U, 0x8000001CU} ,
	{0x0010e0a4U, 0x80000020U} ,
	{0x0010e0a4U, 0x8000002CU} ,
	{0x0010e0a4U, 0x80000030U} ,
	{0x0010e0a4U, 0x80000018U} ,
	{0x0010e0a4U, 0x80000024U} ,
	{0x0010e0a4U, 0x80000028U} ,
	{0x0010e0a4U, 0x00000007U} ,
	{0x0010e0a4U, 0x80000003U} ,
	{0x0010e0a4U, 0x8000000CU} ,
	{0x0010e0a4U, 0x8000001DU} ,
	{0x0010e0a4U, 0x80000021U} ,
	{0x0010e0a4U, 0x8000002DU} ,
	{0x0010e0a4U, 0x80000031U} ,
	{0x0010e0a4U, 0x80000019U} ,
	{0x0010e0a4U, 0x80000025U} ,
	{0x0010e0a4U, 0x80000029U} ,
	{0x0010e0a4U, 0x80000012U} ,
	{0x0010e0a4U, 0x80000010U} ,
	{0x0010e0a4U, 0x00000013U} ,
	{0x0010e0a4U, 0x80000011U} ,
	{0x0010e0a4U, 0x80000008U} ,
	{0x0010e0a4U, 0x8000000DU} ,
	{0x0010e190U, 0x00000200U} ,
	{0x0010e194U, 0x80000015U} ,
	{0x0010e194U, 0x80000014U} ,
	{0x0010aba0U, 0x00000200U} ,
	{0x0010aba4U, 0x8000000EU} ,
	{0x0010aba4U, 0x0000000FU} ,
	{0x0010ab34U, 0x00000001U} ,
	{0x00020004U, 0x00000000U} ,
};

void gv11b_pmu_init_perfmon_counter(struct gk20a *g)
{
        u32 data;

        gk20a_pmu_init_perfmon_counter(g);

        /* assign same mask setting from GR ELPG to counter #3 */
        data = gk20a_readl(g, pwr_pmu_idle_mask_2_supp_r(0));
        gk20a_writel(g, pwr_pmu_idle_mask_2_r(3), data);
}

void gv11b_pmu_setup_elpg(struct gk20a *g)
{
	size_t reg_writes;
	size_t index;

	nvgpu_log_fn(g, " ");

	if (g->can_elpg && g->elpg_enabled) {
		reg_writes = ARRAY_SIZE(_pginitseq_gv11b);
		/* Initialize registers with production values*/
		for (index = 0; index < reg_writes; index++) {
			nvgpu_writel(g, _pginitseq_gv11b[index].regaddr,
				_pginitseq_gv11b[index].writeval);
		}
	}

	nvgpu_log_fn(g, "done");
}

int gv11b_pmu_bootstrap(struct gk20a *g, struct nvgpu_pmu *pmu,
	u32 args_offset)
{
	struct mm_gk20a *mm = &g->mm;
	struct nvgpu_firmware *fw = NULL;
	struct pmu_ucode_desc *desc = NULL;
	u32 addr_code_lo, addr_data_lo, addr_load_lo;
	u32 addr_code_hi, addr_data_hi;
	u32 i, blocks;
	int err;
	u32 inst_block_ptr;

	nvgpu_log_fn(g, " ");

	fw = nvgpu_pmu_fw_desc_desc(g, pmu);
	desc = (struct pmu_ucode_desc *)(void *)fw->data;

	nvgpu_writel(g, pwr_falcon_itfen_r(),
		nvgpu_readl(g, pwr_falcon_itfen_r()) |
		pwr_falcon_itfen_ctxen_enable_f());

	inst_block_ptr = nvgpu_inst_block_ptr(g, &mm->pmu.inst_block);
	nvgpu_writel(g, pwr_pmu_new_instblk_r(),
		pwr_pmu_new_instblk_ptr_f(inst_block_ptr) |
		pwr_pmu_new_instblk_valid_f(1) |
		(nvgpu_is_enabled(g, NVGPU_USE_COHERENT_SYSMEM) ?
		pwr_pmu_new_instblk_target_sys_coh_f() :
		pwr_pmu_new_instblk_target_sys_ncoh_f()));

	nvgpu_writel(g, pwr_falcon_dmemc_r(0),
		pwr_falcon_dmemc_offs_f(0) |
		pwr_falcon_dmemc_blk_f(0)  |
		pwr_falcon_dmemc_aincw_f(1));

	addr_code_lo = u64_lo32((pmu->fw->ucode.gpu_va +
			desc->app_start_offset +
			desc->app_resident_code_offset) >> 8);

	addr_code_hi = u64_hi32((pmu->fw->ucode.gpu_va +
			desc->app_start_offset +
			desc->app_resident_code_offset) >> 8);
	addr_data_lo = u64_lo32((pmu->fw->ucode.gpu_va +
			desc->app_start_offset +
			desc->app_resident_data_offset) >> 8);
	addr_data_hi = u64_hi32((pmu->fw->ucode.gpu_va +
			desc->app_start_offset +
			desc->app_resident_data_offset) >> 8);
	addr_load_lo = u64_lo32((pmu->fw->ucode.gpu_va +
			desc->bootloader_start_offset) >> 8);

	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), GK20A_PMU_DMAIDX_UCODE);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), addr_code_lo << 8);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), addr_code_hi);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_code_offset);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_code_size);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x0U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), desc->app_imem_entry);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), addr_data_lo << 8);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), addr_data_hi);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_data_size);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), 0x1U);
	nvgpu_writel(g, pwr_falcon_dmemd_r(0), args_offset);

	g->ops.pmu.write_dmatrfbase(g,
				addr_load_lo -
				(desc->bootloader_imem_offset >> U32(8)));

	blocks = ((desc->bootloader_size + 0xFFU) & ~0xFFU) >> 8U;

	for (i = 0; i < blocks; i++) {
		nvgpu_writel(g, pwr_falcon_dmatrfmoffs_r(),
			desc->bootloader_imem_offset + (i << 8));
		nvgpu_writel(g, pwr_falcon_dmatrffboffs_r(),
			desc->bootloader_imem_offset + (i << 8));
		nvgpu_writel(g, pwr_falcon_dmatrfcmd_r(),
			pwr_falcon_dmatrfcmd_imem_f(1)  |
			pwr_falcon_dmatrfcmd_write_f(0) |
			pwr_falcon_dmatrfcmd_size_f(6)  |
			pwr_falcon_dmatrfcmd_ctxdma_f(GK20A_PMU_DMAIDX_UCODE));
	}

	err = nvgpu_falcon_bootstrap(pmu->flcn, desc->bootloader_entry_point);

	nvgpu_writel(g, pwr_falcon_os_r(), desc->app_version);

	return err;
}

u32 gv11b_pmu_queue_head_r(u32 i)
{
	return pwr_pmu_queue_head_r(i);
}

u32 gv11b_pmu_queue_head__size_1_v(void)
{
	return pwr_pmu_queue_head__size_1_v();
}

u32 gv11b_pmu_queue_tail_r(u32 i)
{
	return pwr_pmu_queue_tail_r(i);
}

u32 gv11b_pmu_queue_tail__size_1_v(void)
{
	return pwr_pmu_queue_tail__size_1_v();
}

u32 gv11b_pmu_mutex__size_1_v(void)
{
	return pwr_pmu_mutex__size_1_v();
}

void gv11b_secured_pmu_start(struct gk20a *g)
{
	nvgpu_writel(g, pwr_falcon_cpuctl_alias_r(),
		pwr_falcon_cpuctl_startcpu_f(1));
}

void gv11b_write_dmatrfbase(struct gk20a *g, u32 addr)
{
	nvgpu_writel(g, pwr_falcon_dmatrfbase_r(), addr);
	nvgpu_writel(g, pwr_falcon_dmatrfbase1_r(), 0x0U);
}
#endif

#ifdef CONFIG_NVGPU_INJECT_HWERR
void gv11b_pmu_inject_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info)
{
	(void)error_info;
	nvgpu_info(g, "Injecting PMU fault %s", err->name);
	nvgpu_writel(g, err->get_reg_addr(), err->get_reg_val(1U));
}

static inline u32 pmu_falcon_ecc_control_r(void)
{
	return pwr_pmu_falcon_ecc_control_r();
}

static inline u32 pmu_falcon_ecc_control_inject_corrected_err_f(u32 v)
{
	return pwr_pmu_falcon_ecc_control_inject_corrected_err_f(v);
}

static inline u32 pmu_falcon_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return pwr_pmu_falcon_ecc_control_inject_uncorrected_err_f(v);
}

static struct nvgpu_hw_err_inject_info pmu_ecc_err_desc[] = {
	NVGPU_ECC_ERR("falcon_imem_ecc_corrected",
			gv11b_pmu_inject_ecc_error,
			pmu_falcon_ecc_control_r,
			pmu_falcon_ecc_control_inject_corrected_err_f),
	NVGPU_ECC_ERR("falcon_imem_ecc_uncorrected",
			gv11b_pmu_inject_ecc_error,
			pmu_falcon_ecc_control_r,
			pmu_falcon_ecc_control_inject_uncorrected_err_f),
};

static struct nvgpu_hw_err_inject_info_desc pmu_err_desc;

struct nvgpu_hw_err_inject_info_desc *
gv11b_pmu_intr_get_err_desc(struct gk20a *g)
{
	(void)g;
	pmu_err_desc.info_ptr = pmu_ecc_err_desc;
	pmu_err_desc.info_size = nvgpu_safe_cast_u64_to_u32(
			sizeof(pmu_ecc_err_desc) /
			sizeof(struct nvgpu_hw_err_inject_info));

	return &pmu_err_desc;
}
#endif /* CONFIG_NVGPU_INJECT_HWERR */
