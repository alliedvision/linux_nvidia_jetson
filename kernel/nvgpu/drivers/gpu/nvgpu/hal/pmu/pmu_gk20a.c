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

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu.h>
#include <nvgpu/falcon.h>
#include <nvgpu/firmware.h>
#include <nvgpu/pmu/mutex.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/pmu/debug.h>
#include <nvgpu/pmu/pmu_pg.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/mc.h>

#include <nvgpu/hw/gk20a/hw_pwr_gk20a.h>

#include "pmu_gk20a.h"

void gk20a_pmu_dump_falcon_stats(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = pmu->g;
	unsigned int i;

	for (i = 0; i < pwr_pmu_mailbox__size_1_v(); i++) {
		nvgpu_err(g, "pwr_pmu_mailbox_r(%d) : 0x%x",
			i, gk20a_readl(g, pwr_pmu_mailbox_r(i)));
	}

	for (i = 0; i < pwr_pmu_debug__size_1_v(); i++) {
		nvgpu_err(g, "pwr_pmu_debug_r(%d) : 0x%x",
			i, gk20a_readl(g, pwr_pmu_debug_r(i)));
	}

	i = gk20a_readl(g, pwr_pmu_bar0_error_status_r());
	nvgpu_err(g, "pwr_pmu_bar0_error_status_r : 0x%x", i);
	if (i != 0U) {
		nvgpu_err(g, "pwr_pmu_bar0_addr_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_addr_r()));
		nvgpu_err(g, "pwr_pmu_bar0_data_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_data_r()));
		nvgpu_err(g, "pwr_pmu_bar0_timeout_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_timeout_r()));
		nvgpu_err(g, "pwr_pmu_bar0_ctl_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_ctl_r()));
	}

	i = gk20a_readl(g, pwr_pmu_bar0_fecs_error_r());
	nvgpu_err(g, "pwr_pmu_bar0_fecs_error_r : 0x%x", i);

	i = gk20a_readl(g, pwr_falcon_exterrstat_r());
	nvgpu_err(g, "pwr_falcon_exterrstat_r : 0x%x", i);
	if (pwr_falcon_exterrstat_valid_v(i) ==
			pwr_falcon_exterrstat_valid_true_v()) {
		nvgpu_err(g, "pwr_falcon_exterraddr_r : 0x%x",
			gk20a_readl(g, pwr_falcon_exterraddr_r()));
	}
}

/* perfmon */
void gk20a_pmu_init_perfmon_counter(struct gk20a *g)
{
	u32 data;

	/* use counter #3 for GR && CE2 busy cycles */
	gk20a_writel(g, pwr_pmu_idle_mask_r(3),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	/* assign same mask setting from GR ELPG to counter #3 */
	data = gk20a_readl(g, pwr_pmu_idle_mask_1_supp_r(0));
	gk20a_writel(g, pwr_pmu_idle_mask_1_r(3), data);

	/* disable idle filtering for counters 3 and 6 */
	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(3));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_busy_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(3), data);

	/* use counter #6 for total cycles */
	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(6));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_always_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(6), data);

	/*
	 * We don't want to disturb counters #3 and #6, which are used by
	 * perfmon, so we add wiring also to counters #1 and #2 for
	 * exposing raw counter readings.
	 */
	gk20a_writel(g, pwr_pmu_idle_mask_r(1),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(1));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_busy_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(1), data);

	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(2));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_always_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(2), data);

	/*
	 * use counters 4 and 0 for perfmon to log busy cycles and total
	 * cycles counter #0 overflow sets pmu idle intr status bit
	 */
	gk20a_writel(g, pwr_pmu_idle_intr_r(),
		pwr_pmu_idle_intr_en_f(0));

	gk20a_writel(g, pwr_pmu_idle_threshold_r(0),
		pwr_pmu_idle_threshold_value_f(0x7FFFFFFF));

	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(0));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_always_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(0), data);

	gk20a_writel(g, pwr_pmu_idle_mask_r(4),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(4));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_busy_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(4), data);

	gk20a_writel(g, pwr_pmu_idle_count_r(0),
		pwr_pmu_idle_count_reset_f(1));
	gk20a_writel(g, pwr_pmu_idle_count_r(4),
		pwr_pmu_idle_count_reset_f(1));
	gk20a_writel(g, pwr_pmu_idle_intr_status_r(),
		pwr_pmu_idle_intr_status_intr_f(1));
}

void gk20a_pmu_pg_idle_counter_config(struct gk20a *g, u32 pg_engine_id)
{
	gk20a_writel(g, pwr_pmu_pg_idlefilth_r(pg_engine_id),
		PMU_PG_IDLE_THRESHOLD);
	gk20a_writel(g, pwr_pmu_pg_ppuidlefilth_r(pg_engine_id),
		PMU_PG_POST_POWERUP_IDLE_THRESHOLD);
}

u32 gk20a_pmu_read_idle_counter(struct gk20a *g, u32 counter_id)
{
	return pwr_pmu_idle_count_value_v(
		gk20a_readl(g, pwr_pmu_idle_count_r(counter_id)));
}

void gk20a_pmu_reset_idle_counter(struct gk20a *g, u32 counter_id)
{
	gk20a_writel(g, pwr_pmu_idle_count_r(counter_id),
		pwr_pmu_idle_count_reset_f(1));
}

u32 gk20a_pmu_read_idle_intr_status(struct gk20a *g)
{
	return pwr_pmu_idle_intr_status_intr_v(
		gk20a_readl(g, pwr_pmu_idle_intr_status_r()));
}

void gk20a_pmu_clear_idle_intr_status(struct gk20a *g)
{
	gk20a_writel(g, pwr_pmu_idle_intr_status_r(),
		pwr_pmu_idle_intr_status_intr_f(1));
}

/* ELPG */
void gk20a_pmu_dump_elpg_stats(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = pmu->g;

	nvgpu_pmu_dbg(g, "pwr_pmu_idle_mask_supp_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_mask_supp_r(3)));
	nvgpu_pmu_dbg(g, "pwr_pmu_idle_mask_1_supp_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_mask_1_supp_r(3)));
	nvgpu_pmu_dbg(g, "pwr_pmu_idle_ctrl_supp_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_ctrl_supp_r(3)));
	nvgpu_pmu_dbg(g, "pwr_pmu_pg_idle_cnt_r(0): 0x%08x",
		gk20a_readl(g, pwr_pmu_pg_idle_cnt_r(0)));
	nvgpu_pmu_dbg(g, "pwr_pmu_pg_intren_r(0): 0x%08x",
		gk20a_readl(g, pwr_pmu_pg_intren_r(0)));

	nvgpu_pmu_dbg(g, "pwr_pmu_idle_count_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_count_r(3)));
	nvgpu_pmu_dbg(g, "pwr_pmu_idle_count_r(4): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_count_r(4)));
	nvgpu_pmu_dbg(g, "pwr_pmu_idle_count_r(7): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_count_r(7)));
}

/* Muetx */
u32 gk20a_pmu_mutex_owner(struct gk20a *g, struct pmu_mutexes *mutexes, u32 id)
{
	struct pmu_mutex *mutex;

	mutex = &mutexes->mutex[id];

	return pwr_pmu_mutex_value_v(
		gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));
}

int gk20a_pmu_mutex_acquire(struct gk20a *g, struct pmu_mutexes *mutexes,
		u32 id, u32 *token)
{
	struct pmu_mutex *mutex;
	u32 data, owner, max_retry;
	int ret = -EBUSY;

	mutex = &mutexes->mutex[id];

	owner = pwr_pmu_mutex_value_v(
		gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));

	max_retry = 40;
	do {
		data = pwr_pmu_mutex_id_value_v(
			gk20a_readl(g, pwr_pmu_mutex_id_r()));
		if (data == pwr_pmu_mutex_id_value_init_v() ||
		    data == pwr_pmu_mutex_id_value_not_avail_v()) {
			nvgpu_warn(g,
				"fail to generate mutex token: val 0x%08x",
				owner);
			nvgpu_usleep_range(20, 40);
			continue;
		}

		owner = data;
		gk20a_writel(g, pwr_pmu_mutex_r(mutex->index),
			pwr_pmu_mutex_value_f(owner));

		data = pwr_pmu_mutex_value_v(
			gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));

		if (owner == data) {
			nvgpu_log_info(g, "mutex acquired: id=%d, token=0x%x",
				mutex->index, *token);
			*token = owner;
			ret = 0;
			break;
		}

		nvgpu_log_info(g, "fail to acquire mutex idx=0x%08x",
			mutex->index);

		data = gk20a_readl(g, pwr_pmu_mutex_id_release_r());
		data = set_field(data,
			pwr_pmu_mutex_id_release_value_m(),
			pwr_pmu_mutex_id_release_value_f(owner));
		gk20a_writel(g, pwr_pmu_mutex_id_release_r(), data);

		nvgpu_usleep_range(20, 40);
	} while (max_retry-- > 0U);

	return ret;
}

void gk20a_pmu_mutex_release(struct gk20a *g, struct pmu_mutexes *mutexes,
	u32 id, u32 *token)
{
	struct pmu_mutex *mutex;
	u32 owner, data;

	mutex = &mutexes->mutex[id];

	owner = pwr_pmu_mutex_value_v(
		gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));

	gk20a_writel(g, pwr_pmu_mutex_r(mutex->index),
		pwr_pmu_mutex_value_initial_lock_f());

	data = gk20a_readl(g, pwr_pmu_mutex_id_release_r());
	data = set_field(data, pwr_pmu_mutex_id_release_value_m(),
		pwr_pmu_mutex_id_release_value_f(owner));
	gk20a_writel(g, pwr_pmu_mutex_id_release_r(), data);

	nvgpu_log_info(g, "mutex released: id=%d, token=0x%x",
		mutex->index, *token);
}


/* queue */
int gk20a_pmu_queue_head(struct gk20a *g, u32 queue_id, u32 queue_index,
			u32 *head, bool set)
{
	u32 queue_head_size = 0;

	if (g->ops.pmu.pmu_get_queue_head_size != NULL) {
		queue_head_size = g->ops.pmu.pmu_get_queue_head_size();
	}

	BUG_ON((head == NULL) || (queue_head_size == 0U));

	if (PMU_IS_COMMAND_QUEUE(queue_id)) {

		if (queue_index >= queue_head_size) {
			return -EINVAL;
		}

		if (!set) {
			*head = pwr_pmu_queue_head_address_v(
				gk20a_readl(g,
				g->ops.pmu.pmu_get_queue_head(queue_index)));
		} else {
			gk20a_writel(g,
				g->ops.pmu.pmu_get_queue_head(queue_index),
				pwr_pmu_queue_head_address_f(*head));
		}
	} else {
		if (!set) {
			*head = pwr_pmu_msgq_head_val_v(
				gk20a_readl(g, pwr_pmu_msgq_head_r()));
		} else {
			gk20a_writel(g,
				pwr_pmu_msgq_head_r(),
				pwr_pmu_msgq_head_val_f(*head));
		}
	}

	return 0;
}

int gk20a_pmu_queue_tail(struct gk20a *g, u32 queue_id, u32 queue_index,
			u32 *tail, bool set)
{
	u32 queue_tail_size = 0;

	if (g->ops.pmu.pmu_get_queue_tail_size != NULL) {
		queue_tail_size = g->ops.pmu.pmu_get_queue_tail_size();
	}

	BUG_ON((tail == NULL) || (queue_tail_size == 0U));

	if (PMU_IS_COMMAND_QUEUE(queue_id)) {

		if (queue_index >= queue_tail_size) {
			return -EINVAL;
		}

		if (!set) {
			*tail = pwr_pmu_queue_tail_address_v(gk20a_readl(g,
					g->ops.pmu.pmu_get_queue_tail(queue_index)));
		} else {
			gk20a_writel(g,
				g->ops.pmu.pmu_get_queue_tail(queue_index),
				pwr_pmu_queue_tail_address_f(*tail));
		}

	} else {
		if (!set) {
			*tail = pwr_pmu_msgq_tail_val_v(
				gk20a_readl(g, pwr_pmu_msgq_tail_r()));
		} else {
			gk20a_writel(g,
				pwr_pmu_msgq_tail_r(),
				pwr_pmu_msgq_tail_val_f(*tail));
		}
	}

	return 0;
}

void gk20a_pmu_msgq_tail(struct nvgpu_pmu *pmu, u32 *tail, bool set)
{
	struct gk20a *g = pmu->g;
	u32 queue_tail_size = 0;

	if (g->ops.pmu.pmu_get_queue_tail_size != NULL) {
		queue_tail_size = g->ops.pmu.pmu_get_queue_tail_size();
	}

	BUG_ON((tail == NULL) || (queue_tail_size == 0U));

	if (!set) {
		*tail = pwr_pmu_msgq_tail_val_v(
			gk20a_readl(g, pwr_pmu_msgq_tail_r()));
	} else {
		gk20a_writel(g,
			pwr_pmu_msgq_tail_r(),
			pwr_pmu_msgq_tail_val_f(*tail));
	}
}

/* ISR */
u32 gk20a_pmu_get_irqdest(struct gk20a *g)
{
	u32 intr_dest;

	(void)g;

	/* dest 0=falcon, 1=host; level 0=irq0, 1=irq1 */
	intr_dest = pwr_falcon_irqdest_host_gptmr_f(0)    |
		pwr_falcon_irqdest_host_wdtmr_f(1)    |
		pwr_falcon_irqdest_host_mthd_f(0)     |
		pwr_falcon_irqdest_host_ctxsw_f(0)    |
		pwr_falcon_irqdest_host_halt_f(1)     |
		pwr_falcon_irqdest_host_exterr_f(0)   |
		pwr_falcon_irqdest_host_swgen0_f(1)   |
		pwr_falcon_irqdest_host_swgen1_f(0)   |
		pwr_falcon_irqdest_host_ext_f(0xff)   |
		pwr_falcon_irqdest_target_gptmr_f(1)  |
		pwr_falcon_irqdest_target_wdtmr_f(0)  |
		pwr_falcon_irqdest_target_mthd_f(0)   |
		pwr_falcon_irqdest_target_ctxsw_f(0)  |
		pwr_falcon_irqdest_target_halt_f(0)   |
		pwr_falcon_irqdest_target_exterr_f(0) |
		pwr_falcon_irqdest_target_swgen0_f(0) |
		pwr_falcon_irqdest_target_swgen1_f(0) |
		pwr_falcon_irqdest_target_ext_f(0xff);

	return intr_dest;
}

void gk20a_pmu_enable_irq(struct nvgpu_pmu *pmu, bool enable)
{
	struct gk20a *g = pmu->g;
	u32 intr_mask;
	u32 intr_dest;

	nvgpu_log_fn(g, " ");

	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_PMU, NVGPU_CIC_INTR_DISABLE);

	nvgpu_falcon_set_irq(pmu->flcn, false, 0x0, 0x0);

	if (enable) {
		intr_dest = g->ops.pmu.get_irqdest(g);
		/* 0=disable, 1=enable */
		intr_mask = pwr_falcon_irqmset_gptmr_f(1)  |
			pwr_falcon_irqmset_wdtmr_f(1)  |
			pwr_falcon_irqmset_mthd_f(0)   |
			pwr_falcon_irqmset_ctxsw_f(0)  |
			pwr_falcon_irqmset_halt_f(1)   |
			pwr_falcon_irqmset_exterr_f(1) |
			pwr_falcon_irqmset_swgen0_f(1) |
			pwr_falcon_irqmset_swgen1_f(1);

		nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_PMU,
						 NVGPU_CIC_INTR_ENABLE);

		nvgpu_falcon_set_irq(pmu->flcn, true, intr_mask, intr_dest);
	}

	nvgpu_log_fn(g, "done");
}

bool gk20a_pmu_is_interrupted(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = pmu->g;
	u32 servicedpmuint;

	servicedpmuint = pwr_falcon_irqstat_halt_true_f() |
			pwr_falcon_irqstat_exterr_true_f() |
			pwr_falcon_irqstat_swgen0_true_f();

	if ((gk20a_readl(g, pwr_falcon_irqstat_r()) & servicedpmuint) != 0U) {
		return true;
	}

	return false;
}

void gk20a_pmu_handle_interrupts(struct gk20a *g, u32 intr)
{
	struct nvgpu_pmu *pmu = g->pmu;
	bool recheck = false;
	int err = 0;

	if ((intr & pwr_falcon_irqstat_halt_true_f()) != 0U) {
		nvgpu_err(g, "pmu halt intr not implemented");
		nvgpu_pmu_dump_falcon_stats(pmu);
		if (nvgpu_readl(g, pwr_pmu_mailbox_r
				(PMU_MODE_MISMATCH_STATUS_MAILBOX_R)) ==
				PMU_MODE_MISMATCH_STATUS_VAL) {
			if (g->ops.pmu.dump_secure_fuses != NULL) {
				g->ops.pmu.dump_secure_fuses(g);
			}
		}
	}
	if ((intr & pwr_falcon_irqstat_exterr_true_f()) != 0U) {
		nvgpu_err(g,
			"pmu exterr intr not implemented. Clearing interrupt.");
		nvgpu_pmu_dump_falcon_stats(pmu);

		nvgpu_writel(g, pwr_falcon_exterrstat_r(),
			nvgpu_readl(g, pwr_falcon_exterrstat_r()) &
				~pwr_falcon_exterrstat_valid_m());
	}

	if (g->ops.pmu.handle_swgen1_irq != NULL) {
		g->ops.pmu.handle_swgen1_irq(g, intr);
	}

	if ((intr & pwr_falcon_irqstat_swgen0_true_f()) != 0U) {
		err = nvgpu_pmu_process_message(pmu);
		if (err != 0) {
			nvgpu_err(g, "nvgpu_pmu_process_message failed err=%d",
				err);
		}
		recheck = true;
	}

	if (recheck) {
		if (!nvgpu_pmu_queue_is_empty(&pmu->queues,
					      PMU_MESSAGE_QUEUE)) {
			nvgpu_writel(g, pwr_falcon_irqsset_r(),
				pwr_falcon_irqsset_swgen0_set_f());
		}
	}
}

static u32 pmu_bar0_host_tout_etype(u32 val)
{
	return (val != 0U) ? PMU_BAR0_HOST_WRITE_TOUT : PMU_BAR0_HOST_READ_TOUT;
}

static u32 pmu_bar0_fecs_tout_etype(u32 val)
{
	return (val != 0U) ? PMU_BAR0_FECS_WRITE_TOUT : PMU_BAR0_FECS_READ_TOUT;
}

static u32 pmu_bar0_cmd_hwerr_etype(u32 val)
{
	return (val != 0U) ? PMU_BAR0_CMD_WRITE_HWERR : PMU_BAR0_CMD_READ_HWERR;
}

static u32 pmu_bar0_fecserr_etype(u32 val)
{
	return (val != 0U) ? PMU_BAR0_WRITE_FECSERR : PMU_BAR0_READ_FECSERR;
}

static u32 pmu_bar0_hosterr_etype(u32 val)
{
	return (val != 0U) ? PMU_BAR0_WRITE_HOSTERR : PMU_BAR0_READ_HOSTERR;
}

/* error handler */
int gk20a_pmu_bar0_error_status(struct gk20a *g, u32 *bar0_status,
	u32 *etype)
{
	u32 val = 0;
	u32 err_status = 0;
	u32 err_cmd = 0;

	val = gk20a_readl(g, pwr_pmu_bar0_error_status_r());
	*bar0_status = val;
	if (val == 0U) {
		return 0;
	}

	err_cmd = val & pwr_pmu_bar0_error_status_err_cmd_m();

	if ((val & pwr_pmu_bar0_error_status_timeout_host_m()) != 0U) {
		*etype = pmu_bar0_host_tout_etype(err_cmd);
	} else if ((val & pwr_pmu_bar0_error_status_timeout_fecs_m()) != 0U) {
		*etype = pmu_bar0_fecs_tout_etype(err_cmd);
	} else if ((val & pwr_pmu_bar0_error_status_cmd_hwerr_m()) != 0U) {
		*etype = pmu_bar0_cmd_hwerr_etype(err_cmd);
	} else if ((val & pwr_pmu_bar0_error_status_fecserr_m()) != 0U) {
		*etype = pmu_bar0_fecserr_etype(err_cmd);
		err_status = gk20a_readl(g, pwr_pmu_bar0_fecs_error_r());
		/*
		 * BAR0_FECS_ERROR would only record the first error code if
		 * multiple FECS error happen. Once BAR0_FECS_ERROR is cleared,
		 * BAR0_FECS_ERROR can record the error code from FECS again.
		 * Writing status regiter to clear the FECS Hardware state.
		 */
		gk20a_writel(g, pwr_pmu_bar0_fecs_error_r(), err_status);
	} else if ((val & pwr_pmu_bar0_error_status_hosterr_m()) != 0U) {
		*etype = pmu_bar0_hosterr_etype(err_cmd);
		/*
		 * BAR0_HOST_ERROR would only record the first error code if
		 * multiple HOST error happen. Once BAR0_HOST_ERROR is cleared,
		 * BAR0_HOST_ERROR can record the error code from HOST again.
		 * Writing status regiter to clear the FECS Hardware state.
		 *
		 * Defining clear ops for host err as gk20a does not have
		 * status register for this.
		 */
		if (g->ops.pmu.pmu_clear_bar0_host_err_status != NULL) {
			g->ops.pmu.pmu_clear_bar0_host_err_status(g);
		}
	} else {
		nvgpu_err(g, "PMU bar0 status type is not found");
	}

	/* Writing Bar0 status regiter to clear the Hardware state */
	gk20a_writel(g, pwr_pmu_bar0_error_status_r(), val);
	return (-EIO);
}

/* non-secure boot */
int gk20a_pmu_ns_bootstrap(struct gk20a *g, struct nvgpu_pmu *pmu,
	u32 args_offset)
{
	struct mm_gk20a *mm = &g->mm;
	struct nvgpu_firmware *fw = NULL;
	struct pmu_ucode_desc *desc = NULL;
	u32 addr_code, addr_data, addr_load;
	u32 i, blocks;
	int err;
	u64 tmp_addr;

	nvgpu_log_fn(g, " ");

	fw = nvgpu_pmu_fw_desc_desc(g, pmu);
	desc = (struct pmu_ucode_desc *)(void *)fw->data;

	gk20a_writel(g, pwr_falcon_itfen_r(),
		gk20a_readl(g, pwr_falcon_itfen_r()) |
		pwr_falcon_itfen_ctxen_enable_f());
	tmp_addr = nvgpu_inst_block_addr(g, &mm->pmu.inst_block) >> 12;
	nvgpu_assert(u64_hi32(tmp_addr) == 0U);
	gk20a_writel(g, pwr_pmu_new_instblk_r(),
		pwr_pmu_new_instblk_ptr_f((u32)tmp_addr) |
		pwr_pmu_new_instblk_valid_f(1) |
		pwr_pmu_new_instblk_target_sys_coh_f());

	gk20a_writel(g, pwr_falcon_dmemc_r(0),
		pwr_falcon_dmemc_offs_f(0) |
		pwr_falcon_dmemc_blk_f(0)  |
		pwr_falcon_dmemc_aincw_f(1));

	addr_code = u64_lo32((pmu->fw->ucode.gpu_va +
			desc->app_start_offset +
			desc->app_resident_code_offset) >> 8) ;
	addr_data = u64_lo32((pmu->fw->ucode.gpu_va +
			desc->app_start_offset +
			desc->app_resident_data_offset) >> 8);
	addr_load = u64_lo32((pmu->fw->ucode.gpu_va +
			desc->bootloader_start_offset) >> 8);

	gk20a_writel(g, pwr_falcon_dmemd_r(0), GK20A_PMU_DMAIDX_UCODE);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_code);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_size);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_code_size);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_imem_entry);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_data);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_data_size);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_code);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), 0x1);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), args_offset);

	g->ops.pmu.write_dmatrfbase(g,
			addr_load - (desc->bootloader_imem_offset >> U32(8)));

	blocks = ((desc->bootloader_size + 0xFFU) & ~0xFFU) >> 8;

	for (i = 0; i < blocks; i++) {
		gk20a_writel(g, pwr_falcon_dmatrfmoffs_r(),
			desc->bootloader_imem_offset + (i << 8));
		gk20a_writel(g, pwr_falcon_dmatrffboffs_r(),
			desc->bootloader_imem_offset + (i << 8));
		gk20a_writel(g, pwr_falcon_dmatrfcmd_r(),
			pwr_falcon_dmatrfcmd_imem_f(1)  |
			pwr_falcon_dmatrfcmd_write_f(0) |
			pwr_falcon_dmatrfcmd_size_f(6)  |
			pwr_falcon_dmatrfcmd_ctxdma_f(GK20A_PMU_DMAIDX_UCODE));
	}

	err = nvgpu_falcon_bootstrap(g->pmu->flcn,
				     desc->bootloader_entry_point);

	gk20a_writel(g, pwr_falcon_os_r(), desc->app_version);

	return err;
}

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
bool gk20a_pmu_is_engine_in_reset(struct gk20a *g)
{
	bool status = false;

	status = g->ops.mc.is_enabled(g, NVGPU_UNIT_PWR);

	return status;
}

void gk20a_pmu_engine_reset(struct gk20a *g, bool do_reset)
{
	g->ops.mc.enable_units(g, NVGPU_UNIT_PWR, do_reset);
}
#endif

void gk20a_write_dmatrfbase(struct gk20a *g, u32 addr)
{
	gk20a_writel(g, pwr_falcon_dmatrfbase_r(), addr);
}

u32 gk20a_pmu_falcon_base_addr(void)
{
	return pwr_falcon_irqsset_r();
}

bool gk20a_is_pmu_supported(struct gk20a *g)
{
	(void)g;
	return true;
}
