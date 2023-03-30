/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_BITOPS_LINUX_H
#define NVGPU_BITOPS_LINUX_H

#include <linux/bitops.h>
#include <linux/bitmap.h>

static inline void nvgpu_bitmap_set(unsigned long *map, unsigned int start,
					unsigned int len)
{
	BUG_ON(len > U32(INT_MAX));
	bitmap_set(map, start, (int)len);
}

static inline void nvgpu_bitmap_clear(unsigned long *map, unsigned int start,
					unsigned int len)
{
	BUG_ON(len > U32(INT_MAX));
	bitmap_clear(map, start, (int)len);
}

static inline bool nvgpu_test_bit(unsigned int nr,
					const volatile unsigned long *addr)
{
	BUG_ON(nr > U32(INT_MAX));
	return test_bit((int)nr, addr);
}

static inline bool nvgpu_test_and_set_bit(unsigned int nr,
						volatile unsigned long *addr)
{
	BUG_ON(nr > U32(INT_MAX));
	return test_and_set_bit((int)nr, addr);
}

static inline bool nvgpu_test_and_clear_bit(unsigned int nr,
						volatile unsigned long *addr)
{
	BUG_ON(nr > U32(INT_MAX));
	return test_and_clear_bit((int)nr, addr);
}

static inline void nvgpu_set_bit(unsigned int nr, volatile unsigned long *addr)
{
	BUG_ON(nr > U32(INT_MAX));
	set_bit((int)nr, addr);
}

static inline void nvgpu_clear_bit(unsigned int nr,
					volatile unsigned long *addr)
{
	BUG_ON(nr > U32(INT_MAX));
	clear_bit((int)nr, addr);
}

static inline unsigned long nvgpu_ffs(unsigned long word)
{
	unsigned long ret = 0UL;

	if (word == 0UL) {
		return ret;
	}

	ret = __ffs(word) + 1UL;

	return ret;
}
static inline unsigned long nvgpu_fls(unsigned long word)
{
	unsigned long ret = 0UL;

	if (word == 0UL) {
		return ret;
	}

	ret = __fls(word) + 1UL;

	return ret;
}

#endif /* NVGPU_LOCK_LINUX_H */
