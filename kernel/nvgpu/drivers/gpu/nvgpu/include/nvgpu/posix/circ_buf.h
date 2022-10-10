/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_POSIX_CIRC_BUF_H
#define NVGPU_POSIX_CIRC_BUF_H

#include <nvgpu/bug.h>

/**
 * @brief Return count in buffer.
 *
 * Calculates the number of elements present in the circular buffer and
 * returns the value. The circular buffer should have a power of 2 size.
 * Macro does not perform any validation of the parameters.
 *
 * @param head [in]        Head of the buffer.
 * @param tail [in]        Tail of the buffer.
 * @param size [in]        Max number of elements in buffer.
 *
 * @return Count of elements in the buffer.
 */
#define CIRC_CNT(head, tail, size) ((__typeof(head))(((head) - (tail))) & ((size)-1U))

/**
 * @brief Return space in buffer.
 *
 * Calculates the space available in the circular buffer and returns the value.
 * The circular buffer should have a power of 2 size.
 * Macro does not perform any validation of the parameters.
 *
 * @param head [in]        Head of the buffer.
 * @param tail [in]        Tail of the buffer.
 * @param size [in]        Max number of elements in buffer.
 *
 * @return The space available in the buffer.
 */
#define CIRC_SPACE(head, tail, size) CIRC_CNT((tail), ((head)+1U), (size))

#endif /* NVGPU_POSIX_CIRC_BUF_H */
