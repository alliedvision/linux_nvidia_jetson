/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <stdlib.h>
#include <stdint.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/circ_buf.h>

#include "posix-circbuf.h"

#define BUFFER_SIZE	16UL

int test_circbufcnt(struct unit_module *m,
			struct gk20a *g, void *args)
{
	unsigned int i;
	unsigned long head, tail, size, ret;

	head = 0;
	tail = 0;
	size = BUFFER_SIZE;
	for (i = 0; i < BUFFER_SIZE; i++) {
		head = i;
		ret = CIRC_CNT(head, tail, size);
		if (ret != i) {
			unit_return_fail(m, "CIRC_CNT failed %ld\n", ret);
		}
	}

	head = BUFFER_SIZE - 1;
	for (i = 0; i < BUFFER_SIZE; i++) {
		tail = (BUFFER_SIZE - 1) - i;
		ret = CIRC_CNT(head, tail, size);
		if (ret != i) {
			unit_return_fail(m, "CIRC_CNT failed %ld\n", ret);
		}
	}

	head = BUFFER_SIZE/2;
	tail = BUFFER_SIZE/2;
	ret = CIRC_CNT(head, tail, size);
	if (ret) {
		unit_return_fail(m, "CIRC_CNT failed %ld\n", ret);
	}

	return UNIT_SUCCESS;
}

int test_circbufspace(struct unit_module *m,
			struct gk20a *g, void *args)
{
	unsigned int i;
	unsigned long head, tail, size, ret;

	tail = BUFFER_SIZE;
	size = BUFFER_SIZE;
	for (i = 0; i < BUFFER_SIZE; i++) {
		head = i;
		ret = CIRC_SPACE(head, tail, size);
		if (ret != ((BUFFER_SIZE - 1) - i)) {
			unit_return_fail(m, "CIRC_SPACE failed %ld\n", ret);
		}
	}

	return UNIT_SUCCESS;
}

struct unit_module_test posix_circbuf_tests[] = {
	UNIT_TEST(circbufcnt, test_circbufcnt, NULL, 0),
	UNIT_TEST(circbufspace, test_circbufspace, NULL, 0),
};

UNIT_MODULE(posix_circbuf, posix_circbuf_tests, UNIT_PRIO_POSIX_TEST);
