/*
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log.h>
#include <nvgpu/debug.h>
#include <nvgpu/utils.h>
#include <nvgpu/fifo.h>
#include <nvgpu/rc.h>
#include <nvgpu/runlist.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/types.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/power_features/power_features.h>
#include <nvgpu/gr/fecs_trace.h>

#include <hal/fifo/mmu_fault_gk20a.h>
#include <hal/rc/rc_gk20a.h>

#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>


void gk20a_fifo_recover(struct gk20a *g, u32 eng_bitmask,
			u32 hw_id, unsigned int id_type, unsigned int rc_type,
			 struct mmu_fault_info *mmufault)
{
	unsigned long engine_id, i;
	unsigned long _engine_ids = eng_bitmask;
	unsigned long engine_ids = 0UL;
	u32 mmu_fault_engines = 0U;
	u32 ref_type;
	u32 ref_id;
	bool ref_id_is_tsg = false;
	bool id_is_known = (id_type != ID_TYPE_UNKNOWN) ? true : false;
	bool id_is_tsg = (id_type == ID_TYPE_TSG) ? true : false;

	(void)rc_type;
	(void)mmufault;

	nvgpu_log_info(g, "acquire engines_reset_mutex");
	nvgpu_mutex_acquire(&g->fifo.engines_reset_mutex);

	nvgpu_runlist_lock_active_runlists(g);

	if (id_is_known) {
		engine_ids = nvgpu_engine_get_mask_on_id(g, hw_id, id_is_tsg);
		ref_id = hw_id;
		ref_type = id_is_tsg ?
			fifo_engine_status_id_type_tsgid_v() :
			fifo_engine_status_id_type_chid_v();
		ref_id_is_tsg = id_is_tsg;
		/* atleast one engine will get passed during sched err*/
		engine_ids |= eng_bitmask;
		for_each_set_bit(engine_id, &engine_ids, 32U) {
			u32 mmu_id = nvgpu_engine_id_to_mmu_fault_id(g,
							(u32)engine_id);

			if (mmu_id != NVGPU_INVALID_ENG_ID) {
				mmu_fault_engines |= (u32)BIT(mmu_id);
			}
		}
	} else {
		/* store faulted engines in advance */
		for_each_set_bit(engine_id, &_engine_ids, 32U) {
			nvgpu_engine_get_id_and_type(g, (u32)engine_id,
						      &ref_id, &ref_type);
			if (ref_type == fifo_engine_status_id_type_tsgid_v()) {
				ref_id_is_tsg = true;
			} else {
				ref_id_is_tsg = false;
			}
			/*
			 * Reset *all* engines that use the
			 * same channel as faulty engine
			 */
			for (i = 0; i < g->fifo.num_engines; i++) {
				const struct nvgpu_device *dev =
					g->fifo.active_engines[i];
				u32 active_engine_id = dev->engine_id;
				u32 type;
				u32 id;

				nvgpu_engine_get_id_and_type(g,
					active_engine_id, &id, &type);
				if (ref_type == type && ref_id == id) {
					u32 mmu_id = nvgpu_engine_id_to_mmu_fault_id(g,
							active_engine_id);

					engine_ids |= BIT(active_engine_id);
					if (mmu_id != NVGPU_INVALID_ENG_ID) {
						mmu_fault_engines |= (u32)BIT(mmu_id);
					}
				}
			}
		}
	}

	if (mmu_fault_engines != 0U) {
		g->ops.fifo.intr_set_recover_mask(g);

		g->ops.fifo.trigger_mmu_fault(g, engine_ids);

		gk20a_fifo_handle_mmu_fault_locked(g, mmu_fault_engines,
				ref_id, ref_id_is_tsg);

		g->ops.fifo.intr_unset_recover_mask(g);
	}

	nvgpu_runlist_unlock_active_runlists(g);

	nvgpu_log_info(g, "release engines_reset_mutex");
	nvgpu_mutex_release(&g->fifo.engines_reset_mutex);
}
