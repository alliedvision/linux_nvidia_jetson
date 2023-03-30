/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
 * This header provides macros for different types and conversions
 */

#ifndef _DT_BINDINGS_TYPES_H_
#define _DT_BINDINGS_TYPES_H_

/*
 * S32_TO_U32: This macro converts the signed number to 2's complement
 * unisgned number. E.g. S32_TO_U32(-3) will be 0xfffffffd and
 * S32_TO_U32(3) will be 0x3;
 * Use of_property_read_s32() for getting back the correct signed value
 * in driver.
 */
#define S32_TO_U32(x) (((x) < 0) ? (((-(x)) ^ 0xFFFFFFFFU) + 1) : (x))

#endif

