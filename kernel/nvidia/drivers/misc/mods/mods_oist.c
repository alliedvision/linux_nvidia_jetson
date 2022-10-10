// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include "mods_internal.h"
#include <linux/arm-smccc.h>

#define SMCCC_VERSION 0x80000000

int esc_mods_oist_status(struct mods_client             *client,
		struct MODS_TEGRA_OIST_STATUS  *p)
{
	int ret = 0;
	struct arm_smccc_res res = { 0 };

	if (p->smc_func_id == SMCCC_VERSION) {
		// For SMC version, We are only reading res.a0 value, not a1,a2,a3
		arm_smccc_1_1_smc(p->smc_func_id, res.a0, &res);
		p->smc_status = res.a0;
	} else {
		arm_smccc_1_1_smc(p->smc_func_id, p->a1, p->a2, &res);
		p->smc_status = res.a1;
	}

	return ret;
}
