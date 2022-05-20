/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


#ifndef NVGPU_USER_SYNCPT_PRIV_H
#define NVGPU_USER_SYNCPT_PRIV_H

#include <nvgpu/types.h>
#include <nvgpu/nvgpu_mem.h>

struct nvgpu_channel;
struct nvgpu_nvhost_dev;

struct nvgpu_channel_user_syncpt {
	struct nvgpu_channel *ch;
	struct nvgpu_nvhost_dev *nvhost;
	uint32_t syncpt_id;
	struct nvgpu_mem syncpt_buf;
};

#define SYNCPT_NAME_SZ 32UL

#endif /* NVGPU_USER_SYNC_PRIV_H */
