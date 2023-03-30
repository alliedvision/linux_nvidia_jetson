/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/clk_arb.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/timers.h>
#include <nvgpu/boardobjgrp_e255.h>
#include <nvgpu/pmu/perf.h>

#ifdef __KERNEL__
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#include "os/linux/scale.h"
#endif
#endif

#include "clk_arb_gv100.h"

bool gv100_check_clk_arb_support(struct gk20a *g)
{
	if ((g->ops.clk_arb.get_arbiter_clk_domains != NULL) &&
			nvgpu_is_enabled(g, NVGPU_PMU_PSTATE)){
		return true;
	}
	else {
		return false;
	}
}

u32 gv100_get_arbiter_clk_domains(struct gk20a *g)
{
	(void)g;
	return (CTRL_CLK_DOMAIN_GPCCLK);
}

int gv100_get_arbiter_f_points(struct gk20a *g,u32 api_domain,
				u32 *num_points, u16 *freqs_in_mhz)
{
	return g->ops.clk.clk_domain_get_f_points(g,
		api_domain, num_points, freqs_in_mhz);
}

int gv100_get_arbiter_clk_range(struct gk20a *g, u32 api_domain,
		u16 *min_mhz, u16 *max_mhz)
{
	u32 clkwhich;
	struct nvgpu_pmu_perf_pstate_clk_info *p0_info;
	u16 max_min_freq_mhz;
	u16 limit_min_mhz;
	u16 gpcclk_cap_mhz;
	bool error_status = false;

	switch (api_domain) {
	case CTRL_CLK_DOMAIN_MCLK:
		clkwhich = CLKWHICH_MCLK;
		break;

	case CTRL_CLK_DOMAIN_GPCCLK:
		clkwhich = CLKWHICH_GPCCLK;
		break;

	default:
		error_status = true;
		break;
	}

	if (error_status == true) {
		return -EINVAL;
	}

	p0_info = nvgpu_pmu_perf_pstate_get_clk_set_info(g,
			CTRL_PERF_PSTATE_P0, clkwhich);
	if (p0_info == NULL) {
		return -EINVAL;
	}

	limit_min_mhz = p0_info->min_mhz;
	gpcclk_cap_mhz = p0_info->max_mhz;

	max_min_freq_mhz = nvgpu_pmu_clk_fll_get_min_max_freq(g);
	/*
	 * When DVCO min is 0 in vbios update it to DVCO_MIN_DEFAULT_MHZ.
	 */
	if (max_min_freq_mhz == 0U) {
		max_min_freq_mhz = DVCO_MIN_DEFAULT_MHZ;
	}

	/*
	 * Needed for DVCO min.
	 */
	if (api_domain == CTRL_CLK_DOMAIN_GPCCLK) {
		if ((max_min_freq_mhz != 0U) &&
			(max_min_freq_mhz >= limit_min_mhz)) {
			limit_min_mhz = nvgpu_safe_cast_u32_to_u16(
				nvgpu_safe_add_u32(max_min_freq_mhz, 1U));
		}
		if ((g->clk_arb->gpc_cap_clkmhz != 0U) &&
			(p0_info->max_mhz > g->clk_arb->gpc_cap_clkmhz )) {
			gpcclk_cap_mhz = g->clk_arb->gpc_cap_clkmhz;
		}
	}
	*min_mhz = limit_min_mhz;
	*max_mhz = gpcclk_cap_mhz;

	return 0;
}

int gv100_get_arbiter_clk_default(struct gk20a *g, u32 api_domain,
		u16 *default_mhz)
{
	u32 clkwhich;
	struct nvgpu_pmu_perf_pstate_clk_info *p0_info;
	bool error_status = false;
	u16 gpcclk_cap_mhz;

	switch (api_domain) {
	case CTRL_CLK_DOMAIN_MCLK:
		clkwhich = CLKWHICH_MCLK;
		break;

	case CTRL_CLK_DOMAIN_GPCCLK:
		clkwhich = CLKWHICH_GPCCLK;
		break;

	default:
		error_status = true;
		break;
	}

	if (error_status == true) {
		return -EINVAL;
	}

	p0_info = nvgpu_pmu_perf_pstate_get_clk_set_info(g,
			CTRL_PERF_PSTATE_P0, clkwhich);
	if (p0_info == NULL) {
		return -EINVAL;
	}

	gpcclk_cap_mhz = p0_info->max_mhz;
	if (api_domain == CTRL_CLK_DOMAIN_GPCCLK) {
		if ((g->clk_arb->gpc_cap_clkmhz != 0U) &&
			(p0_info->max_mhz > g->clk_arb->gpc_cap_clkmhz )) {
			gpcclk_cap_mhz = g->clk_arb->gpc_cap_clkmhz;
		}
	}
	*default_mhz = gpcclk_cap_mhz;

	return 0;
}

int gv100_init_clk_arbiter(struct gk20a *g)
{
	struct nvgpu_clk_arb *arb;
	u16 default_mhz;
	int err;
	int index;
	struct nvgpu_clk_vf_table *table;
	clk_arb_dbg(g, " ");

	if (g->clk_arb != NULL) {
		return 0;
	}
	arb = nvgpu_kzalloc(g, sizeof(struct nvgpu_clk_arb));
	if (arb == NULL) {
		return -ENOMEM;
	}

	nvgpu_mutex_init(&arb->pstate_lock);
	nvgpu_spinlock_init(&arb->sessions_lock);
	nvgpu_spinlock_init(&arb->users_lock);
	nvgpu_spinlock_init(&arb->requests_lock);

	arb->mclk_f_points = nvgpu_kcalloc(g, MAX_F_POINTS, sizeof(u16));
	if (arb->mclk_f_points == NULL) {
		err = -ENOMEM;
		goto init_fail;
	}

	arb->gpc2clk_f_points = nvgpu_kcalloc(g, MAX_F_POINTS, sizeof(u16));
	if (arb->gpc2clk_f_points == NULL) {
		err = -ENOMEM;
		goto init_fail;
	}

	for (index = 0; index < 2; index++) {
		table = &arb->vf_table_pool[index];
		table->gpc2clk_num_points = MAX_F_POINTS;
		table->mclk_num_points = MAX_F_POINTS;

		table->gpc2clk_points = nvgpu_kcalloc(g, MAX_F_POINTS,
			sizeof(struct nvgpu_clk_vf_point));
		if (table->gpc2clk_points == NULL) {
			err = -ENOMEM;
			goto init_fail;
		}


		table->mclk_points = nvgpu_kcalloc(g, MAX_F_POINTS,
			sizeof(struct nvgpu_clk_vf_point));
		if (table->mclk_points == NULL) {
			err = -ENOMEM;
			goto init_fail;
		}
	}

	g->clk_arb = arb;
	arb->g = g;

	err =  g->ops.clk_arb.get_arbiter_clk_default(g,
			CTRL_CLK_DOMAIN_MCLK, &default_mhz);
	if (err < 0) {
		err = -EINVAL;
		goto init_fail;
	}

	arb->mclk_default_mhz = default_mhz;

	err =  g->ops.clk_arb.get_arbiter_clk_default(g,
			CTRL_CLK_DOMAIN_GPCCLK, &default_mhz);
	if (err < 0) {
		err = -EINVAL;
		goto init_fail;
	}

	arb->gpc2clk_default_mhz = default_mhz;

	arb->actual = &arb->actual_pool[0];

	nvgpu_atomic_set(&arb->req_nr, 0);

	nvgpu_atomic64_set(&arb->alarm_mask, 0);
	err = nvgpu_clk_notification_queue_alloc(g, &arb->notification_queue,
		DEFAULT_EVENT_NUMBER);
	if (err < 0) {
		goto init_fail;
	}
	nvgpu_init_list_node(&arb->users);
	nvgpu_init_list_node(&arb->sessions);
	nvgpu_init_list_node(&arb->requests);

	(void)nvgpu_cond_init(&arb->request_wq);

	nvgpu_init_list_node(&arb->update_vf_table_work_item.worker_item);
	nvgpu_init_list_node(&arb->update_arb_work_item.worker_item);
	arb->update_vf_table_work_item.arb = arb;
	arb->update_arb_work_item.arb = arb;
	arb->update_vf_table_work_item.item_type = CLK_ARB_WORK_UPDATE_VF_TABLE;
	arb->update_arb_work_item.item_type = CLK_ARB_WORK_UPDATE_ARB;
	err = nvgpu_clk_arb_worker_init(g);
	if (err < 0) {
		goto init_fail;
	}

	if (g->dgpu_max_clk != 0U) {
		g->dgpu_max_clk = (g->dgpu_max_clk /
			FREQ_STEP_SIZE_MHZ) * FREQ_STEP_SIZE_MHZ;
		arb->gpc_cap_clkmhz = g->dgpu_max_clk;
	}
#ifdef CONFIG_DEBUG_FS
	arb->debug = &arb->debug_pool[0];

	if (!arb->debugfs_set) {
		if (nvgpu_clk_arb_debugfs_init(g))
			arb->debugfs_set = true;
	}
#endif
	err = nvgpu_clk_vf_point_cache(g);
	if (err < 0) {
		goto init_fail;
	}

	err = nvgpu_clk_arb_update_vf_table(arb);
	if (err < 0) {
		goto init_fail;
	}

	do {
		/* Check that first run is completed */
		nvgpu_smp_mb();
		NVGPU_COND_WAIT_INTERRUPTIBLE(&arb->request_wq,
			nvgpu_atomic_read(&arb->req_nr), 0U);
	} while (nvgpu_atomic_read(&arb->req_nr) == 0);
	return arb->status;

init_fail:
	nvgpu_kfree(g, arb->gpc2clk_f_points);
	nvgpu_kfree(g, arb->mclk_f_points);

	for (index = 0; index < 2; index++) {
		nvgpu_kfree(g, arb->vf_table_pool[index].gpc2clk_points);
		nvgpu_kfree(g, arb->vf_table_pool[index].mclk_points);
	}

	nvgpu_mutex_destroy(&arb->pstate_lock);
	nvgpu_kfree(g, arb);

	return err;
}

void gv100_clk_arb_run_arbiter_cb(struct nvgpu_clk_arb *arb)
{
	struct nvgpu_clk_session *session;
	struct nvgpu_clk_dev *dev;
	struct nvgpu_clk_dev *tmp;
	struct nvgpu_clk_arb_target *target, *actual;
	struct gk20a *g = arb->g;

	u32 current_pstate = VF_POINT_INVALID_PSTATE;
	u32 voltuv = 0;
	bool mclk_set, gpc2clk_set;
	u32 alarms_notified = 0;
	u32 current_alarm;
	int status = 0;
	/* Temporary variables for checking target frequency */
	u16 gpc2clk_target, mclk_target;
	struct nvgpu_clk_slave_freq vf_point;

#ifdef CONFIG_DEBUG_FS
	s64 t0, t1;
	struct nvgpu_clk_arb_debug *debug;

#endif

	clk_arb_dbg(g, " ");

	/* bail out if gpu is down */
	if (nvgpu_atomic64_read(&arb->alarm_mask) & EVENT(ALARM_GPU_LOST)) {
		goto exit_arb;
	}

#ifdef CONFIG_DEBUG_FS
	t0 = nvgpu_current_time_ns();
#endif

	/* Only one arbiter should be running */
	gpc2clk_target = 0;
	mclk_target = 0;
	nvgpu_spinlock_acquire(&arb->sessions_lock);
	nvgpu_list_for_each_entry(session, &arb->sessions,
			nvgpu_clk_session, link) {
		if (!session->zombie) {
			mclk_set = false;
			gpc2clk_set = false;
			target = (session->target == &session->target_pool[0] ?
					&session->target_pool[1] :
					&session->target_pool[0]);
			nvgpu_spinlock_acquire(&session->session_lock);
			if (!nvgpu_list_empty(&session->targets)) {
				/* Copy over state */
				target->mclk = session->target->mclk;
				target->gpc2clk = session->target->gpc2clk;
				/* Query the latest committed request */
				nvgpu_list_for_each_entry_safe(dev, tmp,
				 &session->targets, nvgpu_clk_dev, node) {
					if ((mclk_set == false) && (dev->mclk_target_mhz != 0U)) {
						target->mclk =
							dev->mclk_target_mhz;
						mclk_set = true;
					}
					if ((gpc2clk_set == false) &&
						(dev->gpc2clk_target_mhz != 0U)) {
						target->gpc2clk =
							dev->gpc2clk_target_mhz;
						gpc2clk_set = true;
					}
					nvgpu_ref_get(&dev->refcount);
					nvgpu_list_del(&dev->node);
					nvgpu_spinlock_acquire(
						&arb->requests_lock);
					nvgpu_list_add(
						&dev->node, &arb->requests);
					nvgpu_spinlock_release(&arb->requests_lock);
				}
				session->target = target;
			}
			nvgpu_spinlock_release(
				&session->session_lock);

			mclk_target = mclk_target > session->target->mclk ?
				mclk_target : session->target->mclk;

			gpc2clk_target =
				gpc2clk_target > session->target->gpc2clk ?
				gpc2clk_target : session->target->gpc2clk;
		}
	}
	nvgpu_spinlock_release(&arb->sessions_lock);

	gpc2clk_target = (gpc2clk_target > 0U) ? gpc2clk_target :
			arb->gpc2clk_default_mhz;

	if (gpc2clk_target < arb->gpc2clk_min) {
		gpc2clk_target = arb->gpc2clk_min;
	}

	if (gpc2clk_target > arb->gpc2clk_max) {
		gpc2clk_target = arb->gpc2clk_max;
	}

	mclk_target = (mclk_target > 0U) ? mclk_target :
			arb->mclk_default_mhz;

	if (mclk_target < arb->mclk_min) {
		mclk_target = arb->mclk_min;
	}

	if (mclk_target > arb->mclk_max) {
		mclk_target = arb->mclk_max;
	}

	if ((arb->gpc_cap_clkmhz != 0U) &&
			(gpc2clk_target > arb->gpc_cap_clkmhz)) {
		gpc2clk_target = arb->gpc_cap_clkmhz;
	}

#ifdef __KERNEL__
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	gpc2clk_target = gk20a_scale_clamp_clk_target(g, gpc2clk_target);
#endif
#endif

	vf_point.gpc_mhz = gpc2clk_target;
	(void)nvgpu_clk_arb_find_slave_points(arb, &vf_point);
	if (status != 0) {
		nvgpu_err(g, "Unable to get slave frequency");
		goto exit_arb;
	}

	status = nvgpu_pmu_perf_changeseq_set_clks(g, &vf_point);
	if (status != 0) {
		nvgpu_err(g, "Unable to program frequency");
		goto exit_arb;
	}

	actual = NV_READ_ONCE(arb->actual) == &arb->actual_pool[0] ?
			&arb->actual_pool[1] : &arb->actual_pool[0];

	/* do not reorder this pointer */
	nvgpu_smp_rmb();
	actual->gpc2clk = gpc2clk_target;
	actual->mclk = mclk_target;
	arb->voltuv_actual = voltuv;
	actual->pstate = current_pstate;
	arb->status = status;

	/* Make changes visible to other threads */
	nvgpu_smp_wmb();
	arb->actual = actual;

	/* status must be visible before atomic inc */
	nvgpu_smp_wmb();
	nvgpu_atomic_inc(&arb->req_nr);

	/* VF Update complete */
	nvgpu_clk_arb_set_global_alarm(g, EVENT(VF_UPDATE));

	nvgpu_cond_signal_interruptible(&arb->request_wq);
#ifdef CONFIG_DEBUG_FS
	t1 = nvgpu_current_time_ns();

	debug = arb->debug == &arb->debug_pool[0] ?
		&arb->debug_pool[1] : &arb->debug_pool[0];

	memcpy(debug, arb->debug, sizeof(arb->debug_pool[0]));
	debug->switch_num++;

	if (debug->switch_num == 1) {
		debug->switch_max = debug->switch_min =
			debug->switch_avg = (t1-t0)/1000;
		debug->switch_std = 0;
	} else {
		s64 prev_avg;
		s64 curr = (t1-t0)/1000;

		debug->switch_max = curr > debug->switch_max ?
			curr : debug->switch_max;
		debug->switch_min = debug->switch_min ?
			(curr < debug->switch_min ?
				curr : debug->switch_min) : curr;
		prev_avg = debug->switch_avg;
		debug->switch_avg = (curr +
			(debug->switch_avg * (debug->switch_num-1))) /
			debug->switch_num;
		debug->switch_std +=
			(curr - debug->switch_avg) * (curr - prev_avg);
	}
	/* commit changes before exchanging debug pointer */
	nvgpu_smp_wmb();
	arb->debug = debug;
#endif

exit_arb:
	if (status < 0) {
		nvgpu_err(g, "Error in arbiter update");
		nvgpu_clk_arb_set_global_alarm(g,
			EVENT(ALARM_CLOCK_ARBITER_FAILED));
	}

	current_alarm = (u32) nvgpu_atomic64_read(&arb->alarm_mask);
	/* notify completion for all requests */
	nvgpu_spinlock_acquire(&arb->requests_lock);
	nvgpu_list_for_each_entry_safe(dev, tmp, &arb->requests,
			nvgpu_clk_dev, node) {
		/* avoid casting composite expression below */
		u32 tmp_mask = NVGPU_POLLIN | NVGPU_POLLRDNORM;

		nvgpu_atomic_set(&dev->poll_mask, (int)tmp_mask);
		nvgpu_clk_arb_event_post_event(dev);
		nvgpu_list_del(&dev->node);
		nvgpu_ref_put(&dev->refcount, nvgpu_clk_arb_free_fd);
	}
	nvgpu_spinlock_release(&arb->requests_lock);

	nvgpu_atomic_set(&arb->notification_queue.head,
		nvgpu_atomic_read(&arb->notification_queue.tail));
	/* notify event for all users */
	nvgpu_spinlock_acquire(&arb->users_lock);
	nvgpu_list_for_each_entry(dev, &arb->users, nvgpu_clk_dev, link) {
		alarms_notified |=
			nvgpu_clk_arb_notify(dev, arb->actual, current_alarm);
	}
	nvgpu_spinlock_release(&arb->users_lock);

	/* clear alarms */
	nvgpu_clk_arb_clear_global_alarm(g, alarms_notified &
		~EVENT(ALARM_GPU_LOST));
}

void gv100_clk_arb_cleanup(struct nvgpu_clk_arb *arb)
{
	struct gk20a *g = arb->g;
	int index;

	nvgpu_kfree(g, arb->gpc2clk_f_points);
	nvgpu_kfree(g, arb->mclk_f_points);

	for (index = 0; index < 2; index++) {
		nvgpu_kfree(g,
			arb->vf_table_pool[index].gpc2clk_points);
		nvgpu_kfree(g, arb->vf_table_pool[index].mclk_points);
	}

	nvgpu_mutex_destroy(&g->clk_arb->pstate_lock);
	nvgpu_kfree(g, g->clk_arb);

	g->clk_arb = NULL;
}

void gv100_stop_clk_arb_threads(struct gk20a *g)
{
	nvgpu_clk_arb_worker_deinit(g);
}
