/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_UTIL_COMMON_H
#define DCE_UTIL_COMMON_H

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>
#include <linux/workqueue.h>

/**
 * This file contains all dce common fucntions and data strutcures which are
 * abstarcted out from the operating system. The underlying OS layer will
 * implement the pertinent low level details. This design is to make sure that
 * dce cpu driver can be leveraged across multiple OSes if the neede arises.
 */

struct tegra_dce;

void dce_writel(struct tegra_dce *d, u32 r, u32 v);

u32 dce_readl(struct tegra_dce *d, u32 r);

void dce_writel_check(struct tegra_dce *d, u32 r, u32 v);

bool dce_io_exists(struct tegra_dce *d);

bool dce_io_valid_reg(struct tegra_dce *d, u32 r);

struct dce_firmware *dce_request_firmware(struct tegra_dce *d,
					  const char *fw_name);

void dce_release_fw(struct tegra_dce *d, struct dce_firmware *fw);

void *dce_kzalloc(struct tegra_dce *d, size_t size,  bool dma_flag);

void dce_kfree(struct tegra_dce *d, void *addr);

unsigned long dce_get_nxt_pow_of_2(unsigned long *addr, u8 nbits);

static inline void dce_bitmap_set(unsigned long *map,
				  unsigned int start, unsigned int len)
{
	bitmap_set(map, start, (int)len);
}

static inline void dce_bitmap_clear(unsigned long *map,
				    unsigned int start, unsigned int len)
{
	bitmap_clear(map, start, (int)len);
}

#endif
