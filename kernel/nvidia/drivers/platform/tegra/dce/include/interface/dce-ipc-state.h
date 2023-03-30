/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_IPC_STATE_H
#define DCE_IPC_STATE_H

#include <interface/dce-bitops.h>

/*
 * Flags used to denote the state of IPC data structures
 */
typedef uint32_t				dce_ipc_flags_t;

#define DCE_IPC_FL_VALID		((dce_ipc_flags_t)DCE_BIT(0))
#define DCE_IPC_FL_REGISTERED	((dce_ipc_flags_t)DCE_BIT(1))
#define DCE_IPC_FL_INIT			((dce_ipc_flags_t)DCE_BIT(2))
#define DCE_IPC_FL_READY		((dce_ipc_flags_t)DCE_BIT(3))
#define DCE_IPC_FL_RM_ALLOWED	((dce_ipc_flags_t)DCE_BIT(4))
#define DCE_IPC_FL_MSG_HEADER	((dce_ipc_flags_t)DCE_BIT(15))

/*
 * Different types of signal mechanisms
 */
typedef uint32_t				dce_ipc_signal_type_t;

#define DCE_IPC_SIGNAL_MAILBOX	((dce_ipc_signal_type_t)0U)
#define DCE_IPC_SIGNAL_DOORBELL ((dce_ipc_signal_type_t)1U)

#endif
