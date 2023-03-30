/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_TSG_H
#define NVGPU_GOPS_TSG_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_channel;
struct nvgpu_tsg;

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
enum nvgpu_event_id_type;
#endif

struct gops_tsg {

	/**
	* @brief Enable TSG
	*
	* @param tsg [in]	Pointer to the TSG struct.
	*
	* Configure H/W so that this TSG can be scheduled.
	*/
	void (*enable)(struct nvgpu_tsg *tsg);

	/**
	* @brief Disable TSG
	*
	* @param tsg [in]	Pointer to the TSG struct.
	*
	* Configure H/W so that it skips this TSG for scheduling.
	*/
	void (*disable)(struct nvgpu_tsg *tsg);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	int (*open)(struct nvgpu_tsg *tsg);
	void (*release)(struct nvgpu_tsg *tsg);
	int (*init_eng_method_buffers)(struct gk20a *g,
			struct nvgpu_tsg *tsg);
	void (*deinit_eng_method_buffers)(struct gk20a *g,
			struct nvgpu_tsg *tsg);
	int (*bind_channel)(struct nvgpu_tsg *tsg,
			struct nvgpu_channel *ch);
	void (*bind_channel_eng_method_buffers)(struct nvgpu_tsg *tsg,
			struct nvgpu_channel *ch);
	int (*unbind_channel)(struct nvgpu_tsg *tsg,
			struct nvgpu_channel *ch);
	int (*unbind_channel_check_hw_state)(struct nvgpu_tsg *tsg,
			struct nvgpu_channel *ch);
	int (*unbind_channel_check_hw_next)(struct nvgpu_channel *ch,
			struct nvgpu_channel_hw_state *state);
	void (*unbind_channel_check_ctx_reload)(struct nvgpu_tsg *tsg,
			struct nvgpu_channel *ch,
			struct nvgpu_channel_hw_state *state);
	void (*unbind_channel_check_eng_faulted)(struct nvgpu_tsg *tsg,
			struct nvgpu_channel *ch,
			struct nvgpu_channel_hw_state *state);
	int (*set_timeslice)(struct nvgpu_tsg *tsg, u32 timeslice_us);
	int (*set_long_timeslice)(struct nvgpu_tsg *tsg, u32 timeslice_us);
	u32 (*default_timeslice_us)(struct gk20a *g);
	int (*set_interleave)(struct nvgpu_tsg *tsg, u32 new_level);

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	bool (*check_ctxsw_timeout)(struct nvgpu_tsg *tsg,
			bool *verbose, u32 *ms);
#endif

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	int (*force_reset)(struct nvgpu_channel *ch,
				u32 err_code, bool verbose);
	void (*post_event_id)(struct nvgpu_tsg *tsg,
			      enum nvgpu_event_id_type event_id);
#endif
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

};


#endif
