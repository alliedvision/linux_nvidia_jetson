/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

/*
 * @file
 *
 * <b> NVIDIA Software Communications Interface (SCI): Error Handling </b>
 *
 * @b Description: This file declares error codes for NvSci APIs.
 */

#ifndef INCLUDED_NVSCI_ERROR_H
#define INCLUDED_NVSCI_ERROR_H

/**
 * @defgroup NvSciError SCI Error Handling
 *
 * Contains error code enumeration and helper macros.
 *
 * @ingroup nvsci_top
 * @{
 */

/**
 * @brief Return/error codes for all NvSci functions.
 *
 * This enumeration contains unique return/error codes to identify the
 * source of a failure. Some errors have direct correspondence to standard
 * errno.h codes, indicated [IN BRACKETS], and may result from failures in
 * lower level system calls. Others indicate failures specific to misuse
 * of NvSci library function.
 */
typedef enum NvSciErrorRec {
	/* Range 0x00000000 - 0x00FFFFFF : Common errors
	 * This range is used for errors common to all NvSci libraries.
	 */

	/** [EOK] No error */
	NvSciError_Success                  = 0x00000000,

	/** Unidentified error with no additional info */
	NvSciError_Unknown                  = 0x00000001,

	/* Generic errors */
	/** [ENOSYS] Feature is not implemented */
	NvSciError_NotImplemented           = 0x00000010,
	/** [ENOTSUP] Feature is not supported */
	NvSciError_NotSupported             = 0x00000011,
	/** [EACCES] Access to resource denied */
	NvSciError_AccessDenied             = 0x00000020,
	/** [EPERM] No permission to perform operation */
	NvSciError_NotPermitted             = 0x00000021,
	/** Resource is in wrong state to perform operation */
	NvSciError_InvalidState             = 0x00000022,
	/** Requested operation is not legal */
	NvSciError_InvalidOperation         = 0x00000023,
	/** Required resource is not initialized */
	NvSciError_NotInitialized           = 0x00000024,
	/** [ENOMEM] Not enough memory */
	NvSciError_InsufficientMemory       = 0x00000030,
	/** Not enough (non-memory) resources */
	NvSciError_InsufficientResource     = 0x00000031,
	/** Resource failed */
	NvSciError_ResourceError            = 0x00000032,

	/* Function parameter errors */
	/** [EINVAL] Invalid parameter value */
	NvSciError_BadParameter             = 0x00000100,
	/** [EFAULT] Invalid address */
	NvSciError_BadAddress               = 0x00000101,
	/** [E2BIG] Parameter list too long */
	NvSciError_TooBig                   = 0x00000102,
	/** [EOVERFLOW] Value too large for data type */
	NvSciError_Overflow                 = 0x00000103,

	/* Timing/temporary errors */
	/** [ETIMEDOUT] Operation timed out*/
	NvSciError_Timeout                  = 0x00000200,
	/** [EAGAIN] Resource unavailable. Try again. */
	NvSciError_TryItAgain               = 0x00000201,
	/** [EBUSY] Resource is busy */
	NvSciError_Busy                     = 0x00000202,
	/** [EINTR] An interrupt ocurred */
	NvSciError_InterruptedCall          = 0x00000203,

	/* Device errors */
	/** [ENODEV] No such device */
	NvSciError_NoSuchDevice             = 0x00001000,
	/** [ENOSPC] No space left on device */
	NvSciError_NoSpace                  = 0x00001001,
	/** [ENXIO] No such device or address */
	NvSciError_NoSuchDevAddr            = 0x00001002,
	/** [EIO] Input/output error */
	NvSciError_IO                       = 0x00001003,
	/** [ENOTTY] Inappropriate I/O control operation */
	NvSciError_InvalidIoctlNum          = 0x00001004,

	/* File system errors */
	/** [ENOENT] No such file or directory*/
	NvSciError_NoSuchEntry              = 0x00001100,
	/** [EBADF] Bad file descriptor */
	NvSciError_BadFileDesc              = 0x00001101,
	/** [EBADFSYS] Corrupted file system detected */
	NvSciError_CorruptedFileSys         = 0x00001102,
	/** [EEXIST] File already exists */
	NvSciError_FileExists               = 0x00001103,
	/** [EISDIR] File is a directory */
	NvSciError_IsDirectory              = 0x00001104,
	/** [EROFS] Read-only file system */
	NvSciError_ReadOnlyFileSys          = 0x00001105,
	/** [ETXTBSY] Text file is busy */
	NvSciError_TextFileBusy             = 0x00001106,
	/** [ENAMETOOLONG] File name is too long */
	NvSciError_FileNameTooLong          = 0x00001107,
	/** [EFBIG] File is too large */
	NvSciError_FileTooBig               = 0x00001108,
	/** [ELOOP] Too many levels of symbolic links */
	NvSciError_TooManySymbolLinks       = 0x00001109,
	/** [EMFILE] Too many open files in process*/
	NvSciError_TooManyOpenFiles         = 0x0000110A,
	/** [ENFILE] Too many open files in system */
	NvSciError_FileTableOverflow        = 0x0000110B,
	/** End of file reached */
	NvSciError_EndOfFile                = 0x0000110C,


	/* Communication errors */
	/** [ECONNRESET] Connection was closed or lost */
	NvSciError_ConnectionReset          = 0x00001200,
	/** [EALREADY] Pending connection is already in progress */
	NvSciError_AlreadyInProgress        = 0x00001201,
	/** [ENODATA] No message data available */
	NvSciError_NoData                   = 0x00001202,
	/** [ENOMSG] No message of the desired type available*/
	NvSciError_NoDesiredMessage         = 0x00001203,
	/** [EMSGSIZE] Message is too large */
	NvSciError_MessageSize              = 0x00001204,
	/** [ENOREMOTE] Remote node doesn't exist */
	NvSciError_NoRemote                 = 0x00001205,

	/* Process/thread errors */
	/** [ESRCH] No such process */
	NvSciError_NoSuchProcess            = 0x00002000,

	/* Mutex errors */
	/** [ENOTRECOVERABLE] Mutex damaged by previous owner's death */
	NvSciError_MutexNotRecoverable      = 0x00002100,
	/** [EOWNERDEAD] Previous owner died while holding mutex */
	NvSciError_LockOwnerDead            = 0x00002101,
	/** [EDEADLK] Taking ownership would cause deadlock */
	NvSciError_ResourceDeadlock         = 0x00002102,

	/* NvSci attribute list errors */
	/** Could not reconcile attributes */
	NvSciError_ReconciliationFailed     = 0x00010100,
	/** Could not validate attributes */
	NvSciError_AttrListValidationFailed = 0x00010101,

	/** End of range for common error codes */
	NvSciError_CommonEnd                = 0x00FFFFFF,


	/* Range 0x01000000 - 0x01FFFFFF : NvSciBuf errors */
	/** Unidentified NvSciBuf error with no additional info */
	NvSciError_NvSciBufUnknown          = 0x01000000,
	/** End of range for NvSciBuf errors */
	NvSciError_NvSciBufEnd              = 0x01FFFFFF,


	/* Range 0x02000000 - 0x02FFFFFF : NvSciSync errors */
	/** Unidentified NvSciSync error with no additional info */
	NvSciError_NvSciSyncUnknown         = 0x02000000,
	/** Unsupported configuration */
	NvSciError_UnsupportedConfig        = 0x02000001,
	/** Provided fence is cleared */
	NvSciError_ClearedFence             = 0x02000002,
	/* End of range for NvScSync errors */
	NvSciError_NvSciSyncEnd             = 0x02FFFFFF,


	/* Range 0x03000000 - 0x03FFFFFF : NvSciStream errors */
	/** Unidentified NvSciStream error with no additional info */
	NvSciError_NvSciStreamUnknown       = 0x03000000,
	/** Internal stream resource failure occurred */
	NvSciError_StreamInternalError      = 0x30000001,
	/** Operation requires stream be fully connected */
	NvSciError_StreamNotConnected       = 0x30000200,
	/** No stream packet available */
	NvSciError_NoStreamPacket           = 0x30001000,
	/** End of range for NvSciStream errors */
	NvSciError_NvSciStreamEnd           = 0x03FFFFFF,


	/* Range 0x04000000 - 0x04FFFFFF : NvSciIpc errors */
	/** Unidentified NvSciIpc error with no additional info */
	NvSciError_NvSciIpcUnknown          = 0x04000000,
	/** End of range for NvSciIpc errors */
	NvSciError_NvSciIpcEnd              = 0x04FFFFFF,


	/* Range 0x05000000 - 0x05FFFFFF : NvSciEvent errors */
	/** Unidentified NvSciEvent error with no additional info */
	NvSciError_NvSciEventUnknown        = 0x05000000,
	/** End of range for NvSciEvent errors */
	NvSciError_NvSciEventEnd            = 0x05FFFFFF,

} NvSciError;

/*
 * @}
 */

#endif /* INCLUDED_NVSCI_ERROR_H */
