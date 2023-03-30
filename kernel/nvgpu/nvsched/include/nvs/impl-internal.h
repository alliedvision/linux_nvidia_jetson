/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVS_IMPL_INTERNAL_H
#define NVS_IMPL_INTERNAL_H

/*
 * Each implementation of the nvsched code needs to provide a few basic
 * functions to use for interaction with the environment. Things such as
 * memory allocation and de-allocation.
 *
 * These should be provided to nvsched as macro definitions as laid out
 * below. Each implementation should provide an impl.h header file in an
 * accessible header include path that defines these macros.
 */

#include "impl.h"

#ifndef nvs_malloc
#error "Missing impl def: nvs_malloc()"
#else
/**
 * @brief Allocate and return a pointer to memory.
 *
 * @param size		Size of the memory in bytes.
 *
 * @return		A void pointer to memory containing \a size
 *			bytes of available memory.
 *
 * Implementation notes: This may allocate more memory than is strictly
 * needed. The \a size argument is an unsigned 64 bit type, u64.
 *
 *   #define nvs_malloc(sched, size)
 */
#endif

#ifndef nvs_free
#error "Missing impl def: nvs_free()"
#else
/**
 * @brief Free a ptr created with nvs_malloc().
 *
 *   #define nvs_free(sched, ptr)
 */
#endif

#ifndef nvs_memset
#error "Missing impl def: nvs_memset()"
#else
/**
 * @brief Set contents of \a ptr to \a value.
 *
 *   #define nvs_memset(ptr, value, size)
 */
#endif

#ifndef nvs_timestamp
#error "Missing impl def: nvs_timestamp()"
#else
/**
 * @brief Return the current time in _nanoseconds_. Expected return is a s64; this
 * makes it easier on Linux.
 *
 *   #define nvs_timestamp()
 */
#endif

#ifndef nvs_log
#error "Missing impl def: nvs_timestamp()"
#else
/**
 * @brief Print a log message; log messages are by definition informational. They
 * are likely going to be printed to a uart or something similar so will be very
 * slow.
 *
 * It is up to the integrator to turn them on and off as needed.
 *
 *   #define nvs_log(sched, fmt, ...)
 */
#endif

#endif
