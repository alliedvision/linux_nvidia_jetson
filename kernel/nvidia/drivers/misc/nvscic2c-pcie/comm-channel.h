/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __COMM_CHANNEL_H__
#define __COMM_CHANNEL_H__

#include <uapi/misc/nvscic2c-pcie-ioctl.h>

#include "common.h"

/* forward declaration. */
struct driver_ctx_t;

enum comm_msg_type {
	/* invalid.*/
	COMM_MSG_TYPE_INVALID = 0,

	/*
	 * One time message from peer @DRV_MODE_EPC (PCIe RP) towards
	 * @DRV_MODE_EPF(PCIe EP) for boot-strap mechanism.
	 */
	COMM_MSG_TYPE_BOOTSTRAP,

	/* Link status shared between @DRV_MODE_EPC and @DRV_MODE_EPF.*/
	COMM_MSG_TYPE_LINK,

	/* Share/Register export object with peer.*/
	COMM_MSG_TYPE_REGISTER,

	/* Unregister exported object back with peer.*/
	COMM_MSG_TYPE_UNREGISTER,

	/* return edma rx descriptor iova to peer x86 */
	COMM_MSG_TYPE_EDMA_RX_DESC_IOVA_RETURN,

	/* Maximum. */
	COMM_MSG_TYPE_MAXIMUM,
};

/*
 * For @DRV_MODE_EPF(PCIe EP), it doesn't know the send area towards
 * @DRV_MODE_EPC (PCIe RP - there is not BAR with PCIe RP). This is the first
 * message and only once sent by @DRV_MODE_EPC towards @DRV_MODE_EPF for latter
 * to configure it's outbound translation.
 */
struct comm_msg_bootstrap {
	u64 iova;
	enum peer_cpu_t  peer_cpu;
};

/* to simply,only one channel c2c remote edma case   */
struct comm_msg_edma_rx_desc_iova {
	dma_addr_t iova;
};

/* Link status shared between @DRV_MODE_EPC and @DRV_MODE_EPF.
 * Possible use: @DRV_MODE_EPC sends bootstrap message
 * to @DRV_MODE_EPF without setting it's own PCIe link = UP, therefore,
 * on compeleting initialization, @DRV_MODE_EPF(once bootstrap msg
 * is received) shall send link = up message to @DRV_MODE_EPC.
 */
struct comm_msg_link {
	enum nvscic2c_pcie_link status;
};

/*
 * Private channel communication message sent by NvSciC2cPcie consumer
 * towards producer to register the exported object at the NvSciC2cPcie
 * producer SoC.
 */
struct comm_msg_register {
	u64 export_desc;
	u64 iova;
	size_t size;
	size_t offsetof;
};

/*
 * Private channel communication message sent by NvSciC2cPcie producer
 * towards onsumer to unregister it's exported object.
 */
struct comm_msg_unregister {
	u64 export_desc;
	u64 iova;
	size_t size;
	size_t offsetof;
};

struct comm_msg {
	enum comm_msg_type type;
	union data {
		struct comm_msg_bootstrap bootstrap;
		struct comm_msg_link link;
		struct comm_msg_register reg;
		struct comm_msg_unregister unreg;
		struct comm_msg_edma_rx_desc_iova edma_rx_desc_iova;
	} u;
} __aligned(8);

int
comm_channel_init(struct driver_ctx_t *drv_ctx, void **comm_channel_h);

void
comm_channel_deinit(void **comm_channel_h);

int
comm_channel_msg_send(void *comm_channel_h, struct comm_msg *msg);

int
comm_channel_bootstrap_msg_send(void *comm_channel_h, struct comm_msg *msg);

int
comm_channel_register_msg_cb(void *comm_channel_h, enum comm_msg_type type,
			     struct callback_ops *ops);
int
comm_channel_unregister_msg_cb(void *comm_channel_h, enum comm_msg_type type);

int
comm_channel_edma_rx_desc_iova_send(void *comm_channel_h, struct comm_msg *msg);

#endif //__COMM_CHANNEL_H__
