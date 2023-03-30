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

#include <stdlib.h> /* for abs() */
#include <unit/unit.h>
#include <unit/io.h>
#include <unit/unit-requirement-ids.h>

#include <nvgpu/atomic.h>
#include <nvgpu/bug.h>

#include "atomic.h"

struct atomic_struct {
	long not_atomic;
	nvgpu_atomic_t atomic;
	nvgpu_atomic64_t atomic64;
};
enum atomic_type {
	ATOMIC_32,
	ATOMIC_64,
	NOT_ATOMIC,
};
enum atomic_op {
	op_inc,
	op_dec,
	op_add,
	op_sub,
	op_inc_and_test,
	op_dec_and_test,
	op_sub_and_test,
	op_add_unless,
	op_cmpxchg,
};
struct atomic_test_args {
	enum atomic_op op;
	enum atomic_type type;
	long start_val;
	unsigned long loop_count;
	unsigned long value; /* absolute value */
	unsigned int repeat_count; /* This sets how many times to repeat a test
				    * Only applies to threaded tests
				    */
};
struct atomic_thread_info {
	struct atomic_struct *atomic;
	struct atomic_test_args *margs;
	pthread_t thread;
	unsigned int thread_num;
	unsigned int iterations;
	long final_val;
	long final_expected_val;
	long xchg_val;
	long unless;
};

static pthread_barrier_t thread_barrier;
bool stop_threads;

/*
 * Define functions for atomic ops that handle all types so we can
 * keep the code cleaner.
 */
static inline void func_set(enum atomic_type type, struct atomic_struct *ref,
			  long val)
{
	switch (type) {
	case NOT_ATOMIC:
		ref->not_atomic = val;
		break;
	case ATOMIC_32:
		nvgpu_atomic_set(&(ref->atomic), val);
		break;
	case ATOMIC_64:
		nvgpu_atomic64_set(&(ref->atomic64), val);
		break;
	}
}

static inline long func_read(enum atomic_type type, struct atomic_struct *ref)
{
	long ret = 0;

	switch (type) {
	case NOT_ATOMIC:
		ret = ref->not_atomic;
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_read(&(ref->atomic));
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_read(&(ref->atomic64));
		break;
	}
	return ret;
}

static inline void func_inc(enum atomic_type type, struct atomic_struct *ref)
{
	switch (type) {
	case NOT_ATOMIC:
		++ref->not_atomic;
		break;
	case ATOMIC_32:
		nvgpu_atomic_inc(&(ref->atomic));
		break;
	case ATOMIC_64:
		nvgpu_atomic64_inc(&(ref->atomic64));
		break;
	}
}

static inline long func_inc_return(enum atomic_type type,
				   struct atomic_struct *ref)
{
	long ret = 0;

	switch (type) {
	case NOT_ATOMIC:
		++ref->not_atomic;
		ret = ref->not_atomic;
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_inc_return(&(ref->atomic));
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_inc_return(&(ref->atomic64));
		break;
	}
	return ret;
}

static inline bool func_inc_and_test(enum atomic_type type,
				     struct atomic_struct *ref)
{
	bool ret = false;

	switch (type) {
	case NOT_ATOMIC:
		++ref->not_atomic;
		ret = (ref->not_atomic == 0);
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_inc_and_test(&(ref->atomic));
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_inc_and_test(&(ref->atomic64));
		break;
	}
	return ret;
}

static inline void func_dec(enum atomic_type type, struct atomic_struct *ref)
{
	switch (type) {
	case NOT_ATOMIC:
		--ref->not_atomic;
		break;
	case ATOMIC_32:
		nvgpu_atomic_dec(&(ref->atomic));
		break;
	case ATOMIC_64:
		nvgpu_atomic64_dec(&(ref->atomic64));
		break;
	}
}

static inline long func_dec_return(enum atomic_type type,
				   struct atomic_struct *ref)
{
	long ret = 0;

	switch (type) {
	case NOT_ATOMIC:
		--ref->not_atomic;
		ret = ref->not_atomic;
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_dec_return(&(ref->atomic));
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_dec_return(&(ref->atomic64));
		break;
	}
	return ret;
}

static inline bool func_dec_and_test(enum atomic_type type,
				     struct atomic_struct *ref)
{
	bool ret = false;

	switch (type) {
	case NOT_ATOMIC:
		--ref->not_atomic;
		ret = (ref->not_atomic == 0);
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_dec_and_test(&(ref->atomic));
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_dec_and_test(&(ref->atomic64));
		break;
	}
	return ret;
}

static inline void func_add(enum atomic_type type, long val,
			    struct atomic_struct *ref)
{
	switch (type) {
	case NOT_ATOMIC:
		ref->not_atomic += val;
		break;
	case ATOMIC_32:
		nvgpu_atomic_add(val, &(ref->atomic));
		break;
	case ATOMIC_64:
		nvgpu_atomic64_add(val, &(ref->atomic64));
		break;
	}
}

static inline long func_add_return(enum atomic_type type, long val,
				   struct atomic_struct *ref)
{
	long ret = 0;

	switch (type) {
	case NOT_ATOMIC:
		ref->not_atomic += val;
		ret = ref->not_atomic;
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_add_return(val, &(ref->atomic));
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_add_return(val, &(ref->atomic64));
		break;
	}
	return ret;
}

static inline long func_add_unless(enum atomic_type type,
				   struct atomic_struct *ref, long val,
				   long unless)
{
	long ret = 0;

	switch (type) {
	case NOT_ATOMIC:
		ret = ref->not_atomic;
		if (ret != unless) {
			ref->not_atomic += val;
		}
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_add_unless(&(ref->atomic), val,
						unless);
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_add_unless(&(ref->atomic64), val,
						unless);
		break;
	}
	return ret;
}

static inline void func_sub(enum atomic_type type, long val,
			    struct atomic_struct *ref)
{
	switch (type) {
	case NOT_ATOMIC:
		ref->not_atomic -= val;
		break;
	case ATOMIC_32:
		nvgpu_atomic_sub(val, &(ref->atomic));
		break;
	case ATOMIC_64:
		nvgpu_atomic64_sub(val, &(ref->atomic64));
		break;
	}
}

static inline long func_sub_return(enum atomic_type type, long val,
				   struct atomic_struct *ref)
{
	long ret = 0;

	switch (type) {
	case NOT_ATOMIC:
		ref->not_atomic -= val;
		ret = ref->not_atomic;
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_sub_return(val, &(ref->atomic));
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_sub_return(val, &(ref->atomic64));
		break;
	}
	return ret;
}

static inline bool func_sub_and_test(enum atomic_type type, long val,
				     struct atomic_struct *ref)
{
	bool ret = 0;

	switch (type) {
	case NOT_ATOMIC:
		ref->not_atomic -= val;
		ret = (ref->not_atomic == 0);
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_sub_and_test(val, &(ref->atomic));
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_sub_and_test(val,
							&(ref->atomic64));
		break;
	}
	return ret;
}

static inline long func_xchg(enum atomic_type type, struct atomic_struct *ref,
			     long new)
{
	long ret = 0;

	switch (type) {
	case NOT_ATOMIC:
		ret = ref->not_atomic;
		ref->not_atomic = new;
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_xchg(&(ref->atomic), new);
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_xchg(&(ref->atomic64), new);
		break;
	}
	return ret;
}

static inline long func_cmpxchg(enum atomic_type type,
				struct atomic_struct *ref, long old, long new)
{
	long ret = 0;

	switch (type) {
	case NOT_ATOMIC:
		ret = ref->not_atomic;
		if (ret == old) {
			ref->not_atomic = new;
		}
		break;
	case ATOMIC_32:
		ret = nvgpu_atomic_cmpxchg(&(ref->atomic), old, new);
		break;
	case ATOMIC_64:
		ret = nvgpu_atomic64_cmpxchg(&(ref->atomic64), old,
						new);
		break;
	}
	return ret;
}

/*
 * Helper macro that takes an atomic op from the enum and returns +1/-1
 * to help doing arithemtic.
 */
#define ATOMIC_OP_SIGN(atomic_op)					\
	({								\
		long sign;						\
		switch (atomic_op) {					\
			case op_dec:					\
			case op_sub:					\
			case op_dec_and_test:				\
			case op_sub_and_test:				\
				sign = -1;				\
				break;					\
			default:					\
				sign = 1;				\
		}							\
		sign;							\
	})

/* For the non-atomic case, we usually have to invert success/failure */
#define INVERTED_RESULT(result) \
	(((result) == UNIT_FAIL) ? UNIT_SUCCESS : UNIT_FAIL)

/* Support function to do an atomic set and read verification */
static int single_set_and_read(struct unit_module *m,
			       struct atomic_struct *atomic,
			       enum atomic_type type, const long set_val)
{
	long read_val;

	if ((type == ATOMIC_32) &&
	    ((set_val < INT_MIN) || (set_val > INT_MAX))) {
		unit_return_fail(m, "Invalid value for 32 op\n");
	}

	func_set(type, atomic, set_val);
	read_val = func_read(type, atomic);
	if (read_val != set_val) {
		unit_err(m, "Atomic returned wrong value. Expected: %ld "
			    "Received: %ld\n", (long)set_val, (long)read_val);
		return UNIT_FAIL;
	}
	return  UNIT_SUCCESS;
}

int test_atomic_set_and_read(struct unit_module *m,
				struct gk20a *g, void *__args)
{
	struct atomic_test_args *args = (struct atomic_test_args *)__args;
	const unsigned int loop_limit = args->type == ATOMIC_32 ?
				(sizeof(int) * 8) : (sizeof(long) * 8);
	const long min_value = args->type == ATOMIC_32 ? INT_MIN :
							 LONG_MIN;
	const long max_value = args->type == ATOMIC_32 ? INT_MAX :
							 LONG_MAX;
	struct atomic_struct atomic = {0};
	unsigned int i;

	single_set_and_read(m, &atomic, args->type, min_value);
	single_set_and_read(m, &atomic, args->type, max_value);
	single_set_and_read(m, &atomic, args->type, 0);

	for (i = 0; i < loop_limit; i++) {
		if (single_set_and_read(m, &atomic, args->type, (1 << i))
							!= UNIT_SUCCESS) {
			return UNIT_FAIL;
		}
	}

	return UNIT_SUCCESS;
}

int test_atomic_arithmetic(struct unit_module *m,
				struct gk20a *g, void *__args)
{
	struct atomic_test_args *args = (struct atomic_test_args *)__args;
	struct atomic_struct atomic = {0};
	unsigned int i;
	long delta_magnitude;
	long read_val;
	long expected_val;
	bool result_bool;
	bool check_result_bool = false;

	if (single_set_and_read(m, &atomic, args->type, args->start_val)
							!= UNIT_SUCCESS) {
		return UNIT_FAIL;
	}

	for (i = 1; i <= args->loop_count; i++) {
		if (args->op == op_inc) {
			/* use 2 since we test both inc and inc_return */
			delta_magnitude = 2;
			func_inc(args->type, &atomic);
			read_val = func_inc_return(args->type, &atomic);
		} else if (args->op == op_inc_and_test) {
			delta_magnitude = 1;
			check_result_bool = true;
			result_bool = func_inc_and_test(args->type, &atomic);
			read_val = func_read(args->type, &atomic);
		} else if (args->op == op_dec) {
			/* use 2 since we test both dec and dec_return */
			delta_magnitude = 2;
			func_dec(args->type, &atomic);
			read_val = func_dec_return(args->type, &atomic);
		} else if (args->op == op_dec_and_test) {
			delta_magnitude = 1;
			check_result_bool = true;
			result_bool = func_dec_and_test(args->type, &atomic);
			read_val = func_read(args->type, &atomic);
		} else if (args->op == op_add) {
			delta_magnitude = args->value * 2;
			func_add(args->type, args->value, &atomic);
			read_val = func_add_return(args->type, args->value,
							&atomic);
		} else if (args->op == op_sub) {
			delta_magnitude = args->value * 2;
			func_sub(args->type, args->value, &atomic);
			read_val = func_sub_return(args->type, args->value,
							&atomic);
		} else if (args->op == op_sub_and_test) {
			delta_magnitude = args->value;
			check_result_bool = true;
			result_bool = func_sub_and_test(args->type,
						args->value, &atomic);
			read_val = func_read(args->type, &atomic);
		} else {
			unit_return_fail(m, "Test error: invalid op in %s\n",
					__func__);
		}

		expected_val = args->start_val +
			(i * delta_magnitude * ATOMIC_OP_SIGN(args->op));

		/* sanity check */
		if ((args->type == ATOMIC_32) &&
		    ((expected_val > INT_MAX) || (expected_val < INT_MIN))) {
			unit_return_fail(m, "Test error: invalid value in %s\n",
					__func__);
		}

		if (read_val != expected_val) {
			unit_return_fail(m, "Atomic returned wrong value. "
				    "Expected: %ld Received: %ld\n",
				    (long)expected_val, (long)read_val);
		}

		if (check_result_bool) {
			if (((expected_val == 0) && !result_bool) ||
			    ((expected_val != 0) && result_bool)) {
				    unit_return_fail(m,
						"Test result incorrect\n");
			    }
		}
	}

	return UNIT_SUCCESS;
}

static void cmpxchg_inc(enum atomic_type type, struct atomic_struct *ref)
{
	bool done = false;
	long old;

	while (!done) {
		old = func_read(type, ref);
		if (old == func_cmpxchg(type, ref, old, old + 1)) {
			done = true;
		}
	}
}

/*
 * Support function that runs in the threads for the arithmetic threaded
 * test below
 */
static void *arithmetic_thread(void *__args)
{
	struct atomic_thread_info *targs = (struct atomic_thread_info *)__args;
	unsigned int i;

	pthread_barrier_wait(&thread_barrier);

	for (i = 0; i < targs->margs->loop_count; i++) {
		if (targs->margs->op == op_cmpxchg) {
			/* special case with special function */
			cmpxchg_inc(targs->margs->type, targs->atomic);
		} else if (targs->margs->op == op_inc) {
			func_inc(targs->margs->type, targs->atomic);
		} else if (targs->margs->op == op_dec) {
			func_dec(targs->margs->type, targs->atomic);
		} else if (targs->margs->op == op_add) {
			/*
			 * Save the last value to sanity that threads aren't
			 * running sequentially
			 */
			targs->final_val = func_add_return(
						targs->margs->type,
						targs->margs->value,
						targs->atomic);
		} else if (targs->margs->op == op_add) {
			func_add(targs->margs->type, targs->margs->value,
					targs->atomic);
		} else if (targs->margs->op == op_sub) {
			func_sub(targs->margs->type, targs->margs->value,
					targs->atomic);
		} else if (targs->margs->op == op_inc_and_test) {
			if (func_inc_and_test(targs->margs->type,
							targs->atomic)) {
				/*
				 * Only increment if atomic op returns true
				 * (that the value is 0)
				 */
				targs->iterations++;
			}
		} else if (targs->margs->op == op_dec_and_test) {
			if (func_dec_and_test(targs->margs->type,
							targs->atomic)) {
				/*
				 * Only increment if atomic op returns true
				 * (that the value is 0)
				 */
				targs->iterations++;
			}
		} else if (targs->margs->op == op_sub_and_test) {
			if (func_sub_and_test(targs->margs->type,
						targs->margs->value,
						targs->atomic)) {
				/*
				 * Only increment if atomic op returns true
				 * (that the value is 0)
				 */
				targs->iterations++;
			}
		} else if (targs->margs->op == op_add_unless) {
			if (func_add_unless(targs->margs->type,
					targs->atomic, targs->margs->value,
					targs->unless) != targs->unless) {
				/*
				 * Increment until the atomic value is the
				 * "unless" value.
				 */
				targs->iterations++;
			}
		} else {
			/*
			 * Don't print an error here because it would print
			 * for each thread. The main thread will catch this.
			 */
			break;
		}
	}

	return NULL;
}

/*
 * Support function to make sure the threaded arithmetic tests ran the correct
 * number of iterations across threads, if applicable.
 */
static bool correct_thread_iteration_count(struct unit_module *m,
					   struct atomic_thread_info *threads,
					   unsigned int num_threads,
					   long expected_iterations)
{
	unsigned int i;
	long total_iterations = 0;

	for (i = 0; i < num_threads; i++) {
		total_iterations += threads[i].iterations;
	}

	if (total_iterations != expected_iterations) {
		unit_err(m, "threaded test op took unexpected number of "
			 "iterations expected %ld took: %ld\n",
			 expected_iterations, total_iterations);
		return false;
	}

	return true;
}

int test_atomic_arithmetic_threaded(struct unit_module *m,
					struct gk20a *g, void *__args)
{
	struct atomic_test_args *args = (struct atomic_test_args *)__args;
	struct atomic_struct atomic = {0};
	const unsigned int num_threads = 100;
	struct atomic_thread_info threads[num_threads];
	unsigned int i;
	long expected_val, val, expected_iterations;
	int ret = UNIT_SUCCESS;

	if (single_set_and_read(m, &atomic, args->type, args->start_val)
							!= UNIT_SUCCESS) {
		return UNIT_FAIL;
	}

	pthread_barrier_init(&thread_barrier, NULL, num_threads);

	/* setup and start threads */
	for (i = 0; i < num_threads; i++) {
		threads[i].atomic = &atomic;
		threads[i].margs = args;
		threads[i].thread_num = i;
		threads[i].iterations = 0;
		/* For add_unless, add until we hit half the iterations */
		threads[i].unless = args->start_val +
				(num_threads * args->loop_count / 2);
		pthread_create(&threads[i].thread, NULL, arithmetic_thread,
				&threads[i]);
	}

	/* wait for all threads to complete */
	for (i = 0; i < num_threads; i++) {
		pthread_join(threads[i].thread, NULL);
	}

	val = func_read(args->type, &atomic);

	switch (args->op) {
		case op_add_unless:
			/*
			 * For add_unless, the threads increment their iteration
			 * counts until the atomic reaches the unless value,
			 * but continue calling the op in the loop to make sure
			 * it doesn't actually add anymore.
			 */
			expected_iterations = (threads[0].unless -
						args->start_val + 1) /
						args->value;
			if (!correct_thread_iteration_count(m, threads,
					num_threads, expected_iterations)) {
				ret = UNIT_FAIL;
				goto exit;
			}
			expected_val = threads[0].unless;
			break;

		case op_inc_and_test:
		case op_dec_and_test:
		case op_sub_and_test:
			/*
			 * The threads only increment when the atomic op
			 * reports that it hit 0 which should only happen once.
			 */
			if (!correct_thread_iteration_count(m, threads,
				num_threads, 1)) {
				ret = UNIT_FAIL;
				goto exit;
			}
			/* fall through! */

		case op_add:
		case op_sub:
		case op_inc:
		case op_dec:
		case op_cmpxchg:
			expected_val = args->start_val +
				(args->loop_count * num_threads *
					ATOMIC_OP_SIGN(args->op) * args->value);
			break;

		default:
			unit_err(m, "Test error: invalid op in %s\n", __func__);
			ret = UNIT_FAIL;
			goto exit;
	}

	/* sanity check */
	if ((args->type == ATOMIC_32) &&
	    ((expected_val > INT_MAX) || (expected_val < INT_MIN))) {
		unit_err(m, "Test error: invalid value in %s\n", __func__);
		ret = UNIT_FAIL;
		goto exit;
	}

	if (val != expected_val) {
		unit_err(m, "threaded value incorrect expected: %ld "
				"result: %ld\n",
				expected_val, val);
		ret = UNIT_FAIL;
		goto exit;
	}

	if (args->op == op_add) {
		/* sanity test that the threads aren't all sequential */
		bool sequential = true;
		for (i = 0; i < (num_threads - 1); i++) {
			if (labs(threads[i].final_val - threads[i+1].final_val)
						!= (long)args->loop_count) {
				sequential = false;
				break;
			}
		}
		if (sequential) {
			unit_err(m, "threads appear to have run "
					"sequentially!\n");
			ret = UNIT_FAIL;
			goto exit;
		}
	}

exit:
	pthread_barrier_destroy(&thread_barrier);

	if (args->type == NOT_ATOMIC) {
		/* For the non-atomics, pass is fail and fail is pass */
		return INVERTED_RESULT(ret);
	} else {
		return ret;
	}
}

/*
 * Thread function for the test_atomic_arithmetic_and_test_threaded() test.
 * Calls the *_and_inc_test op once and saves whether the op returned true by
 * incrementing in the iterations thread struct.
 */
static void *arithmetic_and_test_updater_thread(void *__args)
{
	struct atomic_thread_info *targs = (struct atomic_thread_info *)__args;
	struct atomic_struct *atomic_p = targs->atomic;
	bool is_zero;
	unsigned int i;

	while (true) {
		/* wait here to start */
		pthread_barrier_wait(&thread_barrier);
		if (stop_threads) {
			return NULL;
		}

		for (i = 0; i < targs->margs->loop_count; i++) {
			switch (targs->margs->op) {
				case op_inc_and_test:
					is_zero = func_inc_and_test(
							targs->margs->type,
							atomic_p);
					break;
				case op_dec_and_test:
					is_zero = func_dec_and_test(
							targs->margs->type,
							atomic_p);
					break;
				case op_sub_and_test:
					is_zero = func_sub_and_test(
							targs->margs->type,
							targs->margs->value,
							atomic_p);
					break;
				default:
					/* designate failure */
					is_zero = false;
					break;
			}

			if (is_zero) {
				/*
				* Only count iterations where the op says the
				* value is 0
				*/
				targs->iterations++;
			}
		}

		/* wait until everyone finishes this iteration */
		pthread_barrier_wait(&thread_barrier);
	}

	return NULL;
}

int test_atomic_arithmetic_and_test_threaded(struct unit_module *m,
						struct gk20a *g, void *__args)
{
	struct atomic_test_args *args = (struct atomic_test_args *)__args;
	struct atomic_struct atomic;
	const int num_threads = 100;
	/* Start the atomic such that half the threads will potentially see 0 */
	const long start_val = 0 -
		(ATOMIC_OP_SIGN(args->op) * num_threads / 2) * args->loop_count;
	struct atomic_thread_info threads[num_threads];
	int i;
	unsigned int repeat = args->repeat_count;
	int result = UNIT_SUCCESS;

	pthread_barrier_init(&thread_barrier, NULL, num_threads + 1);
	stop_threads = false;

	do {
		if (single_set_and_read(m, &atomic, args->type, start_val) !=
								UNIT_SUCCESS) {
			return UNIT_FAIL;
		}

		/* setup threads */
		for (i = 0; i < num_threads; i++) {
			threads[i].iterations = 0;
			if (repeat == args->repeat_count) {
				threads[i].atomic = &atomic;
				threads[i].margs = args;
				threads[i].thread_num = i;
				pthread_create(&threads[i].thread, NULL,
					arithmetic_and_test_updater_thread,
					&threads[i]);
			}
		}

		/* start threads */
		pthread_barrier_wait(&thread_barrier);

		/* wait for all threads to complete */
		pthread_barrier_wait(&thread_barrier);

		/*
		 * The threads only count iterations where the test func
		 * returns true. So, this should only happen once.
		 */
		if (!correct_thread_iteration_count(m, threads,
						num_threads, 1)) {
			result = UNIT_FAIL;
			break;
		}
		/*
		 * Note: The final value isn't verified because we are testing
		 * the atomicity of the operation and the testing. And the
		 * non-atomic case may fail the final value before failing the
		 * test being tested for.
		 */
	} while (repeat-- > 0);

	/* signal the end to the threads, then wake them */
	stop_threads = true;
	pthread_barrier_wait(&thread_barrier);

	/* wait for all threads to exit */
	for (i = 0; i < num_threads; i++) {
		pthread_join(threads[i].thread, NULL);
	}

	pthread_barrier_destroy(&thread_barrier);

	if (args->type == NOT_ATOMIC) {
		/* For the non-atomics, pass is fail and fail is pass */
		return INVERTED_RESULT(result);
	} else {
		return result;
	}
}

int test_atomic_xchg(struct unit_module *m,
			struct gk20a *g, void *__args)
{
	struct atomic_test_args *args = (struct atomic_test_args *)__args;
	struct atomic_struct atomic = {0};
	unsigned int i;
	long new_val, old_val, ret_val;

	if (single_set_and_read(m, &atomic, args->type, args->start_val)
							!= UNIT_SUCCESS) {
		return UNIT_FAIL;
	}

	old_val = args->start_val;
	for (i = 0; i < args->loop_count; i++) {
		/*
		 * alternate positive and negative values while increasing
		 * based on the loop counter
		 */
		new_val = (i % 2 ? 1 : -1) * (args->start_val + i);
		/* only a 32bit xchg op */
		ret_val = func_xchg(args->type, &atomic, new_val);
		if (ret_val != old_val) {
			unit_return_fail(m, "xchg returned bad old val "
					"Expected: %ld, Received: %ld\n",
					old_val, ret_val);
		}
		old_val = new_val;
	}

	return UNIT_SUCCESS;
}

/*
 * Function to do xchg operation for the test_atomic_xchg_threaded() test
 *
 * Each thread will run a for loop which will xchg its value with the atomic
 * See the main test for more details
 */
static void *xchg_thread(void *__args)
{
	struct atomic_thread_info *targs = (struct atomic_thread_info *)__args;
	unsigned int i;

	while (true) {
		/* wait here to start iteration */
		pthread_barrier_wait(&thread_barrier);
		if (stop_threads) {
			return NULL;
		}

		for (i = 0; i < 1000; i++) {
			targs->xchg_val = func_xchg(targs->margs->type,
						targs->atomic, targs->xchg_val);
		}

		/* wait until everyone finishes this iteration */
		pthread_barrier_wait(&thread_barrier);
	}

	return NULL;
}

int test_atomic_xchg_threaded(struct unit_module *m,
				struct gk20a *g, void *__args)
{
	struct atomic_test_args *args = (struct atomic_test_args *)__args;
	struct atomic_struct atomic = {0};
	const unsigned int num_threads = 100;
	struct atomic_thread_info threads[num_threads];
	unsigned int i;
	unsigned int repeat = args->repeat_count;
	int result = UNIT_SUCCESS;
	const long start_val = -999;
	bool start_val_present;

	pthread_barrier_init(&thread_barrier, NULL, num_threads + 1);
	stop_threads = false;

	do {
		/* start at -999 */
		if (single_set_and_read(m, &atomic, args->type, start_val) !=
								UNIT_SUCCESS) {
			result = UNIT_FAIL;
			goto exit;
		}

		/* setup threads */
		for (i = 0; i < num_threads; i++) {
			threads[i].iterations = 0;
			threads[i].xchg_val = i;
			if (repeat == args->repeat_count) {
				threads[i].atomic = &atomic;
				threads[i].margs = args;
				threads[i].thread_num = i;
				pthread_create(&threads[i].thread, NULL,
						xchg_thread, &threads[i]);
			}
		}

		/* start threads */
		pthread_barrier_wait(&thread_barrier);

		/* wait for all threads to complete */
		pthread_barrier_wait(&thread_barrier);

		start_val_present = false;
		for (i = 0; i < num_threads; i++) {
			unsigned int j;

			if (threads[i].xchg_val == start_val) {
				start_val_present = true;
			}
			for (j = (i + 1); j < num_threads; j++) {
				if (threads[i].xchg_val ==
						threads[j].xchg_val) {
					unit_err(m, "duplicate value\n");
					result = UNIT_FAIL;
					goto exit;
				}
			}
		}
		if ((func_read(args->type, &atomic) != start_val) &&
		    !start_val_present) {
			unit_err(m, "start value no present\n");
			result = UNIT_FAIL;
			goto exit;
		}
	} while (repeat-- > 0);

exit:
	/* signal the end to the threads, then wake them */
	stop_threads = true;
	pthread_barrier_wait(&thread_barrier);

	/* wait for all threads to exit */
	for (i = 0; i < num_threads; i++) {
		pthread_join(threads[i].thread, NULL);
	}

	pthread_barrier_destroy(&thread_barrier);

	if (args->type == NOT_ATOMIC) {
		/* For the non-atomics, pass is fail and fail is pass */
		return INVERTED_RESULT(result);
	} else {
		return result;
	}
}

int test_atomic_cmpxchg(struct unit_module *m,
			struct gk20a *g, void *__args)
{
	struct atomic_test_args *args = (struct atomic_test_args *)__args;
	struct atomic_struct atomic = {0};
	const int switch_interval = 5;
	unsigned int i;
	long new_val, old_val, ret_val;
	bool should_match = true;

	if (single_set_and_read(m, &atomic, args->type, args->start_val)
							!= UNIT_SUCCESS) {
		return UNIT_FAIL;
	}

	old_val = args->start_val;
	for (i = 0; i < args->loop_count; i++) {
		/*
		 * alternate whether the cmp should match each
		 * switch_interval
		 */
		if ((i % switch_interval) == 0) {
			should_match = !should_match;
		}

		new_val = args->start_val + i;
		if (should_match) {
			ret_val = func_cmpxchg(args->type, &atomic,
						old_val, new_val);
			if (ret_val != old_val) {
				unit_return_fail(m,
					"cmpxchg returned bad old val "
					"Expected: %ld, Received: %ld\n",
					old_val, ret_val);
			}
			ret_val = func_read(args->type, &atomic);
			if (ret_val != new_val) {
				unit_return_fail(m,
					"cmpxchg did not update "
					"Expected: %ld, Received: %ld\n",
					new_val, ret_val);
			}
			old_val = new_val;
		} else {
			ret_val = func_cmpxchg(args->type, &atomic,
						-1 * old_val, new_val);
			if (ret_val != old_val) {
				unit_return_fail(m,
					"cmpxchg returned bad old val "
					"Expected: %ld, Received: %ld\n",
					old_val, ret_val);
			}
			ret_val = func_read(args->type, &atomic);
			if (ret_val != old_val) {
				unit_return_fail(m,
					"cmpxchg should not have updated "
					"Expected: %ld, Received: %ld\n",
					old_val, ret_val);
			}
		}
	}

	return UNIT_SUCCESS;
}

int test_atomic_add_unless(struct unit_module *m,
				struct gk20a *g, void *__args)
{
	struct atomic_test_args *args = (struct atomic_test_args *)__args;
	struct atomic_struct atomic = {0};
	const int switch_interval = 5;
	unsigned int i;
	int new_val, old_val, ret_val;
	bool should_update = true;

	if (single_set_and_read(m, &atomic, args->type, args->start_val)
							!= UNIT_SUCCESS) {
		return UNIT_FAIL;
	}
	old_val = args->start_val;
	for (i = 0; i < args->loop_count; i++) {
		/* alternate whether add should occur every switch_interval */
		if ((i % switch_interval) == 0) {
			should_update = !should_update;
		}

		if (should_update) {
			/* This will fail to match and do the add */
			ret_val = func_add_unless(args->type, &atomic,
						args->value, old_val - 1);
			if (ret_val != old_val) {
				unit_return_fail(m,
					"add_unless returned bad old val "
					"Expected: %d, Received: %d\n",
					old_val, ret_val);
			}
			new_val = old_val + args->value;
			ret_val = func_read(args->type, &atomic);
			if (ret_val != new_val) {
				unit_return_fail(m, "add_unless did not "
						"update Expected: %d, "
						"Received: %d\n",
						new_val, ret_val);
			}
			old_val = ret_val;
		} else {
			/* This will match the old value and won't add */
			ret_val = func_add_unless(args->type, &atomic,
						args->value, old_val);
			if (ret_val != old_val) {
				unit_return_fail(m,
					"add_unless returned bad old val "
					"Expected: %d, Received: %d\n",
					old_val, ret_val);
			}
			ret_val = func_read(args->type, &atomic);
			if (ret_val != old_val) {
				unit_return_fail(m, "add_unless should not "
						"have updated Expected: %d, "
						"Received: %d\n",
						old_val, ret_val);
			}
		}
	}

	return UNIT_SUCCESS;
}

static struct atomic_test_args set_and_read_32_arg = {
	.type = ATOMIC_32,
};
static struct atomic_test_args set_and_read_64_arg = {
	.type = ATOMIC_64,
};
static struct atomic_test_args inc_32_arg = {
	.op = op_inc,
	.type = ATOMIC_32,
	.start_val = -500,
	.loop_count = 10000,
	.value = 1,
};
static struct atomic_test_args inc_and_test_32_arg = {
	/* must cross 0 */
	.op = op_inc_and_test,
	.type = ATOMIC_32,
	.start_val = -500,
	.loop_count = 10000,
	.value = 1,
};
static struct atomic_test_args inc_and_test_64_arg = {
	/* must cross 0 */
	.op = op_inc_and_test,
	.type = ATOMIC_64,
	.start_val = -500,
	.loop_count = 10000,
	.value = 1,
};
static struct atomic_test_args inc_and_test_not_atomic_threaded_arg = {
	/* must cross 0 */
	.op = op_inc_and_test,
	.type = NOT_ATOMIC,
	.loop_count = 100,
	.repeat_count = 5000, /* for threaded test */
};
static struct atomic_test_args inc_and_test_32_threaded_arg = {
	/* must cross 0 */
	.op = op_inc_and_test,
	.type = ATOMIC_32,
	.loop_count = 100,
	.repeat_count = 5000, /* for threaded test */
};
static struct atomic_test_args inc_and_test_64_threaded_arg = {
	/* must cross 0 */
	.op = op_inc_and_test,
	.type = ATOMIC_32,
	.loop_count = 100,
	.repeat_count = 5000, /* for threaded test */
};
static struct atomic_test_args inc_64_arg = {
	.op = op_inc,
	.type = ATOMIC_64,
	.start_val = INT_MAX - 500,
	.loop_count = 10000,
	.value = 1,
};
static struct atomic_test_args dec_32_arg = {
	.op = op_dec,
	.type = ATOMIC_32,
	.start_val = 500,
	.loop_count = 10000,
	.value = 1,
};
static struct atomic_test_args dec_and_test_32_arg = {
	/* must cross 0 */
	.op = op_dec_and_test,
	.type = ATOMIC_32,
	.start_val = 500,
	.loop_count = 10000,
	.value = 1,
};
static struct atomic_test_args dec_and_test_64_arg = {
	/* must cross 0 */
	.op = op_dec_and_test,
	.type = ATOMIC_64,
	.start_val = 500,
	.loop_count = 10000,
	.value = 1,
};
static struct atomic_test_args dec_and_test_not_atomic_threaded_arg = {
	/* must cross 0 */
	.op = op_dec_and_test,
	.type = NOT_ATOMIC,
	.loop_count = 100,
	.repeat_count = 5000, /* for threaded test */
};
static struct atomic_test_args dec_and_test_32_threaded_arg = {
	/* must cross 0 */
	.op = op_dec_and_test,
	.type = ATOMIC_32,
	.loop_count = 100,
	.repeat_count = 5000, /* for threaded test */
};
static struct atomic_test_args dec_and_test_64_threaded_arg = {
	/* must cross 0 */
	.op = op_dec_and_test,
	.type = ATOMIC_32,
	.loop_count = 100,
	.repeat_count = 5000, /* for threaded test */
};
static struct atomic_test_args dec_64_arg = {
	.op = op_dec,
	.type = ATOMIC_64,
	.start_val = INT_MIN + 500,
	.loop_count = 10000,
	.value = 1,
};
static struct atomic_test_args add_32_arg = {
	.op = op_add,
	.type = ATOMIC_32,
	.start_val = -500,
	.loop_count = 10000,
	.value = 7,
};
static struct atomic_test_args add_64_arg = {
	.op = op_add,
	.type = ATOMIC_64,
	.start_val = INT_MAX - 500,
	.loop_count = 10000,
	.value = 7,
};
struct atomic_test_args sub_32_arg = {
	.op = op_sub,
	.type = ATOMIC_32,
	.start_val = 500,
	.loop_count = 10000,
	.value = 7,
};
static struct atomic_test_args sub_64_arg = {
	.op = op_sub,
	.type = ATOMIC_64,
	.start_val = INT_MIN + 500,
	.loop_count = 10000,
	.value = 7,
};
static struct atomic_test_args sub_and_test_32_arg = {
	/* must cross 0 */
	.op = op_sub_and_test,
	.type = ATOMIC_32,
	.start_val = 500,
	.loop_count = 10000,
	.value = 5,
};
static struct atomic_test_args sub_and_test_64_arg = {
	/* must cross 0 */
	.op = op_sub_and_test,
	.type = ATOMIC_64,
	.start_val = 500,
	.loop_count = 10000,
	.value = 5,
};
static struct atomic_test_args sub_and_test_not_atomic_threaded_arg = {
	/* must cross 0 */
	.op = op_sub_and_test,
	.type = NOT_ATOMIC,
	.loop_count = 100,
	.value = 5,
	.repeat_count = 5000, /* for threaded test */
};
static struct atomic_test_args sub_and_test_32_threaded_arg = {
	/* must cross 0 */
	.op = op_sub_and_test,
	.type = ATOMIC_32,
	.loop_count = 100,
	.value = 5,
	.repeat_count = 5000, /* for threaded test */
};
static struct atomic_test_args sub_and_test_64_threaded_arg = {
	/* must cross 0 */
	.op = op_sub_and_test,
	.type = ATOMIC_32,
	.loop_count = 100,
	.value = 5,
	.repeat_count = 5000, /* for threaded test */
};
struct atomic_test_args xchg_not_atomic_arg = {
	.op = op_cmpxchg,
	.type = NOT_ATOMIC,
	.start_val = 1,
	.value = 1,
	.loop_count = 10000,
	.repeat_count = 5000, /* for threaded test */
};
struct atomic_test_args xchg_32_arg = {
	.op = op_cmpxchg,
	.type = ATOMIC_32,
	.start_val = 1,
	.value = 1,
	.loop_count = 10000,
	.repeat_count = 5000, /* for threaded test */
};
struct atomic_test_args xchg_64_arg = {
	.op = op_cmpxchg,
	.type = ATOMIC_64,
	.start_val = INT_MAX,
	.value = 1,
	.loop_count = 10000,
	.repeat_count = 5000, /* for threaded test */
};
struct atomic_test_args cmpxchg_not_atomic_arg = {
	.op = op_cmpxchg,
	.type = NOT_ATOMIC,
	.start_val = 1,
	.value = 1,
	.loop_count = 10000,
	.repeat_count = 50000, /* for threaded test */
};
struct atomic_test_args cmpxchg_32_arg = {
	.op = op_cmpxchg,
	.type = ATOMIC_32,
	.start_val = 1,
	.value = 1,
	.loop_count = 10000,
	.repeat_count = 50000, /* for threaded test */
};
struct atomic_test_args cmpxchg_64_arg = {
	.op = op_cmpxchg,
	.type = ATOMIC_64,
	.start_val = INT_MAX,
	.value = 1,
	.loop_count = 10000,
	.repeat_count = 50000, /* for threaded test */
};
static struct atomic_test_args add_unless_32_arg = {
	/* must loop at least 10 times */
	.op = op_add_unless,
	.type = ATOMIC_32,
	.start_val = -500,
	.loop_count = 10000,
	.value = 5,
};
static struct atomic_test_args add_unless_64_arg = {
	/* must loop at least 10 times */
	.op = op_add_unless,
	.type = ATOMIC_64,
	.start_val = -500,
	.loop_count = 10000,
	.value = 5,
};

struct unit_module_test atomic_tests[] = {
	/* Level 0 tests */
	UNIT_TEST(atomic_set_and_read_32,			test_atomic_set_and_read,			&set_and_read_32_arg, 0),
	UNIT_TEST(atomic_set_and_read_64,			test_atomic_set_and_read,			&set_and_read_64_arg, 0),
	UNIT_TEST(atomic_inc_32,				test_atomic_arithmetic,				&inc_32_arg, 0),
	UNIT_TEST(atomic_inc_and_test_32,			test_atomic_arithmetic,				&inc_and_test_32_arg, 0),
	UNIT_TEST(atomic_inc_and_test_64,			test_atomic_arithmetic,				&inc_and_test_64_arg, 0),
	UNIT_TEST(atomic_inc_64,				test_atomic_arithmetic,				&inc_64_arg, 0),
	UNIT_TEST(atomic_dec_32,				test_atomic_arithmetic,				&dec_32_arg, 0),
	UNIT_TEST(atomic_dec_64,				test_atomic_arithmetic,				&dec_64_arg, 0),
	UNIT_TEST(atomic_dec_and_test_32,			test_atomic_arithmetic,				&dec_and_test_32_arg, 0),
	UNIT_TEST(atomic_dec_and_test_64,			test_atomic_arithmetic,				&dec_and_test_64_arg, 0),
	UNIT_TEST(atomic_add_32,				test_atomic_arithmetic,				&add_32_arg, 0),
	UNIT_TEST(atomic_add_64,				test_atomic_arithmetic,				&add_64_arg, 0),
	UNIT_TEST(atomic_sub_32,				test_atomic_arithmetic,				&sub_32_arg, 0),
	UNIT_TEST(atomic_sub_64,				test_atomic_arithmetic,				&sub_64_arg, 0),
	UNIT_TEST(atomic_sub_and_test_32,			test_atomic_arithmetic,				&sub_and_test_32_arg, 0),
	UNIT_TEST(atomic_sub_and_test_64,			test_atomic_arithmetic,				&sub_and_test_64_arg, 0),
	UNIT_TEST(atomic_xchg_32,				test_atomic_xchg,				&xchg_32_arg, 0),
	UNIT_TEST(atomic_xchg_64,				test_atomic_xchg,				&xchg_64_arg, 0),
	UNIT_TEST(atomic_cmpxchg_32,				test_atomic_cmpxchg,				&xchg_32_arg, 0),
	UNIT_TEST(atomic_cmpxchg_64,				test_atomic_cmpxchg,				&xchg_64_arg, 0),
	UNIT_TEST(atomic_add_unless_32,				test_atomic_add_unless,				&add_unless_32_arg, 0),
	UNIT_TEST(atomic_add_unless_64,				test_atomic_add_unless,				&add_unless_64_arg, 0),
	UNIT_TEST(atomic_inc_32_threaded,			test_atomic_arithmetic_threaded,		&inc_32_arg, 0),
	UNIT_TEST(atomic_inc_64_threaded,			test_atomic_arithmetic_threaded,		&inc_64_arg, 0),
	UNIT_TEST(atomic_dec_32_threaded,			test_atomic_arithmetic_threaded,		&dec_32_arg, 0),
	UNIT_TEST(atomic_dec_64_threaded,			test_atomic_arithmetic_threaded,		&dec_64_arg, 0),
	UNIT_TEST(atomic_add_32_threaded,			test_atomic_arithmetic_threaded,		&add_32_arg, 0),
	UNIT_TEST(atomic_add_64_threaded,			test_atomic_arithmetic_threaded,		&add_64_arg, 0),
	UNIT_TEST(atomic_sub_32_threaded,			test_atomic_arithmetic_threaded,		&sub_32_arg, 0),
	UNIT_TEST(atomic_sub_64_threaded,			test_atomic_arithmetic_threaded,		&sub_64_arg, 0),
	UNIT_TEST(atomic_cmpxchg_not_atomic_threaded,		test_atomic_arithmetic_threaded,		&cmpxchg_not_atomic_arg, 0),
	UNIT_TEST(atomic_cmpxchg_32_threaded,			test_atomic_arithmetic_threaded,		&cmpxchg_32_arg, 0),
	UNIT_TEST(atomic_cmpxchg_64_threaded,			test_atomic_arithmetic_threaded,		&cmpxchg_64_arg, 0),

	/* Level 1 tests */
	UNIT_TEST(atomic_inc_and_test_not_atomic_threaded,	test_atomic_arithmetic_and_test_threaded,	&inc_and_test_not_atomic_threaded_arg, 1),
	UNIT_TEST(atomic_inc_and_test_32_threaded,		test_atomic_arithmetic_and_test_threaded,	&inc_and_test_32_threaded_arg, 1),
	UNIT_TEST(atomic_inc_and_test_64_threaded,		test_atomic_arithmetic_and_test_threaded,	&inc_and_test_64_threaded_arg, 1),
	UNIT_TEST(atomic_dec_and_test_not_atomic_threaded,	test_atomic_arithmetic_and_test_threaded,	&dec_and_test_not_atomic_threaded_arg, 1),
	UNIT_TEST(atomic_dec_and_test_32_threaded,		test_atomic_arithmetic_and_test_threaded,	&dec_and_test_32_threaded_arg, 1),
	UNIT_TEST(atomic_dec_and_test_64_threaded,		test_atomic_arithmetic_and_test_threaded,	&dec_and_test_64_threaded_arg, 1),
	UNIT_TEST(atomic_sub_and_test_not_atomic_threaded,	test_atomic_arithmetic_and_test_threaded,	&sub_and_test_not_atomic_threaded_arg, 1),
	UNIT_TEST(atomic_sub_and_test_32_threaded,		test_atomic_arithmetic_and_test_threaded,	&sub_and_test_32_threaded_arg, 1),
	UNIT_TEST(atomic_sub_and_test_64_threaded,		test_atomic_arithmetic_and_test_threaded,	&sub_and_test_64_threaded_arg, 1),
	UNIT_TEST(atomic_add_unless_32_threaded,		test_atomic_arithmetic_threaded,		&add_unless_32_arg, 1),
	UNIT_TEST(atomic_add_unless_64_threaded,		test_atomic_arithmetic_threaded,		&add_unless_64_arg, 1),
	UNIT_TEST(atomic_xchg_not_atomic_threaded,		test_atomic_xchg_threaded,			&xchg_not_atomic_arg, 1),
	UNIT_TEST(atomic_xchg_32_threaded,			test_atomic_xchg_threaded,			&xchg_32_arg, 1),
	UNIT_TEST(atomic_xchg_64_threaded,			test_atomic_xchg_threaded,			&xchg_64_arg, 1),
};

UNIT_MODULE(atomic, atomic_tests, UNIT_PRIO_POSIX_TEST);
