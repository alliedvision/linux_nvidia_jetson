/*
 * GP10B CDE
 *
 * Copyright (c) 2015-2019, NVIDIA Corporation.  All rights reserved.
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

#ifndef _NVHOST_GP10B_CDE
#define _NVHOST_GP10B_CDE

#include "os_linux.h"

void gp10b_cde_get_program_numbers(struct gk20a *g,
				   u32 block_height_log2,
				   u32 shader_parameter,
				   int *hprog_out, int *vprog_out);
bool gp10b_need_scatter_buffer(struct gk20a *g);
int gp10b_populate_scatter_buffer(struct gk20a *g,
				  struct sg_table *sgt,
				  size_t surface_size,
				  void *scatter_buffer_ptr,
				  size_t scatter_buffer_size);
#endif
