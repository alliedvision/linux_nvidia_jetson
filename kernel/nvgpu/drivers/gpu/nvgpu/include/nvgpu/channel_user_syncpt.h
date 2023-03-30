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

#ifndef NVGPU_USER_SYNC_H
#define NVGPU_USER_SYNC_H

#ifdef CONFIG_TEGRA_GK20A_NVHOST

#include <nvgpu/types.h>

struct nvgpu_channel;
struct nvgpu_channel_user_syncpt;

/**
 * @brief Create user syncpoint for a channel.
 *
 * @param c [in]		Pointer to Channel.
 *
 * Construct a #nvgpu_channel_user_syncpt that represents a syncpoint allocation
 * to be managed by userspace in conjunction with usermode submits.
 *
 * @return Pointer to #nvgpu_channel_user_syncpt in case of success, or NULL in
 * case of failure.
 */
struct nvgpu_channel_user_syncpt *
nvgpu_channel_user_syncpt_create(struct nvgpu_channel *ch);

/**
 * @brief Get user syncpoint id
 *
 * @param s [in]	User syncpoint pointer.
 *
 * @return Syncpoint id of \a s.
 */
u32 nvgpu_channel_user_syncpt_get_id(struct nvgpu_channel_user_syncpt *s);

/**
 * @brief Get user syncpoint address
 *
 * @param s [in]	User syncpoint pointer.
 *
 * This function returns syncpoint GPU VA. This address can be used in push
 * buffer entries for acquire/release operations.
 *
 * @return Syncpoint address (GPU VA) of syncpoint or 0 if not supported
 */
u64 nvgpu_channel_user_syncpt_get_address(struct nvgpu_channel_user_syncpt *s);

/**
 * @brief Set the user syncpoint to safe state
 *
 * @param s [in]	User syncpoint pointer.
 *
 * This should be used to reset user managed syncpoint since we don't track
 * threshold values for those syncpoints
 */
void nvgpu_channel_user_syncpt_set_safe_state(struct nvgpu_channel_user_syncpt *s);

/**
 * @brief Free user syncpoint
 *
 * @param s [in]	User syncpoint pointer.
 *
 * Free the resources allocated by #nvgpu_channel_user_syncpt_create.
 */
void nvgpu_channel_user_syncpt_destroy(struct nvgpu_channel_user_syncpt *s);

#endif /* CONFIG_TEGRA_GK20A_NVHOST */

#endif /* NVGPU_USER_SYNC_H */
