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
 * @addtogroup SWUTS-posix-circbuf
 * @{
 *
 * Software Unit Test Specification for posix-circbuf
 */

#ifndef UNIT_POSIX_CIRCBUF_H
#define UNIT_POSIX_CIRCBUF_H

/**
 * Test specification for test_circbufcnt
 *
 * Description: Test the buffer count implementation.
 *
 * Test Type: Feature
 *
 * Targets: CIRC_CNT
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke CIRC_CNT in loop with head assigned with loop index value and
 *    tail as zero.
 * 2) Check if the return value of buffer count is equal to loop index. Else
 *    return fail.
 * 3) Invoke CIRC_CNT in loop with head assigned as maximum index and tail
 *    decreased according to the loop index value.
 * 4) Check if the return value of buffer count is equal to loop index. Else
 *    return fail.
 * 5) Invoke CIRC_CNT with both head and tail assigned with same value.
 * 6) Check if the return value is equal to zero, else return fail.
 * 7) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of CIRC_CNT returns the
 * expected value as buffer count. Otherwise, test returns FAIL.
 */
int test_circbufcnt(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_circbufspace
 *
 * Description: Test the buffer space check implementation.
 *
 * Test Type: Feature
 *
 * Targets: CIRC_SPACE
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke CIRC_SPACE in loop with head assigned as loop index and
 *    tail as buffer size.
 * 2) Check if the return value of buffer space is equal to the maximum
 *    entries that the buffer can hold minus loop index. Else
 *    return fail.
 * 3) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of CIRC_SPACE returns the
 * expected value as available buffer space. Otherwise, test returns FAIL.
 */
int test_circbufspace(struct unit_module *m, struct gk20a *g, void *args);

#endif /* UNIT_POSIX_CIRCBUF_H */
