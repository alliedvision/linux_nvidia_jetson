/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_ADMIN_CMDS_H
#define DCE_ADMIN_CMDS_H

#include <interface/dce-types.h>

/*
 * Version of the ADMIN command interface.
 *
 * This MUST be updated any time any changes are made to the ADMIN
 * commands.
 *
 * To keep things simple, this value should be incremented by 1 each
 * time changes are made.
 */
#define DCE_ADMIN_VERSION				2

#define DCE_ADMIN_CMD_SIZE	sizeof(struct dce_admin_ipc_cmd)
#define DCE_ADMIN_RESP_SIZE	sizeof(struct dce_admin_ipc_resp)

#define DCE_ADMIN_CMD_CHAN_FSIZE max(DCE_ADMIN_CMD_SIZE, \
				 DCE_ADMIN_RESP_SIZE)

#define DCE_ADMIN_CMD_MAX_NFRAMES		4

#define DCE_ADMIN_CMD_VERSION		0x00U   // returns version of interface
#define DCE_ADMIN_CMD_HOST_VERSION	0x01U   // host supplied version
#define DCE_ADMIN_CMD_GET_FW_VERSION	0x02U   // return FW version info
#define DCE_ADMIN_CMD_ECHO		0x03U   // echo data back to CCPLEX
#define DCE_ADMIN_CMD_MEM_MAP		0x04U   // map a region of memory
#define DCE_ADMIN_CMD_MEM_INFO		0x05U   // return info about a region
#define DCE_ADMIN_CMD_IPC_INFO		0x06U   // return IPC chan info
#define DCE_ADMIN_CMD_IPC_CREATE	0x07U   // create an IPC channel
#define DCE_ADMIN_CMD_PREPARE_SC7	0x08U   // prepare to enter SC7
#define DCE_ADMIN_CMD_ENTER_SC7		0x09U   // enter SC7
#define DCE_ADMIN_CMD_SET_LOGGING	0x0AU   // set logging level
#define DCE_ADMIN_CMD_GET_LOG_INFO	0x0BU   // get current log info
#define DCE_ADMIN_CMD_LOCK_CHANGES	0x0CU   // lock creating new channels
						// and changing memory areas
#define DCE_ADMIN_CMD_CODE_COVERAGE_START 0x0DU // start collecting code
						// coverage data
#define DCE_ADMIN_CMD_CODE_COVERAGE_STOP 0x0EU  // stop collecting code
						// coverage data
#define DCE_ADMIN_CMD_PERF_START	0x0FU   // start collecting perf data
#define DCE_ADMIN_CMD_PERF_STOP		0x10U   // stop collecting perf data
#define DCE_ADMIN_CMD_TEST_START	0x11U   // start tests
#define DCE_ADMIN_CMD_TEST_STOP		0x12U   // stop tests and return status
#define DCE_ADMIN_CMD_DEBUG		0x13U   // debug command

#define DCE_ADMIN_CMD_RM_BOOTSTRAP	0x14U   // tell RM to "bootstrap"

#define DCE_ADMIN_CMD_NEXT		0x15U   // must be last command ID + 1

struct dce_admin_version_info {
	uint32_t	version;
};

struct dce_admin_fw_version_info {
	uint32_t	bootstrap_interface;
	uint32_t	admin_interface;
	uint32_t	driver_headers;
	uint32_t	core_interface;
	uint8_t		fw_version[4];
	uint32_t	gcid_revision;
	uint8_t		safertos_major;
	uint8_t		safertos_minor;
};

struct dce_admin_echo {
	uint32_t	data;
};

struct dce_admin_log_args {
	uint32_t	log_enable;
	uint32_t	log_level;
};

struct dce_admin_mem_args {
	uint32_t	region;
	dce_iova	iova;
	uint32_t	length;
	uint32_t	sid;
};

struct dce_admin_ipc_info_args {
	uint32_t	type;
};

struct dce_admin_ipc_signal {
	uint32_t	signal_type;
	union {
		uint32_t	mailbox;
		struct {
			uint32_t	doorbell_num;
			uint32_t	doorbell_bit_num;
		} doorbell;
	} signal;
	uint32_t	semaphore;
	uint32_t	semaphore_bit_num;
};

struct dce_admin_ipc_info {
	uint32_t			type;
	uint32_t			flags;
	uint32_t			mem_region;
	dce_iova			rd_iova;
	dce_iova			wr_iova;
	uint32_t			fsize;
	uint32_t			n_frames;
	struct dce_admin_ipc_signal	signal_from_dce;
	struct dce_admin_ipc_signal	signal_to_dce;
};

struct dce_admin_ipc_create_args {
	uint32_t			type;
	dce_iova			rd_iova;
	dce_iova			wr_iova;
	uint32_t			fsize;
	uint32_t			n_frames;
};

struct dce_admin_ipc_cmd {
	uint32_t	cmd;
	union {
		struct dce_admin_version_info		version;
		struct dce_admin_echo			echo;
		struct dce_admin_log_args		log;
		struct dce_admin_ipc_info_args		ipc_info;
		struct dce_admin_mem_args		mem_map;
		struct dce_admin_ipc_create_args	ipc_create;
	} args;
};

struct dce_admin_ipc_resp {
	uint32_t			error;
	union {
		struct dce_admin_version_info		version;
		struct dce_admin_echo				echo;
		struct dce_admin_log_args			log;
		struct dce_admin_ipc_info			ipc;
		struct dce_admin_mem_args			mem_info;
		struct dce_admin_fw_version_info	fw_version;
	} args;
};

#endif
