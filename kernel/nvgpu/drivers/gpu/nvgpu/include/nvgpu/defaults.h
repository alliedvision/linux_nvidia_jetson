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

#ifndef NVGPU_DEFAULTS_H
#define NVGPU_DEFAULTS_H

/**
 * @defgroup NVGPU_COMMON_DEFAULTS
 *
 * Default values used in NvGPU for timeouts, inits, etc.
 */

/**
 * @ingroup NVGPU_COMMON_DEFAULTS
 *
 * The default timeout value defined in msec and used on a silicon
 * platform. This timeout value is used for channel watchdog, ctxsw timeouts, gr
 * timeouts, etc.
 */
#define NVGPU_DEFAULT_POLL_TIMEOUT_MS	3000

/**
 * @ingroup NVGPU_COMMON_DEFAULTS
 *
 * The default timeout value defined in msec and used for railgate delay. This
 * defines a value for auto-suspend delay.
 */
#define NVGPU_DEFAULT_RAILGATE_IDLE_TIMEOUT 500

/**
 * @ingroup NVGPU_COMMON_DEFAULTS
 *
 * The default timeout value defined in msec and used on FPGA platform. This
 * timeout value is used at places similar to NVGPU_DEFAULT_POLL_TIMEOUT_MS.
 */
#define NVGPU_DEFAULT_FPGA_TIMEOUT_MS 100000U /* 100 sec */

#endif /* NVGPU_DEFAULTS_H */
