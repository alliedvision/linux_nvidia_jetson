/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef DCE_CORE_INTERFACE_IPC_TYPES_H
#define DCE_CORE_INTERFACE_IPC_TYPES_H


/*
 * Because this file is to be shared between DCE Core, RM and driver,
 * it must not include or reference anything that is not contained
 * in the following header files:
 *	  stdint.h
 *	  stddef.h
 *	  stdbool.h
 */

typedef uint32_t		dce_ipc_type_t;

#define DCE_IPC_TYPE_ADMIN		((dce_ipc_type_t)0U)
#define DCE_IPC_TYPE_DISPRM		((dce_ipc_type_t)1U)
#define DCE_IPC_TYPE_HDCP		((dce_ipc_type_t)2U)
#define DCE_IPC_TYPE_RM_NOTIFY		((dce_ipc_type_t)3U)
#define DCE_IPC_TYPE_BPMP		((dce_ipc_type_t)4U)
#define DCE_IPC_TYPE_MAX		((dce_ipc_type_t)5U)


#endif
