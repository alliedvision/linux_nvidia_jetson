/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_LINUX_DRIVER_COMMON
#define NVGPU_LINUX_DRIVER_COMMON

struct gk20a;

int nvgpu_probe(struct gk20a *g,
		const char *debugfs_symlink);

void nvgpu_init_gk20a(struct gk20a *g);
void nvgpu_read_support_gpu_tools(struct gk20a *g);

#endif
