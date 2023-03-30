/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/timers.h>
#include <nvgpu/io.h>
#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/bug.h>
#include <nvgpu/utils.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/pmu.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/falcon.h>

#include <nvgpu/pmu/mutex.h>
#include <nvgpu/pmu/seq.h>
#include <nvgpu/pmu/lsfm.h>
#include <nvgpu/pmu/super_surface.h>
#include <nvgpu/pmu/pmu_perfmon.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/pmu/debug.h>
#include <nvgpu/pmu/pmu_pstate.h>
#include <nvgpu/riscv.h>

#include "boardobj/boardobj.h"

#ifdef CONFIG_NVGPU_POWER_PG
#include <nvgpu/pmu/pmu_pg.h>
#endif

#ifdef CONFIG_NVGPU_DGPU
#include <nvgpu/sec2/lsfm.h>
#endif

#if defined(CONFIG_NVGPU_NON_FUSA)
#define PMU_PRIV_LOCKDOWN_RELEASE_POLLING_US (1U)
#endif

/* PMU locks used to sync with PMU-RTOS */
int nvgpu_pmu_lock_acquire(struct gk20a *g, struct nvgpu_pmu *pmu,
			u32 id, u32 *token)
{
	if (!g->support_ls_pmu) {
		return 0;
	}

	if (!g->can_elpg) {
		return 0;
	}

#ifdef CONFIG_NVGPU_POWER_PG
	if (!pmu->pg->initialized) {
		return -EINVAL;
	}
#endif

	return nvgpu_pmu_mutex_acquire(g, pmu->mutexes, id, token);
}

int nvgpu_pmu_lock_release(struct gk20a *g, struct nvgpu_pmu *pmu,
			u32 id, u32 *token)
{
	if (!g->support_ls_pmu) {
		return 0;
	}

	if (!g->can_elpg) {
		return 0;
	}

#ifdef CONFIG_NVGPU_POWER_PG
	if (!pmu->pg->initialized) {
		return -EINVAL;
	}
#endif

	return nvgpu_pmu_mutex_release(g, pmu->mutexes, id, token);
}

/* PMU RTOS init/setup functions */
int nvgpu_pmu_destroy(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_POWER_PG
	if (g->can_elpg) {
		nvgpu_pmu_pg_destroy(g, pmu, pmu->pg);
	}
#endif

	nvgpu_pmu_queues_free(g, &pmu->queues);

	/*
	 * This is to clear the content of FBQ command and message queue data
	 * as part rail-gate sequence to make sure FBQ is clean for un-railgate
	 * sequence.
	 */
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PMU_RTOS_FBQ)) {
		nvgpu_pmu_ss_fbq_flush(g, pmu);
	}

	nvgpu_pmu_fw_state_change(g, pmu, PMU_FW_STATE_OFF, false);
	nvgpu_pmu_set_fw_ready(g, pmu, false);
	nvgpu_pmu_lsfm_clean(g, pmu, pmu->lsfm);
	pmu->pmu_perfmon->perfmon_ready = false;
#ifdef CONFIG_NVGPU_FALCON_DEBUG
	nvgpu_falcon_dbg_error_print_enable(pmu->flcn, false);
#endif
	nvgpu_log_fn(g, "done");
	return 0;
}

static void remove_pmu_support(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = pmu->g;
	struct pmu_board_obj *obj, *obj_tmp;
	struct boardobjgrp *pboardobjgrp, *pboardobjgrp_tmp;
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (nvgpu_alloc_initialized(&pmu->dmem)) {
		nvgpu_alloc_destroy(&pmu->dmem);
	}

	if (nvgpu_is_enabled(g, NVGPU_PMU_PSTATE)) {
		nvgpu_list_for_each_entry_safe(pboardobjgrp,
			pboardobjgrp_tmp, &g->boardobjgrp_head,
			boardobjgrp, node) {
				err = pboardobjgrp->destruct(pboardobjgrp);
				if (err != 0) {
					nvgpu_err(g, "pboardobjgrp destruct failed");
				}
		}

		nvgpu_list_for_each_entry_safe(obj, obj_tmp,
			&g->boardobj_head, boardobj, node) {
				obj->destruct(obj);
		}
	}

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PMU_SUPER_SURFACE)) {
		nvgpu_pmu_super_surface_deinit(g, pmu, pmu->super_surface);
	}

	if (nvgpu_is_enabled(g, NVGPU_PMU_PSTATE)) {
		nvgpu_pmu_pstate_deinit(g);
	}

#ifdef CONFIG_NVGPU_FALCON_DEBUG
	if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
		nvgpu_falcon_dbg_buf_destroy(pmu->flcn);
	}
#endif

	nvgpu_pmu_debug_deinit(g, pmu);
	nvgpu_pmu_lsfm_deinit(g, pmu, pmu->lsfm);
#ifdef CONFIG_NVGPU_POWER_PG
	nvgpu_pmu_pg_deinit(g, pmu, pmu->pg);
#endif
	nvgpu_pmu_sequences_deinit(g, pmu, pmu->sequences);
	nvgpu_pmu_mutexe_deinit(g, pmu, pmu->mutexes);
	nvgpu_pmu_fw_deinit(g, pmu, pmu->fw);
	nvgpu_pmu_deinitialize_perfmon(g, pmu);
}

static int pmu_sw_setup(struct gk20a *g, struct nvgpu_pmu *pmu )
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	/* set default value to mutexes */
	nvgpu_pmu_mutex_sw_setup(g, pmu, pmu->mutexes);

	/* set default value to sequences */
	nvgpu_pmu_sequences_sw_setup(g, pmu, pmu->sequences);

#ifdef CONFIG_NVGPU_POWER_PG
	if (g->can_elpg) {
		err = nvgpu_pmu_pg_sw_setup(g, pmu, pmu->pg);
		if (err != 0){
			goto exit;
		}
	}
#endif

	if (pmu->sw_ready) {
		nvgpu_log_fn(g, "skip PMU-RTOS shared buffer realloc");
		goto exit;
	}

	/* alloc shared buffer to read PMU-RTOS debug message */
	err = nvgpu_pmu_debug_init(g, pmu);
	if (err != 0) {
		goto exit;
	}

	/* alloc shared buffer super buffer to communicate with PMU-RTOS */
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PMU_SUPER_SURFACE)) {
		err = nvgpu_pmu_super_surface_buf_alloc(g,
				pmu, pmu->super_surface);
		if (err != 0) {
			goto exit;
		}
	}

	pmu->sw_ready = true;
exit:
	if (err != 0) {
		nvgpu_pmu_remove_support(g, pmu);
	}

	return err;
}

void nvgpu_pmu_rtos_cmdline_args_init(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	nvgpu_log_fn(g, " ");

	pmu->fw->ops.set_cmd_line_args_trace_size(
		pmu, PMU_RTOS_TRACE_BUFSIZE);
	pmu->fw->ops.set_cmd_line_args_trace_dma_base(pmu);
	pmu->fw->ops.set_cmd_line_args_trace_dma_idx(
		pmu, GK20A_PMU_DMAIDX_VIRT);

	pmu->fw->ops.set_cmd_line_args_cpu_freq(pmu,
		(u32)g->ops.clk.get_rate(g, CTRL_CLK_DOMAIN_PWRCLK));

	if (pmu->fw->ops.config_cmd_line_args_super_surface != NULL) {
		pmu->fw->ops.config_cmd_line_args_super_surface(pmu);
	}
}

#if defined(CONFIG_NVGPU_NON_FUSA)
void nvgpu_pmu_next_core_rtos_args_setup(struct gk20a *g,
		struct nvgpu_pmu *pmu)
{
	struct nv_pmu_boot_params boot_params;
	struct nv_next_core_bootldr_params *btldr_params;
	struct nv_next_core_rtos_params *rtos_params;
	struct pmu_cmdline_args_v7 *cmd_line_args;
	u64 phyadr = 0;

	nvgpu_pmu_rtos_cmdline_args_init(g, pmu);

	btldr_params = &boot_params.boot_params.bl;
	rtos_params = &boot_params.boot_params.rtos;
	cmd_line_args = &boot_params.cmd_line_args;

	/* setup core dump */
	rtos_params->core_dump_size = NV_REG_STR_NEXT_CORE_DUMP_SIZE_DEFAULT;
	rtos_params->core_dump_phys = nvgpu_mem_get_addr(g,
			&pmu->fw->ucode_core_dump);

	/* copy cmd line args to pmu->boot_params.cmd_line_args */
	nvgpu_memcpy((u8 *)cmd_line_args,
			(u8 *) (pmu->fw->ops.get_cmd_line_args_ptr(pmu)),
			pmu->fw->ops.get_cmd_line_args_size(pmu));

	cmd_line_args->ctx_bind_addr = g->ops.pmu.get_inst_block_config(g);

	/* setup boot loader args */
	btldr_params->boot_type = NV_NEXT_CORE_BOOTLDR_BOOT_TYPE_RM;
	btldr_params->size = U16(sizeof(struct nv_pmu_boot_params));
	btldr_params->version = NV_NEXT_CORE_BOOTLDR_VERSION;

	/* copy to boot_args phyadr */
	nvgpu_mem_wr_n(g, &pmu->fw->ucode_boot_args, 0,
			&boot_params.boot_params.bl,
			sizeof(struct nv_pmu_boot_params));

	/* copy boot args phyadr to mailbox 0/1 */
	phyadr = nvgpu_safe_add_u64(NV_NEXT_CORE_AMAP_EXTMEM2_START,
			nvgpu_mem_get_addr(g, &pmu->fw->ucode_boot_args));

	nvgpu_falcon_mailbox_write(g->pmu->flcn, FALCON_MAILBOX_0,
			u64_lo32(phyadr));
	nvgpu_falcon_mailbox_write(g->pmu->flcn, FALCON_MAILBOX_1,
			u64_hi32(phyadr));
}

s32 nvgpu_pmu_next_core_rtos_args_allocate(struct gk20a *g,
		struct nvgpu_pmu *pmu)
{
	struct pmu_rtos_fw *rtos_fw = pmu->fw;
	s32 err =0;

	nvgpu_log_fn(g, " ");

	/* alloc boot args */
	if (!nvgpu_mem_is_valid(&rtos_fw->ucode_boot_args)) {
		err = nvgpu_dma_alloc_flags_sys(g,
				NVGPU_DMA_PHYSICALLY_ADDRESSED,
				sizeof(struct nv_pmu_boot_params),
				&rtos_fw->ucode_boot_args);
		if (err != 0) {
			goto exit;
		}
	}

	/* alloc core dump */
	if (!nvgpu_mem_is_valid(&rtos_fw->ucode_core_dump)) {
		err = nvgpu_dma_alloc_flags_sys(g,
				NVGPU_DMA_PHYSICALLY_ADDRESSED,
				NV_REG_STR_NEXT_CORE_DUMP_SIZE_DEFAULT,
				&rtos_fw->ucode_core_dump);
		if (err != 0) {
			goto exit;
		}
	}

exit:
	return err;
}

static int nvgpu_pmu_wait_for_priv_lockdown_release(struct gk20a *g,
	struct nvgpu_falcon *flcn, unsigned int timeout)
{
	struct nvgpu_timeout to;
	int status = 0;

	nvgpu_log_fn(g, " ");

	nvgpu_timeout_init_cpu_timer(g, &to, timeout);

	/* poll for priv lockdown release */
	do {
		if (!g->ops.falcon.is_priv_lockdown(flcn)) {
			break;
		}

		nvgpu_udelay(PMU_PRIV_LOCKDOWN_RELEASE_POLLING_US);
	} while (nvgpu_timeout_expired(&to) == 0);

	if (nvgpu_timeout_peek_expired(&to)) {
		status = -ETIMEDOUT;
	}

	return status;
}
#endif

int nvgpu_pmu_rtos_init(struct gk20a *g)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (!g->support_ls_pmu || (g->pmu == NULL)) {
		goto exit;
	}

	err = pmu_sw_setup(g, g->pmu);
	if (err != 0) {
		goto exit;
	}

	if (nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
#ifdef CONFIG_NVGPU_DGPU
		if (nvgpu_is_enabled(g, NVGPU_SUPPORT_SEC2_RTOS)) {
			/* Reset PMU engine */
			err = nvgpu_falcon_reset(g->pmu->flcn);

			/* Bootstrap PMU from SEC2 RTOS*/
			err = nvgpu_sec2_bootstrap_ls_falcons(g, &g->sec2,
				FALCON_ID_PMU);
			if (err != 0) {
				goto exit;
			}
		}
#endif

		if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
			/* Load register configuration for SLCG and BLCG for PMU */
			nvgpu_cg_slcg_pmu_load_enable(g);
			nvgpu_cg_blcg_pmu_load_enable(g);
		}

		if (!nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
			/*
			 * clear halt interrupt to avoid PMU-RTOS ucode
			 * hitting breakpoint due to PMU halt
			 */
			err = nvgpu_falcon_clear_halt_intr_status(g->pmu->flcn,
					nvgpu_get_poll_timeout(g));
			if (err != 0) {
				goto exit;
			}
		}

		if (g->ops.pmu.setup_apertures != NULL) {
			g->ops.pmu.setup_apertures(g);
		}

#if defined(CONFIG_NVGPU_NON_FUSA)
		if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
			err = nvgpu_pmu_next_core_rtos_args_allocate(g, g->pmu);
			if (err != 0) {
				goto exit;
			}

			nvgpu_pmu_next_core_rtos_args_setup(g, g->pmu);
		} else
#endif
		{
			err = nvgpu_pmu_lsfm_ls_pmu_cmdline_args_copy(g, g->pmu,
				g->pmu->lsfm);
			if (err != 0) {
				goto exit;
			}
		}

		nvgpu_pmu_enable_irq(g, true);

#if defined(CONFIG_NVGPU_NON_FUSA)
		if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
#ifdef CONFIG_NVGPU_FALCON_DEBUG
			err = nvgpu_falcon_dbg_buf_init(g->pmu->flcn,
					NV_RISCV_DMESG_BUFFER_SIZE,
					g->ops.pmu.pmu_get_queue_head(NV_RISCV_DEBUG_BUFFER_QUEUE),
					g->ops.pmu.pmu_get_queue_tail(NV_RISCV_DEBUG_BUFFER_QUEUE));
			if (err != 0) {
				nvgpu_err(g,
					"Failed to allocate RISCV PMU debug buffer status=0x%x)",
					err);
				goto exit;
			}
#endif
			g->ops.falcon.bootstrap(g->pmu->flcn, 0U);
		} else
#endif
		{
			/*Once in LS mode, cpuctl_alias is only accessible*/
			if (g->ops.pmu.secured_pmu_start != NULL) {
				g->ops.pmu.secured_pmu_start(g);
			}
		}
	} else {
		/* non-secure boot */
		err = nvgpu_pmu_ns_fw_bootstrap(g, g->pmu);
		if (err != 0) {
			goto exit;
		}
	}

	nvgpu_pmu_fw_state_change(g, g->pmu, PMU_FW_STATE_STARTING, false);
#if defined(CONFIG_NVGPU_NON_FUSA)
	if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {

		err = nvgpu_falcon_wait_for_nvriscv_brom_completion(g->pmu->flcn);
		if (err != 0) {
			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PMU,
					GPU_PMU_NVRISCV_BROM_FAILURE);
			nvgpu_err(g, "PMU NVRISCV BROM FAILURE");
			goto exit;
		}

		err = nvgpu_pmu_wait_for_priv_lockdown_release(g,
				g->pmu->flcn, nvgpu_get_poll_timeout(g));
		if(err != 0) {
			nvgpu_err(g, "PRIV lockdown polling failed");
			return err;
		}
	}
#endif
exit:
	return err;
}

int nvgpu_pmu_rtos_early_init(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	/* Allocate memory for pmu_perfmon */
	err = nvgpu_pmu_initialize_perfmon(g, pmu, &pmu->pmu_perfmon);
	if (err != 0) {
		goto exit;
	}

	err = nvgpu_pmu_init_pmu_fw(g, pmu, &pmu->fw);
	if (err != 0) {
		goto init_failed;
	}

	err = nvgpu_pmu_init_mutexe(g, pmu, &pmu->mutexes);
	if (err != 0) {
		goto init_failed;
	}

	err = nvgpu_pmu_sequences_init(g, pmu, &pmu->sequences);
	if (err != 0) {
		goto init_failed;
	}

#ifdef CONFIG_NVGPU_POWER_PG
	if (g->can_elpg) {
		err = nvgpu_pmu_pg_init(g, pmu, &pmu->pg);
		if (err != 0) {
			goto init_failed;
		}
	}
#endif

	err = nvgpu_pmu_lsfm_init(g, &pmu->lsfm);
	if (err != 0) {
		goto init_failed;
	}

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PMU_SUPER_SURFACE)) {
		err = nvgpu_pmu_super_surface_init(g, pmu,
				&pmu->super_surface);
		if (err != 0) {
			goto init_failed;
		}
	}

	pmu->remove_support = remove_pmu_support;
	goto exit;

init_failed:
	remove_pmu_support(pmu);

exit:
	return err;
}
