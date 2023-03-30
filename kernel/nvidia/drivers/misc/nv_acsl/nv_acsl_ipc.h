/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __NV_ACSL_IPC_H__
#define __NV_ACSL_IPC_H__

#include <linux/tegra_nvadsp.h>

#define MAX_COMP 120   /** Max Components */

#define ACSL_TIMEOUT 5000 /* 5000 ms */
#define ADSP_CORES 4      /* Numbers of ADSP Cores */
#define MAX_PORT_BUFF 4

#define CSM_MSG_QUEUE_WSIZE 0x200
#define MAX_PAYLOAD_SIZE 20

/** Enumeration to define the PORT direction */
enum port_dir {
	IN_PORT,
	OUT_PORT,
	MAX_PORTS,
};

#pragma pack(4)
/* csm message definition */
union csm_message_t {
	msgq_message_t msgq_msg;
	struct {
		int32_t header[MSGQ_MESSAGE_HEADER_WSIZE];
		int32_t payload[MAX_PAYLOAD_SIZE];
	} msg;
};

/* csm queue definition */
union csm_msgq_t {
	msgq_t msgq;
	struct {
		int32_t header[MSGQ_HEADER_WSIZE];
		int32_t queue[CSM_MSG_QUEUE_WSIZE];
	} app_msgq;
};

#define MSGQ_MSG_SIZE(x) \
	(((sizeof(x) + sizeof(int32_t) - 1) & (~(sizeof(int32_t) - 1))) >> 2)

#pragma pack()

/** ADSP CSM shared structure */
struct csm_sm_state_t {
	union csm_msgq_t recv_msgq;
	union csm_msgq_t send_msgq;
	uint16_t mbox_csm_send_id;
	uint16_t mbox_csm_recv_id;
	uint16_t mbox_buf_in_send_id;
	uint16_t mbox_buf_out_send_id;
	uint16_t mbox_buf_in_recv_id;
	uint16_t mbox_buf_out_recv_id;
	uint16_t core_config[ADSP_CORES];
	uint32_t acq_buf_index[MAX_PORTS][MAX_COMP];
	uint32_t rel_buf_index[MAX_PORTS][MAX_COMP];
};

#endif /* __NV_ACSL_IPC_H__ */
