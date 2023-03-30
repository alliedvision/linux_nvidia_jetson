/*
 * tegra_isomgr_bw.h
 *
 * Copyright (c) 2016-2020 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_ISOMGR_BW_H__
#define __TEGRA_ISOMGR_BW_H__
void tegra_isomgr_adma_register(struct device *dev);
void tegra_isomgr_adma_unregister(struct device *dev);
void tegra_isomgr_adma_setbw(struct snd_pcm_substream *substream,
			bool is_running);
void tegra_isomgr_adma_renegotiate(void *p, u32 avail_bw);
#endif
