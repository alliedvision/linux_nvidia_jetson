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

/*
 * Internal to gos-nvscic2c module. This file is not supposed to be included
 * by any other external modules.
 */
#ifndef __STREAM_EXTENSIONS_H__
#define __STREAM_EXTENSIONS_H__

#include <linux/types.h>

#include "common.h"

/* forward declaration. */
struct driver_ctx_t;

/* params to instantiate a stream-extension instance.*/
struct stream_ext_params {
	struct node_info_t *local_node;
	struct node_info_t *peer_node;
	u32 ep_id;
	char *ep_name;
	struct platform_device *host1x_pdev;
	enum drv_mode_t drv_mode;
	void *pci_client_h;
	void *comm_channel_h;
	void *vmap_h;
	void *edma_h;
};

int
stream_extension_ioctl(void *stream_ext_h, unsigned int cmd, void *arg);

int
stream_extension_init(struct stream_ext_params *params, void **handle);

void
stream_extension_deinit(void **handle);

void
stream_extension_edma_deinit(void *stream_ext_h);
#endif //__STREAM_EXTENSIONS_H__
