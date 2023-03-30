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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/mlx5/driver.h>
#include <linux/mlx5/thermal.h>

#define MLX5_THERMAL_POLL_INT	1000
#define MLX5_THERMAL_NUM_TRIPS	0

/* Make sure all trips are writable */
#define MLX5_THERMAL_TRIP_MASK	(BIT(MLX5_THERMAL_NUM_TRIPS) - 1)

static int mlx5_thermal_get_mtmp_temp(struct mlx5_core_dev *core, u32 id, int *p_temp)
{
	u32 mtmp_out[MLX5_ST_SZ_DW(mtmp_reg)];
	u32 mtmp_in[MLX5_ST_SZ_DW(mtmp_reg)];
	int err;

	memset(mtmp_in, 0, sizeof(mtmp_in));

	MLX5_SET(mtmp_reg, mtmp_in, sensor_id, id);

	err = mlx5_core_access_reg(core, &mtmp_in,  sizeof(mtmp_in),
				   &mtmp_out, sizeof(mtmp_out),
				   MLX5_REG_MTMP, 0, 0);

	if (err)
		*p_temp = -1; //If something is broken, report wrong temp
	else
		/* The unit of temp returned is in 0.125 C. The thermal
		 * framework expects the value in 0.001 C.
		 */
		*p_temp = MLX5_GET(mtmp_reg, mtmp_out, temp) * 125;

	return 0;
}

static int mlx5_thermal_get_temp(struct thermal_zone_device *tzdev,
				 int *p_temp)
{
	struct mlx5_thermal *thermal = tzdev->devdata;
	struct mlx5_core_dev *core = thermal->core;
	int ret = 0;

	ret = mlx5_thermal_get_mtmp_temp(core, 0, p_temp);

	return ret;
}

static struct thermal_zone_device_ops mlx5_thermal_ops = {
	.get_temp = mlx5_thermal_get_temp,
};

int mlx5_thermal_init(struct mlx5_core_dev *core)
{
	struct mlx5_thermal *thermal;
	struct device *dev = &core->pdev->dev;
	struct thermal_zone_device *tzd;
	const char *data = "mlx5";

	core->thermal = NULL;

	tzd = thermal_zone_get_zone_by_name(data);
	if (!IS_ERR(tzd))
		return 0;

	thermal = devm_kzalloc(dev, sizeof(*thermal), GFP_KERNEL);
	if (!thermal)
		return -ENOMEM;

	thermal->core = core;
	thermal->tzdev = thermal_zone_device_register("mlx5", 0,
						      MLX5_THERMAL_TRIP_MASK,
						      thermal,
						      &mlx5_thermal_ops,
						      NULL, 0, MLX5_THERMAL_POLL_INT);
	if (IS_ERR(thermal->tzdev))
		return PTR_ERR(thermal->tzdev);

	core->thermal = thermal;
	return 0;
}

void mlx5_thermal_deinit(struct mlx5_core_dev *core)
{
	if (core->thermal)
		thermal_zone_device_unregister(core->thermal->tzdev);
}
