/*
 * Copyright (c) 2014-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/enabled.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/vgpu/vgpu_ivc.h>
#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/timers.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/string.h>
#include <nvgpu/ltc.h>
#include <nvgpu/cbc.h>
#include <nvgpu/fbp.h>
#include <nvgpu/cyclestats_snapshot.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/device.h>
#include <nvgpu/fb.h>
#include <nvgpu/nvs.h>

#include "init_vgpu.h"
#include "hal/vgpu/init/init_hal_vgpu.h"
#include "common/vgpu/gr/fecs_trace_vgpu.h"
#include "common/vgpu/fifo/fifo_vgpu.h"
#include "common/vgpu/mm/mm_vgpu.h"
#include "common/vgpu/gr/gr_vgpu.h"
#include "common/vgpu/fbp/fbp_vgpu.h"
#include "common/vgpu/ivc/comm_vgpu.h"

u64 vgpu_connect(void)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_connect_params *p = &msg.params.connect;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_CONNECT;
	p->module = TEGRA_VGPU_MODULE_GPU;
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));

	return (err || msg.ret) ? 0 : p->handle;
}

void vgpu_remove_support_common(struct gk20a *g)
{
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);
	struct tegra_vgpu_intr_msg msg;
	int err;

#ifdef CONFIG_NVGPU_DEBUGGER
	if (g->dbg_regops_tmp_buf) {
		nvgpu_kfree(g, g->dbg_regops_tmp_buf);
	}
#endif

	nvgpu_gr_remove_support(g);

	if (g->ops.grmgr.remove_gr_manager != NULL) {
		if (g->ops.grmgr.remove_gr_manager(g) != 0) {
			nvgpu_err(g, "g->ops.grmgr.remove_gr_manager-failed");
		}
	}

	if (g->fifo.remove_support) {
		g->fifo.remove_support(&g->fifo);
	}

#if defined(CONFIG_NVGPU_NON_FUSA)
	if (nvgpu_fb_vab_teardown_hal(g) != 0) {
		nvgpu_err(g, "failed to teardown VAB");
	}
#endif

	if (g->ops.mm.mmu_fault.info_mem_destroy != NULL) {
		g->ops.mm.mmu_fault.info_mem_destroy(g);
	}

	nvgpu_pmu_remove_support(g, g->pmu);

	if (g->mm.remove_support) {
		g->mm.remove_support(&g->mm);
	}

#if defined(CONFIG_NVGPU_CYCLESTATS)
	nvgpu_free_cyclestats_snapshot_data(g);
#endif

	nvgpu_fbp_remove_support(g);

	msg.event = TEGRA_VGPU_EVENT_ABORT;
	err = vgpu_ivc_send(vgpu_ivc_get_peer_self(), TEGRA_VGPU_QUEUE_INTR,
				&msg, sizeof(msg));
	WARN_ON(err);
	nvgpu_thread_stop(&priv->intr_handler);

	nvgpu_clk_arb_cleanup_arbiter(g);

	nvgpu_mutex_destroy(&g->clk_arb_enable_lock);
	nvgpu_mutex_destroy(&priv->vgpu_clk_get_freq_lock);

	nvgpu_kfree(g, priv->freqs);
}

int vgpu_init_gpu_characteristics(struct gk20a *g)
{
	int err;
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);

	nvgpu_log_fn(g, " ");

	err = nvgpu_init_gpu_characteristics(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init GPU characteristics");
		return err;
	}

	/* features vgpu does not support */
	nvgpu_set_enabled(g, NVGPU_SUPPORT_MAP_BUFFER_BATCH, false);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_RESCHEDULE_RUNLIST, false);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SPARSE_ALLOCS, false);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SET_CTX_MMU_DEBUG_MODE, false);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SM_TTU, priv->constants.support_sm_ttu != 0U);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_GET_GR_CONTEXT, false);

	/* per-device identifier */
	g->per_device_identifier = priv->constants.per_device_identifier;

	return 0;
}

int vgpu_get_constants(struct gk20a *g)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_constants_params *p;
	void *oob_handle;
	size_t oob_size;
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);
	int err;

	nvgpu_log_fn(g, " ");

	oob_handle = vgpu_ivc_oob_get_ptr(vgpu_ivc_get_server_vmid(),
			TEGRA_VGPU_QUEUE_CMD,
			(void **)&p, &oob_size);
	if (!oob_handle || oob_size < sizeof(*p)) {
		return -EINVAL;
	}

	msg.cmd = TEGRA_VGPU_CMD_GET_CONSTANTS;
	msg.handle = vgpu_get_handle(g);
	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;

	if (unlikely(err)) {
		nvgpu_err(g, "%s failed, err=%d", __func__, err);
		goto fail;
	}

	nvgpu_smp_rmb();

	if (unlikely(p->gpc_count > TEGRA_VGPU_MAX_GPC_COUNT ||
		p->max_tpc_per_gpc_count > TEGRA_VGPU_MAX_TPC_COUNT_PER_GPC)) {
		nvgpu_err(g, "gpc_count %d max_tpc_per_gpc %d overflow",
			(int)p->gpc_count, (int)p->max_tpc_per_gpc_count);
		err = -EINVAL;
		goto fail;
	}

	priv->constants = *p;
fail:
	vgpu_ivc_oob_put_ptr(oob_handle);
	return err;
}

int vgpu_finalize_poweron_common(struct gk20a *g)
{
	int err;

	nvgpu_log_fn(g, " ");

	vgpu_detect_chip(g);
	err = vgpu_init_hal(g);
	if (err != 0) {
		return err;
	}

	err = nvgpu_device_init(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init devices");
		return err;
	}

	err = nvgpu_init_ltc_support(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init ltc");
		return err;
	}

	err = vgpu_init_mm_support(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init gk20a mm");
		return err;
	}

	err = nvgpu_fifo_init_support(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init gk20a fifo");
		return err;
	}

	err = nvgpu_nvs_init(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init gk20a nvs");
		return err;
	}

	err = vgpu_fbp_init_support(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init gk20a fbp");
		return err;
	}

	err = g->ops.grmgr.init_gr_manager(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init gk20a grmgr");
		return err;
	}

	err = nvgpu_gr_alloc(g);
	if (err != 0) {
		nvgpu_err(g, "couldn't allocate gr memory");
		return err;
	}

	err = vgpu_init_gr_support(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init gk20a gr");
		return err;
	}

	err = nvgpu_clk_arb_init_arbiter(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init clk arb");
		return err;
	}

#ifdef CONFIG_NVGPU_COMPRESSION
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_COMPRESSION)) {
		err = nvgpu_cbc_init_support(g);
		if (err != 0) {
			nvgpu_err(g, "failed to init cbc");
			return err;
		}
	}
#endif

	err = g->ops.chip_init_gpu_characteristics(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init GPU characteristics");
		return err;
	}

	err = g->ops.channel.resume_all_serviceable_ch(g);
	if (err != 0) {
		nvgpu_err(g, "Failed to resume channels");
		return err;
	}

	return 0;
}
