/*
 * Copyright (c) 2022-2023, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/tegra_l1ss_kernel_interface.h>
#include <linux/tegra_l1ss_ioctl.h>
#include <linux/tegra_nv_guard_service_id.h>
#include <linux/tegra_nv_guard_group_id.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/lock.h>
#include <nvgpu/timers.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/log.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/l1ss_err_reporting.h>

#include "os/linux/os_linux.h"

#define NVGPU_ERR_INVALID   U32_MAX

struct nvgpu_l1ss_ecc_reporting {
	struct gk20a *g;
	client_param_t priv;
	bool service_enabled;
	/* protects service enabled */
	struct nvgpu_spinlock lock;
};

struct nvgpu_l1ss_error_id_mappings {
	u32 num_errs;
	u32 *error_id_mappings;
};

static struct nvgpu_l1ss_error_id_mappings mappings[] = {
	{
/* *************** SERVICE ID for IGPU_HOST*************** */
		.num_errs = 16,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PFIFO_BIND_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PFIFO_SCHED_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PFIFO_CHSW_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PFIFO_MEMOP_TIMEOUT_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PFIFO_LB_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PBUS_SQUASH_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PBUS_FECS_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PBUS_TIMEOUT_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PBDMA_TIMEOUT_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PBDMA_EXTRA_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PBDMA_GPFIFO_PB_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PBDMA_METHOD_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PBDMA_SIGNATURE_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PBDMA_HCE_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PFIFO_CTXSW_TIMEOUT_ERROR,
			NVGUARD_SERVICE_IGPU_HOST_SWERR_PFIFO_FB_FLUSH_TIMEOUT_ERROR,
			NVGPU_ERR_INVALID,
		},
	},
	{
/* *************** SERVICE ID for IGPU_SM*************** */
		.num_errs = 11,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_SM_SWERR_L1_TAG_ECC_CORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_L1_TAG_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_CBU_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_LRF_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_L1_DATA_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_ICACHE_L0_DATA_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_ICACHE_L1_DATA_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_ICACHE_L0_PREDECODE_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_L1_TAG_MISS_FIFO_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_L1_TAG_S2R_PIXPRF_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_SM_SWERR_MACHINE_CHECK_ERROR,
			NVGPU_ERR_INVALID,
		},
	},
	{
/* *************** SERVICE ID for IGPU_FECS*************** */
		.num_errs = 7,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_FECS_SWERR_FALCON_IMEM_ECC_CORRECTED,
			NVGUARD_SERVICE_IGPU_FECS_SWERR_FALCON_IMEM_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_FECS_SWERR_FALCON_DMEM_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_FECS_SWERR_CTXSW_WATCHDOG_TIMEOUT,
			NVGUARD_SERVICE_IGPU_FECS_SWERR_CTXSW_CRC_MISMATCH,
			NVGUARD_SERVICE_IGPU_FECS_SWERR_FAULT_DURING_CTXSW,
			NVGUARD_SERVICE_IGPU_FECS_SWERR_CTXSW_INIT_ERROR,
		},
	},
	{
/* *************** SERVICE ID for IGPU_GPCCS*************** */
		.num_errs = 3,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_GPCCS_SWERR_FALCON_IMEM_ECC_CORRECTED,
			NVGUARD_SERVICE_IGPU_GPCCS_SWERR_FALCON_IMEM_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_GPCCS_SWERR_FALCON_DMEM_ECC_UNCORRECTED,
		},
	},
	{
/* *************** SERVICE ID for IGPU_MMU*************** */
		.num_errs = 2,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_MMU_SWERR_L1TLB_SA_DATA_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_MMU_SWERR_L1TLB_FA_DATA_ECC_UNCORRECTED,
		},
	},
	{
/* *************** SERVICE ID for IGPU_GCC*************** */
		.num_errs = 1,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_GCC_SWERR_L15_ECC_UNCORRECTED
		},
	},
	{
/* *************** SERVICE ID for IGPU_PMU*************** */
		.num_errs = 10,
		.error_id_mappings = (u32 []) {
			NVGPU_ERR_INVALID,
			NVGPU_ERR_INVALID,
			NVGPU_ERR_INVALID,
			NVGPU_ERR_INVALID,
			NVGUARD_SERVICE_IGPU_PMU_SWERR_FALCON_IMEM_ECC_UNCORRECTED,
			NVGPU_ERR_INVALID,
			NVGUARD_SERVICE_IGPU_PMU_SWERR_FALCON_DMEM_ECC_UNCORRECTED,
			NVGPU_ERR_INVALID,
			NVGPU_ERR_INVALID,
			NVGUARD_SERVICE_IGPU_PMU_SWERR_BAR0_ERROR_TIMEOUT,
		},
	},
	{
/* *************** SERVICE ID for IGPU_PGRAPH*************** */
		.num_errs = 21,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_FE_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_MEMFMT_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_PD_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_SCC_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_DS_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_SSYNC_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_MME_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_SKED_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_BE_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_BE_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_MPC_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_ILLEGAL_ERROR,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_ILLEGAL_ERROR,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_ILLEGAL_ERROR,
			NVGPU_ERR_INVALID,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_GPC_GFX_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_GPC_GFX_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_GPC_GFX_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_GPC_GFX_EXCEPTION,
			NVGUARD_SERVICE_IGPU_PGRAPH_SWERR_GPC_GFX_EXCEPTION,
			NVGPU_ERR_INVALID,
		},
	},
	{
/* *************** SERVICE ID for IGPU_LTC*************** */
		.num_errs = 4,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_LTC_SWERR_CACHE_DSTG_ECC_CORRECTED,
			NVGUARD_SERVICE_IGPU_LTC_SWERR_CACHE_DSTG_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_LTC_SWERR_CACHE_TSTG_ECC_UNCORRECTED,
			NVGPU_ERR_INVALID,
		},
	},
	{
/* *************** SERVICE ID for IGPU_HUBMMU*************** */
		.num_errs = 9,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_HUBMMU_SWERR_L2TLB_SA_DATA_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_HUBMMU_SWERR_TLB_SA_DATA_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_HUBMMU_SWERR_PTE_DATA_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_HUBMMU_SWERR_PDE0_DATA_ECC_UNCORRECTED,
			NVGUARD_SERVICE_IGPU_HUBMMU_SWERR_PAGE_FAULT_ERROR,
			NVGUARD_SERVICE_IGPU_HUBMMU_SWERR_PAGE_FAULT_ERROR,
			NVGUARD_SERVICE_IGPU_HUBMMU_SWERR_PAGE_FAULT_ERROR,
			NVGUARD_SERVICE_IGPU_HUBMMU_SWERR_PAGE_FAULT_ERROR,
			NVGUARD_SERVICE_IGPU_HUBMMU_SWERR_PAGE_FAULT_ERROR,
		},
	},
	{
/* *************** SERVICE ID for IGPU_PRI*************** */
		.num_errs = 2,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_PRI_SWERR_TIMEOUT_ERROR,
			NVGUARD_SERVICE_IGPU_PRI_SWERR_ACCESS_VIOLATION,
		},
	},
	{
/* *************** SERVICE ID for IGPU_CE*************** */
		.num_errs = 5,
		.error_id_mappings = (u32 []) {
			NVGUARD_SERVICE_IGPU_CE_SWERR_LAUNCH_ERROR,
			NVGUARD_SERVICE_IGPU_CE_SWERR_METHOD_BUFFER_FAULT,
			NVGPU_ERR_INVALID,
			NVGPU_ERR_INVALID,
			NVGUARD_SERVICE_IGPU_CE_SWERR_INVALID_CONFIG,
		},
	},
};

static int nvgpu_l1ss_report_error_linux(struct gk20a *g, u32 hw_unit_id, u32 err_id,
			bool is_critical)
{
	int err = 0;
	u32 nv_service_id = 0;
	u8 err_status = 0;
	u64 timestamp = (u64)nvgpu_current_time_ns();
	nv_guard_request_t req;

	if (hw_unit_id >= sizeof(mappings)) {
		nvgpu_err(g, "Error Id H/W index out of bounds\n");
		return -EINVAL;
	} else if (err_id >= mappings[hw_unit_id].num_errs) {
		nvgpu_err(g, "Error Id index out of bounds\n");
		return -EINVAL;
	}

	memset(&req, 0, sizeof(req));

	nv_service_id = mappings[hw_unit_id].error_id_mappings[err_id];

	if (nv_service_id == NVGPU_ERR_INVALID) {
		/* error id not supported */
		return -EOPNOTSUPP;
	}

	if (is_critical)
		err_status = NVGUARD_ERROR_DETECTED;
	else
		err_status = NVGUARD_NO_ERROR;

	req.srv_id_cmd = NVGUARD_SERVICESTATUS_NOTIFICATION;
	req.srv_status.srv_id = (nv_guard_service_id_t)nv_service_id;
	req.srv_status.status = err_status;
	req.srv_status.timestamp = timestamp;

	/*
	 * l1ss_submit_rq may fail due to kmalloc failures but may pass in
	 * subsequent calls
	 */
	err = l1ss_submit_rq(&req, true);
	if (err != 0)
		nvgpu_err(g, "Error returned from L1SS submit %d", err);

	if (is_critical)
		nvgpu_sw_quiesce(g);

	return err;
}

static int nvgpu_l1ss_report_error_empty(struct gk20a *g,
		u32 hw_unit_id, u32 err_id, bool is_critical)
{
	nvgpu_log(g, gpu_dbg_info, "ECC reporting is empty");
	return -EOPNOTSUPP;
}

static int nvgpu_l1ss_callback(l1ss_cli_callback_param param, void *data)
{
	struct gk20a *g = (struct gk20a *)data;
	struct nvgpu_os_linux *l = NULL;
	struct nvgpu_l1ss_ecc_reporting *l1ss_linux_ecc_reporting = NULL;
	int err = 0;
	/* Ensure we have a valid gk20a struct before proceeding */
	if ((g == NULL) || (nvgpu_get(g) == NULL))
		return -ENODEV;

	l = nvgpu_os_linux_from_gk20a(g);
	l1ss_linux_ecc_reporting = l->l1ss_linux_ecc_reporting;

	nvgpu_spinlock_acquire(&l1ss_linux_ecc_reporting->lock);
	if (param == L1SS_READY) {
		if (!l1ss_linux_ecc_reporting->service_enabled) {
			l1ss_linux_ecc_reporting->service_enabled = true;
			nvgpu_log(g, gpu_dbg_info, "ECC reporting is enabled");
		}
	} else if (param == L1SS_NOT_READY) {
		if (l1ss_linux_ecc_reporting->service_enabled) {
			l1ss_linux_ecc_reporting->service_enabled = false;
			nvgpu_log(g, gpu_dbg_info, "ECC reporting is disabled");
		}
	} else {
		err = -EINVAL;
	}
	nvgpu_spinlock_release(&l1ss_linux_ecc_reporting->lock);

	nvgpu_put(g);

	return err;
}

void nvgpu_l1ss_init_reporting(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_l1ss_ecc_reporting *ecc_report_linux = NULL;
	int err = 0;

	l->l1ss_linux_ecc_reporting = nvgpu_kzalloc(g, sizeof(*l->l1ss_linux_ecc_reporting));
	if (l->l1ss_linux_ecc_reporting == NULL) {
		nvgpu_err(g, "unable to allocate memory for l1ss safety services");
		return;
	}

	ecc_report_linux = l->l1ss_linux_ecc_reporting;

	/* This will invoke the registration API */
	nvgpu_spinlock_init(&ecc_report_linux->lock);
	ecc_report_linux->priv.id = (NVGUARD_GROUPID_IGPU & NVGUARD_GROUPINDEX_FIELDMASK);
	ecc_report_linux->priv.cli_callback = nvgpu_l1ss_callback;
	ecc_report_linux->priv.data = g;

	nvgpu_log(g, gpu_dbg_info, "ECC reporting Init (L1SS)");

	/*
	 * err == 0 indicates service is available but not active yet.
	 * err == 1 indicates service is available and active
	 * error for other cases.
	 */
	err = l1ss_register_client(&ecc_report_linux->priv);
	if (err == 0) {
		nvgpu_spinlock_acquire(&ecc_report_linux->lock);
		ecc_report_linux->service_enabled = false;
		nvgpu_spinlock_release(&ecc_report_linux->lock);
		nvgpu_log(g, gpu_dbg_info, "ECC reporting init success");
	} else if (err == 1) {
		nvgpu_spinlock_acquire(&ecc_report_linux->lock);
		ecc_report_linux->service_enabled = true;
		nvgpu_spinlock_release(&ecc_report_linux->lock);
		nvgpu_log(g, gpu_dbg_info, "ECC reporting init started");
	} else {
		nvgpu_log(g, gpu_dbg_info, "ECC reporting init failure %d", err);
	}
}

void nvgpu_l1ss_deinit_reporting(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_l1ss_ecc_reporting *ecc_report_linux = l->l1ss_linux_ecc_reporting;

	if (ecc_report_linux == NULL)
		return;

	nvgpu_spinlock_acquire(&ecc_report_linux->lock);
	if (ecc_report_linux->service_enabled) {
		ecc_report_linux->service_enabled = false;
	}
	nvgpu_spinlock_release(&ecc_report_linux->lock);

	(void)l1ss_deregister_client(ecc_report_linux->priv.id);
	nvgpu_log(g, gpu_dbg_info, "ECC reporting de-init success");

	nvgpu_kfree(g, ecc_report_linux);
	l->l1ss_linux_ecc_reporting = NULL;
}

int nvgpu_l1ss_report_err(struct gk20a *g, u32 err_id)
{
	int ret = 0;
	bool service_enabled;

	/* - HW_unit_id (4-bits: bit-0 to 3),
	 * - Error_id (5-bits: bit-4 to 8),
	 * - Corrected/Uncorrected error (1-bit: bit-9),
	 * - Remaining 22-bits are unused.
	 */

	u32 hw_unit = (err_id & HW_UNIT_ID_MASK);
	u32 error_id = ((err_id >> ERR_ID_FIELD_SHIFT) & ERR_ID_MASK);
	bool is_critical = ((err_id & (1 << CORRECTED_BIT_FIELD_SHIFT)) != 0U) ? true : false;

	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_l1ss_ecc_reporting *ecc_report_linux = l->l1ss_linux_ecc_reporting;

	nvgpu_log(g, gpu_dbg_info, "hw_unit = %u, error_id = %u, is_critical = %d",
		 hw_unit, error_id, is_critical);

	nvgpu_spinlock_acquire(&ecc_report_linux->lock);
	service_enabled = ecc_report_linux->service_enabled;
	nvgpu_spinlock_release(&ecc_report_linux->lock);

	if (service_enabled) {
		ret = nvgpu_l1ss_report_error_linux(g, hw_unit, error_id, is_critical);
	} else {
		ret = nvgpu_l1ss_report_error_empty(g, hw_unit, err_id, is_critical);
	}

	return ret;
}
