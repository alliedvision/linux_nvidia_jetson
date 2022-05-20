/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_POSIX_TYPES_H
#define NVGPU_POSIX_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdarg.h>

/*
 * For endianness functions.
 */
#include <netinet/in.h>

/**
 * Alias for unsigned 8 bit char.
 */
typedef unsigned char		u8;

/**
 * Alias for unsigned 16 bit short.
 */
typedef unsigned short		u16;

/**
 * Alias for unsigned 32 bit int.
 */
typedef unsigned int		u32;

/**
 * Alias for unsigned 64 bit long long.
 */
typedef unsigned long long	u64;

/**
 * Alias for signed 8 bit char.
 */
typedef signed char		s8;

/**
 * Alias for signed 16 bit short.
 */
typedef signed short		s16;

/**
 * Alias for signed 32 bit int.
 */
typedef signed int		s32;

/**
 * Alias for signed 64 bit long long.
 */
typedef signed long long	s64;

#endif /* NVGPU_POSIX_TYPES_H */
