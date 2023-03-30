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
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/list.h>
#include <nvgpu/log.h>
#include <nvgpu/log2.h>
#include <nvgpu/mm.h>
#include <nvgpu/circ_buf.h>
#include <nvgpu/timers.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gr/global_ctx.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/subctx.h>
#include <nvgpu/gr/fecs_trace.h>
#include <nvgpu/gr/gr_utils.h>

static void nvgpu_gr_fecs_trace_periodic_polling(void *arg);

int nvgpu_gr_fecs_trace_add_context(struct gk20a *g, u32 context_ptr,
	pid_t pid, u32 vmid, struct nvgpu_list_node *list)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	struct nvgpu_fecs_trace_context_entry *entry;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_ctxsw,
		"adding hash entry context_ptr=%x -> pid=%d, vmid=%d",
		context_ptr, pid, vmid);

	entry = nvgpu_kzalloc(g, sizeof(*entry));
	if (entry == NULL) {
		nvgpu_err(g,
			"can't alloc new entry for context_ptr=%x pid=%d vmid=%d",
			context_ptr, pid, vmid);
		return -ENOMEM;
	}

	nvgpu_init_list_node(&entry->entry);
	entry->context_ptr = context_ptr;
	entry->pid = pid;
	entry->vmid = vmid;

	nvgpu_mutex_acquire(&trace->list_lock);
	nvgpu_list_add_tail(&entry->entry, list);
	nvgpu_mutex_release(&trace->list_lock);

	return 0;
}

void nvgpu_gr_fecs_trace_remove_context(struct gk20a *g, u32 context_ptr,
	struct nvgpu_list_node *list)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	struct nvgpu_fecs_trace_context_entry *entry, *tmp;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_ctxsw,
		"freeing entry context_ptr=%x", context_ptr);

	nvgpu_mutex_acquire(&trace->list_lock);
	nvgpu_list_for_each_entry_safe(entry, tmp, list,
			nvgpu_fecs_trace_context_entry,	entry) {
		if (entry->context_ptr == context_ptr) {
			nvgpu_list_del(&entry->entry);
			nvgpu_log(g, gpu_dbg_ctxsw,
				"freed entry=%p context_ptr=%x", entry,
				entry->context_ptr);
			nvgpu_kfree(g, entry);
			break;
		}
	}
	nvgpu_mutex_release(&trace->list_lock);
}

void nvgpu_gr_fecs_trace_remove_contexts(struct gk20a *g,
	struct nvgpu_list_node *list)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	struct nvgpu_fecs_trace_context_entry *entry, *tmp;

	nvgpu_mutex_acquire(&trace->list_lock);
	nvgpu_list_for_each_entry_safe(entry, tmp, list,
			nvgpu_fecs_trace_context_entry,	entry) {
		nvgpu_list_del(&entry->entry);
		nvgpu_kfree(g, entry);
	}
	nvgpu_mutex_release(&trace->list_lock);
}

void nvgpu_gr_fecs_trace_find_pid(struct gk20a *g, u32 context_ptr,
	struct nvgpu_list_node *list, pid_t *pid, u32 *vmid)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	struct nvgpu_fecs_trace_context_entry *entry;

	nvgpu_mutex_acquire(&trace->list_lock);
	nvgpu_list_for_each_entry(entry, list, nvgpu_fecs_trace_context_entry,
			entry) {
		if (entry->context_ptr == context_ptr) {
			nvgpu_log(g, gpu_dbg_ctxsw,
				"found context_ptr=%x -> pid=%d, vmid=%d",
				entry->context_ptr, entry->pid, entry->vmid);
			*pid = entry->pid;
			*vmid = entry->vmid;
			nvgpu_mutex_release(&trace->list_lock);
			return;
		}
	}
	nvgpu_mutex_release(&trace->list_lock);

	*pid = 0;
	*vmid = 0xffffffffU;
}

int nvgpu_gr_fecs_trace_init(struct gk20a *g)
{
	struct nvgpu_gr_fecs_trace *trace;
	int err;

	if (!is_power_of_2((u32)GK20A_FECS_TRACE_NUM_RECORDS)) {
		nvgpu_err(g, "invalid NUM_RECORDS chosen");
		nvgpu_set_enabled(g, NVGPU_SUPPORT_FECS_CTXSW_TRACE, false);
		return -EINVAL;
	}

	trace = nvgpu_kzalloc(g, sizeof(struct nvgpu_gr_fecs_trace));
	if (trace == NULL) {
		nvgpu_err(g, "failed to allocate fecs_trace");
		nvgpu_set_enabled(g, NVGPU_SUPPORT_FECS_CTXSW_TRACE, false);
		return -ENOMEM;
	}
	g->fecs_trace = trace;

	nvgpu_mutex_init(&trace->poll_lock);
	nvgpu_mutex_init(&trace->list_lock);
	nvgpu_mutex_init(&trace->enable_lock);

	nvgpu_init_list_node(&trace->context_list);

	trace->enable_count = 0;

	err = nvgpu_periodic_timer_init(&trace->poll_timer,
			nvgpu_gr_fecs_trace_periodic_polling, g);
	if (err != 0) {
		nvgpu_err(g, "failed to create fecs_trace timer err=%d", err);
	}

	return err;
}

int nvgpu_gr_fecs_trace_deinit(struct gk20a *g)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;

	if (trace == NULL) {
		return 0;
	}

	/*
	 * Check if tracer was enabled before attempting to stop the
	 * tracer timer.
	 */
	if (trace->enable_count > 0) {
		nvgpu_periodic_timer_stop(&trace->poll_timer);
	}
	nvgpu_periodic_timer_destroy(&trace->poll_timer);

	nvgpu_gr_fecs_trace_remove_contexts(g, &trace->context_list);

	nvgpu_mutex_destroy(&g->fecs_trace->list_lock);
	nvgpu_mutex_destroy(&g->fecs_trace->poll_lock);
	nvgpu_mutex_destroy(&g->fecs_trace->enable_lock);

	nvgpu_kfree(g, g->fecs_trace);
	g->fecs_trace = NULL;
	return 0;
}

int nvgpu_gr_fecs_trace_num_ts(struct gk20a *g)
{
	return (int)((g->ops.gr.ctxsw_prog.hw_get_ts_record_size_in_bytes()
		- sizeof(struct nvgpu_fecs_trace_record)) / sizeof(u64));
}

struct nvgpu_fecs_trace_record *nvgpu_gr_fecs_trace_get_record(
	struct gk20a *g, int idx)
{
	struct nvgpu_gr_global_ctx_buffer_desc *gr_global_ctx_buffer =
				nvgpu_gr_get_global_ctx_buffer_ptr(g);
	struct nvgpu_mem *mem = nvgpu_gr_global_ctx_buffer_get_mem(
					gr_global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_FECS_TRACE_BUFFER);
	if (mem == NULL) {
		return NULL;
	}

	return (struct nvgpu_fecs_trace_record *)
		((u8 *) mem->cpu_va +
		((u32)idx * g->ops.gr.ctxsw_prog.hw_get_ts_record_size_in_bytes()));
}

bool nvgpu_gr_fecs_trace_is_valid_record(struct gk20a *g,
	struct nvgpu_fecs_trace_record *r)
{
	/*
	 * testing magic_hi should suffice. magic_lo is sometimes used
	 * as a sequence number in experimental ucode.
	 */
	return g->ops.gr.ctxsw_prog.is_ts_valid_record(r->magic_hi);
}

size_t nvgpu_gr_fecs_trace_buffer_size(struct gk20a *g)
{
	return GK20A_FECS_TRACE_NUM_RECORDS
			* g->ops.gr.ctxsw_prog.hw_get_ts_record_size_in_bytes();
}

int nvgpu_gr_fecs_trace_max_entries(struct gk20a *g,
		struct nvgpu_gpu_ctxsw_trace_filter *filter)
{
	int n;
	int tag;

	/* Compute number of entries per record, with given filter */
	for (n = 0, tag = 0; tag < nvgpu_gr_fecs_trace_num_ts(g); tag++)
		n += (NVGPU_GPU_CTXSW_FILTER_ISSET(tag, filter) != 0);

	/* Return max number of entries generated for the whole ring */
	return n * GK20A_FECS_TRACE_NUM_RECORDS;
}

int nvgpu_gr_fecs_trace_enable(struct gk20a *g)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	int write;
	int err = 0;

	nvgpu_mutex_acquire(&trace->enable_lock);
	trace->enable_count++;

	if (trace->enable_count == 1U) {
		/* drop data in hw buffer */
		if (g->ops.gr.fecs_trace.flush)
			g->ops.gr.fecs_trace.flush(g);

		write = g->ops.gr.fecs_trace.get_write_index(g);

		if (nvgpu_is_enabled(g, NVGPU_FECS_TRACE_FEATURE_CONTROL)) {
			/*
			 * For enabling FECS trace support, MAILBOX1's MSB
			 * (Bit 31:31) should be set to 1. Bits 30:0 represents
			 * actual pointer value.
			 */
			write = (int)((u32)write |
				(BIT32(NVGPU_FECS_TRACE_FEATURE_CONTROL_BIT)));
		}

		g->ops.gr.fecs_trace.set_read_index(g, write);

		/*
		 * FECS ucode does a priv holdoff around the assertion of
		 * context reset. So, pri transactions (e.g. mailbox1 register
		 * write) might fail due to this. Hence, do write with ack
		 * i.e. write and read it back to make sure write happened for
		 * mailbox1.
		 */
		while (g->ops.gr.fecs_trace.get_read_index(g) != write) {
			nvgpu_log(g, gpu_dbg_ctxsw, "mailbox1 update failed");
			g->ops.gr.fecs_trace.set_read_index(g, write);
		}

		err = nvgpu_periodic_timer_start(&trace->poll_timer,
				GK20A_FECS_TRACE_FRAME_PERIOD_NS);
		if (err != 0) {
			nvgpu_warn(g, "failed to start FECS polling timer");
			goto done;
		}
	}

done:
	nvgpu_mutex_release(&trace->enable_lock);
	return err;
}

int nvgpu_gr_fecs_trace_disable(struct gk20a *g)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	int read = 0;

	if (trace == NULL) {
		return -EINVAL;
	}

	nvgpu_mutex_acquire(&trace->enable_lock);
	if (trace->enable_count <= 0U) {
		nvgpu_mutex_release(&trace->enable_lock);
		return 0;
	}

	trace->enable_count--;
	if (trace->enable_count == 0U) {
		if (nvgpu_is_enabled(g, NVGPU_FECS_TRACE_FEATURE_CONTROL)) {
			/*
			 * For disabling FECS trace support, MAILBOX1's MSB
			 * (Bit 31:31) should be set to 0.
			 */
			read = (int)((u32)(g->ops.gr.fecs_trace.get_read_index(g)) &
				(~(BIT32(NVGPU_FECS_TRACE_FEATURE_CONTROL_BIT))));

			g->ops.gr.fecs_trace.set_read_index(g, read);

			/*
			 * FECS ucode does a priv holdoff around the assertion
			 * of context reset. So, pri transactions (e.g.
			 * mailbox1 register write) might fail due to this.
			 * Hence, do write with ack i.e. write and read it back
			 * to make sure write happened for mailbox1.
			 */
			while (g->ops.gr.fecs_trace.get_read_index(g) != read) {
				nvgpu_log(g, gpu_dbg_ctxsw,
						"mailbox1 update failed");
				g->ops.gr.fecs_trace.set_read_index(g, read);
			}
		}
		nvgpu_periodic_timer_stop(&trace->poll_timer);
	}
	nvgpu_mutex_release(&trace->enable_lock);

	return 0;
}

bool nvgpu_gr_fecs_trace_is_enabled(struct gk20a *g)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;

	return (trace && (trace->enable_count > 0));
}

void nvgpu_gr_fecs_trace_reset_buffer(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_fn|gpu_dbg_ctxsw, " ");

	g->ops.gr.fecs_trace.set_read_index(g,
		g->ops.gr.fecs_trace.get_write_index(g));
}

/*
 * Converts HW entry format to userspace-facing format and pushes it to the
 * queue.
 */
int nvgpu_gr_fecs_trace_ring_read(struct gk20a *g, int index,
	u32 *vm_update_mask)
{
	int i;
	struct nvgpu_gpu_ctxsw_trace_entry entry = { };
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	pid_t cur_pid = 0, new_pid = 0;
	u32 cur_vmid = 0U, new_vmid = 0U;
	u32 vmid = 0U;
	int count = 0;

	struct nvgpu_fecs_trace_record *r =
		nvgpu_gr_fecs_trace_get_record(g, index);
	if (r == NULL) {
		return -EINVAL;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_ctxsw,
		"consuming record trace=%p read=%d record=%p", trace, index, r);

	if (!nvgpu_gr_fecs_trace_is_valid_record(g, r)) {
		nvgpu_warn(g,
			"trace=%p read=%d record=%p magic_lo=%08x magic_hi=%08x (invalid)",
			trace, index, r, r->magic_lo, r->magic_hi);
		return -EINVAL;
	}

	/* Clear magic_hi to detect cases where CPU could read write index
	 * before FECS record is actually written to DRAM. This should not
	 * as we force FECS writes to SYSMEM by reading through PRAMIN.
	 */
	r->magic_hi = 0;

	if ((r->context_ptr != 0U) && (r->context_id != 0U)) {
		nvgpu_gr_fecs_trace_find_pid(g, r->context_ptr,
			&trace->context_list, &cur_pid, &cur_vmid);
	} else {
		cur_vmid = 0xffffffffU;
		cur_pid = 0;
	}

	if (r->new_context_ptr != 0U) {
		nvgpu_gr_fecs_trace_find_pid(g, r->new_context_ptr,
			&trace->context_list, &new_pid, &new_vmid);
	} else {
		new_vmid = 0xffffffffU;
		new_pid = 0;
	}

	nvgpu_log(g, gpu_dbg_ctxsw,
		"context_ptr=%x (vmid=%u pid=%d)",
		r->context_ptr, cur_vmid, cur_pid);
	nvgpu_log(g, gpu_dbg_ctxsw,
		"new_context_ptr=%x (vmid=%u pid=%d)",
		r->new_context_ptr, new_vmid, new_pid);

	entry.context_id = r->context_id;

	/* break out FECS record into trace events */
	for (i = 0; i < nvgpu_gr_fecs_trace_num_ts(g); i++) {

		entry.tag = (u8)g->ops.gr.ctxsw_prog.hw_get_ts_tag(r->ts[i]);
		entry.timestamp =
			g->ops.gr.ctxsw_prog.hw_record_ts_timestamp(r->ts[i]);
		entry.timestamp <<= GK20A_FECS_TRACE_PTIMER_SHIFT;

		nvgpu_log(g, gpu_dbg_ctxsw,
			"tag=%x timestamp=%llx context_id=%08x new_context_id=%08x",
			entry.tag, entry.timestamp, r->context_id,
			r->new_context_id);

		switch (nvgpu_gpu_ctxsw_tags_to_common_tags(entry.tag)) {
		case NVGPU_GPU_CTXSW_TAG_RESTORE_START:
		case NVGPU_GPU_CTXSW_TAG_CONTEXT_START:
			entry.context_id = r->new_context_id;
			entry.pid = (u64)new_pid;
			entry.vmid = (u8)new_vmid;
			break;

		case NVGPU_GPU_CTXSW_TAG_CTXSW_REQ_BY_HOST:
		case NVGPU_GPU_CTXSW_TAG_FE_ACK:
		case NVGPU_GPU_CTXSW_TAG_FE_ACK_WFI:
		case NVGPU_GPU_CTXSW_TAG_FE_ACK_GFXP:
		case NVGPU_GPU_CTXSW_TAG_FE_ACK_CTAP:
		case NVGPU_GPU_CTXSW_TAG_FE_ACK_CILP:
		case NVGPU_GPU_CTXSW_TAG_SAVE_END:
			entry.context_id = r->context_id;
			entry.pid = (u64)cur_pid;
			entry.vmid = (u8)cur_vmid;
			break;

		default:
			/* tags are not guaranteed to start at the beginning */
			if ((entry.tag != 0) && (entry.tag !=
				    NVGPU_GPU_CTXSW_TAG_INVALID_TIMESTAMP)) {
				nvgpu_warn(g, "TAG not found");
			}
			continue;
		}

		nvgpu_log(g, gpu_dbg_ctxsw, "tag=%x context_id=%x pid=%lld",
			entry.tag, entry.context_id, entry.pid);

		if (!entry.context_id)
			continue;

		if (g->ops.gr.fecs_trace.vm_dev_write != NULL) {
			g->ops.gr.fecs_trace.vm_dev_write(g, entry.vmid,
				vm_update_mask, &entry);
		} else {
			nvgpu_gr_fecs_trace_write_entry(g, &entry);
		}
		count++;
	}

	nvgpu_gr_fecs_trace_wake_up(g, (int)vmid);
	return count;
}

int nvgpu_gr_fecs_trace_poll(struct gk20a *g)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	u32 vm_update_mask = 0U;
	int read = 0;
	int write = 0;
	int cnt;
	int err = 0;

	nvgpu_mutex_acquire(&trace->poll_lock);
	if (trace->enable_count == 0) {
		goto done_unlock;
	}

	err = gk20a_busy(g);
	if (err) {
		goto done_unlock;
	}

	write = g->ops.gr.fecs_trace.get_write_index(g);
	if ((write < 0) || (write >= GK20A_FECS_TRACE_NUM_RECORDS)) {
		nvgpu_err(g,
			"failed to acquire write index, write=%d", write);
		err = write;
		goto done;
	}

	read = g->ops.gr.fecs_trace.get_read_index(g);

	cnt = CIRC_CNT((u32)write, (u32)read, GK20A_FECS_TRACE_NUM_RECORDS);
	if (!cnt)
		goto done;

	nvgpu_log(g, gpu_dbg_ctxsw,
		"circular buffer: read=%d (mailbox=%d) write=%d cnt=%d",
		read, g->ops.gr.fecs_trace.get_read_index(g), write, cnt);

	/* Ensure all FECS writes have made it to SYSMEM */
	err = g->ops.mm.cache.fb_flush(g);
	if (err != 0) {
		nvgpu_err(g, "mm.cache.fb_flush() failed err=%d", err);
		goto done;
	}

	if (nvgpu_is_enabled(g, NVGPU_FECS_TRACE_FEATURE_CONTROL)) {
		/* Bits 30:0 of MAILBOX1 represents actual read pointer value */
		read = ((u32)read) & (~(BIT32(NVGPU_FECS_TRACE_FEATURE_CONTROL_BIT)));
	}

	while (read != write) {
		cnt = nvgpu_gr_fecs_trace_ring_read(g, read, &vm_update_mask);
		if (cnt <= 0) {
			break;
		}

		/* Get to next record. */
		read = (read + 1) & (GK20A_FECS_TRACE_NUM_RECORDS - 1);
	}

	if (nvgpu_is_enabled(g, NVGPU_FECS_TRACE_FEATURE_CONTROL)) {
		/*
		 * In the next step, read pointer is going to be updated.
		 * So, MSB of read pointer should be set back to 1. This will
		 * keep FECS trace enabled.
		 */
		read = (int)(((u32)read) | (BIT32(NVGPU_FECS_TRACE_FEATURE_CONTROL_BIT)));
	}

	/* ensure FECS records has been updated before incrementing read index */
	nvgpu_wmb();
	g->ops.gr.fecs_trace.set_read_index(g, read);

	/*
	 * FECS ucode does a priv holdoff around the assertion of context
	 * reset. So, pri transactions (e.g. mailbox1 register write) might
	 * fail due to this. Hence, do write with ack i.e. write and read
	 * it back to make sure write happened for mailbox1.
	 */
	while (g->ops.gr.fecs_trace.get_read_index(g) != read) {
		nvgpu_log(g, gpu_dbg_ctxsw, "mailbox1 update failed");
		g->ops.gr.fecs_trace.set_read_index(g, read);
	}

	if (g->ops.gr.fecs_trace.vm_dev_update) {
		g->ops.gr.fecs_trace.vm_dev_update(g, vm_update_mask);
	}

done:
	gk20a_idle(g);
done_unlock:
	nvgpu_mutex_release(&trace->poll_lock);
	return err;
}

static void nvgpu_gr_fecs_trace_periodic_polling(void *arg)
{
	struct gk20a *g = (struct gk20a *)arg;
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;

	if (trace->enable_count > 0U) {
		nvgpu_gr_fecs_trace_poll(g);
	}
}

int nvgpu_gr_fecs_trace_reset(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_fn|gpu_dbg_ctxsw, " ");

	if (!g->ops.gr.fecs_trace.is_enabled(g))
		return 0;

	nvgpu_gr_fecs_trace_poll(g);
	return g->ops.gr.fecs_trace.set_read_index(g, 0);
}

/*
 * map global circ_buf to the context space and store the GPU VA
 * in the context header.
 */
int nvgpu_gr_fecs_trace_bind_channel(struct gk20a *g,
	struct nvgpu_mem *inst_block, struct nvgpu_gr_subctx *subctx,
	struct nvgpu_gr_ctx *gr_ctx, pid_t pid, u32 vmid)
{
	u64 addr = 0ULL;
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	struct nvgpu_mem *mem;
	struct nvgpu_gr_global_ctx_buffer_desc *gr_global_ctx_buffer =
				nvgpu_gr_get_global_ctx_buffer_ptr(g);
	u32 context_ptr;
	u32 aperture_mask;
	int ret;

	if (trace == NULL) {
		return -EINVAL;
	}

	context_ptr = nvgpu_inst_block_ptr(g, inst_block);

	nvgpu_log(g, gpu_dbg_fn|gpu_dbg_ctxsw,
			"pid=%d context_ptr=%x inst_block=%llx",
			pid, context_ptr,
			nvgpu_inst_block_addr(g, inst_block));

	mem = nvgpu_gr_global_ctx_buffer_get_mem(gr_global_ctx_buffer,
					NVGPU_GR_GLOBAL_CTX_FECS_TRACE_BUFFER);
	if (mem == NULL) {
		return -EINVAL;
	}

	if (nvgpu_is_enabled(g, NVGPU_FECS_TRACE_VA)) {
		addr = nvgpu_gr_ctx_get_global_ctx_va(gr_ctx,
				NVGPU_GR_CTX_FECS_TRACE_BUFFER_VA);
		nvgpu_log(g, gpu_dbg_ctxsw, "gpu_va=%llx", addr);
		aperture_mask = 0;
	} else {
		addr = nvgpu_inst_block_addr(g, mem);
		nvgpu_log(g, gpu_dbg_ctxsw, "pa=%llx", addr);
		aperture_mask =
		       g->ops.gr.ctxsw_prog.get_ts_buffer_aperture_mask(g, mem);
	}
	if (addr == 0ULL) {
		return -ENOMEM;
	}

	mem = nvgpu_gr_ctx_get_ctx_mem(gr_ctx);

	nvgpu_log(g, gpu_dbg_ctxsw, "addr=%llx count=%d", addr,
		GK20A_FECS_TRACE_NUM_RECORDS);

	g->ops.gr.ctxsw_prog.set_ts_num_records(g, mem,
		GK20A_FECS_TRACE_NUM_RECORDS);

	if (nvgpu_is_enabled(g, NVGPU_FECS_TRACE_VA) && subctx != NULL) {
		mem = nvgpu_gr_subctx_get_ctx_header(subctx);
	}

	g->ops.gr.ctxsw_prog.set_ts_buffer_ptr(g, mem, addr, aperture_mask);

	ret = nvgpu_gr_fecs_trace_add_context(g, context_ptr, pid, vmid,
		&trace->context_list);

	return ret;
}

int nvgpu_gr_fecs_trace_unbind_channel(struct gk20a *g,
	struct nvgpu_mem *inst_block)
{
	struct nvgpu_gr_fecs_trace *trace = g->fecs_trace;
	u32 context_ptr;

	if (trace == NULL) {
		return -EINVAL;
	}

	context_ptr = nvgpu_inst_block_ptr(g, inst_block);

	nvgpu_log(g, gpu_dbg_fn|gpu_dbg_ctxsw,
		"context_ptr=%x", context_ptr);

	if (g->ops.gr.fecs_trace.is_enabled(g)) {
		if (g->ops.gr.fecs_trace.flush) {
			g->ops.gr.fecs_trace.flush(g);
		}
		nvgpu_gr_fecs_trace_poll(g);
	}

	nvgpu_gr_fecs_trace_remove_context(g, context_ptr,
		&trace->context_list);

	return 0;
}
