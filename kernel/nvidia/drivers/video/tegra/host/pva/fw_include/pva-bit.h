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

#ifndef PVA_BIT_H
#define PVA_BIT_H

/*
 * Bit manipulation macros
 */

#define PVA_BITS_PER_BYTE 8UL
/*
 * 8-bits
 */
#define PVA_BIT8(_b_) ((uint8_t)(((uint8_t)1U << (_b_)) & 0xffu))

/*
 * 8-bits
 */
#define PVA_BIT8(_b_) ((uint8_t)(((uint8_t)1U << (_b_)) & 0xffu))
#define PVA_MASK8(_msb_, _lsb_)                                                \
	((uint8_t)((((PVA_BIT8(_msb_) - 1U) | PVA_BIT8(_msb_)) &               \
		    ~(PVA_BIT8(_lsb_) - 1U)) &                                 \
		   0xff))
#define PVA_EXTRACT8(_x_, _msb_, _lsb_, _type_)                                \
	((_type_)(((_x_)&PVA_MASK8((_msb_), (_lsb_))) >> (_lsb_)))
#define PVA_EXTRACT8_RANGE(_x_, _name_, _type_)                                \
	PVA_EXTRACT8(_x_, (_name_##_MSB), (_name_##_LSB), _type_)
#define PVA_INSERT8(_x_, _msb_, _lsb_)                                         \
	((((uint8_t)(_x_)) << (_lsb_)) & PVA_MASK8((_msb_), (_lsb_)))

/*
 * 16-bits
 */
#define PVA_BIT16(_b_) ((uint16_t)(((uint16_t)1U << (_b_)) & 0xffffu))
#define PVA_MASK16(_msb_, _lsb_)                                               \
	((uint16_t)((((PVA_BIT16(_msb_) - 1U) | PVA_BIT16(_msb_)) &            \
		     ~(PVA_BIT16(_lsb_) - 1U)) &                               \
		    0xffff))
#define PVA_EXTRACT16(_x_, _msb_, _lsb_, _type_)                               \
	((_type_)(((_x_)&PVA_MASK16((_msb_), (_lsb_))) >> (_lsb_)))
#define PVA_INSERT16(_x_, _msb_, _lsb_)                                        \
	((((uint16_t)(_x_)) << (_lsb_)) & PVA_MASK16((_msb_), (_lsb_)))

/*
 * 32-bits
 */
#define PVA_BIT(_b_) ((uint32_t)(((uint32_t)1U << (_b_)) & 0xffffffffUL))
#define PVA_MASK(_msb_, _lsb_)                                                 \
	(((PVA_BIT(_msb_) - 1U) | PVA_BIT(_msb_)) & ~(PVA_BIT(_lsb_) - 1U))
#define PVA_MASK_RANGE(_name_) PVA_MASK((_name_##_MSB), (_name_##_LSB))
#define PVA_EXTRACT(_x_, _msb_, _lsb_, _type_)                                 \
	((_type_)(((_x_)&PVA_MASK((_msb_), (_lsb_))) >> (_lsb_)))
#define PVA_EXTRACT_RANGE(_x_, _name_, _type_)                                 \
	PVA_EXTRACT(_x_, (_name_##_MSB), (_name_##_LSB), _type_)
#define PVA_INSERT(_x_, _msb_, _lsb_)                                          \
	((((uint32_t)(_x_)) << (_lsb_)) & (uint32_t)PVA_MASK((_msb_), (_lsb_)))
#define PVA_INSERT_RANGE(_x_, _name_)                                          \
	PVA_INSERT(_x_, (_name_##_MSB), (_name_##_LSB))

/*
 * 64-bits
 */
#define PVA_BIT64(_b_)                                                         \
	((uint64_t)(((uint64_t)1UL << (_b_)) & ((uint64_t)(0U) - 1U)))
#define PVA_MASK64(_msb_, _lsb_)                                               \
	(((PVA_BIT64(_msb_) - (uint64_t)1U) | PVA_BIT64(_msb_)) &              \
	 ~(PVA_BIT64(_lsb_) - (uint64_t)1U))
#define PVA_MASK64_RANGE(_name_) PVA_MASK64((_name_##_MSB), (_name_##_LSB))
#define PVA_EXTRACT64(_x_, _msb_, _lsb_, _type_)                               \
	((_type_)(((_x_)&PVA_MASK64((_msb_), (_lsb_))) >> (_lsb_)))
#define PVA_EXTRACT64_RANGE(_x_, _name_, _type_)                               \
	PVA_EXTRACT64(_x_, (_name_##_MSB), (_name_##_LSB), _type_)
#define PVA_INSERT64(_x_, _msb_, _lsb_)                                        \
	((((uint64_t)(_x_)) << (_lsb_)) & PVA_MASK64((_msb_), (_lsb_)))
#define PVA_INSERT64_RANGE(_x_, _name_)                                        \
	PVA_INSERT64(_x_, (_name_##_MSB), (_name_##_LSB))

#define PVA_PACK64(_l_, _h_)                                                   \
	(PVA_INSERT64((_h_), 63U, 32U) | PVA_INSERT64((_l_), 31U, 0U))

#define PVA_HI32(_x_) PVA_EXTRACT64((_x_), 63U, 32U, uint32_t)
#define PVA_LOW32(_x_) PVA_EXTRACT64((_x_), 31U, 0U, uint32_t)

#define PVA_RANGE_LOW(_name_) (_name_##_LSB)
#define PVA_RANGE_HIGH(_name_) (_name_##_MSB)
#define PVA_NUM_IN_RANGE(_n_, _name_)                                          \
	((PVA_RANGE_LOW(_name_) <= (_n_)) && ((_n_) <= PVA_RANGE_HIGH(_name_)))

#endif
