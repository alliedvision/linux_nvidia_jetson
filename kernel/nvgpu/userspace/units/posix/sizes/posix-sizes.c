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

#include "posix-sizes.h"

int test_size_defines(struct unit_module *m, struct gk20a *g, void *args)
{
	unsigned long long size = 0;

	size = 256;
	if (size != SZ_256) {
		unit_return_fail(m, "Size 256 Mismatch : %ld\n", SZ_256);
	}

	size = 1024;
	if (size != SZ_1K) {
		unit_return_fail(m, "Size 1K Mismatch : %ld\n", SZ_1K);
	}

	size = (4 * 1024);
	if (size != SZ_4K) {
		unit_return_fail(m, "Size 4K Mismatch : %ld\n", SZ_4K);
	}

	size = (64 * 1024);
	if (size != SZ_64K) {
		unit_return_fail(m, "Size 64K Mismatch : %ld\n", SZ_64K);
	}

	size = (128 * 1024);
	if (size != SZ_128K) {
		unit_return_fail(m, "Size 128K Mismatch : %ld\n", SZ_128K);
	}

	size = (1024 * 1024);
	if (size != SZ_1M) {
		unit_return_fail(m, "Size 1M Mismatch : %ld\n", SZ_1M);
	}

	size = (16 * 1024 * 1024);
	if (size != SZ_16M) {
		unit_return_fail(m, "Size 16M Mismatch : %ld\n", SZ_16M);
	}

	size = (32 * 1024 * 1024);
	if (size != SZ_32M) {
		unit_return_fail(m, "Size 32M Mismatch : %ld\n", SZ_32M);
	}

	size = (256 * 1024 * 1024);
	if (size != SZ_256M) {
		unit_return_fail(m, "Size 256M Mismatch : %ld\n", SZ_256M);
	}

	size = (512 * 1024 * 1024);
	if (size != SZ_512M) {
		unit_return_fail(m, "Size 512M Mismatch : %ld\n", SZ_512M);
	}

	size = (1024 * 1024 * 1024);
	if (size != SZ_1G) {
		unit_return_fail(m, "Size 1G Mismatch : %ld\n", SZ_1G);
	}

	size = ((unsigned long)4 * (unsigned long)(1024 * 1024 * 1024));
	if (size != SZ_4G) {
		unit_return_fail(m, "Size 4G Mismatch : %ld\n", SZ_4G);
	}

	return UNIT_SUCCESS;
}

struct unit_module_test posix_sizes_tests[] = {
	UNIT_TEST(size_defines, test_size_defines, NULL, 0),
};

UNIT_MODULE(posix_sizes, posix_sizes_tests, UNIT_PRIO_POSIX_TEST);
