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

#ifndef NVGPU_POSIX_LOG_H
#define NVGPU_POSIX_LOG_H

#include <nvgpu/types.h>
#include <nvgpu/bitops.h>
#include <nvgpu/log_common.h>

struct gk20a;

__attribute__((format (printf, 5, 6)))
void nvgpu_log_msg_impl(struct gk20a *g, const char *func_name, int line,
			enum nvgpu_log_type type, const char *fmt, ...);

__attribute__((format (printf, 5, 6)))
void nvgpu_log_dbg_impl(struct gk20a *g, u64 log_mask,
			const char *func_name, int line,
			const char *fmt, ...);

/**
 * nvgpu_log_impl - Print a debug message
 */
#define nvgpu_log_impl(g, log_mask, fmt, arg...)				\
	nvgpu_log_dbg_impl(g, log_mask, __func__, __LINE__, fmt, ##arg)

/**
 * nvgpu_err_impl - Print an error
 */
#define nvgpu_err_impl(g, fmt, arg...)					\
	nvgpu_log_msg_impl(g, __func__, __LINE__, NVGPU_ERROR, fmt, ##arg)

/**
 * nvgpu_warn_impl - Print a warning
 */
#define nvgpu_warn_impl(g, fmt, arg...)					\
	nvgpu_log_msg_impl(g, __func__, __LINE__, NVGPU_WARNING, fmt, ##arg)

/**
 * nvgpu_info_impl - Print an info message
 */
#define nvgpu_info_impl(g, fmt, arg...)					\
	nvgpu_log_msg_impl(g, __func__, __LINE__, NVGPU_INFO, fmt, ##arg)

/*
 * Deprecated API. Do not use!!
 */

#define gk20a_dbg_impl(log_mask, fmt, arg...)				\
	do {								\
		if (((log_mask) & NVGPU_DEFAULT_DBG_MASK) != 0)	{	\
			nvgpu_log_msg_impl(NULL, __func__, __LINE__,	\
					NVGPU_DEBUG, fmt "\n", ##arg);	\
		}							\
	} while (false)

#endif