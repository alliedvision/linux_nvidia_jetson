/*
 * Tegra GK20A GPU Debugger/Profiler Driver
 *
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/poll.h>
#include <uapi/linux/nvgpu.h>

#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/vm.h>
#include <nvgpu/atomic.h>
#include <nvgpu/cond.h>
#include <nvgpu/debugger.h>
#include <nvgpu/profiler.h>
#include <nvgpu/perfbuf.h>
#include <nvgpu/utils.h>
#include <nvgpu/mm.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/regops.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/preempt.h>
#include <nvgpu/string.h>

#include <nvgpu/linux/vm.h>

/* Access gr_gk20a_suspend_context and gr_gk20a_resume_context functions */
#include "hal/gr/gr/gr_gk20a.h"

#include "os_linux.h"
#include "platform_gk20a.h"
#include "ioctl_dbg.h"
#include "ioctl_channel.h"
#include "ioctl.h"
#include "dmabuf_priv.h"
#include "dmabuf_vidmem.h"

#include "common/gr/ctx_priv.h"

#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/grmgr.h>
#include <nvgpu/bug.h>

struct dbg_session_gk20a_linux {
	struct device	*dev;
	struct dbg_session_gk20a dbg_s;
};

struct dbg_session_channel_data_linux {
	/*
	 * We have to keep a ref to the _file_, not the channel, because
	 * close(channel_fd) is synchronous and would deadlock if we had an
	 * open debug session fd holding a channel ref at that time. Holding a
	 * ref to the file makes close(channel_fd) just drop a kernel ref to
	 * the file; the channel will close when the last file ref is dropped.
	 */
	struct file *ch_f;
	struct dbg_session_channel_data ch_data;
};
/* turn seriously unwieldy names -> something shorter */
#define REGOP_LINUX(x) NVGPU_DBG_GPU_REG_OP_##x

/* silly allocator - just increment id */
static nvgpu_atomic_t unique_id = NVGPU_ATOMIC_INIT(0);
static int generate_unique_id(void)
{
	return nvgpu_atomic_add_return(1, &unique_id);
}

static int alloc_session(struct gk20a *g, struct dbg_session_gk20a_linux **_dbg_s_linux)
{
	struct dbg_session_gk20a_linux *dbg_s_linux;
	*_dbg_s_linux = NULL;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	dbg_s_linux = nvgpu_kzalloc(g, sizeof(*dbg_s_linux));
	if (!dbg_s_linux)
		return -ENOMEM;

	dbg_s_linux->dbg_s.id = generate_unique_id();
	*_dbg_s_linux = dbg_s_linux;
	return 0;
}

static int gk20a_perfbuf_release_locked(struct gk20a *g,
		struct dbg_session_gk20a *dbg_s, u64 offset);

static int nvgpu_ioctl_channel_reg_ops(struct dbg_session_gk20a *dbg_s,
				struct nvgpu_dbg_gpu_exec_reg_ops_args *args);

static int nvgpu_ioctl_powergate_gk20a(struct dbg_session_gk20a *dbg_s,
				struct nvgpu_dbg_gpu_powergate_args *args);

static int nvgpu_dbg_gpu_ioctl_smpc_ctxsw_mode(struct dbg_session_gk20a *dbg_s,
			      struct nvgpu_dbg_gpu_smpc_ctxsw_mode_args *args);

static int nvgpu_dbg_gpu_ioctl_hwpm_ctxsw_mode(struct dbg_session_gk20a *dbg_s,
			      struct nvgpu_dbg_gpu_hwpm_ctxsw_mode_args *args);

static int nvgpu_dbg_gpu_ioctl_set_mmu_debug_mode(
		struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_set_ctx_mmu_debug_mode_args *args);

static int nvgpu_dbg_gpu_ioctl_suspend_resume_sm(
		struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_suspend_resume_all_sms_args *args);

static int nvgpu_ioctl_allocate_profiler_object(struct dbg_session_gk20a_linux *dbg_s,
				struct nvgpu_dbg_gpu_profiler_obj_mgt_args *args);

static int nvgpu_ioctl_free_profiler_object(struct dbg_session_gk20a_linux *dbg_s_linux,
				struct nvgpu_dbg_gpu_profiler_obj_mgt_args *args);

static int nvgpu_ioctl_profiler_reserve(struct dbg_session_gk20a *dbg_s,
			   struct nvgpu_dbg_gpu_profiler_reserve_args *args);

static int gk20a_perfbuf_map(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_perfbuf_map_args *args);

static int gk20a_perfbuf_unmap(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_perfbuf_unmap_args *args);

static int nvgpu_dbg_timeout_enable(struct dbg_session_gk20a *dbg_s,
			  int timeout_mode);

static int nvgpu_profiler_reserve_acquire(struct dbg_session_gk20a *dbg_s,
								u32 profiler_handle);

static void gk20a_dbg_session_nvgpu_mutex_acquire(struct dbg_session_gk20a *dbg_s);

static void gk20a_dbg_session_nvgpu_mutex_release(struct dbg_session_gk20a *dbg_s);

static int nvgpu_profiler_reserve_release(struct dbg_session_gk20a *dbg_s,
								u32 profiler_handle);

static int dbg_unbind_all_channels_gk20a(struct dbg_session_gk20a *dbg_s);

static int gk20a_dbg_gpu_do_dev_open(struct gk20a *g,
		struct file *filp, u32 gpu_instance_id, bool is_profiler);

static int nvgpu_dbg_get_context_buffer(struct gk20a *g, struct nvgpu_mem *ctx_mem,
		void __user *ctx_buf, u32 ctx_buf_size);

unsigned int gk20a_dbg_gpu_dev_poll(struct file *filep, poll_table *wait)
{
	unsigned int mask = 0;
	struct dbg_session_gk20a_linux *dbg_session_linux = filep->private_data;
	struct dbg_session_gk20a *dbg_s = &dbg_session_linux->dbg_s;
	struct gk20a *g = dbg_s->g;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	poll_wait(filep, &dbg_s->dbg_events.wait_queue.wq, wait);

	gk20a_dbg_session_nvgpu_mutex_acquire(dbg_s);

	if (dbg_s->dbg_events.events_enabled &&
			dbg_s->dbg_events.num_pending_events > 0) {
		nvgpu_log(g, gpu_dbg_gpu_dbg, "found pending event on session id %d",
				dbg_s->id);
		nvgpu_log(g, gpu_dbg_gpu_dbg, "%d events pending",
				dbg_s->dbg_events.num_pending_events);
		mask = (POLLPRI | POLLIN);
	}

	gk20a_dbg_session_nvgpu_mutex_release(dbg_s);

	return mask;
}

int gk20a_dbg_gpu_dev_release(struct inode *inode, struct file *filp)
{
	struct dbg_session_gk20a_linux *dbg_session_linux = filp->private_data;
	struct dbg_session_gk20a *dbg_s = &dbg_session_linux->dbg_s;
	struct gk20a *g = dbg_s->g;
	struct nvgpu_profiler_object *prof_obj, *tmp_obj;

	nvgpu_log(g, gpu_dbg_gpu_dbg | gpu_dbg_fn, "%s", g->name);

	/* unbind channels */
	dbg_unbind_all_channels_gk20a(dbg_s);

	/* Powergate/Timeout enable is called here as possibility of dbg_session
	 * which called powergate/timeout disable ioctl, to be killed without
	 * calling powergate/timeout enable ioctl
	 */
	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
	if (dbg_s->is_pg_disabled) {
		nvgpu_set_powergate_locked(dbg_s, false);
	}
	nvgpu_dbg_timeout_enable(dbg_s, NVGPU_DBG_GPU_IOCTL_TIMEOUT_ENABLE);

	/* If this session owned the perf buffer, release it */
	if (g->perfbuf.owner == dbg_s)
		gk20a_perfbuf_release_locked(g, dbg_s, g->perfbuf.offset);

	/* Per-context profiler objects were released when we called
	 * dbg_unbind_all_channels. We could still have global ones.
	 */
	nvgpu_list_for_each_entry_safe(prof_obj, tmp_obj, &g->profiler_objects,
				nvgpu_profiler_object, prof_obj_entry) {
		if (prof_obj->session_id == dbg_s->id) {
			nvgpu_profiler_pm_resource_release(prof_obj,
				NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY);
			nvgpu_profiler_free(prof_obj);
		}
	}
	dbg_s->gpu_instance_id = 0U;
	nvgpu_mutex_release(&g->dbg_sessions_lock);

	nvgpu_mutex_destroy(&dbg_s->ch_list_lock);
	nvgpu_mutex_destroy(&dbg_s->ioctl_lock);

	nvgpu_kfree(g, dbg_session_linux);
	nvgpu_put(g);

	return 0;
}

int gk20a_prof_gpu_dev_open(struct inode *inode, struct file *filp)
{
	struct gk20a *g;
	struct nvgpu_cdev *cdev;
	u32 gpu_instance_id;

	cdev = container_of(inode->i_cdev, struct nvgpu_cdev, cdev);
	g = nvgpu_get_gk20a_from_cdev(cdev);
	gpu_instance_id = nvgpu_get_gpu_instance_id_from_cdev(g, cdev);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");
	return gk20a_dbg_gpu_do_dev_open(g, filp, gpu_instance_id, true /* is profiler */);
}

static int nvgpu_dbg_gpu_ioctl_timeout(struct dbg_session_gk20a *dbg_s,
			 struct nvgpu_dbg_gpu_timeout_args *args)
{
	int err;
	struct gk20a *g = dbg_s->g;

	nvgpu_log(g, gpu_dbg_fn, "timeout enable/disable = %d", args->enable);

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
	err = nvgpu_dbg_timeout_enable(dbg_s, args->enable);
	nvgpu_mutex_release(&g->dbg_sessions_lock);

	return err;
}

static int nvgpu_dbg_gpu_ioctl_read_single_sm_error_state(
		struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_read_single_sm_error_state_args *args)
{
	struct gk20a *g = dbg_s->g;
	const struct nvgpu_tsg_sm_error_state *sm_error_state = NULL;
	struct nvgpu_dbg_gpu_sm_error_state_record sm_error_state_record;
	struct nvgpu_channel *ch;
	struct nvgpu_tsg *tsg;
	u32 sm_id;
	int err = 0;
	struct nvgpu_gr_config *gr_config =
		nvgpu_gr_get_gpu_instance_config_ptr(g, dbg_s->gpu_instance_id);

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (ch == NULL) {
		return -EINVAL;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		nvgpu_err(g, "no valid tsg from ch");
		return -EINVAL;
	}

	sm_id = args->sm_id;
	if (sm_id >= nvgpu_gr_config_get_no_of_sm(gr_config)) {
		return -EINVAL;
	}

	if (tsg->sm_error_states == NULL) {
		return -EINVAL;
	}

	nvgpu_speculation_barrier();

	sm_error_state = nvgpu_tsg_get_sm_error_state(tsg, sm_id);
	sm_error_state_record.hww_global_esr =
		sm_error_state->hww_global_esr;
	sm_error_state_record.hww_warp_esr =
		sm_error_state->hww_warp_esr;
	sm_error_state_record.hww_warp_esr_pc =
		sm_error_state->hww_warp_esr_pc;
	sm_error_state_record.hww_global_esr_report_mask =
		sm_error_state->hww_global_esr_report_mask;
	sm_error_state_record.hww_warp_esr_report_mask =
		sm_error_state->hww_warp_esr_report_mask;

	if (args->sm_error_state_record_size > 0) {
		size_t write_size = sizeof(*sm_error_state);

		nvgpu_speculation_barrier();
		if (write_size > args->sm_error_state_record_size)
			write_size = args->sm_error_state_record_size;

		nvgpu_mutex_acquire(&g->dbg_sessions_lock);
		err = copy_to_user((void __user *)(uintptr_t)
						args->sm_error_state_record_mem,
				   &sm_error_state_record,
				   write_size);
		nvgpu_mutex_release(&g->dbg_sessions_lock);
		if (err != 0) {
			nvgpu_err(g, "copy_to_user failed!");
			return err;
		}

		args->sm_error_state_record_size = write_size;
	}

	return 0;
}


static int nvgpu_dbg_gpu_ioctl_set_next_stop_trigger_type(
		struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_set_next_stop_trigger_type_args *args)
{
	struct gk20a *g = dbg_s->g;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	gk20a_dbg_session_nvgpu_mutex_acquire(dbg_s);

	dbg_s->broadcast_stop_trigger = (args->broadcast != 0);

	gk20a_dbg_session_nvgpu_mutex_release(dbg_s);

	return 0;
}

static int nvgpu_dbg_timeout_enable(struct dbg_session_gk20a *dbg_s,
			  int timeout_mode)
{
	struct gk20a *g = dbg_s->g;
	int err = 0;

	nvgpu_log(g, gpu_dbg_gpu_dbg, "Timeouts mode requested : %d",
			timeout_mode);

	nvgpu_speculation_barrier();
	switch (timeout_mode) {
	case NVGPU_DBG_GPU_IOCTL_TIMEOUT_ENABLE:
		if (dbg_s->is_timeout_disabled == true)
			nvgpu_atomic_dec(&g->timeouts_disabled_refcount);
		dbg_s->is_timeout_disabled = false;
		break;

	case NVGPU_DBG_GPU_IOCTL_TIMEOUT_DISABLE:
		if (dbg_s->is_timeout_disabled == false)
			nvgpu_atomic_inc(&g->timeouts_disabled_refcount);
		dbg_s->is_timeout_disabled = true;
		break;

	default:
		nvgpu_err(g,
			   "unrecognized dbg gpu timeout mode : 0x%x",
			   timeout_mode);
		err = -EINVAL;
		break;
	}

	if (!err)
		nvgpu_log(g, gpu_dbg_gpu_dbg, "dbg is timeout disabled %s, "
				"timeouts disabled refcount %d",
			dbg_s->is_timeout_disabled ? "true" : "false",
			nvgpu_atomic_read(&g->timeouts_disabled_refcount));
	return err;
}

static int gk20a_dbg_gpu_do_dev_open(struct gk20a *g,
		struct file *filp, u32 gpu_instance_id, bool is_profiler)
{
	struct dbg_session_gk20a_linux *dbg_session_linux;
	struct dbg_session_gk20a *dbg_s;
	struct device *dev;
	int err;

	g = nvgpu_get(g);
	if (!g)
		return -ENODEV;

	dev = dev_from_gk20a(g);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "dbg session: %s", g->name);

	err  = alloc_session(g, &dbg_session_linux);
	if (err)
		goto free_ref;

	dbg_s = &dbg_session_linux->dbg_s;

	filp->private_data = dbg_session_linux;
	dbg_session_linux->dev   = dev;
	dbg_s->g     = g;
	dbg_s->is_profiler = is_profiler;
	dbg_s->is_pg_disabled = false;
	dbg_s->is_timeout_disabled = false;
	dbg_s->gpu_instance_id = gpu_instance_id;

	nvgpu_cond_init(&dbg_s->dbg_events.wait_queue);
	nvgpu_init_list_node(&dbg_s->ch_list);
	nvgpu_mutex_init(&dbg_s->ch_list_lock);
	nvgpu_mutex_init(&dbg_s->ioctl_lock);
	dbg_s->dbg_events.events_enabled = false;
	dbg_s->dbg_events.num_pending_events = 0;

	return 0;

free_ref:
	nvgpu_put(g);
	return err;
}

void nvgpu_dbg_session_post_event(struct dbg_session_gk20a *dbg_s)
{
	nvgpu_cond_broadcast_interruptible(&dbg_s->dbg_events.wait_queue);
}

static int dbg_unbind_single_channel_gk20a(struct dbg_session_gk20a *dbg_s,
			struct dbg_session_channel_data *ch_data)
{
	struct gk20a *g = dbg_s->g;
	u32 chid;
	struct dbg_session_data *session_data;
	struct nvgpu_profiler_object *prof_obj, *tmp_obj;
	struct dbg_session_channel_data_linux *ch_data_linux;
	struct nvgpu_channel *ch;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	chid = ch_data->chid;
	ch = &g->fifo.channel[chid];

	/* If there's a profiler ctx reservation record associated with this
	 * session/channel pair, release it.
	 */
	nvgpu_list_for_each_entry_safe(prof_obj, tmp_obj, &g->profiler_objects,
				nvgpu_profiler_object, prof_obj_entry) {
		if ((prof_obj->session_id == dbg_s->id) &&
				(prof_obj->tsg->tsgid == ch->tsgid)) {
			nvgpu_profiler_pm_resource_release(prof_obj,
				NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY);
			nvgpu_profiler_free(prof_obj);
		}
	}

	nvgpu_list_del(&ch_data->ch_entry);

	session_data = ch_data->session_data;
	nvgpu_list_del(&session_data->dbg_s_entry);
	nvgpu_kfree(dbg_s->g, session_data);

	ch_data_linux = container_of(ch_data, struct dbg_session_channel_data_linux,
										ch_data);

	fput(ch_data_linux->ch_f);
	nvgpu_kfree(dbg_s->g, ch_data_linux);

	return 0;
}

static int dbg_bind_channel_gk20a(struct dbg_session_gk20a *dbg_s,
			  struct nvgpu_dbg_gpu_bind_channel_args *args)
{
	struct file *f;
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch;
	struct dbg_session_channel_data_linux *ch_data_linux;
	struct dbg_session_data *session_data;
	int err = 0;

	nvgpu_log(g, gpu_dbg_fn|gpu_dbg_gpu_dbg, "%s fd=%d",
		   g->name, args->channel_fd);

	/*
	 * Although nvgpu_channel_get_from_file gives us a channel ref, need to
	 * hold a ref to the file during the session lifetime. See comment in
	 * struct dbg_session_channel_data.
	 */
	f = fget(args->channel_fd);
	if (!f)
		return -ENODEV;

	ch = nvgpu_channel_get_from_file(args->channel_fd);
	if (!ch) {
		nvgpu_log_fn(g, "no channel found for fd");
		err = -EINVAL;
		goto out_fput;
	}

	nvgpu_log_fn(g, "%s hwchid=%d", g->name, ch->chid);

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
	nvgpu_mutex_acquire(&ch->dbg_s_lock);

	ch_data_linux = nvgpu_kzalloc(g, sizeof(*ch_data_linux));
	if (!ch_data_linux) {
		err = -ENOMEM;
		goto out_chput;
	}
	ch_data_linux->ch_f = f;
	ch_data_linux->ch_data.channel_fd = args->channel_fd;
	ch_data_linux->ch_data.chid = ch->chid;
	ch_data_linux->ch_data.unbind_single_channel = dbg_unbind_single_channel_gk20a;
	nvgpu_init_list_node(&ch_data_linux->ch_data.ch_entry);

	session_data = nvgpu_kzalloc(g, sizeof(*session_data));
	if (!session_data) {
		err = -ENOMEM;
		goto out_kfree;
	}
	session_data->dbg_s = dbg_s;
	nvgpu_init_list_node(&session_data->dbg_s_entry);
	ch_data_linux->ch_data.session_data = session_data;

	nvgpu_list_add(&session_data->dbg_s_entry, &ch->dbg_s_list);

	nvgpu_mutex_acquire(&dbg_s->ch_list_lock);
	nvgpu_list_add_tail(&ch_data_linux->ch_data.ch_entry, &dbg_s->ch_list);
	nvgpu_mutex_release(&dbg_s->ch_list_lock);

	nvgpu_mutex_release(&ch->dbg_s_lock);
	nvgpu_mutex_release(&g->dbg_sessions_lock);

	nvgpu_channel_put(ch);

	return 0;

out_kfree:
	nvgpu_kfree(g, ch_data_linux);
out_chput:
	nvgpu_channel_put(ch);
	nvgpu_mutex_release(&ch->dbg_s_lock);
	nvgpu_mutex_release(&g->dbg_sessions_lock);
out_fput:
	fput(f);
	return err;
}

static int dbg_unbind_all_channels_gk20a(struct dbg_session_gk20a *dbg_s)
{
	struct dbg_session_channel_data *ch_data, *tmp;
	struct gk20a *g = dbg_s->g;

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
	nvgpu_mutex_acquire(&dbg_s->ch_list_lock);
	nvgpu_list_for_each_entry_safe(ch_data, tmp, &dbg_s->ch_list,
				dbg_session_channel_data, ch_entry)
		ch_data->unbind_single_channel(dbg_s, ch_data);
	nvgpu_mutex_release(&dbg_s->ch_list_lock);
	nvgpu_mutex_release(&g->dbg_sessions_lock);

	return 0;
}

static int nvgpu_dbg_gpu_ioctl_tsg_set_timeslice(struct dbg_session_gk20a *dbg_s,
			struct nvgpu_timeslice_args *arg)
{
	struct nvgpu_tsg *tsg;
	struct nvgpu_channel *ch;
	struct gk20a *g = dbg_s->g;
	struct nvgpu_sched_ctrl *sched = &g->sched_ctrl;
	int err;

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (ch == NULL) {
		return -EINVAL;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		nvgpu_err(g, "no valid tsg from ch");
		return -EINVAL;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_sched, "tsgid=%u timeslice=%u",
				tsg->tsgid, arg->timeslice_us);

	nvgpu_mutex_acquire(&sched->control_lock);
	if (sched->control_locked) {
		err = -EPERM;
		goto done;
	}
	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to power on gpu");
		goto done;
	}
	err = g->ops.tsg.set_long_timeslice(tsg, arg->timeslice_us);
	gk20a_idle(g);
done:
	nvgpu_mutex_release(&sched->control_lock);
	return err;
}

static int nvgpu_dbg_gpu_ioctl_tsg_get_timeslice(struct dbg_session_gk20a *dbg_s,
			struct nvgpu_timeslice_args *arg)
{
	struct nvgpu_tsg *tsg;
	struct nvgpu_channel *ch;
	struct gk20a *g = dbg_s->g;

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (ch == NULL) {
		return -EINVAL;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		nvgpu_err(g, "no valid tsg from ch");
		return -EINVAL;
	}

	arg->timeslice_us = nvgpu_tsg_get_timeslice(tsg);
	return 0;
}

#ifdef CONFIG_NVGPU_DEBUGGER
/*
 * Convert common regops op values of the form of NVGPU_DBG_REG_OP_*
 * into linux regops op values of the form of NVGPU_DBG_GPU_REG_OP_*
 */
u32 nvgpu_get_regops_op_values_linux(u32 regops_op)
{
	switch (regops_op) {
	case REGOP(READ_32):
		return REGOP_LINUX(READ_32);
	case REGOP(WRITE_32):
		return REGOP_LINUX(WRITE_32);
	case REGOP(READ_64):
		return REGOP_LINUX(READ_64);
	case REGOP(WRITE_64):
		return REGOP_LINUX(WRITE_64);
	case REGOP(READ_08):
		return REGOP_LINUX(READ_08);
	case REGOP(WRITE_08):
		return REGOP_LINUX(WRITE_08);
	}

	return regops_op;
}

/*
 * Convert linux regops op values of the form of NVGPU_DBG_GPU_REG_OP_*
 * into common regops op values of the form of NVGPU_DBG_REG_OP_*
 */
u32 nvgpu_get_regops_op_values_common(u32 regops_op)
{
	switch (regops_op) {
	case REGOP_LINUX(READ_32):
		return REGOP(READ_32);
	case REGOP_LINUX(WRITE_32):
		return REGOP(WRITE_32);
	case REGOP_LINUX(READ_64):
		return REGOP(READ_64);
	case REGOP_LINUX(WRITE_64):
		return REGOP(WRITE_64);
	case REGOP_LINUX(READ_08):
		return REGOP(READ_08);
	case REGOP_LINUX(WRITE_08):
		return REGOP(WRITE_08);
	}

	return regops_op;
}

/*
 * Convert common regops type values of the form of NVGPU_DBG_REG_OP_TYPE_*
 * into linux regops type values of the form of NVGPU_DBG_GPU_REG_OP_TYPE_*
 */
static u32 nvgpu_get_regops_type_values_linux(u32 regops_type)
{
	switch (regops_type) {
	case REGOP(TYPE_GLOBAL):
		return REGOP_LINUX(TYPE_GLOBAL);
	case REGOP(TYPE_GR_CTX):
		return REGOP_LINUX(TYPE_GR_CTX);
	case REGOP(TYPE_GR_CTX_TPC):
		return REGOP_LINUX(TYPE_GR_CTX_TPC);
	case REGOP(TYPE_GR_CTX_SM):
		return REGOP_LINUX(TYPE_GR_CTX_SM);
	case REGOP(TYPE_GR_CTX_CROP):
		return REGOP_LINUX(TYPE_GR_CTX_CROP);
	case REGOP(TYPE_GR_CTX_ZROP):
		return REGOP_LINUX(TYPE_GR_CTX_ZROP);
	case REGOP(TYPE_GR_CTX_QUAD):
		return REGOP_LINUX(TYPE_GR_CTX_QUAD);
	}

	return regops_type;
}

/*
 * Convert linux regops type values of the form of NVGPU_DBG_GPU_REG_OP_TYPE_*
 * into common regops type values of the form of NVGPU_DBG_REG_OP_TYPE_*
 */
static u32 nvgpu_get_regops_type_values_common(u32 regops_type)
{
	switch (regops_type) {
	case REGOP_LINUX(TYPE_GLOBAL):
		return REGOP(TYPE_GLOBAL);
	case REGOP_LINUX(TYPE_GR_CTX):
		return REGOP(TYPE_GR_CTX);
	case REGOP_LINUX(TYPE_GR_CTX_TPC):
		return REGOP(TYPE_GR_CTX_TPC);
	case REGOP_LINUX(TYPE_GR_CTX_SM):
		return REGOP(TYPE_GR_CTX_SM);
	case REGOP_LINUX(TYPE_GR_CTX_CROP):
		return REGOP(TYPE_GR_CTX_CROP);
	case REGOP_LINUX(TYPE_GR_CTX_ZROP):
		return REGOP(TYPE_GR_CTX_ZROP);
	case REGOP_LINUX(TYPE_GR_CTX_QUAD):
		return REGOP(TYPE_GR_CTX_QUAD);
	}

	return regops_type;
}

/*
 * Convert common regops status values of the form of NVGPU_DBG_REG_OP_STATUS_*
 * into linux regops type values of the form of NVGPU_DBG_GPU_REG_OP_STATUS_*
 */
u32 nvgpu_get_regops_status_values_linux(u32 regops_status)
{
	switch (regops_status) {
	case REGOP(STATUS_SUCCESS):
		return REGOP_LINUX(STATUS_SUCCESS);
	case REGOP(STATUS_INVALID_OP):
		return REGOP_LINUX(STATUS_INVALID_OP);
	case REGOP(STATUS_INVALID_TYPE):
		return REGOP_LINUX(STATUS_INVALID_TYPE);
	case REGOP(STATUS_INVALID_OFFSET):
		return REGOP_LINUX(STATUS_INVALID_OFFSET);
	case REGOP(STATUS_UNSUPPORTED_OP):
		return REGOP_LINUX(STATUS_UNSUPPORTED_OP);
	case REGOP(STATUS_INVALID_MASK ):
		return REGOP_LINUX(STATUS_INVALID_MASK);
	}

	return regops_status;
}

/*
 * Convert linux regops status values of the form of NVGPU_DBG_GPU_REG_OP_STATUS_*
 * into common regops type values of the form of NVGPU_DBG_REG_OP_STATUS_*
 */
u32 nvgpu_get_regops_status_values_common(u32 regops_status)
{
	switch (regops_status) {
	case REGOP_LINUX(STATUS_SUCCESS):
		return REGOP(STATUS_SUCCESS);
	case REGOP_LINUX(STATUS_INVALID_OP):
		return REGOP(STATUS_INVALID_OP);
	case REGOP_LINUX(STATUS_INVALID_TYPE):
		return REGOP(STATUS_INVALID_TYPE);
	case REGOP_LINUX(STATUS_INVALID_OFFSET):
		return REGOP(STATUS_INVALID_OFFSET);
	case REGOP_LINUX(STATUS_UNSUPPORTED_OP):
		return REGOP(STATUS_UNSUPPORTED_OP);
	case REGOP_LINUX(STATUS_INVALID_MASK ):
		return REGOP(STATUS_INVALID_MASK);
	}

	return regops_status;
}

static int nvgpu_get_regops_data_common(struct nvgpu_dbg_gpu_reg_op *in,
		struct nvgpu_dbg_reg_op *out, u32 num_ops)
{
	u32 i;

	if(in == NULL || out == NULL)
		return -ENOMEM;

	for (i = 0; i < num_ops; i++) {
		out[i].op = nvgpu_get_regops_op_values_common(in[i].op);
		out[i].type = nvgpu_get_regops_type_values_common(in[i].type);
		out[i].status = nvgpu_get_regops_status_values_common(in[i].status);
		out[i].quad = in[i].quad;
		out[i].group_mask = in[i].group_mask;
		out[i].sub_group_mask = in[i].sub_group_mask;
		out[i].offset = in[i].offset;
		out[i].value_lo = in[i].value_lo;
		out[i].value_hi = in[i].value_hi;
		out[i].and_n_mask_lo = in[i].and_n_mask_lo;
		out[i].and_n_mask_hi = in[i].and_n_mask_hi;
	}

	return 0;
}

static int nvgpu_get_regops_data_linux(struct nvgpu_dbg_reg_op *in,
		struct nvgpu_dbg_gpu_reg_op *out, u32 num_ops)
{
	u32 i;

	if(in == NULL || out == NULL)
		return -ENOMEM;

	for (i = 0; i < num_ops; i++) {
		out[i].op = nvgpu_get_regops_op_values_linux(in[i].op);
		out[i].type = nvgpu_get_regops_type_values_linux(in[i].type);
		out[i].status = nvgpu_get_regops_status_values_linux(in[i].status);
		out[i].quad = in[i].quad;
		out[i].group_mask = in[i].group_mask;
		out[i].sub_group_mask = in[i].sub_group_mask;
		out[i].offset = in[i].offset;
		out[i].value_lo = in[i].value_lo;
		out[i].value_hi = in[i].value_hi;
		out[i].and_n_mask_lo = in[i].and_n_mask_lo;
		out[i].and_n_mask_hi = in[i].and_n_mask_hi;
	}

	return 0;
}

static int nvgpu_ioctl_channel_reg_ops(struct dbg_session_gk20a *dbg_s,
				struct nvgpu_dbg_gpu_exec_reg_ops_args *args)
{
	int err = 0, powergate_err = 0;
	bool is_pg_disabled = false;
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch;
	struct nvgpu_tsg *tsg = NULL;
	u32 flags = NVGPU_REG_OP_FLAG_MODE_ALL_OR_NONE;
	u32 gr_instance_id =
		nvgpu_grmgr_get_gr_instance_id(g, dbg_s->gpu_instance_id);

	nvgpu_log_fn(g, "%d ops, max fragment %d", args->num_ops, g->dbg_regops_tmp_buf_ops);

	if (args->num_ops > NVGPU_IOCTL_DBG_REG_OPS_LIMIT) {
		nvgpu_err(g, "regops limit exceeded");
		return -EINVAL;
	}

	if (args->num_ops == 0) {
		/* Nothing to do */
		return 0;
	}

	if (g->dbg_regops_tmp_buf_ops == 0 || !g->dbg_regops_tmp_buf) {
		nvgpu_err(g, "reg ops work buffer not allocated");
		return -ENODEV;
	}

	if (!dbg_s->id) {
		nvgpu_err(g, "can't call reg_ops on an unbound debugger session");
		return -EINVAL;
	}

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (!dbg_s->is_profiler && !ch) {
		nvgpu_err(g, "bind a channel before regops for a debugging session");
		return -EINVAL;
	}

	if (ch != NULL) {
		tsg = nvgpu_tsg_from_ch(ch);
		if (tsg == NULL) {
			nvgpu_err(g, "channel not bound to TSG");
			return -EINVAL;
		}
	}

	/* since exec_reg_ops sends methods to the ucode, it must take the
	 * global gpu lock to protect against mixing methods from debug sessions
	 * on other channels */
	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	if (!dbg_s->is_pg_disabled && !g->is_virtual) {
		/* In the virtual case, the server will handle
		 * disabling/enabling powergating when processing reg ops
		 */
		powergate_err = nvgpu_set_powergate_locked(dbg_s, true);
		if (!powergate_err) {
			is_pg_disabled = true;
		}
	}

	if (!powergate_err) {
		u64 ops_offset = 0; /* index offset */

		struct nvgpu_dbg_gpu_reg_op *linux_fragment = NULL;

		linux_fragment = nvgpu_kzalloc(g, g->dbg_regops_tmp_buf_ops *
				sizeof(struct nvgpu_dbg_gpu_reg_op));

		if (!linux_fragment)
			return -ENOMEM;

		while (ops_offset < args->num_ops && !err) {
			const u32 num_ops =
				min(args->num_ops - ops_offset,
				    (u64)(g->dbg_regops_tmp_buf_ops));
			const u64 fragment_size =
				num_ops * sizeof(struct nvgpu_dbg_gpu_reg_op);

			void __user *const fragment =
				(void __user *)(uintptr_t)
				(args->ops +
				 ops_offset * sizeof(struct nvgpu_dbg_gpu_reg_op));

			nvgpu_log_fn(g, "Regops fragment: start_op=%llu ops=%u",
				     ops_offset, num_ops);

			nvgpu_log_fn(g, "Copying regops from userspace");

			if (copy_from_user(linux_fragment,
					   fragment, fragment_size)) {
				nvgpu_err(g, "copy_from_user failed!");
				err = -EFAULT;
				break;
			}

			err = nvgpu_get_regops_data_common(linux_fragment,
					g->dbg_regops_tmp_buf, num_ops);

			if (err)
				break;

			err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
					nvgpu_regops_exec(g, tsg, NULL,
						g->dbg_regops_tmp_buf, num_ops, &flags));

			if (err) {
				break;
			}

			if (ops_offset == 0) {
				args->gr_ctx_resident =
					flags & NVGPU_REG_OP_FLAG_DIRECT_OPS;
			}

			err = nvgpu_get_regops_data_linux(g->dbg_regops_tmp_buf,
					linux_fragment, num_ops);

			if (err)
				break;

			nvgpu_log_fn(g, "Copying result to userspace");

			if (copy_to_user(fragment, linux_fragment,
					 fragment_size)) {
				nvgpu_err(g, "copy_to_user failed!");
				err = -EFAULT;
				break;
			}

			ops_offset += num_ops;
		}

		nvgpu_speculation_barrier();
		nvgpu_kfree(g, linux_fragment);

		/* enable powergate, if previously disabled */
		if (is_pg_disabled) {
			powergate_err =	nvgpu_set_powergate_locked(dbg_s,
									false);
		}
	}

	nvgpu_mutex_release(&g->dbg_sessions_lock);

	if (!err && powergate_err)
		err = powergate_err;

	if (err)
		nvgpu_err(g, "dbg regops failed");

	return err;
}
#endif /* CONFIG_NVGPU_DEBUGGER */

static int nvgpu_ioctl_powergate_gk20a(struct dbg_session_gk20a *dbg_s,
				struct nvgpu_dbg_gpu_powergate_args *args)
{
	int err;
	struct gk20a *g = dbg_s->g;
	nvgpu_log_fn(g, "%s  powergate mode = %d",
		      g->name, args->mode);

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
	if ((args->mode != NVGPU_DBG_GPU_POWERGATE_MODE_DISABLE) &&
	    (args->mode != NVGPU_DBG_GPU_POWERGATE_MODE_ENABLE)) {
		nvgpu_err(g, "invalid powergate mode");
		err = -EINVAL;
		goto pg_err_end;
	}

	err = nvgpu_set_powergate_locked(dbg_s,
			args->mode == NVGPU_DBG_GPU_POWERGATE_MODE_DISABLE);
pg_err_end:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	return  err;
}

static int nvgpu_dbg_gpu_ioctl_smpc_ctxsw_mode(struct dbg_session_gk20a *dbg_s,
			       struct nvgpu_dbg_gpu_smpc_ctxsw_mode_args *args)
{
	int err;
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch_gk20a;
	struct nvgpu_tsg *tsg;
	bool global_mode = false;
	u32 gr_instance_id =
		nvgpu_grmgr_get_gr_instance_id(g, dbg_s->gpu_instance_id);

	nvgpu_log_fn(g, "%s smpc ctxsw mode = %d",
		     g->name, args->mode);

	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to poweron");
		return err;
	}

	/* Take the global lock, since we'll be doing global regops */
	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	ch_gk20a = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (!ch_gk20a) {
		global_mode = true;
	}

	if (global_mode) {
		if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_SMPC_GLOBAL_MODE)) {
			nvgpu_err(g, "SMPC global mode not supported");
			err = -EINVAL;
			goto clean_up;
		}

		err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
				g->ops.gr.update_smpc_global_mode(g,
					args->mode == NVGPU_DBG_GPU_SMPC_CTXSW_MODE_CTXSW));
		if (err) {
			nvgpu_err(g,
				  "error (%d) during smpc global mode update", err);
		}
	} else {
		tsg = nvgpu_tsg_from_ch(ch_gk20a);
		if (tsg == NULL) {
			nvgpu_err(g, "channel not bound to TSG");
			err = -EINVAL;
			goto clean_up;
		}

		err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
				g->ops.gr.update_smpc_ctxsw_mode(g, tsg,
					args->mode == NVGPU_DBG_GPU_SMPC_CTXSW_MODE_CTXSW));
		if (err) {
			nvgpu_err(g,
				  "error (%d) during smpc ctxsw mode update", err);
		}
	}

 clean_up:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	gk20a_idle(g);
	return  err;
}

/*
 * Convert linux hwpm ctxsw mode type of the form of NVGPU_DBG_GPU_HWPM_CTXSW_MODE_*
 * into common hwpm ctxsw mode type of the form of NVGPU_DBG_HWPM_CTXSW_MODE_*
 */

static u32 nvgpu_hwpm_ctxsw_mode_to_common_mode(u32 mode)
{
	nvgpu_speculation_barrier();
	switch (mode){
	case NVGPU_DBG_GPU_HWPM_CTXSW_MODE_NO_CTXSW:
		return NVGPU_GR_CTX_HWPM_CTXSW_MODE_NO_CTXSW;
	case NVGPU_DBG_GPU_HWPM_CTXSW_MODE_CTXSW:
		return NVGPU_GR_CTX_HWPM_CTXSW_MODE_CTXSW;
	case NVGPU_DBG_GPU_HWPM_CTXSW_MODE_STREAM_OUT_CTXSW:
		return NVGPU_GR_CTX_HWPM_CTXSW_MODE_STREAM_OUT_CTXSW;
	}

	return mode;
}


static int nvgpu_dbg_gpu_ioctl_hwpm_ctxsw_mode(struct dbg_session_gk20a *dbg_s,
			       struct nvgpu_dbg_gpu_hwpm_ctxsw_mode_args *args)
{
	int err;
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch_gk20a;
	struct nvgpu_tsg *tsg;
	u32 mode = nvgpu_hwpm_ctxsw_mode_to_common_mode(args->mode);
	struct nvgpu_profiler_object *prof_obj, *tmp_obj;
	bool reserved = false;
	u32 gr_instance_id =
		nvgpu_grmgr_get_gr_instance_id(g, dbg_s->gpu_instance_id);

	nvgpu_log_fn(g, "%s pm ctxsw mode = %d", g->name, args->mode);

	/* Must have a valid reservation to enable/disable hwpm cxtsw.
	 * Just print an error message for now, but eventually this should
	 * return an error, at the point where all client sw has been
	 * cleaned up.
	 */
	nvgpu_list_for_each_entry_safe(prof_obj, tmp_obj, &g->profiler_objects,
				nvgpu_profiler_object, prof_obj_entry) {
		if (prof_obj->session_id == dbg_s->id) {
			if (prof_obj->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY]) {
				reserved = true;
			}
		}
	}

	if (!reserved) {
		nvgpu_err(g, "session doesn't have a valid reservation");
	}

	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to poweron");
		return err;
	}

	/* Take the global lock, since we'll be doing global regops */
	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	ch_gk20a = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (!ch_gk20a) {
		nvgpu_err(g,
			  "no bound channel for pm ctxsw mode update");
		err = -EINVAL;
		goto clean_up;
	}
	if (g->dbg_powergating_disabled_refcount == 0) {
		nvgpu_err(g, "powergate is not disabled");
		err = -ENOSYS;
		goto clean_up;
	}

	tsg = nvgpu_tsg_from_ch(ch_gk20a);
	if (tsg == NULL) {
		nvgpu_err(g, "channel not bound to TSG");
		err = -EINVAL;
		goto clean_up;
	}

	err = g->ops.gr.update_hwpm_ctxsw_mode(g, gr_instance_id, tsg, mode);

	if (err)
		nvgpu_err(g,
			"error (%d) during pm ctxsw mode update", err);
	/* gk20a would require a fix to set the core PM_ENABLE bit, not
	 * added here with gk20a being deprecated
	 */
 clean_up:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	gk20a_idle(g);
	return  err;
}

static int nvgpu_dbg_gpu_ioctl_set_mmu_debug_mode(
		struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_set_ctx_mmu_debug_mode_args *args)
{
	int err;
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch;
	bool enable = (args->mode == NVGPU_DBG_GPU_CTX_MMU_DEBUG_MODE_ENABLED);
	u32 gr_instance_id =
		nvgpu_grmgr_get_gr_instance_id(g, dbg_s->gpu_instance_id);

	nvgpu_log_fn(g, "mode=%u", args->mode);

	if (args->reserved != 0U) {
		return -EINVAL;
	}

	if ((g->ops.fb.set_mmu_debug_mode == NULL) &&
		(g->ops.gr.set_mmu_debug_mode == NULL)) {
		return -ENOSYS;
	}

	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to poweron");
		return err;
	}

	/* Take the global lock, since we'll be doing global regops */
	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (!ch) {
		nvgpu_err(g, "no bound channel for mmu debug mode");
		err = -EINVAL;
		goto clean_up;
	}

	err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
			nvgpu_tsg_set_mmu_debug_mode(ch, enable));
	if (err) {
		nvgpu_err(g, "set mmu debug mode failed, err=%d", err);
	}

clean_up:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	gk20a_idle(g);
	return err;
}

static int nvgpu_dbg_gpu_ioctl_suspend_resume_sm(
		struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_suspend_resume_all_sms_args *args)
{
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch;
	int err = 0, action = args->mode;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "action: %d", args->mode);

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (!ch)
		return -EINVAL;

	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to poweron");
		return err;
	}

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	/* Suspend GPU context switching */
	err = nvgpu_gr_disable_ctxsw(g);
	if (err) {
		nvgpu_err(g, "unable to stop gr ctxsw");
		/* this should probably be ctx-fatal... */
		goto clean_up;
	}

	nvgpu_speculation_barrier();
	switch (action) {
	case NVGPU_DBG_GPU_SUSPEND_ALL_SMS:
		gr_gk20a_suspend_context(ch);
		break;

	case NVGPU_DBG_GPU_RESUME_ALL_SMS:
		gr_gk20a_resume_context(ch);
		break;
	}

	err = nvgpu_gr_enable_ctxsw(g);
	if (err)
		nvgpu_err(g, "unable to restart ctxsw!");

clean_up:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	gk20a_idle(g);

	return  err;
}

static int nvgpu_ioctl_allocate_profiler_object(
				struct dbg_session_gk20a_linux *dbg_session_linux,
				struct nvgpu_dbg_gpu_profiler_obj_mgt_args *args)
{
	int err = 0;
	struct dbg_session_gk20a *dbg_s = &dbg_session_linux->dbg_s;
	struct gk20a *g = get_gk20a(dbg_session_linux->dev);
	struct nvgpu_profiler_object *prof_obj;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg;
	enum nvgpu_profiler_pm_reservation_scope scope;

	nvgpu_log_fn(g, "%s", g->name);

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	if (!dbg_s->is_profiler) {
		ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
		if (ch == NULL) {
			nvgpu_err(g, "no channel for dbg session");
			err = -EINVAL;
			goto clean_up;
		}
	}

	if (ch != NULL) {
		scope = NVGPU_PROFILER_PM_RESERVATION_SCOPE_CONTEXT;
	} else {
		scope = NVGPU_PROFILER_PM_RESERVATION_SCOPE_DEVICE;
	}

	err = nvgpu_profiler_alloc(g, &prof_obj, scope, dbg_s->gpu_instance_id);
	if (err != 0) {
		goto clean_up;
	}

	if (ch != NULL) {
		tsg = nvgpu_tsg_from_ch(ch);
		if (tsg == NULL) {
			nvgpu_profiler_free(prof_obj);
			goto clean_up;
		}

		prof_obj->tsg = tsg;
	}

	prof_obj->session_id = dbg_s->id;

	/* Return handle to client */
	args->profiler_handle = prof_obj->prof_handle;

clean_up:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	return  err;
}

static int nvgpu_ioctl_free_profiler_object(
				struct dbg_session_gk20a_linux *dbg_s_linux,
				struct nvgpu_dbg_gpu_profiler_obj_mgt_args *args)
{
	int err = 0;
	struct dbg_session_gk20a *dbg_s = &dbg_s_linux->dbg_s;
	struct gk20a *g = get_gk20a(dbg_s_linux->dev);
	struct nvgpu_profiler_object *prof_obj, *tmp_obj;
	bool obj_found = false;

	nvgpu_log_fn(g, "%s session_id = %d profiler_handle = %x",
		     g->name, dbg_s->id, args->profiler_handle);

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	/* Remove profiler object from the list, if a match is found */
	nvgpu_list_for_each_entry_safe(prof_obj, tmp_obj, &g->profiler_objects,
				nvgpu_profiler_object, prof_obj_entry) {
		if (prof_obj->prof_handle == args->profiler_handle) {
			if (prof_obj->session_id != dbg_s->id) {
				nvgpu_err(g,
						"invalid handle %x",
						args->profiler_handle);
				err = -EINVAL;
				break;
			}
			nvgpu_profiler_pm_resource_release(prof_obj,
				NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY);
			nvgpu_profiler_free(prof_obj);
			obj_found = true;
			break;
		}
	}
	if (!obj_found) {
		nvgpu_err(g, "profiler %x not found",
							args->profiler_handle);
		err = -EINVAL;
	}

	nvgpu_mutex_release(&g->dbg_sessions_lock);
	return  err;
}

static struct nvgpu_profiler_object *find_matching_prof_obj(
						struct dbg_session_gk20a *dbg_s,
						u32 profiler_handle)
{
	struct gk20a *g = dbg_s->g;
	struct nvgpu_profiler_object *prof_obj;

	nvgpu_list_for_each_entry(prof_obj, &g->profiler_objects,
				nvgpu_profiler_object, prof_obj_entry) {
		if (prof_obj->prof_handle == profiler_handle) {
			if (prof_obj->session_id != dbg_s->id) {
				nvgpu_err(g,
						"invalid handle %x",
						profiler_handle);
				return NULL;
			}
			return prof_obj;
		}
	}
	return NULL;
}

/* used in scenarios where the debugger session can take just the inter-session
 * lock for performance, but the profiler session must take the per-gpu lock
 * since it might not have an associated channel. */
static void gk20a_dbg_session_nvgpu_mutex_acquire(struct dbg_session_gk20a *dbg_s)
{
	struct nvgpu_channel *ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);

	if (dbg_s->is_profiler || !ch)
		nvgpu_mutex_acquire(&dbg_s->g->dbg_sessions_lock);
	else
		nvgpu_mutex_acquire(&ch->dbg_s_lock);
}

static void gk20a_dbg_session_nvgpu_mutex_release(struct dbg_session_gk20a *dbg_s)
{
	struct nvgpu_channel *ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);

	if (dbg_s->is_profiler || !ch)
		nvgpu_mutex_release(&dbg_s->g->dbg_sessions_lock);
	else
		nvgpu_mutex_release(&ch->dbg_s_lock);
}

static void gk20a_dbg_gpu_events_enable(struct dbg_session_gk20a *dbg_s)
{
	struct gk20a *g = dbg_s->g;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	gk20a_dbg_session_nvgpu_mutex_acquire(dbg_s);

	dbg_s->dbg_events.events_enabled = true;
	dbg_s->dbg_events.num_pending_events = 0;

	gk20a_dbg_session_nvgpu_mutex_release(dbg_s);
}

static void gk20a_dbg_gpu_events_disable(struct dbg_session_gk20a *dbg_s)
{
	struct gk20a *g = dbg_s->g;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	gk20a_dbg_session_nvgpu_mutex_acquire(dbg_s);

	dbg_s->dbg_events.events_enabled = false;
	dbg_s->dbg_events.num_pending_events = 0;

	gk20a_dbg_session_nvgpu_mutex_release(dbg_s);
}

static void gk20a_dbg_gpu_events_clear(struct dbg_session_gk20a *dbg_s)
{
	struct gk20a *g = dbg_s->g;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	gk20a_dbg_session_nvgpu_mutex_acquire(dbg_s);

	if (dbg_s->dbg_events.events_enabled &&
			dbg_s->dbg_events.num_pending_events > 0)
		dbg_s->dbg_events.num_pending_events--;

	gk20a_dbg_session_nvgpu_mutex_release(dbg_s);
}


static int gk20a_dbg_gpu_events_ctrl(struct dbg_session_gk20a *dbg_s,
			  struct nvgpu_dbg_gpu_events_ctrl_args *args)
{
	int ret = 0;
	struct nvgpu_channel *ch;
	struct gk20a *g = dbg_s->g;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "dbg events ctrl cmd %d", args->cmd);

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (!ch) {
		nvgpu_err(g, "no channel bound to dbg session");
		return -EINVAL;
	}

	nvgpu_speculation_barrier();
	switch (args->cmd) {
	case NVGPU_DBG_GPU_EVENTS_CTRL_CMD_ENABLE:
		gk20a_dbg_gpu_events_enable(dbg_s);
		break;

	case NVGPU_DBG_GPU_EVENTS_CTRL_CMD_DISABLE:
		gk20a_dbg_gpu_events_disable(dbg_s);
		break;

	case NVGPU_DBG_GPU_EVENTS_CTRL_CMD_CLEAR:
		gk20a_dbg_gpu_events_clear(dbg_s);
		break;

	default:
		nvgpu_err(g, "unrecognized dbg gpu events ctrl cmd: 0x%x",
			   args->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int nvgpu_perfbuf_reserve_pma(struct dbg_session_gk20a *dbg_s)
{
	struct gk20a *g = dbg_s->g;
	int err;

	/* Legacy profiler only supports global PMA stream */
	err = nvgpu_profiler_alloc(g, &dbg_s->prof,
			NVGPU_PROFILER_PM_RESERVATION_SCOPE_DEVICE,
			dbg_s->gpu_instance_id);
	if (err != 0) {
		nvgpu_err(g, "Failed to allocate profiler object");
		return err;
	}

	err = nvgpu_profiler_pm_resource_reserve(dbg_s->prof,
			NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM);
	if (err != 0) {
		nvgpu_err(g, "Failed to reserve PMA stream");
		nvgpu_profiler_free(dbg_s->prof);
		return err;
	}

	return err;
}

static void nvgpu_perfbuf_release_pma(struct dbg_session_gk20a *dbg_s)
{
	struct gk20a *g = dbg_s->g;
	int err;

	err = nvgpu_profiler_pm_resource_release(dbg_s->prof,
			NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM);
	if (err != 0) {
		nvgpu_err(g, "Failed to release PMA stream");
	}

	nvgpu_profiler_free(dbg_s->prof);
}

static int gk20a_perfbuf_map(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_perfbuf_map_args *args)
{
	struct gk20a *g = dbg_s->g;
	struct mm_gk20a *mm = &g->mm;
	int err;
	u32 virt_size;

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	if (g->perfbuf.owner) {
		nvgpu_mutex_release(&g->dbg_sessions_lock);
		return -EBUSY;
	}

	err = nvgpu_perfbuf_reserve_pma(dbg_s);
	if (err != 0) {
		nvgpu_mutex_release(&g->dbg_sessions_lock);
		return err;
	}

	err = nvgpu_perfbuf_init_vm(g);
	if (err != 0) {
		goto err_release_pma;
	}

	args->offset = mm->perfbuf.pma_buffer_gpu_va;
	err = nvgpu_vm_map_buffer(mm->perfbuf.vm,
			args->dmabuf_fd,
			&args->offset,
			NVGPU_AS_MAP_BUFFER_FLAGS_FIXED_OFFSET,
			SZ_4K,
			0,
			0,
			0,
			0,
			NULL);
	if (err)
		goto err_remove_vm;

	/* perf output buffer may not cross a 4GB boundary */
	virt_size = u64_lo32(args->mapping_size);
	if (u64_hi32(args->offset) != u64_hi32(args->offset + virt_size - 1)) {
		err = -EINVAL;
		goto err_unmap;
	}

	err = g->ops.perfbuf.perfbuf_enable(g, args->offset, virt_size);
	if (err)
		goto err_unmap;

	g->perfbuf.owner = dbg_s;
	g->perfbuf.offset = args->offset;
	nvgpu_mutex_release(&g->dbg_sessions_lock);

	return 0;

err_unmap:
	nvgpu_vm_unmap(mm->perfbuf.vm, args->offset, NULL);
err_remove_vm:
	nvgpu_perfbuf_deinit_vm(g);
err_release_pma:
	nvgpu_perfbuf_release_pma(dbg_s);
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	return err;
}

static int gk20a_perfbuf_unmap(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_perfbuf_unmap_args *args)
{
	struct gk20a *g = dbg_s->g;
	int err;

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
	if ((g->perfbuf.owner != dbg_s) ||
					(g->perfbuf.offset != args->offset)) {
		nvgpu_mutex_release(&g->dbg_sessions_lock);
		return -EINVAL;
	}

	err = gk20a_perfbuf_release_locked(g, dbg_s, args->offset);

	nvgpu_mutex_release(&g->dbg_sessions_lock);

	return err;
}

static int gk20a_dbg_pc_sampling(struct dbg_session_gk20a *dbg_s,
			  struct nvgpu_dbg_gpu_pc_sampling_args *args)
{
	struct nvgpu_channel *ch;
	struct gk20a *g = dbg_s->g;
	u32 gr_instance_id =
		nvgpu_grmgr_get_gr_instance_id(g, dbg_s->gpu_instance_id);

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (!ch)
		return -EINVAL;

	nvgpu_log_fn(g, " ");

	return (g->ops.gr.update_pc_sampling ?
		nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
			g->ops.gr.update_pc_sampling(ch, args->enable)) : -EINVAL);
}

static int nvgpu_dbg_gpu_ioctl_clear_single_sm_error_state(
		struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_clear_single_sm_error_state_args *args)
{
	struct gk20a *g = dbg_s->g;
	u32 sm_id;
	struct nvgpu_channel *ch;
	int err = 0;
	u32 gr_instance_id =
		nvgpu_grmgr_get_gr_instance_id(g, dbg_s->gpu_instance_id);
	struct nvgpu_gr_config *gr_config =
		nvgpu_gr_get_gpu_instance_config_ptr(g, dbg_s->gpu_instance_id);

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (ch == NULL) {
		return -EINVAL;
	}

	sm_id = args->sm_id;
	if (sm_id >= nvgpu_gr_config_get_no_of_sm(gr_config)) {
		return -EINVAL;
	}

	nvgpu_speculation_barrier();

	err = gk20a_busy(g);
	if (err != 0) {
		return err;
	}

	err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
			nvgpu_pg_elpg_protected_call(g,
				g->ops.gr.clear_sm_error_state(g, ch, sm_id)));

	gk20a_idle(g);

	return err;
}

static int
nvgpu_dbg_gpu_ioctl_suspend_resume_contexts(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_suspend_resume_contexts_args *args)
{
	struct gk20a *g = dbg_s->g;
	int err = 0;
	int ctx_resident_ch_fd = -1;
	u32 gr_instance_id =
		nvgpu_grmgr_get_gr_instance_id(g, dbg_s->gpu_instance_id);

	err = gk20a_busy(g);
	if (err)
		return err;

	nvgpu_speculation_barrier();
	switch (args->action) {
	case NVGPU_DBG_GPU_SUSPEND_ALL_CONTEXTS:
		err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
				g->ops.gr.suspend_contexts(g, dbg_s,
					&ctx_resident_ch_fd));
		break;

	case NVGPU_DBG_GPU_RESUME_ALL_CONTEXTS:
		err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
				g->ops.gr.resume_contexts(g, dbg_s,
					&ctx_resident_ch_fd));
		break;
	}

	if (ctx_resident_ch_fd < 0) {
		args->is_resident_context = 0;
	} else {
		args->is_resident_context = 1;
		args->resident_context_fd = ctx_resident_ch_fd;
	}

	gk20a_idle(g);

	return err;
}

#ifdef CONFIG_NVGPU_DGPU
static int nvgpu_dbg_gpu_ioctl_access_fb_memory(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_access_fb_memory_args *args)
{
	struct gk20a *g = dbg_s->g;
	struct dma_buf *dmabuf;
	void __user *user_buffer = (void __user *)(uintptr_t)args->buffer;
	void *buffer;
	u64 size, access_size, offset;
	u64 access_limit_size = SZ_4K;
	int err = 0;

	if ((args->offset & 3) || (!args->size) || (args->size & 3))
		return -EINVAL;

	dmabuf = dma_buf_get(args->dmabuf_fd);
	if (IS_ERR(dmabuf))
		return -EINVAL;

	if ((args->offset > dmabuf->size) ||
	    (args->size > dmabuf->size) ||
	    (args->offset + args->size > dmabuf->size)) {
		err = -EINVAL;
		goto fail_dmabuf_put;
	}

	buffer = nvgpu_big_zalloc(g, access_limit_size);
	if (!buffer) {
		err = -ENOMEM;
		goto fail_dmabuf_put;
	}

	size = args->size;
	offset = 0;

	err = gk20a_busy(g);
	if (err)
		goto fail_free_buffer;

	while (size) {
		/* Max access size of access_limit_size in one loop */
		access_size = min(access_limit_size, size);

		if (args->cmd ==
		    NVGPU_DBG_GPU_IOCTL_ACCESS_FB_MEMORY_CMD_WRITE) {
			err = copy_from_user(buffer, user_buffer + offset,
					     access_size);
			if (err)
				goto fail_idle;
		}

		err = nvgpu_vidmem_buf_access_memory(g, dmabuf, buffer,
					 args->offset + offset, access_size,
					 args->cmd);
		if (err)
			goto fail_idle;

		if (args->cmd ==
		    NVGPU_DBG_GPU_IOCTL_ACCESS_FB_MEMORY_CMD_READ) {
			err = copy_to_user(user_buffer + offset,
					   buffer, access_size);
			if (err)
				goto fail_idle;
		}

		size -= access_size;
		offset += access_size;
	}
	nvgpu_speculation_barrier();

fail_idle:
	gk20a_idle(g);
fail_free_buffer:
	nvgpu_big_free(g, buffer);
fail_dmabuf_put:
	dma_buf_put(dmabuf);

	return err;
}
#endif

static int nvgpu_ioctl_profiler_reserve(struct dbg_session_gk20a *dbg_s,
			   struct nvgpu_dbg_gpu_profiler_reserve_args *args)
{
	if (args->acquire)
		return nvgpu_profiler_reserve_acquire(dbg_s, args->profiler_handle);

	return nvgpu_profiler_reserve_release(dbg_s, args->profiler_handle);
}

static void nvgpu_dbg_gpu_ioctl_get_timeout(struct dbg_session_gk20a *dbg_s,
			 struct nvgpu_dbg_gpu_timeout_args *args)
{
	bool status;
	struct gk20a *g = dbg_s->g;

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
	status = nvgpu_is_timeouts_enabled(g);
	nvgpu_mutex_release(&g->dbg_sessions_lock);

	if (status)
		args->enable = NVGPU_DBG_GPU_IOCTL_TIMEOUT_ENABLE;
	else
		args->enable = NVGPU_DBG_GPU_IOCTL_TIMEOUT_DISABLE;
}

static int nvgpu_dbg_gpu_ioctl_get_gr_context_size(struct dbg_session_gk20a *dbg_s,
			 struct nvgpu_dbg_gpu_get_gr_context_size_args *args)
{
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch;
	struct nvgpu_tsg *tsg;
	struct nvgpu_mem *ctx_mem;

	nvgpu_log_fn(g, " ");

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_GET_GR_CONTEXT)) {
		nvgpu_err(g, "get_gr_context is not supported on current config");
		return -EINVAL;
	}

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (ch == NULL) {
		nvgpu_err(g, "no bound channel");
		return -EINVAL;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		nvgpu_err(ch->g, "chid: %d is not bound to tsg", ch->chid);
		return -EINVAL;
	}

	ctx_mem = nvgpu_gr_ctx_get_ctx_mem(tsg->gr_ctx);
	if (ctx_mem == NULL || !nvgpu_mem_is_valid(ctx_mem)) {
		nvgpu_err(g, "invalid context mem");
		return -EINVAL;
	}

	if (ctx_mem->size > (u64)UINT_MAX) {
		nvgpu_err(ch->g, "ctx size is larger than expected");
		return -EINVAL;
	}

	args->size = (u32)ctx_mem->size;

	return 0;
}

static int nvgpu_dbg_gpu_ioctl_get_gr_context(struct dbg_session_gk20a *dbg_s,
			struct nvgpu_dbg_gpu_get_gr_context_args *args)
{
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch;
	struct nvgpu_tsg *tsg;
	struct nvgpu_mem *ctx_mem;
	void __user *user_buffer = (void __user *)(uintptr_t)args->buffer;
	u32 size;
	int err = 0, enable_err = 0;

	nvgpu_log_fn(g, " ");

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_GET_GR_CONTEXT)) {
		nvgpu_err(g, "get_gr_context is not supported on current config");
		return -EINVAL;
	}

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (ch == NULL) {
		nvgpu_err(g, "no bound channel");
		return -EINVAL;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		nvgpu_err(ch->g, "chid: %d is not bound to tsg", ch->chid);
		return -EINVAL;
	}

	ctx_mem = nvgpu_gr_ctx_get_ctx_mem(tsg->gr_ctx);
	if (ctx_mem == NULL || !nvgpu_mem_is_valid(ctx_mem)) {
		nvgpu_err(g, "invalid context mem");
		return -EINVAL;
	}

	if (ctx_mem->size > (u64)UINT_MAX) {
		nvgpu_err(ch->g, "ctx size is larger than expected");
		return -EINVAL;
	}

	/* Check if the input buffer size equals the gr context size */
	size = (u32)ctx_mem->size;
	if (args->size != size) {
		nvgpu_err(g, "size mismatch: %d != %d", args->size, size);
		return -EINVAL;
	}

	if (nvgpu_channel_disable_tsg(g, ch) != 0) {
		nvgpu_err(g, "failed to disable channel/TSG");
		return -EINVAL;
	}

	err = nvgpu_preempt_channel(g, ch);
	if (err != 0) {
		nvgpu_err(g, "failed to preempt channel/TSG");
		goto done;
	}

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	err = nvgpu_pg_elpg_ms_protected_call(g, g->ops.mm.cache.l2_flush(g, true));
	if (err != 0) {
		nvgpu_err(g, "l2_flush failed");
		goto done;
	}

	err = nvgpu_dbg_get_context_buffer(g, ctx_mem, user_buffer, size);

done:
	enable_err = nvgpu_channel_enable_tsg(g, ch);
	if (enable_err != 0) {
		nvgpu_err(g, "failed to re-enable channel/TSG");
		return (err != 0) ? err : enable_err;
	}

	return err;
}

static int nvgpu_dbg_get_context_buffer(struct gk20a *g, struct nvgpu_mem *ctx_mem,
			void __user *ctx_buf, u32 ctx_buf_size)
{
	int err = 0;
#ifdef CONFIG_NVGPU_DGPU
	void *buffer = NULL;
	u32 size, access_size;
	u32 access_limit_size = SZ_4K;
	u32 offset = 0;
#endif

	if (ctx_mem->aperture == APERTURE_SYSMEM) {
		if (ctx_mem->cpu_va == NULL) {
			nvgpu_err(g, "CPU pointer is NULL. Note that this feature is currently \
				not supported on virtual GPU.");
			err = -EINVAL;
		} else {
			err = copy_to_user(ctx_buf, ctx_mem->cpu_va, ctx_buf_size);
		}
	}
#ifdef CONFIG_NVGPU_DGPU
	else {
		/* We already checked nvgpu_mem_is_valid, so ctx_mem->aperture must be
		   APERTURE_VIDMEM if we reach here */

		buffer = nvgpu_big_zalloc(g, access_limit_size);
		if (buffer == NULL) {
			err = -ENOMEM;
			goto done;
		}

		size = ctx_buf_size;
		while (size > 0) {
			/* Max access size of access_limit_size in one loop */
			access_size = min(access_limit_size, size);

			nvgpu_mem_rd_n(g, ctx_mem, offset, buffer, access_size);

			err = copy_to_user(ctx_buf + offset, buffer, access_size);
			if (err != 0)
				goto done;

			size -= access_size;
			offset += access_size;
		}
done:
		if (buffer != NULL) {
			nvgpu_big_free(g, buffer);
		}
	}
#endif

	return err;
}

static int gk20a_perfbuf_release_locked(struct gk20a *g,
		struct dbg_session_gk20a *dbg_s, u64 offset)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->perfbuf.vm;
	int err;

	err = g->ops.perfbuf.perfbuf_disable(g);

	nvgpu_vm_unmap(vm, offset, NULL);

	nvgpu_perfbuf_deinit_vm(g);

	nvgpu_perfbuf_release_pma(dbg_s);

	g->perfbuf.owner = NULL;
	g->perfbuf.offset = 0;
	return err;
}

static int nvgpu_profiler_reserve_release(struct dbg_session_gk20a *dbg_s,
								u32 profiler_handle)
{
	struct gk20a *g = dbg_s->g;
	struct nvgpu_profiler_object *prof_obj;
	int err = 0;

	nvgpu_log_fn(g, "%s profiler_handle = %x", g->name, profiler_handle);

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	/* Find matching object. */
	prof_obj = find_matching_prof_obj(dbg_s, profiler_handle);

	if (!prof_obj) {
		nvgpu_err(g, "object not found");
		err = -EINVAL;
		goto exit;
	}

	err = nvgpu_profiler_pm_resource_release(prof_obj,
			NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY);

exit:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	return err;
}

static int nvgpu_profiler_reserve_acquire(struct dbg_session_gk20a *dbg_s,
								u32 profiler_handle)
{
	struct gk20a *g = dbg_s->g;
	struct nvgpu_profiler_object *prof_obj;
	int err = 0;

	nvgpu_log_fn(g, "%s profiler_handle = %x", g->name, profiler_handle);

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	/* Find matching object. */
	prof_obj = find_matching_prof_obj(dbg_s, profiler_handle);

	if (!prof_obj) {
		nvgpu_err(g, "object not found");
		err = -EINVAL;
		goto exit;
	}

	if (prof_obj->tsg != NULL) {
		struct nvgpu_tsg *tsg = prof_obj->tsg;
		struct nvgpu_profiler_object *tmp_obj;

		nvgpu_list_for_each_entry(tmp_obj, &g->profiler_objects,
				nvgpu_profiler_object, prof_obj_entry) {
			if (tmp_obj->tsg == NULL) {
				continue;
			}
			if ((tmp_obj->tsg->tsgid == tsg->tsgid) &&
				  tmp_obj->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY]) {
				err = -EINVAL;
				goto exit;
			}
		}
	}

	err = nvgpu_profiler_pm_resource_reserve(prof_obj,
		NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY);

exit:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	return err;
}

static int dbg_unbind_channel_gk20a(struct dbg_session_gk20a *dbg_s,
			  struct nvgpu_dbg_gpu_unbind_channel_args *args)
{
	struct dbg_session_channel_data *ch_data;
	struct gk20a *g = dbg_s->g;
	bool channel_found = false;
	struct nvgpu_channel *ch;
	int err;

	nvgpu_log(g, gpu_dbg_fn|gpu_dbg_gpu_dbg, "%s fd=%d",
		   g->name, args->channel_fd);

	ch = nvgpu_channel_get_from_file(args->channel_fd);
	if (!ch) {
		nvgpu_log_fn(g, "no channel found for fd");
		return -EINVAL;
	}

	nvgpu_mutex_acquire(&dbg_s->ch_list_lock);
	nvgpu_list_for_each_entry(ch_data, &dbg_s->ch_list,
				dbg_session_channel_data, ch_entry) {
		if (ch->chid == ch_data->chid) {
			channel_found = true;
			break;
		}
	}
	nvgpu_mutex_release(&dbg_s->ch_list_lock);

	if (!channel_found) {
		nvgpu_log_fn(g, "channel not bounded, fd=%d\n", args->channel_fd);
		err = -EINVAL;
		goto out;
	}

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
	nvgpu_mutex_acquire(&dbg_s->ch_list_lock);
	err = dbg_unbind_single_channel_gk20a(dbg_s, ch_data);
	nvgpu_mutex_release(&dbg_s->ch_list_lock);
	nvgpu_mutex_release(&g->dbg_sessions_lock);

out:
	nvgpu_channel_put(ch);
	return err;
}

static int nvgpu_dbg_gpu_set_sm_exception_type_mask(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_set_sm_exception_type_mask_args *args)
{
	int err = 0;
	struct gk20a *g = dbg_s->g;
	u32 sm_exception_mask_type = NVGPU_SM_EXCEPTION_TYPE_MASK_NONE;
	struct nvgpu_channel *ch = NULL;

	nvgpu_speculation_barrier();
	switch (args->exception_type_mask) {
	case NVGPU_DBG_GPU_IOCTL_SET_SM_EXCEPTION_TYPE_MASK_FATAL:
		sm_exception_mask_type = NVGPU_SM_EXCEPTION_TYPE_MASK_FATAL;
		break;
	case NVGPU_DBG_GPU_IOCTL_SET_SM_EXCEPTION_TYPE_MASK_NONE:
		sm_exception_mask_type = NVGPU_SM_EXCEPTION_TYPE_MASK_NONE;
		break;
	default:
		nvgpu_err(g,
			   "unrecognized dbg sm exception type mask: 0x%x",
			   args->exception_type_mask);
		err = -EINVAL;
		break;
	}

	if (err != 0) {
		return err;
	}

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (ch != NULL) {
		if (g->ops.fifo.set_sm_exception_type_mask == NULL) {
			nvgpu_err(g, "set_sm_exception_type_mask not set");
			return -EINVAL;
		}
		err = g->ops.fifo.set_sm_exception_type_mask(ch,
				sm_exception_mask_type);
	} else {
		err = -EINVAL;
	}

	return err;
}

#if defined(CONFIG_NVGPU_CYCLESTATS)
static int nvgpu_dbg_gpu_cycle_stats(struct dbg_session_gk20a *dbg_s,
			struct nvgpu_dbg_gpu_cycle_stats_args *args)
{
	struct nvgpu_channel *ch = NULL;
	int err;

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (ch == NULL) {
		return -EINVAL;
	}

	err = gk20a_busy(ch->g);
	if (err != 0) {
		return err;
	}

	err = gk20a_channel_cycle_stats(ch, args->dmabuf_fd);

	gk20a_idle(ch->g);
	return err;
}

static int nvgpu_dbg_gpu_cycle_stats_snapshot(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_cycle_stats_snapshot_args *args)
{
	struct nvgpu_channel *ch = NULL;
	int err;

	if (!args->dmabuf_fd) {
		return -EINVAL;
	}

	nvgpu_speculation_barrier();

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (ch == NULL) {
		return -EINVAL;
	}

	/* is it allowed to handle calls for current GPU? */
	if (!nvgpu_is_enabled(ch->g, NVGPU_SUPPORT_CYCLE_STATS_SNAPSHOT)) {
		return -ENOSYS;
	}

	err = gk20a_busy(ch->g);
	if (err != 0) {
		return err;
	}

	/* handle the command (most frequent cases first) */
	switch (args->cmd) {
	case NVGPU_DBG_GPU_IOCTL_CYCLE_STATS_SNAPSHOT_CMD_FLUSH:
		err = gk20a_flush_cycle_stats_snapshot(ch);
		args->extra = 0;
		break;

	case NVGPU_DBG_GPU_IOCTL_CYCLE_STATS_SNAPSHOT_CMD_ATTACH:
		err = gk20a_attach_cycle_stats_snapshot(ch,
						args->dmabuf_fd,
						args->extra,
						&args->extra);
		break;

	case NVGPU_DBG_GPU_IOCTL_CYCLE_STATS_SNAPSHOT_CMD_DETACH:
		err = gk20a_channel_free_cycle_stats_snapshot(ch);
		args->extra = 0;
		break;

	default:
		pr_err("cyclestats: unknown command %u\n", args->cmd);
		err = -EINVAL;
		break;
	}

	gk20a_idle(ch->g);
	return err;
}

#endif

static void nvgpu_dbg_gpu_get_valid_mappings(struct nvgpu_channel *ch, u64 start,
		u64 end, u32 *buf_count, u8 *has_more, u32 count_lmt,
		struct nvgpu_dbg_gpu_get_mappings_entry *buffer)
{
	struct vm_gk20a *vm = ch->vm;
	u64 key = start;
	u64 size = 0;
	struct nvgpu_mapped_buf *mapped_buf = NULL;
	struct nvgpu_rbtree_node *node = NULL;
	struct dma_buf *dmabuf = NULL;
	u32 f_mode = FMODE_READ;
	u32 count = 0;
	u64 offset = 0;
	bool just_count = *buf_count ? false : true;

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	nvgpu_rbtree_enum_start(0, &node, vm->mapped_buffers);

	while (node != NULL) {
		mapped_buf = mapped_buffer_from_rbtree_node(node);
		dmabuf = mapped_buf->os_priv.dmabuf;

		/* Find first key node */
		if (key > (mapped_buf->addr + mapped_buf->size)) {
			nvgpu_rbtree_enum_next(&node, node);
			continue;
		}

		if (key < mapped_buf->addr) {
			key = mapped_buf->addr;
		}

		if (key >= end) {
			break;
		}

		/*
		 * Check for adjacent ranges are having same access permissions,
		 * coalesced them into single ops_buffer. Keep the gpu_va same
		 * and just increase the size of the buffer. Need to decrease
		 * count to get the correct buffer index as it was increased in
		 * last iteration.
		 */
		if ((offset + size == mapped_buf->addr) && count &&
			(f_mode == dmabuf->file->f_mode)) {
			count--;
			size += min(end, mapped_buf->addr
				+ mapped_buf->size) - key;
		} else {
			size = min(end, mapped_buf->addr
				+ mapped_buf->size) - key;
			offset = key;
			if (just_count == false) {
				buffer[count].gpu_va = offset;
			}
		}

		if (just_count == false) {
			buffer[count].size = size;
		}

		(count)++;
		if (count == count_lmt) {
			*has_more = 1;
			break;
		}

		f_mode = dmabuf->file->f_mode;
		nvgpu_rbtree_enum_next(&node, node);
	}

	*buf_count = count;
	nvgpu_mutex_release(&vm->update_gmmu_lock);
}

static int nvgpu_dbg_gpu_get_mappings(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_get_mappings_args *arg)
{
	int err;
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch;
	u64 start = arg->va_lo;
	u64 end = arg->va_hi;
	u32 count_in = arg->count;
	u32 buf_len = 0U;
	struct nvgpu_dbg_gpu_get_mappings_entry *buffer = NULL;

	if (start > end) {
		nvgpu_err(g, "start is greater than end");
		return -EINVAL;
	}

	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to poweron");
		return err;
	}

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (!ch) {
		nvgpu_err(g, "no bound channel for mmu debug mode");
		err = -EINVAL;
		goto clean_up;
	}

	if (count_in) {
		if (arg->ops_buffer == 0UL) {
			err = -EINVAL;
			nvgpu_err(g, "ops_buffer is pointing to NULL");
			goto clean_up;
		}
		buf_len = sizeof(*buffer) * count_in;
		buffer = nvgpu_kzalloc(g, buf_len);
		if (!buffer) {
			err = -ENOMEM;
			goto clean_up;
		}
	}

	nvgpu_dbg_gpu_get_valid_mappings(ch, start, end, &arg->count,
		&arg->has_more, count_in, buffer);

	/*
	 * Buffer will be copied to userspace only when arg->ops_buffer is not
	 * 0. If value of arg->ops_buffer is 0 then interface only sets count.
	 */
	if (count_in) {
		err = copy_to_user((void __user *)arg->ops_buffer, buffer,
			(arg->count * sizeof(*buffer)));
		if (err != 0) {
			nvgpu_err(g, "gpu va copy_to_user failed");
			err = -EFAULT;
			goto clean_up;
		}
	}

clean_up:
	if (buffer) {
		nvgpu_kfree(g, buffer);
		buffer = NULL;
	}

	gk20a_idle(g);
	return err;
}

static int nvgpu_gpu_access_sysmem_gpu_va(struct gk20a *g, u8 cmd, u32 size,
		u64 *data, struct dma_buf *dmabuf, u64 offset)
{
	int ret = 0;
	u8 *cpu_va = NULL;

	cpu_va = (u8 *)gk20a_dmabuf_vmap(dmabuf);
	if (!cpu_va) {
		return -ENOMEM;
	}

	switch (cmd) {
	case NVGPU_DBG_GPU_IOCTL_ACCESS_GPUVA_CMD_READ:
		nvgpu_memcpy((u8 *)data, cpu_va + offset, size);
		break;

	case NVGPU_DBG_GPU_IOCTL_ACCESS_GPUVA_CMD_WRITE:
		nvgpu_memcpy(cpu_va + offset, (u8 *)data, size);
		break;

	default:
		nvgpu_err(g, "%x is invalid command", cmd);
		ret = -EINVAL;
	}

	gk20a_dmabuf_vunmap(dmabuf, cpu_va);

	return ret;
}

#ifdef CONFIG_NVGPU_DGPU
static int nvgpu_gpu_access_vidmem_va(struct gk20a *g, u8 cmd, u64 size,
	void *data, struct dma_buf *dmabuf, u64 offset)
{
	int ret = 0;

	switch (cmd) {
	case NVGPU_DBG_GPU_IOCTL_ACCESS_GPUVA_CMD_READ:
		ret = nvgpu_vidmem_buf_access_memory(g, dmabuf, data, offset,
			size, NVGPU_DBG_GPU_IOCTL_ACCESS_FB_MEMORY_CMD_READ);
		break;

	case NVGPU_DBG_GPU_IOCTL_ACCESS_GPUVA_CMD_WRITE:
		ret = nvgpu_vidmem_buf_access_memory(g, dmabuf, data, offset,
			size, NVGPU_DBG_GPU_IOCTL_ACCESS_FB_MEMORY_CMD_WRITE);
		break;

	default:
		nvgpu_err(g, "%x is invalid command", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}
#endif

static int nvgpu_dbg_gpu_buf_access_check(struct gk20a *g, u8 cmd, u64 offset,
	struct dma_buf *dmabuf)
{
	int ret = 0;

	if (cmd == NVGPU_DBG_GPU_IOCTL_ACCESS_GPUVA_CMD_WRITE) {
		if ((dmabuf->file->f_mode & (FMODE_WRITE | FMODE_PWRITE)) == 0) {
			nvgpu_err(g, "offset %llu does not have write permission",
				offset);
			ret = -EINVAL;
		}
	} else if (cmd == NVGPU_DBG_GPU_IOCTL_ACCESS_GPUVA_CMD_READ) {
		if ((dmabuf->file->f_mode & (FMODE_READ | FMODE_PREAD)) == 0) {
			nvgpu_err(g, "offset %llu does not have read permission",
				offset);
			ret = -EINVAL;
		}
	} else {
		nvgpu_err(g, "Invalid command");
		ret = -EINVAL;
	}

	return ret;

}

static int nvgpu_dbg_gpu_op_on_mapped_buf(struct gk20a *g, u8 cmd, u64 offset,
	u32 *size_in, struct dma_buf *dmabuf, struct nvgpu_mapped_buf *mapped_buf,
	u64 *gpu_va, u64 *data)
{
	int ret = 0;
	bool is_vidmem;
	u32 size = *size_in;
	u32 access_buf_sz = 0;

	access_buf_sz = mapped_buf->addr + mapped_buf->size - *gpu_va;
	if (size < access_buf_sz) {
		access_buf_sz = size;
		size = 0;
	} else {
		size -= access_buf_sz;
	}

	is_vidmem = (gk20a_dmabuf_aperture(g, dmabuf) ==
		APERTURE_VIDMEM) ? true : false;
#ifdef CONFIG_NVGPU_DGPU
	if (is_vidmem) {
		ret = nvgpu_gpu_access_vidmem_va(g, cmd,
			(u64)access_buf_sz, (void *)data, dmabuf,
			offset);
	}
	else
#endif
	{
		ret = nvgpu_gpu_access_sysmem_gpu_va(g, cmd, access_buf_sz,
			data, dmabuf, offset);
	}

	if (ret) {
		nvgpu_err(g, "gpu va access failed");
		return ret;
	}

	*gpu_va += access_buf_sz;
	*size_in = size;
	data = (u64 *)((u8 *)data + access_buf_sz);

	return ret;
}

static int nvgpu_dbg_gpu_access_gpu_va_mapping(struct gk20a *g,
		struct nvgpu_channel *ch, u8 cmd, u64 *op_data,
		struct nvgpu_dbg_gpu_va_access_entry *op)
{
	u64 gpu_va = op->gpu_va;
	int ret = 0;
	u32 size = 0;
	struct vm_gk20a *vm = ch->vm;
	struct nvgpu_mapped_buf *mapped_buf = NULL;
	struct dma_buf *dmabuf = NULL;
	u64 *data = op_data;
	u64 offset = 0;

	op->valid = 0;
	size = op->size;
	if (size & 0x3) {
		nvgpu_err(g, "given size is not 4byte aligned");
		return -EINVAL;
	}

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);
	while (size > 0) {
		mapped_buf = nvgpu_vm_find_mapped_buf_range(vm, gpu_va);
		if (mapped_buf == NULL) {
			nvgpu_err(g, "gpuva is not mapped");
			ret = -EINVAL;
			break;
		}

		offset = gpu_va - mapped_buf->addr;
		if (offset & 0x3) {
			nvgpu_err(g, "given offset is not 4byte aligned");
			ret = -EINVAL;
			break;
		}

		dmabuf = mapped_buf->os_priv.dmabuf;
		ret = nvgpu_dbg_gpu_buf_access_check(g, cmd, offset, dmabuf);
		if (ret) {
			break;
		}

		ret = nvgpu_dbg_gpu_op_on_mapped_buf(g, cmd, offset, &size,
			dmabuf, mapped_buf, &gpu_va, data);
		if (ret) {
			break;
		}
	}

	if (ret == 0) {
		op->valid = 1;
	}
	nvgpu_mutex_release(&vm->update_gmmu_lock);
	return ret;
}

static int nvgpu_dbg_gpu_access_gpu_va(struct dbg_session_gk20a *dbg_s,
		struct nvgpu_dbg_gpu_va_access_args *arg)
{
	int ret = 0;
	size_t buf_len;
	u32 i;
	u8 cmd;
	u64 *buffer = NULL;
	size_t size, allocated_size = 0;
	void __user *user_buffer;
	struct gk20a *g = dbg_s->g;
	struct nvgpu_channel *ch;
	struct nvgpu_dbg_gpu_va_access_entry *ops_buffer = NULL;

	ch = nvgpu_dbg_gpu_get_session_channel(dbg_s);
	if (!ch) {
		nvgpu_err(g, "no bound channel for debug session");
		return -EINVAL;
	}

	if (arg->count == 0) {
		nvgpu_err(g, "access count is 0");
		return -EINVAL;
	}

	buf_len = sizeof(*ops_buffer) * arg->count;
	ops_buffer = nvgpu_kzalloc(g, buf_len);
	if (!ops_buffer) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = copy_from_user(ops_buffer, (void __user *)arg->ops_buf, buf_len);
	if (ret != 0) {
		nvgpu_err(g, "gpu va copy_from_user failed");
		ret = -EFAULT;
		goto fail;
	}

	cmd = arg->cmd;
	for (i = 0; i < arg->count; i++) {
		size = ops_buffer[i].size;

		if (size == 0UL) {
			nvgpu_err(g, "size is zero");
			ret = -EINVAL;
			goto fail;
		}

		if ((ops_buffer[i].gpu_va & 0x3)) {
			nvgpu_err(g, "gpu va is not aligned %u 0x%llx", i,
				ops_buffer[i].gpu_va);
			ret = -EINVAL;
			goto fail;
		}
		user_buffer = (void __user *)(uintptr_t)ops_buffer[i].data;

		if (size > allocated_size) {
			if (buffer) {
				nvgpu_big_free(g, buffer);
				buffer = NULL;
			}

			buffer = nvgpu_big_zalloc(g, size);
			if (buffer == NULL) {
				ret = -ENOMEM;
				goto fail;
			}
		}
		(void)memset(buffer, 0, size);
		allocated_size = size;
		if (cmd == NVGPU_DBG_GPU_IOCTL_ACCESS_GPUVA_CMD_WRITE) {
			ret = copy_from_user(buffer, user_buffer, size);
			if (ret != 0) {
				nvgpu_err(g, "gpu va copy_from_user failed");
				ret = -EFAULT;
				goto fail;
			}
		}
		ret = nvgpu_dbg_gpu_access_gpu_va_mapping(g, ch, cmd, buffer,
			&ops_buffer[i]);
		if (ret != 0) {
			nvgpu_err(g, "gpu va buffer access failed for itr %u"
				"cmd %u ch %p", i, cmd, ch);
			goto fail;
		}

		if (cmd == NVGPU_DBG_GPU_IOCTL_ACCESS_GPUVA_CMD_READ) {
			ret = copy_to_user(user_buffer, buffer, size);
			if (ret != 0) {
				nvgpu_err(g, "gpu va copy_to_user failed");
				ret = -EFAULT;
				goto fail;
			}
		}
	}
fail:
	if (buffer) {
		 nvgpu_big_free(g, buffer);
	}

	if (ops_buffer) {
		nvgpu_kfree(g, ops_buffer);
	}
	return ret;
}

int gk20a_dbg_gpu_dev_open(struct inode *inode, struct file *filp)
{
	struct gk20a *g;
	struct nvgpu_cdev *cdev;
	u32 gpu_instance_id;

	cdev = container_of(inode->i_cdev, struct nvgpu_cdev, cdev);
	g = nvgpu_get_gk20a_from_cdev(cdev);
	gpu_instance_id = nvgpu_get_gpu_instance_id_from_cdev(g, cdev);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");
	return gk20a_dbg_gpu_do_dev_open(g, filp, gpu_instance_id, false /* not profiler */);
}

long gk20a_dbg_gpu_dev_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct dbg_session_gk20a_linux *dbg_s_linux = filp->private_data;
	struct dbg_session_gk20a *dbg_s = &dbg_s_linux->dbg_s;
	struct gk20a *g = dbg_s->g;
	u8 buf[NVGPU_DBG_GPU_IOCTL_MAX_ARG_SIZE];
	int err = 0;
	u32 gr_instance_id =
		nvgpu_grmgr_get_gr_instance_id(g, dbg_s->gpu_instance_id);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
		"gpu_instance_id [%u] gr_instance_id [%u]",
		dbg_s->gpu_instance_id, gr_instance_id);

	nvgpu_assert(dbg_s->gpu_instance_id < g->mig.num_gpu_instances);
	nvgpu_assert(gr_instance_id < g->num_gr_instances);

	if ((_IOC_TYPE(cmd) != NVGPU_DBG_GPU_IOCTL_MAGIC) ||
	    (_IOC_NR(cmd) == 0) ||
	    (_IOC_NR(cmd) > NVGPU_DBG_GPU_IOCTL_LAST) ||
	    (_IOC_SIZE(cmd) > NVGPU_DBG_GPU_IOCTL_MAX_ARG_SIZE))
		return -EINVAL;

	(void) memset(buf, 0, sizeof(buf));
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	if (!g->sw_ready) {
		err = gk20a_busy(g);
		if (err)
			return err;

		gk20a_idle(g);
	}

	/* protect from threaded user space calls */
	nvgpu_mutex_acquire(&dbg_s->ioctl_lock);

	nvgpu_speculation_barrier();
	switch (cmd) {
	case NVGPU_DBG_GPU_IOCTL_BIND_CHANNEL:
		err = dbg_bind_channel_gk20a(dbg_s,
			     (struct nvgpu_dbg_gpu_bind_channel_args *)buf);
		break;

#ifdef CONFIG_NVGPU_DEBUGGER
	case NVGPU_DBG_GPU_IOCTL_REG_OPS:
		err = nvgpu_ioctl_channel_reg_ops(dbg_s,
			   (struct nvgpu_dbg_gpu_exec_reg_ops_args *)buf);
		break;
#endif

	case NVGPU_DBG_GPU_IOCTL_POWERGATE:
		err = nvgpu_ioctl_powergate_gk20a(dbg_s,
			   (struct nvgpu_dbg_gpu_powergate_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_EVENTS_CTRL:
		err = gk20a_dbg_gpu_events_ctrl(dbg_s,
			   (struct nvgpu_dbg_gpu_events_ctrl_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_SMPC_CTXSW_MODE:
		err = nvgpu_dbg_gpu_ioctl_smpc_ctxsw_mode(dbg_s,
			   (struct nvgpu_dbg_gpu_smpc_ctxsw_mode_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_HWPM_CTXSW_MODE:
		err = nvgpu_dbg_gpu_ioctl_hwpm_ctxsw_mode(dbg_s,
			   (struct nvgpu_dbg_gpu_hwpm_ctxsw_mode_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_SUSPEND_RESUME_ALL_SMS:
		err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
				nvgpu_dbg_gpu_ioctl_suspend_resume_sm(dbg_s,
					(struct nvgpu_dbg_gpu_suspend_resume_all_sms_args *)buf));
		break;

	case NVGPU_DBG_GPU_IOCTL_PERFBUF_MAP:
		err = gk20a_perfbuf_map(dbg_s,
		       (struct nvgpu_dbg_gpu_perfbuf_map_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_PERFBUF_UNMAP:
		err = gk20a_perfbuf_unmap(dbg_s,
		       (struct nvgpu_dbg_gpu_perfbuf_unmap_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_PC_SAMPLING:
		err = gk20a_dbg_pc_sampling(dbg_s,
			   (struct nvgpu_dbg_gpu_pc_sampling_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_SET_NEXT_STOP_TRIGGER_TYPE:
		err = nvgpu_dbg_gpu_ioctl_set_next_stop_trigger_type(dbg_s,
		       (struct nvgpu_dbg_gpu_set_next_stop_trigger_type_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_TIMEOUT:
		err = nvgpu_dbg_gpu_ioctl_timeout(dbg_s,
			   (struct nvgpu_dbg_gpu_timeout_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_GET_TIMEOUT:
		nvgpu_dbg_gpu_ioctl_get_timeout(dbg_s,
			   (struct nvgpu_dbg_gpu_timeout_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_GET_GR_CONTEXT_SIZE:
		err = nvgpu_dbg_gpu_ioctl_get_gr_context_size(dbg_s,
			(struct nvgpu_dbg_gpu_get_gr_context_size_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_GET_GR_CONTEXT:
		err = nvgpu_dbg_gpu_ioctl_get_gr_context(dbg_s,
			(struct nvgpu_dbg_gpu_get_gr_context_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_READ_SINGLE_SM_ERROR_STATE:
		err = nvgpu_dbg_gpu_ioctl_read_single_sm_error_state(dbg_s,
		   (struct nvgpu_dbg_gpu_read_single_sm_error_state_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_CLEAR_SINGLE_SM_ERROR_STATE:
		err = nvgpu_dbg_gpu_ioctl_clear_single_sm_error_state(dbg_s,
		  (struct nvgpu_dbg_gpu_clear_single_sm_error_state_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_UNBIND_CHANNEL:
		err = dbg_unbind_channel_gk20a(dbg_s,
			     (struct nvgpu_dbg_gpu_unbind_channel_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_SUSPEND_RESUME_CONTEXTS:
		err = nvgpu_dbg_gpu_ioctl_suspend_resume_contexts(dbg_s,
		      (struct nvgpu_dbg_gpu_suspend_resume_contexts_args *)buf);
		break;

#ifdef CONFIG_NVGPU_DGPU
	case NVGPU_DBG_GPU_IOCTL_ACCESS_FB_MEMORY:
		err = nvgpu_dbg_gpu_ioctl_access_fb_memory(dbg_s,
			(struct nvgpu_dbg_gpu_access_fb_memory_args *)buf);
		break;
#endif

	case NVGPU_DBG_GPU_IOCTL_PROFILER_ALLOCATE:
		err = nvgpu_ioctl_allocate_profiler_object(dbg_s_linux,
			(struct nvgpu_dbg_gpu_profiler_obj_mgt_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_PROFILER_FREE:
		err = nvgpu_ioctl_free_profiler_object(dbg_s_linux,
			(struct nvgpu_dbg_gpu_profiler_obj_mgt_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_PROFILER_RESERVE:
		err = nvgpu_ioctl_profiler_reserve(dbg_s,
			   (struct nvgpu_dbg_gpu_profiler_reserve_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_SET_SM_EXCEPTION_TYPE_MASK:
		err = nvgpu_dbg_gpu_set_sm_exception_type_mask(dbg_s,
		   (struct nvgpu_dbg_gpu_set_sm_exception_type_mask_args *)buf);
		break;

#ifdef CONFIG_NVGPU_CYCLESTATS
	case NVGPU_DBG_GPU_IOCTL_CYCLE_STATS:
		err = nvgpu_dbg_gpu_cycle_stats(dbg_s,
				(struct nvgpu_dbg_gpu_cycle_stats_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_CYCLE_STATS_SNAPSHOT:
		err = nvgpu_dbg_gpu_cycle_stats_snapshot(dbg_s,
				(struct nvgpu_dbg_gpu_cycle_stats_snapshot_args *)buf);
		break;
#endif
	case NVGPU_DBG_GPU_IOCTL_SET_CTX_MMU_DEBUG_MODE:
		err = nvgpu_dbg_gpu_ioctl_set_mmu_debug_mode(dbg_s,
		   (struct nvgpu_dbg_gpu_set_ctx_mmu_debug_mode_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_TSG_SET_TIMESLICE:
		err = nvgpu_dbg_gpu_ioctl_tsg_set_timeslice(dbg_s,
		   (struct nvgpu_timeslice_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_TSG_GET_TIMESLICE:
		err = nvgpu_dbg_gpu_ioctl_tsg_get_timeslice(dbg_s,
		   (struct nvgpu_timeslice_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_GET_MAPPINGS:
		err = nvgpu_dbg_gpu_get_mappings(dbg_s,
			(struct nvgpu_dbg_gpu_get_mappings_args *)buf);
		break;

	case NVGPU_DBG_GPU_IOCTL_ACCESS_GPU_VA:
		err = nvgpu_dbg_gpu_access_gpu_va(dbg_s,
			(struct nvgpu_dbg_gpu_va_access_args *)buf);
		break;

	default:
		nvgpu_err(g,
			   "unrecognized dbg gpu ioctl cmd: 0x%x",
			   cmd);
		err = -ENOTTY;
		break;
	}

	nvgpu_mutex_release(&dbg_s->ioctl_lock);

	nvgpu_log(g, gpu_dbg_gpu_dbg, "ret=%d", err);

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *)arg,
				   buf, _IOC_SIZE(cmd));

	return err;
}
