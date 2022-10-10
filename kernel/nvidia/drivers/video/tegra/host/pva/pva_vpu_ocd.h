/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

#ifndef PVA_VPU_OCD_H
#define PVA_VPU_OCD_H
#include <linux/types.h>
#include "pva.h"

int pva_vpu_ocd_init(struct pva *pva);
int pva_vpu_ocd_io(struct pva_vpu_dbg_block *block, u32 instr, const u32 *wdata,
		   u32 nw, u32 *rdata, u32 nr);
#endif // PVA_VPU_OCD_H
