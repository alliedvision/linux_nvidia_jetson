/*
 * Tegra Graphics Host Syncpoint Integration to dma_fence/sync_file framework
 *
 * Copyright (c) 2013-2020, NVIDIA Corporation.  All rights reserved.
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

#ifndef __NVHOST_SYNC_V2_H
#define __NVHOST_SYNC_V2_H

struct dma_fence *nvhost_dma_fence_create(
	struct nvhost_syncpt *syncpt, struct nvhost_ctrl_sync_fence_info *pts,
	int num_pts);

#endif /* __NVHOST_SYNC_V2_H */
