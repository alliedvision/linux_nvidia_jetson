/*
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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

#ifndef PVA_BIT_HELPERS_H_
#define PVA_BIT_HELPERS_H_

#include <linux/types.h>

#define RMOS_BYTES_PER_WORD (sizeof(unsigned int))
#define RMOS_BITS_PER_WORD (RMOS_BYTES_PER_WORD * 8U)

static inline uint32_t rmos_get_first_set_bit(uint32_t val)
{
	uint32_t index = 0U;

	for (index = 0U; index < 32U; index++) {
		if (1U == (val & 1U))
			break;

		val = val >> 1U;
	}

	return index;
}

static inline uint32_t rmos_get_first_zero_bit(uint32_t val)
{
	if ((~(uint32_t)0U) == val)
		return RMOS_BITS_PER_WORD;

	return rmos_get_first_set_bit(~val);
}

static inline uint32_t rmos_find_first_zero_bit(uint32_t *addr, uint32_t size)
{
	const uint32_t *p = addr;
	uint32_t result = 0U;
	uint32_t tmp;
	uint32_t first_zero_bit;

	while (size >= RMOS_BITS_PER_WORD) {
		tmp = *(p++);
		if (0U != ~tmp) {
			first_zero_bit = rmos_get_first_zero_bit(tmp);

			/*
			 * Result will not wrap around in any case as the
			 * Maximum possible return value is the 'size' itself.
			 */
			return result + first_zero_bit;
		}
		result += RMOS_BITS_PER_WORD;
		size -= RMOS_BITS_PER_WORD;
	}

	if (size == 0U)
		return result;

	tmp = (*p) | (~0U << size);
	tmp = rmos_get_first_zero_bit(tmp);
	if (tmp == 32U)
		return result + size;

	return result + tmp;
}

static inline void rmos_set_bit32(unsigned int nr, unsigned int *addr)
{
	*addr |= (1U << nr);
}

static inline void rmos_clear_bit32(unsigned int nr, unsigned int *addr)
{
	*addr &= ~(1U << nr);
}

static inline bool rmos_test_bit32(unsigned int nr, const unsigned int *addr)
{
	return (*addr & (1 << nr)) != 0U;
}

#endif
