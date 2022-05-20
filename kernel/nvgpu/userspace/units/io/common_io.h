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

/**
 * @addtogroup SWUTS-io
 * @{
 *
 * Software Unit Test Specification for common_io
 */

#ifndef __UNIT_COMMON_IO_H__
#define __UNIT_COMMON_IO_H__

/**
 * Test specification for test_writel_check
 *
 * Description: Write value v to reg and read it back.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_writel_check
 *
 * Inputs: None
 *
 * Steps:
 * Initialize address space and register io callbacks.
 * Call nvgpu_writel_check with value 0. Here read and write value will be
 * different as read callback always returns NVGPU_READ_VAL.
 * Call nvgpu_writel_check with value NVGPU_READ_VAL. Here read and write value
 * will be same as read callback always returns NVGPU_READ_VAL.
 *
 * Output:
 * The test returns PASS only as target nvgpu_writel_check() always returns
 * void.
 *
 */
int test_writel_check(struct unit_module *m, struct gk20a *g, void *args);

#endif /* __UNIT_COMMON_IO_H__ */
