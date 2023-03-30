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

#ifndef UNIT_INTERFACE_ATOMIC_H
#define UNIT_INTERFACE_ATOMIC_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-atomic
 *  @{
 *
 * Software Unit Test Specification for interface-atomic
 */

/**
 * Test specification for: test_atomic_set_and_read
 *
 * Description: Test atomic set and read operations.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_atomic_set, nvgpu_atomic64_set,
 *          nvgpu_atomic_read, nvgpu_atomic64_read
 *
 * Input: struct atomic_test_args passed via the __args parameter.
 *
 * Steps:
 * - Set the limit values for each atomic's size and read back to verify.
 * - Loop through setting each bit in the atomic, reading each time to verify.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_atomic_set_and_read(struct unit_module *m,
				struct gk20a *g, void *__args);

/**
 * Test specification for: test_atomic_arithmetic
 *
 * Description: Test arithemtic atomic operations inc, dec, add, sub and friends
 *              (except add_unless) single threaded for proper functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_atomic_inc, nvgpu_atomic64_inc,
 *          nvgpu_atomic_inc_return, nvgpu_atomic64_inc_return,
 *          nvgpu_atomic_inc_and_test, nvgpu_atomic64_inc_and_test,
 *          nvgpu_atomic_dec, nvgpu_atomic64_dec,
 *          nvgpu_atomic_dec_return, nvgpu_atomic64_dec_return,
 *          nvgpu_atomic_dec_and_test, nvgpu_atomic64_dec_and_test,
 *          nvgpu_atomic_add, nvgpu_atomic64_add,
 *          nvgpu_atomic_add_return, nvgpu_atomic64_add_return,
 *          nvgpu_atomic_sub, nvgpu_atomic64_sub,
 *          nvgpu_atomic_sub_return, nvgpu_atomic64_sub_return,
 *          nvgpu_atomic_sub_and_test, nvgpu_atomic64_sub_and_test,
 *          nvgpu_atomic_read, nvgpu_atomic64_read,
 *          nvgpu_atomic_set, nvgpu_atomic64_set
 *
 * Input: struct atomic_test_args passed via the __args parameter.
 *        For *_and_test ops, the args should make sure the loop traverses
 *        across 0 to test the "test" part.
 *
 * Steps:
 * - Sets a start value from args.
 * - Loops (iterations per args param).
 * - Validates final result.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_atomic_arithmetic(struct unit_module *m,
				struct gk20a *g, void *__args);

/**
 * Test specification for: test_atomic_arithmetic_threaded
 *
 * Description: Test atomic operations inc, dec, add, sub, cmpxchg, in threads
 *              to verify atomicity.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_atomic_cmpxchg, nvgpu_atomic64_cmpxchg,
 *          nvgpu_atomic_inc, nvgpu_atomic64_inc,
 *          nvgpu_atomic_inc_and_test, nvgpu_atomic64_inc_and_test,
 *          nvgpu_atomic_dec, nvgpu_atomic64_dec,
 *          nvgpu_atomic_dec_and_test, nvgpu_atomic64_dec_and_test,
 *          nvgpu_atomic_add, nvgpu_atomic64_add,
 *          nvgpu_atomic_add_return, nvgpu_atomic64_add_return,
 *          nvgpu_atomic_sub, nvgpu_atomic64_sub,
 *          nvgpu_atomic_sub_and_test, nvgpu_atomic64_sub_and_test,
 *          nvgpu_atomic_read, nvgpu_atomic64_read,
 *          nvgpu_atomic_set, nvgpu_atomic64_set,
 *          nvgpu_atomic_add_unless, nvgpu_atomic64_add_unless
 *
 * Input: struct atomic_test_args passed via the __args parameter.
 *
 * Steps:
 * - Sets initial start value.
 * - Kicks off threads to loop running ops.
 * - When threads finish loops, verify values.
 * - With the ops that have a return, save the final value for each thread and
 *   use that to try to ensure that the threads aren't executing sequentially.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_atomic_arithmetic_threaded(struct unit_module *m,
					struct gk20a *g, void *__args);

/**
 * Test specification for: test_atomic_arithmetic_and_test_threaded
 *
 * Description: Test arithmetic *_and_test functions in threads to verify
 *              atomicity.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_atomic_inc_and_test, nvgpu_atomic64_inc_and_test,
 *          nvgpu_atomic_dec_and_test, nvgpu_atomic64_dec_and_test,
 *          nvgpu_atomic_sub_and_test, nvgpu_atomic64_sub_and_test,
 *          nvgpu_atomic_read, nvgpu_atomic64_read,
 *          nvgpu_atomic_set, nvgpu_atomic64_set
 *
 * Input: struct atomic_test_args passed via the __args parameter.
 *
 * Steps:
 * - Set the atomic to a value to allow the arithmetic op to pass 0.
 * - Start a lot of threads that will each execute the atomic op many times to
 *   ensure concurrency.
 * - If the atomic op reports the value is zero, this iteration is recorded.
 * - Check iteration count to make sure only 0 was observed exactly once.
 * - Repeat above steps until reaching the input argument repeat_count or seeing
 *   a failure.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_atomic_arithmetic_and_test_threaded(struct unit_module *m,
						struct gk20a *g, void *__args);

/**
 * Test specification for: test_atomic_xchg
 *
 * Description: Test xchg op single threaded for proper functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_atomic_xchg, nvgpu_atomic64_xchg,
 *          nvgpu_atomic_set, nvgpu_atomic64_set,
 *          nvgpu_atomic_read, nvgpu_atomic64_read
 *
 * Input: struct atomic_test_args passed via the __args parameter.
 *
 * Steps:
 * - Loops calling xchg op with different values making sure the returned
 *   value is the last one written.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_atomic_xchg(struct unit_module *m,
			struct gk20a *g, void *__args);

/**
 * Test specification for: test_atomic_xchg_threaded
 *
 * Description: Test atomic exchange operation with threads to test atomicity.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_atomic_xchg, nvgpu_atomic64_xchg,
 *          nvgpu_atomic_set, nvgpu_atomic64_set,
 *          nvgpu_atomic_read, nvgpu_atomic64_read
 *
 * Input: struct atomic_test_args passed via the __args parameter.
 *
 * Steps:
 * - Set the atomic to a starting value.
 * - Setup and start the exchange threads.
 *   - Setup includes setting each thread's "xchg_val" to its thread number.
 * - When threads complete, loop through the thread's xchg_val and make sure
 *   each number is unique and someone still has the starting value.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_atomic_xchg_threaded(struct unit_module *m,
				struct gk20a *g, void *__args);

/**
 * Test specification for: test_atomic_cmpxchg
 *
 * Description: Test cmpxchg single threaded for proper functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_atomic_cmpxchg, nvgpu_atomic64_cmpxchg,
 *          nvgpu_atomic_set, nvgpu_atomic64_set,
 *          nvgpu_atomic_read, nvgpu_atomic64_read
 *
 * Input: struct atomic_test_args passed via the __args parameter.
 *
 * Steps:
 * - Loop calling cmpxchg. Alternating between matching and not matching.
 * - Verify correct behavior for each call to the operation.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_atomic_cmpxchg(struct unit_module *m,
			struct gk20a *g, void *__args);

/**
 * Test specification for: test_atomic_add_unless
 *
 * Description: Test add_unless op single threaded for proper functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_atomic_add_unless, nvgpu_atomic64_add_unless
 *
 * Input: struct atomic_test_args passed via the __args parameter.
 *
 * Steps:
 * - Loop through calling the operation. Alternating whether the add should
 *   occur or not (i.e. changing the "unless" value).
 * - Verify correct behavior for each operation.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_atomic_add_unless(struct unit_module *m,
				struct gk20a *g, void *__args);

#endif /* UNIT_INTERFACE_ATOMIC_H */
