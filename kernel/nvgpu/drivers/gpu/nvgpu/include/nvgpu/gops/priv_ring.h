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
#ifndef NVGPU_GOPS_PRIV_RING_H
#define NVGPU_GOPS_PRIV_RING_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * common.priv_ring interface.
 */
struct gk20a;

/**
 * common.priv_ring unit hal operations.
 *
 * This structure stores priv_ring unit hal pointers.
 *
 * @see gpu_ops
 */
struct gops_priv_ring {
	/**
	 * @brief Enable priv ring h/w register access for S/W
	 *
	 * @param g [in]     [in] The GPU driver struct.
	 * - The function does not perform validation of g parameter.
	 *
	 * Enable Privilege Ring to access H/W functionality.
	 * Steps:
	 *  - Load slcg priv ring values through a call to
	 *    \ref #nvgpu_cg_slcg_priring_load_enable "nvgpu_cg_slcg_priring_load_enable(g)".
	 *  - Invoke \ref #nvgpu_cic_mon_intr_stall_unit_config
	 *    "nvgpu_cic_mon_intr_stall_unit_config".
	 *    with parameters #NVGPU_CIC_INTR_UNIT_PRIV_RING and #NVGPU_CIC_INTR_ENABLE.
	 *  - Initiate priv ring enumeration by writing
	 *    #pri_ringmaster_command_cmd_enumerate_and_start_ring_f to register
	 *    pri_ringmaster_command_r().
	 *  - Write #CONFIG_RING_WAIT_FOR_RING_START_COMPLETE to register
	 *    pri_ringstation_sys_decode_config_r() followed by a read of
	 *    pri_ringstation_sys_decode_config_r().
	 *  - Enable the PRIV_RING unit stalling interrupt at MC level by calling \ref
	 *    nvgpu_mc_intr_stall_unit_config #nvgpu_mc_intr_stall_unit_config with parameters
	 *    \a g, #MC_INTR_UNIT_PRIV_RING, #MC_INTR_ENABLE respectively.
	 *
	 * @return 0 Always return success after completion.
	 */
	int (*enable_priv_ring)(struct gk20a *g);

	/**
	 * @brief ISR handler for priv ring error.
	 *
	 * @param g     [in] The GPU driver struct.
	 * - The function does not perform validation of g parameter.
	 *
	 * This functions handles interrupts related to priv ring faults.
	 * priv ring faults are related to priv ring connection errors and
	 * global register write errors.
	 *
	 * Steps:
	 * - Reads the values of registers pri_ringmaster_intr_status0_r()
	 *   and pri_ringmaster_intr_status1_r() as \a status0 and \a status1
	 *   respectively.
	 * - Log an error message displaying the values of \a status0 and
	 *   \a status1.
	 * - Log an error if pri_ringmaster_intr_status0_ring_start_conn_fault_v(status0)
	 *   doesn't equal zero.
	 * - Log an error if pri_ringmaster_intr_status0_disconnect_fault_v(status0)
	 *   doesn't equal zero.
	 * - Log an error if pri_ringmaster_intr_status0_overflow_fault_v(status0)
	 *   doesn't equal zero
	 * - If pri_ringmaster_intr_status0_gbl_write_error_sys_v(status0) doesn't equal
	 * 	 zero, then do the below steps
	 *   - Read the value of register pri_ringstation_sys_priv_error_info_r() as \a
	 *     error_info.
	 *   - Read the value of register pri_ringstation_sys_priv_error_code_r() as \a
	 *     error_code.
	 *   - Read the value of register pri_ringstation_sys_priv_error_adr_r() as \a
	 *     error_adr.
	 *   - Read the value of register pri_ringstation_sys_priv_error_wrdat_r() as \a
	 *     error_wrdat.
	 *   - Log error message with above values. i.e. \a error_info, \a error_code,
	 *     \a error_adr and \a error_wrdat.
	 *   - Invoke \ref #decode_error_code "g->ops.priv_ring.decode_error_code" with params \a g,
	 *     \a error_code respectively.
	 * - If \a status1 doesn't equal zero, then do the following steps
	 *   - Read \ref nvgpu_get_litter_value "nvgpu_get_litter_value" with params \a g
	 *     and #GPU_LIT_GPC_PRIV_STRIDE into \a gpc_stride.
	 *   - Iterate a variable \a gpc(via \a for loop\a) from 0 to \ref #get_gpc_count
	 *     "g->ops.priv_ring.get_gpc_count(g)" and
	 *     increment by one.
	 *     - Safely add \a gpc_stride to \a gpc and store in \a gpc_offset
	 *     - Read the value of register pri_ringstation_gpc_gpc0_priv_error_info_r(gpc_offset)
	 *       into \a error_info.
	 *     - Read the value of register pri_ringstation_gpc_gpc0_priv_error_code_r(gpc_offset)
	 *       into \a error_code.
	 *     - Read the value of register pri_ringstation_gpc_gpc0_priv_error_adr_r(gpc_offset)
	 *       into \a error_adr.
	 *     - Read the value of register pri_ringstation_gpc_gpc0_priv_error_wrdat_r(gpc_offset)
	 *       into \a error_wrdat.
	 *     - Log error message with above values. i.e. \a error_info, \a error_code,
	 *       \a error_adr and \a error_wrdat.
	 *     - Invoke \ref #decode_error_code "g->ops.priv_ring.decode_error_code(g, error_code)"
	 *     - Update \a status1 as follows
	 *       \code{.c}
	 *       status1 = status1 & (~(BIT32(gpc)));
	 *       \endcode
	 *     - if \a status1 equals zero then break from the for loop.
	 * - Clear Interrupt by following steps.
	 *   - Read the value of the register pri_ringmaster_command_r() into \a cmd.
	 *   - Call \ref #set_field "set_field" with params \a cmd,
	 *     \a pri_ringmaster_command_cmd_m(),
	 *     \a pri_ringmaster_command_cmd_ack_interrupt_f()) respectively
	 *     and store the value in \a cmd.
	 *   - Write the value of \a cmd back in pri_ringmaster_command_r()
	 * - Read value of register pri_ringmaster_command_r() in \a cmd.
	 * - Poll until Interrupt is cleared. i.e. following steps are executed in a while loop
	 *   - While value of \a cmd doesn't equal pri_ringmaster_command_cmd_no_cmd_v()
	 *     and \a retry doesn't equal zero
	 *     - Call \ref #nvgpu_udelay with param #GP10B_PRIV_RING_POLL_CLEAR_INTR_UDELAY.
	 *     - Read value of register pri_ringmaster_command_r() into \a cmd
	 *     - Subtract \a retry by 1.
	 * - If \a retry equals zero, log error for interrupt acknowledgement failure.
	 */
	void (*isr)(struct gk20a *g);

	/**
	 * @brief Unit level interrupt handler for priv ring
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param status0 [in]		Value of interrupt status register
	 *
	 * This function handles interrupts associated with priv ring
	 * status0 interrupt register.
	 */

	void (*isr_handle_0)(struct gk20a *g, u32 status0);

	/**
	 * @brief Unit level interrupt handler for priv ring
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param status1 [in]		Value of interrupt status register
	 *
	 * This function handles interrupts associated with priv ring
	 * status1 interrupt register.
	 */

	void (*isr_handle_1)(struct gk20a *g, u32 status1);

	/**
	 * @brief Sets Priv ring timeout value in cycles.
	 * @brief Sets Priv ring timeout value in cycles when initializing GR H/W unit.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * - The function does not perform validation of g parameter.
	 *
	 * This functions sets h/w specified timeout value in the number of
	 * cycles after sending a priv request. If timeout is exceeded then
	 * timeout error is reported back via \ref #isr_stall "g->ops.mc.isr_stall(g)".
	 *
	 * Steps:
	 *       - Write \a 0x800 to register pri_ringstation_sys_master_config_r()
	 *         at offset 0x15.
	 *       - Write \a 0x800 to register pri_ringstation_gpc_master_config_r()
	 *         at offset 0xa.
	 */

	void (*set_ppriv_timeout_settings)(struct gk20a *g);
	/**
	 * @brief Returns number of enumerated Level Two Cache (LTC) chiplets.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * - The function does not perform validation of g parameter.
	 *
	 * This function returns number of enumerated ltc chiplets after
	 * the enumeration step of enable_priv_ring. The number of valid ltc chiplets
	 * returned equals 2.
	 *
	 * Steps:
	 *      - Read and return value of register pri_ringmaster_enum_ltc_r().
	 *
	 * @return U32 Number of ltc units.
	 */
	u32 (*enum_ltc)(struct gk20a *g);

	/**
	 * @brief Returns number of enumerated Graphics Processing Cluster (GPC)
	 * chiplets.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * - The function does not perform validation of g parameter.
	 *
	 * This function returns number of enumerated gpc chiplets after
	 * the enumeration step of enable_priv_ring.
	 *
	 * Steps:
	 *      - Read the value of register pri_ringmaster_enum_gpc_r().
	 *      - Return value of pri_ringmaster_enum_gpc_count_v() at offset obtained from above.
	 *
	 * @return U32 Number of gpc units.
	 */
	u32 (*get_gpc_count)(struct gk20a *g);

	/**
	 * @brief Returns number of enumerated Frame Buffer Partitions (FBP).
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * - The function does not perform validation of g parameter.
	 *
	 * This function returns number of enumerated fbp chiplets after
	 * the enumeration step of enable_priv_ring.
	 *
	 * Steps:
	 *      - Read the value of register pri_ringmaster_enum_fbp_r() as \a offset.
	 *      - Return value of pri_ringmaster_enum_fbp_count_v() at offset obtained
	 *        from above value.
	 *
	 * @return U32 Number of fbp units.
	 */
	u32 (*get_fbp_count)(struct gk20a *g);

	/**
	 * @brief Decodes priv ring error code.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * - The function does not perform validation of g parameter.
	 *
	 * @param error_code [in] Priv error code reported from h/w.
	 * - This function does not perform validation of \a error_code parameter.
	 *
	 * This function decodes and prints appropriate error message for
	 * priv error_code reported by h/w.
	 *
	 * Steps:
	 *      - Declare static string arrays error_type_badf1xyy, error_type_badf2xyy,
	 *        error_type_badf3xyy, error_type_badf5xyy as below.
	 *      \code
	 *		static const char *const error_type_badf1xyy[] = {
	 *			"client timeout",
	 *			"decode error",
	 *			"client in reset",
	 *			"client floorswept",
	 *			"client stuck ack",
	 *			"client expected ack",
	 *			"fence error",
	 *			"subid error",
	 *			"byte access unsupported",
	 *		};
	 *
	 *		static const char *const error_type_badf2xyy[] = {
	 *			"orphan gpc/fbp"
	 *		};
	 *
	 *		static const char *const error_type_badf3xyy[] = {
	 *			"priv ring dead"
	 *		};
	 *
	 *		static const char *const error_type_badf5xyy[] = {
	 *			"client error",
	 *			"priv level violation",
	 *			"indirect priv level violation",
	 *			"local ring error",
	 *			"falcon mem access priv level violation",
	 *			"pri route error"
	 *		};
	 *      \endcode
	 *      - Invoke \ref #nvgpu_report_pri_err "nvgpu_report_err_to_sdl" with parameters \a g,
	 *         #GPU_PRI_ACCESS_VIOLATION, respectively.
	 *      - Declare a variable error_type_index and store the bits [8-12] as below.
	 *        error_type_index will be used as an index to the above error tables.
	 *        error_code is also updated.
	 *      \code{.c}
	 *      error_type_index = (error_code & 0x00000f00U) >> 8U;
	 *      error_code = error_code & 0xBADFf000U;
	 *      \endcode
	 *      - If error_code equals 0xBADF1000U
	 *        - log error_type_badf1xyy[error_type_index] if error_type_index is within bounds.
	 *      - else if error_code equals 0xBADF2000U
	 *        - log error_type_badf2xyy[error_type_index] if error_type_index is within bounds.
	 *      - else if error_code equals 0xBADF3000U
	 *        - log error_type_badf3xyy[error_type_index] if error_type_index is within bounds.
	 *      - else f error_code equals 0xBADF5000U
	 *        - log error_type_badf5xyy[error_type_index] if error_type_index is within bounds.
	 *      - else
	 *        - log a "non-supported" debug message.
	*/
	void (*decode_error_code)(struct gk20a *g, u32 error_code);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_PROFILER
	void (*read_pri_fence)(struct gk20a *g);
#endif
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#if defined(CONFIG_NVGPU_HAL_NON_FUSA) && defined(CONFIG_NVGPU_MIG)
	int (*config_gr_remap_window)(struct gk20a *g, u32 gr_syspipe_indx,
		bool enable);
	int (*config_gpc_rs_map)(struct gk20a *g, bool enable);
#endif

};

#endif /* NVGPU_GOPS_PRIV_RING_H */

