/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_POSIX_ATOMIC_H
#define NVGPU_POSIX_ATOMIC_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/utils.h>
#include <nvgpu/cov_whitelist.h>

#include <stdatomic.h>

/*
 * Note: this code uses the GCC builtins to implement atomics.
 */

typedef struct nvgpu_posix_atomic {
	/** 32 bit atomic variable. */
	atomic_int v;
} nvgpu_atomic_t;

typedef struct nvgpu_posix_atomic64 {
	/** 64 bit atomic variable. */
	atomic_long v;
} nvgpu_atomic64_t;

/**
 * POSIX implementation for 32 bit atomic initialization. Defined to be
 * congruent with kernel define names.
 */
#define nvgpu_atomic_init_impl(i)		{ i }

/**
 * POSIX implementation for 64 bit atomic initialization. Defined to be
 * congruent with kernel define names.
 */
#define nvgpu_atomic64_init_impl(i)		{ i }

/*
 * These macros define the common cases to maximize code reuse, especially
 * between the 32bit and 64bit cases. The static inline functions are
 * maintained to provide type checking.
 */

/**
 * @brief Define for atomic set.
 *
 * @param v	Atomic variable to be set.
 * @param i	Value to set in atomic variable.
 */
#define NVGPU_POSIX_ATOMIC_SET(v, i) \
	atomic_store(&((v)->v), (i))

/**
 * @brief Define for atomic read.
 *
 * @param v	Atomic variable to be read.
 */
#define NVGPU_POSIX_ATOMIC_READ(v) \
	atomic_load(&((v)->v))

/**
 * @brief Define for atomic add and return.
 *
 * @param v	Atomic variable.
 * @param i	Value to add.
 *
 * @return Returns \a i added with the current value in atomic variable.
 */
#define NVGPU_POSIX_ATOMIC_ADD_RETURN(v, i)				\
	({								\
		typeof(i) tmp;					\
									\
		NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1,		\
			NVGPU_MISRA(Rule, 10_3), "TID 374")		\
		tmp = (typeof((v)->v))atomic_fetch_add(&((v)->v), (i));	\
		NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))	\
		tmp = __builtin_choose_expr(				\
				IS_SIGNED_LONG_TYPE(i),			\
				(nvgpu_safe_add_s64((s64)(tmp), (s64)(i))),	\
				(nvgpu_safe_add_s32((s32)(tmp), (s32)(i))));	\
		tmp;							\
	})

/**
 * @brief Define for atomic subtract and return.
 *
 * @param v	Atomic variable.
 * @param i	Value to subtract.
 *
 * @return Subtracts \a i from the current value in atomic variable and returns.
 */
#define NVGPU_POSIX_ATOMIC_SUB_RETURN(v, i)				\
	({								\
		typeof(i) tmp;					\
									\
		NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1,		\
			NVGPU_MISRA(Rule, 10_3), "TID 374")        	\
		tmp = (typeof((v)->v))atomic_fetch_sub(&((v)->v), (i));	\
		NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))	\
		tmp = __builtin_choose_expr(				\
				IS_SIGNED_LONG_TYPE(i),			\
				(nvgpu_safe_sub_s64((s64)(tmp), (s64)(i))),	\
				(nvgpu_safe_sub_s32((s32)(tmp), (s32)(i))));	\
		tmp;							\
	})

/**
 * @brief Define for atomic compare exchange.
 *
 * @param v	Atomic variable.
 * @param old	Value to compare.
 * @param new	Value to exchange.
 *
 * @return If \a old matches with the current value in \a v, \a new is written
 * into \a v and value of \a old is returned. If \a old does not match with
 * the current value in \a v then the current value in \a v is returned.
 */
#define NVGPU_POSIX_ATOMIC_CMPXCHG(v, old, new)				\
	({								\
		typeof(old) tmp = (old);				\
									\
		NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1,		\
			NVGPU_MISRA(Rule, 10_3), "TID 374")		\
		(void) atomic_compare_exchange_strong(&((v)->v),	\
					&tmp, (new));			\
		NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))	\
		tmp;							\
	})

/**
 * @brief Define for atomic exchange.
 *
 * @param v	Atomic variable.
 * @param new	Value to exchange.
 *
 * @return Original value in the atomic variable.
 */
#define NVGPU_POSIX_ATOMIC_XCHG(v, new) \
	atomic_exchange(&((v)->v), (new))

/**
 * @brief POSIX implementation of atomic set.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param i [in]	Value to set.
 *
 * Sets the value mentioned by \a i atomically in structure \a v.
 */
static inline void nvgpu_atomic_set_impl(nvgpu_atomic_t *v, int i)
{
	NVGPU_POSIX_ATOMIC_SET(v, i);
}

/**
 * @brief POSIX implementation of atomic read.
 *
 * @param v [in]	Structure holding atomic variable to read.
 *
 * Atomically reads the value from structure \a v.
 *
 * @return Value read from the atomic variable.
 */
static inline int nvgpu_atomic_read_impl(nvgpu_atomic_t *v)
{
	return NVGPU_POSIX_ATOMIC_READ(v);
}

/**
 * @brief POSIX implementation of atomic increment.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically increments the value in structure \a v.
 */

static inline void nvgpu_atomic_inc_impl(nvgpu_atomic_t *v)
{
	(void)NVGPU_POSIX_ATOMIC_ADD_RETURN(v, 1);
}

/**
 * @brief POSIX implementation of atomic increment and return.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically increases the value in structure \a v and returns.
 *
 * @return Increments the value in \a v by 1 and returns.
 */
static inline int nvgpu_atomic_inc_return_impl(nvgpu_atomic_t *v)
{
	return NVGPU_POSIX_ATOMIC_ADD_RETURN(v, 1);
}
/**
 * @brief POSIX implementation of atomic decrement.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically decrement the value in structure \a v.
 */
static inline void nvgpu_atomic_dec_impl(nvgpu_atomic_t *v)
{
	(void)NVGPU_POSIX_ATOMIC_SUB_RETURN(v, 1);
}

/**
 * @brief POSIX implementation of atomic decrement and return.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically decrements the value in structure v and returns.
 *
 * @return Value in \a v is decremented by 1 and returned.
 */
static inline int nvgpu_atomic_dec_return_impl(nvgpu_atomic_t *v)
{
	return NVGPU_POSIX_ATOMIC_SUB_RETURN(v, 1);
}
/**
 * @brief POSIX implementation of atomic compare and exchange.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param old [in]	Value to compare.
 * @param new [in]	Value to store.
 *
 * Reads the value stored in structure \a v, replaces the value with \a new if
 * the read value is equal to \a old.
 *
 * @return If \a old matches the current value in \a v, then the value of \a
 * old is returned else the current value in \a v is returned.
 */
static inline int nvgpu_atomic_cmpxchg_impl(nvgpu_atomic_t *v, int old, int new)
{
	return NVGPU_POSIX_ATOMIC_CMPXCHG(v, old, new);
}
/**
 * @brief POSIX implementation of atomic exchange.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param new [in]	Value to set.
 *
 * Atomically exchanges the value stored in \a v with \a new.
 *
 * @return The original value in atomic variable \a v.
 */
static inline int nvgpu_atomic_xchg_impl(nvgpu_atomic_t *v, int new)
{
	return NVGPU_POSIX_ATOMIC_XCHG(v, new);
}

/**
 * @brief POSIX implementation of atomic increment and test.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically increments the value stored in \a v and returns true if
 * the result is zero. Returns false in other cases.
 *
 * @return Returns TRUE if the value in atomic variable is equal to zero after
 * incrementing, else returns FALSE.
 */
static inline bool nvgpu_atomic_inc_and_test_impl(nvgpu_atomic_t *v)
{
	return NVGPU_POSIX_ATOMIC_ADD_RETURN(v, 1) == 0;
}

/**
 * @brief POSIX implementation of atomic decrement and test.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically decrements the value stored in \a v and returns true if
 * the result is zero. Returns false in other cases.
 *
 * @return Returns TRUE if the value in atomic variable is equal to zero after
 * decrementing, else returns FALSE.
 */
static inline bool nvgpu_atomic_dec_and_test_impl(nvgpu_atomic_t *v)
{
	return NVGPU_POSIX_ATOMIC_SUB_RETURN(v, 1) == 0;
}

/**
 * @brief POSIX implementation of atomic subtract.
 *
 * @param i [in]	Value to set.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically subtracts \a i from the value stored in structure \a v.
 */
static inline void nvgpu_atomic_sub_impl(int i, nvgpu_atomic_t *v)
{
	(void)NVGPU_POSIX_ATOMIC_SUB_RETURN(v, i);
}

/**
 * @brief POSIX implementation of atomic subtract and return.
 *
 * @param i [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically subtracts \a i from the value stored in structure \a v and
 * returns.
 *
 * @return Returns \a v minus \a i.
 */
static inline int nvgpu_atomic_sub_return_impl(int i, nvgpu_atomic_t *v)
{
	return NVGPU_POSIX_ATOMIC_SUB_RETURN(v, i);
}

/**
 * @brief POSIX implementation of atomic subtract and test.
 *
 * @param i [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically subtracts i from the value stored in v and returns true if
 * the result is zero. Returns false in other cases.
 *
 * @return Returns TRUE if the subtraction result is equal to zero, otherwise
 * returns FALSE.
 */
static inline bool nvgpu_atomic_sub_and_test_impl(int i, nvgpu_atomic_t *v)
{
	return NVGPU_POSIX_ATOMIC_SUB_RETURN(v, i) == 0;
}

/**
 * @brief POSIX implementation of atomic add.
 *
 * @param i [in]	Value to add.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically adds \a i to the value stored in structure \a v.
 */
static inline void nvgpu_atomic_add_impl(int i, nvgpu_atomic_t *v)
{
	(void)NVGPU_POSIX_ATOMIC_ADD_RETURN(v, i);
}
/**
 * @brief POSIX implementation of atomic add and return.
 *
 * @param i [in]	Value to add.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically adds \a i to the value in structure \a v and returns.
 *
 * @return \a i added with the current value in \a v.
 */
static inline int nvgpu_atomic_add_return_impl(int i, nvgpu_atomic_t *v)
{
	return NVGPU_POSIX_ATOMIC_ADD_RETURN(v, i);
}

/**
 * @brief POSIX implementation of atomic add unless.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param a [in]	Value to add.
 * @param u [in]	Value to check.
 *
 * Atomically adds \a a to \a v if \a v is not \a u.
 *
 * @return Value in \a v before the operation.
 */
static inline int nvgpu_atomic_add_unless_impl(nvgpu_atomic_t *v, int a, int u)
{
	int old;

	do {
		old = NVGPU_POSIX_ATOMIC_READ(v);
		if (old == (u)) {
			break;
		}
		NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1, \
			NVGPU_MISRA(Rule, 10_3), "TID 374")
	} while (!atomic_compare_exchange_strong(&((v)->v), &old, old + (a)));
		NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))

	return old;
}

/**
 * @brief POSIX implementation of 64 bit atomic set.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param i [in]	Value to set.
 *
 * Atomically sets the 64 bit value \a i in structure \a v.
 */
static inline void nvgpu_atomic64_set_impl(nvgpu_atomic64_t *v, long i)
{
	NVGPU_POSIX_ATOMIC_SET(v, i);
}

/**
 * @brief POSIX implementation of 64 bit atomic read.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically reads the 64 bit value in structure \a v.
 *
 * @return Current value in atomic variable.
 */
static inline long nvgpu_atomic64_read_impl(nvgpu_atomic64_t *v)
{
	return NVGPU_POSIX_ATOMIC_READ(v);
}

/**
 * @brief POSIX implementation of 64 bit atomic add.
 *
 * @param x [in]	Value to add.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically adds the 64 bit value \a x to \a v.
 */
static inline void nvgpu_atomic64_add_impl(long x, nvgpu_atomic64_t *v)
{
	(void)NVGPU_POSIX_ATOMIC_ADD_RETURN(v, x);
}

/**
 * @brief POSIX implementation of 64 bit atomic add and return.
 *
 * @param x [in]	Value to add.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically adds the value \a x with \a v and returns.
 *
 * @return \a x added with \a v is returned.
 */
static inline long nvgpu_atomic64_add_return_impl(long x, nvgpu_atomic64_t *v)
{
	return NVGPU_POSIX_ATOMIC_ADD_RETURN(v, x);
}

/**
 * @brief POSIX implementation of 64 bit atomic add unless.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param a [in]	Value to add.
 * @param u [in]	Value to check.
 *
 * Atomically adds 64 bit value \a a to \a v if \a v is not \a u.
 *
 * @return Value in atomic variable before the operation.
 */
static inline long nvgpu_atomic64_add_unless_impl(nvgpu_atomic64_t *v, long a,
						long u)
{
	long old;

	do {
		old = NVGPU_POSIX_ATOMIC_READ(v);
		if (old == (u)) {
			break;
		}
		NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1, \
			NVGPU_MISRA(Rule, 10_3), "TID 374")
	} while (!atomic_compare_exchange_strong(&((v)->v), &old, old + (a)));
		NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))

	return old;
}

/**
 * @brief POSIX implementation of 64 bit atomic increment.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically increments the value in structure \a v.
 */
static inline void nvgpu_atomic64_inc_impl(nvgpu_atomic64_t *v)
{
	(void)NVGPU_POSIX_ATOMIC_ADD_RETURN(v, 1L);
}

/**
 * @brief POSIX implementation of 64 bit atomic increment and return.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically increments the value in structure \a v and returns.
 *
 * @return Value in the atomic variable incremented by 1.
 */
static inline long nvgpu_atomic64_inc_return_impl(nvgpu_atomic64_t *v)
{
	return NVGPU_POSIX_ATOMIC_ADD_RETURN(v, 1L);
}

/**
 * @brief POSIX implementation of 64 bit atomic increment and test.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically increments the value stored in \a v and test. Returns true if the
 * incremented value is 0 else returns false.
 *
 * @return TRUE if the addition results in zero, otherwise FALSE.
 */
static inline bool nvgpu_atomic64_inc_and_test_impl(nvgpu_atomic64_t *v)
{
	return NVGPU_POSIX_ATOMIC_ADD_RETURN(v, 1L) == 0L;
}

/**
 * @brief POSIX implementation of 64 bit atomic decrement.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically decrements the value in structure \a v.
 */
static inline void nvgpu_atomic64_dec_impl(nvgpu_atomic64_t *v)
{
	(void)NVGPU_POSIX_ATOMIC_SUB_RETURN(v, 1L);
}

/**
 * @brief POSIX implementation of 64 bit atomic decrement and return.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically decrements the value in structure \a v and returns.
 *
 * @return Value in atomic variable decremented by 1.
 */
static inline long nvgpu_atomic64_dec_return_impl(nvgpu_atomic64_t *v)
{
	return NVGPU_POSIX_ATOMIC_SUB_RETURN(v, 1L);
}

/**
 * @brief POSIX implementation of 64 bit atomic decrement and test.
 *
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically decrements the value stored in \a v and test. Returns true if
 * the decremented value is 0 else returns false.
 *
 * @return TRUE if the decremented value is equal to zero, otherwise FALSE.
 */
static inline bool nvgpu_atomic64_dec_and_test_impl(nvgpu_atomic64_t *v)
{
	return NVGPU_POSIX_ATOMIC_SUB_RETURN(v, 1L) == 0;
}

/**
 * @brief POSIX implementation of 64 bit atomic exchange.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param new [in]	Value to set.
 *
 * Atomically exchanges the value \a new with the value in \a v.
 *
 * @return Value in the atomic variable before the exchange operation.
 */
static inline long nvgpu_atomic64_xchg_impl(nvgpu_atomic64_t *v, long new)
{
	return NVGPU_POSIX_ATOMIC_XCHG(v, new);
}

/**
 * @brief POSIX implementation of 64 bit atomic compare and exchange.
 *
 * @param v [in]	Structure holding atomic variable.
 * @param old [in]	Value to compare.
 * @param new [in]	Value to exchange.

 * Reads the value stored in structure \a v, replaces the value with \a new if
 * the read value is equal to \a old.
 *
 * @return Returns the original value in atomic variable.
 */
static inline long nvgpu_atomic64_cmpxchg_impl(nvgpu_atomic64_t *v,
					long old, long new)
{
	return NVGPU_POSIX_ATOMIC_CMPXCHG(v, old, new);
}

/**
 * @brief POSIX implementation of 64 bit atomic subtract.
 *
 * @param x [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically subtracts the 64 bit \a x from structure \a v.
 */
static inline void nvgpu_atomic64_sub_impl(long x, nvgpu_atomic64_t *v)
{
	(void)NVGPU_POSIX_ATOMIC_SUB_RETURN(v, x);
}

/**
 * @brief POSIX implementation of 64 bit atomic sub and return.
 *
 * @param x [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically subtracts \a x from \a v and returns.
 *
 * @return \a x subtracted from the current value in \a v.
 */
static inline long nvgpu_atomic64_sub_return_impl(long x, nvgpu_atomic64_t *v)
{
	return NVGPU_POSIX_ATOMIC_SUB_RETURN(v, x);
}

/**
 * @brief POSIX implementation of 64 bit atomic subtract and test.
 *
 * @param x [in]	Value to subtract.
 * @param v [in]	Structure holding atomic variable.
 *
 * Atomically subtracts \a x from \a v and returns true if the result is 0.
 * Else returns false.
 *
 * @return TRUE if the operation results in zero, otherwise FALSE.
 */
static inline bool nvgpu_atomic64_sub_and_test_impl(long x, nvgpu_atomic64_t *v)
{
	return NVGPU_POSIX_ATOMIC_SUB_RETURN(v, x) == 0;
}

#endif /* NVGPU_POSIX_ATOMIC_H */
