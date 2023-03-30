/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/enabled.h>
#include <nvgpu/power_features/cg.h>

static void nvgpu_cg_set_mode(struct gk20a *g, u32 cgmode, u32 mode_config)
{
	u32 n;
	u32 engine_id = 0;
	const struct nvgpu_device *dev = NULL;
	struct nvgpu_fifo *f = &g->fifo;

	nvgpu_log_fn(g, " ");

	for (n = 0; n < f->num_engines; n++) {
		dev = f->active_engines[n];

#ifdef CONFIG_NVGPU_NON_FUSA
		/* gr_engine supports both BLCG and ELCG */
		if ((cgmode == BLCG_MODE) &&
		    (dev->type == NVGPU_DEVTYPE_GRAPHICS)) {
			g->ops.therm.init_blcg_mode(g, (u32)mode_config,
						engine_id);
			break;
		} else
#endif
		if (cgmode == ELCG_MODE) {
			g->ops.therm.init_elcg_mode(g, (u32)mode_config,
						dev->engine_id);
		} else {
			nvgpu_err(g, "invalid cg mode %d, config %d for "
							"engine_id %d",
					cgmode, mode_config, engine_id);
		}
	}
}

void nvgpu_cg_elcg_enable_no_wait(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (g->elcg_enabled) {
		nvgpu_cg_set_mode(g, ELCG_MODE, ELCG_AUTO);
	}
	nvgpu_mutex_release(&g->cg_pg_lock);
}


void nvgpu_cg_elcg_disable_no_wait(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (g->elcg_enabled) {
		nvgpu_cg_set_mode(g, ELCG_MODE, ELCG_RUN);
	}
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_blcg_fb_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->blcg_enabled) {
		goto done;
	}
	if (g->ops.cg.blcg_fb_load_gating_prod != NULL) {
		g->ops.cg.blcg_fb_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_blcg_ltc_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->blcg_enabled) {
		goto done;
	}
	if (g->ops.cg.blcg_ltc_load_gating_prod != NULL) {
		g->ops.cg.blcg_ltc_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_blcg_fifo_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->blcg_enabled) {
		goto done;
	}
	if (g->ops.cg.blcg_fifo_load_gating_prod != NULL) {
		g->ops.cg.blcg_fifo_load_gating_prod(g, true);
	}
	if (g->ops.cg.blcg_runlist_load_gating_prod != NULL) {
		g->ops.cg.blcg_runlist_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_blcg_pmu_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->blcg_enabled) {
		goto done;
	}
	if (g->ops.cg.blcg_pmu_load_gating_prod != NULL) {
		g->ops.cg.blcg_pmu_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_blcg_ce_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->blcg_enabled) {
		goto done;
	}
	if (g->ops.cg.blcg_ce_load_gating_prod != NULL) {
		g->ops.cg.blcg_ce_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_blcg_gr_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->blcg_enabled) {
		goto done;
	}
	if (g->ops.cg.blcg_gr_load_gating_prod != NULL) {
		g->ops.cg.blcg_gr_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_slcg_fb_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}
	if (g->ops.cg.slcg_fb_load_gating_prod != NULL) {
		g->ops.cg.slcg_fb_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_slcg_ltc_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}
	if (g->ops.cg.slcg_ltc_load_gating_prod != NULL) {
		g->ops.cg.slcg_ltc_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

static void nvgpu_cg_slcg_priring_load_prod(struct gk20a *g, bool enable)
{

	if (g->ops.cg.slcg_priring_load_gating_prod != NULL) {
		g->ops.cg.slcg_priring_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_rs_ctrl_fbp_load_gating_prod != NULL) {
		g->ops.cg.slcg_rs_ctrl_fbp_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_rs_ctrl_gpc_load_gating_prod != NULL) {
		g->ops.cg.slcg_rs_ctrl_gpc_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_rs_ctrl_sys_load_gating_prod != NULL) {
		g->ops.cg.slcg_rs_ctrl_sys_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_rs_fbp_load_gating_prod != NULL) {
		g->ops.cg.slcg_rs_fbp_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_rs_gpc_load_gating_prod != NULL) {
		g->ops.cg.slcg_rs_gpc_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_rs_sys_load_gating_prod != NULL) {
		g->ops.cg.slcg_rs_sys_load_gating_prod(g, enable);
	}

}

void nvgpu_cg_slcg_priring_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}

	nvgpu_cg_slcg_priring_load_prod(g, true);
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_slcg_fifo_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}
	if (g->ops.cg.slcg_fifo_load_gating_prod != NULL) {
		g->ops.cg.slcg_fifo_load_gating_prod(g, true);
	}
	if (g->ops.cg.slcg_runlist_load_gating_prod != NULL) {
		g->ops.cg.slcg_runlist_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_slcg_pmu_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}
	if (g->ops.cg.slcg_pmu_load_gating_prod != NULL) {
		g->ops.cg.slcg_pmu_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_slcg_therm_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}
	if (g->ops.cg.slcg_therm_load_gating_prod != NULL) {
		g->ops.cg.slcg_therm_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_slcg_ce2_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}
	if (g->ops.cg.slcg_ce2_load_gating_prod != NULL) {
		g->ops.cg.slcg_ce2_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

#if defined(CONFIG_NVGPU_NON_FUSA)
void nvgpu_cg_slcg_timer_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}
	if (g->ops.cg.slcg_timer_load_gating_prod != NULL) {
		g->ops.cg.slcg_timer_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}
#endif

#ifdef CONFIG_NVGPU_PROFILER
void nvgpu_cg_slcg_perf_load_enable(struct gk20a *g, bool enable)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}

	if (g->ops.cg.slcg_perf_load_gating_prod != NULL) {
		g->ops.cg.slcg_perf_load_gating_prod(g, enable);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}
#endif

void nvgpu_cg_slcg_gsp_load_enable(struct gk20a *g, bool enable)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}

	if (g->ops.cg.slcg_gsp_load_gating_prod != NULL) {
		g->ops.cg.slcg_gsp_load_gating_prod(g, enable);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_slcg_ctrl_load_enable(struct gk20a *g, bool enable)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}

	if (g->ops.cg.slcg_ctrl_load_gating_prod != NULL) {
		g->ops.cg.slcg_ctrl_load_gating_prod(g, enable);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

static void cg_init_gr_slcg_load_gating_prod(struct gk20a *g)
{
	if (g->ops.cg.slcg_bus_load_gating_prod != NULL) {
		g->ops.cg.slcg_bus_load_gating_prod(g, true);
	}
	if (g->ops.cg.slcg_chiplet_load_gating_prod != NULL) {
		g->ops.cg.slcg_chiplet_load_gating_prod(g, true);
	}
	if (g->ops.cg.slcg_gr_load_gating_prod != NULL) {
		g->ops.cg.slcg_gr_load_gating_prod(g, true);
	}
	if (g->ops.cg.slcg_perf_load_gating_prod != NULL) {
		g->ops.cg.slcg_perf_load_gating_prod(g, true);
	}
	if (g->ops.cg.slcg_xbar_load_gating_prod != NULL) {
		g->ops.cg.slcg_xbar_load_gating_prod(g, true);
	}
	if (g->ops.cg.slcg_hshub_load_gating_prod != NULL) {
		g->ops.cg.slcg_hshub_load_gating_prod(g, true);
	}
}

static void cg_init_gr_blcg_load_gating_prod(struct gk20a *g)
{
	if (g->ops.cg.blcg_bus_load_gating_prod != NULL) {
		g->ops.cg.blcg_bus_load_gating_prod(g, true);
	}
	if (g->ops.cg.blcg_gr_load_gating_prod != NULL) {
		g->ops.cg.blcg_gr_load_gating_prod(g, true);
	}
	if (g->ops.cg.blcg_xbar_load_gating_prod != NULL) {
		g->ops.cg.blcg_xbar_load_gating_prod(g, true);
	}
	if (g->ops.cg.blcg_hshub_load_gating_prod != NULL) {
		g->ops.cg.blcg_hshub_load_gating_prod(g, true);
	}
}

void nvgpu_cg_init_gr_load_gating_prod(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);

	if (!g->slcg_enabled) {
		goto check_can_blcg;
	}

	cg_init_gr_slcg_load_gating_prod(g);

check_can_blcg:
	if (!g->blcg_enabled) {
		goto exit;
	}

	cg_init_gr_blcg_load_gating_prod(g);

exit:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

#ifdef CONFIG_NVGPU_NON_FUSA
void nvgpu_cg_elcg_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	g->ops.gr.init.wait_initialized(g);

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (g->elcg_enabled) {
		nvgpu_cg_set_mode(g, ELCG_MODE, ELCG_AUTO);
	}
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_elcg_disable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	g->ops.gr.init.wait_initialized(g);

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (g->elcg_enabled) {
		nvgpu_cg_set_mode(g, ELCG_MODE, ELCG_RUN);
	}
	nvgpu_mutex_release(&g->cg_pg_lock);

}

void nvgpu_cg_blcg_mode_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	g->ops.gr.init.wait_initialized(g);

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (g->blcg_enabled) {
		nvgpu_cg_set_mode(g, BLCG_MODE, BLCG_AUTO);
	}
	nvgpu_mutex_release(&g->cg_pg_lock);

}

void nvgpu_cg_blcg_mode_disable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	g->ops.gr.init.wait_initialized(g);

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (g->blcg_enabled) {
		nvgpu_cg_set_mode(g, BLCG_MODE, BLCG_RUN);
	}
	nvgpu_mutex_release(&g->cg_pg_lock);


}

void nvgpu_cg_slcg_gr_perf_ltc_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	g->ops.gr.init.wait_initialized(g);

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}
	if (g->ops.cg.slcg_ltc_load_gating_prod != NULL) {
		g->ops.cg.slcg_ltc_load_gating_prod(g, true);
	}
	if (g->ops.cg.slcg_perf_load_gating_prod != NULL) {
		g->ops.cg.slcg_perf_load_gating_prod(g, true);
	}
	if (g->ops.cg.slcg_gr_load_gating_prod != NULL) {
		g->ops.cg.slcg_gr_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_slcg_gr_perf_ltc_load_disable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	g->ops.gr.init.wait_initialized(g);

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->slcg_enabled) {
		goto done;
	}
	if (g->ops.cg.slcg_gr_load_gating_prod != NULL) {
		g->ops.cg.slcg_gr_load_gating_prod(g, false);
	}
	if (g->ops.cg.slcg_perf_load_gating_prod != NULL) {
		g->ops.cg.slcg_perf_load_gating_prod(g, false);
	}
	if (g->ops.cg.slcg_ltc_load_gating_prod != NULL) {
		g->ops.cg.slcg_ltc_load_gating_prod(g, false);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_elcg_set_elcg_enabled(struct gk20a *g, bool enable)
{
	nvgpu_log_fn(g, " ");

	g->ops.gr.init.wait_initialized(g);

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (enable) {
		if (!g->elcg_enabled) {
			g->elcg_enabled = true;
			nvgpu_cg_set_mode(g, ELCG_MODE, ELCG_AUTO);
		}
	} else {
		if (g->elcg_enabled) {
			g->elcg_enabled = false;
			nvgpu_cg_set_mode(g, ELCG_MODE, ELCG_RUN);
		}
	}
	if (g->ops.cg.elcg_ce_load_gating_prod != NULL) {
		g->ops.cg.elcg_ce_load_gating_prod(g, g->elcg_enabled);
	}
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_blcg_set_blcg_enabled(struct gk20a *g, bool enable)
{
	bool load = false;

	nvgpu_log_fn(g, " ");

	g->ops.gr.init.wait_initialized(g);

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (enable) {
		if (!g->blcg_enabled) {
			load = true;
			g->blcg_enabled = true;
		}
	} else {
		if (g->blcg_enabled) {
			load = true;
			g->blcg_enabled = false;
		}
	}
	if (!load ) {
		goto done;
	}

	if (g->ops.cg.blcg_bus_load_gating_prod != NULL) {
		g->ops.cg.blcg_bus_load_gating_prod(g, enable);
	}
	if (g->ops.cg.blcg_ce_load_gating_prod != NULL) {
		g->ops.cg.blcg_ce_load_gating_prod(g, enable);
	}
	if (g->ops.cg.blcg_fb_load_gating_prod != NULL) {
		g->ops.cg.blcg_fb_load_gating_prod(g, enable);
	}
	if (g->ops.cg.blcg_fifo_load_gating_prod != NULL) {
		g->ops.cg.blcg_fifo_load_gating_prod(g, enable);
	}
	if (g->ops.cg.blcg_gr_load_gating_prod != NULL) {
		g->ops.cg.blcg_gr_load_gating_prod(g, enable);
	}
	if (g->ops.cg.blcg_runlist_load_gating_prod != NULL) {
		g->ops.cg.blcg_runlist_load_gating_prod(g, enable);
	}
	if (g->ops.cg.blcg_ltc_load_gating_prod != NULL) {
		g->ops.cg.blcg_ltc_load_gating_prod(g, enable);
	}
	if (g->ops.cg.blcg_pmu_load_gating_prod != NULL) {
		g->ops.cg.blcg_pmu_load_gating_prod(g, enable);
	}
	if (g->ops.cg.blcg_xbar_load_gating_prod != NULL) {
		g->ops.cg.blcg_xbar_load_gating_prod(g, enable);
	}
	if (g->ops.cg.blcg_hshub_load_gating_prod != NULL) {
		g->ops.cg.blcg_hshub_load_gating_prod(g, enable);
	}

done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_slcg_set_slcg_enabled(struct gk20a *g, bool enable)
{
	bool load = false;

	nvgpu_log_fn(g, " ");

	g->ops.gr.init.wait_initialized(g);

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (enable) {
		if (!g->slcg_enabled) {
			load = true;
			g->slcg_enabled = true;
		}
	} else {
		if (g->slcg_enabled) {
			load = true;
			g->slcg_enabled = false;
		}
	}
	if (!load ) {
		goto done;
	}

	if (g->ops.cg.slcg_bus_load_gating_prod != NULL) {
		g->ops.cg.slcg_bus_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_ce2_load_gating_prod != NULL) {
		g->ops.cg.slcg_ce2_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_chiplet_load_gating_prod != NULL) {
		g->ops.cg.slcg_chiplet_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_fb_load_gating_prod != NULL) {
		g->ops.cg.slcg_fb_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_fifo_load_gating_prod != NULL) {
		g->ops.cg.slcg_fifo_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_runlist_load_gating_prod != NULL) {
		g->ops.cg.slcg_runlist_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_timer_load_gating_prod != NULL) {
		g->ops.cg.slcg_timer_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_gr_load_gating_prod != NULL) {
		g->ops.cg.slcg_gr_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_ltc_load_gating_prod != NULL) {
		g->ops.cg.slcg_ltc_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_perf_load_gating_prod != NULL) {
		g->ops.cg.slcg_perf_load_gating_prod(g, enable);
	}

	nvgpu_cg_slcg_priring_load_prod(g, enable);

	if (g->ops.cg.slcg_pmu_load_gating_prod != NULL) {
		g->ops.cg.slcg_pmu_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_xbar_load_gating_prod != NULL) {
		g->ops.cg.slcg_xbar_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_hshub_load_gating_prod != NULL) {
		g->ops.cg.slcg_hshub_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_ctrl_load_gating_prod != NULL) {
		g->ops.cg.slcg_ctrl_load_gating_prod(g, enable);
	}
	if (g->ops.cg.slcg_gsp_load_gating_prod != NULL) {
		g->ops.cg.slcg_gsp_load_gating_prod(g, enable);
	}

done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}

void nvgpu_cg_elcg_ce_load_enable(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->cg_pg_lock);
	if (!g->elcg_enabled) {
		goto done;
	}
	if (g->ops.cg.elcg_ce_load_gating_prod != NULL) {
		g->ops.cg.elcg_ce_load_gating_prod(g, true);
	}
done:
	nvgpu_mutex_release(&g->cg_pg_lock);
}
#endif
