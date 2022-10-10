/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of NVIDIA CORPORATION nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NVIDIA CORPORATION OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __NVSCIIPC_IOCTL_H__
#define __NVSCIIPC_IOCTL_H__

#include <linux/ioctl.h>

#define NVSCIIPC_MAX_EP_NAME      64

struct nvsciipc_config_entry {
	/* endpoint name */
	char ep_name[NVSCIIPC_MAX_EP_NAME];
	/* node name for shm/sem */
	char dev_name[NVSCIIPC_MAX_EP_NAME];
	uint32_t backend;       /* backend type */
	uint32_t nframes;       /* frame count */
	uint32_t frame_size;    /* frame size */
	/* ep id    for inter-Proc/Thread
	 * queue id for inter-VM
	 * dev id   for inter-Chip
	 */
	uint32_t id;
	uint64_t vuid;  /* VM-wide unique id */
};

struct nvsciipc_db {
	int num_eps;
	struct nvsciipc_config_entry **entry;
};

struct nvsciipc_get_vuid {
	char ep_name[NVSCIIPC_MAX_EP_NAME];
	uint64_t vuid;
};

/* IOCTL magic number - seen available in ioctl-number.txt*/
#define NVSCIIPC_IOCTL_MAGIC    0xC3

#define NVSCIIPC_IOCTL_SET_DB \
	_IOW(NVSCIIPC_IOCTL_MAGIC, 1, struct nvsciipc_db)

#define NVSCIIPC_IOCTL_GET_VUID \
	_IOWR(NVSCIIPC_IOCTL_MAGIC, 2, struct nvsciipc_get_vuid)

#define NVSCIIPC_IOCTL_NUMBER_MAX 2

#endif /* __NVSCIIPC_IOCTL_H__ */
