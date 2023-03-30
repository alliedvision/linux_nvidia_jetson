/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_COMMON_LINUX_CLK_GA10B_H

struct gk20a;

unsigned long nvgpu_ga10b_linux_clk_get_rate(
					struct gk20a *g, u32 api_domain);
int nvgpu_ga10b_linux_clk_set_rate(struct gk20a *g,
					u32 api_domain, unsigned long rate);

#endif
