/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"

void enable_cpu_core_reporting(u64 config);

/*Set the ERR_SEL register to choose the
 *node for which to enable/disable errors for
 */
void set_err_sel(u64 sel_val);

/*Set the ERR_CTRL register selected
 *by ERR_SEL
 */
void set_err_ctrl(u64 ctrl_val);

/*Get the ERR_CTRL register selected
 *by ERR_SEL
 */
u64 get_err_ctrl(void);

