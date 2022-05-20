/*
 * Copyright (c) 2020-2021 NVIDIA CORPORATION.  All rights reserved.
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

#include "hal/class/class_ga10b.h"

#include "class_ga100.h"

bool ga100_class_is_valid(u32 class_num)
{
	bool valid;

	nvgpu_speculation_barrier();

	switch (class_num) {
	case AMPERE_COMPUTE_A:
	case AMPERE_DMA_COPY_A:
#ifdef CONFIG_NVGPU_GRAPHICS
	case AMPERE_A:
#endif
		valid = true;
		break;
	default:
		valid = ga10b_class_is_valid(class_num);
		break;
	}
	return valid;
};

#ifdef CONFIG_NVGPU_GRAPHICS
bool ga100_class_is_valid_gfx(u32 class_num)
{
	bool valid;

	nvgpu_speculation_barrier();

	switch (class_num) {
	case AMPERE_A:
		valid = true;
		break;
	default:
		valid = false;
		break;
	}
	return valid;
}
#endif

bool ga100_class_is_valid_compute(u32 class_num)
{
	bool valid;

	nvgpu_speculation_barrier();

	switch (class_num) {
	case AMPERE_COMPUTE_A:
		valid = true;
		break;
	default:
		valid = ga10b_class_is_valid_compute(class_num);
		break;
	}
	return valid;
}
