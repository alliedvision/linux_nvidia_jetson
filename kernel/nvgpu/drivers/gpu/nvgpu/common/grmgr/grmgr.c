/*
 * GR MANAGER
 *
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/enabled.h>
#include <nvgpu/os_sched.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/grmgr.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/fbp.h>

int nvgpu_init_gr_manager(struct gk20a *g)
{
	u32 gpc_id;
	struct nvgpu_gpu_instance *gpu_instance = &g->mig.gpu_instance[0];
	struct nvgpu_gr_syspipe *gr_syspipe = &gpu_instance->gr_syspipe;
	u32 local_gpc_mask;
	u32 ffs_bit = 0U;
	u32 index;
	int err = 0;
	const struct nvgpu_device *gr_dev = NULL;

	if (g->ops.grmgr.load_timestamp_prod != NULL) {
		g->ops.grmgr.load_timestamp_prod(g);
	}
	/* Number of gpu instance is 1 for legacy mode */
	g->mig.max_gpc_count = g->ops.top.get_max_gpc_count(g);
	nvgpu_assert(g->mig.max_gpc_count > 0U);
	g->mig.gpc_count = g->ops.priv_ring.get_gpc_count(g);
	nvgpu_assert(g->mig.gpc_count > 0U);
	g->mig.num_gpu_instances = 1U;
	g->mig.is_nongr_engine_sharable = false;
	g->mig.max_fbps_count = nvgpu_fbp_get_max_fbps_count(g->fbp);

	gpu_instance->gpu_instance_id = 0U;
	gpu_instance->is_memory_partition_supported = false;
	gpu_instance->gpu_instance_type = NVGPU_MIG_TYPE_PHYSICAL;

	gr_syspipe->gr_instance_id = 0U;
	gr_syspipe->gr_syspipe_id = 0U;
	gr_syspipe->num_gpc = g->mig.gpc_count;
	gr_syspipe->gr_dev = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS, 0U);
	nvgpu_assert(gr_syspipe->gr_dev != NULL);

	if (g->ops.gr.config.get_gpc_mask != NULL) {
		gr_syspipe->gpc_mask = g->ops.gr.config.get_gpc_mask(g);
		nvgpu_assert(gr_syspipe->gpc_mask != 0U);
	} else {
		gr_syspipe->gpc_mask = nvgpu_safe_sub_u32(
			BIT32(gr_syspipe->num_gpc),
			1U);
	}

	if (g->ops.grmgr.discover_gpc_ids != NULL) {
		if (g->ops.grmgr.discover_gpc_ids(g,
				gr_syspipe->num_gpc,
				gr_syspipe->gpcs) != 0) {
			nvgpu_err(g, "discover_gpc_ids -failed");
			return -EINVAL;
		}
	} else {
		/*
		 * For tu104 and before chips,
		 * Local GPC Id = physical GPC Id = Logical GPC Id for
		 * non-floorswept config else physical gpcs are assigned
		 * serially and floorswept gpcs are skipped.
		 */
		local_gpc_mask = gr_syspipe->gpc_mask;
		for (gpc_id = 0U; gpc_id < gr_syspipe->num_gpc; gpc_id++) {
			gr_syspipe->gpcs[gpc_id].logical_id = gpc_id;
			nvgpu_assert(local_gpc_mask != 0U);
			ffs_bit = (u32)(nvgpu_ffs(local_gpc_mask) - 1U);
			local_gpc_mask &= ~(1U << ffs_bit);
			gr_syspipe->gpcs[gpc_id].physical_id = ffs_bit;
			gr_syspipe->gpcs[gpc_id].gpcgrp_id = 0U;
		}
		nvgpu_assert(local_gpc_mask == 0U);
	}

	g->mig.usable_gr_syspipe_count =
	nvgpu_device_count(g, NVGPU_DEVTYPE_GRAPHICS);
	if ((g->mig.usable_gr_syspipe_count == 0U) ||
			(g->mig.usable_gr_syspipe_count >=
			 NVGPU_MIG_MAX_ENGINES)) {
		nvgpu_err(g, "Usable GR engine syspipe"
			"count[%u] is more than[%u]! or "
			"No GR engine available on the device!",
			g->mig.usable_gr_syspipe_count,
			NVGPU_MIG_MAX_ENGINES);
		nvgpu_assert(g->mig.usable_gr_syspipe_count <
			NVGPU_MIG_MAX_ENGINES);
		return -EINVAL;
	}

	index = 0U;
	nvgpu_device_for_each(g, gr_dev, NVGPU_DEVTYPE_GRAPHICS) {
		g->mig.usable_gr_syspipe_instance_id[index] =
			gr_dev->inst_id;
		g->mig.usable_gr_syspipe_mask |=
			BIT32(gr_dev->inst_id);
		index = nvgpu_safe_add_u32(index, 1U);
	}

	if (g->ops.grmgr.get_gpcgrp_count != NULL) {
		g->ops.grmgr.get_gpcgrp_count(g);
	} else {
		g->mig.gpcgrp_gpc_count[0] = gr_syspipe->num_gpc;
	}

	if (g->ops.gr.init.get_max_subctx_count != NULL) {
		gr_syspipe->max_veid_count_per_tsg =
			g->ops.gr.init.get_max_subctx_count();
		nvgpu_assert(gr_syspipe->max_veid_count_per_tsg > 0U);
	} else {
		/*
		 * For vgpu, NvGpu has to rely on chip constant
		 * queried from nvgpu server.
		 * For legacy chips, g->fifo.max_subctx_count should be 0U.
		 */
		gr_syspipe->max_veid_count_per_tsg = g->fifo.max_subctx_count;
	}
	gr_syspipe->veid_start_offset = 0U;

	gpu_instance->num_lce = nvgpu_device_get_copies(g, gpu_instance->lce_devs,
							NVGPU_MIG_MAX_ENGINES);
	nvgpu_assert(gpu_instance->num_lce > 0U);

	g->mig.max_gr_sys_pipes_supported = 1U;
	g->mig.gr_syspipe_en_mask = 1U;
	g->mig.num_gr_sys_pipes_enabled = 1U;
	g->mig.recursive_ref_count = 0U;
	g->mig.cur_tid = -1;

	gpu_instance->fbp_en_mask = nvgpu_fbp_get_fbp_en_mask(g->fbp);
#ifdef CONFIG_NVGPU_NON_FUSA
	gpu_instance->num_fbp = nvgpu_fbp_get_num_fbps(g->fbp);
	gpu_instance->fbp_l2_en_mask = nvgpu_fbp_get_l2_en_mask(g->fbp);
#endif

	g->mig.current_gr_syspipe_id = NVGPU_MIG_INVALID_GR_SYSPIPE_ID;

	nvgpu_log(g, gpu_dbg_mig,
		"[%s] gpu_instance_id[%u] gr_instance_id[%u] "
			"gr_syspipe_id[%u] max_gpc_count[%u] num_gpc[%u] "
			"gr_engine_id[%u] max_veid_count_per_tsg[%u] "
			"veid_start_offset[%u] is_memory_partition_support[%d] "
			"num_lce[%u] max_fbps_count[%u] num_fbp[%u] "
			"fbp_en_mask [0x%x] ",
		(nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG) ?
			"MIG_Physical" : "Physical"),
		gpu_instance->gpu_instance_id,
		gr_syspipe->gr_instance_id,
		gr_syspipe->gr_syspipe_id,
		g->mig.max_gpc_count,
		gr_syspipe->num_gpc,
		gr_syspipe->gr_dev->engine_id,
		gr_syspipe->max_veid_count_per_tsg,
		gr_syspipe->veid_start_offset,
		gpu_instance->is_memory_partition_supported,
		gpu_instance->num_lce,
		g->mig.max_fbps_count,
		gpu_instance->num_fbp,
		gpu_instance->fbp_en_mask);

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		/* No need to bring GR out of reset */
		return 0;
	}

	/*
	 * HW design is such that if any GR engine is not out of reset (reset is
	 * not de-asserted), it still takes clock. So for iGPU, SW should bring
	 * all supported GR engines out of reset during gpu boot.
	 *
	 * This will help low power feature like elcg to engage
	 * correctly and improve dynamic power savings.
	 *
	 * for dGPU all GRs are out of reset by default by dev init.
	 * Thus, this change is needed for iGPU only.
	 */
	if (g->pci_class == 0U) {
		/* iGPU */
		gr_dev = NULL;
		nvgpu_device_for_each(g, gr_dev,
				NVGPU_DEVTYPE_GRAPHICS) {
			/*
			 * Skip for GR0 as it will be put
			 * out of reset later
			 */
			if (g->mig.usable_gr_syspipe_instance_id[0U] ==
						gr_dev->inst_id) {
				continue;
			}

			/* Bring other GR devices out of reset */
			err = nvgpu_mc_reset_dev(g, gr_dev);
			if (err != 0) {
				nvgpu_err(g, "GR%u reset failed",
					gr_dev->inst_id);
				return err;
			}
		}
	}

	return 0;
}

#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_MIG)
static void nvgpu_grmgr_acquire_gr_syspipe(struct gk20a *g, u32 gr_syspipe_id)
{
	g->mig.recursive_ref_count = nvgpu_safe_add_u32(
		g->mig.recursive_ref_count, 1U);

	if (g->mig.cur_tid == -1) {
		g->mig.current_gr_syspipe_id = gr_syspipe_id;
		g->mig.cur_tid = nvgpu_current_tid(g);
	} else {
		nvgpu_log(g, gpu_dbg_mig,
			"Repeated gr remap window acquire call from same "
				"thread tid[%d] requsted gr_syspipe_id[%u] "
				"current_gr_syspipe_id[%u] "
				"recursive_ref_count[%u]",
			g->mig.cur_tid, gr_syspipe_id,
			g->mig.current_gr_syspipe_id,
			g->mig.recursive_ref_count);
		nvgpu_assert((g->mig.cur_tid == nvgpu_current_tid(g)) &&
			(g->mig.current_gr_syspipe_id == gr_syspipe_id));
	}
}

static void nvgpu_grmgr_release_gr_syspipe(struct gk20a *g)
{
	g->mig.recursive_ref_count = nvgpu_safe_sub_u32(
		g->mig.recursive_ref_count, 1U);

	if (g->mig.recursive_ref_count == 0U) {
		g->mig.current_gr_syspipe_id = NVGPU_MIG_INVALID_GR_SYSPIPE_ID;
		g->mig.cur_tid = -1;
		nvgpu_mutex_release(&g->mig.gr_syspipe_lock);
	} else {
		nvgpu_log(g, gpu_dbg_mig,
			"Repeated gr remap window release call from same "
				"thread tid[%d] current_gr_syspipe_id[%u] "
				"recursive_ref_count[%u]",
			g->mig.cur_tid, g->mig.current_gr_syspipe_id,
			g->mig.recursive_ref_count);
		nvgpu_assert(g->mig.cur_tid == nvgpu_current_tid(g));
	}
}
#endif

int nvgpu_grmgr_config_gr_remap_window(struct gk20a *g,
		u32 gr_syspipe_id, bool enable)
{
	int err = 0;
#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_MIG)
	if (nvgpu_grmgr_is_multi_gr_enabled(g)) {
		/*
		 * GR remap window enable/disable sequence for a GR
		 * SYSPIPE PGRAPH programming:
		 * 1) Config_gr_remap_window (syspipe_index, enable).
		 * 2) Acquire gr_syspipe_lock.
		 * 3) HW write to enable the gr syspipe programming.
		 * 4) Return success.
		 * 5) Do GR programming belong to particular gr syspipe.
		 * 6) Config_gr_remap_window (syspipe_index, disable).
		 * 7) HW write to disable the gr syspipe programming.
		 * 8) Release the gr_syspipe_lock.
		 *
		 * GR remap window disable/enable request for legacy
		 * GR PGRAPH programming:
		 * 1) Config_gr_remap_window (invalid_syspipe_index, disable).
		 * 2) Acquire gr_syspipe_lock.
		 * 3) HW write to enable the legacy gr syspipe programming.
		 * 4) Return success.
		 * 5) Do legacy GR PGRAPH programming.
		 * 6) Config_gr_remap_window (invalid_syspipe_index, enable).
		 * 7) HW write to disable the legacy gr syspipe programming.
		 * 8) Release the gr_syspipe_lock.
		 */

		if ((gr_syspipe_id !=
				NVGPU_MIG_INVALID_GR_SYSPIPE_ID) &&
				((g->mig.usable_gr_syspipe_mask & BIT32(
					gr_syspipe_id)) == 0U)) {
			nvgpu_err(g, "Invalid param syspipe_id[%x] en_mask[%x]",
				gr_syspipe_id,
				g->mig.usable_gr_syspipe_mask);
			return -EINVAL;
		}

		if (enable && (g->mig.current_gr_syspipe_id ==
				NVGPU_MIG_INVALID_GR_SYSPIPE_ID) &&
				(gr_syspipe_id ==
				NVGPU_MIG_INVALID_GR_SYSPIPE_ID)) {
			nvgpu_warn(g,
				"Legacy GR PGRAPH window enable called before "
					"disable sequence call ");
			return -EPERM;
		}

		if (enable) {
			if ((gr_syspipe_id !=
					NVGPU_MIG_INVALID_GR_SYSPIPE_ID) &&
				(g->mig.cur_tid != nvgpu_current_tid(g))) {
				nvgpu_mutex_acquire(&g->mig.gr_syspipe_lock);
			}
		} else {
			if ((gr_syspipe_id ==
					NVGPU_MIG_INVALID_GR_SYSPIPE_ID) &&
				(g->mig.cur_tid != nvgpu_current_tid(g))) {
				nvgpu_mutex_acquire(&g->mig.gr_syspipe_lock);
			} else {
				gr_syspipe_id = 0U;
			}
		}

		nvgpu_log(g, gpu_dbg_mig,
			"[start]tid[%d] current_gr_syspipe_id[%u] "
				"requested_gr_syspipe_id[%u] enable[%d] "
				"recursive_ref_count[%u] ",
			g->mig.cur_tid, g->mig.current_gr_syspipe_id,
			gr_syspipe_id, enable, g->mig.recursive_ref_count);

		if (gr_syspipe_id != NVGPU_MIG_INVALID_GR_SYSPIPE_ID) {
			if (((g->mig.current_gr_syspipe_id ==
					NVGPU_MIG_INVALID_GR_SYSPIPE_ID) &&
					(g->mig.recursive_ref_count == 0U)) ||
					(!enable &&
					(g->mig.recursive_ref_count == 1U))) {
				err = g->ops.priv_ring.config_gr_remap_window(g,
					gr_syspipe_id, enable);
			}
		} else {
			nvgpu_log(g, gpu_dbg_mig,
				"Legacy GR PGRAPH window enable[%d] ",
				enable);
		}

		if (err != 0) {
			nvgpu_mutex_release(&g->mig.gr_syspipe_lock);
			nvgpu_err(g, "Failed [%d]", err);
			return err;
		}

		if (enable) {
			if ((gr_syspipe_id ==
					NVGPU_MIG_INVALID_GR_SYSPIPE_ID) &&
					(g->mig.current_gr_syspipe_id == 0U)) {
				nvgpu_grmgr_release_gr_syspipe(g);
			} else {
				nvgpu_grmgr_acquire_gr_syspipe(g,
					gr_syspipe_id);
			}
		} else {
			if (g->mig.current_gr_syspipe_id !=
					NVGPU_MIG_INVALID_GR_SYSPIPE_ID) {
				nvgpu_grmgr_release_gr_syspipe(g);
			} else {
				nvgpu_grmgr_acquire_gr_syspipe(g, 0U);
			}
		}
		nvgpu_log(g, gpu_dbg_mig,
			"[end]tid[%d] current_gr_syspipe_id[%u] "
				"requested_gr_syspipe_id[%u] enable[%d] "
				"recursive_ref_count[%u] ",
			g->mig.cur_tid, g->mig.current_gr_syspipe_id,
			gr_syspipe_id, enable, g->mig.recursive_ref_count);
	}
#else
	(void)g;
	(void)gr_syspipe_id;
	(void)enable;
#endif
	return err;
}

u32 nvgpu_grmgr_get_num_gr_instances(struct gk20a *g)
{
	/*
	 * There is only one gr engine instance per syspipe.
	 * Hence just return number of syspipes here.
	 */
	return g->mig.num_gr_sys_pipes_enabled;
}

static inline u32 nvgpu_grmgr_get_gpu_instance_id(struct gk20a *g,
		u32 gr_instance_id)
{
	u32 gpu_instance_id = 0U;

	if (nvgpu_grmgr_is_multi_gr_enabled(g)) {
		/* 0th entry is physical device gpu instance */
		gpu_instance_id = nvgpu_safe_add_u32(gr_instance_id, 1U);

		if (gpu_instance_id >= g->mig.num_gpu_instances) {
			nvgpu_err(g,
				"gpu_instance_id[%u] >= num_gpu_instances[%u]",
				gpu_instance_id, g->mig.num_gpu_instances);
			nvgpu_assert(
				gpu_instance_id < g->mig.num_gpu_instances);
			gpu_instance_id = 0U;
		}
	}

	nvgpu_log(g, gpu_dbg_mig, "gr_instance_id[%u] gpu_instance_id[%u]",
		gr_instance_id, gpu_instance_id);

	return gpu_instance_id;
}

u32 nvgpu_grmgr_get_gr_syspipe_id(struct gk20a *g, u32 gr_instance_id)
{
	struct nvgpu_gpu_instance *gpu_instance;
	struct nvgpu_gr_syspipe *gr_syspipe;
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	gpu_instance = &g->mig.gpu_instance[gpu_instance_id];
	gr_syspipe = &gpu_instance->gr_syspipe;

	return gr_syspipe->gr_syspipe_id;
}

u32 nvgpu_grmgr_get_gr_num_gpcs(struct gk20a *g, u32 gr_instance_id)
{
	struct nvgpu_gpu_instance *gpu_instance;
	struct nvgpu_gr_syspipe *gr_syspipe;
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	gpu_instance = &g->mig.gpu_instance[gpu_instance_id];
	gr_syspipe = &gpu_instance->gr_syspipe;

	return gr_syspipe->num_gpc;
}

u32 nvgpu_grmgr_get_gr_num_fbps(struct gk20a *g, u32 gr_instance_id)
{
	struct nvgpu_gpu_instance *gpu_instance;
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	gpu_instance = &g->mig.gpu_instance[gpu_instance_id];

	return gpu_instance->num_fbp;
}

u32 nvgpu_grmgr_get_gr_gpc_phys_id(struct gk20a *g, u32 gr_instance_id,
		u32 gpc_local_id)
{
	struct nvgpu_gpu_instance *gpu_instance;
	struct nvgpu_gr_syspipe *gr_syspipe;
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	gpu_instance = &g->mig.gpu_instance[gpu_instance_id];
	gr_syspipe = &gpu_instance->gr_syspipe;

	nvgpu_assert(gpc_local_id < gr_syspipe->num_gpc);

	nvgpu_log(g, gpu_dbg_mig,
		"gpu_instance_id[%u] gpc_local_id[%u] physical_id[%u]",
		gpu_instance_id, gpc_local_id,
		gr_syspipe->gpcs[gpc_local_id].physical_id);

	return gr_syspipe->gpcs[gpc_local_id].physical_id;
}

u32 nvgpu_grmgr_get_gr_gpc_logical_id(struct gk20a *g, u32 gr_instance_id,
		u32 gpc_local_id)
{
	struct nvgpu_gpu_instance *gpu_instance;
	struct nvgpu_gr_syspipe *gr_syspipe;
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	gpu_instance = &g->mig.gpu_instance[gpu_instance_id];
	gr_syspipe = &gpu_instance->gr_syspipe;

	nvgpu_assert(gpc_local_id < gr_syspipe->num_gpc);

	nvgpu_log(g, gpu_dbg_mig,
		"gpu_instance_id[%u] gpc_local_id[%u] logical_id[%u]",
		gpu_instance_id, gpc_local_id,
		gr_syspipe->gpcs[gpc_local_id].logical_id);

	return gr_syspipe->gpcs[gpc_local_id].logical_id;
}

u32 nvgpu_grmgr_get_gr_instance_id(struct gk20a *g, u32 gpu_instance_id)
{
	u32 gr_instance_id = 0U;

	/* TODO : Add gr_instance_id for physical device when MIG is enabled. */
	if ((nvgpu_grmgr_is_multi_gr_enabled(g)) &&
			(gpu_instance_id != 0U)) {
		if (gpu_instance_id < g->mig.num_gpu_instances) {
			/* 0th entry is physical device gpu instance */
			gr_instance_id = nvgpu_safe_sub_u32(
				gpu_instance_id, 1U);
		} else {
			nvgpu_err(g,
				"gpu_instance_id[%u] >= num_gpu_instances[%u]",
				gpu_instance_id, g->mig.num_gpu_instances);
			nvgpu_assert(
				gpu_instance_id < g->mig.num_gpu_instances);
		}
	}

	nvgpu_log(g, gpu_dbg_mig, "gpu_instance_id[%u] gr_instance_id[%u]",
		gpu_instance_id, gr_instance_id);

	return gr_instance_id;
}

bool nvgpu_grmgr_is_valid_runlist_id(struct gk20a *g,
		u32 gpu_instance_id, u32 runlist_id)
{
	if (gpu_instance_id < g->mig.num_gpu_instances) {
		struct nvgpu_gpu_instance *gpu_instance =
			&g->mig.gpu_instance[gpu_instance_id];
		struct nvgpu_gr_syspipe *gr_syspipe =
			&gpu_instance->gr_syspipe;
		const struct nvgpu_device *gr_dev = gr_syspipe->gr_dev;
		u32 id;

		if (gr_dev->runlist_id == runlist_id) {
			nvgpu_log(g, gpu_dbg_mig, "gr runlist found[%u]",
				runlist_id);
			return true;
		}

		for (id = 0U; id < gpu_instance->num_lce; id++) {
			const struct nvgpu_device *lce_dev =
				gpu_instance->lce_devs[id];
			if (lce_dev->runlist_id == runlist_id) {
				nvgpu_log(g, gpu_dbg_mig,
					"lce/ce runlist found[%u]",
					runlist_id);
				return true;
			}
		}
	}

	nvgpu_err(g,
		"gpu_instance_id[%u] >= num_gpu_instances[%u]",
		gpu_instance_id, g->mig.num_gpu_instances);

	return false;
}

u32 nvgpu_grmgr_get_gpu_instance_runlist_id(struct gk20a *g,
		u32 gpu_instance_id)
{
	if (gpu_instance_id < g->mig.num_gpu_instances) {
		struct nvgpu_gpu_instance *gpu_instance =
			&g->mig.gpu_instance[gpu_instance_id];
		struct nvgpu_gr_syspipe *gr_syspipe =
			&gpu_instance->gr_syspipe;
		const struct nvgpu_device *gr_dev = gr_syspipe->gr_dev;

		return gr_dev->runlist_id;
	}

	nvgpu_err(g,
		"gpu_instance_id[%u] >= num_gpu_instances[%u]",
		gpu_instance_id, g->mig.num_gpu_instances);

	return U32_MAX;
}

u32 nvgpu_grmgr_get_gr_instance_id_for_syspipe(struct gk20a *g,
		u32 gr_syspipe_id)
{
	if (nvgpu_grmgr_is_multi_gr_enabled(g)) {
		u32 gr_instance_id = 0U;
		u32 index;
		/* 0th entry is physical device gpu instance. */
		for (index = 1U; index < g->mig.num_gpu_instances; ++index) {
			struct nvgpu_gpu_instance *gpu_instance =
				&g->mig.gpu_instance[index];
			struct nvgpu_gr_syspipe *gr_syspipe =
				&gpu_instance->gr_syspipe;

			if (gr_syspipe->gr_syspipe_id == gr_syspipe_id) {
				nvgpu_log(g, gpu_dbg_mig,
					"gr_instance_id[%u] gr_syspipe_id[%u]",
					gr_instance_id, gr_syspipe_id);
				return gr_instance_id;
			}
			++gr_instance_id;
		}
	}

	/* Default gr_instance_id is 0U for legacy mode. */
	return 0U;
}

static u32 nvgpu_grmgr_get_max_veid_count(struct gk20a *g, u32 gpu_instance_id)
{
	struct nvgpu_gpu_instance *gpu_instance;
	struct nvgpu_gr_syspipe *gr_syspipe;

	if (gpu_instance_id < g->mig.num_gpu_instances) {
		gpu_instance = &g->mig.gpu_instance[gpu_instance_id];
		gr_syspipe = &gpu_instance->gr_syspipe;

		nvgpu_log(g, gpu_dbg_mig,
			"gpu_instance_id[%u] max_veid_count_per_tsg[%u]",
			gpu_instance_id, gr_syspipe->max_veid_count_per_tsg);

		return gr_syspipe->max_veid_count_per_tsg;
	}

	nvgpu_err(g,
		"gpu_instance_id[%u] >= num_gpu_instances[%u]",
		gpu_instance_id, g->mig.num_gpu_instances);

	return U32_MAX;
}

u32 nvgpu_grmgr_get_gpu_instance_max_veid_count(struct gk20a *g,
		u32 gpu_instance_id)
{
	return nvgpu_grmgr_get_max_veid_count(g, gpu_instance_id);
}

u32 nvgpu_grmgr_get_gr_max_veid_count(struct gk20a *g, u32 gr_instance_id)
{
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	return nvgpu_grmgr_get_max_veid_count(g, gpu_instance_id);
}

u32 nvgpu_grmgr_get_gr_logical_gpc_mask(struct gk20a *g, u32 gr_instance_id)
{
	u32 logical_gpc_mask = 0U;
	u32 gpc_indx;
	struct nvgpu_gpu_instance *gpu_instance;
	struct nvgpu_gr_syspipe *gr_syspipe;
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	gpu_instance = &g->mig.gpu_instance[gpu_instance_id];
	gr_syspipe = &gpu_instance->gr_syspipe;

	for (gpc_indx = 0U; gpc_indx < gr_syspipe->num_gpc; gpc_indx++) {
		logical_gpc_mask |= BIT32(
			gr_syspipe->gpcs[gpc_indx].logical_id);

		nvgpu_log(g, gpu_dbg_mig,
			"gpu_instance_id[%u] gr_instance_id[%u] gpc_indx[%u] "
				"logical_gpc_id[%u] logical_gpc_mask[%x]",
			gpu_instance_id, gr_instance_id, gpc_indx,
			gr_syspipe->gpcs[gpc_indx].logical_id,
			logical_gpc_mask);
	}

	return logical_gpc_mask;
}

u32 nvgpu_grmgr_get_gr_physical_gpc_mask(struct gk20a *g, u32 gr_instance_id)
{
	u32 physical_gpc_mask = 0U;
	u32 gpc_indx;
	struct nvgpu_gpu_instance *gpu_instance;
	struct nvgpu_gr_syspipe *gr_syspipe;
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	gpu_instance = &g->mig.gpu_instance[gpu_instance_id];
	gr_syspipe = &gpu_instance->gr_syspipe;

	for (gpc_indx = 0U; gpc_indx < gr_syspipe->num_gpc; gpc_indx++) {
		physical_gpc_mask |= BIT32(
			gr_syspipe->gpcs[gpc_indx].physical_id);

		nvgpu_log(g, gpu_dbg_mig,
			"gpu_instance_id[%u] gr_instance_id[%u] gpc_indx[%u] "
				"physical_id[%u] physical_gpc_mask[%x]",
			gpu_instance_id, gr_instance_id, gpc_indx,
			gr_syspipe->gpcs[gpc_indx].physical_id,
			physical_gpc_mask);
	}

	return physical_gpc_mask;
}

u32 nvgpu_grmgr_get_num_fbps(struct gk20a *g, u32 gpu_instance_id)
{
	struct nvgpu_gpu_instance *gpu_instance;

	if (gpu_instance_id < g->mig.num_gpu_instances) {
		gpu_instance = &g->mig.gpu_instance[gpu_instance_id];

		nvgpu_log(g, gpu_dbg_mig,
			"gpu_instance_id[%u] num_fbp[%u]",
			gpu_instance_id, gpu_instance->num_fbp);

		return gpu_instance->num_fbp;
	}

	nvgpu_err(g,
		"gpu_instance_id[%u] >= num_gpu_instances[%u]",
		gpu_instance_id, g->mig.num_gpu_instances);

	nvgpu_assert(gpu_instance_id < g->mig.num_gpu_instances);

	return U32_MAX;
}

u32 nvgpu_grmgr_get_fbp_en_mask(struct gk20a *g, u32 gpu_instance_id)
{
	struct nvgpu_gpu_instance *gpu_instance;

	if (gpu_instance_id < g->mig.num_gpu_instances) {
		gpu_instance = &g->mig.gpu_instance[gpu_instance_id];

		nvgpu_log(g, gpu_dbg_mig,
			"gpu_instance_id[%u] fbp_en_mask[0x%x]",
			gpu_instance_id, gpu_instance->fbp_en_mask);

		return gpu_instance->fbp_en_mask;
	}

	nvgpu_err(g,
		"gpu_instance_id[%u] >= num_gpu_instances[%u]",
		gpu_instance_id, g->mig.num_gpu_instances);

	nvgpu_assert(gpu_instance_id < g->mig.num_gpu_instances);

	return U32_MAX;
}

u32 nvgpu_grmgr_get_fbp_logical_id(struct gk20a *g, u32 gr_instance_id,
	u32 fbp_local_id)
{
	struct nvgpu_gpu_instance *gpu_instance;
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	if (gpu_instance_id >= g->mig.num_gpu_instances) {
		nvgpu_err(g,
			"gpu_instance_id[%u] >= g->mig.num_gpu_instances[%u]",
			fbp_local_id, g->mig.num_gpu_instances);

		nvgpu_assert(gpu_instance_id >= g->mig.num_gpu_instances);

		return U32_MAX;
	}

	gpu_instance = &g->mig.gpu_instance[gpu_instance_id];

	if (fbp_local_id < gpu_instance->num_fbp) {
		nvgpu_log(g, gpu_dbg_mig,
			"gpu_instance_id[%u], fbp_local_id[%u], fbp_physical_id[%u]",
			gpu_instance->gpu_instance_id, fbp_local_id,
			gpu_instance->fbp_mappings[fbp_local_id]);

		return gpu_instance->fbp_mappings[fbp_local_id];
	} else {
		nvgpu_err(g,
			"fbp_local_id[%u] >= gpu_instance->num_fbp[%u]",
			fbp_local_id, gpu_instance->num_fbp);

		nvgpu_assert(fbp_local_id >= gpu_instance->num_fbp);

		return U32_MAX;
	}
}

bool nvgpu_grmgr_get_memory_partition_support_status(struct gk20a *g,
	u32 gr_instance_id)
{
	struct nvgpu_gpu_instance *gpu_instance;
	u32 gpu_instance_id = nvgpu_grmgr_get_gpu_instance_id(
		g, gr_instance_id);

	if (gpu_instance_id >= g->mig.num_gpu_instances) {
		nvgpu_err(g,
			"gpu_instance_id[%u] >= g->mig.num_gpu_instances[%u]",
			gpu_instance_id, g->mig.num_gpu_instances);

		nvgpu_assert(gpu_instance_id >= g->mig.num_gpu_instances);

		return false;
	}

	gpu_instance = &g->mig.gpu_instance[gpu_instance_id];

	return gpu_instance->is_memory_partition_supported;
}

u32 *nvgpu_grmgr_get_fbp_l2_en_mask(struct gk20a *g, u32 gpu_instance_id)
{
	struct nvgpu_gpu_instance *gpu_instance;

	if (gpu_instance_id < g->mig.num_gpu_instances) {
		gpu_instance = &g->mig.gpu_instance[gpu_instance_id];

		return gpu_instance->fbp_l2_en_mask;
	}

	nvgpu_err(g,
		"gpu_instance_id[%u] >= num_gpu_instances[%u]",
		gpu_instance_id, g->mig.num_gpu_instances);

	nvgpu_assert(gpu_instance_id < g->mig.num_gpu_instances);

	return NULL;
}
