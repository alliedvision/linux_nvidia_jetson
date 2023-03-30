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
#ifndef NVGPU_GOPS_LTC_H
#define NVGPU_GOPS_LTC_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * common.ltc interface.
 */
struct gk20a;

/**
 * common.ltc intr subunit hal operations.
 *
 * This structure stores common.ltc interrupt subunit hal pointers.
 *
 * @see gpu_ops
 */
struct gops_ltc_intr {
	/**
	 * @brief ISR for handling ltc interrupts.
	 *
	 * @param g [in]		- The GPU driver struct.
	 *                                - The function does not perform
	 *                                  validation of g parameter.
	 * @param ltc [in]		- Index of LTC.
	 *				  - The function validates that
	 *				    ltc < g->ltc->ltc_count.
	 *
	 * - For each ltc slice \a slice from 0 to g->ltc->slices_per_ltc - 1:
	 *   -# The L2 has SEC-DED protection on its data RAM and parity protection on the
	 *      byte enables RAM.
	 *   -# See <a href="https:/p4viewer.nvidia.com/get//hw/doc/gpu/ampere/ampere/design/Functional_Descriptions/Resiliency/Ampere_gpu_resiliency_ECC.docx</a> for details.
	 *   -# Following PRI registers are used for controlling parity ECC and
	 *      getting the status and information of ECC.
	 *      -# Control:
	 *         -# ECC_CONTROL
	 *      -# Error status and information:
	 *         -# ECC_STATUS
	 *         -# ECC_ADDRESS
	 *         -# ECC_CORRECTED_ERR_COUNT
	 *         -# ECC_UNCORRECTED_ERR_COUNT
	 *   -# Detect and handle ECC PARITY errors and SEC-DED errors.
	 *      SEC errors are reported as DSTG corrected errors and
	 *      DED errors are reported as DSTG uncorrected errors.
	 *      Below are the supported errors:
	 *      -# UNCORRECTED_ERR_RSTG - signals a parity error in RSTG RAMS, for now only CBC RAMS
	 *      -# UNCORRECTED_ERR_TSTG - signals a parity error in TSTG RAMS
	 *      -# UNCORRECTED_ERR_DSTG - signals a parity error in DSTG RAMS, non-data RAMS
	 *                                and DED in data RAMS.
	 *      -# CORRECTED_ERR_DSTG - signals an ecc corrected error in DSTG data RAMS (SEC)
	 *   -# Read ltc_ltc0_lts0_intr3_r() register corresponding to the slice adding the offset:
	 *      \f$(ltc * GPU\_LIT\_LTC\_STRIDE) + (slice * GPU\_LIT\_LTS\_STRIDE)\f$
	 *   -# Check if ltc_ltcs_ltss_intr3_ecc_uncorrected_m() or
	 *      ltc_ltcs_ltss_intr3_ecc_corrected_m() is set in
	 *      ltc_ltc0_lts0_intr3_r() register read above.
	 *      If so, handle as below:
	 *      -# Read following registers for the slice:
	 *         -# ecc status register: ltc_ltc0_lts0_l2_cache_ecc_status_r()
	 *         -# ecc address register: ltc_ltc0_lts0_l2_cache_ecc_address_r()
	 *         -# ecc uncorrected count register:
	 *            ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_r()
	 *         -# ecc corrected count register:
	 *            ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_r()
	 *      -# Calculate counter delta by applying
	 *         ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_total_v()
	 *         to uncorrected count register read above.
	 *      -# Check if the uncorrected count overflow happened by AND'ing ecc status
	 *         read above with ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_total_counter_overflow_m().
	 *      -# Reset the counter ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_r()
	 *         to zero if the counter delta is non-zero or if there is overflow.
	 *      -# Calculate counter delta by applying
	 *         ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_total_v()
	 *         to corrected count register read above.
	 *      -# Check if the corrected count overflow happened by AND'ing ecc status
	 *         read above with ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_total_counter_overflow_m().
	 *      -# Reset the counter ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_r() to zero if
	 *         the counter delta is non-zero or if there is overflow.
	 *      -# Reset the counter ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_r() to zero if
	 *         the counter delta is non-zero or if there is overflow.
	 *      -# Write ltc_ltc0_lts0_l2_cache_ecc_status_reset_task_f() to
	 *         ltc_ltc0_lts0_l2_cache_ecc_status_r() to reset the entire register.
	 *      -# Add to the uncorrected counter delta
	 *         BIT32(ltc_ltc0_lts0_l2_cache_ecc_ununcorrected_err_count_total_s())
	 *         if there is overflow.
	 *      -# Add to the corrected counter delta
	 *         BIT32(ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_total_s())
	 *         if there is overflow.
	 *      -# Handle ecc errors for subunits (part of the L2 slice detected an error).
	 *         There are three subunits. Pass below parameters to these units:
	 *         -# \a g
	 *         -# \a ltc
	 *         -# \a slice
	 *         -# ecc status read
	 *         -# ecc address read
	 *         -# uncorrected delta
	 *         -# corrected delta (This is passed to only DSTG ECC handling function)
	 *
	 *         ECC error handling for subunits is given below:
	 *         -# r-stg : the input command queues and the compression bit cache
	 *            -# If ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_rstg_m() is
	 *               set in ecc status:
	 *               -# Increment g->ecc.ltc.rstg_ecc_parity_count[\a ltc][\a slice].counter
	 *                  with uncorrected counter delta with
	 *                  \ref nvgpu_wrapping_add_u32 "nvgpu_wrapping_add_u32".
	 *               -# Report to |qnx.sdl| unit by calling \ref nvgpu_report_err_to_sdl
	 *                  "nvgpu_report_err_to_sdl" with following parameters:
	 *                  -# \a g
	 *                  -# \ref NVGPU_ERR_MODULE_LTC
	 *                  -# \ref GPU_LTC_CACHE_RSTG_ECC_UNCORRECTED
	 *                     "GPU_LTC_CACHE_RSTG_ECC_UNCORRECTED"
	 *            -# If ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_rstg_m() is
	 *               set in ecc status, then it is considered as fatal error as it is not
	 *               expected and call \ref BUG "BUG()".
	 *         -# t-stg : tag lookup and miss fifos
	 *            -# If ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_tstg_m() is
	 *               set in ecc status:
	 *               -# Increment g->ecc.ltc.tstg_ecc_parity_count[\a ltc][\a slice].counter
	 *                  with uncorrected counter delta with
	 *                  \ref nvgpu_wrapping_add_u32 "nvgpu_wrapping_add_u32".
	 *               -# Report to |qnx.sdl| unit by calling \ref nvgpu_report_err_to_sdl
	 *                  "nvgpu_report_err_to_sdl" with following parameters:
	 *                  -# \a g
	 *                  -# \ref NVGPU_ERR_MODULE_LTC
	 *                  -# \ref GPU_LTC_CACHE_TSTG_ECC_UNCORRECTED
	 *                     "GPU_LTC_CACHE_TSTG_ECC_UNCORRECTED"
	 *            -# If ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_tstg_m() is
	 *               set in ecc status, then it is considered as fatal error as it is not
	 *               expected and call \ref BUG "BUG()".
	 *         -# d-stg : sram data banks and write data queues
	 *            -# If ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_dstg_m() is
	 *               set in ecc status:
	 *               -# The correctable data ram errors are SEC errors.
	 *               -# Increment g->ecc.ltc.ecc_sec_count[\a ltc][\a slice].counter
	 *                  with corrected counter delta with
	 *                  \ref nvgpu_wrapping_add_u32 "nvgpu_wrapping_add_u32".
	 *               -# Report to |qnx.sdl| unit by calling \ref nvgpu_report_err_to_sdl
	 *                  "nvgpu_report_err_to_sdl" with following parameters:
	 *                  -# \a g
	 *                  -# \ref NVGPU_ERR_MODULE_LTC
	 *                  -# \ref GPU_LTC_CACHE_DSTG_ECC_CORRECTED
	 *                     "GPU_LTC_CACHE_DSTG_ECC_CORRECTED"
	 *               -# Flush the L2 cache by calling
	 *                  \ref gops_mm_cache.l2_flush "gops_mm_cache.l2_flush".
	 *               -# If it fails then call \ref BUG "BUG()".
	 *            -# If ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_dstg_m() is
	 *               set in ecc status:
	 *               -# The uncorrectable data ram errors are reported with the dstg non-data
	 *                  ram parity errors in the UNCORRECTED_ERR_DSTG field.
	 *               -# Check if the ECC address corresponds to data ram:
	 *                  -# Increment g->ecc.ltc.ecc_ded_count[\a ltc][\a slice].counter
	 *                     with uncorrected counter delta with
	 *                     \ref nvgpu_wrapping_add_u32 "nvgpu_wrapping_add_u32".
	 *                  -# Report to |qnx.sdl| unit by calling \ref nvgpu_report_err_to_sdl
	 *                     "nvgpu_report_err_to_sdl" with following parameters:
	 *                     -# \a g
	 *                  -# \ref NVGPU_ERR_MODULE_LTC
	 *                     -# \ref GPU_LTC_CACHE_DSTG_ECC_UNCORRECTED
	 *                        "GPU_LTC_CACHE_DSTG_ECC_UNCORRECTED"
	 *               -# Else if the ECC address correspongs to DSTG BE RAM:
	 *                  -# Increment g->ecc.ltc.dstg_be_ecc_parity_count[\a ltc][\a slice].counter
	 *                     with uncorrected counter delta with
	 *                     \ref nvgpu_wrapping_add_u32 "nvgpu_wrapping_add_u32".
	 *                  -# Report to |qnx.sdl| unit by calling \ref nvgpu_report_err_to_sdl
	 *                     "nvgpu_report_err_to_sdl" with following parameters:
	 *                     -# \a g
	 *                     -# \ref NVGPU_ERR_MODULE_LTC
	 *                     -# \ref GPU_LTC_CACHE_DSTG_BE_ECC_UNCORRECTED
	 *                        "GPU_LTC_CACHE_DSTG_BE_ECC_UNCORRECTED"
	 *               -# Else call \ref BUG "BUG()" as this type of ECC error is not supported.
	 *      -# Clear the register ltc_ltc0_lts0_intr3_r() by writing the read value.
	 * - return 0
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ENODEV if invalid LTC number specified.
	 */
	void (*isr)(struct gk20a *g, u32 ltc);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	void (*configure)(struct gk20a *g);
#ifdef CONFIG_NVGPU_NON_FUSA
	void (*en_illegal_compstat)(struct gk20a *g, bool enable);
#endif
	void (*isr_extra)(struct gk20a *g, u32 ltc, u32 slice, u32 *reg_value);
	void (*ltc_intr3_configure_extra)(struct gk20a *g, u32 *reg);
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

/**
 * common.ltc unit hal operations.
 *
 * This structure stores common.ltc unit hal pointers.
 *
 * @see gpu_ops
 */
struct gops_ltc {
	/**
	 * @brief Initialize Level Two Cache (LTC) support.
	 *
	 * @param g [in]            - The GPU driver struct.
	 *                            - The function does not perform validation
	 *                              of g parameter.
	 *
	 * This function reads ltc unit info from GPU h/w and stores
	 * it in #nvgpu_ltc structure. This function also initializes
	 * LTC unit ecc counters. Steps are given below:
	 *
	 * - Allocate memory for g->ltc.
	 * - Initialize LTC floorsweep state by calling the hal
	 *   \ref gops_ltc.init_fs_state "gops_ltc.init_fs_state" with parameter \a g.
	 *   - Initialize g->ltc->max_ltc_count with value returned by calling
	 *     \ref gops_top.get_num_ltcs "g->ops.top.get_num_ltcs" with parameter \a g.
	 *   - Initialize g->ltc->ltc_count with value returned by calling
	 *     \ref gops_priv_ring.enum_ltc "g->ops.priv_ring.enum_ltc" with parameter \a g.
	 *   - Initialize g->ltc->slices_per_ltc with value obtained by applying
	 *     ltc_ltcs_ltss_cbc_param_slices_per_ltc_v() to register value read
	 *     for the register ltc_ltcs_ltss_cbc_param_r().
	 *   - Initialize g->ltc->cacheline_size with value obtained by shifting 512 to left by
	 *     the shift value obtained by applying ltc_ltcs_ltss_cbc_param_cache_line_size_v()
	 *     to register value read for the register ltc_ltcs_ltss_cbc_param_r().
	 * - The L2 cache (LTC) has SEC-DED ECC protection on its data RAM and parity protection
	 *   for byte enables.
	 * - Initialize ECC counters for LTCs. On ga10b there are 2 LTC and each LTC has 2 slices.
	 *   For each following counters are initialized:
	 *   -# ECC SEC count
	 *   -# ECC DED count
	 *   -# RSTG ECC parity count
	 *   -# TSTG ECC parity count
	 *   -# DSTG BE ECC parity count
	 *   See also \ref gops_ltc.intr.isr "gops_ltc.intr.isr".
	 * - Enable stalling interrupt for LTC unit.
	 *   -# Enable interrupts at MC level: call #nvgpu_mc_intr_stall_unit_config by passing
	 *      below parameters:
	 *      -# \a g
	 *      -# #MC_INTR_UNIT_LTC
	 *      -# #MC_INTR_ENABLE
	 *   -# Enable interrupts at unit level.
	 *      The L2 interrupts controlled by ltc_ltcs_ltss_intr_r() register are only enabled
	 *      by nvgpu. Various L2 interrupts are:
	 *      -# IDLE_ERROR_CBC - flag if cbc gets a request while slcg clock is disabled
	 *      -# IDLE_ERROR_TSTG - flag if tstg gets a request while slcg clock is disabled
	 *      -# IDLE_ERROR_DSTG - flag if dstg gets a request while slcg clock is disabled
	 *      -# EVICTED_CB - indicates that a CB was demoted.  Normally this should not happen
	 *                      because the CBs should be flushed during context switch and/or
	 *                      invalidated when no longer used.
	 *      -# ILLEGAL_COMPSTAT - indicates an unexpected compression status given the kind.
	 *      -# BLOCKLINEAR_CB  - indicates that a valid evict_last entry is accessed by a
	 *                           block linear transaction.
	 *      -# ECC_SEC_ERROR - single bit error in data banks. Obsolete.
	 *      -# ECC_DED_ERROR - double bit error in data banks. Obsolete.
	 *      -# DEBUG - unused
	 *      -# ATOMIC_TO_Z - atomic to packing Z or S8.
	 *      -# ILLEGAL_ATOMIC - unsupported atomic op and/or size received.
	 *      -# BLKACTIVITY_ERR - internal error in power sensing block activity monitor
	 *      -# ILLEGAL_COMPSTAT_ACCESS - indicates that some memory access read/wrote into
	 *                                   the memory space reserved for the compression bit
	 *                                   carveout (Bug 942161)
	 *      -# ILLEGAL_ROP_ACCESS - zwr or cwr is scrubbed
	 *
	 *
	 *      Of these, EVICTED_CB and ILLEGAL_COMPSTAT_ACCESS are disabled to reduce noise
	 *      and increase performance. Rest of the interrupts are kept in hardware
	 *      initialized state.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ENOMEM if memory allocation fails for #nvgpu_ltc.
	 */
	int (*init_ltc_support)(struct gk20a *g);

	/**
	 * @brief Remove LTC support.
	 *
	 * @param g [in]            - The GPU driver struct.
	 *                            - The function does not perform validation
	 *                              of g parameter.
	 *
	 * This function will free memory allocated for #nvgpu_ltc structure.
	 * Steps are given below:
	 *
	 * - If g->ltc is NULL return.
	 * - Free g->ltc.
	 * - Set g->ltc to NULL.
	 */
	void (*ltc_remove_support)(struct gk20a *g);

	/**
	 * @brief Returns GPU L2 cache size.
	 *
	 * @param g [in]            - The GPU driver struct.
	 *                            - The function does not perform validation
	 *                              of g parameter.
	 *
	 * This function returns GPU L2 cache size by reading HW ltc
	 * config register.
	 *
	 * - Read register ltc_ltc0_lts0_tstg_info_1_r().
	 * - Get slice_size by applying ltc_ltc0_lts0_tstg_info_1_slice_size_in_kb_v()
	 *   to the register value read above.
	 * - Get slices_per_l2 by applying ltc_ltc0_lts0_tstg_info_1_slices_per_l2_v()
	 *   to the register value read in 1st step.
	 * - Calculate the size as:
	 *   \f$ g->ltc->ltc\_count * slices\_per\_l2 * (slice\_size * 1024) \f$
	 * - Return the size.
	 *
	 * @return Size of L2 cache in bytes.
	 */
	u64 (*determine_L2_size_bytes)(struct gk20a *g);

	/**
	 * @brief Flush GPU L2 cache.
	 *
	 * @param g [in]            - The GPU driver struct.
	 *                            - The function does not perform validation
	 *                              of g parameter.
	 *
	 * This function flushes all L2 cache data to main memory by cleaning
	 * and invalidating all cache sub-units. SW will poll for completion
	 * of each ltc unit cache cleaning/invalidation for 5ms.
	 *
	 * The 5ms timeout is based on following calculations:
	 * Lowest EMC clock rate will be around 204MHz and thus available
	 * bandwidth is 128B (Cacheline size) * 2 (LTCs) * 204MHz = ~52GB/s.
	 * Of that bandwidth, GPU will likely get about half, so 26GB/s
	 * at worst. Assuming at most 1MB of GPU L2 cache, worst case
	 * it will take 1MB/26GB/s = 38us.
	 * So 5ms timeout here should be more than enough.
	 *
	 * - First stage is to clean the LTCs with the below write:
	 *   \code
	 *	nvgpu_writel(g, ltc_ltcs_ltss_tstg_cmgmt1_r(),
	 *		ltc_ltcs_ltss_tstg_cmgmt1_clean_pending_f() |
	 *		ltc_ltcs_ltss_tstg_cmgmt1_max_cycles_between_cleans_3_f() |
	 *		ltc_ltcs_ltss_tstg_cmgmt1_clean_wait_for_fb_to_pull_true_f() |
	 *		ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_last_class_true_f() |
	 *		ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_normal_class_true_f() |
	 *		ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_first_class_true_f());
	 *   \endcode
	 * - This cleans all LTCs.
	 * - For each LTC, wait for clean to finish for 5ms.
	 *   -# Initialize poll timer with timeout of 5ms by calling
	 *      \ref nvgpu_timeout_init "nvgpu_timeout_init"
	 *      with below parameters:
	 *      -# \a g
	 *      -# local timeout variable
	 *      -# 5
	 *      -# \ref NVGPU_TIMER_CPU_TIMER "NVGPU_TIMER_CPU_TIMER"
	 *   -# do while LTCs are not cleared or timeout is not expired
	 *      -# Read ltc_ltc0_ltss_tstg_cmgmt1_r() corresponding to the LTC.
	 *         The offset is calculated as:
	 *      \f$ltc\_ltc0\_ltss\_tstg\_cmgmt1\_r() + (ltc * GPU\_LIT\_LTC\_STRIDE)\f$
	 *      -# Check if ltc_ltc0_ltss_tstg_cmgmt1_clean_pending_f() is cleared.
	 * - Second stage is to invalidate the LTCs with the below write:
	 *   \code
	 *	nvgpu_writel(g, ltc_ltcs_ltss_tstg_cmgmt0_r(),
	 *	     ltc_ltcs_ltss_tstg_cmgmt0_invalidate_pending_f() |
	 *	     ltc_ltcs_ltss_tstg_cmgmt0_max_cycles_between_invalidates_3_f() |
	 *	     ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_last_class_true_f() |
	 *	     ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_normal_class_true_f() |
	 *	     ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_first_class_true_f());
	 *   \endcode
	 * - This invalidates all LTCs.
	 * - For each LTC, wait for invalidate to finish for 5ms.
	 *   -# Initialize poll timer with timeout of 5ms by calling
	 *      \ref nvgpu_timeout_init "nvgpu_timeout_init"
	 *      with below parameters:
	 *      -# \a g
	 *      -# local timeout variable
	 *      -# 5
	 *      -# \ref NVGPU_TIMER_CPU_TIMER "NVGPU_TIMER_CPU_TIMER"
	 *   -# do while LTCs are not cleared or timeout is not expired
	 *      -# Read ltc_ltc0_ltss_tstg_cmgmt0_r() corresponding to the LTC.
	 *      -# Check if ltc_ltc0_ltss_tstg_cmgmt0_invalidate_pending_f() is cleared.
	 */
	void (*flush)(struct gk20a *g);

	/** This structure stores ltc interrupt subunit hal pointers. */
	struct gops_ltc_intr intr;

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	/**
	 * @brief Initialize LTC unit ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function allocates memory to track the ecc error counts
	 * for LTC unit.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*ecc_init)(struct gk20a *g);

	void (*init_fs_state)(struct gk20a *g);
#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
	void (*set_enabled)(struct gk20a *g, bool enabled);
#endif
	int (*ltc_lts_set_mgmt_setup)(struct gk20a *g);
#ifdef CONFIG_NVGPU_GRAPHICS
	void (*set_zbc_color_entry)(struct gk20a *g,
					u32 *color_val_l2, u32 index);
	void (*set_zbc_depth_entry)(struct gk20a *g,
					u32 depth_val, u32 index);
	void (*set_zbc_s_entry)(struct gk20a *g,
					u32 s_val, u32 index);
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	bool (*pri_is_ltc_addr)(struct gk20a *g, u32 addr);
	bool (*is_ltcs_ltss_addr)(struct gk20a *g, u32 addr);
	bool (*is_ltcn_ltss_addr)(struct gk20a *g, u32 addr);
	void (*split_lts_broadcast_addr)(struct gk20a *g, u32 addr,
						u32 *priv_addr_table,
						u32 *priv_addr_table_index);
	void (*split_ltc_broadcast_addr)(struct gk20a *g, u32 addr,
						u32 *priv_addr_table,
						u32 *priv_addr_table_index);
	int (*set_l2_max_ways_evict_last)(struct gk20a *g, struct nvgpu_tsg *tsg,
				u32 num_ways);
	int (*get_l2_max_ways_evict_last)(struct gk20a *g, struct nvgpu_tsg *tsg,
			u32 *num_ways);
	u32 (*pri_is_lts_tstg_addr)(struct gk20a *g, u32 addr);
	int (*set_l2_sector_promotion)(struct gk20a *g, struct nvgpu_tsg *tsg,
			u32 policy);
	u32 (*pri_shared_addr)(struct gk20a *g, u32 addr);
#endif
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

};

#endif /* NVGPU_GOPS_LTC_H */
