/*
 * PVA Command Queue Interface handling
 *
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef PVA_CCQ_T23X_H
#define PVA_CCQ_T23X_H

#include <linux/kernel.h>

#include "pva.h"
#include "pva_status_regs.h"

int pva_ccq_send_task_t23x(struct pva *pva, struct pva_cmd *cmd);
void pva_ccq_isr_handler(struct pva *pva, unsigned int queue_id);
int pva_ccq_send_cmd_sync(struct pva *pva,
			struct pva_cmd *cmd, u32 nregs,
			struct pva_cmd_status_regs *ccq_status_regs);

int pva_send_cmd_sync(struct pva *pva,
			struct pva_cmd *cmd, u32 nregs,
			struct pva_cmd_status_regs *ccq_status_regs);
int pva_send_cmd_sync_locked(struct pva *pva,
			struct pva_cmd *cmd, u32 nregs,
			struct pva_cmd_status_regs *ccq_status_regs);

#endif
