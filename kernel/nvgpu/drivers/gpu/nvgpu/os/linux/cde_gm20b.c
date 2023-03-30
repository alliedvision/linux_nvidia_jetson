/*
 * GM20B CDE
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

#include <nvgpu/gk20a.h>

#include "cde_gm20b.h"

enum programs {
	PROG_HPASS              = 0,
	PROG_VPASS_LARGE        = 1,
	PROG_VPASS_SMALL        = 2,
	PROG_HPASS_DEBUG        = 3,
	PROG_VPASS_LARGE_DEBUG  = 4,
	PROG_VPASS_SMALL_DEBUG  = 5,
	PROG_PASSTHROUGH        = 6,
};

void gm20b_cde_get_program_numbers(struct gk20a *g,
				   u32 block_height_log2,
				   u32 shader_parameter,
				   int *hprog_out, int *vprog_out)
{
	int hprog = PROG_HPASS;
	int vprog = (block_height_log2 >= 2) ?
		PROG_VPASS_LARGE : PROG_VPASS_SMALL;
	if (shader_parameter == 1) {
		hprog = PROG_PASSTHROUGH;
		vprog = PROG_PASSTHROUGH;
	} else if (shader_parameter == 2) {
		hprog = PROG_HPASS_DEBUG;
		vprog = (block_height_log2 >= 2) ?
			PROG_VPASS_LARGE_DEBUG :
			PROG_VPASS_SMALL_DEBUG;
	}

	*hprog_out = hprog;
	*vprog_out = vprog;
}
