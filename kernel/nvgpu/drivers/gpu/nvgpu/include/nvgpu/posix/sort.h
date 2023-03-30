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

#ifndef NVGPU_POSIX_SORT_H
#define NVGPU_POSIX_SORT_H

#include <stdlib.h>

/**
 * @brief Sort a given set of data.
 *
 * @param base [in,out]		Pointer to the first element in the list.
 * @param num [in]		Number of elements in the list pointed by base.
 * @param size [in]		Size in bytes of each element in the list.
 * @param cmp [in]		Function to compare two items in the list.
 * @param swap [in]		Function to swap two items in the list.
 *
 * The function sorts the given set of data referenced by \a base. The internal
 * implementation used for sorting is qsort.
 */
static void sort(void *base, size_t num, size_t size,
		 int (*cmp)(const void *a, const void *b),
		 void (*swap)(void *a, void *b, int n))
{
	(void)swap;
	qsort(base, num, size, cmp);
}

#endif /* NVGPU_POSIX_SORT_H */
