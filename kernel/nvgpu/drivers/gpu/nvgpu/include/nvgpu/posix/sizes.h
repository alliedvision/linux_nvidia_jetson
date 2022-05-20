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

#ifndef NVGPU_POSIX_SIZES_H
#define NVGPU_POSIX_SIZES_H

/**
 * Define for size equal to 0x100.
 */
#define SZ_256		256UL

/**
 * Define for size equal to 0x400(1K).
 */
#define SZ_1K		(1UL << 10)

/**
 * Define for size equal to 0x1000(4K).
 */
#define SZ_4K		(SZ_1K << 2)

/**
 * Define for size equal to 0x10000(64K).
 */
#define SZ_64K		(SZ_1K << 6)

/**
 * Define for size equal to 0x20000(128K).
 */
#define SZ_128K		(SZ_1K << 7)

/**
 * Define for size equal to 0x100000(1M).
 */
#define SZ_1M		(1UL << 20)

/**
 * Define for size equal to 0x1000000(16M).
 */
#define SZ_16M		(SZ_1M << 4)

/**
 * Define for size equal to 0x2000000(32M).
 */
#define SZ_32M		(SZ_1M << 5)

/**
 * Define for size equal to 0x10000000(256M).
 */
#define SZ_256M		(SZ_1M << 8)

/**
 * Define for size equal to 0x20000000(512M).
 */
#define SZ_512M		(SZ_1M << 9)

/**
 * Define for size equal to 0x40000000(1G).
 */
#define SZ_1G		(1UL << 30)

/**
 * Define for size equal to 0x100000000(4G).
 */
#define SZ_4G		(SZ_1G << 2)

#endif /* NVGPU_POSIX_SIZES_H */
