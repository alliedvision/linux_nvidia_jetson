/*
 * Copyright (c) 2014-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef INCLUDE_RTCPU_HSP_COMBO_H
#define INCLUDE_RTCPU_HSP_COMBO_H

#include <linux/types.h>

struct camrtc_hsp;
struct device;

struct camrtc_hsp *camrtc_hsp_create(
	struct device *dev,
	void (*group_notify)(struct device *dev, u16 group),
	long cmd_timeout);

void camrtc_hsp_free(struct camrtc_hsp *camhsp);

void camrtc_hsp_group_ring(struct camrtc_hsp *camhsp,
		u16 group);

int camrtc_hsp_sync(struct camrtc_hsp *camhsp);
int camrtc_hsp_resume(struct camrtc_hsp *camhsp);
int camrtc_hsp_suspend(struct camrtc_hsp *camhsp);
int camrtc_hsp_bye(struct camrtc_hsp *camhsp);
int camrtc_hsp_ch_setup(struct camrtc_hsp *camhsp, dma_addr_t iova);
int camrtc_hsp_ping(struct camrtc_hsp *camhsp, u32 data, long timeout);
int camrtc_hsp_get_fw_hash(struct camrtc_hsp *camhsp,
		u8 hash[], size_t hash_size);


#endif	/* INCLUDE_RTCPU_HSP_COMBO_H */
