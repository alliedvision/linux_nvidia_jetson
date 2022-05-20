/*
 * NVIDIA GPU Hardware Abstraction Layer functions definitions.
 *
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_HAL_INIT_H
#define NVGPU_HAL_INIT_H

struct gk20a;

/**
 * @brief Detect GPU and initialize the HAL.
 *
 * @param g [in] The GPU
 *
 * Initializes GPU units in the GPU driver. Each sub-unit is responsible for HW
 * initialization.
 *
 * Note: Requires the GPU is already powered on.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int nvgpu_detect_chip(struct gk20a *g);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
int nvgpu_init_hal(struct gk20a *g);
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif /* NVGPU_HAL_INIT_H */
