/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_INTR_PRIV_H
#define NVGPU_GR_INTR_PRIV_H

#include <nvgpu/types.h>
#include <nvgpu/lock.h>
#include <include/nvgpu/gr/gr_falcon.h>

struct nvgpu_channel;

/**
 * Size of lookup buffer used for context translation to GPU channel
 * and TSG identifiers.
 * This value must be a power of 2.
 */
#define GR_CHANNEL_MAP_TLB_SIZE		2U

/**
 * GR interrupt information struct.
 *
 * This structure maintains information on pending GR engine interrupts.
 */
struct nvgpu_gr_intr_info {
	/**
	 * This value is set in case notification interrupt is pending.
	 * Same value is used to clear the interrupt.
	 */
	u32 notify;
	/**
	 * This value is set in case semaphore interrupt is pending.
	 * Same value is used to clear the interrupt.
	 */
	u32 semaphore;
	/**
	 * This value is set in case illegal notify interrupt is pending.
	 * Same value is used to clear the interrupt.
	 */
	u32 illegal_notify;
	/**
	 * This value is set in case illegal method interrupt is pending.
	 * Same value is used to clear the interrupt.
	 */
	u32 illegal_method;
	/**
	 * This value is set in case illegal class interrupt is pending.
	 * Same value is used to clear the interrupt.
	 */
	u32 illegal_class;
	/**
	 * This value is set in case FECS error interrupt is pending.
	 * Same value is used to clear the interrupt.
	 */
	u32 fecs_error;
	/**
	 * This value is set in case illegal class interrupt is pending.
	 * Same value is used to clear the interrupt.
	 */
	u32 class_error;
	/**
	 * This value is set in case firmware method interrupt is pending.
	 * Same value is used to clear the interrupt.
	 */
	u32 fw_method;
	/**
	 * This value is set in case exception is pending in graphics pipe.
	 * Same value is used to clear the interrupt.
	 */
	u32 exception;
	/*
	 * This value is set when the FE receives a valid method and it
	 * matches with the value configured in PRI_FE_DEBUG_METHOD_* pri
	 * registers; In case of a match, FE proceeds to drop that method.
	 * This provides a way to the SW to turn off HW decoding of this
	 * method and convert it to a SW method.
	 */
	u32 debug_method;
	/*
	 * This value is set on the completion of a LaunchDma method with
	 * InterruptType field configured to INTERRUPT.
	 */
	u32 buffer_notify;
};

/**
 * TPC exception data structure.
 *
 * TPC exceptions can be decomposed into exceptions triggered by its
 * subunits. This structure keeps track of which subunits have
 * triggered exception.
 */
struct nvgpu_gr_tpc_exception {
	/**
	 * This flag is set in case TEX exception is pending.
	 */
	bool tex_exception;
	/**
	 * This flag is set in case SM exception is pending.
	 */
	bool sm_exception;
	/**
	 * This flag is set in case MPC exception is pending.
	 */
	bool mpc_exception;
	/**
	 * This flag is set in case PE exception is pending.
	 */
	bool pe_exception;
};

/**
 * GR ISR data structure.
 *
 * This structure holds all necessary information to handle all GR engine
 * error/exception interrupts.
 */
struct nvgpu_gr_isr_data {
	/**
	 * Contents of TRAPPED_ADDR register used to decode below
	 * fields.
	 */
	u32 addr;
	/**
	 * Low word of the trapped method data.
	 */
	u32 data_lo;
	/**
	 * High word of the trapped method data.
	 */
	u32 data_hi;
	/**
	 * Information of current context.
	 */
	u32 curr_ctx;
	/**
	 * Pointer to faulted GPU channel.
	 */
	struct nvgpu_channel *ch;
	/**
	 * Address of the trapped method.
	 */
	u32 offset;
	/**
	 * Subchannel ID of the trapped method.
	 */
	u32 sub_chan;
	/**
	 * Class ID corresponding to above subchannel.
	 */
	u32 class_num;
	/**
	 * Value read from fecs_host_int_status h/w reg.
	 */
	u32 fecs_intr;
	/**
	 * S/W defined status for fecs_host_int_status.
	 */
	struct nvgpu_fecs_host_intr_status fecs_host_intr_status;
};

/**
 * Details of lookup buffer used to translate context to GPU
 * channel/TSG identifiers.
 */
struct gr_channel_map_tlb_entry {
	/**
	 * Information of context.
	 */
	u32 curr_ctx;
	/**
	 * GPU channel ID.
	 */
	u32 chid;
	/**
	 * GPU Time Slice Group  ID.
	 */
	u32 tsgid;
};

/**
 * GR interrupt management data structure.
 *
 * This structure holds various fields to manage GR engine interrupt
 * handling.
 */
struct nvgpu_gr_intr {
	/**
	 * Lookup buffer structure used to translate context to GPU
	 * channel and TSG identifiers.
	 */
	struct gr_channel_map_tlb_entry chid_tlb[GR_CHANNEL_MAP_TLB_SIZE];
	/**
	 * Entry in lookup buffer that should be overwritten if there is
	 * no remaining free entry.
	 */
	u32 channel_tlb_flush_index;
	/**
	 * Spinlock for all lookup buffer accesses.
	 */
	struct nvgpu_spinlock ch_tlb_lock;
};

#endif /* NVGPU_GR_INTR_PRIV_H */

