/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_POSIX_MOCK_REGS_H
#define NVGPU_POSIX_MOCK_REGS_H

#include <nvgpu/types.h>

struct gk20a;

struct nvgpu_mock_iospace {
	u32             base;
	size_t          size;
	const uint32_t *data;
};

#define MOCK_REGS_GR		0U
#define MOCK_REGS_FUSE		1U
#define MOCK_REGS_MASTER	2U
#define MOCK_REGS_TOP		3U
#define MOCK_REGS_FIFO		4U
#define MOCK_REGS_PRI		5U
#define MOCK_REGS_PBDMA		6U
#define MOCK_REGS_CCSR		7U
#define MOCK_REGS_USERMODE	8U
#define MOCK_REGS_CE		9U
#define MOCK_REGS_PBUS		10U
#define MOCK_REGS_HSHUB		11U
#define MOCK_REGS_FB		12U
#define MOCK_REGS_LAST		13U

/**
 * Load a mocked register list into the passed IO space description.
 */
int nvgpu_get_mock_reglist(struct gk20a *g, u32 reg_space,
			   struct nvgpu_mock_iospace *iospace);

#endif
