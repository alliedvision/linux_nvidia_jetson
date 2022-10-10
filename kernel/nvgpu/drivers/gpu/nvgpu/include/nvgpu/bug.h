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
#ifndef NVGPU_BUG_H
#define NVGPU_BUG_H

#ifdef __KERNEL__
#include <linux/bug.h>
#else
#include <nvgpu/posix/bug.h>
#endif
#include <nvgpu/cov_whitelist.h>
#include <nvgpu/list.h>

/**
 * @brief Assert macro based on condition check that code within nvgpu can use.
 *
 * The goal of this macro is to support handling an unexpected state in SW
 * based on the \a cond parameter passed. The implementation is OS specific.
 * In QNX and POSIX implementation, this macro will invoke the #BUG_ON() macro
 * with parameter as #true or #false which is based on the evaluation of
 * \a cond. MAcro does not perform any validation of the parameter.
 *
 * @param cond [in]   The condition to check.
 */
#if defined(__KERNEL__)
#define nvgpu_assert(cond)	((void) WARN_ON(!(cond)))
#else
/*
 * When this assert fails, the function will not return.
 */
#define nvgpu_assert(cond)						\
	({								\
		BUG_ON((cond) == ((bool)(0 != 0)));			\
	})
#endif

/**
 * @brief Macro to force a failed assert.
 *
 * The goal of this macro is to force the consequences of a failed assert.
 * Invokes the macro #nvgpu_assert with parameter as #true.
 */
#define nvgpu_do_assert()						\
	nvgpu_assert((bool)(0 != 0))

/*
 * Define compile-time assert check.
 */
#define ASSERT_CONCAT(a, b) a##b
#define ASSERT_ADD_INFO(a, b) ASSERT_CONCAT(a, b)
#define nvgpu_static_assert(e)						\
	enum {								\
		ASSERT_ADD_INFO(assert_line_, __LINE__) = 1 / (!!(e))	\
	}

struct gk20a;

/**
 * @brief Macro to force a failed assert with error prints.
 *
 * The goal of this macro is to print an error message and force the
 * consequences of a failed assert. Invokes the macro #nvgpu_err with
 * parameters \a g, \a fmt and \a arg to print an error info and then invokes
 * #nvgpu_do_assert to force a failed assert.
 */
#define nvgpu_do_assert_print(g, fmt, arg...)				\
	do {								\
		nvgpu_err(g, fmt, ##arg);				\
		nvgpu_do_assert();					\
	} while (false)


struct nvgpu_bug_cb
{
	void (*cb)(void *arg);
	void *arg;
	struct nvgpu_list_node node;
	bool sw_quiesce_data;
};

static inline struct nvgpu_bug_cb *
nvgpu_bug_cb_from_node(struct nvgpu_list_node *node)
{
	return (struct nvgpu_bug_cb *)
		((uintptr_t)node - offsetof(struct nvgpu_bug_cb, node));
};

#ifdef __KERNEL__
static inline void nvgpu_bug_exit(void) { }
static inline void nvgpu_bug_register_cb(struct nvgpu_bug_cb *cb) { }
static inline void nvgpu_bug_unregister_cb(struct nvgpu_bug_cb *cb) { }
#endif

#endif /* NVGPU_BUG_H */
