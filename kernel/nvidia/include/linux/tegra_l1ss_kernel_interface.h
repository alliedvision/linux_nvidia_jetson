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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _L1SS_INTERFACE_H
#define _L1SS_INTERFACE_H

#include <linux/types.h>
#include <linux/tegra_l1ss_ioctl.h>
#include <linux/platform/tegra/l1ss_datatypes.h>

typedef int (*client_callback)(l1ss_cli_callback_param, void *);

typedef struct {
	nv_guard_client_id_t id;
	client_callback cli_callback;
	void *data;
} client_param_t;

struct l1ss_client_param_node {
	struct list_head cli_list;
	client_param_t *p;
};

#ifdef CONFIG_TEGRA_SAFETY
int l1ss_submit_rq(nv_guard_request_t *req, bool can_sleep);
int l1ss_register_client(client_param_t *p);
int l1ss_deregister_client(nv_guard_client_id_t id);
int l1ss_notify_client(l1ss_cli_callback_param val);
void l1ss_set_ivc_ready(void);
#else
inline int l1ss_register_client(client_param_t *p)
{
	return -ENODEV;
}

inline int l1ss_deregister_client(nv_guard_client_id_t id)
{
	return 0;
}

inline int l1ss_notify_client(l1ss_cli_callback_param val)
{
	return 0;
}

inline int l1ss_submit_rq(nv_guard_request_t *req, bool can_sleep)
{
	return 0;
}
#endif
#endif
