/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef NVGPU_INTERRUPTS_H
#define NVGPU_INTERRUPTS_H

#include <nvgpu/types.h>

#define NVGPU_MAX_INTERRUPTS_STALL    0x3U
#define NVGPU_MAX_INTERRUPTS_NONSTALL 0x1U
#define NVGPU_MAX_INTERRUPTS          (NVGPU_MAX_INTERRUPTS_STALL + \
                                       NVGPU_MAX_INTERRUPTS_NONSTALL)

struct nvgpu_interrupts {
	bool enabled;

	u32 stall_size;
	u32 nonstall_size;

	u32 stall_lines[NVGPU_MAX_INTERRUPTS_STALL];
	u32 nonstall_line;    /* FIXME: if multiple nonstall lines are needed */
};

#endif /* NVGPU_INTERRUPTS_H */
