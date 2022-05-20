/*
 * Copyright (c) 2021 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVS_TYPES_INTERNAL_H
#define NVS_TYPES_INTERNAL_H

/*
 * If an implementation decides to it can provide a types.h header that
 * nvsched will attempt to include. If so then the below define should
 * be set.
 *
 * If no types.h is passed then stdint.h is used to build the expected
 * types in nvsched.
 */
#ifdef NVS_USE_IMPL_TYPES
#include "types.h"
#else
#include <stdint.h>

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;

typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;
typedef int64_t		s64;
#endif

#endif
