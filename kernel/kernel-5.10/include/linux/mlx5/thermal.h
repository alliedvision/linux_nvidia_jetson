/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MLX5_THERMAL_DRIVER_H
#define MLX5_THERMAL_DRIVER_H

#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/pci.h>
#include <linux/irq.h>
#include <linux/spinlock_types.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/xarray.h>
#include <linux/workqueue.h>
#include <linux/mempool.h>
#include <linux/interrupt.h>
#include <linux/idr.h>
#include <linux/notifier.h>
#include <linux/refcount.h>
#include <linux/thermal.h>

#include <linux/mlx5/device.h>
#include <linux/mlx5/doorbell.h>
#include <linux/mlx5/eq.h>
#include <linux/timecounter.h>
#include <linux/ptp_clock_kernel.h>
#include <net/devlink.h>

#ifdef CONFIG_MLX5_CORE_THERMAL
int mlx5_thermal_init(struct mlx5_core_dev *core);
void mlx5_thermal_deinit(struct mlx5_core_dev *core);
#else
static inline int mlx5_thermal_init(struct mlx5_core_dev *core)
{
	core->thermal = NULL;
	return 0;
}

static inline void mlx5_thermal_deinit(struct mlx5_core_dev *core)
{

}
#endif

#endif /* MLX5_THERMAL_DRIVER_H */
