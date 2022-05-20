/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>

#include "os_linux.h"

/*
 * Locks out the driver from accessing GPU registers. This prevents access to
 * thse registers after the GPU has been clock or power gated. This should help
 * find annoying bugs where register reads and writes are silently dropped
 * after the GPU has been turned off. On older chips these reads and writes can
 * also lock the entire CPU up.
 */
void nvgpu_lockout_usermode_registers(struct gk20a *g)
{
	g->usermode_regs = 0U;
}

/*
 * Undoes t19x_lockout_registers().
 */
void nvgpu_restore_usermode_registers(struct gk20a *g)
{
	g->usermode_regs = g->usermode_regs_saved;
}

void nvgpu_remove_usermode_support(struct gk20a *g)
{
	if (g->usermode_regs) {
		g->usermode_regs = 0U;
	}
}

void nvgpu_init_usermode_support(struct gk20a *g)
{
	if (g->ops.usermode.base == NULL) {
		return;
	}

	if (g->usermode_regs == 0U) {
		g->usermode_regs = g->regs + g->ops.usermode.bus_base(g);
		g->usermode_regs_saved = g->usermode_regs;
	}

	g->usermode_regs_bus_addr = g->regs_bus_addr +
					g->ops.usermode.bus_base(g);
}
