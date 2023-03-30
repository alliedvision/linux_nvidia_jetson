/*
 * drivers/video/tegra/nvmap/nvmap_kasan_wrapper.c
 *
 * place to add wrapper function to drop kasan scan
 *
 * Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#include <linux/export.h>
#include <linux/types.h>
#include <linux/io.h>
#include "nvmap_ioctl.h"

void kasan_memcpy_toio(void __iomem *to,
			const void *from, size_t count)
{
	while (count && (!IS_ALIGNED((unsigned long)to, 8) ||
			 !IS_ALIGNED((unsigned long)from, 8))) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}

	while (count >= 8) {
		__raw_writeq(*(u64 *)from, to);
		from += 8;
		to += 8;
		count -= 8;
	}

	while (count) {
		__raw_writeb(*(u8 *)from, to);
		from++;
		to++;
		count--;
	}
}
