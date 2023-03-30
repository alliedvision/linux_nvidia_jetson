/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_ATOMIC_H
#define NVGPU_ATOMIC_H

#ifdef __KERNEL__
#include <nvgpu/linux/atomic.h>
#else
#include <nvgpu/posix/atomic.h>
#endif

/** Initialize nvgpu atomic structure. */
#define NVGPU_ATOMIC_INIT(i)	nvgpu_atomic_init_impl(i)
/** Initialize 64 bit nvgpu atomic structure. */
#define NVGPU_ATOMIC64_INIT(i)	nvgpu_atomic64_init_impl(i)

/**
 * @brief Set atomic variable.
 *
 * Sets the value \a i atomically in structure \a v. Invokes the standard
 * function \a atomic_store internally with the atomic variable \a v in
 * #nvgpu_atomic_t and \a i as parameters for the atomic operation.
 * Function does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param i [in]	Value to set.
 */
static inline void nvgpu_atomic_set(nvgpu_atomic_t *v, int i)
{
	nvgpu_atomic_set_impl(v, i);
}

/**
 * @brief Read atomic variable.
 *
 * Atomically reads the value from structure \a v. Invokes the standard
 * function \a atomic_load internally with the atomic variable \a v in
 * #nvgpu_atomic_t as parameter for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable to read.
 *
 * @return Value read from the atomic variable.
 */
static inline int nvgpu_atomic_read(nvgpu_atomic_t *v)
{
	return nvgpu_atomic_read_impl(v);
}

/**
 * @brief Increment the atomic variable.
 *
 * Atomically increments the value in structure \a v by 1. Invokes the standard
 * function \a atomic_fetch_add internally with the atomic variable \a v in
 * #nvgpu_atomic_t and 1 as parameters for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 */
static inline void nvgpu_atomic_inc(nvgpu_atomic_t *v)
{
	nvgpu_atomic_inc_impl(v);
}

/**
 * @brief Increment the atomic variable and return.
 *
 * Atomically increases the value in structure \a v by 1 and returns the
 * incremented value. Invokes the standard  function \a atomic_fetch_add
 * internally with the atomic variable \a v in #nvgpu_atomic_t and 1 as the
 * parameters for the atomic operation. Function does not perform any
 * validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Value in atomic variable incremented by 1.
 */
static inline int nvgpu_atomic_inc_return(nvgpu_atomic_t *v)
{
	return nvgpu_atomic_inc_return_impl(v);
}

/**
 * @brief Decrement the atomic variable.
 *
 * Atomically decrements the value in structure \a v by 1. Invokes the standard
 * function \a atomic_fetch_sub internally with the atomic variable \a v in
 * #nvgpu_atomic_t and 1 as parameters for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 */
static inline void nvgpu_atomic_dec(nvgpu_atomic_t *v)
{
	nvgpu_atomic_dec_impl(v);
}

/**
 * @brief Decrement the atomic variable and return.
 *
 * Atomically decrements the value in structure \a v by 1 and returns the
 * decremented value. Invokes the standard function \a atomic_fetch_sub
 * internally with the atomic variable \a v in #nvgpu_atomic_t and 1 as
 * parameters for the atomic operation. Function does not perform any
 * validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Value in atomic variable decremented by 1.
 */
static inline int nvgpu_atomic_dec_return(nvgpu_atomic_t *v)
{
	return nvgpu_atomic_dec_return_impl(v);
}

/**
 * @brief Compare and exchange the value in atomic variable.
 *
 * Reads the value stored in \a v, replace the value with \a new if the read
 * value is equal to \a old. In cases where the current value in \a v is not
 * equal to \a old, this function acts as a read operation of atomic variable.
 * Uses the standard function \a atomic_compare_exchange_strong internally with
 * the atomic variable \a v in #nvgpu_atomic_t, \a old and \a new as parameters
 * for the atomic operation. Function does not perform any validation of the
 * input parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param old [in]	Value to compare.
 * @param new [in]	Value to store.
 *
 * @return Reads the value in \a v before the exchange operation and returns.
 * Read value is returned irrespective of whether the exchange operation is
 * carried out or not.
 */
static inline int nvgpu_atomic_cmpxchg(nvgpu_atomic_t *v, int old, int new)
{
	return nvgpu_atomic_cmpxchg_impl(v, old, new);
}

/**
 * @brief Exchange the value in atomic variable.
 *
 * Atomically exchanges the value stored in \a v with \a new. Invokes the
 * standard function \a atomic_exchange internally with the atomic variable
 * \a v in #nvgpu_atomic_t and \a new as parameters for the atomic operation.
 * Function does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param new [in]	Value to set.
 *
 * @return Value in atomic variable before the operation.
 */
static inline int nvgpu_atomic_xchg(nvgpu_atomic_t *v, int new)
{
	return nvgpu_atomic_xchg_impl(v, new);
}

/**
 * @brief Increment the atomic variable and test for zero.
 *
 * Atomically increments the value stored in \a v by 1 and compare the result
 * with zero. Uses the standard function \a atomic_fetch_add with the atomic
 * variable \a v in #nvgpu_atomic_t and 1 as parameters for the atomic
 * operation. Function does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Boolean value indicating the status of comparison operation done
 * after incrementing the atomic variable.
 *
 * @retval TRUE if the increment operation results in zero.
 * @retval FALSE if the increment operation results in a non-zero value.
 */
static inline bool nvgpu_atomic_inc_and_test(nvgpu_atomic_t *v)
{
	return nvgpu_atomic_inc_and_test_impl(v);
}

/**
 * @brief Decrement the atomic variable and test for zero.
 *
 * Atomically decrements the value stored in \a v by 1 and compare the result
 * with zero. Uses the standard function \a atomic_fetch_sub with the atomic
 * variable \a v in #nvgpu_atomic_t and 1 as parameters for the atomic
 * operation. Function does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Boolean value indicating the status of comparison operation done
 * after decrementing the atomic variable.
 *
 * @retval TRUE if the decrement operation results in zero.
 * @retval FALSE if the decrement operation results in a non-zero value.
 */
static inline bool nvgpu_atomic_dec_and_test(nvgpu_atomic_t *v)
{
	return nvgpu_atomic_dec_and_test_impl(v);
}

/**
 * @brief Subtract integer from atomic variable and test.
 *
 * Atomically subtracts \a i from the value stored in \a v and compare the
 * result with zero. Uses the standard function \a atomic_fetch_sub with the
 * atomic variable \a v in #nvgpu_atomic_t and \a i as parameters for the
 * atomic operation. Function does not perform any validation of the parameters.
 *
 * @param i [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Boolean value indicating the status of comparison operation done
 * after the subtraction operation on atomic variable.
 *
 * @retval TRUE if the operation results in zero.
 * @retval FALSE if the operation results in a non-zero value.
 */
static inline bool nvgpu_atomic_sub_and_test(int i, nvgpu_atomic_t *v)
{
	return nvgpu_atomic_sub_and_test_impl(i, v);
}

/**
 * @brief Add integer to atomic variable.
 *
 * Atomically adds \a i to the value stored in structure \a v. Uses the
 * standard function \a atomic_fetch_add with the atomic variable \a v in
 * #nvgpu_atomic_t and \a i as parameters for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param i [in]	Value to add.
 * @param v [in]	Structure holding atomic variable.
 */
static inline void nvgpu_atomic_add(int i, nvgpu_atomic_t *v)
{
	nvgpu_atomic_add_impl(i, v);
}

/**
 * @brief Subtract integer form atomic variable and return.
 *
 * Atomically subtracts \a i from the value stored in structure \a v and
 * returns. Uses the standard function \a atomic_fetch_sub with the
 * atomic variable \a v in #nvgpu_atomic_t and \a i as parameters for the
 * atomic operation. Function does not perform any validation of the parameters.
 *
 * @param i [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 *
 * @return \a i subtracted from \a v is returned.
 */
static inline int nvgpu_atomic_sub_return(int i, nvgpu_atomic_t *v)
{
	return nvgpu_atomic_sub_return_impl(i, v);
}

/**
 * @brief Subtract integer from atomic variable.
 *
 * Atomically subtracts \a i from the value stored in structure \a v. Uses the
 * standard function \a atomic_fetch_sub with the atomic variable \a v in
 * #nvgpu_atomic_t and \a i as parameters for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param i [in]	Value to set.
 * @param v [in]	Structure holding atomic variable.
 */
static inline void nvgpu_atomic_sub(int i, nvgpu_atomic_t *v)
{
	nvgpu_atomic_sub_impl(i, v);
}

/**
 * @brief Add integer to atomic variable and return.
 *
 * Atomically adds \a i to the value in structure \a v and returns. Uses the
 * standard function \a atomic_fetch_add with the atomic variable \a v in
 * #nvgpu_atomic_t and \a i as parameters for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param i [in]	Value to add.
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Current value in \a v added with \a i is returned.
 */
static inline int nvgpu_atomic_add_return(int i, nvgpu_atomic_t *v)
{
	return nvgpu_atomic_add_return_impl(i, v);
}

/**
 * @brief Add integer to atomic variable on unless condition.
 *
 * Atomically adds \a a to \a v if \a v is not \a u. Uses the standard function
 * \a atomic_load with atomic variable \a v in nvgpu_atomic_t as parameter to
 * load the current value of the variable. If the current value of the atomic
 * variable is not equal to \a u, Use the standard atomic function
 * \a atomic_compare_exchange_strong with the atomic variable \a v in
 * #nvgpu_atomic_t, the current value in the variable and the new value to be
 * populated in the variable as parameters. Function does not perform any
 * validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param a [in]	Value to add.
 * @param u [in]	Value to check.
 *
 * @return Value in the atomic variable before the operation.
 */
static inline int nvgpu_atomic_add_unless(nvgpu_atomic_t *v, int a, int u)
{
	return nvgpu_atomic_add_unless_impl(v, a, u);
}

/**
 * @brief Set the 64 bit atomic variable.
 *
 * Atomically sets the 64 bit value \a x in structure \a v. Invokes the standard
 * function \a atomic_store with the atomic variable \a v in #nvgpu_atomic64_t
 * and \a x as parameters internally for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param x [in]	Value to set.
 */
static inline void nvgpu_atomic64_set(nvgpu_atomic64_t *v, long x)
{
	return  nvgpu_atomic64_set_impl(v, x);
}

/**
 * @brief Read the 64 bit atomic variable.
 *
 * Atomically reads the 64 bit value in structure \a v. Invokes the standard
 * function \a atomic_load internally with the atomic variable \a v in
 * #nvgpu_atomic64_t as parameter for the atomic operation. Function does not
 * perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Value read from the atomic variable.
 */
static inline long nvgpu_atomic64_read(nvgpu_atomic64_t *v)
{
	return  nvgpu_atomic64_read_impl(v);
}

/**
 * @brief Addition of 64 bit integer to atomic variable.
 *
 * Atomically adds the 64 bit value \a x to \a v. Uses the standard function
 * \a atomic_fetch_add with the atomic variable \a v in #nvgpu_atomic64_t and
 * \a x as parameters for the atomic operation. Function does not perform any
 * validation of the parameters.
 *
 * @param x [in]	Value to add.
 * @param v [in]	Structure holding atomic variable.
 */
static inline void nvgpu_atomic64_add(long x, nvgpu_atomic64_t *v)
{
	nvgpu_atomic64_add_impl(x, v);
}

/**
 * @brief Increment the 64 bit atomic variable.
 *
 * Atomically increments the value in structure \a v by 1. Invokes the standard
 * function \a atomic_fetch_add internally with the atomic variable \a v in
 * #nvgpu_atomic64_t and 1 as parameters for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 */
static inline void nvgpu_atomic64_inc(nvgpu_atomic64_t *v)
{
	nvgpu_atomic64_inc_impl(v);
}

/**
 * @brief  Increment the 64 bit atomic variable and return.
 *
 * Atomically increases the value in structure \a v by 1 and returns the
 * incremented value. Invokes the standard  function \a atomic_fetch_add
 * internally with the atomic variable \a v in #nvgpu_atomic64_t and 1 as the
 * parameters for the atomic operation. Function does not perform any
 * validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Value in atomic variable incremented by 1.
 */
static inline long nvgpu_atomic64_inc_return(nvgpu_atomic64_t *v)
{
	return nvgpu_atomic64_inc_return_impl(v);
}

/**
 * @brief Decrement the 64 bit atomic variable.
 *
 * Atomically decrements the value in structure \a v by 1.
 * Atomically decrements the value in structure \a v by 1. Invokes the standard
 * function \a atomic_fetch_sub internally with the atomic variable \a v in
 * #nvgpu_atomic64_t and 1 as parameters for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 */
static inline void nvgpu_atomic64_dec(nvgpu_atomic64_t *v)
{
	nvgpu_atomic64_dec_impl(v);
}

/**
 * @brief Decrement the 64 bit atomic variable and return.
 *
 * Atomically decrements the value in structure \a v by 1 and returns the.
 * decremented value. Invokes the standard  function \a atomic_fetch_sub
 * internally with the atomic variable \a v in #nvgpu_atomic64_t and 1 as
 * parameters for the atomic operation. Function does not perform any
 * validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Value in atomic variable decremented by 1.
 */
static inline long nvgpu_atomic64_dec_return(nvgpu_atomic64_t *v)
{
	return nvgpu_atomic64_dec_return_impl(v);
}

/**
 * @brief Exchange the 64 bit atomic variable value.
 *
 * Atomically exchanges the value \a new with the value in \a v. Invokes the
 * standard function \a atomic_exchange internally with the atomic variable
 * \a v in #nvgpu_atomic64_t and \a new as parameters for the atomic operation.
 * Function does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param new [in]	Value to set.
 *
 * @return Value in the atomic variable before the exchange operation.
 */
static inline long nvgpu_atomic64_xchg(nvgpu_atomic64_t *v, long new)
{
	return nvgpu_atomic64_xchg_impl(v, new);
}

/**
 * @brief Compare and exchange the 64 bit atomic variable value.
 *
 * Reads the value stored in \a v, replace the value with \a new if the read
 * value is equal to \a old. In cases where the current value in \a v is not
 * equal to \a old, this function acts as a read operation of atomic variable.
 * Uses the standard function \a atomic_compare_exchange_strong internally with
 * the atomic variable \a v in #nvgpu_atomic64_t, \a old and \a new as parameters
 * for the atomic operation. Function does not perform any validation of the
 * input parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param old [in]	Value to compare.
 * @param new [in]	Value to exchange.
 *
 * @return Returns the original value in atomic variable.
 */
static inline long nvgpu_atomic64_cmpxchg(nvgpu_atomic64_t *v, long old,
					long new)
{
	return nvgpu_atomic64_cmpxchg_impl(v, old, new);
}

/**
 * @brief Addition of 64 bit integer to atomic variable and return.
 *
 * Atomically add the value \a x with \a v and returns. Uses the
 * standard function \a atomic_fetch_add with the atomic variable \a v in
 * #nvgpu_atomic64_t and \a x as parameters for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param x [in]	Value to add.
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Current value in atomic variable added with \a x.
 */
static inline long nvgpu_atomic64_add_return(long x, nvgpu_atomic64_t *v)
{
	return nvgpu_atomic64_add_return_impl(x, v);
}

/**
 * @brief Addition of 64 bit atomic variable on unless condition.
 *
 * Atomically adds 64 bit value \a a to \a v if \a v is not \a u. Uses the
 * standard function \a atomic_load with atomic variable \a v in nvgpu_atomic_t
 * as parameter to load the current value of the variable. If the current value
 * of the atomic variable is not equal to \a u, Use the standard atomic function
 * \a atomic_compare_exchange_strong with the atomic variable \a v in
 * #nvgpu_atomic64_t, the current value in the variable and the new value to be
 * populated in the variable as parameters. Function does not perform any
 * validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param a [in]	Value to add.
 * @param u [in]	Value to check.
 *
 * @return Value in atomic variable before the operation.
 */
static inline long nvgpu_atomic64_add_unless(nvgpu_atomic64_t *v, long a,
						long u)
{
	return nvgpu_atomic64_add_unless_impl(v, a, u);
}

/**
 * @brief Subtraction of 64 bit integer from atomic variable.
 *
 * Atomically subtracts the 64 bit value \a x from structure \a v. Uses the
 * standard function \a atomic_fetch_sub with the atomic variable \a v in
 * #nvgpu_atomic64_t and \a x as parameters for the atomic operation. Function
 * does not perform any validation of the parameters.
 *
 * @param x [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 */
static inline void nvgpu_atomic64_sub(long x, nvgpu_atomic64_t *v)
{
	nvgpu_atomic64_sub_impl(x, v);
}

/**
 * @brief Increment the 64 bit atomic variable and test.
 *
 * Atomically increments the value stored in \a v and compare the result with
 * zero. Uses the standard function \a atomic_fetch_add with the atomic
 * variable \a v in #nvgpu_atomic64_t and 1 as parameters for the atomic
 * operation. Function does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Boolean value indicating the status of comparison operation done
 * after incrementing the atomic variable.
 *
 * @retval TRUE if the operation results in zero.
 * @retval FALSE if the operation results in a non-zero value.
 */
static inline bool nvgpu_atomic64_inc_and_test(nvgpu_atomic64_t *v)
{
	return nvgpu_atomic64_inc_and_test_impl(v);
}

/**
 * @brief Decrement the 64 bit atomic variable and test.
 *
 * Atomically decrements the value stored in \a v and compare the result with
 * zero. Uses the standard function \a atomic_fetch_sub with the atomic
 * variable \a v in #nvgpu_atomic64_t and 1 as parameters for the atomic
 * operation. Function does not perform any validation of the parameters.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Boolean value indicating the status of comparison operation done
 * after decrementing the atomic variable.
 *
 * @retval TRUE if the operation results in zero.
 * @retval FALSE if the operation results in a non-zero value.
 */
static inline bool nvgpu_atomic64_dec_and_test(nvgpu_atomic64_t *v)
{
	return nvgpu_atomic64_dec_and_test_impl(v);
}

/**
 * @brief Subtract 64 bit integer from atomic variable and test.
 *
 * Atomically subtracts \a x from \a v and compare the result with zero.
 * Uses the standard function \a atomic_fetch_sub with the atomic
 * variable \a v in #nvgpu_atomic64_t and \a x as parameters for the atomic
 * operation. Function does not perform any validation of the parameters.
 *
 * @param x [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 *
 * @return Boolean value indicating the status of comparison operation done
 * after the subtraction operation on atomic variable.
 *
 * @retval TRUE if the operation results in zero.
 * @retval FALSE if the operation results in a non-zero value.
 */
static inline bool nvgpu_atomic64_sub_and_test(long x, nvgpu_atomic64_t *v)
{
	return nvgpu_atomic64_sub_and_test_impl(x, v);
}

/**
 * @brief Subtract 64 bit integer from atomic variable and return.
 *
 * Atomically subtracts \a x from \a v and returns the subtracted value.
 * Uses the standard function \a atomic_fetch_sub with the atomic
 * variable \a v in #nvgpu_atomic64_t and \a x as parameters for the atomic
 * operation. Function does not perform any validation of the parameters.
 *
 * @param x [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 *
 * @return \a x subtracted from the current value in atomic variable.
 */
static inline long nvgpu_atomic64_sub_return(long x, nvgpu_atomic64_t *v)
{
	return nvgpu_atomic64_sub_return_impl(x, v);
}

#endif /* NVGPU_ATOMIC_H */
