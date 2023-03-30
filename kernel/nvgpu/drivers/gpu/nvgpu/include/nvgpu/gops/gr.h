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
#ifndef NVGPU_GOPS_GR_H
#define NVGPU_GOPS_GR_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * GR HAL interface.
 */
struct gk20a;
struct vm_gk20a;
struct nvgpu_mem;
struct nvgpu_channel;
struct nvgpu_gr_ctx;
struct nvgpu_gr_config;
struct nvgpu_gr_isr_data;
struct nvgpu_gr_intr_info;
struct nvgpu_gr_falcon;
struct nvgpu_gr_falcon_query_sizes;
struct nvgpu_gr_tpc_exception;
struct nvgpu_fecs_method_op;
struct nvgpu_fecs_ecc_status;
struct nvgpu_fecs_host_intr_status;
struct netlist_av_list;
struct nvgpu_hw_err_inject_info_desc;
struct nvgpu_gr_sm_ecc_status;
struct nvgpu_gr_zbc_table_indices;
struct nvgpu_gr_obj_ctx_gfx_regs;

enum nvgpu_gr_sm_ecc_error_types;

#ifdef CONFIG_NVGPU_FECS_TRACE
struct nvgpu_gr_subctx;
struct nvgpu_gpu_ctxsw_trace_filter;
struct nvgpu_gpu_ctxsw_trace_entry;
#endif

#ifdef CONFIG_NVGPU_GRAPHICS
struct nvgpu_gr_zbc;
struct nvgpu_gr_zbc_entry;
struct nvgpu_gr_zbc_query_params;
struct nvgpu_gr_zcull;
struct nvgpu_gr_zcull_info;
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
struct nvgpu_debug_context;
struct nvgpu_warpstate;
struct dbg_session_gk20a;
struct netlist_aiv_list;
struct ctxsw_buf_offset_map_entry;

enum ctxsw_addr_type;
enum nvgpu_event_id_type;
#endif
struct netlist_av64_list;

/**
 * This structure stores the GR engine ecc subunit hal pointers.
 *
 * @see gops_gr
 */
struct gops_gr_ecc {
	/**
	 * @brief Initialize GR unit ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function allocates memory to track the ecc error counts
	 * for GR unit and subunits of GR (like GPCs, TPCs etc).
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*gpc_tpc_ecc_init)(struct gk20a *g);

	/**
	 * @brief Initialize GR unit ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function allocates memory to track the ecc error counts
	 * for FECS in GR.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*fecs_ecc_init)(struct gk20a *g);

	/**
	 * @brief Deinitialize GR unit ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function deallocates memory to track the ecc error counts
	 * for GR unit and subunits of GR (like GPCs, TPCs etc) and removes
	 * it from global list.
	 */
	void (*gpc_tpc_ecc_deinit)(struct gk20a *g);

	/**
	 * @brief Deinitialize GR unit ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function deallocates memory to track the ecc error counts
	 * for FECS in GR and removes it from global list.
	 */
	void (*fecs_ecc_deinit)(struct gk20a *g);

	/**
	 * @brief Detect ECC enabled units in GR engine.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function check the feature override ecc registers
	 * to figure out whether the feature is enabled or disabled.
	 * This function enables the GR SM_ECC and LTC_ECC feature,
	 * on checking the fuses override register and opt ecc enable
	 * register.
	 */
	void (*detect)(struct gk20a *g);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	struct nvgpu_hw_err_inject_info_desc * (*get_mmu_err_desc)
						(struct gk20a *g);
	struct nvgpu_hw_err_inject_info_desc * (*get_gcc_err_desc)
						(struct gk20a *g);
	struct nvgpu_hw_err_inject_info_desc * (*get_sm_err_desc)
						(struct gk20a *g);
	struct nvgpu_hw_err_inject_info_desc * (*get_gpccs_err_desc)
						(struct gk20a *g);
	struct nvgpu_hw_err_inject_info_desc * (*get_fecs_err_desc)
						(struct gk20a *g);
	/** @endcond */
};


/**
 * This structure stores the GR engine setup subunit hal pointers.
 *
 * @see gops_gr
 */
struct gops_gr_setup {
	/**
	 * @brief Allocate and setup object context s/w image
	 *        for GPU channel.
	 *
	 * @param c [in]		Pointer to GPU channel.
	 * @param class_num [in]	GPU class ID.
	 * @param flags [in]		Flags for context allocation.
	 *
	 * This HAL allocates and sets up object context for
	 * a GPU channel. This HAL always maps to
	 * #nvgpu_gr_setup_alloc_obj_ctx.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ENOMEM if memory allocation fails for
	 *          any context image.
	 * @retval -EINVAL if invalid GPU class ID is provided.
	 *
	 * @see nvgpu_gr_setup_alloc_obj_ctx
	 */
	int (*alloc_obj_ctx)(struct nvgpu_channel  *c,
			     u32 class_num, u32 flags);

	/**
	 * @brief Free GR engine context image.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 * @param vm [in]	Pointer to virtual memory.
	 * @param gr_ctx [in]	Pointer to GR engine context image.
	 *
	 * This function will free memory allocated for patch
	 * context image and GR engine context image in
	 * #alloc_obj_ctx.
	 * This HAL maps to #nvgpu_gr_setup_free_gr_ctx.
	 *
	 * @see nvgpu_gr_setup_free_gr_ctx
	 */
	void (*free_gr_ctx)(struct gk20a *g,
			    struct vm_gk20a *vm,
			    struct nvgpu_gr_ctx *gr_ctx);

	/**
	 * @brief Free GR engine subcontext.
	 *
	 * @param c [in]			Pointer to GPU channel.
	 *
	 * This function will free memory allocated for GR engine
	 * subcontext image in #alloc_obj_ctx.
	 * This HAL maps to #nvgpu_gr_setup_free_subctx.
	 *
	 * @see nvgpu_gr_setup_free_subctx
	 */
	void (*free_subctx)(struct nvgpu_channel *c);

	/**
	 * @brief Setup preemption mode in GR engine context image.
	 *
	 * @param ch [in]			Pointer to GPU channel.
	 * @param graphics_preempt_mode [in]	Requested graphics
	 *					preemption mode.
	 * @param compute_preempt_mode [in]	Requested compute
	 *					preemption mode.
	 * @param gr_instance_id[in]		Instance ID of
	 * 					graphics engine.
	 *
	 * This function will program newly requested preemption modes
	 * into GR engine context image.
	 * This HAL maps to #nvgpu_gr_setup_set_preemption_mode.
	 *
	 * Note that if requested preemption modes are already set,
	 * this function will return 0. The function supports
	 * NVGPU_PREEMTION_MODE_GRAPHICS_WFI graphics preemption mode and
	 * NVGPU_PREEMTION_MODE_COMPUTE_WFI, NVGPU_PREEMTION_MODE_COMPUTE_CTA
	 * compute preemption modes.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -EINVAL if invalid preemption modes are provided.
	 * @retval -EINVAL if invalid GPU channel pointer is provided.
	 *
	 * @see nvgpu_gr_setup_set_preemption_mode
	 */
	int (*set_preemption_mode)(struct nvgpu_channel *ch,
				   u32 graphics_preempt_mode,
				   u32 compute_preempt_mode,
				   u32 gr_instance_id);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_GRAPHICS
	int (*bind_ctxsw_zcull)(struct gk20a *g,
				struct nvgpu_channel *c,
				u64 zcull_va,
				u32 mode);
#endif
	/** @endcond */
};

/**
 * This structure stores the GR engine falcon subunit hal pointers.
 *
 * @see gops_gr
 */
struct gops_gr_falcon {
	/**
	 * @brief Read context switch mailbox.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param reg_index [in]	Register Index value.
	 *
	 * This function reads the context switch mailbox for the
	 * register index.
	 *
	 * @return context switch mailbox register value.
	 */
	u32 (*read_fecs_ctxsw_mailbox)(struct gk20a *g,
				       u32 reg_index);

	/**
	 * @brief Clear context switch mailbox for bitmask speciifed.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param reg_index [in]	Register Index value.
	 *        clear_val [in]	Bitmask of bits to be clear.
	 *
	 * This function clears specified bitmask of context switch mailbox
	 * register value.
	 */
	void (*fecs_ctxsw_clear_mailbox)(struct gk20a *g,
				u32 reg_index, u32  clear_val);

	/**
	 * @brief Dump context switch mailbox register values.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function reads and prints all
	 * - fecs context switch mailbox values
	 * - gpccs context switch mailbox values for all available gpcs
	 * This is helpful for ucode debugging.
	 */
	void (*dump_stats)(struct gk20a *g);

	/**
	 * @brief Get context switch register Major revision Id.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function reads the Major revision id. This id is used
	 * to check which version of the firmware ucode to use.
	 *
	 * @return context switch Major revision Id.
	 */
	u32 (*get_fecs_ctx_state_store_major_rev_id)(struct gk20a *g);

	/**
	 * @brief Control the context switch methods and data.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param fecs_method [in]	Fecs Method.
	 * @param fecs_data [in]	Fecs Data.
	 * @param ret_val [in]		Pointer to mailbox return value.
	 *
	 * This function helps to pass the fecs methods and data from
	 * user to the firmware submitting through mailbox registers.
	 * The ucode status is checked to see whether the method
	 * failed/timedout or passed.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*ctrl_ctxsw)(struct gk20a *g, u32 fecs_method,
			  u32 fecs_data, u32 *ret_val);

	/**
	 * @brief Wait for scrubbing of IMEM and DMEM of FECS and GPCCS
	 *        falcons to complete with a timeout of
	 *        \a CTXSW_MEM_SCRUBBING_TIMEOUT_MAX_US.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ETIMEDOUT if falcon scrubbing timed out.
	 */
	int (*wait_mem_scrubbing)(struct gk20a *g);

	/**
	 * @brief Ensure CTXSW falcon is ready and CTXSW watchdog
	 *        timeout is configured.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * Wait for CTXSW falcon to get ready by waiting for upto
	 * \a NVGPU_DEFAULT_POLL_TIMEOUT_MS to get correct response
	 * codes in falcon mailboxes.
	 * Configure CTXSW watchdog timeout with value of
	 * \a CTXSW_WDT_DEFAULT_VALUE.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ETIMEDOUT if communication with falcon timed out.
	 */
	int (*wait_ctxsw_ready)(struct gk20a *g);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	void (*handle_fecs_ecc_error)(struct gk20a *g,
		struct nvgpu_fecs_ecc_status *fecs_ecc_status);
	void (*fecs_host_clear_intr)(struct gk20a *g,
				     u32 fecs_intr);
	u32 (*fecs_host_intr_status)(struct gk20a *g,
		struct nvgpu_fecs_host_intr_status *fecs_host_intr);
	u32 (*fecs_base_addr)(void);
	u32 (*gpccs_base_addr)(void);
	void (*set_current_ctx_invalid)(struct gk20a *g);
	u32 (*fecs_ctxsw_mailbox_size)(void);
	void (*start_gpccs)(struct gk20a *g);
	void (*start_fecs)(struct gk20a *g);
	u32 (*get_gpccs_start_reg_offset)(void);
	int (*load_ctxsw_ucode)(struct gk20a *g,
				struct nvgpu_gr_falcon *falcon);
	u32 (*get_current_ctx)(struct gk20a *g);
	u32 (*get_ctx_ptr)(u32 ctx);
	u32 (*get_fecs_current_ctx_data)(struct gk20a *g,
					 struct nvgpu_mem *inst_block);
	int (*init_ctx_state)(struct gk20a *g,
			struct nvgpu_gr_falcon_query_sizes *sizes);
	void (*fecs_host_int_enable)(struct gk20a *g);
	u32 (*read_fecs_ctxsw_status0)(struct gk20a *g);
	u32 (*read_fecs_ctxsw_status1)(struct gk20a *g);
	void (*bind_instblk)(struct gk20a *g,
			     struct nvgpu_mem *mem, u64 inst_ptr);
#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
	void (*load_ctxsw_ucode_header)(struct gk20a *g,
				u32 reg_offset, u32 boot_signature,
				u32 addr_code32, u32 addr_data32,
				u32 code_size, u32 data_size);
	void (*load_ctxsw_ucode_boot)(struct gk20a *g,
				u32 reg_offset, u32 boot_entry,
				u32 addr_load32, u32 blocks, u32 dst);
	void (*load_gpccs_dmem)(struct gk20a *g,
				const u32 *ucode_u32_data, u32 size);
	void (*gpccs_dmemc_write)(struct gk20a *g, u32 port, u32 offs,
				u32 blk, u32 ainc);
	void (*load_fecs_dmem)(struct gk20a *g,
			       const u32 *ucode_u32_data, u32 size);
	void (*fecs_dmemc_write)(struct gk20a *g, u32 reg_offset, u32 port,
				u32 offs, u32 blk, u32 ainc);
	void (*load_gpccs_imem)(struct gk20a *g,
				const u32 *ucode_u32_data, u32 size);
	void (*gpccs_imemc_write)(struct gk20a *g, u32 port, u32 offs,
				u32 blk, u32 ainc);
	void (*load_fecs_imem)(struct gk20a *g,
			       const u32 *ucode_u32_data, u32 size);
	void (*fecs_imemc_write)(struct gk20a *g, u32 port, u32 offs,
				u32 blk, u32 ainc);
	void (*start_ucode)(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_SIM
	void (*configure_fmodel)(struct gk20a *g);
#endif
	/** @endcond */

};

/**
 * This structure stores the GR engine interrupt subunit hal pointers.
 *
 * @see gops_gr
 */
struct gops_gr_intr {
	/**
	 * @brief ISR for GR engine non stalling interrupts.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This is the entry point to handle GR engine
	 * non stalling interrupts.
	 * - Check for trap pending interrupts.
	 * - Clear trap pending interrupts.
	 * - Set the semaphore wakeup and post events bits
	 *   if there is pending interrupt.
	 *
	 * @return 0 in case of no trap pending interrupts, > 1 in
	 * case of pending interrupts.
	 */
	u32 (*nonstall_isr)(struct gk20a *g);

	/**
	 * @brief ISR for GR engine stalling interrupts.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This is the entry point to handle all GR engine
	 * stalling interrupts. This HAL maps to
	 * #nvgpu_gr_intr_stall_isr.
	 *
	 * This function will check for any pending exceptions/errors,
	 * and call appropriate function to handle it.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 *
	 * @see nvgpu_gr_intr_stall_isr
	 */
	int (*stall_isr)(struct gk20a *g);

	/**
	 * @brief Flush channel lookup TLB.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * GR interrupt unit maintains a TLB to translate context
	 * into GPU channel ID. The HAL maps to
	 * #nvgpu_gr_intr_flush_channel_tlb.
	 *
	 * @see nvgpu_gr_intr_flush_channel_tlb
	 */
	void (*flush_channel_tlb)(struct gk20a *g);

	/**
	 * @brief Record error state registers for given SM.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param gpc [in]		Index of GPC that includes SM.
	 * @param tpc [in]		Index of TPC that includes SM.
	 * @param sm [in]		Index of SM within TPC for which
	 * 				error state should be recorded.
	 * @param fault_ch [in]		Pointer to faulting channel.
	 *
	 * SM error state needs to be recorded from Error status registers
	 * upon any SM exception. This error state can be later queried by
	 * userspace clients to decode the reason of SM exception.
	 *
	 * This function records the Error status registers for requested
	 * \a sm and stores them into \a tsg->sm_error_states array.
	 * TSG pointer is extracted from \a fault_ch channel pointer.
	 *
	 * @return Logical global SM index of requested \a sm.
	 */
	u32 (*record_sm_error_state)(struct gk20a *g, u32 gpc,
				     u32 tpc, u32 sm,
				     struct nvgpu_channel *fault_ch);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	int (*retrigger)(struct gk20a *g);
	u32 (*enable_mask)(struct gk20a *g);
	int (*handle_fecs_error)(struct gk20a *g,
				 struct nvgpu_channel *ch,
				 struct nvgpu_gr_isr_data *isr_data);
	int (*handle_sw_method)(struct gk20a *g, u32 addr,
				u32 class_num, u32 offset, u32 data);
	void (*set_shader_exceptions)(struct gk20a *g,
				      u32 data);
	void (*handle_class_error)(struct gk20a *g, u32 chid,
				   struct nvgpu_gr_isr_data *isr_data);
	void (*clear_pending_interrupts)(struct gk20a *g,
					 u32 gr_intr);
	u32 (*read_pending_interrupts)(struct gk20a *g,
				struct nvgpu_gr_intr_info *intr_info);
	bool (*handle_exceptions)(struct gk20a *g,
				  bool *is_gpc_exception);
	u32 (*read_gpc_tpc_exception)(u32 gpc_exception);
	u32 (*read_gpc_exception)(struct gk20a *g, u32 gpc);
	u32 (*read_exception1)(struct gk20a *g);
	void (*trapped_method_info)(struct gk20a *g,
				struct nvgpu_gr_isr_data *isr_data);
	void (*handle_semaphore_pending)(struct gk20a *g,
				struct nvgpu_gr_isr_data *isr_data);
	void (*handle_notify_pending)(struct gk20a *g,
				struct nvgpu_gr_isr_data *isr_data);
	void (*handle_gpc_zrop_hww)(struct gk20a *g, u32 gpc,
			u32 gpc_exception);
	void (*handle_gpc_crop_hww)(struct gk20a *g, u32 gpc,
			u32 gpc_exception);
	void (*handle_gpc_rrh_hww)(struct gk20a *g, u32 gpc,
			u32 gpc_exception);
	void (*handle_gcc_exception)(struct gk20a *g, u32 gpc,
			u32 gpc_exception,
			u32 *corrected_err, u32 *uncorrected_err);
	void (*handle_gpc_gpcmmu_exception)(struct gk20a *g,
			u32 gpc, u32 gpc_exception,
			u32 *corrected_err, u32 *uncorrected_err);
	void (*handle_gpc_prop_exception)(struct gk20a *g,
					  u32 gpc, u32 gpc_exception);
	void (*handle_gpc_zcull_exception)(struct gk20a *g,
					   u32 gpc, u32 gpc_exception);
	void (*handle_gpc_setup_exception)(struct gk20a *g,
					   u32 gpc, u32 gpc_exception);
	void (*handle_gpc_pes_exception)(struct gk20a *g,
					 u32 gpc, u32 gpc_exception);
	void (*handle_gpc_gpccs_exception)(struct gk20a *g,
			u32 gpc, u32 gpc_exception,
			u32 *corrected_err, u32 *uncorrected_err);
	u32 (*get_tpc_exception)(struct gk20a *g, u32 offset,
			struct nvgpu_gr_tpc_exception *pending_tpc);
	void (*handle_tpc_mpc_exception)(struct gk20a *g,
					 u32 gpc, u32 tpc);
	void (*handle_tpc_pe_exception)(struct gk20a *g,
					u32 gpc, u32 tpc);
	void (*enable_hww_exceptions)(struct gk20a *g);
	void (*enable_interrupts)(struct gk20a *g, bool enable);
	void (*enable_exceptions)(struct gk20a *g,
				  struct nvgpu_gr_config *gr_config,
				  bool enable);
	void (*enable_gpc_crop_hww)(struct gk20a *g);
	void (*enable_gpc_zrop_hww)(struct gk20a *g);
	void (*enable_gpc_exceptions)(struct gk20a *g,
				struct nvgpu_gr_config *gr_config);
	void (*tpc_exception_sm_enable)(struct gk20a *g);
	int (*handle_sm_exception)(struct gk20a *g,
				   u32 gpc, u32 tpc, u32 sm,
				   bool *post_event,
				   struct nvgpu_channel *fault_ch,
				   u32 *hww_global_esr);
	void (*set_hww_esr_report_mask)(struct gk20a *g);
	void (*handle_tpc_sm_ecc_exception)(struct gk20a *g,
					    u32 gpc, u32 tpc);
	void (*get_esr_sm_sel)(struct gk20a *g, u32 gpc, u32 tpc,
			       u32 *esr_sm_sel);
	void (*clear_sm_hww)(struct gk20a *g, u32 gpc, u32 tpc,
			     u32 sm, u32 global_esr);
	void (*handle_ssync_hww)(struct gk20a *g, u32 *ssync_esr);
	u32 (*get_sm_hww_warp_esr)(struct gk20a *g,
				   u32 gpc, u32 tpc, u32 sm);
	u32 (*get_sm_hww_global_esr)(struct gk20a *g,
				     u32 gpc, u32 tpc, u32 sm);
	u64 (*get_sm_hww_warp_esr_pc)(struct gk20a *g, u32 offset);
	u32 (*get_sm_no_lock_down_hww_global_esr_mask)(
						struct gk20a *g);
	u32 (*get_ctxsw_checksum_mismatch_mailbox_val)(void);
	bool (*sm_ecc_status_errors)(struct gk20a *g, u32 ecc_status_reg,
				enum nvgpu_gr_sm_ecc_error_types err_type,
				struct nvgpu_gr_sm_ecc_status *ecc_status);
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	void (*handle_tex_exception)(struct gk20a *g,
				     u32 gpc, u32 tpc);
#endif
#ifdef CONFIG_NVGPU_DGPU
	void (*log_mme_exception)(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	void (*tpc_exception_sm_disable)(struct gk20a *g,
					 u32 offset);
	u64 (*tpc_enabled_exceptions)(struct gk20a *g);
#endif
	/** @endcond */
};

/**
 * This structure stores GR engine init subunit hal function pointers.
 *
 * @see gops_gr
 */
struct gops_gr_init {
	/**
	 * @brief Get number of SMs.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * This function returns number of SMs in GR engine.
	 * This HAL maps to #nvgpu_gr_get_no_of_sm.
	 *
	 * @return number of SMs.
	 *
	 * @see nvgpu_gr_get_no_of_sm
	 */
	u32 (*get_no_of_sm)(struct gk20a *g);

	/**
	 * @brief Get the count of tpc not attached PES unit.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param gpc [in]		Index of GPC.
	 * @param tpc [in]		Index of TPC.
	 * @param gr_config [in]	Pointer to GR
	 *				configuration struct.
	 *
	 * Calling this function returns the tpc that is not attached
	 * to PES unit.
	 *
	 * @return the tpc count not attached to PES unit.
	 */
	u32 (*get_nonpes_aware_tpc)(struct gk20a *g,
				    u32 gpc, u32 tpc,
				    struct nvgpu_gr_config *gr_config);

	/**
	 * @brief Control access to GR FIFO.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 * @param enable [in]	Boolean flag.
	 *
	 * This function sets/clear the register access to the
	 * graphics method FIFO. ACCESS bit determines whether
	 * Front Engine fetch methods out of the GR FIFO and
	 * SEMAPHORE_ACCESS bit determins whether the Front Engine
	 * makes semaphore memory requests.
	 */
	void (*fifo_access)(struct gk20a *g, bool enable);

	/**
	 * @brief Get maximum count of subcontexts.
	 *
	 * This function returns the maximum number of subcontexts
	 * in GR engine.
	 *
	 * @return maximum number of subcontexts.
	 */
	u32 (*get_max_subctx_count)(void);

	/**
	 * @brief Detect SM properties.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * This functions reports the SM hardware properties.
	 * Reports total number of warps and SM version.
	 */
	void (*detect_sm_arch)(struct gk20a *g);

#ifndef CONFIG_NVGPU_NON_FUSA
	/**
	 * @brief Set compute specific register values.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 * @param gr_ctx [in]	Pointer to GR engine context image.
	 *
	 * This function programs compute functionality specific register
	 * values using patch context in safety build. Bit
	 * gr_sked_hww_esr_en_skedcheck18_l1_config_too_small_disabled_f() is
	 * set in register gr_sked_hww_esr_en_r() and bit
	 * gr_gpcs_tpcs_sm_l1tag_ctrl_always_cut_collector_enable_f() is set
	 * in register gr_gpcs_tpcs_sm_l1tag_ctrl_r().
	 */
	void (*set_default_compute_regs)(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx);
#endif

	/**
	 * @brief Capture graphics specific register values.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 * @param gfx_regs [in] Pointer to struct holding gfx specific register init values.
	 *
	 * This function captures values of some registers that need to be
	 * configured differently only for graphics context.
	 */
	void (*capture_gfx_regs)(struct gk20a *g, struct nvgpu_gr_obj_ctx_gfx_regs *gfx_regs);

	/**
	 * @brief Set graphics specific register values.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 * @param gr_ctx [in]	Pointer to GR engine context image.
	 * @param gfx_regs [in] Pointer to struct holding gfx specific register init values.
	 *
	 * This function sets graphics specific register values in the
	 * patch context so that register values are set only for graphics
	 * contexts.
	 */
	void (*set_default_gfx_regs)(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
			struct nvgpu_gr_obj_ctx_gfx_regs *gfx_regs);

	/**
	 * @brief Get supported preemption mode flags.
	 *
	 * @param graphics_preemption_mode_flags [out]
	 *				Pointer to unsigned int.
	 * @param graphics_preemption_mode_flags [out]
	 *				Pointer to unsigned int.
	 * This function returns the supported preemption
	 * graphics and compute mode flags.
	 */
	void (*get_supported__preemption_modes)(
				u32 *graphics_preemption_mode_flags,
				u32 *compute_preepmtion_mode_flags);

	/**
	 * @brief Get default preemption modes.
	 *
	 * @param default_graphics_preempt_mode [out]
	 *				Pointer to a variable.
	 * @param default_compute_preempt_mode [out]
	 *				Pointer to a variable.
	 * This function returns the default preemption
	 * graphics and compute modes set.
	 */
	void (*get_default_preemption_modes)(
				u32 *default_graphics_preempt_mode,
				u32 *default_compute_preempt_mode);

	/**
	 * @brief Wait for graphics engine to idle with timeout of
	 *        \a NVGPU_DEFAULT_POLL_TIMEOUT_MS.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * During graphics engine programming it is necessary to ensure
	 * engine is idle at various steps.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -EAGAIN if graphics engine is busy and cannot idle.
	 */
	int (*wait_idle)(struct gk20a *g);

	/**
	 * @brief Wait for FE method pipeline to idle with timeout of
	 *        \a NVGPU_DEFAULT_POLL_TIMEOUT_MS.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * During graphics engine programming it is necessary to ensure
	 * FE method pipeline is idle at various steps.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -EAGAIN if FE method pipeline is busy and cannot idle.
	 */
	int (*wait_fe_idle)(struct gk20a *g);

	/**
	 * @brief Force FE power mode to always on.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param force_on [in]		Boolean flag to enable/disable power mode.
	 *
	 * Enable or disable force power on mode for graphics engine based
	 * on \a force_on parameter. Wait for upto 2000 uS to ensure power mode
	 * is correctly set.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ETIMEDOUT if power mode was not updated correctly within timeout.
	 */
	int (*fe_pwr_mode_force_on)(struct gk20a *g, bool force_on);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	int (*ecc_scrub_reg)(struct gk20a *g,
			      struct nvgpu_gr_config *gr_config);
	void (*lg_coalesce)(struct gk20a *g, u32 data);
	void (*su_coalesce)(struct gk20a *g, u32 data);
	void (*pes_vsc_stream)(struct gk20a *g);
	void (*gpc_mmu)(struct gk20a *g);
	u32 (*get_sm_id_size)(void);
	int (*sm_id_config)(struct gk20a *g, u32 *tpc_sm_id,
			    struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch);
	void (*sm_id_numbering)(struct gk20a *g, u32 gpc,
				u32 tpc, u32 smid,
				struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch);
	void (*tpc_mask)(struct gk20a *g,
			 u32 gpc_index, u32 pes_tpc_mask);
	void (*fs_state)(struct gk20a *g);
	void (*pd_tpc_per_gpc)(struct gk20a *g,
		struct nvgpu_gr_config *gr_config);
	void (*pd_skip_table_gpc)(struct gk20a *g,
				  struct nvgpu_gr_config *gr_config);
	void (*cwd_gpcs_tpcs_num)(struct gk20a *g,
				  u32 gpc_count, u32 tpc_count);
	void (*gr_load_tpc_mask)(struct gk20a *g,
			struct nvgpu_gr_config *gr_config);
	int (*wait_empty)(struct gk20a *g);
	void (*override_context_reset)(struct gk20a *g);
	void (*fe_go_idle_timeout)(struct gk20a *g, bool enable);
	void (*load_method_init)(struct gk20a *g,
			struct netlist_av_list *sw_method_init);
	int (*load_sw_bundle_init)(struct gk20a *g,
			struct netlist_av_list *sw_method_init);
	int (*load_sw_veid_bundle)(struct gk20a *g,
			struct netlist_av_list *sw_method_init);
	void (*commit_global_timeslice)(struct gk20a *g);
	u32 (*get_bundle_cb_default_size)(struct gk20a *g);
	u32 (*get_min_gpm_fifo_depth)(struct gk20a *g);
	u32 (*get_bundle_cb_token_limit)(struct gk20a *g);
	u32 (*get_attrib_cb_default_size)(struct gk20a *g);
	u32 (*get_alpha_cb_default_size)(struct gk20a *g);
	u32 (*get_attrib_cb_size)(struct gk20a *g, u32 tpc_count);
	u32 (*get_alpha_cb_size)(struct gk20a *g, u32 tpc_count);
	u32 (*get_global_attr_cb_size)(struct gk20a *g,
				       u32 tpc_count, u32 max_tpc);
	u32 (*get_global_ctx_cb_buffer_size)(struct gk20a *g);
	u32 (*get_global_ctx_pagepool_buffer_size)(struct gk20a *g);
	void (*commit_global_bundle_cb)(struct gk20a *g,
					struct nvgpu_gr_ctx *ch_ctx,
					u64 addr, u32 size,
					bool patch);
	u32 (*pagepool_default_size)(struct gk20a *g);
	void (*commit_global_pagepool)(struct gk20a *g,
				       struct nvgpu_gr_ctx *ch_ctx,
				       u64 addr, size_t size,
				       bool patch, bool global_ctx);
	void (*commit_global_attrib_cb)(struct gk20a *g,
					struct nvgpu_gr_ctx *ch_ctx,
					u32 tpc_count, u32 max_tpc,
					u64 addr, bool patch);
	void (*commit_global_cb_manager)(struct gk20a *g,
				struct nvgpu_gr_config *config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch);
	void (*pipe_mode_override)(struct gk20a *g, bool enable);
	void (*commit_ctxsw_spill)(struct gk20a *g,
				   struct nvgpu_gr_ctx *gr_ctx,
				   u64 addr, u32 size, bool patch);
	u32 (*get_patch_slots)(struct gk20a *g,
			       struct nvgpu_gr_config *config);
	int (*load_sw_bundle64)(struct gk20a *g,
			struct netlist_av64_list *sw_bundle64_init);
#ifdef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
	void (*restore_stats_counter_bundle_data)(struct gk20a *g,
			struct netlist_av_list *sw_bundle_init);
#endif
#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
	void (*get_access_map)(struct gk20a *g,
			       u32 **whitelist, u32 *num_entries);
#endif
#ifdef CONFIG_NVGPU_SM_DIVERSITY
	int (*commit_sm_id_programming)(struct gk20a *g,
				struct nvgpu_gr_config *config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch);
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
	void (*rop_mapping)(struct gk20a *g,
		struct nvgpu_gr_config *gr_config);
	u32 (*get_rtv_cb_size)(struct gk20a *g);
	void (*commit_rtv_cb)(struct gk20a *g, u64 addr,
			      struct nvgpu_gr_ctx *gr_ctx, bool patch);
	void (*commit_rops_crop_override)(struct gk20a *g,
			      struct nvgpu_gr_ctx *gr_ctx, bool patch);
#endif
#ifdef CONFIG_NVGPU_GFXP
	int (*preemption_state)(struct gk20a *g);
	u32 (*get_ctx_attrib_cb_size)(struct gk20a *g, u32 betacb_size,
				      u32 tpc_count, u32 max_tpc);
	void (*commit_cbes_reserve)(struct gk20a *g,
				    struct nvgpu_gr_ctx *gr_ctx,
				    bool patch);
	void (*commit_gfxp_rtv_cb)(struct gk20a *g,
				   struct nvgpu_gr_ctx *gr_ctx,
				   bool patch);
	u32 (*get_attrib_cb_gfxp_default_size)(struct gk20a *g);
	u32 (*get_attrib_cb_gfxp_size)(struct gk20a *g);
	u32 (*get_gfxp_rtv_cb_size)(struct gk20a *g);
	void (*gfxp_wfi_timeout)(struct gk20a *g,
				 struct nvgpu_gr_ctx *gr_ctx,
				 bool patch);
	u32 (*get_ctx_spill_size)(struct gk20a *g);
	u32 (*get_ctx_pagepool_size)(struct gk20a *g);
	u32 (*get_ctx_betacb_size)(struct gk20a *g);
#endif /* CONFIG_NVGPU_GFXP */
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	/**
	 * @brief Wait for GR engine to be initialized.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * Calling this function ensures that GR engine initialization
	 * is complete. This HAL maps to #nvgpu_gr_wait_initialized.
	 *
	 * @see nvgpu_gr_wait_initialized
	 */
	void (*wait_initialized)(struct gk20a *g);
#endif
	bool (*is_allowed_sw_bundle)(struct gk20a *g,
			u32 bundle_addr, u32 bundle_value, int *context);
	bool (*is_allowed_reg)(struct gk20a *g, u32 addr);
	void (*auto_go_idle)(struct gk20a *g, bool enable);
	void (*eng_config)(struct gk20a *g);
	int (*reset_gpcs)(struct gk20a *g);
	int (*sm_id_config_early)(struct gk20a *g,
			struct nvgpu_gr_config *config);
	void (*set_sm_l1tag_surface_collector)(struct gk20a *g);
	/** @endcond */
};

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
struct gops_gr_config {
	u32 (*get_gpc_tpc_mask)(struct gk20a *g,
				struct nvgpu_gr_config *config,
				u32 gpc_index);
	u32 (*get_gpc_pes_mask)(struct gk20a *g,
				struct nvgpu_gr_config *config,
				u32 gpc_index);
	u32 (*get_gpc_rop_mask)(struct gk20a *g,
				struct nvgpu_gr_config *config,
				u32 gpc_index);
	u32 (*get_gpc_mask)(struct gk20a *g);
	u32 (*get_tpc_count_in_gpc)(struct gk20a *g,
				    struct nvgpu_gr_config *config,
				    u32 gpc_index);
	u32 (*get_pes_tpc_mask)(struct gk20a *g,
				struct nvgpu_gr_config *config,
				u32 gpc_index, u32 pes_index);
	u32 (*get_pd_dist_skip_table_size)(void);
	int (*init_sm_id_table)(struct gk20a *g,
				struct nvgpu_gr_config *gr_config);
#ifdef CONFIG_NVGPU_GRAPHICS
	u32 (*get_zcull_count_in_gpc)(struct gk20a *g,
				      struct nvgpu_gr_config *config,
				      u32 gpc_index);
#endif
};
/** @endcond */

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
struct gops_gr_ctxsw_prog {
	u32 (*hw_get_fecs_header_size)(void);
	u32 (*get_patch_count)(struct gk20a *g,
			       struct nvgpu_mem *ctx_mem);
	void (*set_patch_count)(struct gk20a *g,
				struct nvgpu_mem *ctx_mem, u32 count);
	void (*set_patch_addr)(struct gk20a *g,
			       struct nvgpu_mem *ctx_mem, u64 addr);
	void (*set_compute_preemption_mode_cta)(struct gk20a *g,
				struct nvgpu_mem *ctx_mem);
	void (*set_context_buffer_ptr)(struct gk20a *g,
				       struct nvgpu_mem *ctx_mem,
				       u64 addr);
	void (*set_type_per_veid_header)(struct gk20a *g,
					 struct nvgpu_mem *ctx_mem);
#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
	void (*set_priv_access_map_config_mode)(struct gk20a *g,
			struct nvgpu_mem *ctx_mem, bool allow_all);
	void (*set_priv_access_map_addr)(struct gk20a *g,
					 struct nvgpu_mem *ctx_mem,
					 u64 addr);
#endif
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	void (*disable_verif_features)(struct gk20a *g,
				       struct nvgpu_mem *ctx_mem);
	void (*init_ctxsw_hdr_data)(struct gk20a *g,
				    struct nvgpu_mem *ctx_mem);
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
	void (*set_zcull_ptr)(struct gk20a *g,
			      struct nvgpu_mem *ctx_mem, u64 addr);
	void (*set_zcull)(struct gk20a *g,
			  struct nvgpu_mem *ctx_mem, u32 mode);
	void (*set_zcull_mode_no_ctxsw)(struct gk20a *g,
					struct nvgpu_mem *ctx_mem);
	bool (*is_zcull_mode_separate_buffer)(u32 mode);
#endif
#ifdef CONFIG_NVGPU_GFXP
	void (*set_full_preemption_ptr)(struct gk20a *g,
					struct nvgpu_mem *ctx_mem,
					u64 addr);
	void (*set_full_preemption_ptr_veid0)(struct gk20a *g,
				struct nvgpu_mem *ctx_mem, u64 addr);
	void (*set_graphics_preemption_mode_gfxp)(struct gk20a *g,
					struct nvgpu_mem *ctx_mem);
#endif
#ifdef CONFIG_NVGPU_CILP
	void (*set_compute_preemption_mode_cilp)(struct gk20a *g,
					struct nvgpu_mem *ctx_mem);
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	u32 (*hw_get_gpccs_header_size)(void);
	u32 (*hw_get_extended_buffer_segments_size_in_bytes)(void);
	u32 (*hw_extended_marker_size_in_bytes)(void);
	u32 (*hw_get_perf_counter_control_register_stride)(void);
	u32 (*hw_get_perf_counter_register_stride)(void);
	u32 (*get_main_image_ctx_id)(struct gk20a *g,
				     struct nvgpu_mem *ctx_mem);
	void (*set_pm_ptr)(struct gk20a *g,
			   struct nvgpu_mem *ctx_mem, u64 addr);
	void (*set_pm_mode)(struct gk20a *g,
			    struct nvgpu_mem *ctx_mem, u32 mode);
	void (*set_pm_smpc_mode)(struct gk20a *g,
				 struct nvgpu_mem *ctx_mem,
				 bool enable);
	u32 (*hw_get_pm_mode_no_ctxsw)(void);
	u32 (*hw_get_pm_mode_ctxsw)(void);
	u32 (*hw_get_pm_mode_stream_out_ctxsw)(void);
	void (*set_cde_enabled)(struct gk20a *g,
				struct nvgpu_mem *ctx_mem);
	void (*set_pc_sampling)(struct gk20a *g,
				struct nvgpu_mem *ctx_mem,
				bool enable);
	bool (*check_main_image_header_magic)(u32 *context);
	bool (*check_local_header_magic)(u32 *context);
	u32 (*get_num_gpcs)(u32 *context);
	u32 (*get_num_tpcs)(u32 *context);
	void (*get_extended_buffer_size_offset)(u32 *context,
						u32 *size,
						u32 *offset);
	void (*get_ppc_info)(u32 *context, u32 *num_ppcs,
			     u32 *ppc_mask);
	u32 (*get_local_priv_register_ctl_offset)(u32 *context);
	void (*set_pmu_options_boost_clock_frequencies)(
					struct gk20a *g,
					struct nvgpu_mem *ctx_mem,
					u32 boosted_ctx);
	u32 (*hw_get_pm_gpc_gnic_stride)(struct gk20a *g);
#endif /* CONFIG_NVGPU_DEBUGGER */
#ifdef CONFIG_DEBUG_FS
	void (*dump_ctxsw_stats)(struct gk20a *g,
				 struct nvgpu_mem *ctx_mem);
#endif
#ifdef CONFIG_NVGPU_FECS_TRACE
	u32 (*hw_get_ts_tag_invalid_timestamp)(void);
	u32 (*hw_get_ts_tag)(u64 ts);
	u64 (*hw_record_ts_timestamp)(u64 ts);
	u32 (*hw_get_ts_record_size_in_bytes)(void);
	bool (*is_ts_valid_record)(u32 magic_hi);
	u32 (*get_ts_buffer_aperture_mask)(struct gk20a *g,
					   struct nvgpu_mem *ctx_mem);
	void (*set_ts_num_records)(struct gk20a *g,
				   struct nvgpu_mem *ctx_mem, u32 num);
	void (*set_ts_buffer_ptr)(struct gk20a *g,
				  struct nvgpu_mem *ctx_mem, u64 addr,
				  u32 aperture_mask);
#endif
#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
#ifdef CONFIG_NVGPU_DEBUGGER
	u32 (*hw_get_main_header_size)(void);
	u32 (*hw_get_gpccs_header_stride)(void);
	u32 (*get_compute_sysreglist_offset)(u32 *fecs_hdr);
	u32 (*get_gfx_sysreglist_offset)(u32 *fecs_hdr);
	u32 (*get_ltsreglist_offset)(u32 *fecs_hdr);
	u32 (*get_compute_gpcreglist_offset)(u32 *gpccs_hdr);
	u32 (*get_gfx_gpcreglist_offset)(u32 *gpccs_hdr);
	u32 (*get_compute_tpcreglist_offset)(u32 *gpccs_hdr, u32 tpc_num);
	u32 (*get_gfx_tpcreglist_offset)(u32 *gpccs_hdr, u32 tpc_num);
	u32 (*get_compute_ppcreglist_offset)(u32 *gpccs_hdr);
	u32 (*get_gfx_ppcreglist_offset)(u32 *gpccs_hdr);
	u32 (*get_compute_etpcreglist_offset)(u32 *gpccs_hdr);
	u32 (*get_gfx_etpcreglist_offset)(u32 *gpccs_hdr);
	u32 (*get_tpc_segment_pri_layout)(struct gk20a *g, u32 *main_hdr);
#endif
#endif
};
/** @endcond */

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_FECS_TRACE
struct gops_gr_fecs_trace {
	int (*init)(struct gk20a *g);
	int (*max_entries)(struct gk20a *,
			struct nvgpu_gpu_ctxsw_trace_filter *filter);
	int (*flush)(struct gk20a *g);
	int (*poll)(struct gk20a *g);
	int (*enable)(struct gk20a *g);
	int (*disable)(struct gk20a *g);
	bool (*is_enabled)(struct gk20a *g);
	int (*reset)(struct gk20a *g);
	int (*bind_channel)(struct gk20a *g,
			    struct nvgpu_mem *inst_block,
			    struct nvgpu_gr_subctx *subctx,
			    struct nvgpu_gr_ctx *gr_ctx,
			    pid_t pid, u32 vmid);
	int (*unbind_channel)(struct gk20a *g,
			      struct nvgpu_mem *inst_block);
	int (*deinit)(struct gk20a *g);
	int (*alloc_user_buffer)(struct gk20a *g,
				 void **buf, size_t *size);
	int (*free_user_buffer)(struct gk20a *g);
	void (*get_mmap_user_buffer_info)(struct gk20a *g,
					  void **addr, size_t *size);
	int (*set_filter)(struct gk20a *g,
			  struct nvgpu_gpu_ctxsw_trace_filter *filter);
	u32 (*get_buffer_full_mailbox_val)(void);
	int (*get_read_index)(struct gk20a *g);
	int (*get_write_index)(struct gk20a *g);
	int (*set_read_index)(struct gk20a *g, int index);
	void (*vm_dev_write)(struct gk20a *g, u8 vmid,
			u32 *vm_update_mask,
			struct nvgpu_gpu_ctxsw_trace_entry *entry);
	void (*vm_dev_update)(struct gk20a *g, u32 vm_update_mask);
};
#endif
/** @endcond */

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_DEBUGGER
	struct gops_gr_hwpm_map {
		void (*align_regs_perf_pma)(u32 *offset);
		u32 (*get_active_fbpa_mask)(struct gk20a *g);
	};
#endif
/** @endcond */

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_GRAPHICS
struct gops_gr_zbc {
	void (*add_color)(struct gk20a *g,
		struct nvgpu_gr_zbc_entry *color_val,
		u32 index);
	void (*add_depth)(struct gk20a *g,
		struct nvgpu_gr_zbc_entry *depth_val,
		u32 index);
	int (*set_table)(struct gk20a *g,
		struct nvgpu_gr_zbc *zbc,
		struct nvgpu_gr_zbc_entry *zbc_val);
	int (*query_table)(struct gk20a *g,
		struct nvgpu_gr_zbc *zbc,
		struct nvgpu_gr_zbc_query_params *query_params);
	void (*add_stencil)(struct gk20a *g,
		struct nvgpu_gr_zbc_entry *s_val, u32 index);
	u32 (*get_gpcs_swdx_dss_zbc_c_format_reg)(struct gk20a *g);
	u32 (*get_gpcs_swdx_dss_zbc_z_format_reg)(struct gk20a *g);
	void (*init_table_indices)(struct gk20a *g,
			struct nvgpu_gr_zbc_table_indices *zbc_indices);
};
struct gops_gr_zcull {
	int (*init_zcull_hw)(struct gk20a *g,
			     struct nvgpu_gr_zcull *gr_zcull,
			     struct nvgpu_gr_config *gr_config);
	int (*get_zcull_info)(struct gk20a *g,
			struct nvgpu_gr_config *gr_config,
			struct nvgpu_gr_zcull *gr_zcull,
			struct nvgpu_gr_zcull_info *zcull_params);
	void (*program_zcull_mapping)(struct gk20a *g,
				      u32 zcull_alloc_num,
				      u32 *zcull_map_tiles);
};
#endif /* CONFIG_NVGPU_GRAPHICS */
/** @endcond */

/**
 * This structure stores the GR engine HAL function pointers.
 *
 * @see gpu_ops
 */
struct gops_gr {
	/**
	 * @brief Initialize all the GR engine support and functionality.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * Initialize CTXSW falcon, GR engine HW, GR engine interrupts, and
	 * necessary SW context support.
	 * This HAL always maps to #nvgpu_gr_init_support.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ENOENT if context switch ucode is not found.
	 * @retval -ETIMEDOUT if context switch ucode times out.
	 * @retval -ETIMEDOUT if reading golden context size times out.
	 * @retval -ENOMEM if memory allocation fails for any internal data
	 *         structure.
	 *
	 * @see nvgpu_gr_init_support
	 */
	int (*gr_init_support)(struct gk20a *g);

	/**
	 * @brief Suspend GR engine.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This HAL is typically called while preparing for GPU power off.
	 * This HAL always maps to #nvgpu_gr_suspend.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -EAGAIN if GR engine idle wait times out.
	 *
	 * @see nvgpu_gr_suspend
	 */
	int (*gr_suspend)(struct gk20a *g);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_DEBUGGER
	u32 (*get_gr_status)(struct gk20a *g);
	void (*set_alpha_circular_buffer_size)(struct gk20a *g,
					       u32 data);
	void (*set_circular_buffer_size)(struct gk20a *g, u32 data);
	void (*set_bes_crop_debug3)(struct gk20a *g, u32 data);
	void (*set_bes_crop_debug4)(struct gk20a *g, u32 data);
	void (*get_sm_dsm_perf_regs)(struct gk20a *g,
				     u32 *num_sm_dsm_perf_regs,
				     u32 **sm_dsm_perf_regs,
				     u32 *perf_register_stride);
	void (*get_sm_dsm_perf_ctrl_regs)(struct gk20a *g,
					  u32 *num_sm_dsm_perf_regs,
					  u32 **sm_dsm_perf_regs,
					  u32 *perf_register_stride);
	void (*get_ovr_perf_regs)(struct gk20a *g,
				  u32 *num_ovr_perf_regs,
				  u32 **ovr_perf_regsr);
#ifdef CONFIG_NVGPU_TEGRA_FUSE
	void (*set_gpc_tpc_mask)(struct gk20a *g, u32 gpc_index);
#endif
	int (*decode_egpc_addr)(struct gk20a *g,
				u32 addr, enum ctxsw_addr_type *addr_type,
				u32 *gpc_num, u32 *tpc_num,
				u32 *broadcast_flags);
	void (*egpc_etpc_priv_addr_table)(struct gk20a *g, u32 addr,
					  u32 gpc, u32 tpc,
					  u32 broadcast_flags,
					  u32 *priv_addr_table,
					  u32 *priv_addr_table_index);
	bool (*is_egpc_addr)(struct gk20a *g, u32 addr);
	bool (*is_etpc_addr)(struct gk20a *g, u32 addr);
	void (*get_egpc_etpc_num)(struct gk20a *g, u32 addr,
				  u32 *gpc_num, u32 *tpc_num);
	u32 (*get_egpc_base)(struct gk20a *g);
	int (*update_smpc_ctxsw_mode)(struct gk20a *g,
				      struct nvgpu_tsg *tsg,
				      bool enable);
	int (*update_smpc_global_mode)(struct gk20a *g,
				      bool enable);
	int (*update_hwpm_ctxsw_mode)(struct gk20a *g,
				      u32 gr_instance_id,
				      struct nvgpu_tsg *tsg,
				      u32 mode);
	int (*ctx_patch_smpc)(struct gk20a *g,
			    u32 addr, u32 data,
			    struct nvgpu_gr_ctx *gr_ctx);
	int (*dump_gr_regs)(struct gk20a *g,
			    struct nvgpu_debug_context *o);
	int (*update_pc_sampling)(struct nvgpu_channel *ch,
				  bool enable);
	void (*init_sm_dsm_reg_info)(void);
	void (*init_ovr_sm_dsm_perf)(void);
	void (*init_cyclestats)(struct gk20a *g);
	int (*set_sm_debug_mode)(struct gk20a *g, struct nvgpu_channel *ch,
				 u64 sms, bool enable);
	void (*bpt_reg_info)(struct gk20a *g,
			     struct nvgpu_warpstate *w_state);
	int (*pre_process_sm_exception)(struct gk20a *g,
					u32 gpc, u32 tpc, u32 sm,
					u32 global_esr, u32 warp_esr,
					bool sm_debugger_attached,
					struct nvgpu_channel *fault_ch,
					bool *early_exit,
					bool *ignore_debugger);
	int  (*lock_down_sm)(struct gk20a *g, u32 gpc, u32 tpc, u32 sm,
			     u32 global_esr_mask, bool check_errors);
	int  (*wait_for_sm_lock_down)(struct gk20a *g, u32 gpc, u32 tpc,
				      u32 sm, u32 global_esr_mask,
				      bool check_errors);
	int (*clear_sm_error_state)(struct gk20a *g,
				    struct nvgpu_channel *ch, u32 sm_id);
	int (*suspend_contexts)(struct gk20a *g,
				struct dbg_session_gk20a *dbg_s,
				int *ctx_resident_ch_fd);
	int (*resume_contexts)(struct gk20a *g,
			       struct dbg_session_gk20a *dbg_s,
			       int *ctx_resident_ch_fd);
	int (*set_ctxsw_preemption_mode)(struct gk20a *g,
					 struct nvgpu_gr_ctx *gr_ctx,
					 struct vm_gk20a *vm, u32 class,
					 u32 graphics_preempt_mode,
					 u32 compute_preempt_mode);
	int (*trigger_suspend)(struct gk20a *g);
	int (*wait_for_pause)(struct gk20a *g,
			      struct nvgpu_warpstate *w_state);
	int (*resume_from_pause)(struct gk20a *g);
	int (*clear_sm_errors)(struct gk20a *g);
	bool (*is_tsg_ctx_resident)(struct nvgpu_tsg *tsg);
	bool (*sm_debugger_attached)(struct gk20a *g);
	void (*suspend_single_sm)(struct gk20a *g,
				  u32 gpc, u32 tpc, u32 sm,
				  u32 global_esr_mask, bool check_errors);
	void (*suspend_all_sms)(struct gk20a *g,
				u32 global_esr_mask, bool check_errors);
	void (*resume_single_sm)(struct gk20a *g,
				 u32 gpc, u32 tpc, u32 sm);
	void (*resume_all_sms)(struct gk20a *g);
	int (*add_ctxsw_reg_pm_fbpa)(struct gk20a *g,
				     struct ctxsw_buf_offset_map_entry *map,
				     struct netlist_aiv_list *regs,
				     u32 *count, u32 *offset,
				     u32 max_cnt, u32 base,
				     u32 num_fbpas, u32 stride, u32 mask);
	int (*decode_priv_addr)(struct gk20a *g, u32 addr,
				enum ctxsw_addr_type *addr_type,
				u32 *gpc_num, u32 *tpc_num,
				u32 *ppc_num, u32 *be_num,
				u32 *broadcast_flags);
	int (*create_priv_addr_table)(struct gk20a *g,
					   u32 addr,
					   u32 *priv_addr_table,
					   u32 *num_registers);
	void (*split_fbpa_broadcast_addr)(struct gk20a *g, u32 addr,
					  u32 num_fbpas,
					  u32 *priv_addr_table,
					  u32 *priv_addr_table_index);
	int (*get_offset_in_gpccs_segment)(struct gk20a *g,
					   enum ctxsw_addr_type addr_type,
					   u32 num_tpcs, u32 num_ppcs,
					   u32 reg_list_ppc_count,
					   u32 *__offset_in_segment);
	int (*get_ctx_buffer_offsets)(struct gk20a *g,
					u32 addr,
					u32 max_offsets,
					u32 *offsets, u32 *offset_addrs,
					u32 *num_offsets);
	int (*get_pm_ctx_buffer_offsets)(struct gk20a *g,
				       u32 addr,
				       u32 max_offsets,
				       u32 *offsets, u32 *offset_addrs,
				       u32 *num_offsets);
	int (*find_priv_offset_in_buffer)(struct gk20a *g, u32 addr,
					u32 *context_buffer,
					u32 context_buffer_size,
					u32 *priv_offset);
	int (*process_context_buffer_priv_segment)(struct gk20a *g,
					     enum ctxsw_addr_type addr_type,
					     u32 pri_addr,
					     u32 gpc_num, u32 num_tpcs,
					     u32 num_ppcs, u32 ppc_mask,
					     u32 *priv_offset);
	void (*set_debug_mode)(struct gk20a *g, bool enable);
	int (*set_mmu_debug_mode)(struct gk20a *g,
				  struct nvgpu_channel *ch, bool enable);
	bool (*esr_bpt_pending_events)(u32 global_esr,
				       enum nvgpu_event_id_type bpt_event);
	bool (*check_warp_esr_error)(struct gk20a *g, u32 warp_esr_error);
#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
	int (*set_boosted_ctx)(struct nvgpu_channel *ch, bool boost);
#endif
#endif
	void (*disable_cau)(struct gk20a *g);
	void (*disable_smpc)(struct gk20a *g);
	const u32 *(*get_hwpm_cau_init_data)(u32 *count);
	void (*init_cau)(struct gk20a *g);
	/** @endcond */

	/** This structure stores the GR ecc subunit hal pointers. */
	struct gops_gr_ecc	ecc;
	/** This structure stores the GR setup subunit hal pointers. */
	struct gops_gr_setup	setup;
	/** This structure stores the GR falcon subunit hal pointers. */
	struct gops_gr_falcon	falcon;
	/** This structure stores the GR interrupt subunit hal pointers. */
	struct gops_gr_intr	intr;
	/** This structure stores the GR init subunit hal pointers. */
	struct gops_gr_init	init;

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	struct gops_gr_config		config;
	struct gops_gr_ctxsw_prog	ctxsw_prog;
#ifdef CONFIG_NVGPU_FECS_TRACE
	struct gops_gr_fecs_trace	fecs_trace;
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	struct gops_gr_hwpm_map		hwpm_map;
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
	struct gops_gr_zbc		zbc;
	struct gops_gr_zcull		zcull;
#endif /* CONFIG_NVGPU_GRAPHICS */
#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
	void (*vab_reserve)(struct gk20a *g, u32 vab_reg, u32 num_range_checkers,
	struct nvgpu_vab_range_checker *vab_range_checker);
	void (*vab_configure)(struct gk20a *g, u32 vab_reg);
#endif
	/** @endcond */
};

#endif /* NVGPU_GOPS_GR_H */

