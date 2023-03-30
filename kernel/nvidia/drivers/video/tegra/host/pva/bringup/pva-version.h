/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2022 NVIDIA Corporation. All rights reserved.
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

#ifndef _PVA_VERSION_H_
#define _PVA_VERSION_H_

#include "pva-bit.h"

#define PVA_MAKE_VERSION(_type_, _major_, _minor_, _subminor_)	\
	(PVA_INSERT(_type_, 31, 24)				\
	| PVA_INSERT(_major_, 23, 16)				\
	| PVA_INSERT(_minor_, 15, 8)				\
	| PVA_INSERT(_subminor_, 7, 0))

static inline uint8_t
pva_is_compatible(uint32_t version, uint32_t compat_version)
{
	return PVA_EXTRACT(version, 23, 0, uint32_t)
		>= PVA_EXTRACT(compat_version, 23, 0, uint32_t);
}

#endif
