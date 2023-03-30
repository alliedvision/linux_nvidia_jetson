/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
 */

/**
 * @file camrtc-commands.h
 *
 * @brief Commands used with "nvidia,tegra-camrtc-hsp-vm" & "nvidia,tegra-hsp-mailbox"
 * protocol
 */

#ifndef INCLUDE_CAMRTC_COMMANDS_H
#define INCLUDE_CAMRTC_COMMANDS_H

#include "camrtc-common.h"

/**
 * @defgroup HspVmMsgs Definitions for "nvidia,tegra-camrtc-hsp-vm" protocol
 * @{
 */
#define CAMRTC_HSP_MSG(_id, _param) ( \
	((uint32_t)(_id) << MK_U32(24)) | \
	((uint32_t)(_param) & MK_U32(0xffffff)))
#define CAMRTC_HSP_MSG_ID(_msg) \
	(((_msg) >> MK_U32(24)) & MK_U32(0x7f))
#define CAMRTC_HSP_MSG_PARAM(_msg) \
	((uint32_t)(_msg) & MK_U32(0xffffff))

/**
 * The IRQ message is sent when no other HSP-VM protocol message is being sent
 * (i.e. the messages for higher level protocols implementing HSP such as IVC
 * channel protocol) and the sender has updated its shared semaphore bits.
 */
#define CAMRTC_HSP_IRQ			MK_U32(0x00)

/**
 * The HELLO messages are exchanged at the beginning of VM and RCE FW session.
 * The HELLO message exchange ensures there are no unprocessed messages
 * in transit within VM or RCE FW.
 */
#define CAMRTC_HSP_HELLO		MK_U32(0x40)
/**
 * VM session close in indicated using BYE message,
 * RCE FW reclaims the resources assigned to given VM.
 * It must be sent before the Camera VM shuts down self.
 */
#define CAMRTC_HSP_BYE			MK_U32(0x41)
/**
 * The RESUME message is sent when VM wants to activate the RCE FW
 * and access the camera hardware through it.
 */
#define CAMRTC_HSP_RESUME		MK_U32(0x42)
/**
 * Power off camera HW, switch to idle state. VM initiates it during runtime suspend or SC7.
 */
#define CAMRTC_HSP_SUSPEND		MK_U32(0x43)
/**
 * Used to set up a shared memory area (such as IVC channels, trace buffer etc)
 * between Camera VM and RCE FW.
 */
#define CAMRTC_HSP_CH_SETUP		MK_U32(0x44)
/**
 * The Camera VM can use the PING message to check aliveness of RCE FW and the HSP protocol.
 */
#define CAMRTC_HSP_PING			MK_U32(0x45)
/**
 * SHA1 hash code for RCE FW binary.
 */
#define CAMRTC_HSP_FW_HASH		MK_U32(0x46)
/**
 * The VM includes its protocol version as a parameter to PROTOCOL message.
 * FW responds with its protocol version, or RTCPU_FW_INVALID_VERSION
 * if the VM protocol is not supported.
 */
#define CAMRTC_HSP_PROTOCOL		MK_U32(0x47)
#define CAMRTC_HSP_RESERVED_5E		MK_U32(0x5E) /* bug 200395605 */
#define CAMRTC_HSP_UNKNOWN		MK_U32(0x7F)

/** Shared semaphore bits (FW->VM) */
#define CAMRTC_HSP_SS_FW_MASK		MK_U32(0xFFFF)
#define CAMRTC_HSP_SS_FW_SHIFT		MK_U32(0)

/** Shared semaphore bits (VM->FW) */
#define CAMRTC_HSP_SS_VM_MASK		MK_U32(0x7FFF0000)
#define CAMRTC_HSP_SS_VM_SHIFT		MK_U32(16)

/** Bits used by IVC channels */
#define CAMRTC_HSP_SS_IVC_MASK		MK_U32(0xFF)

/** @} */

/**
 * @defgroup HspMailboxMsgs Definitions for "nvidia,tegra-hsp-mailbox" protocol
 * @{
 */
#define RTCPU_COMMAND(id, value) \
	(((RTCPU_CMD_ ## id) << MK_U32(24)) | ((uint32_t)value))

#define RTCPU_GET_COMMAND_ID(value) \
	((((uint32_t)value) >> MK_U32(24)) & MK_U32(0x7f))

#define RTCPU_GET_COMMAND_VALUE(value) \
	(((uint32_t)value) & MK_U32(0xffffff))
/**
 * RCE FW waits until VM client initiates boot sync with INIT HSP command.
 */
#define RTCPU_CMD_INIT			MK_U32(0)
/**
 * VM client sends host version and expects RCE FW to respond back with
 * current FW version, as part of boot sync.
 */
#define RTCPU_CMD_FW_VERSION		MK_U32(1)
#define RTCPU_CMD_RESERVED_02		MK_U32(2)
#define RTCPU_CMD_RESERVED_03		MK_U32(3)
/**
 * Release RCE FW resources assigned to given VM client, during runtime suspend or SC7.
 */
#define RTCPU_CMD_PM_SUSPEND		MK_U32(4)
#define RTCPU_CMD_RESERVED_05		MK_U32(5)
/**
 * Used to set up a shared memory area (such as IVC channels, trace buffer etc)
 * between Camera VM and RCE FW.
 */
#define RTCPU_CMD_CH_SETUP		MK_U32(6)
#define RTCPU_CMD_RESERVED_5E		MK_U32(0x5E) /* bug 200395605 */
#define RTCPU_CMD_RESERVED_7D		MK_U32(0x7d)
#define RTCPU_CMD_RESERVED_7E		MK_U32(0x7e)
#define RTCPU_CMD_ERROR			MK_U32(0x7f)

#define RTCPU_FW_DB_VERSION		MK_U32(0)
#define RTCPU_FW_VERSION		MK_U32(1)
#define RTCPU_FW_SM2_VERSION		MK_U32(2)
#define RTCPU_FW_SM3_VERSION		MK_U32(3)
/** SM4 firmware can restore itself after suspend */
#define RTCPU_FW_SM4_VERSION		MK_U32(4)

/** SM5 firmware supports IVC synchronization  */
#define RTCPU_FW_SM5_VERSION		MK_U32(5)
/** SM5 driver supports IVC synchronization  */
#define RTCPU_DRIVER_SM5_VERSION	MK_U32(5)

/** SM6 firmware/driver supports camrtc-hsp-vm protocol	 */
#define RTCPU_FW_SM6_VERSION		MK_U32(6)
#define RTCPU_DRIVER_SM6_VERSION	MK_U32(6)

#define RTCPU_IVC_SANS_TRACE		MK_U32(1)
#define RTCPU_IVC_WITH_TRACE		MK_U32(2)

#define RTCPU_FW_HASH_SIZE		MK_U32(20)

#define RTCPU_FW_HASH_ERROR		MK_U32(0xFFFFFF)

#define RTCPU_PM_SUSPEND_SUCCESS	MK_U32(0x100)
#define RTCPU_PM_SUSPEND_FAILURE	MK_U32(0x001)

#define RTCPU_FW_CURRENT_VERSION	RTCPU_FW_SM6_VERSION

#define RTCPU_FW_INVALID_VERSION	MK_U32(0xFFFFFF)

#define RTCPU_RESUME_ERROR		MK_U32(0xFFFFFF)

/** @} */

#endif /* INCLUDE_CAMRTC_COMMANDS_H */
