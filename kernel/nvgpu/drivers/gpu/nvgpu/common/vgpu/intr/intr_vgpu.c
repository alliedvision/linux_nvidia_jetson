/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/gk20a.h>

#include <nvgpu/vgpu/vgpu_ivc.h>

#include "intr_vgpu.h"
#include "common/vgpu/gr/fecs_trace_vgpu.h"
#include "common/vgpu/fifo/fifo_vgpu.h"
#include "common/vgpu/fifo/channel_vgpu.h"
#include "common/vgpu/fifo/tsg_vgpu.h"
#include "common/vgpu/mm/mm_vgpu.h"
#include "common/vgpu/gr/gr_vgpu.h"

int vgpu_intr_thread(void *dev_id)
{
	struct gk20a *g = dev_id;
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);

	while (true) {
		struct tegra_vgpu_intr_msg *msg;
		u32 sender;
		void *handle;
		size_t size;
		int err;

		err = vgpu_ivc_recv(TEGRA_VGPU_QUEUE_INTR, &handle,
					(void **)&msg, &size, &sender);
		if (err == -ETIME) {
			continue;
		}
		if (err != 0) {
			nvgpu_do_assert_print(g,
				"Unexpected vgpu_ivc_recv err=%d", err);
			continue;
		}

		if (msg->event == TEGRA_VGPU_EVENT_ABORT) {
			vgpu_ivc_release(handle);
			break;
		}

		switch (msg->event) {
		case TEGRA_VGPU_EVENT_INTR:
			if (msg->unit == TEGRA_VGPU_INTR_GR) {
				vgpu_gr_isr(g, &msg->info.gr_intr);
			} else if (msg->unit == TEGRA_VGPU_INTR_FIFO) {
				vgpu_fifo_isr(g, &msg->info.fifo_intr);
			}
			break;
#ifdef CONFIG_NVGPU_FECS_TRACE
		case TEGRA_VGPU_EVENT_FECS_TRACE:
			vgpu_fecs_trace_data_update(g);
			break;
#endif
		case TEGRA_VGPU_EVENT_CHANNEL:
			vgpu_tsg_handle_event(g, &msg->info.channel_event);
			break;
		case TEGRA_VGPU_EVENT_SM_ESR:
			vgpu_gr_handle_sm_esr_event(g, &msg->info.sm_esr);
			break;
		case TEGRA_VGPU_EVENT_SEMAPHORE_WAKEUP:
			g->ops.semaphore_wakeup(g,
					!!msg->info.sem_wakeup.post_events);
			break;
		case TEGRA_VGPU_EVENT_CHANNEL_CLEANUP:
			vgpu_channel_abort_cleanup(g,
					msg->info.ch_cleanup.chid);
			break;
		case TEGRA_VGPU_EVENT_SET_ERROR_NOTIFIER:
			vgpu_channel_set_error_notifier(g,
						&msg->info.set_error_notifier);
			break;
		default:
			nvgpu_err(g, "unknown event %u", msg->event);
			break;
		}

		vgpu_ivc_release(handle);
	}

	while (!nvgpu_thread_should_stop(&priv->intr_handler)) {
		nvgpu_msleep(10);
	}
	return 0;
}
