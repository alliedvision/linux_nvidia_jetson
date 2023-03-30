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

#ifndef PVA_FW_VERSION_H
#define PVA_FW_VERSION_H

#define VERSION_TYPE                                                           \
	(PVA_DEBUG | (SAFETY << 1) | (PVA_TEST_SUPPORT << 2) |                 \
	 (STANDALONE_TESTS << 3))

#define PVA_VERSION_MAJOR 0x08
#define PVA_VERSION_MINOR 0x02
#define PVA_VERSION_SUBMINOR 0x03

#ifndef PVA_VERSION_GCID_REVISION
#define PVA_VERSION_GCID_REVISION 0x00000000
#endif

#ifndef PVA_VERSION_BUILT_ON
#define PVA_VERSION_BUILT_ON 0x00000000
#endif

#endif
