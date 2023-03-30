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

#ifndef UNIT_NVGPU_GK20A_H
#define UNIT_NVGPU_GK20A_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-nvgpu-gk20a
 *  @{
 *
 * Software Unit Test Specification for common.nvgpu.gk20a
 */

/**
 * Test specification for: test_get_poll_timeout
 *
 * Description: Verify functionality nvgpu_get_poll_timeout().
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_get_poll_timeout, nvgpu_is_timeouts_enabled
 *
 * Input: None
 *
 * Steps:
 * - Call nvgpu_get_poll_timeout and verify the correct value is returned.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_get_poll_timeout(struct unit_module *m, struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_GK20A_H */
