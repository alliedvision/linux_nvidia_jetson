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

#ifndef NVGPU_OS_SCHED_H
#define NVGPU_OS_SCHED_H

#include <nvgpu/log.h>

struct gk20a;

/**
 * @brief Query the id of current thread.
 *
 * Shall return the thread id of the calling thread. Invokes the library
 * function \a pthread_self to get the thread ID of the calling thread.
 * Function does not perform any validation of the parameter.
 *
 * @param g [in]	GPU driver struct.
 *
 * @return Thread ID of the calling thread.
 */
int nvgpu_current_tid(struct gk20a *g);

/**
 * @brief Query the id of current process.
 *
 * Shall return the process id of the calling process. Invokes the library
 * function \a getpid to get the process ID of the calling process. Function
 * does not perform any validation of the parameter.
 *
 * @param g [in]	GPU driver struct.
 *
 * @return Process ID of the calling process.
 */
int nvgpu_current_pid(struct gk20a *g);

/**
 * @brief Print the name of current thread.
 *
 * Implements the printing of the current thread name along with the function
 * and line details. Implementation of this function is OS specific. For QNX,
 * pthread name is printed along with other provided inputs. Invokes the
 * library function \a pthread_getname_np with parameters 0, a local buffer to
 * hold the name and length of the local buffer as parameters. For POSIX build,
 * pthread name is printed only if the build has support for GNU extensions
 * which provides the thread name. Based on the log level indicated by the
 * parameter \a type one of the following functions is invoked with \a g, the
 * format specifier to print and the thread name to print as parameters,
 * - #nvgpu_err()
 * - #nvgpu_warn()
 * - #nvgpu_log(), a value of 0 is used as parameter to indicate the log mask.
 * - #nvgpu_info()
 * Function does not perform any validation of the parameters.
 *
 * @param g [in]		GPU driver struct.
 * @param func_name [in]	Calling function name.
 * @param line [in]		Calling line number.
 * @param ctx [in]		Context pointer.
 * @param type [in]		Log level.
 */
void nvgpu_print_current_impl(struct gk20a *g, const char *func_name, int line,
		void *ctx, enum nvgpu_log_type type);
/**
 * Print the name of calling thread.
 */
#define nvgpu_print_current(g, ctx, type) \
	nvgpu_print_current_impl(g, __func__, __LINE__, ctx, type)

#endif /* NVGPU_OS_SCHED_H */
