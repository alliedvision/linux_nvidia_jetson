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
#ifndef NVGPU_GOPS_TOP_H
#define NVGPU_GOPS_TOP_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * TOP unit HAL interface
 *
 */
struct gk20a;
struct nvgpu_device;

/**
 * TOP unit HAL operations
 *
 * @see gpu_ops
 */
struct gops_top {
	/**
	 * @brief Parse the GPU device table into a SW representation.
	 *
	 * @param g [in]		GPU device struct pointer
	 * @param token [in]		Token to pass into table parsing code.
	 *
	 * The devinfo table is an array of registers which contains a list of
	 * all devices in the GPU. This list can be parsed by SW to dynamically
	 * determine the presence of HW devices on the GPU.
	 *
	 * Each table entry is a sequence of registers that SW can read. The table
	 * format varies from chip to chip in subtle ways; this particular HAL
	 * is repsonsible for reading a single device from the table.
	 *
	 * \a token is an opaque argument the parser can use for storing state
	 * as the table is parsed. This function is intented to be called
	 * repeatedly to parse all devices in the chip. It will return devices
	 * until there are no more devices to return at which point it will
	 * return NULL. To begine the parsing, \a token should be set to
	 * #NVGPU_DEVICE_TOKEN_INIT.
	 *
	 * @return A valid pointer to an nvgpu_device or NULL if no device was
	 * parsed or an error occured.
	 */
	struct nvgpu_device *(*parse_next_device)(struct gk20a *g, u32 *token);

	/**
	 * @brief Gets maximum number of GPCs in a GPU as programmed in HW
	 *
	 * @param g [in]		GPU device struct pointer
	 *
	 * This HAL reads the NV_PTOP_SCAL_NUM_GPCS HW register, extracts the
	 * NV_PTOP_SCAL_NUM_GPCS_VALUE field and returns it.
	 *
	 * @return The number of GPCs as read from above mentioned HW register.
	 */
	u32 (*get_max_gpc_count)(struct gk20a *g);

	/**
	 * @brief Gets the maximum number of TPCs per GPC in a GPU as programmed
	 * 	in HW.
	 *
	 * @param g [in]		GPU device struct pointer
	 *
	 * This HAL reads the NV_PTOP_SCAL_TPC_PER_GPC HW register, extracts the
	 * NV_PTOP_SCAL_NUM_TPC_PER_GPC_VALUE field and returns it.
	 *
	 * @return The number of TPC per GPC as read from the above mentioned
	 *	HW register.
	 */
	u32 (*get_max_tpc_per_gpc_count)(struct gk20a *g);

	/**
	 * @brief Gets the maximum number of FBPs in a GPU as programmed in HW
	 *
	 * @param g [in]		GPU device struct pointer
	 *
	 * This HAL reads the NV_PTOP_SCAL_NUM_FBPS HW register, extracts the
	 * NV_PTOP_SCAL_NUM_FBPS_VALUE field and returns it.
	 *
	 * @return The number of FBPs as read from above mentioned HW register
	 */
	u32 (*get_max_fbps_count)(struct gk20a *g);

	/**
	 * @brief Gets the maximum number of LTCs per FBP in a GPU as programmed
	 * 	in HW.
	 *
	 * @param g [in]		GPU device struct pointer
	 *
	 * This HAL reads the NV_PTOP_SCAL_LTC_PER_FBP HW register, extracts the
	 * NV_PTOP_SCAL_NUM_LTC_PER_FBP_VALUE field and returns it.
	 *
	 * @return The number of LTC per FBP as read from the above mentioned
	 *	HW register.
	 */
	u32 (*get_max_ltc_per_fbp)(struct gk20a *g);

	/**
	 * @brief Gets the number of LTCs in a GPU as programmed in HW
	 *
	 * @param g [in]		GPU device struct pointer
	 *
	 * This HAL reads the NV_PTOP_SCAL_NUM_LTCS HW register, extracts the
	 * NV_PTOP_SCAL_NUM_LTCS_VALUE field and returns it.
	 *
	 * @return The number of LTCs as read from above mentioned HW register
	 */
	u32 (*get_num_ltcs)(struct gk20a *g);

	/**
	 * @brief Gets the number of copy engines as programmed in HW
	 *
	 * @param g [in]		GPU device struct pointer
	 *
	 * This HAL reads the NV_PTOP_SCAL_NUM_CES HW register, extracts the
	 * NV_PTOP_SCAL_NUM_CES_VALUE field and returns it.
	 *
	 * @return The number of copy engines as read from above mentioned HW
	 * 	register
	 */
	u32 (*get_num_lce)(struct gk20a *g);

	/**
	 * @brief Gets the maximum number of LTS per LTC in a GPU as programmed
	 * 	in HW.
	 *
	 * @param g [in]		GPU device struct pointer
	 *
	 * This HAL reads the NV_PTOP_SCAL_NUM_SLICES_PER_LTC HW register,
	 * extracts the NV_PTOP_SCAL_NUM_SLICES_PER_LTC_VALUE field and
	 * returns it.
	 *
	 * @return The number of LTS per LTC as read from the above mentioned
	 *	HW register.
	 */
	u32 (*get_max_lts_per_ltc)(struct gk20a *g);


	/**
	 * @brief Gets the maximum number of PESs per GPC in a GPU as programmed
	 * 	in HW.
	 *
	 * @param g [in]		GPU device struct pointer
	 *
	 * This HAL reads the NV_PTOP_SCAL_NUM_PES_PER_GPC HW register,
	 *
	 * @return The number of PES per GPC as read from the above mentioned
	 *	HW register.
	 */
	u32 (*get_max_pes_per_gpc)(struct gk20a *g);

	/**
	 * @brief Gets the maximum number of ROPs per GPC in a GPU as programmed
	 * 	in HW.
	 *
	 * @param g [in]		GPU device struct pointer
	 *
	 * This HAL reads the NV_PTOP_SCAL_NUM_ROP_PER_GPC HW register,
	 *
	 * @return The number of ROP per GPC as read from the above mentioned
	 *	HW register.
	 */
	u32 (*get_max_rop_per_gpc)(struct gk20a *g);


	/** @cond DOXYGEN_SHOULD_SKIP_THIS */

	/**
	 * NON-FUSA HALs
	 */
	u32 (*get_nvhsclk_ctrl_e_clk_nvl)(struct gk20a *g);
	void (*set_nvhsclk_ctrl_e_clk_nvl)(struct gk20a *g, u32 val);
	u32 (*get_nvhsclk_ctrl_swap_clk_nvl)(struct gk20a *g);
	void (*set_nvhsclk_ctrl_swap_clk_nvl)(struct gk20a *g, u32 val);
	u32 (*get_max_fbpas_count)(struct gk20a *g);
	u32 (*read_top_scratch1_reg)(struct gk20a *g);
	u32 (*top_scratch1_devinit_completed)(struct gk20a *g,
					u32 value);
	/**
	 * HALs used within "Top" unit. Private HALs.
	 */
	void (*device_info_parse_enum)(struct gk20a *g,
					u32 table_entry,
					u32 *engine_id, u32 *runlist_id,
					u32 *intr_id, u32 *reset_id);
	int (*device_info_parse_data)(struct gk20a *g,
					u32 table_entry, u32 *inst_id,
					u32 *pri_base, u32 *fault_id);
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

};

#endif /* NVGPU_GOPS_TOP_H */
