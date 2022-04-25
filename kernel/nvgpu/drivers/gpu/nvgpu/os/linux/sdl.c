/*
 * Copyright (c) 2021, NVIDIA Corporation. All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/timers.h>
#include <nvgpu/bug.h>

#include "ecc_linux.h"
#include "os_linux.h"
#include "module.h"

/* This look-up table initializes the list of hw units and their errors.
 * It also specifies the error injection mechanism supported, for each error.
 * In case of hw error injection support, this initialization will be overriden
 * by the values provided from the hal layes of corresponding hw units.
 */
static struct nvgpu_err_hw_module gv11b_err_lut[] = {
	{
		.name = "sm",
		.hw_unit = (u32)NVGPU_ERR_MODULE_SM,
		.num_errs = 21U,
		.base_ecc_service_id =
			NVGUARD_SERVICE_IGPU_SM_SWERR_L1_TAG_ECC_CORRECTED,
		.errs = (struct nvgpu_err_desc[]) {
			GPU_NONCRITERR("l1_tag_ecc_corrected",
					GPU_SM_L1_TAG_ECC_CORRECTED, 0, 0),
			GPU_CRITERR("l1_tag_ecc_uncorrected",
					GPU_SM_L1_TAG_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("cbu_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("cbu_ecc_uncorrected",
					GPU_SM_CBU_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("lrf_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("lrf_ecc_uncorrected",
					GPU_SM_LRF_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("l1_data_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("l1_data_ecc_uncorrected",
					GPU_SM_L1_DATA_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("icache_l0_data_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("icache_l0_data_ecc_uncorrected",
					GPU_SM_ICACHE_L0_DATA_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("icache_l1_data_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("icache_l1_data_ecc_uncorrected",
					GPU_SM_ICACHE_L1_DATA_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("icache_l0_predecode_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("icache_l0_predecode_ecc_uncorrected",
					GPU_SM_ICACHE_L0_PREDECODE_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("l1_tag_miss_fifo_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("l1_tag_miss_fifo_ecc_uncorrected",
					GPU_SM_L1_TAG_MISS_FIFO_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("l1_tag_s2r_pixprf_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("l1_tag_s2r_pixprf_ecc_uncorrected",
					GPU_SM_L1_TAG_S2R_PIXPRF_ECC_UNCORRECTED, 0, 0),
			GPU_CRITERR("machine_check_error", 0, 0, 0),
			GPU_NONCRITERR("icache_l1_predecode_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("icache_l1_predecode_ecc_uncorrected",
					GPU_SM_ICACHE_L1_PREDECODE_ECC_UNCORRECTED, 0, 0),
		},
	},
	{
		.name = "fecs",
		.hw_unit = (u32)NVGPU_ERR_MODULE_FECS,
		.num_errs = 4U,
		.base_ecc_service_id =
			NVGUARD_SERVICE_IGPU_FECS_SWERR_FALCON_IMEM_ECC_CORRECTED,
		.errs = (struct nvgpu_err_desc[]) {
			GPU_NONCRITERR("falcon_imem_ecc_corrected",
					GPU_FECS_FALCON_IMEM_ECC_CORRECTED, 0, 0),
			GPU_CRITERR("falcon_imem_ecc_uncorrected",
					GPU_FECS_FALCON_IMEM_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("falcon_dmem_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("falcon_dmem_ecc_uncorrected",
					GPU_FECS_FALCON_DMEM_ECC_UNCORRECTED, 0, 0),
		},
	},
	{
		.name = "pmu",
		.hw_unit = NVGPU_ERR_MODULE_PMU,
		.num_errs = 4U,
		.base_ecc_service_id =
			NVGUARD_SERVICE_IGPU_PMU_SWERR_FALCON_IMEM_ECC_CORRECTED,
		.errs = (struct nvgpu_err_desc[]) {
			GPU_NONCRITERR("falcon_imem_ecc_corrected",
					GPU_PMU_FALCON_IMEM_ECC_CORRECTED, 0, 0),
			GPU_CRITERR("falcon_imem_ecc_uncorrected",
					GPU_PMU_FALCON_IMEM_ECC_UNCORRECTED, 0, 0),
			GPU_NONCRITERR("falcon_dmem_ecc_corrected", 0, 0, 0),
			GPU_CRITERR("falcon_dmem_ecc_uncorrected",
					GPU_PMU_FALCON_DMEM_ECC_UNCORRECTED, 0, 0),
		},
	},
};

static void nvgpu_init_err_msg_header(struct gpu_err_header *header)
{
	header->version.major = (u16)1U;
	header->version.minor = (u16)0U;
	header->sub_err_type = 0U;
	header->sub_unit_id = 0UL;
	header->address = 0UL;
	header->timestamp_ns = 0UL;
}

static void nvgpu_init_ecc_err_msg(struct gpu_ecc_error_info *err_info)
{
	nvgpu_init_err_msg_header(&err_info->header);
	err_info->err_cnt = 0UL;
}

static void nvgpu_report_ecc_error_linux(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, u64 err_addr, u64 err_count)
{
	int err = 0;
	u32 s_id = 0;
	u8 err_status = 0;
	u8 err_info_size = 0;
	u64 timestamp = 0ULL;
	int err_threshold_counter = 0;
	struct gpu_ecc_error_info err_pkt;
	struct nvgpu_err_desc *err_desc = NULL;
	struct nvgpu_err_hw_module *hw_module = NULL;
	nv_guard_request_t req;

	memset(&req, 0, sizeof(req));
	nvgpu_init_ecc_err_msg(&err_pkt);
	if (hw_unit >= sizeof(gv11b_err_lut)/sizeof(gv11b_err_lut[0])) {
		err = -EINVAL;
		goto done;
	}

	hw_module = &gv11b_err_lut[hw_unit];
	if (err_id >= hw_module->num_errs) {
		nvgpu_err(g, "invalid err_id (%u) for hw module (%u)",
			err_id, hw_module->hw_unit);
		err = -EINVAL;
		goto done;
	}
	err_desc = &hw_module->errs[err_id];
	timestamp = (u64)nvgpu_current_time_ns();

	err_pkt.header.timestamp_ns = timestamp;
	err_pkt.header.sub_unit_id = inst;
	err_pkt.header.address = err_addr;
	err_pkt.err_cnt = err_count;
	err_info_size = sizeof(err_pkt);

	s_id = hw_module->base_ecc_service_id + err_id;

	if (err_desc->is_critical) {
		err_status = NVGUARD_ERROR_DETECTED;
	} else {
		err_status = NVGUARD_NO_ERROR;
	}

	nvgpu_atomic_inc(&err_desc->err_count);
	err_threshold_counter = nvgpu_atomic_cmpxchg(&err_desc->err_count,
			err_desc->err_threshold + 1, 0);

	if (unlikely(err_threshold_counter != err_desc->err_threshold + 1)) {
		goto done;
	}

	nvgpu_log(g, gpu_dbg_ecc, "ECC reporting hw: %s, desc:%s, count:%llu",
		hw_module->name, err_desc->name, err_count);

	req.srv_id_cmd = NVGUARD_SERVICESTATUS_NOTIFICATION;
	req.srv_status.srv_id = (nv_guard_service_id_t)s_id;
	req.srv_status.status = err_status;
	req.srv_status.timestamp = timestamp;
	req.srv_status.error_info_size = err_info_size;
	memcpy(req.srv_status.error_info, (u8*)&err_pkt, err_info_size);

	/*
	 * l1ss_submit_rq may fail due to kmalloc failures but may pass in
	 * subsequent calls
	 */
	err = l1ss_submit_rq(&req, true);
	if (err != 0) {
		nvgpu_err(g, "Error returned from L1SS submit %d", err);
	}

	if (err_desc->is_critical) {
		nvgpu_quiesce(g);
	}

done:
	return;
}

static void nvgpu_report_ecc_error_empty(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, u64 err_addr, u64 err_count) {
		nvgpu_log(g, gpu_dbg_ecc, "ECC reporting empty");
}

const struct nvgpu_ecc_reporting_ops default_disabled_ecc_report_ops = {
	.report_ecc_err = nvgpu_report_ecc_error_empty,
};

const struct nvgpu_ecc_reporting_ops ecc_enable_report_ops = {
	.report_ecc_err = nvgpu_report_ecc_error_linux,
};

static int nvgpu_l1ss_callback(l1ss_cli_callback_param param, void *data)
{
	struct gk20a *g = (struct gk20a *)data;
	struct nvgpu_os_linux *l = NULL;
	struct nvgpu_ecc_reporting_linux *ecc_reporting_linux = NULL;
	int err = 0;
	/* Ensure we have a valid gk20a struct before proceeding */
	if ((g == NULL) || (gk20a_get(g) == NULL)) {
		return -ENODEV;
	}

	l = nvgpu_os_linux_from_gk20a(g);
	ecc_reporting_linux = &l->ecc_reporting_linux;

	nvgpu_spinlock_acquire(&ecc_reporting_linux->common.lock);
	if (param == L1SS_READY) {
		if (!ecc_reporting_linux->common.ecc_reporting_service_enabled) {
			ecc_reporting_linux->common.ecc_reporting_service_enabled = true;
			ecc_reporting_linux->common.ops = &ecc_enable_report_ops;
			nvgpu_log(g, gpu_dbg_ecc, "ECC reporting is enabled");
		}
	} else if (param == L1SS_NOT_READY) {
		if (ecc_reporting_linux->common.ecc_reporting_service_enabled) {
			ecc_reporting_linux->common.ecc_reporting_service_enabled = false;
			ecc_reporting_linux->common.ops = &default_disabled_ecc_report_ops;
			nvgpu_log(g, gpu_dbg_ecc, "ECC reporting is disabled");
		}
	} else {
		err = -EINVAL;
	}
	nvgpu_spinlock_release(&ecc_reporting_linux->common.lock);

	gk20a_put(g);

	return err;
}

void nvgpu_init_ecc_reporting(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_ecc_reporting_linux *ecc_report_linux = &l->ecc_reporting_linux;
	int err = 0;
	/* This will invoke the registration API */
	nvgpu_spinlock_init(&ecc_report_linux->common.lock);
	ecc_report_linux->priv.id = (NVGUARD_GROUPID_IGPU & NVGUARD_GROUPINDEX_FIELDMASK);
	ecc_report_linux->priv.cli_callback = nvgpu_l1ss_callback;
	ecc_report_linux->priv.data = g;
	ecc_report_linux->common.ops = &default_disabled_ecc_report_ops;

	nvgpu_log(g, gpu_dbg_ecc, "ECC reporting Init");

	/*
	 * err == 0 indicates service is available but not active yet.
	 * err == 1 indicates service is available and active
	 * error for other cases.
	 */
	err = l1ss_register_client(&ecc_report_linux->priv);
	if (err == 0) {
		ecc_report_linux->common.ecc_reporting_service_enabled = false;
		nvgpu_log(g, gpu_dbg_ecc, "ECC reporting init success");
	} else if (err == 1) {
		ecc_report_linux->common.ecc_reporting_service_enabled = true;
		/* Actual Ops will be replaced during nvgpu_enable_ecc_reporting
		 * called as part of gk20a_busy()
		 */
	} else {
		nvgpu_log(g, gpu_dbg_ecc, "ECC reporting init failure %d", err);
	}
}

void nvgpu_deinit_ecc_reporting(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_ecc_reporting_linux *ecc_report_linux = &l->ecc_reporting_linux;

	if (ecc_report_linux->common.ecc_reporting_service_enabled) {
		ecc_report_linux->common.ecc_reporting_service_enabled = false;
		l1ss_deregister_client(ecc_report_linux->priv.id);
		memset(ecc_report_linux, 0, sizeof(*ecc_report_linux));
		nvgpu_log(g, gpu_dbg_ecc, "ECC reporting de-init success");
	}

}

void nvgpu_enable_ecc_reporting(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_ecc_reporting_linux *ecc_report_linux = &l->ecc_reporting_linux;
	struct nvgpu_ecc_reporting *error_reporting = &ecc_report_linux->common;

	nvgpu_spinlock_acquire(&ecc_report_linux->common.lock);
	if (error_reporting->ecc_reporting_service_enabled) {
		error_reporting->ops = &ecc_enable_report_ops;
		nvgpu_log(g, gpu_dbg_ecc, "ECC reporting is enabled");
	}
	nvgpu_spinlock_release(&ecc_report_linux->common.lock);
}

void nvgpu_disable_ecc_reporting(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_ecc_reporting_linux *ecc_report_linux = &l->ecc_reporting_linux;
	struct nvgpu_ecc_reporting *error_reporting = &ecc_report_linux->common;

	nvgpu_spinlock_acquire(&ecc_report_linux->common.lock);
	error_reporting->ops = &default_disabled_ecc_report_ops;
	nvgpu_log(g, gpu_dbg_ecc, "ECC reporting is disabled");
	nvgpu_spinlock_release(&ecc_report_linux->common.lock);
}

void nvgpu_report_ecc_err(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, u64 err_addr, u64 err_count)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_ecc_reporting_linux *ecc_report_linux = &l->ecc_reporting_linux;
	struct nvgpu_ecc_reporting *error_reporting = &ecc_report_linux->common;
	void (*report_ecc_err_func)(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, u64 err_addr, u64 err_count);

	nvgpu_spinlock_acquire(&ecc_report_linux->common.lock);
	report_ecc_err_func = error_reporting->ops->report_ecc_err;
	nvgpu_spinlock_release(&ecc_report_linux->common.lock);

	report_ecc_err_func(g, hw_unit, inst, err_id, err_addr, err_count);
}
