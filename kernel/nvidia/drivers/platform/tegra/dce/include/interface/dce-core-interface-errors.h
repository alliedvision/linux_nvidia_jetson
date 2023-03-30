/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_CORE_INTERFACE_ERRORS_H
#define DCE_CORE_INTERFACE_ERRORS_H

#define DCE_ERR_CORE_SUCCESS                    0U

#define DCE_ERR_CORE_NOT_IMPLEMENTED            1U
#define DCE_ERR_CORE_SC7_SEQUENCE               2U
#define DCE_ERR_CORE_RD_MEM_MAP                 3U
#define DCE_ERR_CORE_WR_MEM_MAP                 4U
#define DCE_ERR_CORE_IVC_INIT                   5U
#define DCE_ERR_CORE_MEM_NOT_FOUND              6U
#define DCE_ERR_CORE_MEM_NOT_MAPPED             7U

#define DCE_ERR_CORE_VMINDEX_INVALID            8U
#define DCE_ERR_CORE_VMINDEX_NO_AST_BASE        9U

#define DCE_ERR_CORE_MEM_ALREADY_MAPPED         10U

#define DCE_ERR_CORE_BAD_ADMIN_CMD              11U
#define DCE_ERR_CORE_INTERFACE_LOCKED           12U
#define DCE_ERR_CORE_INTERFACE_INCOMPATIBLE     13U

#define DCE_ERR_CORE_MEM_SIZE                   14U

#define DCE_ERR_CORE_NULL_PTR                   15U

#define DCE_ERR_CORE_TIMER_INVALID              16U
#define DCE_ERR_CORE_TIMER_EXPIRED              17U

#define DCE_ERR_CORE_IPC_BAD_TYPE               18U
#define DCE_ERR_CORE_IPC_NO_HANDLES             19U
#define DCE_ERR_CORE_IPC_BAD_CHANNEL            20U
#define DCE_ERR_CORE_IPC_CHAN_REGISTERED        21U
#define DCE_ERR_CORE_IPC_BAD_HANDLE             22U
#define DCE_ERR_CORE_IPC_MSG_TOO_LARGE          23U
#define DCE_ERR_CORE_IPC_NO_BUFFERS             24U
#define DCE_ERR_CORE_IPC_BAD_HEADER             25U
#define DCE_ERR_CORE_IPC_IVC_INIT               26U
#define DCE_ERR_CORE_IPC_NO_DATA                27U
#define DCE_ERR_CORE_IPC_INVALID_SIGNAL         28U
#define DCE_ERR_CORE_IPC_IVC_ERR                29U

#define DCE_ERR_CORE_MEM_BAD_REGION             30U

#define DCE_ERR_CORE_GPIO_INVALID_ID            31U
#define DCE_ERR_CORE_GPIO_NO_SPACE              32U

#define DCE_ERR_CORE_RM_BOOTSTRAP               40U

#define DCE_ERR_CORE_IPC_SIGNAL_REGISTERED      50U

#define DCE_ERR_CORE_NOT_FOUND                  60U
#define DCE_ERR_CORE_NOT_INITIALIZED            61U

#define DCE_ERR_CORE_OTHER                      9999U

#endif
