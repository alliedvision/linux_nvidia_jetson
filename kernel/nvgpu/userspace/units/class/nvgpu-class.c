/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <unit/unit.h>
#include <unit/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/class.h>

#include "hal/class/class_gv11b.h"

#include "nvgpu-class.h"

u32 valid_compute_classes[] = {
	0xC3C0U, /* VOLTA_COMPUTE_A */
};

u32 invalid_compute_classes[] = {
	0xC397U, /* VOLTA_A */
	0xC3B5U, /* VOLTA_DMA_COPY_A */
	0xC36FU, /* VOLTA_CHANNEL_GPFIFO_A */
	0xC0B5U, /* PASCAL_DMA_COPY_A */
	0xC06FU, /* PASCAL_CHANNEL_GPFIFO_A */
	0xB06FU, /* MAXWELL_CHANNEL_GPFIFO_A */
	0xB0B5U, /* MAXWELL_DMA_COPY_A */
	0xA140U, /* KEPLER_INLINE_TO_MEMORY_B */
	0xA0B5U, /* KEPLER_DMA_COPY_A */
	0xC097U, /* PASCAL_A */
	0xC0C0U, /* PASCAL_COMPUTE_A */
	0xB1C0U, /* MAXWELL_COMPUTE_B */
	0xB197U, /* MAXWELL_B */
	0x902DU, /* FERMI_TWOD_A */
	0x1234U, /* random value */
	0x76543210U, /* random value */
	0x0000U, /* BVEC test value */
	0xB000U, /* BVEC test value */
	0xC3BFU, /* BVEC test value */
	0xC3C1U, /* BVEC test value */
	0xD000U, /* BVEC test value */
	0xFFFFFFFFU, /* BVEC test value */
};

u32 valid_classes[] = {
	0xC36FU, /* VOLTA_CHANNEL_GPFIFO_A */
	0xC397U, /* VOLTA_A */
	0xC3B5U, /* VOLTA_DMA_COPY_A */
	0xC3C0U, /* VOLTA_COMPUTE_A */
};

u32 invalid_classes[] = {
	0x1234U, /* random value */
	0xC097U, /* PASCAL_A */
	0xC0C0U, /* PASCAL_COMPUTE_A */
	0xB1C0U, /* MAXWELL_COMPUTE_B */
	0xB197U, /* MAXWELL_B */
	0x902DU, /* FERMI_TWOD_A */
	0xC0B5U, /* PASCAL_DMA_COPY_A */
	0xC06FU, /* PASCAL_CHANNEL_GPFIFO_A */
	0xB06FU, /* MAXWELL_CHANNEL_GPFIFO_A */
	0xB0B5U, /* MAXWELL_DMA_COPY_A */
	0xA140U, /* KEPLER_INLINE_TO_MEMORY_B */
	0xA0B5U, /* KEPLER_DMA_COPY_A */
	0x76543210U, /* random value */
	0x0000U, /* BVEC test value */
	0xB000U, /* BVEC test value */
	0xC36EU, /* BVEC test value */
	0xC370U, /* BVEC test value */
	0xC396U, /* BVEC test value */
	0xC398U, /* BVEC test value */
	0xC3B4U, /* BVEC test value */
	0xC3B6U, /* BVEC test value */
	0xC3BFU, /* BVEC test value */
	0xC3C1U, /* BVEC test value */
	0xD000U, /* BVEC test value */
	0xFFFFFFFFU, /* BVEC test value */
};

int class_validate_setup(struct unit_module *m, struct gk20a *g, void *args)
{
	bool valid = false;
	u32 i;

	/* Init HAL */
	g->ops.gpu_class.is_valid_compute = gv11b_class_is_valid_compute;
	g->ops.gpu_class.is_valid = gv11b_class_is_valid;

	for (i = 0;
	     i < sizeof(valid_compute_classes)/sizeof(u32);
	     i++) {
		valid = g->ops.gpu_class.is_valid_compute(
				valid_compute_classes[i]);
		if (!valid) {
			goto fail;
		}
	}

	for (i = 0;
	     i < sizeof(invalid_compute_classes)/sizeof(u32);
	     i++) {
		valid = g->ops.gpu_class.is_valid_compute(
				invalid_compute_classes[i]);
		if (valid) {
			goto fail;
		}
	}

	for (i = 0;
	     i < sizeof(valid_classes)/sizeof(u32);
	     i++) {
		valid = g->ops.gpu_class.is_valid(valid_classes[i]);
		if (!valid) {
			goto fail;
		}
	}

	for (i = 0;
	     i < sizeof(invalid_classes)/sizeof(u32);
	     i++) {
		valid = g->ops.gpu_class.is_valid(invalid_classes[i]);
		if (valid) {
			goto fail;
		}
	}

	return UNIT_SUCCESS;

fail:
	unit_err(m, "%s: failed to validate class API\n", __func__);
	return UNIT_FAIL;
}

struct unit_module_test class_tests[] = {
	UNIT_TEST(class_validate, class_validate_setup, NULL, 0),
};

UNIT_MODULE(class, class_tests, UNIT_PRIO_NVGPU_TEST);
