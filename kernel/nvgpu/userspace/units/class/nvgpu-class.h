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

#ifndef UNIT_NVGPU_CLASS_H
#define UNIT_NVGPU_CLASS_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-class
 *  @{
 *
 * Software Unit Test Specification for nvgpu.common.class
 */

/**
 * Test specification for: class_validate
 *
 * Description: Validate common.class unit API.
 *
 * Test Type: Feature, Boundary Values
 *
 * Targets: gops_class.is_valid, gv11b_class_is_valid
 * Equivalence classes:
 * Variable: class_num
 * - Valid : { 0xC3C0U }, { 0xC3B5U }, { 0xC36FU }, { 0xC397U }
 *
 * Targets: gops_class.is_valid_compute, gv11b_class_is_valid_compute,
 * Equivalence classes:
 * Variable: class_num
 * - Valid : { 0xC3C0U }
 *
 * Input: None
 *
 * Steps:
 * - Initialize common.class HAL function pointers.
 * - Validate common.class unit API with below positive/negative data
 *   sets.
 *
 *   - g->ops.gpu_class.is_valid_compute()
 *     Pass data set of supported compute classes and ensure API
 *     returns success in each case.
 *
 *   - g->ops.gpu_class.is_valid_compute()
 *     Pass data set of unsupported compute classes and ensure API
 *     returns failure in each case.
 *
 *   - g->ops.gpu_class.is_valid()
 *     Pass data set of all supported classes and ensure API
 *     returns success in each case.
 *
 *   - g->ops.gpu_class.is_valid()
 *     Pass data set of unsupported classes and ensure API
 *     returns failure in each case.
 *
 * Output:
 * Returns PASS if above validation was performed successfully. FAIL
 * otherwise.
 */
int class_validate_setup(struct unit_module *m, struct gk20a *g, void *args);


#endif /* UNIT_NVGPU_CLASS_H */
