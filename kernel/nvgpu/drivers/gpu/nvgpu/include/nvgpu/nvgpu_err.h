/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_NVGPU_ERR_H
#define NVGPU_NVGPU_ERR_H

/**
 * @file
 *
 * Define indices for HW units and errors. Define structures used to carry error
 * information. Declare prototype for APIs that are used to report GPU HW errors
 * to the Safety_Services framework.
 */

#include <nvgpu/types.h>

struct gk20a;
struct mmu_fault_info;

/**
 * @defgroup INDICES_FOR_GPU_HW_UNITS
 * Macros used to assign unique index to GPU HW units.
 * @{
 */
#define NVGPU_ERR_MODULE_HOST		(0U)
#define NVGPU_ERR_MODULE_SM		(1U)
#define NVGPU_ERR_MODULE_FECS		(2U)
#define NVGPU_ERR_MODULE_GPCCS		(3U)
#define NVGPU_ERR_MODULE_MMU		(4U)
#define NVGPU_ERR_MODULE_GCC		(5U)
#define NVGPU_ERR_MODULE_PMU		(6U)
#define NVGPU_ERR_MODULE_PGRAPH		(7U)
#define NVGPU_ERR_MODULE_LTC		(8U)
#define NVGPU_ERR_MODULE_HUBMMU		(9U)
#define NVGPU_ERR_MODULE_PRI		(10U)
#define NVGPU_ERR_MODULE_CE		(11U)
#define NVGPU_ERR_MODULE_GSP_ACR	(12U)
#define NVGPU_ERR_MODULE_GSP_SCHED	(13U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_HOST
 * Macros used to assign unique index to errors reported from the HOST unit.
 * @{
 */
#define GPU_HOST_PFIFO_BIND_ERROR		(0U)
#define GPU_HOST_PFIFO_SCHED_ERROR		(1U)
#define GPU_HOST_PFIFO_CHSW_ERROR		(2U)
#define GPU_HOST_PFIFO_MEMOP_TIMEOUT_ERROR	(3U)
#define GPU_HOST_PFIFO_LB_ERROR			(4U)
#define GPU_HOST_PBUS_SQUASH_ERROR		(5U)
#define GPU_HOST_PBUS_FECS_ERROR		(6U)
#define GPU_HOST_PBUS_TIMEOUT_ERROR		(7U)
#define GPU_HOST_PBDMA_TIMEOUT_ERROR		(8U)
#define GPU_HOST_PBDMA_EXTRA_ERROR		(9U)
#define GPU_HOST_PBDMA_GPFIFO_PB_ERROR		(10U)
#define GPU_HOST_PBDMA_METHOD_ERROR		(11U)
#define GPU_HOST_PBDMA_SIGNATURE_ERROR		(12U)
#define GPU_HOST_PBDMA_HCE_ERROR		(13U)
#define GPU_HOST_PFIFO_CTXSW_TIMEOUT_ERROR	(14U)
#define GPU_HOST_PFIFO_FB_FLUSH_TIMEOUT_ERROR	(15U)
#define GPU_HOST_INVALID_ERROR			(16U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_SM
 * Macros used to assign unique index to errors reported from the SM unit.
 * @{
 */
#define GPU_SM_L1_TAG_ECC_CORRECTED			(0x0U)
#define GPU_SM_L1_TAG_ECC_UNCORRECTED			(0x1U)
#define GPU_SM_CBU_ECC_UNCORRECTED			(0x2U)
#define GPU_SM_LRF_ECC_UNCORRECTED			(0x3U)
#define GPU_SM_L1_DATA_ECC_UNCORRECTED			(0x4U)
#define GPU_SM_ICACHE_L0_DATA_ECC_UNCORRECTED		(0x5U)
#define GPU_SM_ICACHE_L1_DATA_ECC_UNCORRECTED		(0x6U)
#define GPU_SM_ICACHE_L0_PREDECODE_ECC_UNCORRECTED	(0x7U)
#define GPU_SM_L1_TAG_MISS_FIFO_ECC_UNCORRECTED		(0x8U)
#define GPU_SM_L1_TAG_S2R_PIXPRF_ECC_UNCORRECTED	(0x9U)
#define GPU_SM_MACHINE_CHECK_ERROR			(0xAU)
#define GPU_SM_RAMS_URF_ECC_UNCORRECTED			(0xBU)

/**
 * @}
 */


/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_FECS
 * Macros used to assign unique index to errors reported from the FECS unit.
 * @{
 */
#define GPU_FECS_FALCON_IMEM_ECC_CORRECTED	(0U)
#define GPU_FECS_FALCON_IMEM_ECC_UNCORRECTED	(1U)
#define GPU_FECS_FALCON_DMEM_ECC_UNCORRECTED	(2U)
#define GPU_FECS_CTXSW_WATCHDOG_TIMEOUT		(3U)
#define GPU_FECS_CTXSW_CRC_MISMATCH		(4U)
#define GPU_FECS_FAULT_DURING_CTXSW		(5U)
#define GPU_FECS_CTXSW_INIT_ERROR		(6U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_GPCCS
 * Macros used to assign unique index to errors reported from the GPCCS unit.
 * @{
 */
#define GPU_GPCCS_FALCON_IMEM_ECC_CORRECTED	(0U)
#define GPU_GPCCS_FALCON_IMEM_ECC_UNCORRECTED	(1U)
#define GPU_GPCCS_FALCON_DMEM_ECC_UNCORRECTED	(2U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_MMU
 * Macros used to assign unique index to errors reported from the MMU unit.
 * @{
 */
#define GPU_MMU_L1TLB_SA_DATA_ECC_UNCORRECTED	(0U)
#define GPU_MMU_L1TLB_FA_DATA_ECC_UNCORRECTED	(1U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_GCC
 * Macros used to assign unique index to errors reported from the GCC unit.
 * @{
 */
#define GPU_GCC_L15_ECC_UNCORRECTED		(0U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_PMU
 * Macros used to assign unique index to errors reported from the PMU unit.
 * @{
 */
#define GPU_PMU_NVRISCV_BROM_FAILURE		(0U)
#define GPU_PMU_ACCESS_TIMEOUT_UNCORRECTED	(1U)
#define GPU_PMU_MPU_ECC_UNCORRECTED		(2U)
#define GPU_PMU_ILLEGAL_ACCESS_UNCORRECTED	(3U)
#define GPU_PMU_IMEM_ECC_UNCORRECTED		(4U)
#define GPU_PMU_DCLS_UNCORRECTED		(5U)
#define GPU_PMU_DMEM_ECC_UNCORRECTED		(6U)
#define GPU_PMU_WDT_UNCORRECTED			(7U)
#define GPU_PMU_REG_ECC_UNCORRECTED		(8U)
#define GPU_PMU_BAR0_ERROR_TIMEOUT		(9U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_GSP_ACR
 * Macros used to assign unique index to errors reported from the GSP ACR unit.
 * @{
 */
#define GPU_GSP_ACR_NVRISCV_BROM_FAILURE		(0U)
#define GPU_GSP_ACR_EMEM_ECC_UNCORRECTED		(1U)
#define GPU_GSP_ACR_REG_ACCESS_TIMEOUT_UNCORRECTED	(2U)
#define GPU_GSP_ACR_ILLEGAL_ACCESS_UNCORRECTED		(3U)
#define GPU_GSP_ACR_IMEM_ECC_UNCORRECTED		(4U)
#define GPU_GSP_ACR_DCLS_UNCORRECTED			(5U)
#define GPU_GSP_ACR_DMEM_ECC_UNCORRECTED		(6U)
#define GPU_GSP_ACR_WDT_UNCORRECTED			(7U)
#define GPU_GSP_ACR_REG_ECC_UNCORRECTED			(8U)
#define GPU_GSP_ACR_FECS_PKC_LSSIG_FAILURE		(9U)
#define GPU_GSP_ACR_GPCCS_PKC_LSSIG_FAILURE		(10U)
#define GPU_GSP_ACR_LSPMU_PKC_LSSIG_FAILURE		(11U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_GSP_SCHED
 * Macros used to assign unique index to errors reported from the GSP SCHED unit.
 * @{
 */
#define GPU_GSP_SCHED_NVRISCV_BROM_FAILURE		(0U)
#define GPU_GSP_SCHED_EMEM_ECC_UNCORRECTED		(1U)
#define GPU_GSP_SCHED_REG_ACCESS_TIMEOUT_UNCORRECTED	(2U)
#define GPU_GSP_SCHED_ILLEGAL_ACCESS_UNCORRECTED	(3U)
#define GPU_GSP_SCHED_IMEM_ECC_UNCORRECTED		(4U)
#define GPU_GSP_SCHED_DCLS_UNCORRECTED			(5U)
#define GPU_GSP_SCHED_DMEM_ECC_UNCORRECTED		(6U)
#define GPU_GSP_SCHED_WDT_UNCORRECTED			(7U)
#define GPU_GSP_SCHED_REG_ECC_UNCORRECTED		(8U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_PGRAPH
 * Macros used to assign unique index to errors reported from the PGRAPH unit.
 * @{
 */
#define GPU_PGRAPH_FE_EXCEPTION			(0U)
#define GPU_PGRAPH_MEMFMT_EXCEPTION		(1U)
#define GPU_PGRAPH_PD_EXCEPTION			(2U)
#define GPU_PGRAPH_SCC_EXCEPTION		(3U)
#define GPU_PGRAPH_DS_EXCEPTION			(4U)
#define GPU_PGRAPH_SSYNC_EXCEPTION		(5U)
#define GPU_PGRAPH_MME_EXCEPTION		(6U)
#define GPU_PGRAPH_SKED_EXCEPTION		(7U)
#define GPU_PGRAPH_BE_CROP_EXCEPTION		(8U)
#define GPU_PGRAPH_BE_ZROP_EXCEPTION		(9U)
#define GPU_PGRAPH_MPC_EXCEPTION		(10U)
#define GPU_PGRAPH_ILLEGAL_NOTIFY_ERROR		(11U)
#define GPU_PGRAPH_ILLEGAL_METHOD_ERROR		(12U)
#define GPU_PGRAPH_ILLEGAL_CLASS_ERROR		(13U)
#define GPU_PGRAPH_CLASS_ERROR			(14U)
#define GPU_PGRAPH_GPC_GFX_PROP_EXCEPTION	(15U)
#define GPU_PGRAPH_GPC_GFX_ZCULL_EXCEPTION	(16U)
#define GPU_PGRAPH_GPC_GFX_SETUP_EXCEPTION	(17U)
#define GPU_PGRAPH_GPC_GFX_PES_EXCEPTION	(18U)
#define GPU_PGRAPH_GPC_GFX_TPC_PE_EXCEPTION	(19U)
#define GPU_PGRAPH_MME_FE1_EXCEPTION		(20U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_LTC
 * Macros used to assign unique index to errors reported from the LTC unit.
 * @{
 */
#define GPU_LTC_CACHE_DSTG_ECC_CORRECTED	(0U)
#define GPU_LTC_CACHE_DSTG_ECC_UNCORRECTED	(1U)
#define GPU_LTC_CACHE_TSTG_ECC_UNCORRECTED	(2U)
#define GPU_LTC_CACHE_RSTG_CBC_ECC_UNCORRECTED	(3U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_HUBMMU
 * Macros used to assign unique index to errors reported from the HUBMMU unit.
 * @{
 */
#define GPU_HUBMMU_L2TLB_SA_DATA_ECC_UNCORRECTED			(0U)
#define GPU_HUBMMU_TLB_SA_DATA_ECC_UNCORRECTED				(1U)
#define GPU_HUBMMU_PTE_DATA_ECC_UNCORRECTED				(2U)
#define GPU_HUBMMU_PDE0_DATA_ECC_UNCORRECTED				(3U)
#define GPU_HUBMMU_PAGE_FAULT_OTHER_FAULT_NOTIFY_ERROR			(4U)
#define GPU_HUBMMU_PAGE_FAULT_NONREPLAYABLE_FAULT_OVERFLOW_ERROR	(5U)
#define GPU_HUBMMU_PAGE_FAULT_REPLAYABLE_FAULT_OVERFLOW_ERROR		(6U)
#define GPU_HUBMMU_PAGE_FAULT_REPLAYABLE_FAULT_NOTIFY_ERROR		(7U)
#define GPU_HUBMMU_PAGE_FAULT_NONREPLAYABLE_FAULT_NOTIFY_ERROR		(8U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_PRI
 * Macros used to assign unique index to errors reported from the PRI unit.
 * @{
 */
#define GPU_PRI_TIMEOUT_ERROR		(0U)
#define GPU_PRI_ACCESS_VIOLATION	(1U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_CE
 * Macros used to assign unique index to errors reported from the CE unit.
 * @{
 */
#define GPU_CE_LAUNCH_ERROR			(0x0)
#ifdef CONFIG_NVGPU_NON_FUSA
#define GPU_CE_METHOD_BUFFER_FAULT		(0x1)
#define GPU_CE_FBUF_CRC_FAIL			(0x2)
#define GPU_CE_FBUF_MAGIC_CHK_FAIL		(0x3)
#define GPU_CE_INVALID_CONFIG			(0x4)
#endif
/**
 * @}
 */

/**
 * This macro is used to initialize the members of nvgpu_hw_err_inject_info
 * struct.
 */
#define NVGPU_ECC_ERR(err_name, inject_fn, addr, val)	\
{							\
		.name = (err_name),			\
		.inject_hw_fault = (inject_fn),		\
		.get_reg_addr = (addr),			\
		.get_reg_val = (val)			\
}

/**
 * This structure carries the information required for HW based error injection
 * for a given error.
 */
struct nvgpu_hw_err_inject_info {
	/** String representation of error. */
	const char *name;
	void (*inject_hw_fault)(struct gk20a *g,
			struct nvgpu_hw_err_inject_info *err, u32 err_info);
	u32 (*get_reg_addr)(void);
	u32 (*get_reg_val)(u32 val);
};

/**
 * This structure contains a pointer to an array containing HW based error
 * injection information and the size of that array.
 */
struct nvgpu_hw_err_inject_info_desc {
	struct nvgpu_hw_err_inject_info *info_ptr;
	u32 info_size;
};

#ifdef CONFIG_NVGPU_INTR_DEBUG

/**
 * This structure is used to store SM machine check related information.
 */
struct gr_sm_mcerr_info {
	/** PC which triggered the machine check error. */
	u64 hww_warp_esr_pc;

	/** Error status register. */
	u32 hww_warp_esr_status;

	/** GR engine context of the faulted channel. */
	u32 curr_ctx;

	/** Channel to which the context belongs. */
	u32 chid;

	/** TSG to which the channel is bound. */
	u32 tsgid;

	/** IDs of TPC, GPC, and SM. */
	u32 tpc, gpc, sm;
};

/**
 * This structure is used to store CTXSW error related information.
 */
struct ctxsw_err_info {

	/** GR engine context of the faulted channel. */
	u32 curr_ctx;

	/** Context-switch status register-0. */
	u32 ctxsw_status0;

	/** Context-switch status register-1. */
	u32 ctxsw_status1;

	/** Channel to which the context belongs. */
	u32 chid;

	/**
	 * In case of any fault during context-switch transaction,
	 * context-switch error interrupt is set and the FECS firmware
	 * writes error code into FECS mailbox 6. This exception
	 * is handled at GR unit.
	 */
	u32 mailbox_value;
};

/**
 * This structure is used to store GR exception related information.
 */
struct gr_exception_info {
	/** GR engine context of the faulted channel. */
	u32 curr_ctx;

	/** Channel bound to the context. */
	u32 chid;

	/** TSG to which the channel is bound. */
	u32 tsgid;

	/** GR interrupt status. */
	u32 status;
};

/**
 * This structure is used to store GR error related information.
 */
struct gr_err_info {
	/** SM machine check error information. */
	struct gr_sm_mcerr_info *sm_mcerr_info;

	/** GR exception related information. */
	struct gr_exception_info *exception_info;
};

/**
 * @brief This function provides an interface to report errors from HOST
 *        (PFIFO/PBDMA/PBUS) unit to SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 *                                - The function does not perform validation of
 *                                  g parameter.
 * @param hw_unit [in]		- Index of HW unit.
 *				  - The function validates that hw_unit ==
 *				    \link NVGPU_ERR_MODULE_HOST \endlink.
 * @param inst [in]		- Instance ID.
 *				  - In case of multiple instances of the same HW
 *				    unit (e.g., there are multiple instances of
 *				    PBDMA), it is used to identify the instance
 *				    that encountered a fault.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param err_id [in]		- Error index.
 *				  - The function validates that, this paramter
 *				    has a value within valid range.
 *				  - Min: \link GPU_HOST_PFIFO_BIND_ERROR \endlink
 *				  - Max: \link GPU_HOST_PFIFO_FB_FLUSH_TIMEOUT_ERROR \endlink
 * @param intr_info [in]	- Content of interrupt status register.
 *				  - The function does not perform any validation
 *				    on this parameter.
 *
 * - Check nvgpu_os_rmos.is_sdl_supported equals true (pointer to nvgpu_os_rmos
 *   is obtained using \ref nvgpu_os_rmos_from_gk20a()
 *   "nvgpu_os_rmos_from_gk20a(g)"), return on failure.
 * - Perform validation of input paramters, see paramter section for detailed
 *   validation criteria. In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Get the current time using api clock_gettime(CLOCK_MONOTONIC, &ts).
 *   In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Declare and initialize an error message packet err_pkt of type
 *   nvgpu_err_msg, using \ref nvgpu_init_host_err_msg()
 *   "nvgpu_init_host_err_msg(&err_pkt)".
 * - Get error description err_desc of type nvgpu_err_desc from internal
 *   look-up table \ref nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref
 *   nvgpu_sdl_rmos.err_lut "err_lut" using hw_unit and err_id.
 * - Update the following fields in the error message packet err_pkt.
 *   - nvgpu_err_msg.hw_unit_id = hw_unit
 *   - nvgpu_err_msg.err_id = err_desc->error_id
 *   - nvgpu_err_msg.is_critical = err_desc->is_critical
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.host_info "host_info".\ref gpu_host_error_info.header
 *     "header".\ref gpu_err_header.sub_unit_id "sub_unit_id" = inst
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.host_info "host_info".\ref gpu_host_error_info.header
 *     "header".\ref gpu_err_header.sub_err_type "sub_err_type" = intr_info
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.host_info "host_info".\ref gpu_host_error_info.header
 *     "header".\ref gpu_err_header.timestamp_ns "timestamp_ns" =
 *     \ref nvgpu_timespec2nsec() "nvgpu_timespec2nsec(&ts)"
 *   - nvgpu_err_msg.err_desc = err_desc
 *   - nvgpu_err_msg.err_size = sizeof(gpu_error_info.host_info)
 * - Invoke \ref nvgpu_sdl_report_error_rmos()
 *   "nvgpu_sdl_report_error_rmos(g, &err_pkt, sizeof(err_pkt))" to enqueue
 *   the packet err_pkt into the circular buffer \ref nvgpu_os_rmos.sdl_rmos
 *   "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q "emsg_q". In case
 *   of failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure() "nvgpu_sdl_handle_report_failure(g)"
 *   and return.
 * - The error packet err_pkt will be dequeued from \ref
 *   nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q
 *   "emsg_q" and reported to Safety Service by nvgpu_sdl_worker() thread.
 *
 * @return	None
 */
void nvgpu_report_host_err(struct gk20a *g, u32 hw_unit,
	u32 inst, u32 err_id, u32 intr_info);

/**
 * @brief This function provides an interface to report errors from CE unit
 *        to SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 *                                - The function does not perform validation of
 *                                  g parameter.
 * @param hw_unit [in]		- Index of HW unit.
 *				  - The function validates that hw_unit ==
 *				    \link NVGPU_ERR_MODULE_CE \endlink.
 * @param inst [in]		- Instance ID.
 *				  - In case of multiple instances of the same HW
 *				    unit (e.g., there are multiple instances of
 *				    CE), it is used to identify the instance
 *				    that encountered a fault.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param err_id [in]		- Error index.
 *				  - The function validates that, this paramter
 *				    has a value within valid range.
 *				  - Min: \link GPU_CE_LAUNCH_ERROR \endlink
 *				  - Max: \link GPU_CE_METHOD_BUFFER_FAULT \endlink
 * @param intr_info [in]	- Content of interrupt status register.
 *				  - The function does not perform any validation
 *				    on this parameter.
 *
 * - Check nvgpu_os_rmos.is_sdl_supported equals true (pointer to nvgpu_os_rmos
 *   is obtained using \ref nvgpu_os_rmos_from_gk20a()
 *   "nvgpu_os_rmos_from_gk20a(g)"), return on failure.
 * - Perform validation of input paramters, see paramter section for detailed
 *   validation criteria. In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Get the current time using api clock_gettime(CLOCK_MONOTONIC, &ts).
 *   In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Declare and initialize an error message packet err_pkt of type
 *   nvgpu_err_msg, using \ref nvgpu_init_ce_err_msg()
 *   "nvgpu_init_ce_err_msg(&err_pkt)".
 * - Get error description err_desc of type nvgpu_err_desc from internal
 *   look-up table \ref nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref
 *   nvgpu_sdl_rmos.err_lut "err_lut" using hw_unit and err_id.
 * - Update the following fields in the error message packet err_pkt.
 *   - nvgpu_err_msg.hw_unit_id = hw_unit
 *   - nvgpu_err_msg.err_id = err_desc->error_id
 *   - nvgpu_err_msg.is_critical = err_desc->is_critical
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ce_info "ce_info".\ref gpu_ce_error_info.header
 *     "header".\ref gpu_err_header.sub_unit_id "sub_unit_id" = 0
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ce_info "ce_info".\ref gpu_ce_error_info.header
 *     "header".\ref gpu_err_header.sub_err_type "sub_err_type" = intr_info
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ce_info "ce_info".\ref gpu_ce_error_info.header
 *     "header".\ref gpu_err_header.timestamp_ns "timestamp_ns" =
 *     \ref nvgpu_timespec2nsec() "nvgpu_timespec2nsec(&ts)"
 *   - nvgpu_err_msg.err_desc = err_desc
 *   - nvgpu_err_msg.err_size = sizeof(gpu_error_info.ce_info)
 * - Invoke \ref nvgpu_sdl_report_error_rmos()
 *   "nvgpu_sdl_report_error_rmos(g, &err_pkt, sizeof(err_pkt))" to enqueue
 *   the packet err_pkt into the circular buffer \ref nvgpu_os_rmos.sdl_rmos
 *   "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q "emsg_q". In case
 *   of failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure() "nvgpu_sdl_handle_report_failure(g)"
 *   and return.
 * - The error packet err_pkt will be dequeued from \ref
 *   nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q
 *   "emsg_q" and reported to Safety Service by nvgpu_sdl_worker() thread.
 *
 * @return	None
 */
void nvgpu_report_ce_err(struct gk20a *g, u32 hw_unit,
	u32 inst, u32 err_id, u32 intr_info);

/**
 * @brief This function provides an interface to report ECC erros to SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 *                                - The function does not perform validation of
 *                                  g parameter.
 * @param hw_unit [in]		- Index of HW unit.
 *				  - The function validates that this parameter
 *				    has anyone of the following values:
 *				  - \link NVGPU_ERR_MODULE_SM \endlink
 *				  - \link NVGPU_ERR_MODULE_FECS \endlink
 *				  - \link NVGPU_ERR_MODULE_GPCCS \endlink
 *				  - \link NVGPU_ERR_MODULE_MMU \endlink
 *				  - \link NVGPU_ERR_MODULE_GCC \endlink
 *				  - \link NVGPU_ERR_MODULE_PMU \endlink
 *				  - \link NVGPU_ERR_MODULE_LTC \endlink
 *				  - \link NVGPU_ERR_MODULE_HUBMMU \endlink
 * @param inst [in]		- Instance ID.
 *				  - In case of multiple instances of the same HW
 *				    unit (e.g., there are multiple instances of
 *				    SM), it is used to identify the instance
 *				    that encountered a fault.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param err_id [in]		- Error index.
 *				  - The function validates that this parameter
 *				    has a value within valid range.
 *				  - For \link NVGPU_ERR_MODULE_SM \endlink
 *				    - Min: \link GPU_SM_L1_TAG_ECC_CORRECTED \endlink
 *				    - Max: \link GPU_SM_ICACHE_L1_PREDECODE_ECC_UNCORRECTED \endlink
 *				  - For \link NVGPU_ERR_MODULE_FECS \endlink
 *				    - Min: \link GPU_FECS_FALCON_IMEM_ECC_CORRECTED \endlink
 *				    - Max: \link GPU_FECS_CTXSW_INIT_ERROR \endlink
 *				  - \link NVGPU_ERR_MODULE_GPCCS \endlink
 *				    - Min: \link GPU_GPCCS_FALCON_IMEM_ECC_CORRECTED \endlink
 *				    - Max: \link GPU_GPCCS_FALCON_DMEM_ECC_UNCORRECTED \endlink
 *				  - \link NVGPU_ERR_MODULE_MMU \endlink
 *				    - Min: \link GPU_MMU_L1TLB_SA_DATA_ECC_UNCORRECTED \endlink
 *				    - Max: \link GPU_MMU_L1TLB_FA_DATA_ECC_UNCORRECTED \endlink
 *				  - \link NVGPU_ERR_MODULE_GCC \endlink
 *				    - Min: \link GPU_GCC_L15_ECC_UNCORRECTED \endlink
 *				    - Max: \link GPU_GCC_L15_ECC_UNCORRECTED \endlink
 *				  - \link NVGPU_ERR_MODULE_PMU \endlink
 *				    - Min: \link GPU_PMU_FALCON_IMEM_ECC_CORRECTED \endlink
 *				    - Max: \link GPU_PMU_FALCON_DMEM_ECC_UNCORRECTED \endlink
 *				  - \link NVGPU_ERR_MODULE_LTC \endlink
 *				    - Min: \link GPU_LTC_CACHE_DSTG_ECC_CORRECTED \endlink
 *				    - Max: \link GPU_LTC_CACHE_DSTG_BE_ECC_UNCORRECTED \endlink
 *				  - \link NVGPU_ERR_MODULE_HUBMMU \endlink
 *				    - Min: \link GPU_HUBMMU_L2TLB_SA_DATA_ECC_UNCORRECTED \endlink
 *				    - Max: \link GPU_HUBMMU_PDE0_DATA_ECC_UNCORRECTED \endlink
 * @param intr_info [in]	- Content of interrupt status register.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param err_addr [in]		- Error address.
 *				  - This is the location at which correctable or
 *				    uncorrectable error has occurred.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param err_count [in]	- Error count.
 *				  - The function does not perform any validation
 *				    on this parameter.
 *
 * - Check nvgpu_os_rmos.is_sdl_supported equals true (pointer to nvgpu_os_rmos
 *   is obtained using \ref nvgpu_os_rmos_from_gk20a()
 *   "nvgpu_os_rmos_from_gk20a(g)"), return on failure.
 * - Perform validation of input paramters, see paramter section for detailed
 *   validation criteria. In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Get the current time using api clock_gettime(CLOCK_MONOTONIC, &ts).
 *   In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Declare and initialize an error message packet err_pkt of type
 *   nvgpu_err_msg, using \ref nvgpu_init_ecc_err_msg()
 *   "nvgpu_init_ecc_err_msg(&err_pkt)".
 * - Get error description err_desc of type nvgpu_err_desc from internal
 *   look-up table \ref nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref
 *   nvgpu_sdl_rmos.err_lut "err_lut" using hw_unit and err_id.
 * - Update the following fields in the error message packet err_pkt.
 *   - nvgpu_err_msg.hw_unit_id = hw_unit
 *   - nvgpu_err_msg.err_id = err_desc->error_id
 *   - nvgpu_err_msg.is_critical = err_desc->is_critical
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ecc_info "ecc_info".\ref gpu_ecc_error_info.header
 *     "header".\ref gpu_err_header.timestamp_ns "timestamp_ns" =
 *     \ref nvgpu_timespec2nsec() "nvgpu_timespec2nsec(&ts)"
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ecc_info "ecc_info".\ref gpu_ecc_error_info.header
 *     "header".\ref gpu_err_header.sub_unit_id "sub_unit_id" = inst
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ecc_info "ecc_info".\ref gpu_ecc_error_info.header
 *     "header".\ref gpu_err_header.address "address" = err_addr
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ecc_info "ecc_info".\ref gpu_ecc_error_info.err_cnt
 *     "err_cnt" = err_count
 *   - nvgpu_err_msg.err_desc = err_desc
 *   - nvgpu_err_msg.err_size = sizeof(gpu_error_info.ecc_info)
 * - Invoke \ref nvgpu_sdl_report_error_rmos()
 *   "nvgpu_sdl_report_error_rmos(g, &err_pkt, sizeof(err_pkt))" to enqueue
 *   the packet err_pkt into the circular buffer \ref nvgpu_os_rmos.sdl_rmos
 *   "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q "emsg_q". In case
 *   of failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure() "nvgpu_sdl_handle_report_failure(g)"
 *   and return.
 * - The error packet err_pkt will be dequeued from \ref
 *   nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q
 *   "emsg_q" and reported to Safety Service by nvgpu_sdl_worker() thread.
 *
 * @return	None
 */
void nvgpu_report_ecc_err(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, u64 err_addr, u64 err_count);

/**
 * @brief This is a wrapper function to report ECC errors from HUBMMU to SDL.
 *
 * @param g [in]		- The GPU driver struct.
 * @param err_id [in]		- Error index.
 *				  - Min: GPU_HUBMMU_L2TLB_SA_DATA_ECC_CORRECTED
 *				  - Max: GPU_HUBMMU_PDE0_DATA_ECC_UNCORRECTED
 * @param err_addr [in]		- Error address.
 *				  - This is the location at which correctable or
 *				    uncorrectable error has occurred.
 * @param err_count [in]	- Error count.
 *
 * Calls nvgpu_report_ecc_err with hw_unit=NVGPU_ERR_MODULE_HUBMMU and inst=0.
 *
 * @return	None
 */
static inline void nvgpu_report_fb_ecc_err(struct gk20a *g, u32 err_id, u64 err_addr,
		u64 err_count)
{
	nvgpu_report_ecc_err(g, NVGPU_ERR_MODULE_HUBMMU, 0, err_id, err_addr,
			err_count);
}

/**
 * @brief This function provides an interface to report CTXSW erros to SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 *                                - The function does not perform validation of
 *                                  g parameter.
 * @param hw_unit [in]		- Index of HW unit.
 *				  - The function validates that hw_unit ==
 *				    \link NVGPU_ERR_MODULE_FECS \endlink.
 * @param err_id [in]		- Error index.
 *				  - The function validates that, this paramter
 *				    has a value within valid range.
 *				  - Min: \link GPU_FECS_CTXSW_WATCHDOG_TIMEOUT \endlink
 *				  - Max: \link GPU_FECS_CTXSW_INIT_ERROR \endlink
 * @param intr_info [in]	- Content of interrupt status register.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param data [in]		- CTXSW error information.
 *				  - The function does not perform any validation
 *				    on this parameter.
 *
 * - Check nvgpu_os_rmos.is_sdl_supported equals true (pointer to nvgpu_os_rmos
 *   is obtained using \ref nvgpu_os_rmos_from_gk20a()
 *   "nvgpu_os_rmos_from_gk20a(g)"), return on failure.
 * - Perform validation of input paramters, see paramter section for detailed
 *   validation criteria. In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Get the current time using api clock_gettime(CLOCK_MONOTONIC, &ts).
 *   In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Declare and initialize an error message packet err_pkt of type
 *   nvgpu_err_msg, using \ref nvgpu_init_ctxsw_err_msg()
 *   "nvgpu_init_ctxsw_err_msg(&err_pkt)".
 * - Get error description err_desc of type nvgpu_err_desc from internal
 *   look-up table \ref nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref
 *   nvgpu_sdl_rmos.err_lut "err_lut" using hw_unit and err_id.
 * - Typecast the input \a data to the type #ctxsw_err_info.
 * - Update the following fields in the error message packet err_pkt.
 *   - nvgpu_err_msg.hw_unit_id = hw_unit
 *   - nvgpu_err_msg.is_critical = err_desc->is_critical
 *   - nvgpu_err_msg.err_id = err_desc->error_id
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ctxsw_info "ctxsw_info".\ref gpu_ctxsw_error_info.header
 *     "header".\ref gpu_err_header.sub_unit_id "sub_unit_id" = 0
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ctxsw_info "ctxsw_info".\ref gpu_ctxsw_error_info.curr_ctx
 *     "curr_ctx" = data->curr_ctx
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ctxsw_info "ctxsw_info".\ref gpu_ctxsw_error_info.chid
 *     "chid" = data->chid
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ctxsw_info "ctxsw_info".\ref gpu_ctxsw_error_info.ctxsw_status0
 *     "cxtsw_status0" = data->cxtsw_status0
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ctxsw_info "ctxsw_info".\ref gpu_ctxsw_error_info.ctxsw_status1
 *     "cxtsw_status1" = data->cxtsw_status1
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ctxsw_info "ctxsw_info".\ref gpu_ctxsw_error_info.mailbox_value
 *     "mailbox_value" = data->mailbox_value
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ctxsw_info "ctxsw_info".\ref gpu_ctxsw_error_info.header
 *     "header".\ref gpu_err_header.timestamp_ns "timestamp_ns" =
 *     \ref nvgpu_timespec2nsec() "nvgpu_timespec2nsec(&ts)"
 *   - nvgpu_err_msg.err_desc = err_desc
 *   - nvgpu_err_msg.err_size = sizeof(gpu_error_info.ctxsw_info)
 * - Invoke \ref nvgpu_sdl_report_error_rmos()
 *   "nvgpu_sdl_report_error_rmos(g, &err_pkt, sizeof(err_pkt))" to enqueue
 *   the packet err_pkt into the circular buffer \ref nvgpu_os_rmos.sdl_rmos
 *   "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q "emsg_q". In case
 *   of failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure() "nvgpu_sdl_handle_report_failure(g)"
 *   and return.
 * - The error packet err_pkt will be dequeued from \ref
 *   nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q
 *   "emsg_q" and reported to Safety Service by nvgpu_sdl_worker() thread.
 *
 * @return	None
 */
void nvgpu_report_ctxsw_err(struct gk20a *g, u32 hw_unit, u32 err_id,
		void *data);

/**
 * @brief This function provides an interface to report SM and PGRAPH erros
 *        to SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 *                                - The function does not perform validation of
 *                                  g parameter.
 * @param hw_unit [in]		- Index of HW unit.
 *				  - The function validates that this paramter
 *				    has anyone of the following values.
 *				  - \link NVGPU_ERR_MODULE_SM \endlink
 *				  - \link NVGPU_ERR_MODULE_PGRAPH \endlink
 * @param inst [in]		- Instance ID.
 *				  - In case of multiple instances of the same HW
 *				    unit (e.g., there are multiple instances of
 *				    SM), it is used to identify the instance
 *				    that encountered a fault.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param err_id [in]		- Error index.
 *				  - The function validates that this parameter
 *				    has a value within valid range.
 *				  - For \link NVGPU_ERR_MODULE_SM \endlink
 *				    - \link GPU_SM_MACHINE_CHECK_ERROR \endlink
 *				  - For \link NVGPU_ERR_MODULE_PGRAPH \endlink
 *				    - Min: \link GPU_PGRAPH_FE_EXCEPTION \endlink
 *				    - Max: \link GPU_PGRAPH_GPC_GFX_EXCEPTION \endlink
 * @param sub_err_type [in]	- Sub error type.
 *				  - This is a sub-error of the error that is
 *				    being reported.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param err_info [in]		- Error information.
 *				  - For SM: Machine Check Error Information.
 *				  - For PGRAPH: Exception Information.
 *				  - The function does not perform any validation
 *				    on this parameter.
 *
 * - Check nvgpu_os_rmos.is_sdl_supported equals true (pointer to nvgpu_os_rmos
 *   is obtained using \ref nvgpu_os_rmos_from_gk20a()
 *   "nvgpu_os_rmos_from_gk20a(g)"), return on failure.
 * - Perform validation of input paramters, see paramter section for detailed
 *   validation criteria. In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Get the current time using api clock_gettime(CLOCK_MONOTONIC, &ts).
 *   In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Declare and initialize an error message packet err_pkt of type
 *   nvgpu_err_msg, using \ref nvgpu_init_ce_err_msg()
 *   "nvgpu_init_gr_err_msg(&err_pkt)".
 * - Get error description err_desc of type nvgpu_err_desc from internal
 *   look-up table \ref nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref
 *   nvgpu_sdl_rmos.err_lut "err_lut" using hw_unit and err_id.
 * - Update the following fields in the error message packet err_pkt.
 *   - nvgpu_err_msg.hw_unit_id = hw_unit
 *   - nvgpu_err_msg.err_id = err_desc->error_id
 *   - nvgpu_err_msg.is_critical = err_desc->is_critical
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ce_info "gr_info".\ref gpu_gr_error_info.header
 *     "header".\ref gpu_err_header.sub_err_type "sub_err_type" = sub_err_type
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ce_info "gr_info".\ref gpu_gr_error_info.header
 *     "header".\ref gpu_err_header.sub_unit_id "sub_unit_id" = inst
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.gr_info "gr_info".\ref gpu_gr_error_info.header
 *     "header".\ref gpu_err_header.timestamp_ns "timestamp_ns" =
 *     \ref nvgpu_timespec2nsec() "nvgpu_timespec2nsec(&ts)"
 *   - nvgpu_err_msg.err_desc = err_desc
 *   - nvgpu_err_msg.err_size = sizeof(gpu_error_info.gr_info)
 * - Invoke \ref nvgpu_sdl_report_error_rmos()
 *   "nvgpu_sdl_report_error_rmos(g, &err_pkt, sizeof(err_pkt))" to enqueue
 *   the packet err_pkt into the circular buffer \ref nvgpu_os_rmos.sdl_rmos
 *   "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q "emsg_q". In case
 *   of failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure() "nvgpu_sdl_handle_report_failure(g)"
 *   and return.
 * - The error packet err_pkt will be dequeued from \ref
 *   nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q
 *   "emsg_q" and reported to Safety Service by nvgpu_sdl_worker() thread.
 *
 * @return	None
 */
void nvgpu_report_gr_err(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, struct gr_err_info *err_info, u32 sub_err_type);

/**
 * @brief This function provides an interface to report PMU erros to SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 *                                - The function does not perform validation of
 *                                  g parameter.
 * @param hw_unit [in]		- Index of HW unit.
 *				  - The function validates that hw_unit ==
 *				    \link NVGPU_ERR_MODULE_PMU \endlink.
 * @param err_id [in]		- Error index.
 *				  - The function validates that err_id ==
 *				    \link GPU_PMU_BAR0_ERROR_TIMEOUT \endlink.
 * @param sub_err_type [in]	- Sub error type.
 *				  - This is a sub-error of the error that is
 *				    being reported.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param status [in]		- Error information.
 *				  - The function does not perform any validation
 *				    on this parameter.
 *
 * - Check nvgpu_os_rmos.is_sdl_supported equals true (pointer to nvgpu_os_rmos
 *   is obtained using \ref nvgpu_os_rmos_from_gk20a()
 *   "nvgpu_os_rmos_from_gk20a(g)"), return on failure.
 * - Perform validation of input paramters, see paramter section for detailed
 *   validation criteria. In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Get the current time using api clock_gettime(CLOCK_MONOTONIC, &ts).
 *   In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Declare and initialize an error message packet err_pkt of type
 *   nvgpu_err_msg, using \ref nvgpu_init_ce_err_msg()
 *   "nvgpu_init_pmu_err_msg(&err_pkt)".
 * - Get error description err_desc of type nvgpu_err_desc from internal
 *   look-up table \ref nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref
 *   nvgpu_sdl_rmos.err_lut "err_lut" using hw_unit and err_id.
 * - Update the following fields in the error message packet err_pkt.
 *   - nvgpu_err_msg.hw_unit_id = hw_unit
 *   - nvgpu_err_msg.err_id = err_desc->error_id
 *   - nvgpu_err_msg.is_critical = err_desc->is_critical
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.pmu_info "pmu_info".\ref gpu_pmu_error_info.status
 *     "status" = status
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ce_info "pmu_info".\ref gpu_pmu_error_info.header
 *     "header".\ref gpu_err_header.sub_err_type "sub_err_type" = sub_err_type
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.pmu_info "pmu_info".\ref gpu_pmu_error_info.header
 *     "header".\ref gpu_err_header.timestamp_ns "timestamp_ns" =
 *     \ref nvgpu_timespec2nsec() "nvgpu_timespec2nsec(&ts)"
 *   - nvgpu_err_msg.err_desc = err_desc
 *   - nvgpu_err_msg.err_size = sizeof(gpu_error_info.pmu_info)
 * - Invoke \ref nvgpu_sdl_report_error_rmos()
 *   "nvgpu_sdl_report_error_rmos(g, &err_pkt, sizeof(err_pkt))" to enqueue
 *   the packet err_pkt into the circular buffer \ref nvgpu_os_rmos.sdl_rmos
 *   "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q "emsg_q". In case
 *   of failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure() "nvgpu_sdl_handle_report_failure(g)"
 *   and return.
 * - The error packet err_pkt will be dequeued from \ref
 *   nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q
 *   "emsg_q" and reported to Safety Service by nvgpu_sdl_worker() thread.
 *
 * @return	None
 */
void nvgpu_report_pmu_err(struct gk20a *g, u32 hw_unit, u32 err_id,
	u32 sub_err_type, u32 status);

/**
 * @brief This function provides an interface to report PRI erros to SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 *                                - The function does not perform validation of
 *                                  g parameter.
 * @param hw_unit [in]		- Index of HW unit.
 *				  - The function validates that hw_unit ==
 *				    \link NVGPU_ERR_MODULE_PRI \endlink.
 * @param inst [in]		- Instance ID.
 *				  - In case of multiple instances of the same HW
 *				    unit, it is used to identify the instance
 *				    that encountered a fault.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param err_id [in]		- Error index.
 *				  - The function validates that this paramter
 *				    has a value within valid range.
 *				  - Min: \link GPU_PRI_TIMEOUT_ERROR \endlink
 *				  - Max: \link GPU_PRI_ACCESS_VIOLATION \endlink
 * @param err_addr [in]		- Error address.
 *				  - This is the address of the first PRI access
 *				    that resulted in an error.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param err_code [in]		- Error code.
 *				  - This is an unique code associated with the
 *				    error that is being reported.
 *				  - The function does not perform any validation
 *				    on this parameter.
 *
 * - Check nvgpu_os_rmos.is_sdl_supported equals true (pointer to nvgpu_os_rmos
 *   is obtained using \ref nvgpu_os_rmos_from_gk20a()
 *   "nvgpu_os_rmos_from_gk20a(g)"), return on failure.
 * - Perform validation of input paramters, see paramter section for detailed
 *   validation criteria. In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Get the current time using api clock_gettime(CLOCK_MONOTONIC, &ts).
 *   In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Declare and initialize an error message packet err_pkt of type
 *   nvgpu_err_msg, using \ref nvgpu_init_pri_err_msg()
 *   "nvgpu_init_pri_err_msg(&err_pkt)".
 * - Get error description err_desc of type nvgpu_err_desc from internal
 *   look-up table \ref nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref
 *   nvgpu_sdl_rmos.err_lut "err_lut" using hw_unit and err_id.
 * - Update the following fields in the error message packet err_pkt.
 *   - nvgpu_err_msg.hw_unit_id = hw_unit
 *   - nvgpu_err_msg.err_id = err_desc->error_id
 *   - nvgpu_err_msg.is_critical = err_desc->is_critical
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.pri_info "pri_info".\ref gpu_pri_error_info.header
 *     "header".\ref gpu_err_header.sub_unit_id "sub_unit_id" = inst
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ce_info "pri_info".\ref gpu_pri_error_info.header
 *     "header".\ref gpu_err_header.address "address" = err_addr
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.ce_info "pri_info".\ref gpu_pri_error_info.header
 *     "header".\ref gpu_err_header.sub_err_type "sub_err_type" = err_code
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.pri_info "pri_info".\ref gpu_pri_error_info.header
 *     "header".\ref gpu_err_header.timestamp_ns "timestamp_ns" =
 *     \ref nvgpu_timespec2nsec() "nvgpu_timespec2nsec(&ts)"
 *   - nvgpu_err_msg.err_desc = err_desc
 *   - nvgpu_err_msg.err_size = sizeof(gpu_error_info.pri_info)
 * - Invoke \ref nvgpu_sdl_report_error_rmos()
 *   "nvgpu_sdl_report_error_rmos(g, &err_pkt, sizeof(err_pkt))" to enqueue
 *   the packet err_pkt into the circular buffer \ref nvgpu_os_rmos.sdl_rmos
 *   "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q "emsg_q". In case
 *   of failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure() "nvgpu_sdl_handle_report_failure(g)"
 *   and return.
 * - The error packet err_pkt will be dequeued from \ref
 *   nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q
 *   "emsg_q" and reported to Safety Service by nvgpu_sdl_worker() thread.
 *
 * @return	None
 */
void nvgpu_report_pri_err(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, u32 err_addr, u32 err_code);

/**
 * @brief This function provides an interface to report HUBMMU erros to SDL.
 *
 * @param g [in]		- The GPU driver struct.
 *                                - The function does not perform validation of
 *                                  g parameter.
 * @param hw_unit [in]		- Index of HW unit.
 *				  - The function validates that hw_unit ==
 *				    \link NVGPU_ERR_MODULE_HUBMMU \endlink.
 * @param err_id [in]		- Error index.
 *				  - The function validates that hw_unit ==
 *				    \link GPU_HUBMMU_PAGE_FAULT_ERROR \endlink.
 * @param fault_info [in]	- MMU page fault information.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param status [in]		- Error information.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param sub_err_type [in]	- Sub error type.
 *				  - This is a sub-error of the error that is
 *				    being reported.
 *				  - The function does not perform any validation
 *				    on this parameter.
 *
 * - Check nvgpu_os_rmos.is_sdl_supported equals true (pointer to nvgpu_os_rmos
 *   is obtained using \ref nvgpu_os_rmos_from_gk20a()
 *   "nvgpu_os_rmos_from_gk20a(g)"), return on failure.
 * - Perform validation of input paramters, see paramter section for detailed
 *   validation criteria. In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Get the current time using api clock_gettime(CLOCK_MONOTONIC, &ts).
 *   In case of a failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure()
 *   "nvgpu_sdl_handle_report_failure(g)" and return.
 * - Declare and initialize an error message packet err_pkt of type
 *   nvgpu_err_msg, using \ref nvgpu_init_mmu_err_msg()
 *   "nvgpu_init_mmu_err_msg(&err_pkt)".
 * - Get error description err_desc of type nvgpu_err_desc from internal
 *   look-up table \ref nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref
 *   nvgpu_sdl_rmos.err_lut "err_lut" using hw_unit and err_id.
 * - Update the following fields in the error message packet err_pkt.
 *   - nvgpu_err_msg.hw_unit_id = hw_unit
 *   - nvgpu_err_msg.err_id = err_desc->error_id
 *   - nvgpu_err_msg.is_critical = err_desc->is_critical
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.header
 *     "header".\ref gpu_err_header.sub_err_type "sub_err_type" = sub_err_type
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.status
 *     "status" = status
 *   - If \a fault_info is not NULL, then copy the content of mmu fault info.
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.inst_ptr "inst_ptr"
 *       = fault_info->inst_ptr
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.inst_aperture "inst_aperture"
 *       = fault_info->inst_aperture
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.fault_addr "fault_addr"
 *       = fault_info->fault_addr
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.fault_addr_aperture
 *       "fault_addr_aperture" = fault_info->fault_addr_aperture
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.timestamp_lo "timestamp_lo"
 *       = fault_info->timestamp_lo
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.timestamp_hi "timestamp_hi"
 *       = fault_info->timestamp_hi
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.mmu_engine_id "mmu_engine_id"
 *       = fault_info->mmu_engine_id
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.gpc_id "gpc_id"
 *       = fault_info->gpc_id
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.client_type "client_type"
 *       = fault_info->client_type
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.client_id "client_id"
 *       = fault_info->client_id
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.fault_type "fault_type"
 *       = fault_info->fault_type
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.access_type "access_type"
 *       = fault_info->access_type
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.protected_mode "protected_mode"
 *       = fault_info->protected_mode
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.replayable_fault "replayable_fault"
 *       = fault_info->replayable_fault
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.replay_fault_en "replay_fault_en"
 *       = fault_info->replay_fault_en
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.valid "valid"
 *       = fault_info->valid
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.faulted_pbdma "faulted_pbdma"
 *       = fault_info->faulted_pbdma
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.faulted_engine "faulted_engine"
 *       = fault_info->faulted_engine
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.faulted_subid "faulted_subid"
 *       = fault_info->faulted_subid
 *     - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *       gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.info
 *       "info".\ref mmu_page_fault_info.chid "chid" = fault_info->chid
 *   - \ref nvgpu_err_msg.err_info "nvgpu_err_msg.err_info".\ref
 *     gpu_error_info.mmu_info "mmu_info".\ref gpu_mmu_error_info.header
 *     "header".\ref gpu_err_header.timestamp_ns "timestamp_ns" =
 *     \ref nvgpu_timespec2nsec() "nvgpu_timespec2nsec(&ts)"
 *   - nvgpu_err_msg.err_desc = err_desc
 *   - nvgpu_err_msg.err_size = sizeof(gpu_error_info.mmu_info)
 * - Invoke \ref nvgpu_sdl_report_error_rmos()
 *   "nvgpu_sdl_report_error_rmos(g, &err_pkt, sizeof(err_pkt))" to enqueue
 *   the packet err_pkt into the circular buffer \ref nvgpu_os_rmos.sdl_rmos
 *   "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q "emsg_q". In case
 *   of failure, print error message and invoke
 *   \ref nvgpu_sdl_handle_report_failure() "nvgpu_sdl_handle_report_failure(g)"
 *   and return.
 * - The error packet err_pkt will be dequeued from \ref
 *   nvgpu_os_rmos.sdl_rmos "nvgpu_os_rmos.sdl_rmos".\ref nvgpu_sdl_rmos.emsg_q
 *   "emsg_q" and reported to Safety Service by nvgpu_sdl_worker() thread.
 *
 * @return	None
 */
void nvgpu_report_mmu_err(struct gk20a *g, u32 hw_unit,
		u32 err_id, struct mmu_fault_info *fault_info,
		u32 status, u32 sub_err_type);

/**
 * @brief This is a wrapper function to report CTXSW errors to SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 *                                - The function does not perform validation of
 *                                  g parameter.
 * @param err_type [in]		- Error index.
 *				  - The function validates that, this paramter
 *				    has a value within valid range.
 *				  - Min: \link GPU_FECS_CTXSW_WATCHDOG_TIMEOUT \endlink
 *				  - Max: \link GPU_FECS_CTXSW_INIT_ERROR \endlink
 * @param chid [in]		- Channel ID.
 *				  - The function does not perform any validation
 *				    on this parameter.
 * @param mailbox_value [in]	- Mailbox value.
 *				  - The function does not perform any validation
 *				    on this parameter.
 *
 * - Creates the variable \a err_info of type #ctxsw_err_info structure.
 * - Fills the details related to current context, ctxsw status, mailbox value,
 *   channel ID in #ctxsw_err_info strucutre.
 *   - \ref ctxsw_err_info.curr_ctx "curr_ctx" =
 *   g->ops.gr.falcon.get_current_ctx(g)
 *   - \ref ctxsw_err_info.ctxsw_status0 "ctxsw_status0" =
 *   g->ops.gr.falcon.read_fecs_ctxsw_status0(g)
 *   - \ref ctxsw_err_info.ctxsw_status1 "ctxsw_status1" =
 *   g->ops.gr.falcon.read_fecs_ctxsw_status1(g)
 *   - \ref ctxsw_err_info.mailbox_value "mailbox_value" = mailbox_value
 *   - \ref ctxsw_err_info.chid "chid" = chid
 * - Invokes #nvgpu_report_ctxsw_err() with the following arguments: (1) g,
 *   (2) \link NVGPU_ERR_MODULE_FECS \endlink (3) err_type,
 *   (4) (void *)&err_info.
 *
 * @return	None
 */
void gr_intr_report_ctxsw_error(struct gk20a *g, u32 err_type, u32 chid,
		u32 mailbox_value);
#endif /* CONFIG_NVGPU_INTR_DEBUG */

/**
 * @brief This is a wrapper function to report ECC errors from HUBMMU to SDL.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit_id [in]	- HW Unit ID.
 * @param err_id [in]		- Error ID.
 *
 * Calls nvgpu_report_err_to_ss to report errors to Safety_Services.
 *
 * @return	None
 */
void nvgpu_report_err_to_sdl(struct gk20a *g, u32 hw_unit_id, u32 err_id);

#endif /* NVGPU_NVGPU_ERR_H */
