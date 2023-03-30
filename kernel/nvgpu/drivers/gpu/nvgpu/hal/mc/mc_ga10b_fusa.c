/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/timers.h>
#include <nvgpu/mc.h>
#include <nvgpu/soc.h>
#include <nvgpu/device.h>

#include "mc_ga10b.h"

#include <nvgpu/hw/ga10b/hw_mc_ga10b.h>

/*
 * In GA10B, multiple registers exist to reset various types of devices.
 * NV_PMC_ENABLE register:
 * - This register should be used to reset (disable then enable) available
 *   h/w units.
 *
 * NV_PMC_ELPG_ENABLE register:
 * - This register is protected by priviledge level mask and is used for secure
 *   reset of XBAR, L2 and HUB units.
 * - NOTE: XBAR, L2 and HUB cannot be enabled/disabled independently.
 * - BPMP controls these units by writing to NV_PMC_ELPG_ENABLE register.
 * - There is one bit across both PMC_ENABLE and ELPG_ENABLE used to reset
 *   units.
 *
 * NV_PMC_DEVICE_ENABLE register:
 * - This register controls reset of esched-method-driven engines enumerated
 *   in nvgpu_device_info structure.
 * - If device_info reset_id is VALID and is_engine is TRUE then
 *   NV_PMC_DEVICE_ENABLE(i) index and bit position can be computed as below:
 *    - register index, i = reset_id / 32
 *    - bit position in 'i'th register word = reset_id % 32
 * - If device_info reset_id is VALID but is_engine is FALSE, then this hardware
 *   unit reset is available in NV_PMC_ENABLE register.
 * - If device_info reset_id is invalid, given device is not driven by any
 *   NV_PMC register.
 *
 * NV_PMC_DEVICE_ELPG_ENABLE register:
 * - Behaves like NV_PMC_DEVICE_ENABLE register.
 * - An engine is out of reset only when both NV_PMC_DEVICE_ELPG_ENABLE and
 *   NV_PMC_DEVICE_ENABLE have same value in that engine's bit position within
 *   the array.
 * - BPMP controls engine state by writing to NV_PMC_DEVICE_ELPG_ENABLE
 *   register.
 */

static int ga10b_mc_poll_device_enable(struct gk20a *g, u32 reg_idx,
	u32 poll_val)
{
	u32 reg_val;
	u32 delay = POLL_DELAY_MIN_US;
	struct nvgpu_timeout timeout;

	nvgpu_timeout_init_cpu_timer(g, &timeout, MC_ENGINE_RESET_DELAY_US);

	reg_val = nvgpu_readl(g, mc_device_enable_r(reg_idx));

	/*
	 * Engine disable/enable status can also be checked by using
	 * status field of mc_device_enable_r().
	 */

	while ((poll_val != reg_val) &&
		(nvgpu_timeout_expired(&timeout) == 0)) {

		nvgpu_log(g, gpu_dbg_info,
			"poll device_enable_r(%u) to be set to 0x%08x",
			reg_idx, poll_val);

		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
		reg_val = nvgpu_readl(g, mc_device_enable_r(reg_idx));
	}

	if (reg_val != poll_val) {
		nvgpu_err(g, "Failed to set device_enable_r(%u) to 0x%08x",
			reg_idx, poll_val);
		return -ETIMEDOUT;
	}
	return 0;
}

static u32 ga10b_mc_unit_reset_mask(struct gk20a *g, u32 unit)
{
	u32 mask = 0U;

	switch (unit) {
	case NVGPU_UNIT_PERFMON:
		mask = mc_enable_perfmon_m();
		break;
	case NVGPU_UNIT_FIFO:
	case NVGPU_UNIT_GRAPH:
	case NVGPU_UNIT_BLG:
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	case NVGPU_UNIT_PWR:
#endif
		nvgpu_log_info(g, "unsupported nvgpu reset unit %d", unit);
		break;
	default:
		WARN(true, "unknown nvgpu reset unit %d", unit);
		break;
	}

	return mask;
}

static u32 ga10b_mc_get_unit_reset_mask(struct gk20a *g, u32 units)
{
	u32 mask = 0U;
	unsigned long i = 0U;
	unsigned long units_bitmask = units;

	for_each_set_bit(i, &units_bitmask, 32U) {
		mask |= ga10b_mc_unit_reset_mask(g, BIT32(i));
	}
	return mask;
}

int ga10b_mc_enable_units(struct gk20a *g, u32 units, bool enable)
{
	u32 mc_enable_val = 0U;
	u32 reg_val = 0U;
	u32 mask = ga10b_mc_get_unit_reset_mask(g, units);

	nvgpu_log(g, gpu_dbg_info, "%s units: mc_enable mask = 0x%08x",
			(enable ? "enable" : "disable"), mask);
	if (enable) {
		nvgpu_udelay(MC_RESET_DELAY_US);
	}

	nvgpu_spinlock_acquire(&g->mc.enable_lock);
	reg_val = nvgpu_readl(g, mc_enable_r());
	if (enable) {
		mc_enable_val = reg_val | mask;
	} else {
		mc_enable_val = reg_val & (~mask);
	}
	nvgpu_writel(g, mc_enable_r(), mc_enable_val);
	reg_val = nvgpu_readl(g, mc_enable_r());
	nvgpu_spinlock_release(&g->mc.enable_lock);

	nvgpu_udelay(MC_ENABLE_DELAY_US);

	if (reg_val != mc_enable_val) {
		nvgpu_err(g, "Failed to %s units: mc_enable mask = 0x%08x",
			(enable ? "enable" : "disable"), mask);
		return -EINVAL;
	}
	return 0U;
}

static int ga10b_mc_enable_engine(struct gk20a *g, u32 *device_enable_val,
		bool enable)
{
	u32 reg_val;
	u32 i;
	int err = 0;

	nvgpu_spinlock_acquire(&g->mc.enable_lock);

	for (i = 0U; i < mc_device_enable__size_1_v(); i++) {
		nvgpu_log(g, gpu_dbg_info, "%s device_enable_r[%u]: 0x%08x",
			(enable ? "enable" : "disable"), i,
			device_enable_val[i]);

		reg_val = nvgpu_readl(g, mc_device_enable_r(i));

		if (enable) {
			reg_val |= device_enable_val[i];
		} else {
			reg_val &= ~device_enable_val[i];
		}

		nvgpu_writel(g, mc_device_enable_r(i), reg_val);
		err = ga10b_mc_poll_device_enable(g, i, reg_val);

		if (err != 0) {
			nvgpu_err(g, "Couldn't %s device_enable_reg[%u]: 0x%x]",
				(enable ? "enable" : "disable"), i, reg_val);
		}
	}

	nvgpu_spinlock_release(&g->mc.enable_lock);
	return err;
}

int ga10b_mc_enable_dev(struct gk20a *g, const struct nvgpu_device *dev,
			bool enable)
{
	int err = 0;
	u32 device_enable_val[mc_device_enable__size_1_v()] = {0};
	u32 reg_index = RESET_ID_TO_REG_IDX(dev->reset_id);

	device_enable_val[reg_index] |= RESET_ID_TO_REG_MASK(dev->reset_id);

	err = ga10b_mc_enable_engine(g, device_enable_val, enable);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_info,
			"Engine [id: %u] reset failed", dev->engine_id);
	}
	return 0;
}

static void ga10b_mc_get_devtype_reset_mask(struct gk20a *g, u32 devtype,
	u32 *device_enable_reg)
{
	u32 reg_index = 0U;
	const struct nvgpu_device *dev = NULL;

	nvgpu_device_for_each(g, dev, devtype) {
		reg_index = RESET_ID_TO_REG_IDX(dev->reset_id);
		device_enable_reg[reg_index] |=
			RESET_ID_TO_REG_MASK(dev->reset_id);
	}
}

int ga10b_mc_enable_devtype(struct gk20a *g, u32 devtype, bool enable)
{
	int err = 0;
	u32 device_enable_val[mc_device_enable__size_1_v()] = {0};

	ga10b_mc_get_devtype_reset_mask(g, devtype, device_enable_val);

	err = ga10b_mc_enable_engine(g, device_enable_val, enable);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_info, "Devtype: %u reset failed", devtype);
	}
	return 0;
}

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void ga10b_mc_elpg_enable(struct gk20a *g)
{
	/*
	 * This is required only when bpmp is not running.
	 * Independently resetting XBAR, L2, or HUB is not
	 * supported. Disabling any of these will cause
	 * XBAR, L2, and HUB to go into reset. To bring any of
	 * these three out of reset, software should enable
	 * all of these.
	 */
	if (!nvgpu_platform_is_silicon(g)) {
		nvgpu_writel(g, mc_elpg_enable_r(),
				mc_elpg_enable_xbar_enabled_f() |
				mc_elpg_enable_l2_enabled_f() |
				mc_elpg_enable_hub_enabled_f());
		nvgpu_readl(g, mc_elpg_enable_r());
	}

}
#endif
