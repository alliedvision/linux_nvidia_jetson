/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
 */

/**
 * @file camrtc-common.h
 *
 * @brief RCE common header file
 */

#ifndef INCLUDE_CAMRTC_COMMON_H
#define INCLUDE_CAMRTC_COMMON_H

#if defined(__KERNEL__)
#include <linux/types.h>
#include <linux/compiler.h>
#define CAMRTC_PACKED __packed
#define CAMRTC_ALIGN __aligned
#else
#include <stdint.h>
#include <stdbool.h>
#ifndef CAMRTC_PACKED
#define CAMRTC_PACKED __attribute__((packed))
#endif
#ifndef CAMRTC_ALIGN
#define CAMRTC_ALIGN(_n) __attribute__((aligned(_n)))
#endif
#ifndef U64_C
#define U64_C(_x_) (uint64_t)(_x_##ULL)
#endif
#ifndef U32_C
#define U32_C(_x_) (uint32_t)(_x_##UL)
#endif
#ifndef U16_C
#define U16_C(_x_) (uint16_t)(_x_##U)
#endif
#ifndef U8_C
#define U8_C(_x_) (uint8_t)(_x_##U)
#endif
#endif

/**
 * @defgroup MK_xxx Macros for defining constants
 *
 * These macros are used to define constants in the camera/firmware-api
 * headers.
 *
 * The user of the header files can predefine them and override the
 * types of the constants.
 *
 * @{
 */
#ifndef MK_U64
#define MK_U64(_x_)	U64_C(_x_)
#endif

#ifndef MK_U32
#define MK_U32(_x_)	U32_C(_x_)
#endif

#ifndef MK_U16
#define MK_U16(_x_)	U16_C(_x_)
#endif

#ifndef MK_U8
#define MK_U8(_x_)	U8_C(_x_)
#endif

#ifndef MK_BIT32
#define MK_BIT32(_x_)	(MK_U32(1) << MK_U32(_x_))
#endif

#ifndef MK_BIT64
#define MK_BIT64(_x_)	(MK_U64(1) << MK_U64(_x_))
#endif

#ifndef MK_ALIGN
#define MK_ALIGN(_x_)	_x_
#endif

#ifndef MK_SIZE
#define MK_SIZE(_x_)	MK_U32(_x_)
#endif

/** @} */

#endif /* INCLUDE_CAMRTC_COMMON_H */
