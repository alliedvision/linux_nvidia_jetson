/*
 * Tegra Host Virtualization Interfaces to Server
 *
 * Copyright (c) 2014-2021, NVIDIA Corporation. All rights reserved.
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

#ifndef __TEGRA_VHOST_H
#define __TEGRA_VHOST_H

enum {
	/* Must start at 1, 0 is used for VGPU */
	TEGRA_VHOST_MODULE_HOST = 1,
	TEGRA_VHOST_MODULE_VIC,
	TEGRA_VHOST_MODULE_VI,
	TEGRA_VHOST_MODULE_VI2,
	TEGRA_VHOST_MODULE_ISP,
	TEGRA_VHOST_MODULE_ISPB,
	TEGRA_VHOST_MODULE_MSENC,
	TEGRA_VHOST_MODULE_NVDEC,
	TEGRA_VHOST_MODULE_NVJPG,
	TEGRA_VHOST_MODULE_NVENC1,
	TEGRA_VHOST_MODULE_NVDEC1,
	TEGRA_VHOST_MODULE_VI_THI,
	TEGRA_VHOST_MODULE_ISP_THI,
	TEGRA_VHOST_MODULE_NVCSI,
	TEGRA_VHOST_MODULE_NVJPG1,
	TEGRA_VHOST_MODULE_OFA,
	TEGRA_VHOST_MODULE_NONE
};

enum {
	TEGRA_VHOST_QUEUE_CMD = 0,
	TEGRA_VHOST_QUEUE_PB,
	TEGRA_VHOST_QUEUE_INTR,
	/* See also TEGRA_VGPU_QUEUE_* in tegra_vgpu.h */
};

enum {
	TEGRA_VHOST_CMD_CONNECT = 0,
	TEGRA_VHOST_CMD_DISCONNECT,
	TEGRA_VHOST_CMD_SUSPEND,
	TEGRA_VHOST_CMD_RESUME,
	TEGRA_VHOST_CMD_GET_CONNECTION_ID,
};

struct tegra_vhost_connect_params {
	u32 module;
	u64 connection_id;
};

struct tegra_vhost_cmd_msg {
	u32 cmd;
	int ret;
	u64 connection_id;
	struct tegra_vhost_connect_params connect;
};

#define TEGRA_VHOST_QUEUE_SIZES			\
	sizeof(struct tegra_vhost_cmd_msg)

#endif
