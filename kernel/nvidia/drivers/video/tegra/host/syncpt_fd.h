// SPDX-License-Identifier: GPL-2.0

#ifndef NVHOST_SYNCPT_FD_H
#define NVHOST_SYNCPT_FD_H

struct nvhost_master;
struct nvhost_ctrl_alloc_syncpt_args;

int nvhost_syncpt_fd_alloc(
	struct nvhost_master *master,
	struct nvhost_ctrl_alloc_syncpt_args *args);

int nvhost_syncpt_fd_get(int fd, struct nvhost_syncpt *syncpt, u32 *id);

#endif
