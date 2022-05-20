// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#include "mods_internal.h"

#include <linux/trusty/trusty_ipc.h>

#define MODS_PORT "com.nvidia.srv.mods"

int esc_mods_send_trustzone_msg(struct mods_client         *client,
				struct MODS_TZ_PARAMS      *p)
{
	int ret;
	void *chan_ctx = NULL;

	ret = te_open_trusted_session(MODS_PORT, &chan_ctx);
	if (ret < 0) {
		cl_error("Couldn't open connection mods service\n");
		goto error;
	}

	ret = te_launch_trusted_oper(p->buf, p->buf_size, p->cmd, chan_ctx);
	if (ret < 0) {
		cl_error("Trusted operation failed\n");
		goto error;
	}

error:
	p->status = ret;
	if (chan_ctx)
		te_close_trusted_session(chan_ctx);
	return ret;
}

