/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_ERR_INFO_H
#define NVGPU_ERR_INFO_H

/**
 * @file
 *
 * Declare the format of error message for various hw units in GPU and add
 * designated initializers for them.
 */

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_hw_err_inject_info;

/**
 * gpu_err_header structure holds fields which are required to identify the
 * version of header, sub-error type, sub-unit id, error address and time stamp.
 */
struct gpu_err_header {
	/** Version of GPU error header. */
	struct {
		/** Major version number. */
		u16 major;
		/** Minor version number. */
		u16 minor;
	} version;

	/** Sub error type corresponding to the error that is being reported. */
	u32 sub_err_type;

	/** ID of the sub-unit in a HW unit which encountered an error. */
	u64 sub_unit_id;

	/** Location of the error. */
	u64 address;

	/** Timestamp in nano seconds. */
	u64 timestamp_ns;
};

struct gpu_host_error_info {
	struct gpu_err_header header;
};

struct gpu_ecc_error_info {
	struct gpu_err_header header;

	/** Number of ECC errors. */
	u64 err_cnt;
};

struct gpu_gr_error_info {
	struct gpu_err_header header;

	/** Context which triggerd exception. */
	u32 curr_ctx;

	/** Channel bound to the context. */
	u32 chid;

	/** TSG to which the channel is bound. */
	u32 tsgid;

	/** Exception status. */
	u32 status;
};

struct gpu_sm_error_info {
	struct gpu_err_header header;

	/** PC when exception was triggered. */
	u64 warp_esr_pc;

	/** SM error status. */
	u32 warp_esr_status;

	/** Current context which triggered exception. */
	u32 curr_ctx;

	/** Channel ID. */
	u32 chid;

	/** TSG ID. */
	u32 tsgid;

	/** IDs of TPC, GPC, and SM. */
	u32 tpc, gpc, sm;
};

/**
 * This structure describes the various debug information reported
 * by GMMU during MMU page fault exceptions. The details of each
 * member in this struct can be found in mmu_fault_info structure
 * defined in
 *
 *  + drivers/gpu/nvgpu/include/nvgpu/mmu_fault.h
 */
struct mmu_page_fault_info {
	u64	inst_ptr;
	u32	inst_aperture;
	u64	fault_addr;
	u32	fault_addr_aperture;
	u32	timestamp_lo;
	u32	timestamp_hi;
	u32	mmu_engine_id;
	u32	gpc_id;
	u32	client_type;
	u32	client_id;
	u32	fault_type;
	u32	access_type;
	u32	protected_mode;
	bool	replayable_fault;
	u32	replay_fault_en;
	bool	valid;
	u32	faulted_pbdma;
	u32	faulted_engine;
	u32	faulted_subid;
	u32	chid;
};

struct gpu_mmu_error_info {
	struct gpu_err_header header;

	struct mmu_page_fault_info info;

	/** MMU page fault status. */
	u32 status;
};

struct gpu_ce_error_info {
	struct gpu_err_header header;
};

struct gpu_pri_error_info {
	struct gpu_err_header header;
};

struct gpu_pmu_error_info {
	struct gpu_err_header header;

	/** PMU bar0 error status value. */
	u32 status;
};

struct gpu_ctxsw_error_info {
	struct gpu_err_header header;

	/** Current context. */
	u32 curr_ctx;

	/** TSG ID. */
	u32 tsgid;

	/** ChanneDl ID. */
	u32 chid;

	/** Context switch status registers. */
	u32 ctxsw_status0, ctxsw_status1;

	/** Mailbox value. */
	u32 mailbox_value;
};

/**
 * gpu_error_info structure holds fields for error info related to each hardware
 * unit whose errors will be reported.
 */
union gpu_error_info {
	struct gpu_host_error_info host_info;
	struct gpu_ecc_error_info ecc_info;
	struct gpu_gr_error_info gr_info;
	struct gpu_sm_error_info sm_info;
	struct gpu_ce_error_info ce_info;
	struct gpu_pri_error_info pri_info;
	struct gpu_pmu_error_info pmu_err_info;
	struct gpu_ctxsw_error_info ctxsw_info;
	struct gpu_mmu_error_info mmu_info;
};

/**
 * nvgpu_err_msg structure holds fields which are required to identify the
 * source and type and criticality of the reported error.
 */
struct nvgpu_err_msg {
	/**
	 * Identify the hw module which generated the error. List of supported
	 * hw modules and errors can be found in
	 *
	 *  + drivers/gpu/nvgpu/include/nvgpu/nvgpu_err.h
	 */
	u32 hw_unit_id;

	/** Flag to indicate the criticality of the error. */
	bool is_critical;

	/** Error ID. */
	u8 err_id;

	/** Size of the error message. */
	u8 err_size;

	/** GPU error information. */
	union gpu_error_info err_info;

	/** Used to get error information from look-up table. */
	struct nvgpu_err_desc *err_desc;
};

/**
 * This macro is used to initialize the members of nvgpu_err_desc struct.
 */
#define GPU_ERR(err, critical, id, inject_support, hw_inject_fn, sw_inject_fn,\
		addr, val, threshold, ecount)				\
{									\
		.name = (err),						\
		.is_critical = (critical),				\
		.error_id = (id),					\
		.err_inject_info.type = (inject_support),		\
		.err_threshold = (threshold),				\
		.err_count = (ecount),					\
		.inject_hw_fault = (hw_inject_fn),			\
		.inject_sw_fault = (sw_inject_fn),			\
		.err_inject_info.get_reg_addr = (addr),			\
		.err_inject_info.get_reg_val = (val)			\
}

/**
 * This macro is used to initialize critical errors.
 */
#define GPU_CRITERR(err, id, inject_support, hw_inject_fn, sw_inject_fn, addr,\
		val, threshold, ecount) \
NVGPU_COV_WHITELIST(false_positive, NVGPU_MISRA(Rule, 10_3), "Bug 2623654") \
	GPU_ERR(err, true, id, inject_support, hw_inject_fn, sw_inject_fn, \
		addr, val, threshold, ecount)

/**
 * This macro is used to initialize non-critical errors.
 */
#define GPU_NONCRITERR(err, id, inject_support, hw_inject_fn, sw_inject_fn,\
		addr, val, threshold, ecount) \
NVGPU_COV_WHITELIST(false_positive, NVGPU_MISRA(Rule, 10_3), "Bug 2623654") \
	GPU_ERR(err, false, id, inject_support, hw_inject_fn, sw_inject_fn,\
		addr, val, threshold, ecount)

/**
 * This defines the various types of error injection supported in SDL.
 */
#define INJECT_NONE	(0U)
#define INJECT_HW	(1U)
#define INJECT_SW	(2U)

struct nvgpu_hw_err_inject_info;

/**
 * nvgpu_err_desc structure holds fields which describe an error along with
 * function callback which can be used to inject the error.
 */
struct nvgpu_err_desc {
	/** String representation of error. */
	const char *name;

	/** Flag to classify an error as critical or non-critical. */
	bool is_critical;

	/**
	 * Error Threshold: once this threshold value is reached, then the
	 * corresponding error counter will be reset to 0 and the error will be
	 * propagated to Safety_Services.
	 */
	int err_threshold;

	/**
	 * Total number of times an error has occurred (since its last reset).
	 */
	int err_count;

	/** Function to support HW based error injection. */
	void (*inject_hw_fault)(struct gk20a *g,
			struct nvgpu_hw_err_inject_info *err, u32 err_info);

	/** Function to support SW based error injection. */
	void (*inject_sw_fault)(struct gk20a *g,
			u32 hw_unit, u32 err_index, u32 inst);

	/** Error ID. */
	u8 error_id;

	/**
	 * err_inject_info structure describes the type of error injection and
	 * the required register address and a write value.
	 */
	struct err_inject_info {
		/** Type of error injection: HW / SW / None. */
		u32 type;
		/** Function to get register address for error injection. */
		u32 (*get_reg_addr)(void);
		/** Function to get register value for error injection. */
		u32 (*get_reg_val)(u32 val);
	} err_inject_info;
};

/**
 * nvgpu_err_hw_module structure holds fields which describe the h/w modules
 * error reporting capabilities.
 */
struct nvgpu_err_hw_module {
	/** String representation of a given HW unit. */
	const char *name;

	/** HW unit ID. */
	u32 hw_unit;

	/** Total number of instances of a given HW unit. */
	u32 num_instances;

	/** Total number of errors reported from a given HW unit. */
	u32 num_errs;

	/** Used to get error description from look-up table. */
	struct nvgpu_err_desc *errs;
};

#endif
