/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef UNIT_FIFO_RAMIN_GK20A_FUSA_H
#define UNIT_FIFO_RAMIN_GK20A_FUSA_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-ramin-gk20a
 *  @{
 *
 * Software Unit Test Specification for fifo/ramin/gk20a
 */

/**
 * Test specification for: test_gk20a_ramin_base_shift
 *
 * Description: Test gk20a base shift value
 *
 * Test Type: Feature
 *
 * Targets: gops_ramin.base_shift, gk20a_ramin_base_shift
 *
 * Input: None
 *
 * Steps:
 * - Check that instance block shift (in bits) is correct as per hardware
 *   manual. This gives number of zeros in instance block physical address and
 *   thus defines alignment.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_ramin_base_shift(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gk20a_ramin_alloc_size
 *
 * Description: Test gk20a alloc size
 *
 * Test Type: Feature
 *
 * Targets: gops_ramin.alloc_size, gk20a_ramin_alloc_size
 *
 * Input: None
 *
 * Steps:
 * - Check instance block alloc size is correct as per hardware manuals.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_ramin_alloc_size(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * @}
 */

#endif /* UNIT_FIFO_RAMIN_GK20A_FUSA_H */
