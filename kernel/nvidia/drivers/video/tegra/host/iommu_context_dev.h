/*
 * Host1x Application Specific Virtual Memory
 *
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All rights reserved.
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

struct platform_device *iommu_context_dev_allocate(void *identifier, struct device *dev);
void iommu_context_dev_release(struct platform_device *pdev);

#endif
