/*
 * Copyright (c) 2019 NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/class.h>
#include <nvgpu/barrier.h>

#include "class_gm20b.h"
#include "class_gp10b.h"

bool gp10b_class_is_valid(u32 class_num)
{
	bool valid;

	nvgpu_speculation_barrier();
	switch (class_num) {
	case PASCAL_DMA_COPY_A:
	case PASCAL_CHANNEL_GPFIFO_A:
		valid = true;
		break;
#ifdef CONFIG_NVGPU_GRAPHICS
	case PASCAL_A:
		valid = true;
		break;
#endif
	case PASCAL_COMPUTE_A:
		valid = true;
		break;
	default:
		valid = gm20b_class_is_valid(class_num);
		break;
	}
	return valid;
}

#ifdef CONFIG_NVGPU_GRAPHICS
bool gp10b_class_is_valid_gfx(u32 class_num)
{
	if (class_num == PASCAL_A ||  class_num == MAXWELL_B) {
		return true;
	} else {
		return false;
	}
}
#endif

bool gp10b_class_is_valid_compute(u32 class_num)
{
	if (class_num == PASCAL_COMPUTE_A ||  class_num == MAXWELL_COMPUTE_B) {
		return true;
	} else {
		return false;
	}
}
