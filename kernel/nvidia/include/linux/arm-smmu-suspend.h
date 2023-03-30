/*
 * Copyright (c) 2018-2022 NVIDIA Corporation.  All rights reserved.
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

#ifndef _ARM_SMMU_SUSPEND_H
#define _ARM_SMMU_SUSPEND_H

#ifdef CONFIG_PM_SLEEP
int arm_smmu_suspend_init(void __iomem **smmu_base, u32 *smmu_base_pa,
				int num_smmus, unsigned long smmu_size,
				unsigned long smmu_pgshift, u32 scratch_reg_pa);
void arm_smmu_suspend_exit(void);
#else
int arm_smmu_suspend_init(void __iomem **smmu_base, u32 *smmu_base_pa,
				int num_smmus, unsigned long smmu_size,
				unsigned long smmu_pgshift, u32 scratch_reg_pa)
{
	return 0;
}
void arm_smmu_suspend_exit(void) {}
#endif

#endif
