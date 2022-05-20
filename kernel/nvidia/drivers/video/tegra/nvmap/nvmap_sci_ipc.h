/*
 * drivers/video/tegra/nvmap/nvmap_sci_ipc.c
 *
 * mapping between nvmap_hnadle and sci_ipc entery
 *
 * Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
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
#ifndef __VIDEO_TEGRA_NVMAP_SCI_IPC_H
#define __VIDEO_TEGRA_NVMAP_SCI_IPC_H

int nvmap_validate_sci_ipc_params(struct nvmap_client *client,
			NvSciIpcEndpointAuthToken auth_token,
			NvSciIpcEndpointVuid *pr_vuid,
			NvSciIpcEndpointVuid *localusr_vuid);

int nvmap_create_sci_ipc_id(struct nvmap_client *client,
				struct nvmap_handle *h,
				u32 flags,
				u32 *sci_ipc_id,
				NvSciIpcEndpointVuid pr_vuid,
				bool is_ro);

int nvmap_get_handle_from_sci_ipc_id(struct nvmap_client *client,
				u32 flags,
				u32 sci_ipc_id,
				NvSciIpcEndpointVuid localusr_vuid,
				u32 *h);
#endif /*  __VIDEO_TEGRA_NVMAP_SCI_IPC_H */
