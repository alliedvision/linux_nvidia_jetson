/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/timers.h>
#include <nvgpu/pmu.h>
#include <nvgpu/mm.h>
#include <nvgpu/fuse.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmu/cmd.h>

#include "pmu_gk20a.h"
#include "pmu_gm20b.h"

#include <nvgpu/hw/gm20b/hw_pwr_gm20b.h>

/* PROD settings for ELPG sequencing registers*/
static struct pg_init_sequence_list _pginitseq_gm20b[] = {
		{ 0x0010ab10U, 0x8180U},
		{ 0x0010e118U, 0x83828180U},
		{ 0x0010e068U, 0x0U},
		{ 0x0010e06cU, 0x00000080U},
		{ 0x0010e06cU, 0x00000081U},
		{ 0x0010e06cU, 0x00000082U},
		{ 0x0010e06cU, 0x00000083U},
		{ 0x0010e06cU, 0x00000084U},
		{ 0x0010e06cU, 0x00000085U},
		{ 0x0010e06cU, 0x00000086U},
		{ 0x0010e06cU, 0x00000087U},
		{ 0x0010e06cU, 0x00000088U},
		{ 0x0010e06cU, 0x00000089U},
		{ 0x0010e06cU, 0x0000008aU},
		{ 0x0010e06cU, 0x0000008bU},
		{ 0x0010e06cU, 0x0000008cU},
		{ 0x0010e06cU, 0x0000008dU},
		{ 0x0010e06cU, 0x0000008eU},
		{ 0x0010e06cU, 0x0000008fU},
		{ 0x0010e06cU, 0x00000090U},
		{ 0x0010e06cU, 0x00000091U},
		{ 0x0010e06cU, 0x00000092U},
		{ 0x0010e06cU, 0x00000093U},
		{ 0x0010e06cU, 0x00000094U},
		{ 0x0010e06cU, 0x00000095U},
		{ 0x0010e06cU, 0x00000096U},
		{ 0x0010e06cU, 0x00000097U},
		{ 0x0010e06cU, 0x00000098U},
		{ 0x0010e06cU, 0x00000099U},
		{ 0x0010e06cU, 0x0000009aU},
		{ 0x0010e06cU, 0x0000009bU},
		{ 0x0010ab14U, 0x00000000U},
		{ 0x0010ab18U, 0x00000000U},
		{ 0x0010e024U, 0x00000000U},
		{ 0x0010e028U, 0x00000000U},
		{ 0x0010e11cU, 0x00000000U},
		{ 0x0010e120U, 0x00000000U},
		{ 0x0010ab1cU, 0x02010155U},
		{ 0x0010e020U, 0x001b1b55U},
		{ 0x0010e124U, 0x01030355U},
		{ 0x0010ab20U, 0x89abcdefU},
		{ 0x0010ab24U, 0x00000000U},
		{ 0x0010e02cU, 0x89abcdefU},
		{ 0x0010e030U, 0x00000000U},
		{ 0x0010e128U, 0x89abcdefU},
		{ 0x0010e12cU, 0x00000000U},
		{ 0x0010ab28U, 0x74444444U},
		{ 0x0010ab2cU, 0x70000000U},
		{ 0x0010e034U, 0x74444444U},
		{ 0x0010e038U, 0x70000000U},
		{ 0x0010e130U, 0x74444444U},
		{ 0x0010e134U, 0x70000000U},
		{ 0x0010ab30U, 0x00000000U},
		{ 0x0010ab34U, 0x00000001U},
		{ 0x00020004U, 0x00000000U},
		{ 0x0010e138U, 0x00000000U},
		{ 0x0010e040U, 0x00000000U},
};

void gm20b_pmu_setup_elpg(struct gk20a *g)
{
	size_t reg_writes;
	size_t index;

	nvgpu_log_fn(g, " ");

	if (g->can_elpg && g->elpg_enabled) {
		reg_writes = ARRAY_SIZE(_pginitseq_gm20b);
		/* Initialize registers with production values*/
		for (index = 0; index < reg_writes; index++) {
			gk20a_writel(g, _pginitseq_gm20b[index].regaddr,
				_pginitseq_gm20b[index].writeval);
		}
	}

	nvgpu_log_fn(g, "done");
}

void gm20b_write_dmatrfbase(struct gk20a *g, u32 addr)
{
	gk20a_writel(g, pwr_falcon_dmatrfbase_r(), addr);
}

/*Dump Security related fuses*/
void pmu_dump_security_fuses_gm20b(struct gk20a *g)
{
	u32 val = 0;

	nvgpu_err(g, "FUSE_OPT_SEC_DEBUG_EN_0: 0x%x",
			g->ops.fuse.fuse_opt_sec_debug_en(g));
	nvgpu_err(g, "FUSE_OPT_PRIV_SEC_EN_0: 0x%x",
			g->ops.fuse.fuse_opt_priv_sec_en(g));
	if (g->ops.fuse.read_gcplex_config_fuse(g, &val) != 0) {
		nvgpu_err(g, "FUSE_GCPLEX_CONFIG_FUSE_0: 0x%x", val);
	}
}

bool gm20b_pmu_is_debug_mode_en(struct gk20a *g)
{
	u32 ctl_stat =  gk20a_readl(g, pwr_pmu_scpctl_stat_r());
	return pwr_pmu_scpctl_stat_debug_mode_v(ctl_stat) != 0U;
}

void gm20b_pmu_ns_setup_apertures(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	/* setup apertures - virtual */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_UCODE),
		pwr_fbif_transcfg_mem_type_virtual_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_VIRT),
		pwr_fbif_transcfg_mem_type_virtual_f());
	/* setup apertures - physical */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_VID),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_local_fb_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_COH),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_coherent_sysmem_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_NCOH),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_noncoherent_sysmem_f());
}

void gm20b_pmu_setup_apertures(struct gk20a *g)
{
	/* setup apertures - virtual */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_UCODE),
			pwr_fbif_transcfg_mem_type_physical_f() |
			pwr_fbif_transcfg_target_local_fb_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_VIRT),
			pwr_fbif_transcfg_mem_type_virtual_f());
	/* setup apertures - physical */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_VID),
			pwr_fbif_transcfg_mem_type_physical_f() |
			pwr_fbif_transcfg_target_local_fb_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_COH),
			pwr_fbif_transcfg_mem_type_physical_f() |
			pwr_fbif_transcfg_target_coherent_sysmem_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_NCOH),
			pwr_fbif_transcfg_mem_type_physical_f() |
			pwr_fbif_transcfg_target_noncoherent_sysmem_f());
}

void gm20b_pmu_flcn_setup_boot_config(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	u32 inst_block_ptr;

	nvgpu_log_fn(g, " ");

	/* setup apertures */
	if (g->ops.pmu.setup_apertures != NULL) {
		g->ops.pmu.setup_apertures(g);
	}

	/* Clearing mailbox register used to reflect capabilities */
	gk20a_writel(g, pwr_falcon_mailbox1_r(), 0);

	/* enable the context interface */
	gk20a_writel(g, pwr_falcon_itfen_r(),
		gk20a_readl(g, pwr_falcon_itfen_r()) |
		pwr_falcon_itfen_ctxen_enable_f());

	/*
	 * The instance block address to write is the lower 32-bits of the 4K-
	 * aligned physical instance block address.
	 */
	inst_block_ptr = nvgpu_inst_block_ptr(g, &mm->pmu.inst_block);

	gk20a_writel(g, pwr_pmu_new_instblk_r(),
		pwr_pmu_new_instblk_ptr_f(inst_block_ptr) |
		pwr_pmu_new_instblk_valid_f(1U) |
		(nvgpu_is_enabled(g, NVGPU_USE_COHERENT_SYSMEM) ?
		pwr_pmu_new_instblk_target_sys_coh_f() :
		pwr_pmu_new_instblk_target_sys_ncoh_f())) ;
}

void gm20b_secured_pmu_start(struct gk20a *g)
{
	gk20a_writel(g, pwr_falcon_cpuctl_alias_r(),
		pwr_falcon_cpuctl_startcpu_f(1));
}

bool gm20b_is_pmu_supported(struct gk20a *g)
{
	(void)g;
	return true;
}

void gm20b_clear_pmu_bar0_host_err_status(struct gk20a *g)
{
	u32 status;

	status = gk20a_readl(g, pwr_pmu_bar0_host_error_r());
	gk20a_writel(g, pwr_pmu_bar0_host_error_r(), status);
}

u32 gm20b_pmu_queue_head_r(u32 i)
{
	return pwr_pmu_queue_head_r(i);
}

u32 gm20b_pmu_queue_head__size_1_v(void)
{
	return pwr_pmu_queue_head__size_1_v();
}

u32 gm20b_pmu_queue_tail_r(u32 i)
{
	return pwr_pmu_queue_tail_r(i);
}

u32 gm20b_pmu_queue_tail__size_1_v(void)
{
	return pwr_pmu_queue_tail__size_1_v();
}

u32 gm20b_pmu_mutex__size_1_v(void)
{
	return pwr_pmu_mutex__size_1_v();
}
