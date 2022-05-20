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

#ifndef UNIT_NVGPU_FUSE_H
#define UNIT_NVGPU_FUSE_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-fuse
 *  @{
 *
 * Software Unit Test Specification for nvgpu-fuse
 */

/**
 * Test specification for: test_fuse_device_common_init
 *
 * Description: Initialization required for before fuse tests for each GPU.
 *
 * Test Type: Other (Setup)
 *
 * Input: struct fuse_test_args passed via args param.
 *
 * Steps:
 * - Setup g struct
 * - Setup fuse ops
 * - Setup mock I/O
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
static int test_fuse_device_common_init(struct unit_module *m,
					struct gk20a *g, void *__args);

/**
 * Test specification for: test_fuse_device_common_init
 *
 * Description: Initialization required for before fuse tests for each GPU.
 *
 * Test Type: Other (Cleanup)
 *
 * Input: struct fuse_test_args passed via args param.
 *
 * Steps:
 * - Remove mock I/O
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
static int test_fuse_device_common_cleanup(struct unit_module *m,
				    struct gk20a *g, void *__args);

#endif /* UNIT_NVGPU_FUSE_H */