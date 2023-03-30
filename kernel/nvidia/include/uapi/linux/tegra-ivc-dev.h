/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __UAPI_TEGRA_IVC_DEV_H
#define __UAPI_TEGRA_IVC_DEV_H

#include <uapi/linux/ioctl.h>

struct nvipc_ivc_info {
	uint32_t nframes;
	uint32_t frame_size;
	uint32_t queue_offset;
	uint32_t queue_size;
	uint32_t area_size;
	bool     rx_first;
	uint64_t noti_ipa;
	uint16_t noti_irq;
};

/*  IOCTL magic number */
#define NVIPC_IVC_IOCTL_MAGIC 0xAA

/* query ivc info */
#define NVIPC_IVC_IOCTL_GET_INFO \
	_IOWR(NVIPC_IVC_IOCTL_MAGIC, 1, struct nvipc_ivc_info)

/* notify remote */
#define NVIPC_IVC_IOCTL_NOTIFY_REMOTE \
	_IO(NVIPC_IVC_IOCTL_MAGIC, 2)

#define NVIPC_IVC_IOCTL_NUMBER_MAX 2

#endif
