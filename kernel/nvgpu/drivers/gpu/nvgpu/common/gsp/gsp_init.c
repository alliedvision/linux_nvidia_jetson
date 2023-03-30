/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/firmware.h>
#include <nvgpu/falcon.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/log.h>
#include <nvgpu/gsp.h>
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
#include <nvgpu/gsp/gsp_test.h>
#endif
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
#include <nvgpu/gsp_sched.h>
#endif

void nvgpu_gsp_isr_support(struct gk20a *g, struct nvgpu_gsp *gsp, bool enable)
{
	nvgpu_log_fn(g, " ");

	/* Enable irq*/
	nvgpu_mutex_acquire(&gsp->isr_mutex);
	if (g->ops.gsp.enable_irq != NULL) {
		g->ops.gsp.enable_irq(g, enable);
	}
	gsp->isr_enabled = enable;
	nvgpu_mutex_release(&gsp->isr_mutex);
}

void nvgpu_gsp_suspend(struct gk20a *g, struct nvgpu_gsp *gsp)
{
	nvgpu_gsp_isr_support(g, gsp, false);

#ifdef CONFIG_NVGPU_FALCON_DEBUG
	nvgpu_falcon_dbg_error_print_enable(gsp->gsp_flcn, false);
#endif
}

void nvgpu_gsp_sw_deinit(struct gk20a *g, struct nvgpu_gsp *gsp)
{
	if (gsp != NULL) {
		nvgpu_mutex_destroy(&gsp->isr_mutex);
#ifdef CONFIG_NVGPU_FALCON_DEBUG
		nvgpu_falcon_dbg_buf_destroy(gsp->gsp_flcn);
#endif

		nvgpu_kfree(g, gsp);
		gsp = NULL;
	}
}

int nvgpu_gsp_debug_buf_init(struct gk20a *g, u32 queue_no, u32 buffer_size)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	/* enable debug buffer support */
#ifdef CONFIG_NVGPU_FALCON_DEBUG
	if ((g->ops.gsp.gsp_get_queue_head != NULL) &&
			(g->ops.gsp.gsp_get_queue_tail != NULL)) {
		err = nvgpu_falcon_dbg_buf_init(
					&g->gsp_flcn, buffer_size,
					g->ops.gsp.gsp_get_queue_head(queue_no),
					g->ops.gsp.gsp_get_queue_tail(queue_no));
		if (err != 0) {
			nvgpu_err(g, "GSP debug init failed");
		}
	}
#endif
	return err;
}

void nvgpu_gsp_isr_mutex_acquire(struct gk20a *g, struct nvgpu_gsp *gsp)
{
	(void)g;
	nvgpu_mutex_acquire(&gsp->isr_mutex);
}

void nvgpu_gsp_isr_mutex_release(struct gk20a *g, struct nvgpu_gsp *gsp)
{
	(void)g;
	nvgpu_mutex_release(&gsp->isr_mutex);
}

bool nvgpu_gsp_is_isr_enable(struct gk20a *g, struct nvgpu_gsp *gsp)
{
	(void)g;
	return gsp->isr_enabled;
}

struct nvgpu_falcon *nvgpu_gsp_falcon_instance(struct gk20a *g)
{
	return &g->gsp_flcn;
}

void nvgpu_gsp_isr(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
	if (nvgpu_gsp_get_stress_test_load(g)) {
		nvgpu_gsp_stest_isr(g);
		return;
	}
#endif
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
	nvgpu_gsp_sched_isr(g);
#endif
	return;
}
