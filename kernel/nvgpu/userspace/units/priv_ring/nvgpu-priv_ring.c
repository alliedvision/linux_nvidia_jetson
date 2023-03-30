/*
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

#include <unit/unit.h>
#include <unit/utils.h>
#include <unit/io.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/cic_mon.h>

#include <nvgpu/gk20a.h>
#include <hal/priv_ring/priv_ring_gm20b.h>
#include <hal/priv_ring/priv_ring_gp10b.h>
#include <hal/init/hal_gv11b_litter.h>
#include <hal/mc/mc_gp10b.h>
#include "hal/cic/mon/cic_ga10b.h"

#include <nvgpu/hw/gv11b/hw_pri_ringstation_sys_gv11b.h>
#include <nvgpu/hw/gv11b/hw_pri_ringstation_gpc_gv11b.h>
#include <nvgpu/hw/gv11b/hw_pri_ringmaster_gv11b.h>
#include <nvgpu/hw/gv11b/hw_proj_gv11b.h>
#include <nvgpu/hw/gv11b/hw_mc_gv11b.h>

#include "nvgpu-priv_ring.h"

u32 read_cmd_reg = 3U;

/*
 * Write callback.
 */
static void writel_access_reg_fn(struct gk20a *g,
					struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

/*
 * Read callback.
 */
static void readl_access_reg_fn(struct gk20a *g,
				struct nvgpu_reg_access *access)
{
	/* Completion of clear_interrupts is indicated by value of
	 * pri_ringmaster_command_r() changing from
	 * pri_ringmaster_command_cmd_ack_interrupt_f() to
	 * pri_ringmaster_command_cmd_no_cmd_v().
	 *
	 * During ISR, the pri_ringmaster_command_r() register is polled to
	 * check if its value changed to no_cmd.
	 * To get complete branch coverage in priv_ring.isr(), after
	 * "read_cmd_reg" read attempts, the value of pri_ringmaster_command_r()
	 * is read as pri_ringmaster_command_cmd_no_cmd_v().
	 * This maps to clearing interrupts after "read_cmd_reg" polling
	 * attempts.
	 */
	if (access->addr == pri_ringmaster_command_r()) {
		if (read_cmd_reg == 0U) {
			access->value = pri_ringmaster_command_cmd_no_cmd_v();
			return;
		}
		read_cmd_reg = nvgpu_safe_sub_u32(read_cmd_reg, 1U);
	}
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}

static struct nvgpu_posix_io_callbacks test_reg_callbacks = {
	/* Write APIs all can use the same accessor. */
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,

	/* Likewise for the read APIs. */
	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,
};

/* NV_PRIV_MASTER register space */
#define NV_PRIV_MASTER_START 0x00120000U
#define NV_PRIV_MASTER_SIZE  0x000003FFU

/* NV_PRIV_SYS register space */
#define NV_PRIV_SYS_START 0x00122000U
#define NV_PRIV_SYS_SIZE  0x000007FFU

/* NV_PRIV_GPC register space */
#define NV_PRIV_GPC_START 0x00128000U
#define NV_PRIV_GPC_SIZE  0x000007FFU

/* NV_PRIV_GPC register space */
#define NV_PMC_START 0x00000000U
#define NV_PMC_SIZE  0x00000FFFU

int test_priv_ring_setup(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Init HAL */
	g->ops.priv_ring.enable_priv_ring = gm20b_priv_ring_enable;
	g->ops.priv_ring.isr = gp10b_priv_ring_isr;
	g->ops.priv_ring.isr_handle_0 = gp10b_priv_ring_isr_handle_0;
	g->ops.priv_ring.isr_handle_1 = gp10b_priv_ring_isr_handle_1;
	g->ops.priv_ring.decode_error_code = gp10b_priv_ring_decode_error_code;
	g->ops.priv_ring.set_ppriv_timeout_settings =
					gm20b_priv_set_timeout_settings;
	g->ops.priv_ring.enum_ltc = gm20b_priv_ring_enum_ltc;
	g->ops.priv_ring.get_gpc_count = gm20b_priv_ring_get_gpc_count;
	g->ops.priv_ring.get_fbp_count = gm20b_priv_ring_get_fbp_count;
	g->ops.get_litter_value = gv11b_get_litter_value;
	g->ops.mc.intr_stall_unit_config =
					mc_gp10b_intr_stall_unit_config;
	g->ops.cic_mon.init = ga10b_cic_mon_init;

	/* Map register space NV_PRIV_MASTER */
	if (nvgpu_posix_io_add_reg_space(g, NV_PRIV_MASTER_START,
						NV_PRIV_MASTER_SIZE) != 0) {
		unit_err(m, "%s: failed to register space: NV_PRIV_MASTER\n",
								__func__);
		return UNIT_FAIL;
	}

	/* Map register space NV_PRIV_SYS */
	if (nvgpu_posix_io_add_reg_space(g, NV_PRIV_SYS_START,
						NV_PRIV_SYS_SIZE) != 0) {
		unit_err(m, "%s: failed to register space: NV_PRIV_SYS\n",
								__func__);
		return UNIT_FAIL;
	}

	/* Map register space NV_PRIV_GPC */
	if (nvgpu_posix_io_add_reg_space(g, NV_PRIV_GPC_START,
						NV_PRIV_GPC_SIZE) != 0) {
		unit_err(m, "%s: failed to register space: NV_PRIV_GPC\n",
								__func__);
		return UNIT_FAIL;
	}

	/* Map register space NV_PMC */
	if (nvgpu_posix_io_add_reg_space(g, NV_PMC_START,
						NV_PMC_SIZE) != 0) {
		unit_err(m, "%s: failed to register space: NV_PMC\n",
								__func__);
		return UNIT_FAIL;
	}

	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

	if (nvgpu_cic_mon_setup(g) != 0) {
		unit_err(m, "%s: Failed to initialize CIC\n",
				__func__);
		return UNIT_FAIL;
	}

	if (nvgpu_cic_mon_init_lut(g) != 0) {
		unit_err(m, "%s: Failed to initialize CIC LUT\n",
				__func__);
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_priv_ring_free_reg_space(struct unit_module *m, struct gk20a *g,
								void *args)
{
	/* Free register space */
	nvgpu_posix_io_delete_reg_space(g, NV_PRIV_MASTER_START);
	nvgpu_posix_io_delete_reg_space(g, NV_PRIV_SYS_START);
	nvgpu_posix_io_delete_reg_space(g, NV_PRIV_GPC_START);
	nvgpu_posix_io_delete_reg_space(g, NV_PMC_START);

	return UNIT_SUCCESS;
}

int test_enable_priv_ring(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	int err = 0;

	/*
	 * Case 1: enable_priv_ring passes
	 *
	 * 1) Configure "read_cmd_reg"=1U, this ensures that ring enumerations
	 *    completes before max_retry attempts.
	 * 2) Write pri_ringmaster_start_results_r=0x1
	 * 3) Call g->ops.priv_ring.enable_priv_ring(g)
	 */
	read_cmd_reg = 1U;
	nvgpu_posix_io_writel_reg_space(g,
			pri_ringmaster_start_results_r(), 0x1);
	err = g->ops.priv_ring.enable_priv_ring(g);

	if (err != 0) {
		unit_err(m, "priv_ring.enable_priv_ring HAL failed.\n");
		ret = UNIT_FAIL;
		goto end;
	}

	/*
	 * Case 2: enable_priv_ring times out.
	 *
	 * 1) Configure "read_cmd_reg"=U32_MAX, this ensures that
	 *    ring enumerations times out after max_retry attempts.
	 * 2) Call g->ops.priv_ring.enable_priv_ring(g)
	 */
	read_cmd_reg = U32_MAX;
	err = g->ops.priv_ring.enable_priv_ring(g);

	if (err != -ETIMEDOUT) {
		unit_err(m, "priv_ring.enable_priv_ring HAL timeout failed.\n");
		ret = UNIT_FAIL;
		goto end;
	}

	/*
	 * Case 3: enable_priv_ring enumeration fails
	 *
	 * 1) Configure "read_cmd_reg"=1U, this ensures that ring enumerations
	 *    completes before max_retry attempts.
	 * 3) Write pri_ringmaster_start_results_r=0x0
	 * 2) Call g->ops.priv_ring.enable_priv_ring(g)
	 */
	read_cmd_reg = 1U;
	nvgpu_posix_io_writel_reg_space(g,
			pri_ringmaster_start_results_r(), 0x0);
	err = g->ops.priv_ring.enable_priv_ring(g);

	if (err != -1) {
		unit_err(m, "priv_ring.enable_priv_ring HAL failed"
				" to detect enumeration fault.\n");
		ret = UNIT_FAIL;
		goto end;
	}

end:
	read_cmd_reg = 3U; // Restore to default

	return ret;
}

int test_set_ppriv_timeout_settings(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val_sys;
	u32 val_gpc;

	/* Call set_ppriv_timeout_settings HAL to set the timeout values
	 * to 0x800.
	 */
	g->ops.priv_ring.set_ppriv_timeout_settings(g);

	/* Read back the registers to make sure the timeouts are set to 0x800 */
	val_sys = nvgpu_posix_io_readl_reg_space(g,
				pri_ringstation_sys_master_config_r(0x15));
	val_gpc = nvgpu_posix_io_readl_reg_space(g,
				pri_ringstation_gpc_master_config_r(0xa));
	if ((val_sys != 0x800) || (val_gpc != 0x800)) {
		unit_err(m, "Timeout setting failed.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_enum_ltc(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set pri_ringmaster_enum_ltc_r to 0x1D */
	nvgpu_posix_io_writel_reg_space(g, pri_ringmaster_enum_ltc_r(), 0x1DU);
	val = g->ops.priv_ring.enum_ltc(g);
	if (val != 0x1DU) {
		unit_err(m, "enum LTC parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_get_gpc_count(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set Count field in pri_ringmaster_enum_gpc_r to 0x1D */
	nvgpu_posix_io_writel_reg_space(g, pri_ringmaster_enum_gpc_r(), 0x1DU);
	val = g->ops.priv_ring.get_gpc_count(g);
	if (val != 0x1DU) {
		unit_err(m, "enum GPC count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_get_fbp_count(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	u32 val;

	/* Set Count field in pri_ringmaster_enum_fbp_r to 0x1D */
	nvgpu_posix_io_writel_reg_space(g, pri_ringmaster_enum_fbp_r(), 0x1DU);
	val = g->ops.priv_ring.get_fbp_count(g);
	if (val != 0x1DU) {
		unit_err(m, "enum FBP count parsing incorrect.\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_priv_ring_isr(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;

	/* Set status0 such that:
	 * 1. start_conn_fault (Bit 0:0) = 1.
	 * 2. disconnect_fault (Bit 1:1)  = 1.
	 * 3. overflow_fault (Bit 2:2) = 1.
	 * 4. gbl_write_error (Bit 8:8) = 1.
	 * So status0 = 0x00000107*/
	nvgpu_posix_io_writel_reg_space(g, pri_ringmaster_intr_status0_r(),
						0x00000107U);

	/* Set status1 such that:
	 * 1. gbl_write_error (Bit 31:0) = 0x14.
	 */
	nvgpu_posix_io_writel_reg_space(g, pri_ringmaster_intr_status1_r(),
						0x00000014U);

	/* Set Count field in pri_ringmaster_enum_gpc_r to 0x1D. */
	nvgpu_posix_io_writel_reg_space(g, pri_ringmaster_enum_gpc_r(), 0x1DU);

	/* Call priv_ring ISR and clear the interrupts using readl callback. */
	g->ops.priv_ring.isr(g);

	/* For better branch coveage, call ISR with:
	 * g->ops.priv_ring.decode_error_code = NULL.
	 */
	g->ops.priv_ring.decode_error_code = NULL;
	g->ops.priv_ring.isr(g);

	/* To cover negative case in for loop, call ISR with
	 *  g->ops.priv_ring.get_gpc_count(g) = 0.
	 */
	nvgpu_posix_io_writel_reg_space(g, pri_ringmaster_enum_gpc_r(), 0x0U);
	g->ops.priv_ring.isr(g);

	/* Call the ISR again without clearing the interrupts and setting
	 * status0 and status1 to 0 to cover additional branches.
	 */
	read_cmd_reg = U32_MAX;
	nvgpu_posix_io_writel_reg_space(g, pri_ringmaster_intr_status0_r(), 0U);
	nvgpu_posix_io_writel_reg_space(g, pri_ringmaster_intr_status1_r(), 0U);
	g->ops.priv_ring.isr(g);

	return ret;
}

u32 error_codes[] = {
	0U,
	0xBADF1100U,
	0xBADF1800U,
	0xBADF1A00U,
	0xBADF2000U,
	0xBADF2100U,
	0xBADF3000U,
	0xBADF3100U,
	0xBADF4100U,
	0xBADF4200U,
	0xBADF5100U,
	0xBADF5500U,
	0xBADF5600U,
	U32_MAX,
};

int test_decode_error_code(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	u32 i;

	/* Call priv_ring ISR and clear the interrupts using readl callback. */
	for (i = 0; i < sizeof(error_codes)/sizeof(u32); i++) {
		g->ops.priv_ring.decode_error_code(g, error_codes[i]);
	}

	/* Call priv_ring ISR randomly on the full range */
	u32 random_code = get_random_u32(1, U32_MAX - 1);
	g->ops.priv_ring.decode_error_code(g, random_code);

	return ret;
}

struct unit_module_test priv_ring_tests[] = {
	UNIT_TEST(priv_ring_setup,	       test_priv_ring_setup,   NULL, 0),
	UNIT_TEST(priv_ring_enable_priv_ring,  test_enable_priv_ring,  NULL, 0),
	UNIT_TEST(priv_ring_set_ppriv_timeout_settings,
				    test_set_ppriv_timeout_settings,   NULL, 0),
	UNIT_TEST(priv_ring_enum_ltc,          test_enum_ltc,          NULL, 0),
	UNIT_TEST(priv_ring_get_gpc_count,     test_get_gpc_count,     NULL, 0),
	UNIT_TEST(priv_ring_get_fbp_count,     test_get_fbp_count,     NULL, 0),
	UNIT_TEST(priv_ring_decode_error_code, test_decode_error_code, NULL, 0),
	UNIT_TEST(priv_ring_isr,               test_priv_ring_isr,     NULL, 0),
	UNIT_TEST(priv_ring_free_reg_space,
				    test_priv_ring_free_reg_space,     NULL, 0),
};

UNIT_MODULE(priv_ring, priv_ring_tests, UNIT_PRIO_NVGPU_TEST);
