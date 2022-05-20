/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_CYCLESTATS_PRIV_H
#define NVGPU_CYCLESTATS_PRIV_H

#include <nvgpu/types.h>

#define MULTICHAR_TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

enum BAR0_DEBUG_OPERATION {
	BARO_ZERO_NOP = 0,
	OP_END = MULTICHAR_TAG('D', 'O', 'N', 'E'),
	BAR0_READ32 = MULTICHAR_TAG('0', 'R', '3', '2'),
	BAR0_WRITE32 = MULTICHAR_TAG('0', 'W', '3', '2'),
};

struct share_buffer_head {
	enum BAR0_DEBUG_OPERATION operation;
/* size of the operation item */
	u32 size;
	u32 completed;
	u32 failed;
	u64 context;
	u64 completion_callback;
};

struct nvgpu_cyclestate_buffer_elem {
	struct share_buffer_head	head;
/* in */
	u64 p_data;
	u64 p_done;
	u32 offset_bar0;
	u16 first_bit;
	u16 last_bit;
/* out */
/* keep 64 bits to be consistent */
	u64 data;
};

#endif /* NVGPU_CYCLESTATS_PRIV_H */
