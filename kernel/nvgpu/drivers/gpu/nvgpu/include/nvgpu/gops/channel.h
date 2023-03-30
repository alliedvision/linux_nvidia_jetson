/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_CHANNEL_H
#define NVGPU_GOPS_CHANNEL_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Channel HAL interface.
 */
struct gk20a;
struct nvgpu_channel;
struct nvgpu_channel_hw_state;
struct nvgpu_debug_context;

/**
 * Channel HAL operations.
 *
 * @see gpu_ops.
 */
struct gops_channel {
	/**
	 * @brief Enable channel for h/w scheduling.
	 *
	 * @param ch [in]	Channel pointer.
	 *
	 * The HAL writes CCSR register to enable channel for h/w scheduling.
	 * Once enabled, the channel can be scheduled to run when this
	 * channel is next on the runlist.
	 */
	void (*enable)(struct nvgpu_channel *ch);

	/**
	 * @brief Disable channel from h/w scheduling.
	 *
	 * @param ch [in]	Channel pointer.
	 *
	 * The HAL writes CCSR register to disable channel from h/w scheduling.
	 * Once disabled, the channel is not scheduled to run even if it
	 * is next on the runlist.
	 */
	void (*disable)(struct nvgpu_channel *ch);

	/**
	 * @brief Get number of channels.
	 *
	 * @param g [in]	The GPU driver struct.
	 *
	 * The HAL reads max number of channels supported by the GPU h/w.
	 *
	 * @return Number of channels as read from GPU h/w.
	 */
	u32 (*count)(struct gk20a *g);

	/**
	 * @brief Suspend all channels.
	 *
	 * @param g [in]	The GPU driver struct.
	 *
	 * The HAL goes through all channels and:
	 * - If channel is not in use, done.
	 * - If channel is not serviceable, done.
	 * - Disable channel.
	 * - Preempt channel.
	 * - Wait for channel to update notifiers.
	 * - Unbind channel context from hardware.
	 *
	 * The HAL is also expected to:
	 * - Update runlists to remove channels.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*suspend_all_serviceable_ch)(struct gk20a *g);

	/**
	 * @brief Resume all channels.
	 *
	 * @param g [in]	The GPU driver struct.
	 *
	 * The HAL goes through all channels and:
	 * - If channel is not in use, done.
	 * - If channel is not serviceable, done,
	 * - Bind channel context to hardware.
	 *
	 * The HAL is also expected to:
	 * - Update runlists to add above channels.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*resume_all_serviceable_ch)(struct gk20a *g);

	/**
	 * @brief Set error notifier for a channel.
	 *
	 * @param ch [in]	Channel pointer.
	 * @param error [in]	Error code for notification.
	 *
	 * The HAL does the following:
	 * - Acquire error_notifer mutex.
	 * - If an error notifier buffer was allocated:
	 *   - Get CPU timestamp in ns.
	 *   - Update timestamp in notification buffer.
	 *   - Update error code in notification buffer.
	 *     \a error should be of the form NVGPU_ERR_NOTIFIER_*
	 * - Release error_notifier mutex.
	 *
	 * @see NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT
	 */
	void (*set_error_notifier)(struct nvgpu_channel *ch, u32 error);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */

	int (*alloc_inst)(struct gk20a *g, struct nvgpu_channel *ch);
	void (*free_inst)(struct gk20a *g, struct nvgpu_channel *ch);
	void (*bind)(struct nvgpu_channel *ch);
	void (*unbind)(struct nvgpu_channel *ch);
	void (*read_state)(struct gk20a *g, struct nvgpu_channel *ch,
			struct nvgpu_channel_hw_state *state);
	void (*force_ctx_reload)(struct nvgpu_channel *ch);
	void (*abort_clean_up)(struct nvgpu_channel *ch);
	void (*reset_faulted)(struct gk20a *g, struct nvgpu_channel *ch,
			bool eng, bool pbdma);
	void (*clear)(struct nvgpu_channel *ch);

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	int (*set_syncpt)(struct nvgpu_channel *ch);
#endif

	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

#endif
