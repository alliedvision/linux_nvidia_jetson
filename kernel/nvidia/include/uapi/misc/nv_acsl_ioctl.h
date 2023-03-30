/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2021 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHIN
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * nv_acsl_ioctl.h  --  acsl host driver IO control
 */
/**
 * \ingroup acsl_ioctl
 */
/**@{*/
/** @file nv_acsl_ioctl.h -  version 0.1
 */

#ifndef __UAPI_NV_ACSL_IOCTL_H__
#define __UAPI_NV_ACSL_IOCTL_H__

#include <linux/ioctl.h>

#define NV_ACSL_MAGIC 'a'

#define MAX_PAYLOAD 20

/** Structure to hold the acsl->csm arg information.
 */
struct acsl_csm_args_t {
	uint8_t comp_id;              /**< Component ID  */
	uint8_t intf_id;              /**< Interface ID */
	uint8_t size;                 /**< CSM Payload size  */
	int32_t payload[MAX_PAYLOAD]; /**< CSM Payload info */
};

/** Structure to hold the acsl buf arg information.
 */
struct acsl_buf_args_t {
	uint8_t buf_index; /**< Buffer Index */
	uint8_t intf_id;   /**< Interface ID */
	uint8_t comp_id;   /**< Component ID  */
	bool block;        /**< Blocking or non-Blocking call */
};

/** Structure to hold the acsl nvmap arg information.
 */
struct acsl_nvmap_args_t {
	uint32_t mem_handle; /**< Memory handle */
	uint64_t iova_addr;  /**< IOVA address */
};


/* This IOCTL call init the ADSP CSM SW on ADSP, blocking call */
#define ACSL_INIT_CMD \
	_IOWR(NV_ACSL_MAGIC, 0x1, struct acsl_csm_args_t *)
/* This IOCTL call deinit the ADSP CSM SW on ADSP, blocking call */
#define ACSL_DEINIT_CMD \
	_IO(NV_ACSL_MAGIC, 0x2)

/* This IOCTL call open an interface on ADSP, blocking call */
#define ACSL_INTF_OPEN_CMD \
	_IOWR(NV_ACSL_MAGIC, 0x3, struct acsl_csm_args_t *)
/* This IOCTL call close an interface on ADSP, blocking call */
#define ACSL_INTF_CLOSE_CMD \
	_IOWR(NV_ACSL_MAGIC, 0x4, struct acsl_csm_args_t *)

/* This IOCTL call open a component on ADSP, blocking call */
#define ACSL_COMP_OPEN_CMD \
	_IOWR(NV_ACSL_MAGIC, 0x5, struct acsl_csm_args_t *)
/* This IOCTL call close a componenton ADSP, blocking call */
#define ACSL_COMP_CLOSE_CMD \
	_IOWR(NV_ACSL_MAGIC, 0x6, struct acsl_csm_args_t *)

/*
 * This IOCTL call get NvRm memory handle mapped into IOVA
 * space and return IOVA, blocking call
 */
#define ACSL_MAP_IOVA_CMD \
	_IOWR(NV_ACSL_MAGIC, 0x7, struct acsl_nvmap_args_t *)
/* This IOCTL call used to unmap IOVA address for given NvRm memory
 * handle, blocking call
 */
#define ACSL_UNMAP_IOVA_CMD \
	_IOWR(NV_ACSL_MAGIC, 0x8, struct acsl_nvmap_args_t *)

/*
 * This IOCTL call acquire an input buffer, supports both blocking and
 * non-blocking call
 */
#define ACSL_IN_ACQ_BUF_CMD \
	_IOWR(NV_ACSL_MAGIC, 0x9, struct acsl_buf_args_t *)
/*
 *	This IOCTL call release an input buffer, supports both blocking and
 * non-blocking call
 */
#define ACSL_IN_REL_BUF_CMD \
	_IOWR(NV_ACSL_MAGIC, 0xa, struct acsl_buf_args_t *)
/*
 * This IOCTL call acquire an output buffer, supports both blocking and
 * non-blocking call
 */
#define ACSL_OUT_ACQ_BUF_CMD \
	_IOWR(NV_ACSL_MAGIC, 0xb, struct acsl_buf_args_t *)
/*
 * This IOCTL call release an output buffer, supports both blocking and
 * non-blocking call
 */
#define ACSL_OUT_REL_BUF_CMD \
	_IOWR(NV_ACSL_MAGIC, 0xc, struct acsl_buf_args_t *)

#endif
/** @} */
