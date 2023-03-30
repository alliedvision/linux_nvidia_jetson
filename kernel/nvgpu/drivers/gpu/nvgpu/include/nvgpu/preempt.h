/*
 * Copyright (c) 2011-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PREEMPT_H
#define NVGPU_PREEMPT_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Preemption interface.
 */
struct gk20a;
struct nvgpu_channel;
struct nvgpu_tsg;

/**
 * @brief Get preemption timeout (ms). This timeout is defined by s/w.
 *
 * @param g [in]	The GPU driver struct to query preempt timeout for.
 *
 * @return Maximum amount of time in ms to wait for preemption completion,
 *         i.e. context non resident on PBDMAs and engines.
 */
u32 nvgpu_preempt_get_timeout(struct gk20a *g);

/**
 * @brief Preempts TSG if channel is bound to TSG.
 *
 * @param g [in]	The GPU driver struct which owns this channel.
 * @param ch [in]	Pointer to channel to be preempted.
 *
 * Preempts TSG if channel is bound to TSG. Preemption implies that the
 * context's state is saved out and also that the context cannot remain parked
 * either in Host or in any engine.
 *
 * After triggering a preempt request for channel's TSG, pbdmas and engines
 * are polled to make sure preemption completed, i.e. context is not loaded
 * on any pbdma or engine.
 *
 * @return 0 in case of success, <0 in case of failure
 * @retval 0 if channel was not bound to TSG.
 * @retval 0 if TSG was not loaded on pbdma or engine.
 * @retval 0 if TSG was loaded (pbdma or engine) and could be preempted.
 * @retval non-zero value if preemption did not complete within s/w defined
 *         timeout.
 */
int nvgpu_preempt_channel(struct gk20a *g, struct nvgpu_channel *ch);

/**
 * Called from recovery handling for volta onwards. This will
 * not be part of safety build after recovery is not supported in safety build.
 */
int nvgpu_preempt_poll_tsg_on_pbdma(struct gk20a *g,
		struct nvgpu_tsg *tsg);
/**
 * @brief Preempt a set of runlists.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param runlists_bitmask [in]	Bitmask of runlists to preempt.
 *
 * Preempt runlists in \a runlists_bitmask:
 * - Write h/w register to trigger preempt on runlists.
 * - All TSG in those runlists are preempted.
 *
 * @note This function is called in case of recovery for error, and does
 * not poll PBDMAs or engines to wait for preempt completion.
 *
 * @note This function should be called with runlist lock held for all
 * the runlists in \a runlists_bitmask.
 */
void nvgpu_fifo_preempt_runlists_for_rc(struct gk20a *g, u32 runlists_bitmask);

/**
 * @brief Preempt TSG.
 *
 * @param g [in]	Pointer to GPU driver struct.
 * @param tsg [in]	Pointer to TSG struct.
 *
 * Preempt TSG:
 * - Acquire lock for active runlist.
 * - Write h/w register to trigger TSG preempt for \a tsg.
 * - Preemption mode (e.g. CTA or WFI) depends on the preemption
 *   mode configured in the GR context.
 * - Release lock acquired for active runlist.
 * - Poll PBDMAs and engines status until preemption is complete,
 *   or poll timeout occurs.
 *
 * On some chips, it is also needed to disable scheduling
 * before preempting TSG.
 *
 * @see nvgpu_preempt_get_timeout
 * @see nvgpu_gr_ctx::compute_preempt_mode
 *
 * @return 0 in case preemption succeeded, < 0 in case of failure.
 * @retval -ETIMEDOUT when preemption was triggered, but did not
 *         complete within preemption poll timeout.
 */
int nvgpu_fifo_preempt_tsg(struct gk20a *g, struct nvgpu_tsg *tsg);
#endif /* NVGPU_PREEMPT_H */
