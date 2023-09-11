/*
 * Host1x Application Specific Virtual Memory
 *
 * Copyright (c) 2022, NVIDIA Corporation.  All rights reserved.
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

#ifndef IOMMU_CONTEXT_DEV_H
#define IOMMU_CONTEXT_DEV_H

struct platform_device
*nvpva_iommu_context_dev_allocate(char *identifier, size_t len, bool shared);
void nvpva_iommu_context_dev_release(struct platform_device *pdev);
int nvpva_iommu_context_dev_get_sids(int *hwids, int *count, int max_cnt);
bool is_cntxt_initialized(void);

#endif
