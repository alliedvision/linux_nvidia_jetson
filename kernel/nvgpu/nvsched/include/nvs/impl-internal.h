/*
 * Copyright (c) 2021 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
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
