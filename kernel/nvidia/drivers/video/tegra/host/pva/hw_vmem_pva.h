/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _hw_vmem_pva_h_
#define _hw_vmem_pva_h_

#define NUM_HEM_GEN		2U
#define VMEM_REGION_COUNT	3U
#define T19X_VMEM0_START	0x40U
#define T19X_VMEM0_END		0x10000U
#define T19X_VMEM1_START	0x40000U
#define T19X_VMEM1_END		0x50000U
#define T19X_VMEM2_START	0x80000U
#define T19X_VMEM2_END		0x90000U

#define T23x_VMEM0_START	0x40U
#define T23x_VMEM0_END		0x20000U
#define T23x_VMEM1_START	0x40000U
#define T23x_VMEM1_END		0x60000U
#define T23x_VMEM2_START	0x80000U
#define T23x_VMEM2_END		0xA0000U

#endif
