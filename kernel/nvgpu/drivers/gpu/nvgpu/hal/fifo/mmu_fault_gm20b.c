/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/timers.h>
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/fifo.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>

#include <hal/fifo/mmu_fault_gm20b.h>

#include <nvgpu/hw/gm20b/hw_fifo_gm20b.h>

static const char * const gm20b_gpc_client_descs[] = {
	"l1 0", "t1 0", "pe 0",
	"l1 1", "t1 1", "pe 1",
	"l1 2", "t1 2", "pe 2",
	"l1 3", "t1 3", "pe 3",
	"rast", "gcc", "gpccs",
	"prop 0", "prop 1", "prop 2", "prop 3",
	"l1 4", "t1 4", "pe 4",
	"l1 5", "t1 5", "pe 5",
	"l1 6", "t1 6", "pe 6",
	"l1 7", "t1 7", "pe 7",
	"l1 9", "t1 9", "pe 9",
	"l1 10", "t1 10", "pe 10",
	"l1 11", "t1 11", "pe 11",
	"unknown", "unknown", "unknown", "unknown",
	"tpccs 0", "tpccs 1", "tpccs 2",
	"tpccs 3", "tpccs 4", "tpccs 5",
	"tpccs 6", "tpccs 7", "tpccs 8",
	"tpccs 9", "tpccs 10", "tpccs 11",
};

void gm20b_fifo_get_mmu_fault_gpc_desc(struct mmu_fault_info *mmufault)
{
	if (mmufault->client_id >= ARRAY_SIZE(gm20b_gpc_client_descs)) {
		WARN_ON(mmufault->client_id >=
				ARRAY_SIZE(gm20b_gpc_client_descs));
	} else {
		mmufault->client_id_desc =
			 gm20b_gpc_client_descs[mmufault->client_id];
	}
}

static inline u32 gm20b_engine_id_to_fault_id(struct gk20a *g,
			u32 engine_id)
{
	const struct nvgpu_device *dev;

	dev = nvgpu_engine_get_active_eng_info(g, engine_id);

	if (dev == NULL) {
		nvgpu_err(g,
			  "engine_id is not in active list/invalid %d",
			  engine_id);
		return INVAL_ID;
	}

	return dev->fault_id;
}

void gm20b_fifo_trigger_mmu_fault(struct gk20a *g,
		unsigned long engine_ids_bitmask)
{
	unsigned int poll_delay = POLL_DELAY_MIN_US;
	unsigned long engine_id;
	int ret;
	struct nvgpu_timeout timeout;
	u32 fault_id;

	/* set trigger mmu fault */
	for_each_set_bit(engine_id, &engine_ids_bitmask, 32UL) {
		if (!nvgpu_engine_check_valid_id(g, (u32)engine_id)) {
			nvgpu_err(g, "faulting unknown engine %ld", engine_id);
			continue;
		}
		fault_id = gm20b_engine_id_to_fault_id(g, (u32)engine_id);

		if (fault_id == INVAL_ID) {
			continue;
		}
		nvgpu_writel(g, fifo_trigger_mmu_fault_r(fault_id),
				     fifo_trigger_mmu_fault_enable_f(1U));
	}

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	/* Wait for MMU fault to trigger */
	ret = -EBUSY;
	do {
		if ((nvgpu_readl(g, fifo_intr_0_r()) &
		     fifo_intr_0_mmu_fault_pending_f()) != 0U) {
			ret = 0;
			break;
		}

		nvgpu_usleep_range(poll_delay, poll_delay * 2U);
		poll_delay = min_t(u32, poll_delay << 1U, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	if (ret != 0) {
		nvgpu_err(g, "timeout: failed to trigger mmu fault");
	}

	/* release trigger mmu fault */
	for_each_set_bit(engine_id, &engine_ids_bitmask, 32UL) {
		nvgpu_writel(g, fifo_trigger_mmu_fault_r((u32)engine_id), 0U);
	}
}
