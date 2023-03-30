/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

/*
 * Author: Junghyun Kim <juskim@nvidia.com>
 *
 * This header provides macros for Tegra sound bindings.
 */

#ifndef _DT_BINDINGS_TEGRA_ASOC_ALT_H
#define _DT_BINDINGS_TEGRA_ASOC_ALT_H

#define TDM_SLOT_MAP(stream_id, nth_channel, nth_byte)	\
	((stream_id << 16) | (nth_channel << 8) | nth_byte)

#endif
