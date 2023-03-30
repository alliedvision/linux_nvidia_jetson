/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_COMMON_H
#define NVGPU_COMMON_H

/**
 * @file
 *
 * @addtogroup unit-common-nvgpu
 * @{
 */

#ifdef CONFIG_NVGPU_NON_FUSA
/**
 * @brief Restart driver as implemented for OS.
 *
 * @param cmd [in]	Pointer to command to execute before restart, if
 *			possible. Pass NULL for no command.
 *
 * This is a very OS-dependent interface.
 * - On Linux, this will request the kernel to execute the command if not NULL,
 *   then the kernel will reboot the OS.
 * - On QNX, this simply calls BUG() which will restart the driver.
 */
void nvgpu_kernel_restart(void *cmd);
#endif

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
struct nvgpu_posix_fault_inj *nvgpu_nvgpu_get_fault_injection(void);
#endif

/**
 * @}
 */
#endif
