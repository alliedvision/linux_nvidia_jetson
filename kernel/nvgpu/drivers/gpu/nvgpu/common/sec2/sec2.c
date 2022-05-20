/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/firmware.h>
#include <nvgpu/log.h>
#include <nvgpu/sec2/sec2.h>
#include <nvgpu/sec2/queue.h>
#include <nvgpu/sec2/seq.h>
#include <nvgpu/sec2/allocator.h>
#include <nvgpu/sec2/msg.h>

static void nvgpu_remove_sec2_support(struct nvgpu_sec2 *sec2)
{
	struct gk20a *g = sec2->g;

	nvgpu_log_fn(g, " ");

	nvgpu_sec2_sequences_free(g, &sec2->sequences);
	nvgpu_mutex_destroy(&sec2->isr_mutex);
}

int nvgpu_init_sec2_setup_sw(struct gk20a *g)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	g->sec2.g = g;

	err = nvgpu_sec2_sequences_alloc(g, &g->sec2.sequences);
	if (err != 0) {
		return err;
	}

	nvgpu_sec2_sequences_init(g, &g->sec2.sequences);

	nvgpu_mutex_init(&g->sec2.isr_mutex);

	g->sec2.remove_support = nvgpu_remove_sec2_support;

	return err;
}

int nvgpu_init_sec2_support(struct gk20a *g)
{
	struct nvgpu_sec2 *sec2 = &g->sec2;
	int err = 0;

	nvgpu_log_fn(g, " ");

	/* Enable irq*/
	nvgpu_mutex_acquire(&sec2->isr_mutex);
	g->ops.sec2.enable_irq(sec2, true);
	sec2->isr_enabled = true;
	nvgpu_mutex_release(&sec2->isr_mutex);

	/* execute SEC2 in secure mode to boot RTOS */
	g->ops.sec2.secured_sec2_start(g);

	return err;
}

int nvgpu_sec2_destroy(struct gk20a *g)
{
	struct nvgpu_sec2 *sec2 = &g->sec2;

	nvgpu_log_fn(g, " ");

	if (g->sec2.fw.fw_image != NULL) {
		nvgpu_release_firmware(g, g->sec2.fw.fw_image);
	}
	if (g->sec2.fw.fw_desc != NULL) {
		nvgpu_release_firmware(g, g->sec2.fw.fw_desc);
	}
	if (g->sec2.fw.fw_sig != NULL) {
		nvgpu_release_firmware(g, g->sec2.fw.fw_sig);
	}

	nvgpu_sec2_dmem_allocator_destroy(&sec2->dmem);

	nvgpu_mutex_acquire(&sec2->isr_mutex);
	sec2->isr_enabled = false;
	nvgpu_mutex_release(&sec2->isr_mutex);

	nvgpu_sec2_queues_free(g, sec2->queues);

	sec2->sec2_ready = false;

	return 0;
}
