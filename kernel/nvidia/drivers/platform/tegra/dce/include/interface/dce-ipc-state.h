/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
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
