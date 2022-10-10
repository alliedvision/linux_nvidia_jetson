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

#ifndef NVGPU_CIC_MON_H
#define NVGPU_CIC_MON_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/log.h>

#define MISC_EC_SW_ERR_CODE_0		(0U)
#define HW_UNIT_ID_MASK			(0xFU)
#define ERR_ID_MASK			(0x1FU)
#define ERR_ID_FIELD_SHIFT		(4U)
#define CORRECTED_BIT_FIELD_SHIFT	(9U)
#define ERR_REPORT_TIMEOUT_US		(5000U)
#define SS_WAIT_DURATION_US		(500U)
#define MAX_SS_RETRIES (ERR_REPORT_TIMEOUT_US / SS_WAIT_DURATION_US)

#define U32_BITS		32U
#define DIV_BY_U32_BITS(x)	((x) / U32_BITS)
#define MOD_BY_U32_BITS(x)	((x) % U32_BITS)

#define RESET_ID_TO_REG_IDX(x)		DIV_BY_U32_BITS((x))
#define RESET_ID_TO_REG_BIT(x)		MOD_BY_U32_BITS((x))
#define RESET_ID_TO_REG_MASK(x)		BIT32(RESET_ID_TO_REG_BIT((x)))

#define GPU_VECTOR_TO_LEAF_REG(i)	DIV_BY_U32_BITS((i))
#define GPU_VECTOR_TO_LEAF_BIT(i)	MOD_BY_U32_BITS((i))
#define GPU_VECTOR_TO_LEAF_MASK(i) (BIT32(GPU_VECTOR_TO_LEAF_BIT(i)))
#define GPU_VECTOR_TO_SUBTREE(i)	((GPU_VECTOR_TO_LEAF_REG(i)) / 2U)
#define GPU_VECTOR_TO_LEAF_SHIFT(i) \
	(nvgpu_safe_mult_u32(((GPU_VECTOR_TO_LEAF_REG(i)) % 2U), 32U))

#define HOST2SOC_0_SUBTREE		0U
#define HOST2SOC_1_SUBTREE		1U
#define HOST2SOC_2_SUBTREE		2U
#define HOST2SOC_3_SUBTREE		3U
#define HOST2SOC_NUM_SUBTREE		4U

#define HOST2SOC_SUBTREE_TO_TOP_IDX(i)	((i) / 32U)
#define HOST2SOC_SUBTREE_TO_TOP_BIT(i)	((i) % 32U)
#define HOST2SOC_SUBTREE_TO_LEAF0(i) \
		(nvgpu_safe_mult_u32((i), 2U))
#define HOST2SOC_SUBTREE_TO_LEAF1(i) \
		(nvgpu_safe_add_u32((nvgpu_safe_mult_u32((i), 2U)), 1U))

#define STALL_SUBTREE_TOP_IDX		0U
#define STALL_SUBTREE_TOP_BITS \
		((BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(HOST2SOC_1_SUBTREE))) | \
		 (BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(HOST2SOC_2_SUBTREE))) | \
		 (BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(HOST2SOC_3_SUBTREE))))

/**
 * These should not contradict NVGPU_CIC_INTR_UNIT_* defines.
 */
#define NVGPU_CIC_INTR_UNIT_MMU_FAULT_ECC_ERROR			10U
#define NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT_ERROR	11U
#define NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT_ERROR		12U
#define NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT		13U
#define NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT		14U
#define NVGPU_CIC_INTR_UNIT_MMU_INFO_FAULT			15U
#define NVGPU_CIC_INTR_UNIT_RUNLIST_TREE_0			16U
#define NVGPU_CIC_INTR_UNIT_RUNLIST_TREE_1			17U
#define NVGPU_CIC_INTR_UNIT_GR_STALL				18U
#define NVGPU_CIC_INTR_UNIT_CE_STALL				19U
#define NVGPU_CIC_INTR_UNIT_GSP					20U
#define NVGPU_CIC_INTR_UNIT_MAX					21U

#define NVGPU_CIC_INTR_VECTORID_SIZE_MAX				32U
#define NVGPU_CIC_INTR_VECTORID_SIZE_ONE				1U

#define RUNLIST_INTR_TREE_0					0U
#define RUNLIST_INTR_TREE_1					1U

void nvgpu_cic_mon_intr_unit_vectorid_init(struct gk20a *g, u32 unit, u32 *vectorid,
		u32 num_entries);
bool nvgpu_cic_mon_intr_is_unit_info_valid(struct gk20a *g, u32 unit);
bool nvgpu_cic_mon_intr_get_unit_info(struct gk20a *g, u32 unit, u32 *subtree,
		u64 *subtree_mask);

struct nvgpu_err_desc;
/**
 * @file
 *
 * Public structs and APIs exposed by Central Interrupt Controller
 * (CIC) unit.
 */

/**
 * @defgroup NVGPU_CIC_INTR_PENDING_DEFINES
 *
 * Defines of all CIC unit interrupt pending scenarios.
 */

/**
 * @ingroup NVGPU_CIC_INTR_PENDING_DEFINES
 * Indicates that pending interrupts should be handled in the ISR thread.
 */
#define NVGPU_CIC_INTR_HANDLE		0U
/**
 * @ingroup NVGPU_CIC_INTR_PENDING_DEFINES
 * Indicates that pending interrupts are erroneous and should be cleared.
 */
#define NVGPU_CIC_INTR_UNMASK		BIT32(0)
/**
 * @ingroup NVGPU_CIC_INTR_PENDING_DEFINES
 * Indicates that there are no pending interrupts.
 */
#define NVGPU_CIC_INTR_NONE		BIT32(1)
/**
 * @ingroup NVGPU_CIC_INTR_PENDING_DEFINES
 * Indicates that quiesce state is pending. This basically means there is no
 * need to handle interrupts (if any) as driver will enter quiesce state.
 */
#define NVGPU_CIC_INTR_QUIESCE_PENDING	BIT32(2)

/**
 * @defgroup NVGPU_CIC_INTR_TYPE_DEFINES
 *
 * Defines of all CIC unit interrupt types.
 */

/**
 * @ingroup NVGPU_CIC_INTR_TYPE_DEFINES
 * Index for accessing registers corresponding to stalling interrupts.
 */
#define NVGPU_CIC_INTR_STALLING		0U
/**
 * @ingroup NVGPU_CIC_INTR_TYPE_DEFINES
 * Index for accessing registers corresponding to non-stalling
 * interrupts.
 */
#define NVGPU_CIC_INTR_NONSTALLING	1U

/**
 * @defgroup NVGPU_CIC_NONSTALL_OP_DEFINES
 *
 * Defines of all operations that are required to be executed on non stall
 * workqueue.
 */
/**
 * @ingroup NVGPU_CIC_NONSTALL_OP_DEFINES
 * Wakeup semaphore operation.
 */
#define NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE		BIT32(0)
/**
 * @ingroup NVGPU_CIC_NONSTALL_OP_DEFINES
 * Post events operation.
 */
#define NVGPU_CIC_NONSTALL_OPS_POST_EVENTS		BIT32(1)

/**
 * @defgroup NVGPU_CIC_INTR_UNIT_DEFINES
 *
 * Defines of all units intended to be used by any interrupt related
 * HAL that requires unit as parameter.
 */

/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for Bus unit.
 */
#define NVGPU_CIC_INTR_UNIT_BUS		0
/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for PRIV_RING unit.
 */
#define NVGPU_CIC_INTR_UNIT_PRIV_RING	1
/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for FIFO unit.
 */
#define NVGPU_CIC_INTR_UNIT_FIFO	2
/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for LTC unit.
 */
#define NVGPU_CIC_INTR_UNIT_LTC		3
/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for HUB unit.
 */
#define NVGPU_CIC_INTR_UNIT_HUB		4
/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for GR unit.
 */
#define NVGPU_CIC_INTR_UNIT_GR		5
/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for PMU unit.
 */
#define NVGPU_CIC_INTR_UNIT_PMU		6
/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for CE unit.
 */
#define NVGPU_CIC_INTR_UNIT_CE		7
/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for NVLINK unit.
 */
#define NVGPU_CIC_INTR_UNIT_NVLINK	8
/**
 * @ingroup NVGPU_CIC_INTR_UNIT_DEFINES
 * CIC interrupt for FBPA unit.
 */
#define NVGPU_CIC_INTR_UNIT_FBPA	9

/**
 * @defgroup NVGPU_CIC_INTR_ENABLE_DEFINES
 *
 * Defines for CIC unit interrupt enabling/disabling.
 */

/**
 * @ingroup NVGPU_CIC_INTR_ENABLE_DEFINES
 * Value to be passed to mc.intr_*_unit_config to enable the interrupt.
 */
#define NVGPU_CIC_INTR_ENABLE		true

/**
 * @ingroup NVGPU_CIC_INTR_ENABLE_DEFINES
 * Value to be passed to mc.intr_*_unit_config to disable the interrupt.
 */
#define NVGPU_CIC_INTR_DISABLE		false

/*
 * Requires a string literal for the format - notice the string
 * concatination.
 */
#define cic_dbg(g, fmt, args...)					\
	nvgpu_log((g), gpu_dbg_cic, "CIC | " fmt, ##args)

/**
 * @brief Initialize the CIC unit's data structures
 *
 * @param g [in]	- The GPU driver struct.
 *
 * - Check if CIC unit is already initialized by checking its
 *   reference in struct gk20a.
 * - If not initialized, allocate memory for CIC's private data
 *   structure.
 * - Initialize the members of this private structure.
 * - Store a reference pointer to the CIC struct in struct gk20a.
 *
 * @return 0 if Initialization had already happened or was
 *           successful in this call.
 *	  < 0 if any steps in initialization fail.
 *
 * @retval -ENOMEM if sufficient memory is not available for CIC
 *            struct.
 *
 */
int nvgpu_cic_mon_setup(struct gk20a *g);

int nvgpu_cic_mon_init_lut(struct gk20a *g);

/**
 * @brief De-initialize the CIC unit's data structures
 *
 * @param g [in]	- The GPU driver struct.
 *
 * - Check if CIC unit is already deinitialized by checking its
 *   reference in struct gk20a.
 * - If not deinitialized, set the LUT pointer to NULL and set the
 *   num_hw_modules to 0.
 * - Free the memory allocated for CIC's private data structure.
 * - Invalidate reference pointer to the CIC struct in struct gk20a.
 *
 * @return 0 if Deinitialization had already happened or was
 *           successful in this call.
 *
 * @retval None.
 */
int nvgpu_cic_mon_remove(struct gk20a *g);
int nvgpu_cic_mon_deinit_lut(struct gk20a *g);
int nvgpu_cic_mon_deinit(struct gk20a *g);

/**
 * @brief Check if the input HW unit ID is valid CIC HW unit.
 *
 * @param g [in]	      - The GPU driver struct.
 * @param hw_unit_id [in]     - HW unit ID to be verified
 *
 * - Check if the CIC unit is initialized so that the LUT is
 *   available to verify the hw_unit_id.
 * - LUT is an array of nvgpu_err_hw_module struct which contains the
 *   hw_unit_id for a specific unit.
 * - The hw_unit_id starts from 0 and ends at
 *   (g->cic->num_hw_modules -1) and hence effectively can serve as
 *   index into the LUT array.
 *
 * @return 0 if input hw_unit_id is valid,
 *       < 0 if input hw_unit_id is invalid
 * @retval -EINVAL if CIC is not initialized and
 *                 if input hw_unit_id is invalid.
 */
int nvgpu_cic_mon_bound_check_hw_unit_id(struct gk20a *g, u32 hw_unit_id);

/**
 * @brief Check if the input error ID is valid in CIC domain.
 *
 * @param g [in]	      - The GPU driver struct.
 * @param hw_unit_id [in]     - HW unit ID corresponding to err_id
 * @param err_id [in]         - Error ID to be verified
 *
 * - Check if the CIC unit is initialized so that the LUT is
 *   available to verify the hw_unit_id.
 * - LUT is an array of nvgpu_err_hw_module struct which contains the
 *   hw_unit_id for a specific unit and also the number of errors
 *   reported by the unit.
 * - The hw_unit_id starts from 0 and ends at
 *   (g->cic->num_hw_modules -1) and hence effectively can serve as
 *   index into the LUT array.
 * - Before using the input hw_unit_id to index into LUT, verify that
 *   the hw_unit_id is valid.
 * - Index using hw_unit_id and derive the num_errs from LUT for the
 *   given HW unit
 * - Check if the input err_id lies between 0 and (num_errs-1).
 *
 * @return 0 if input err_id is valid, < 0 if input err_id is invalid
 * @retval -EINVAL if CIC is not initialized and
 *                 if input hw_unit_id or err_id is invalid.
 */
int nvgpu_cic_mon_bound_check_err_id(struct gk20a *g, u32 hw_unit_id,
						u32 err_id);

/**
 * @brief Get the LUT data for input HW unit ID and error ID
 *
 * @param g [in]	      - The GPU driver struct.
 * @param hw_unit_id [in]     - HW unit ID corresponding to err_id
 * @param err_id [in]         - Error ID whose LUT data is required.
 * @param err_desc [out]      - Pointer to store LUT data into.
 *
 * - LUT is an array of nvgpu_err_hw_module struct which contains the
 *   all the static data for each HW unit reporting error to CIC.
 * - nvgpu_err_hw_module struct is inturn an array of struct
 *   nvgpu_err_desc which stores static data per error ID.
 * - Use the nvgpu_cic_mon_bound_check_err_id() API to
 *     - Check if the CIC unit is initialized so that the LUT is
 *       available to read the static data for input err_id.
 *     - Check if input HW unit ID and error ID are valid.
 * - The hw_unit_id starts from 0 and ends at
 *   (g->cic->num_hw_modules -1) and hence effectively can serve as
 *   index into the LUT array.
 * - The err_id starts from 0 and ends at
 *   [lut[hw_unit_id].num_err) - 1], and hence effectively can serve
 *   as index into array of errs[].
 * - Index using hw_unit_id and err_id and store the LUT data into
 *
 * @return 0 if err_desc was successfully filled with LUT data,
 *       < 0 otherwise.
 * @retval -EINVAL if CIC is not initialized and
 *                 if input hw_unit_id or err_id is invalid.
 */
int nvgpu_cic_mon_get_err_desc(struct gk20a *g, u32 hw_unit_id,
		u32 err_id, struct nvgpu_err_desc **err_desc);

/**
 * @brief GPU HW errors are reported to Safety_Services via SDL unit.
 *        This function provides an interface between error reporting functions
 *        used by sub-units in nvgpu-rm and SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 * @param err_id [in]		- Error ID.
 *
 *  - Reports the errors to Safety_Services.
 *
 * @return 0 in case of success, <0 in case of failure.
 */
int nvgpu_cic_mon_report_err_safety_services(struct gk20a *g,
		u32 err_id);

/**
 * @brief Get the number of HW modules supported by CIC.
 *
 * @param g [in]	      - The GPU driver struct.
 *
 * - Check if the CIC unit is initialized so that num_hw_modules is
 *   initialized.
 * - Return the num_hw_modules variable stored in CIC's private
 *   struct.
 *
 * @return 0 or >0 value of num_hw_modules if successful;
 *       < 0 otherwise.
 * @retval -EINVAL if CIC is not initialized.
 */
int nvgpu_cic_mon_get_num_hw_modules(struct gk20a *g);

/**
 * @brief Top half of stall interrupt ISR.
 *
 * @param g [in]	The GPU driver struct.
 *
 * This function is invoked by stall interrupt ISR to check if there are
 * any pending stall interrupts. The function will return the action to
 * be taken based on stall interrupt, gpu and quiesce status.
 *
 * @retval NVGPU_CIC_INTR_HANDLE if stall interrupts are pending.
 * @retval NVGPU_CIC_INTR_UNMASK if GPU is powered off.
 * @retval NVGPU_CIC_INTR_NONE if none of the stall interrupts are pending.
 * @retval NVGPU_CIC_INTR_QUIESCE_PENDING if quiesce is pending.
 */
u32 nvgpu_cic_mon_intr_stall_isr(struct gk20a *g);

/**
 * @brief Bottom half of stall interrupt ISR.
 *
 * @param g [in]	The GPU driver struct.
 *
 * This function is called to take action based on pending stall interrupts.
 * The unit ISR functions are invoked based on triggered stall interrupts.
 */
void nvgpu_cic_mon_intr_stall_handle(struct gk20a *g);

#ifdef CONFIG_NVGPU_NONSTALL_INTR
/**
 * @brief Top half of nonstall interrupt ISR.
 *
 * @param g [in]	The GPU driver struct.
 *
 * This function is invoked by nonstall interrupt ISR to check if there are
 * any pending nonstall interrupts. The function will return the action to
 * be taken based on nonstall interrupt, gpu and quiesce status.
 *
 * @retval NVGPU_CIC_INTR_HANDLE if nonstall interrupts are pending.
 * @retval NVGPU_CIC_INTR_UNMASK if GPU is powered off.
 * @retval NVGPU_CIC_INTR_NONE if none of the nonstall interrupts are pending.
 * @retval NVGPU_CIC_INTR_QUIESCE_PENDING if quiesce is pending.
 */
u32 nvgpu_cic_mon_intr_nonstall_isr(struct gk20a *g);

/**
 * @brief Bottom half of nonstall interrupt ISR.
 *
 * @param g [in]	The GPU driver struct.
 *
 * This function is called to take action based on pending nonstall interrupts.
 * Based on triggered nonstall interrupts, this function will invoke
 * nonstall operations.
 */
void nvgpu_cic_mon_intr_nonstall_handle(struct gk20a *g);
#endif

/**
 * @brief Clear the GPU device interrupts at master level.
 *
 * @param g [in]	The GPU driver struct.
 *
 * This function is invoked before powering on, powering off or finishing
 * SW quiesce of nvgpu driver.
 *
 * Steps:
 * - Acquire the spinlock g->mc.intr_lock.
 * - Write U32_MAX to the stalling interrupts enable clear register.
 *   mc_intr_en_clear_r are write only registers which clear
 *   the corresponding bit in INTR_EN whenever a 1 is written
 *   to it.
 * - Set g->mc.intr_mask_restore[NVGPU_CIC_INTR_STALLING] and
 *   g->mc.intr_mask_restore[NVGPU_CIC_INTR_NONSTALLING] to 0.
 * - Write U32_MAX to the non-stalling interrupts enable clear register.
 * - Release the spinlock g->mc.intr_lock.
 */
void nvgpu_cic_mon_intr_mask(struct gk20a *g);

/**
 * @brief Enable/Disable the stalling interrupts for given GPU unit at the
 *        master level.
 *
 * @param g [in]	The GPU driver struct.
 * @param unit [in]	Value designating the GPU HW unit/engine
 *                      controlled by MC. Supported values are:
 *			  - #NVGPU_CIC_INTR_UNIT_BUS
 *			  - #NVGPU_CIC_INTR_UNIT_PRIV_RING
 *			  - #NVGPU_CIC_INTR_UNIT_FIFO
 *			  - #NVGPU_CIC_INTR_UNIT_LTC
 *			  - #NVGPU_CIC_INTR_UNIT_HUB
 *			  - #NVGPU_CIC_INTR_UNIT_GR
 *			  - #NVGPU_CIC_INTR_UNIT_PMU
 *			  - #NVGPU_CIC_INTR_UNIT_CE
 *			  - #NVGPU_CIC_INTR_UNIT_NVLINK
 *			  - #NVGPU_CIC_INTR_UNIT_FBPA
 * @param enable [in]	Boolean control to enable/disable the stalling
 *			interrupt. Supported values are:
 *			  - #NVGPU_CIC_INTR_ENABLE
 *			  - #NVGPU_CIC_INTR_DISABLE
 *
 * During a unit's init routine, this function is invoked to enable the
 * unit's stall interrupts.
 *
 * Steps:
 * - Acquire the spinlock g->mc.intr_lock.
 * - Get the interrupt bitmask for \a unit.
 * - If interrupt is to be enabled
 *   - Set interrupt bitmask in
 *     #intr_mask_restore[#NVGPU_CIC_INTR_STALLING].
 *   - Write the interrupt bitmask to the register
 *     mc_intr_en_set_r(#NVGPU_CIC_INTR_STALLING).
 * - Else
 *   - Clear interrupt bitmask in
 *     #intr_mask_restore[#NVGPU_CIC_INTR_STALLING].
 *   - Write the interrupt bitmask to the register
 *     mc_intr_en_clear_r(#NVGPU_CIC_INTR_STALLING).
 * - Release the spinlock g->mc.intr_lock.
 */
void nvgpu_cic_mon_intr_stall_unit_config(struct gk20a *g, u32 unit, bool enable);

#ifdef CONFIG_NVGPU_NONSTALL_INTR
/**
 * @brief Enable/Disable the non-stalling interrupts for given GPU unit at the
 *        master level.
 *
 * @param g [in]	The GPU driver struct.
 * @param unit [in]	Value designating the GPU HW unit/engine
 *                      controlled by MC. Supported values are:
 *			  - #NVGPU_CIC_INTR_UNIT_BUS
 *			  - #NVGPU_CIC_INTR_UNIT_PRIV_RING
 *			  - #NVGPU_CIC_INTR_UNIT_FIFO
 *			  - #NVGPU_CIC_INTR_UNIT_LTC
 *			  - #NVGPU_CIC_INTR_UNIT_HUB
 *			  - #NVGPU_CIC_INTR_UNIT_GR
 *			  - #NVGPU_CIC_INTR_UNIT_PMU
 *			  - #NVGPU_CIC_INTR_UNIT_CE
 *			  - #NVGPU_CIC_INTR_UNIT_NVLINK
 *			  - #NVGPU_CIC_INTR_UNIT_FBPA
 * @param enable [in]	Boolean control to enable/disable the stalling
 *			interrupt. Supported values are:
 *			  - #NVGPU_CIC_INTR_ENABLE
 *			  - #NVGPU_CIC_INTR_DISABLE
 *
 * During a unit's init routine, this function is invoked to enable the
 * unit's nostall interrupts.
 *
 * Steps:
 * - Acquire the spinlock g->mc.intr_lock.
 * - Get the interrupt bitmask for \a unit.
 * - If interrupt is to be enabled
 *   - Set interrupt bitmask in
 *     #intr_mask_restore[#NVGPU_CIC_INTR_NONSTALLING].
 *   - Write the interrupt bitmask to the register
 *     mc_intr_en_set_r(#NVGPU_CIC_INTR_NONSTALLING).
 * - Else
 *   - Clear interrupt bitmask in
 *     #intr_mask_restore[#NVGPU_CIC_INTR_NONSTALLING].
 *   - Write the interrupt bitmask to the register
 *     mc_intr_en_clear_r(#NVGPU_CIC_INTR_NONSTALLING).
 * - Release the spinlock g->mc.intr_lock.
 */
void nvgpu_cic_mon_intr_nonstall_unit_config(struct gk20a *g, u32 unit, bool enable);
#endif

/**
 * @brief Disable/Pause the stalling interrupts.
 *
 * @param g [in]	The GPU driver struct.
 *
 * This function is invoked to disable the stalling interrupts before
 * the ISR is executed.
 *
 * Steps:
 * - Acquire the spinlock g->mc.intr_lock.
 * - Write U32_MAX to the stalling interrupts enable clear register
 *   (mc_intr_en_clear_r(#NVGPU_CIC_INTR_STALLING)).
 * - Release the spinlock g->mc.intr_lock.
 */
void nvgpu_cic_mon_intr_stall_pause(struct gk20a *g);

/**
 * @brief Enable/Resume the stalling interrupts.
 *
 * @param g [in]	The GPU driver struct.
 *
 * This function is invoked to enable the stalling interrupts after
 * the ISR is executed.
 *
 * Steps:
 * - Acquire the spinlock g->mc.intr_lock.
 * - Enable stalling interrupts as configured during #intr_stall_unit_config
 * - Write #intr_mask_restore[#NVGPU_CIC_INTR_STALLING] to the stalling
 *   interrupts enable set register (mc_intr_en_set_r(#NVGPU_CIC_INTR_STALLING)).
 * - Release the spinlock g->mc.intr_lock.
 */
void nvgpu_cic_mon_intr_stall_resume(struct gk20a *g);

#ifdef CONFIG_NVGPU_NONSTALL_INTR
/**
 * @brief Disable/Pause the non-stalling interrupts.
 *
 * @param g [in]	The GPU driver struct.
 *
 * This function is invoked to disable the non-stalling interrupts
 * before the ISR is executed.
 *
 * Steps:
 * - Acquire the spinlock g->mc.intr_lock.
 * - Write U32_MAX to the non-stalling interrupts enable clear register
 *   (mc_intr_en_clear_r(#NVGPU_CIC_INTR_NONSTALLING)).
 * - Release the spinlock g->mc.intr_lock.
 */
void nvgpu_cic_mon_intr_nonstall_pause(struct gk20a *g);

/**
 * @brief Enable/Resume the non-stalling interrupts.
 *
 * @param g [in]	The GPU driver struct.
 *
 * This function is invoked to enable the non-stalling interrupts after
 * the ISR is executed.
 *
 * Steps:
 * - Acquire the spinlock g->mc.intr_lock.
 * - Enable non-stalling interrupts as configured during
 *   #intr_nonstall_unit_config.
 * - Write #intr_mask_restore[#NVGPU_CIC_INTR_NONSTALLING]
 *   to the non-stalling interrupts enable set register
 *   (mc_intr_en_set_r(#NVGPU_CIC_INTR_NONSTALLING)).
 * - Release the spinlock g->mc.intr_lock.
 */
void nvgpu_cic_mon_intr_nonstall_resume(struct gk20a *g);
#endif

void nvgpu_cic_mon_intr_enable(struct gk20a *g);

#endif /* NVGPU_CIC_MON_H */
