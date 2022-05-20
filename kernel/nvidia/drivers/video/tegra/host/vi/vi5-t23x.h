/*
 * Copyright (c) 2021, NVIDIA Corporation.  All rights reserved.
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
 *
 * struct of_device_id initialization for VI on T23x
 */

//static const struct of_device_id tegra_vi5_of_match[] = {

	{
		.name = "vi0",
		.compatible = "nvidia,tegra234-vi",
		.data = &t23x_vi0_info,
	},
	{
		.name = "vi1",
		.compatible = "nvidia,tegra234-vi",
		.data = &t23x_vi1_info,
	},

//}
