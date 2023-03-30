/*
 *
 * Nvgpu Channel Synchronization Abstraction (Syncpoints)
 *
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_CHANNEL_SYNC_SYNCPT_H
#define NVGPU_CHANNEL_SYNC_SYNCPT_H

#include <nvgpu/types.h>
#include <nvgpu/utils.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/channel_sync.h>

struct nvgpu_channel;
struct nvgpu_channel_sync_syncpt;
struct priv_cmd_entry;

#ifdef CONFIG_TEGRA_GK20A_NVHOST

/**
 * @brief Get syncpoint id
 *
 * @param s [in]	Syncpoint pointer.
 *
 * @return Syncpoint id of \a s.
 */
u32 nvgpu_channel_sync_get_syncpt_id(struct nvgpu_channel_sync_syncpt *s);

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
/*
 * Generate a gpu wait cmdbuf from raw fence(can be syncpoints or semaphores).
 * Returns a gpu cmdbuf that performs the wait when executed.
 */
int nvgpu_channel_sync_wait_syncpt(struct nvgpu_channel_sync_syncpt *s,
	u32 id, u32 thresh, struct priv_cmd_entry **entry);
#endif

/**
 * @brief Get syncpoint from sync operations
 *
 * @param sync [in]	Pointer to sync operations.
 *
 * Converts a valid struct nvgpu_channel_sync pointer \a sync to
 * struct nvgpu_channel_sync_syncpt pointer else return NULL
 *
 * @return Pointer to syncpoint, if sync is backed by a syncpoint.
 * @retval NULL if sync is backed by a sempahore.
 */
struct nvgpu_channel_sync_syncpt *
nvgpu_channel_sync_to_syncpt(struct nvgpu_channel_sync *sync);

/**
 * @brief Create syncpoint.
 *
 * @param c [in]		Pointer to channel.
 *
 * Constructs a struct nvgpu_channel_sync_syncpt.
 *
 * @return Pointer to nvgpu_channel_sync associated with created syncpoint.
 */
struct nvgpu_channel_sync *
nvgpu_channel_sync_syncpt_create(struct nvgpu_channel *c);

#else

static inline u32 nvgpu_channel_sync_get_syncpt_id(
	struct nvgpu_channel_sync_syncpt *s)
{
	return NVGPU_INVALID_SYNCPT_ID;
}
static inline u64 nvgpu_channel_sync_get_syncpt_address(
	struct nvgpu_channel_sync_syncpt *s)
{
	return 0ULL;
}

static inline int nvgpu_channel_sync_wait_syncpt(
	struct nvgpu_channel_sync_syncpt *s,
	u32 id, u32 thresh, struct priv_cmd_entry **entry)
{
	return -EINVAL;
}

static inline struct nvgpu_channel_sync_syncpt *
nvgpu_channel_sync_to_syncpt(struct nvgpu_channel_sync *sync)
{
	return NULL;
}

static inline struct nvgpu_channel_sync *
nvgpu_channel_sync_syncpt_create(struct nvgpu_channel *c)
{
	return NULL;
}

#endif

#endif /* NVGPU_CHANNEL_SYNC_SYNCPT_H */
