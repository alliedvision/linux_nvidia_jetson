/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/io.h>
#include <nvgpu/io.h>

#include "os_linux.h"

u32 nvgpu_os_readl(uintptr_t addr)
{
	return readl((void __iomem *)addr);
}

void nvgpu_os_writel(u32 v, uintptr_t addr)
{
	writel(v, (void __iomem *)addr);
}

void nvgpu_os_writel_relaxed(u32 v, uintptr_t addr)
{
	writel_relaxed(v, (void __iomem *)addr);
}

uintptr_t nvgpu_io_map(struct gk20a *g, uintptr_t addr, size_t size)
{
	return (uintptr_t)devm_ioremap(dev_from_gk20a(g), addr, size);
}

void nvgpu_io_unmap(struct gk20a *g, uintptr_t addr, size_t size)
{
	devm_iounmap(dev_from_gk20a(g), (void __iomem *)addr);
}
