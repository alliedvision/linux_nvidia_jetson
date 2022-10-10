/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, NVIDIA Corporation.  All rights reserved.
 */

#ifndef __NVHOST_NVDLA_CG_REGS_H__
#define __NVHOST_NVDLA_CG_REGS_H__

#include <linux/nvhost.h>

static struct nvhost_gating_register __attribute__((__unused__))
	nvdla_gating_registers[] = {
	/* NV_PNVDLA_THI_CLK_OVERRIDE */
	{ .addr = 0x00000e00, .prod = 0x00000000, .disable = 0xffffffff },
	/* NV_PNVDLA_THI_SLCG_OVERRIDE_HIGH_A */
	{ .addr = 0x00000088, .prod = 0x00000000, .disable = 0x000000ff },
	/* NV_PNVDLA_THI_SLCG_OVERRIDE_LOW_A */
	{ .addr = 0x0000008c, .prod = 0x00000000, .disable = 0xffffffff },
	/* NV_PNVDLA_FALCON_CGCTL */
	{ .addr = 0x000010a0, .prod = 0x00000000, .disable = 0x00000001 },
	/* NV_PNVDLA_FALCON_CG2 */
	{ .addr = 0x00001134, .prod = 0x00020008, .disable = 0x0003fffe },
	{}
};

#endif /* End of __NVHOST_NVDLA_CG_REGS_H__ */
