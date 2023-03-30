/*
 * GA10B priv ring
 *
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
#include <nvgpu/timers.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/static_analysis.h>

#include "priv_ring_ga10b.h"

#include <nvgpu/hw/ga10b/hw_proj_ga10b.h>
#include <nvgpu/hw/ga10b/hw_pri_ringmaster_ga10b.h>
#include <nvgpu/hw/ga10b/hw_pri_sys_ga10b.h>
#include <nvgpu/hw/ga10b/hw_pri_gpc_ga10b.h>
#include <nvgpu/hw/ga10b/hw_pri_fbp_ga10b.h>

#ifdef CONFIG_NVGPU_MIG
#include <nvgpu/hw/ga10b/hw_pri_ringstation_sys_ga10b.h>
#include <nvgpu/grmgr.h>
#endif

/*
 * PRI Error decoding
 *
 * Each PRI error is associated with a 32 bit error code. Out of which bits 31:8
 * convey a specific error type and bits 7:0 provide additional information
 * relevant to the specific error type.
 *
 * pri_error_code captures all various types of PRI errors and provides a
 * brief description about the error along with a function to decode the extra
 * error information contained in bits 7:0. Each reported error is attempted to
 * be matched against an entry in the error types list, if a match is found the
 * error is decoded with the information from the matched entry. In case of no
 * matches the error code is reported as "unknown".
 */

/*
 * Helper functions to decode error extra fields: bits 7:0.
 */
static void decode_pri_client_error(struct gk20a *g, u32 value);
static void decode_pri_local_error(struct gk20a *g, u32 value);
static void decode_pri_falcom_mem_violation(struct gk20a *g, u32 value);
static void decode_pri_route_error(struct gk20a *g, u32 value);
static void decode_pri_source_en_violation(struct gk20a *g, u32 value);
static void decode_pri_target_mask_violation(struct gk20a *g, u32 value);
static void decode_pri_undefined_error_extra_info(struct gk20a *g, u32 value);
static void decode_host_pri_error(struct gk20a *g, u32 value);
static void decode_fecs_floorsweep_error(struct gk20a *g, u32 value);
static void decode_gcgpc_error(struct gk20a *g, u32 value);
static void decode_pri_local_decode_error(struct gk20a *g, u32 value);
static void decode_pri_client_badf50_error(struct gk20a *g, u32 value);
static void decode_fecs_pri_orphan_error(struct gk20a *g, u32 value);
static void decode_pri_direct_access_violation(struct gk20a *g, u32 value);
static void decode_pri_indirect_access_violation(struct gk20a *g, u32 value);
static void decode_pri_lock_sec_sensor_violation(struct gk20a *g, u32 value);

/*
 * Helper functions to handle priv_ring bits associated with status0.
 */
static void ga10b_priv_ring_handle_sys_write_errors(struct gk20a *g, u32 status);
static void ga10b_priv_ring_handle_fbp_write_errors(struct gk20a *g, u32 status);

/*
 * Map error code bits 31:8 to a brief description and assign a function
 * pointer capable of decoding the associated error extra bits.
 */
struct pri_error_code {
	const char *desc;
	void (*decode_pri_error_extra_info)(struct gk20a *g, u32 error_extra);
};

/*
 * Group pri error codes in the range [0xbad001xx - 0xbad002xx].
 */
static struct pri_error_code bad001xx[] = {
	{ "host pri timeout error", decode_host_pri_error },
	{ "host pri decode error", decode_host_pri_error },
	{ "undefined", decode_pri_undefined_error_extra_info }
};
static const size_t nbad001xx_entries = sizeof(bad001xx) / sizeof(*bad001xx);

/*
 * Group pri error codes in the range [0xbad00fxx - 0xbad00fxx].
 */
static struct pri_error_code bad00fxx[] = {
	{ "host fecs error", decode_pri_client_error },
	{ "undefined", decode_pri_undefined_error_extra_info }
};
static const size_t nbad00fxx_entries = sizeof(bad00fxx) / sizeof(*bad00fxx);

/*
 * Group pri error codes in the range [0xbad0b0xx - 0xbad0b0xx].
 */
static struct pri_error_code bad0b0xx[] = {
	{ "fb ack timeout error", decode_pri_client_error },
	{ "undefined", decode_pri_undefined_error_extra_info }
};
static const size_t nbad0b0xx_entries = sizeof(bad0b0xx) / sizeof(*bad0b0xx);

/*
 * Group pri error codes in the range [0xbadf10xx - 0xbadf19xx].
 */
static struct pri_error_code badf1yxx[] = {
	{ "client timeout", decode_pri_client_error },
	{ "decode error (range not found)", decode_pri_undefined_error_extra_info },
	{ "client in reset", decode_pri_client_error },
	{ "client floorswept", decode_fecs_floorsweep_error },
	{ "client stuck ack", decode_pri_client_error },
	{ "client expected ack", decode_pri_client_error },
	{ "fence error", decode_pri_client_error },
	{ "subid error", decode_pri_client_error },
	{ "rdata wait violation", decode_pri_client_error },
	{ "write byte enable errror", decode_pri_client_error },
	{ "undefined", decode_pri_undefined_error_extra_info }
};
static const size_t nbadf1yxx_entries =  sizeof(badf1yxx) / sizeof(*badf1yxx);

/*
 * Group pri error codes in the range [0xbadf20xx - 0xbadf23xx].
 */
static struct pri_error_code badf2yxx[] = {
	{ "orphan(gpc/fbp)",  decode_fecs_pri_orphan_error },
	{ "power ok timeout",  decode_pri_local_error },
	{ "orphan(gpc/fbp) powergated",  decode_fecs_pri_orphan_error },
	{ "target powergated",  decode_pri_client_error },
	{ "orphan gcgpc", decode_gcgpc_error },
	{ "decode gcgpc", decode_gcgpc_error },
	{ "local priv decode error", decode_pri_local_decode_error },
	{ "priv poisoned", decode_pri_client_error },
	{ "trans type", decode_pri_client_error },
	{ "undefined", decode_pri_undefined_error_extra_info }
};
static const size_t nbadf2yxx_entries =  sizeof(badf2yxx) / sizeof(*badf2yxx);

/*
 * Group pri error codes in the range [0xbadf30xx - 0xbadf31xx].
 */
static struct pri_error_code badf3yxx[] = {
	{ "priv ring dead",  decode_pri_client_error },
	{ "priv ring dead low power",  decode_pri_client_error },
	{ "undefined", decode_pri_undefined_error_extra_info }
};
static const size_t nbadf3yxx_entries =  sizeof(badf3yxx) / sizeof(*badf3yxx);

/*
 * Group pri error codes in the range [0xbadf40xx - 0xbadf41xx].
 */
static struct pri_error_code badf4yxx[] = {
	{ "trap",  decode_pri_client_error },
	{ "target mask violation", decode_pri_target_mask_violation },
	{ "undefined", decode_pri_undefined_error_extra_info }
};
static const size_t nbadf4yxx_entries =  sizeof(badf4yxx) / sizeof(*badf4yxx);

/*
 * Group pri error codes in the range [0xbadf50xx - 0xbadf59xx].
 */
static struct pri_error_code badf5yxx[] = {
	{ "client error", decode_pri_client_badf50_error },
	{ "priv level violation", decode_pri_direct_access_violation },
	{ "indirect priv level violation", decode_pri_indirect_access_violation },
	{ "local priv ring error", decode_pri_local_error },
	{ "falcon mem priv level violation", decode_pri_falcom_mem_violation },
	{ "route error", decode_pri_route_error },
	{ "custom error", decode_pri_undefined_error_extra_info },
	{ "source enable violation", decode_pri_source_en_violation },
	{ "unknown", decode_pri_undefined_error_extra_info },
	{ "indirect source enable violation", decode_pri_source_en_violation },
	{ "undefined", decode_pri_undefined_error_extra_info }
};
static const size_t nbadf5yxx_entries = sizeof(badf5yxx) / sizeof(*badf5yxx);

/*
 * Group pri error codes in the range [0xbadf60xx].
 */
static struct pri_error_code badf6yxx[] = {
	{ "lock from security sensor", decode_pri_lock_sec_sensor_violation },
	{ "undefined", decode_pri_undefined_error_extra_info }
};
static const size_t nbadf6yxx_entries = sizeof(badf6yxx) / sizeof(*badf6yxx);

/*
 * Group error extra values in range [0x1 - 0x5].
 */
static const char *pri_client_error_extra_0x[] = {
	"async idle",
	"async req",
	"async read data wait",
	"async hold",
	"async wait ack",
	"undefined"
};
static const size_t npri_client_error_extra_0x =
sizeof(pri_client_error_extra_0x) / sizeof(*pri_client_error_extra_0x);

/*
 * Group error extra values in range [0x20 - 0x21].
 */
static const char *pri_client_error_extra_2x[] = {
	"extra sync req",
	"extra sync read data wait",
	"undefined"
};
static const size_t npri_client_error_extra_2x =
sizeof(pri_client_error_extra_2x) / sizeof(*pri_client_error_extra_2x);

/*
 * Group error extra values in range [0x40 - 0x48].
 */
static const char *pri_client_error_extra_4x[] = {
	"no such address",
	"task protection",
	"external error",
	"index range errror",
	"reset",
	"register in reset",
	"power gated",
	"subpri floor swept",
	"subpri clock off",
	"undefined"
};
static const size_t npri_client_error_extra_4x =
sizeof(pri_client_error_extra_4x) / sizeof(*pri_client_error_extra_4x);

/*
 * Group error extra values for route error in range [0x45 - 0x46].
 */
static const char *pri_route_error_extra_4x[] = {
	"write only address",
	"timeout",
	"undefined"
};
static const size_t npri_route_error_extra_4x =
sizeof(pri_route_error_extra_4x) / sizeof(*pri_route_error_extra_4x);

static void decode_pri_undefined_error_extra_info(struct gk20a *g, u32 value)
{
	nvgpu_err(g, "[Extra Info]: undefined, value(0x%x)", value);
}

static void decode_host_pri_error(struct gk20a *g, u32 value)
{
	u32 sub_id;

	sub_id = HOST_PRIV_SUBID_MSK_VAL(value);

	nvgpu_err(g, "[Extra Info]: sub_id(0x%x)", sub_id);
}

static void decode_fecs_floorsweep_error(struct gk20a *g, u32 value)
{
	u32 source_id;

	source_id = FECS_PRIV_SOURCEID_MSK_VAL(value);

	nvgpu_err(g, "[Extra Info]: client floorswept source_id(0x%x)", source_id);
}

static void decode_gcgpc_error(struct gk20a *g, u32 value)
{
	u32 source_id;

	source_id = FECS_PRIV_SOURCEID_MSK_VAL(value);

	nvgpu_err(g, "[Extra Info]: GCGPC error source_id(0x%x)", source_id);
}

static void decode_pri_local_decode_error(struct gk20a *g, u32 value)
{
	u32 source_id;

	source_id = FECS_PRIV_SOURCEID_MSK_VAL(value);

	nvgpu_err(g, "[Extra Info]: pri local decode source_id(0x%x)", source_id);
}

static void decode_pri_client_error(struct gk20a *g, u32 value)
{
	const char **lookup_table = { (const char* []){ "undefined" } };
	size_t lookup_table_size = 1;
	size_t index = 0;

	if (value >= pri_sys_pri_error_extra_extra_sync_req_v()) {
		index = value - pri_sys_pri_error_extra_extra_sync_req_v();
		lookup_table = pri_client_error_extra_2x;
		lookup_table_size = npri_client_error_extra_2x;

	} else if (value >= pri_sys_pri_error_extra_async_idle_v()) {
		index = value - pri_sys_pri_error_extra_async_idle_v();
		lookup_table = pri_client_error_extra_0x;
		lookup_table_size = npri_client_error_extra_0x;
	}

	/*
	 * An index which falls outside the lookup table size is considered
	 * unknown. The index is updated to the last valid entry of the table,
	 * which is reserved for this purpose.
	 */
	if (index >= lookup_table_size) {
		index = lookup_table_size - 1;
	}

	nvgpu_err(g, "[Extra Info]: %s, value(0x%x)",
			lookup_table[index], value);
}

static void decode_pri_client_badf50_error(struct gk20a *g, u32 value)
{
	const char **lookup_table = { (const char* []){ "undefined" } };
	size_t lookup_table_size = 1;
	size_t index = 0;

	if (value >= pri_sys_pri_error_extra_no_such_address_v()) {
		index = (size_t)value - pri_sys_pri_error_extra_no_such_address_v();
		lookup_table = pri_client_error_extra_4x;
		lookup_table_size = npri_client_error_extra_4x;

	}

	/*
	 * An index which falls outside the lookup table size is considered
	 * unknown. The index is updated to the last valid entry of the table,
	 * which is reserved for this purpose.
	 */
	if (index >= lookup_table_size) {
		index = lookup_table_size - 1UL;
	}

	nvgpu_err(g, "[Extra Info]: %s, value(0x%x)",
			lookup_table[index], value);
}

static void decode_fecs_pri_orphan_error(struct gk20a *g, u32 value)
{
	u32 target_ringstation;

	target_ringstation = FECS_PRIV_ORPHAN_TARGET_RINGSTN_MSK_VAL(value);

	nvgpu_err(g, "[Extra Info]: target_ringstation(0x%x)",
		  target_ringstation);
}

static void decode_pri_target_mask_violation(struct gk20a *g, u32 value)
{
	u32 target_mask, source_id;

	target_mask = TARGET_MASK_VIOLATION_MSK_VAL(value);
	source_id = TARGET_MASK_VIOLATION_SRCID_VAL(value);

	nvgpu_err(g, "[Extra Info]: target_mask(0x%x), source_id(0x%x)",
			target_mask, source_id);
}

static void decode_pri_direct_access_violation(struct gk20a *g, u32 value)
{
	u32 priv_mask = PRI_ACCESS_VIOLATION_MSK_VAL(value);
	u32 priv_level = PRI_ACCESS_VIOLATON_LEVEL_VAL(value);

	nvgpu_err(g, "[Extra Info]: priv_level(0x%x), priv_mask(0x%x)",
			priv_level, priv_mask);
}

static void decode_pri_indirect_access_violation(struct gk20a *g, u32 value)
{
	u32 cur_priv_level = PRI_ACCESS_VIOLATION_CUR_REQPL_VAL(value);
	u32 orig_priv_level = PRI_ACCESS_VIOLATION_ORIG_REQPL_VAL(value);

	nvgpu_err(g, "[Extra Info]: orig_priv_level(0x%x), cur_priv_level(0x%x)",
			orig_priv_level, cur_priv_level);
}

static void decode_pri_falcom_mem_violation(struct gk20a *g, u32 value)
{
	bool imem_violation = true;
	u32 fault_priv_level;
	u32 mem_priv_level_mask;

	if (value & FALCON_DMEM_VIOLATION_MSK()) {
		imem_violation = false;
	}
	fault_priv_level = FALCON_MEM_VIOLATION_PRIVLEVEL_ACCESS_VAL(value);
	mem_priv_level_mask = FALCON_MEM_VIOLATION_PRIVLEVEL_MSK_VAL(value);
	nvgpu_err(g, "[Extra Info]: %s violation %s, fault_priv_level(0x%x),"\
			"mem_priv_level_mask(0x%x)",
			imem_violation ? "IMEM" : "DMEM",
			(value & FALCON_MEM_VIOLATION_MSK_VIOLATION()) != 0U ?
				"unequal" : "mask violation",
			fault_priv_level, mem_priv_level_mask);
}

static void decode_pri_route_error(struct gk20a *g, u32 value)
{
	const char **lookup_table = { (const char* []){ "undefined" } };
	size_t lookup_table_size = 1;
	size_t index = 0;

	if (value >= pri_sys_pri_error_fecs_pri_route_err_extra_write_only_v()) {
		index = value - pri_sys_pri_error_fecs_pri_route_err_extra_write_only_v();
		lookup_table = pri_route_error_extra_4x;
		lookup_table_size = npri_route_error_extra_4x;
	}

	/*
	 * An index which falls outside the lookup table size is considered
	 * unknown. The index is updated to the last valid entry of the table,
	 * which is reserved for this purpose.
	 */
	if (index >= lookup_table_size) {
		index = lookup_table_size - 1;
	}

	nvgpu_err(g, "[Extra Info]: %s, value(0x%x)", lookup_table[index],
			value);
}

static void decode_pri_source_en_violation(struct gk20a *g, u32 value)
{
	u32 priv_level, source_id, source_ctrl;

	priv_level = SRC_EN_VIOLATION_PRIV_LEVEL_VAL(value);
	source_ctrl = SRC_EN_VIOLAION_CTRL_VAL(value);
	source_id = SRC_EN_VIOLATION_SRCID_VAL(value);

	nvgpu_err(g, "[Extra Info]: priv_level(0x%x), source_ctrl(0x%x),"\
			" source_id(0x%x)", priv_level, source_ctrl, source_id);
}

static void decode_pri_local_error(struct gk20a *g, u32 value)
{
	if (value == pri_sys_pri_error_local_priv_ring_extra_no_such_target_v()) {
		nvgpu_err(g, "[Extra Info]: no such target, value(0x%x)",
				value);
		return;
	}

	nvgpu_err(g, "[Extra Info]: target index(0x%x)",
			PRIV_LOCAL_TARGET_INDEX(value));
}

static void decode_pri_lock_sec_sensor_violation(struct gk20a *g, u32 value)
{
	nvgpu_err(g, "[Extra Info]: pmu(%s), gsp(%s),"\
		     " sec2(%s), nvdclk(%s), fuse_scm(%s), fuse_prod(%s)",
		  (value & PRI_LOCK_SEC_SENSOR_PMU_MSK()) != 0U ? "yes" : "no",
		  (value & PRI_LOCK_SEC_SENSOR_GSP_MSK()) != 0U ? "yes" : "no",
		  (value & PRI_LOCK_SEC_SENSOR_SEC2_MSK()) != 0U ? "yes" : "no",
		  (value & PRI_LOCK_SEC_SENSOR_NVDCLK_MSK()) != 0U ? "yes" : "no",
		  (value & PRI_LOCK_SEC_SENSOR_FUSE_SCM_MSK()) != 0U ? "yes" : "no",
		  (value & PRI_LOCK_SEC_SENSOR_FUSE_PROD_MSK()) != 0U ? "yes" : "no");
}

void ga10b_priv_ring_decode_error_code(struct gk20a *g, u32 error_code)
{
	u32 err_code;
	u32 error_extra;
	const struct pri_error_code unknown_error_code =
			{ "undefined", decode_pri_undefined_error_extra_info };
	const struct pri_error_code *error_lookup_table = &unknown_error_code;
	size_t lookup_table_size = 1;
	size_t index = 0;

	err_code = pri_sys_pri_error_code_v(error_code);
	error_extra = pri_sys_pri_error_extra_v(error_code);

	if (err_code ==  pri_sys_pri_error_code_fecs_pri_timeout_v()) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PRI,
				GPU_PRI_TIMEOUT_ERROR);
	} else {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PRI,
				GPU_PRI_ACCESS_VIOLATION);
	}

	if (err_code >= pri_sys_pri_error_code_fecs_pri_lock_from_security_sensor_v()) {
		index = err_code -
			pri_sys_pri_error_code_fecs_pri_lock_from_security_sensor_v();
		error_lookup_table = badf6yxx;
		lookup_table_size = nbadf6yxx_entries;
	} else if (err_code >= pri_sys_pri_error_code_fecs_pri_client_err_v()) {
		index = err_code - pri_sys_pri_error_code_fecs_pri_client_err_v();
		error_lookup_table = badf5yxx;
		lookup_table_size = nbadf5yxx_entries;
	} else if (err_code >= pri_sys_pri_error_code_fecs_trap_v()) {
		index = err_code - pri_sys_pri_error_code_fecs_trap_v();
		error_lookup_table = badf4yxx;
		lookup_table_size = nbadf4yxx_entries;
	} else if (err_code >= pri_sys_pri_error_code_fecs_dead_ring_v()) {
		index = err_code - pri_sys_pri_error_code_fecs_dead_ring_v();
		error_lookup_table = badf3yxx;
		lookup_table_size = nbadf3yxx_entries;
	} else if (err_code >= pri_sys_pri_error_code_fecs_pri_orphan_v()) {
		index = err_code - pri_sys_pri_error_code_fecs_pri_orphan_v();
		error_lookup_table = badf2yxx;
		lookup_table_size = nbadf2yxx_entries;
	} else if (err_code >=  pri_sys_pri_error_code_fecs_pri_timeout_v()) {
		index = err_code - pri_sys_pri_error_code_fecs_pri_timeout_v();
		error_lookup_table = badf1yxx;
		lookup_table_size = nbadf1yxx_entries;
	} else if (err_code ==  pri_sys_pri_error_code_host_fb_ack_timeout_v()) {
		error_lookup_table = bad0b0xx;
		lookup_table_size = nbad0b0xx_entries;
	} else if (err_code == pri_sys_pri_error_code_host_fecs_err_v()) {
		error_lookup_table = bad00fxx;
		lookup_table_size = nbad00fxx_entries;
	} else if (err_code == pri_sys_pri_error_code_host_pri_timeout_v()) {
		error_lookup_table = bad001xx;
		lookup_table_size = nbad001xx_entries;
	}

	/*
	 * An index which falls outside the lookup table size is considered
	 * unknown. The index is updated to the last valid entry of the table,
	 * which is reserved for this purpose.
	 */
	if (index >= lookup_table_size) {
		index = lookup_table_size - 1;
	}

	nvgpu_err(g, "[Error Type]: %s", error_lookup_table[index].desc);
	error_lookup_table[index].decode_pri_error_extra_info(g, error_extra);
}

static void ga10b_priv_ring_handle_sys_write_errors(struct gk20a *g, u32 status)
{
	u32 error_info;
	u32 error_code;
	u32 error_adr, error_wrdat;

	if (pri_ringmaster_intr_status0_gbl_write_error_sys_v(status) == 0U) {
		return;
	}

	error_info = nvgpu_readl(g, pri_sys_priv_error_info_r());
	error_code = nvgpu_readl(g, pri_sys_priv_error_code_r());
	error_adr = nvgpu_readl(g, pri_sys_priv_error_adr_r());
	error_wrdat = nvgpu_readl(g, pri_sys_priv_error_wrdat_r());
	nvgpu_err(g, "SYS write error: ADR 0x%08x WRDAT 0x%08x master 0x%08x",
		error_adr, error_wrdat,
		pri_sys_priv_error_info_priv_master_v(error_info));
	nvgpu_err(g,
		"INFO 0x%08x: (subid 0x%08x priv_level %d local_ordering %d)",
		error_info, pri_sys_priv_error_info_subid_v(error_info),
		pri_sys_priv_error_info_priv_level_v(error_info),
		pri_sys_priv_error_info_local_ordering_v(error_info));
	nvgpu_err(g , "CODE 0x%08x", error_code);
	g->ops.priv_ring.decode_error_code(g, error_code);
}

static void ga10b_priv_ring_handle_fbp_write_errors(struct gk20a *g, u32 status)
{
	u32 fbp_status, fbp_stride, fbp_offset, fbp;
	u32 error_info;
	u32 error_code;
	u32 error_adr, error_wrdat;

	fbp_status = pri_ringmaster_intr_status0_gbl_write_error_fbp_v(status);
	if (fbp_status == 0U) {
		return;
	}

	fbp_stride = proj_fbp_priv_stride_v();
	for (fbp = 0U; fbp < g->ops.priv_ring.get_fbp_count(g); fbp++) {
		if ((fbp_status & BIT32(fbp)) == 0U) {
			continue;
		}
		fbp_offset = nvgpu_safe_mult_u32(fbp, fbp_stride);
		error_info = nvgpu_readl(g, nvgpu_safe_add_u32(
					pri_fbp_fbp0_priv_error_info_r(),
					fbp_offset));
		error_code = nvgpu_readl(g, nvgpu_safe_add_u32(
					pri_fbp_fbp0_priv_error_code_r(),
					fbp_offset));
		error_adr = nvgpu_readl(g, nvgpu_safe_add_u32(
					pri_fbp_fbp0_priv_error_adr_r(),
					fbp_offset));
		error_wrdat = nvgpu_readl(g, nvgpu_safe_add_u32(
					pri_fbp_fbp0_priv_error_wrdat_r(),
					fbp_offset));

		nvgpu_err(g, "FBP%u write error: "\
			"ADR 0x%08x WRDAT 0x%08x master 0x%08x", fbp, error_adr,
			error_wrdat,
			pri_fbp_fbp0_priv_error_info_priv_master_v(error_info));
		nvgpu_err(g, "INFO 0x%08x: "\
			"(subid 0x%08x priv_level %d local_ordering %d)",
			error_info,
			pri_fbp_fbp0_priv_error_info_subid_v(error_info),
			pri_fbp_fbp0_priv_error_info_priv_level_v(error_info),
			pri_fbp_fbp0_priv_error_info_local_ordering_v(error_info));
		nvgpu_err(g, "CODE 0x%08x", error_code);
		g->ops.priv_ring.decode_error_code(g, error_code);

		fbp_status = fbp_status & (~(BIT32(fbp)));
		if (fbp == 0U) {
			break;
		}
	}
}

void ga10b_priv_ring_isr_handle_0(struct gk20a *g, u32 status0)
{
	if (status0 == 0) {
		return;
	}

	if (pri_ringmaster_intr_status0_ring_start_conn_fault_v(status0) != 0U) {
		nvgpu_err(g, "connectivity problem on the startup sequence");
	}

	if (pri_ringmaster_intr_status0_disconnect_fault_v(status0) != 0U) {
		nvgpu_err(g, "ring disconnected");
	}

	if (pri_ringmaster_intr_status0_overflow_fault_v(status0) != 0U) {
		nvgpu_err(g, "ring overflowed");
	}

	if (pri_ringmaster_intr_status0_ring_enum_fault_v(status0) != 0U) {
		nvgpu_err(g, "mismatch between FS and enumerated RSs");
	}

	if (pri_ringmaster_intr_status0_gpc_rs_map_config_fault_v(status0) != 0U) {
		nvgpu_err(g, "invalid GPC_RS_MAP");
	}

	ga10b_priv_ring_handle_sys_write_errors(g, status0);
	ga10b_priv_ring_handle_fbp_write_errors(g, status0);
}

void ga10b_priv_ring_isr_handle_1(struct gk20a *g, u32 status1)
{
	u32 error_info;
	u32 error_code;
	u32 error_adr, error_wrdat;
	u32 gpc;
	u32 gpc_stride, gpc_offset;

	if (status1 == 0U) {
		return;
	}

	gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_PRIV_STRIDE);
	for (gpc = 0U; gpc < g->ops.priv_ring.get_gpc_count(g); gpc++) {
		if ((status1 & BIT32(gpc)) == 0U) {
			continue;
		}
		gpc_offset = nvgpu_safe_mult_u32(gpc, gpc_stride);
		error_info = nvgpu_readl(g, nvgpu_safe_add_u32(
					pri_gpc_gpc0_priv_error_info_r(),
					gpc_offset));
		error_code = nvgpu_readl(g, nvgpu_safe_add_u32(
					pri_gpc_gpc0_priv_error_code_r(),
					gpc_offset));
		error_adr = nvgpu_readl(g, nvgpu_safe_add_u32(
					pri_gpc_gpc0_priv_error_adr_r(),
					gpc_offset));
		error_wrdat = nvgpu_readl(g, nvgpu_safe_add_u32(
					pri_gpc_gpc0_priv_error_wrdat_r(),
					gpc_offset));

		nvgpu_err(g, "GPC%u write error: "\
			"ADR 0x%08x WRDAT 0x%08x master 0x%08x", gpc, error_adr,
			error_wrdat,
			pri_gpc_gpc0_priv_error_info_priv_master_v(error_info));
		nvgpu_err(g,"INFO 0x%08x: "\
			"(subid 0x%08x priv_level %d local_ordering %d)",
			error_info,
			pri_gpc_gpc0_priv_error_info_subid_v(error_info),
			pri_gpc_gpc0_priv_error_info_priv_level_v(error_info),
			pri_gpc_gpc0_priv_error_info_local_ordering_v(error_info));
		nvgpu_err(g, "CODE 0x%08x", error_code);

		g->ops.priv_ring.decode_error_code(g, error_code);

		status1 = status1 & (~(BIT32(gpc)));
		if (status1 == 0U) {
			break;
		}
	}
}

u32 ga10b_priv_ring_enum_ltc(struct gk20a *g)
{
	return nvgpu_readl(g, pri_ringmaster_enum_ltc_r());
}

void ga10b_priv_ring_read_pri_fence(struct gk20a *g)
{
	/* Read back to ensure all writes to all chiplets are complete. */
	nvgpu_readl(g, pri_sys_pri_fence_r());
	nvgpu_readl(g, pri_gpc_pri_fence_r());
	nvgpu_readl(g, pri_fbp_pri_fence_r());
}

#ifdef CONFIG_NVGPU_MIG
int ga10b_priv_ring_config_gr_remap_window(struct gk20a *g, u32 gr_syspipe_id,
		bool enable)
{
	u32 reg_val;

	reg_val = nvgpu_readl(g, pri_ringstation_sys_bar0_to_pri_window_r());

	reg_val = set_field(reg_val,
		pri_ringstation_sys_bar0_to_pri_window_index_m(),
		pri_ringstation_sys_bar0_to_pri_window_index_f(
			gr_syspipe_id));

	if(enable) {
		reg_val = set_field(reg_val,
			pri_ringstation_sys_bar0_to_pri_window_enable_m(),
			pri_ringstation_sys_bar0_to_pri_window_enable_f(
				pri_ringstation_sys_bar0_to_pri_window_enable_enable_v()));
	} else {
		reg_val = set_field(reg_val,
			pri_ringstation_sys_bar0_to_pri_window_enable_m(),
			pri_ringstation_sys_bar0_to_pri_window_enable_f(
				pri_ringstation_sys_bar0_to_pri_window_enable_disable_v()));
	}
	nvgpu_writel(g, pri_ringstation_sys_bar0_to_pri_window_r(), reg_val);

	nvgpu_log(g, gpu_dbg_mig,
		"old_gr_syspipe_id[%u] new_gr_syspipe_id[%u] "
			"enable[%u] reg_val[%x] ",
		g->mig.current_gr_syspipe_id, gr_syspipe_id, enable, reg_val);

	return 0;
}

int ga10b_priv_ring_config_gpc_rs_map(struct gk20a *g, bool enable)
{
	u32 reg_val;
	u32 index;
	u32 local_id;
	u32 logical_gpc_id = 0U;
	struct nvgpu_gr_syspipe *gr_syspipe;

	for (index = 0U; index < g->mig.num_gpu_instances; index++) {
		if (!nvgpu_grmgr_is_mig_type_gpu_instance(
				&g->mig.gpu_instance[index])) {
			nvgpu_log(g, gpu_dbg_mig, "skip physical instance[%u]",
				index);
			continue;
		}
		gr_syspipe = &g->mig.gpu_instance[index].gr_syspipe;

		for (local_id = 0U; local_id < gr_syspipe->num_gpc; local_id++) {
			logical_gpc_id = gr_syspipe->gpcs[local_id].logical_id;
			reg_val = nvgpu_readl(g, pri_ringmaster_gpc_rs_map_r(
				logical_gpc_id));

			if (enable) {
				reg_val = set_field(reg_val,
					pri_ringmaster_gpc_rs_map_smc_engine_id_m(),
					pri_ringmaster_gpc_rs_map_smc_engine_id_f(
						gr_syspipe->gr_syspipe_id));
				reg_val = set_field(reg_val,
					pri_ringmaster_gpc_rs_map_smc_engine_local_cluster_id_m(),
					pri_ringmaster_gpc_rs_map_smc_engine_local_cluster_id_f(
						local_id));
				reg_val = set_field(reg_val,
					pri_ringmaster_gpc_rs_map_smc_enable_m(),
					pri_ringmaster_gpc_rs_map_smc_enable_f(
						pri_ringmaster_gpc_rs_map_smc_enable_true_v()));
			} else {
				reg_val = set_field(reg_val,
					pri_ringmaster_gpc_rs_map_smc_enable_m(),
					pri_ringmaster_gpc_rs_map_smc_enable_f(
						pri_ringmaster_gpc_rs_map_smc_enable_false_v()));
			}

			nvgpu_writel(g, pri_ringmaster_gpc_rs_map_r(logical_gpc_id),
				reg_val);

			nvgpu_log(g, gpu_dbg_mig,
				"[%d] gpu_instance_id[%u] gr_syspipe_id[%u] gr_instance_id[%u] "
					"local_gpc_id[%u] physical_id[%u] logical_id[%u] "
					"gpcgrp_id[%u] reg_val[%x] enable[%d] ",
				index,
				g->mig.gpu_instance[index].gpu_instance_id,
				gr_syspipe->gr_syspipe_id,
				gr_syspipe->gr_instance_id,
				local_id,
				gr_syspipe->gpcs[local_id].physical_id,
				gr_syspipe->gpcs[local_id].logical_id,
				gr_syspipe->gpcs[local_id].gpcgrp_id,
				reg_val,
				enable);
		}
		/*
		 * Do a dummy read on last written GPC to ensure that RS_MAP has been acked
		 * by all slave ringstations.
		 */
		reg_val = nvgpu_readl(g, pri_ringmaster_gpc_rs_map_r(
			logical_gpc_id));
	}

	return 0;
}
#endif
