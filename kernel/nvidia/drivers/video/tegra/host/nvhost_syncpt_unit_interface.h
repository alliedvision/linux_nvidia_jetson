/*
 * Engine side synchronization support
 *
 * Copyright (c) 2016-2022, NVIDIA Corporation.  All rights reserved.
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

#ifndef NVHOST_SYNCPT_UNIT_INTERFACE_H
#define NVHOST_SYNCPT_UNIT_INTERFACE_H

struct platform_device;
struct nvhost_syncpt;

struct nvhost_syncpt_unit_interface {
	struct scatterlist sg;
	dma_addr_t start;
	uint32_t syncpt_page_size;
};

#endif
