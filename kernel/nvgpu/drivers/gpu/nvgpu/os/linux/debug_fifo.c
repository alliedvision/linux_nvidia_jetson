/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "debug_fifo.h"
#include "swprofile_debugfs.h"
#include "os_linux.h"

#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <nvgpu/timers.h>
#include <nvgpu/channel.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/runlist.h>

static void *gk20a_fifo_sched_debugfs_seq_start(
		struct seq_file *s, loff_t *pos)
{
	struct gk20a *g = s->private;
	struct nvgpu_fifo *f = &g->fifo;

	if (*pos >= f->num_channels)
		return NULL;

	return &f->channel[*pos];
}

static void *gk20a_fifo_sched_debugfs_seq_next(
		struct seq_file *s, void *v, loff_t *pos)
{
	struct gk20a *g = s->private;
	struct nvgpu_fifo *f = &g->fifo;

	++(*pos);
	if (*pos >= f->num_channels)
		return NULL;

	return &f->channel[*pos];
}

static void gk20a_fifo_sched_debugfs_seq_stop(
		struct seq_file *s, void *v)
{
}

static int gk20a_fifo_sched_debugfs_seq_show(
		struct seq_file *s, void *v)
{
	struct gk20a *g = s->private;
	struct nvgpu_fifo *f = &g->fifo;
	struct nvgpu_channel *ch = v;
	struct nvgpu_tsg *tsg = NULL;

	const struct nvgpu_device *dev;
	struct nvgpu_runlist *runlist;
	u32 runlist_id;
	int ret = SEQ_SKIP;

	dev = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS, 0);
	nvgpu_assert(dev != NULL);

	runlist_id = dev->runlist_id;
	runlist = f->runlists[runlist_id];

	if (ch == f->channel) {
		seq_puts(s, "chid     tsgid    pid      timeslice  timeout  interleave graphics_preempt compute_preempt\n");
		seq_puts(s, "                            (usecs)   (msecs)\n");
		ret = 0;
	}

	if (!test_bit(ch->chid, runlist->domain->active_channels))
		return ret;

	if (nvgpu_channel_get(ch)) {
		tsg = nvgpu_tsg_from_ch(ch);

		if (tsg)
			seq_printf(s, "%-8d %-8d %-8d %-9d %-8d %-10d %-8d %-8d\n",
				ch->chid,
				ch->tsgid,
				ch->tgid,
				tsg->timeslice_us,
				ch->ctxsw_timeout_max_ms,
				tsg->interleave_level,
				nvgpu_gr_ctx_get_graphics_preemption_mode(tsg->gr_ctx),
				nvgpu_gr_ctx_get_compute_preemption_mode(tsg->gr_ctx));
		nvgpu_channel_put(ch);
	}
	return 0;
}

static const struct seq_operations gk20a_fifo_sched_debugfs_seq_ops = {
	.start = gk20a_fifo_sched_debugfs_seq_start,
	.next = gk20a_fifo_sched_debugfs_seq_next,
	.stop = gk20a_fifo_sched_debugfs_seq_stop,
	.show = gk20a_fifo_sched_debugfs_seq_show
};

static int gk20a_fifo_sched_debugfs_open(struct inode *inode,
	struct file *file)
{
	struct gk20a *g = inode->i_private;
	int err;

	err = seq_open(file, &gk20a_fifo_sched_debugfs_seq_ops);
	if (err)
		return err;

	nvgpu_log(g, gpu_dbg_info, "i_private=%p", inode->i_private);

	((struct seq_file *)file->private_data)->private = inode->i_private;
	return 0;
};

/*
 * The file operations structure contains our open function along with
 * set of the canned seq_ ops.
 */
static const struct file_operations gk20a_fifo_sched_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = gk20a_fifo_sched_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

void gk20a_fifo_debugfs_init(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct dentry *gpu_root = l->debugfs;
	struct dentry *fifo_root;

	fifo_root = debugfs_create_dir("fifo", gpu_root);
	if (IS_ERR_OR_NULL(fifo_root))
		return;

	nvgpu_log(g, gpu_dbg_info, "g=%p", g);

	debugfs_create_file("sched", 0600, fifo_root, g,
		&gk20a_fifo_sched_debugfs_fops);

	nvgpu_debugfs_swprofile_init(g, fifo_root, &g->fifo.kickoff_profiler,
				     "kickoff_profiler");
	nvgpu_debugfs_swprofile_init(g, fifo_root, &g->fifo.recovery_profiler,
				     "recovery_profiler");
	nvgpu_debugfs_swprofile_init(g, fifo_root, &g->fifo.eng_reset_profiler,
				     "eng_reset_profiler");
}
