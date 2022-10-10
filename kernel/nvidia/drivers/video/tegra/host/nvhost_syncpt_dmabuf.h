/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, NVIDIA Corporation.  All rights reserved.
 */

#ifndef NVHOST_SYNCPT_DMABUF_H
#define NVHOST_SYNCPT_DMABUF_H

struct nvhost_master;
struct nvhost_ctrl_syncpt_dmabuf_args;

int nvhost_syncpt_dmabuf_alloc(
	struct nvhost_master *host,
	struct nvhost_ctrl_syncpt_dmabuf_args *args);

#endif
