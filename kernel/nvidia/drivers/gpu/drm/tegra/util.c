// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, NVIDIA Corporation.
 */

#include <linux/device.h>
#include <linux/iommu.h>

#include "util.h"

#define THI_STREAMID0		0x30
#define THI_STREAMID1		0x34

#define TRANSCFG_ATT(i, v)	(((v) & 0x3) << (i * 4))
#define TRANSCFG_SID_HW		0
#define TRANSCFG_SID_PHY	1
#define TRANSCFG_SID_FALCON	2

void tegra_drm_program_iommu_regs(struct device *dev, void __iomem *regs, u32 transcfg_offset)
{
#ifdef CONFIG_IOMMU_API
	struct iommu_fwspec *spec = dev_iommu_fwspec_get(dev);

	if (spec) {
		u32 value;

		value = TRANSCFG_ATT(1, TRANSCFG_SID_FALCON) |
			TRANSCFG_ATT(0, TRANSCFG_SID_HW);
		writel(value, regs + transcfg_offset);

		if (spec->num_ids > 0) {
			value = spec->ids[0] & 0xffff;

			/*
			 * STREAMID0 is used for input/output buffers.
			 * Initialize it to the firmware stream ID in case context isolation
			 * is not enabled, and the firmware stream ID is used for both firmware
			 * and data buffers.
			 *
			 * If context isolation is enabled, it will be
			 * overridden by the SETSTREAMID opcode as part of each job.
			 */
			writel(value, regs + THI_STREAMID0);

			/* STREAMID1 is used usually for firmware loading. */
			writel(value, regs + THI_STREAMID1);
		}
	}
#endif
}
