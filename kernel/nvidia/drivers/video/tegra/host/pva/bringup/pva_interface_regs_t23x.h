/*
 * Copyright (c) 2016-2019, NVIDIA Corporation. All rights reserved.
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

#ifndef __PVA_INTERFACE_REGS_T23X_H__
#define __PVA_INTERFACE_REGS_T23X_H__

#include "pva.h"

#define NUM_INTERFACES_T23X	9

#define PVA_EMPTY_STATUS_REG	0

#define PVA_MBOX_STATUS4_REG	0x178000
#define PVA_MBOX_STATUS5_REG	0x180000
#define PVA_MBOX_STATUS6_REG	0x188000
#define PVA_MBOX_STATUS7_REG	0x190000

#define PVA_CCQ0_STATUS3_REG    0x260010
#define PVA_CCQ0_STATUS4_REG	0x260014
#define PVA_CCQ0_STATUS5_REG	0x260018
#define PVA_CCQ0_STATUS6_REG	0x26001c

#define PVA_CCQ1_STATUS3_REG    0x270010
#define PVA_CCQ1_STATUS4_REG	0x270014
#define PVA_CCQ1_STATUS5_REG	0x270018
#define PVA_CCQ1_STATUS6_REG	0x27001c

#define PVA_CCQ2_STATUS3_REG    0x280010
#define PVA_CCQ2_STATUS4_REG	0x280014
#define PVA_CCQ2_STATUS5_REG	0x280018
#define PVA_CCQ2_STATUS6_REG	0x28001c

#define PVA_CCQ3_STATUS3_REG    0x290010
#define PVA_CCQ3_STATUS4_REG	0x290014
#define PVA_CCQ3_STATUS5_REG	0x290018
#define PVA_CCQ3_STATUS6_REG	0x29001c

#define PVA_CCQ4_STATUS3_REG    0x2a0010
#define PVA_CCQ4_STATUS4_REG	0x2a0014
#define PVA_CCQ4_STATUS5_REG	0x2a0018
#define PVA_CCQ4_STATUS6_REG	0x2a001c

#define PVA_CCQ5_STATUS3_REG    0x2b0010
#define PVA_CCQ5_STATUS4_REG	0x2b0014
#define PVA_CCQ5_STATUS5_REG	0x2b0018
#define PVA_CCQ5_STATUS6_REG	0x2b001c

#define PVA_CCQ6_STATUS3_REG    0x2c0010
#define PVA_CCQ6_STATUS4_REG	0x2c0014
#define PVA_CCQ6_STATUS5_REG	0x2c0018
#define PVA_CCQ6_STATUS6_REG	0x2c001c

#define PVA_CCQ7_STATUS3_REG    0x2d0010
#define PVA_CCQ7_STATUS4_REG	0x2d0014
#define PVA_CCQ7_STATUS5_REG	0x2d0018
#define PVA_CCQ7_STATUS6_REG	0x2d001c

void read_status_interface_t23x(struct pva *pva,
				uint32_t interface_id, u32 isr_status,
				struct pva_cmd_status_regs *status_output);
#endif
