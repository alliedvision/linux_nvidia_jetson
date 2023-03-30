/*
 * GM20B Master Control
 *
 * Copyright (c) 2014-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/mc.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>

#include "mc_gm20b.h"

#include <nvgpu/hw/gm20b/hw_mc_gm20b.h>

u32 gm20b_get_chip_details(struct gk20a *g, u32 *arch, u32 *impl, u32 *rev)
{
	u32 val = nvgpu_readl_impl(g, mc_boot_0_r());

	if (val != U32_MAX) {

		if (arch != NULL) {
			*arch = mc_boot_0_architecture_v(val) <<
				NVGPU_GPU_ARCHITECTURE_SHIFT;
		}

		if (impl != NULL) {
			*impl = mc_boot_0_implementation_v(val);
		}

		if (rev != NULL) {
			*rev = (mc_boot_0_major_revision_v(val) << 4) |
				mc_boot_0_minor_revision_v(val);
		}
	}

	return val;
}

u32 gm20b_mc_isr_nonstall(struct gk20a *g)
{
	u32 nonstall_ops = 0U;
	u32 mc_intr_1;
	u32 i;

	mc_intr_1 = g->ops.mc.intr_nonstall(g);

	if ((mc_intr_1 & mc_intr_pbus_pending_f()) != 0U) {
		g->ops.bus.isr(g);
	}

	if (g->ops.mc.is_intr1_pending(g, NVGPU_UNIT_FIFO, mc_intr_1)) {
		nonstall_ops |= g->ops.fifo.intr_1_isr(g);
	}

	for (i = 0U; i < g->fifo.num_engines; i++) {
		const struct nvgpu_device *dev = g->fifo.active_engines[i];

		if ((mc_intr_1 & BIT32(dev->intr_id)) == 0U) {
			continue;
		}

		/* GR Engine */
		if (nvgpu_device_is_graphics(g, dev)) {
			nonstall_ops |= g->ops.gr.intr.nonstall_isr(g);
		}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
		/* CE Engine */
		if (nvgpu_device_is_ce(g, dev) &&
		    (g->ops.ce.isr_nonstall != NULL)) {
			nonstall_ops |= g->ops.ce.isr_nonstall(g,
						      dev->inst_id,
						      dev->pri_base);
		}
#endif
	}

	return nonstall_ops;
}

static int gm20b_mc_enable(struct gk20a *g, u32 mask, bool enable)
{
	u32 mc_enable_val = 0U;
	u32 reg_val = 0U;

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
		nvgpu_err(g, "Failed to %s mc_enable mask = 0x%08x",
			(enable ? "enable" : "disable"), mask);
		return -EINVAL;
	}
	return 0U;
}

static u32 gm20b_mc_unit_reset_mask(struct gk20a *g, u32 unit)
{
	u32 mask = 0U;

	switch (unit) {
	case NVGPU_UNIT_FIFO:
		mask = mc_enable_pfifo_enabled_f();
		break;
	case NVGPU_UNIT_PERFMON:
		mask = mc_enable_perfmon_enabled_f();
		break;
	case NVGPU_UNIT_GRAPH:
		mask = mc_enable_pgraph_enabled_f();
		break;
	case NVGPU_UNIT_BLG:
		mask = mc_enable_blg_enabled_f();
		break;
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	case NVGPU_UNIT_PWR:
		mask = mc_enable_pwr_enabled_f();
		break;
#endif
#ifdef CONFIG_NVGPU_NVLINK
	case NVGPU_UNIT_NVLINK:
		mask = BIT32(g->nvlink.ioctrl_table[0].reset_enum);
		break;
#endif
	case NVGPU_UNIT_CE2:
		mask = mc_enable_ce2_enabled_f();
		break;
	default:
		nvgpu_err(g, "unknown reset unit %d", unit);
		break;
	}

	return mask;
}

static u32 gm20b_mc_get_unit_reset_mask(struct gk20a *g, u32 units)
{
	u32 mask = 0U;
	unsigned long i = 0U;
	unsigned long units_bitmask = units;

	for_each_set_bit(i, &units_bitmask, 32U) {
		mask |= gm20b_mc_unit_reset_mask(g, BIT32(i));
	}
	return mask;
}

int gm20b_mc_enable_units(struct gk20a *g, u32 units, bool enable)
{
	int err = 0;
	u32 mask = gm20b_mc_get_unit_reset_mask(g, units);

	nvgpu_log(g, gpu_dbg_info, "%s units: mc_enable mask = 0x%08x",
			(enable ? "enable" : "disable"), mask);
	if (enable) {
		nvgpu_udelay(MC_RESET_DELAY_US);
	}

	err = gm20b_mc_enable(g, mask, enable);
	if (err != 0) {
		nvgpu_err(g, "Failed to %s units: mc_enable mask = 0x%08x",
			(enable ? "enable" : "disable"), mask);
	}
	return err;
}

int gm20b_mc_enable_dev(struct gk20a *g, const struct nvgpu_device *dev,
			bool enable)
{
	int err = 0;

	nvgpu_log(g, gpu_dbg_info, "%s device: mc_enable mask = 0x%08x",
			(enable ? "enable" : "disable"), BIT32(dev->reset_id));
	if (enable) {
		nvgpu_udelay(MC_RESET_DELAY_US);
	}

	err = gm20b_mc_enable(g, BIT32(dev->reset_id), enable);
	if (err != 0) {
		nvgpu_err(g, "Failed to %s device: mc_enable mask = 0x%08x",
			(enable ? "enable" : "disable"), BIT32(dev->reset_id));
	}
	return err;
}

static u32 gm20b_mc_get_devtype_reset_mask(struct gk20a *g, u32 devtype)
{
	u32 mask = 0U;
	const struct nvgpu_device *dev = NULL;

	nvgpu_device_for_each(g, dev, devtype) {
		mask |= BIT32(dev->reset_id);
	}

	if (devtype == NVGPU_DEVTYPE_LCE) {
		nvgpu_device_for_each(g, dev, NVGPU_DEVTYPE_COPY0) {
			mask |= BIT32(dev->reset_id);
		}
		nvgpu_device_for_each(g, dev, NVGPU_DEVTYPE_COPY1) {
			mask |= BIT32(dev->reset_id);
		}
		nvgpu_device_for_each(g, dev, NVGPU_DEVTYPE_COPY2) {
			mask |= BIT32(dev->reset_id);
		}
	}
	return mask;
}

int gm20b_mc_enable_devtype(struct gk20a *g, u32 devtype, bool enable)
{
	int err = 0;
	u32 mask = gm20b_mc_get_devtype_reset_mask(g, devtype);

	nvgpu_log(g, gpu_dbg_info, "%s devtype %u: mc_enable mask = 0x%08x",
			(enable ? "enable" : "disable"), devtype, mask);

	if (enable && (devtype == NVGPU_DEVTYPE_LCE)) {
		nvgpu_udelay(MC_RESET_CE_DELAY_US);
	} else {
		nvgpu_udelay(MC_RESET_DELAY_US);
	}

	err = gm20b_mc_enable(g, mask, enable);
	if (err != 0) {
		nvgpu_err(g, "Failed to %s devtype %u: mc_enable mask = 0x%08x",
			(enable ? "enable" : "disable"), devtype, mask);
	}
	return err;
}

#ifdef CONFIG_NVGPU_LS_PMU
bool gm20b_mc_is_enabled(struct gk20a *g, u32 unit)
{
	u32 mask = gm20b_mc_unit_reset_mask(g, unit);

	return (nvgpu_readl(g, mc_enable_r()) & mask) != 0U;
}
#endif
