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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef CIC_MON_PRIV_H
#define CIC_MON_PRIV_H

#include <nvgpu/types.h>
#include <nvgpu/lock.h>

struct gk20a;
struct nvgpu_err_hw_module;
struct nvgpu_err_msg;
struct gpu_err_header;

#define ERR_INJECT_TEST_PATTERN		(0xA5U)

/*
 * This struct contains members related to error-policy look-up table,
 * number of units reporting errors.
 */
struct nvgpu_cic_mon {
	/** Pointer for error look-up table. */
	struct nvgpu_err_hw_module *err_lut;

	/** Total number of GPU HW modules considered in CIC. */
	u32 num_hw_modules;
};

/**
 * @brief Inject ECC error.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit [in]		- Index of HW unit.
 * @param err_index [in]	- Error index.
 * @param inst [in]		- Instance ID.
 *
 *  - Sets values for error address and error count.
 *  - Invokes error reporting API with the required set of inputs.
 *
 * @return None
 */
void nvgpu_inject_ecc_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 inst);

/**
 * @brief Inject HOST error.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit [in]		- Index of HW unit.
 * @param err_index [in]	- Error index.
 * @param sub_err_type [in]	- Sub error type.
 *
 *  - Invokes error reporting API with the required set of inputs.
 *
 * @return None
 */
void nvgpu_inject_host_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 sub_err_type);

/**
 * @brief Inject GR error.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit [in]		- Index of HW unit.
 * @param err_index [in]	- Error index.
 * @param sub_err_type [in]	- Sub error type.
 *
 *  - Sets values for GR exception and SM machine check error information.
 *  - Invokes error reporting API with the required set of inputs.
 *
 * @return None
 */
void nvgpu_inject_gr_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 sub_err_type);

/**
 * @brief Inject CE error.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit [in]		- Index of HW unit.
 * @param err_index [in]	- Error index.
 * @param sub_err_type [in]	- Sub error type.
 *
 *  - Invokes error reporting API with the required set of inputs.
 *
 * @return None
 */
void nvgpu_inject_ce_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 sub_err_type);

/**
 * @brief Inject CE error.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit [in]		- Index of HW unit.
 * @param err_index [in]	- Error index.
 * @param err_code [in]		- Error code.
 *
 *  - Invokes error reporting API with the required set of inputs.
 *
 * @return None
 */
void nvgpu_inject_pri_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 err_code);

/**
 * @brief Inject PMU error.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit [in]		- Index of HW unit.
 * @param err_index [in]	- Error index.
 * @param sub_err_type [in]	- Sub error type.
 *
 *  - Sets values for error info.
 *  - Invokes error reporting API with the required set of inputs.
 *
 * @return None
 */
void nvgpu_inject_pmu_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 sub_err_type);

/**
 * @brief Inject CTXSW error.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit [in]		- Index of HW unit.
 * @param err_index [in]	- Error index.
 * @param inst [in]	        - Instance ID.
 *
 *  - Sets values for error info.
 *  - Invokes error reporting API with the required set of inputs.
 *
 * @return None
 */
void nvgpu_inject_ctxsw_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 inst);

/**
 * @brief Inject MMU error.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit [in]		- Index of HW unit.
 * @param err_index [in]	- Error index.
 * @param sub_err_type [in]	- Sub error type.
 *
 *  - Sets values for mmu page fault info.
 *  - Invokes error reporting API with the required set of inputs.
 *
 * @return None
 */
void nvgpu_inject_mmu_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 sub_err_type);

/**
 * @brief Initialize error message header.
 *
 * @param header [in]		- Error message header.
 *
 *  This is used to initialize error message header.
 *
 * @return None
 */
void nvgpu_init_err_msg_header(struct gpu_err_header *header);

/**
 * @brief Initialize error message.
 *
 * @param msg [in]		- Error message.
 *
 *  This is used to initialize error message that is common
 *  for all HW units.
 *
 * @return None
 */
void nvgpu_init_err_msg(struct nvgpu_err_msg *msg);

/**
 * @brief Initialize error message for HOST unit.
 *
 * @param msg [in]		- Error message.
 *
 *  This is used to initialize error message that is specific to HOST unit.
 *
 * @return None
 */
void nvgpu_init_host_err_msg(struct nvgpu_err_msg *msg);

/**
 * @brief Initialize ECC error message.
 *
 * @param msg [in]		- Error message.
 *
 *  This is used to initialize error message that is specific to ECC errors.
 *
 * @return None
 */
void nvgpu_init_ecc_err_msg(struct nvgpu_err_msg *msg);

/**
 * @brief Initialize error message for PRI unit.
 *
 * @param msg [in]		- Error message.
 *
 *  This is used to initialize error message that is specific to PRI unit.
 *
 * @return None
 */
void nvgpu_init_pri_err_msg(struct nvgpu_err_msg *msg);

/**
 * @brief Initialize error message for CE unit.
 *
 * @param msg [in]		- Error message.
 *
 *  This is used to initialize error message that is specific to CE unit.
 *
 * @return None
 */
void nvgpu_init_ce_err_msg(struct nvgpu_err_msg *msg);

/**
 * @brief Initialize error message for PMU unit.
 *
 * @param msg [in]		- Error message.
 *
 *  This is used to initialize error message that is specific to PMU unit.
 *
 * @return None
 */
void nvgpu_init_pmu_err_msg(struct nvgpu_err_msg *msg);

/**
 * @brief Initialize error message for GR unit.
 *
 * @param msg [in]		- Error message.
 *
 *  This is used to initialize error message that is specific to GR unit.
 *
 * @return None
 */
void nvgpu_init_gr_err_msg(struct nvgpu_err_msg *msg);

/**
 * @brief Initialize error message for CTXSW.
 *
 * @param msg [in]		- Error message.
 *
 *  This is used to initialize error message that is specific to CTXSW.
 *
 * @return None
 */
void nvgpu_init_ctxsw_err_msg(struct nvgpu_err_msg *msg);

/**
 * @brief Initialize error message for MMU unit.
 *
 * @param msg [in]		- Error message.
 *
 *  This is used to initialize error message that is specific to MMU unit.
 *
 * @return None
 */
void nvgpu_init_mmu_err_msg(struct nvgpu_err_msg *msg);

#endif /* CIC_MON_PRIV_H */
