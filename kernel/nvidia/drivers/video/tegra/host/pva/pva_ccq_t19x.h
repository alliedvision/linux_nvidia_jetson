/*
 * PVA Command Queue Interface handling
 *
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef PVA_CCQ_T19X_H
#define PVA_CCQ_T19X_H

#include <linux/kernel.h>

#include "pva.h"

int pva_ccq_send_task_t19x(struct pva *pva, u32 queue_id, dma_addr_t task_addr,
			   u8 batchsize, u32 flags);

#endif
