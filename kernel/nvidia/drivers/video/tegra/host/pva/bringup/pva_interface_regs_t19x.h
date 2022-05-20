/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __PVA_INTERFACE_REGS_T19X_H__
#define __PVA_INTERFACE_REGS_T19X_H__

#include "pva.h"
#include "pva_mailbox.h"

#define NUM_INTERFACES_T19X     1

#define PVA_CCQ_STATUS3_REG     0x7200c
#define PVA_CCQ_STATUS4_REG     0x72010
#define PVA_CCQ_STATUS5_REG     0x72014
#define PVA_CCQ_STATUS6_REG     0x72018
#define PVA_CCQ_STATUS7_REG     0x7201c

void read_status_interface_t19x(struct pva *pva,
				uint32_t interface_id, u32 isr_status,
				struct pva_cmd_status_regs *status_output);

#endif
