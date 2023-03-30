/*
 * GM20B priv ring
 *
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log.h>
#include <nvgpu/timers.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/mc.h>

#include "priv_ring_gm20b.h"

#include <nvgpu/hw/gm20b/hw_pri_ringmaster_gm20b.h>
#include <nvgpu/hw/gm20b/hw_pri_ringstation_sys_gm20b.h>
#include <nvgpu/hw/gm20b/hw_pri_ringstation_gpc_gm20b.h>

#define PRIV_INIT_POLL_MAX_RETRIES 		60U
#define PRIV_INIT_POLL_DELAY_US			500U

int gm20b_priv_ring_enable(struct gk20a *g)
{
	unsigned int retry  = 0U;
	u32 status = 0U;

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		nvgpu_log_info(g, "priv ring is already enabled");
		nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_PRIV_RING,
					 NVGPU_CIC_INTR_ENABLE);
		return 0;
	}
#endif

	nvgpu_log_info(g, "enabling priv ring");

	nvgpu_cg_slcg_priring_load_enable(g);

	/*
	 * Enable interrupt early on.
	 */
	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_PRIV_RING,
					 NVGPU_CIC_INTR_ENABLE);

	nvgpu_writel(g, pri_ringmaster_command_r(),
			pri_ringmaster_command_cmd_enumerate_and_start_ring_f());

	/*
	 * Wait for enumeration to complete and verify it has passed.
	 */
	while ((nvgpu_readl(g, pri_ringmaster_command_r()) != 0U)
           && retry < PRIV_INIT_POLL_MAX_RETRIES) {
		nvgpu_udelay(PRIV_INIT_POLL_DELAY_US);
		retry++;
	}

	if (retry == PRIV_INIT_POLL_MAX_RETRIES) {
		nvgpu_err(g, "priv ring enumeration timedout");

		return -ETIMEDOUT;
	}

	status = nvgpu_readl(g, pri_ringmaster_start_results_r());
	if (pri_ringmaster_start_results_connectivity_v(status) !=
			pri_ringmaster_start_results_connectivity_pass_v()) {
		nvgpu_err(g, "priv ring enumeration failed, status(0x%x)",
				nvgpu_readl(g, pri_ringmaster_intr_status0_r()));

		return -1;
	}

	return 0;
}

void gm20b_priv_set_timeout_settings(struct gk20a *g)
{
	/*
	 * Bug 1340570: increase the clock timeout to avoid potential
	 * operation failure at high gpcclk rate. Default values are 0x400.
	 */
	nvgpu_writel(g, pri_ringstation_sys_master_config_r(0x15), 0x800);
	nvgpu_writel(g, pri_ringstation_gpc_master_config_r(0xa), 0x800);
}

u32 gm20b_priv_ring_enum_ltc(struct gk20a *g)
{
	return nvgpu_readl(g, pri_ringmaster_enum_ltc_r());
}

u32 gm20b_priv_ring_get_gpc_count(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g, pri_ringmaster_enum_gpc_r());
	return pri_ringmaster_enum_gpc_count_v(tmp);
}

u32 gm20b_priv_ring_get_fbp_count(struct gk20a *g)
{
	u32 tmp;

	tmp = nvgpu_readl(g, pri_ringmaster_enum_fbp_r());
	return pri_ringmaster_enum_fbp_count_v(tmp);
}
