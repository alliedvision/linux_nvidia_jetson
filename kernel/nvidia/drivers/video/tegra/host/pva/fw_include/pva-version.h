/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION. All rights reserved.
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

#ifndef PVA_VERSION_H
#define PVA_VERSION_H

#include <pva-types.h>
#include <pva-bit.h>
#include <pva-fw-version.h>

#define PVA_MAKE_VERSION(_type_, _major_, _minor_, _subminor_)                 \
	(PVA_INSERT(_type_, 31, 24) | PVA_INSERT(_major_, 23, 16) |            \
	 PVA_INSERT(_minor_, 15, 8) | PVA_INSERT(_subminor_, 7, 0))

#define PVA_VERSION(_type_)                                                    \
	PVA_MAKE_VERSION(_type_, PVA_VERSION_MAJOR, PVA_VERSION_MINOR,         \
			 PVA_VERSION_SUBMINOR)

#endif
