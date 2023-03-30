/*
 * Copyright (c) 2018-2019, NVIDIA Corporation.  All rights reserved.
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

#include "os/linux/os_linux.h"

#include "os/linux/debug_therm_tu104.h"
#include "os/linux/debug_clk_tu104.h"
#include "os/linux/debug_volt.h"
#include "os/linux/debug_s_param.h"

static struct nvgpu_os_linux_ops tu104_os_linux_ops = {
	.therm = {
		.init_debugfs = tu104_therm_init_debugfs,
	},
	.clk = {
		.init_debugfs = tu104_clk_init_debugfs,
	},
	.volt = {
		.init_debugfs = nvgpu_volt_init_debugfs,
	},
	.s_param = {
		.init_debugfs = nvgpu_s_param_init_debugfs,
	},
};

void nvgpu_tu104_init_os_ops(struct nvgpu_os_linux *l)
{
	l->ops.therm = tu104_os_linux_ops.therm;
	l->ops.clk = tu104_os_linux_ops.clk;
	l->ops.volt = tu104_os_linux_ops.volt;
	l->ops.s_param = tu104_os_linux_ops.s_param;
}
