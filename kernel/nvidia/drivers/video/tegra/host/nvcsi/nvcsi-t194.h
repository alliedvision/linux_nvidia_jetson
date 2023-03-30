/*
 * drivers/video/tegra/host/nvcsi/nvcsi-t194.h
 *
 * Tegra T194 Graphics Host NVCSI 2
 *
 * Copyright (c) 2017-2019 NVIDIA Corporation.  All rights reserved.
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

#ifndef __NVHOST_NVCSI_T194_H__
#define __NVHOST_NVCSI_T194_H__

struct file_operations;
struct platform_device;

extern const struct file_operations tegra194_nvcsi_ctrl_ops;

int t194_nvcsi_early_probe(struct platform_device *pdev);
int t194_nvcsi_late_probe(struct platform_device *pdev);

#endif
