/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/atomic.h>

struct gk20a;

/**
 * @defgroup INDICES_FOR_GPU_HW_UNITS
 * Macros used to assign unique index to GPU HW units.
 * @{
 */
#define NVGPU_ERR_MODULE_SM			(0U)
#define NVGPU_ERR_MODULE_FECS		(1U)
#define NVGPU_ERR_MODULE_PMU		(2U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_SM
 * Macros used to assign unique index to errors reported from the SM unit.
 * @{
 */
#define GPU_SM_L1_TAG_ECC_CORRECTED			(0U)
#define GPU_SM_L1_TAG_ECC_UNCORRECTED			(1U)
#define GPU_SM_CBU_ECC_UNCORRECTED			(3U)
#define GPU_SM_LRF_ECC_UNCORRECTED			(5U)
#define GPU_SM_L1_DATA_ECC_UNCORRECTED			(7U)
#define GPU_SM_ICACHE_L0_DATA_ECC_UNCORRECTED		(9U)
#define GPU_SM_ICACHE_L1_DATA_ECC_UNCORRECTED		(11U)
#define GPU_SM_ICACHE_L0_PREDECODE_ECC_UNCORRECTED	(13U)
#define GPU_SM_L1_TAG_MISS_FIFO_ECC_UNCORRECTED		(15U)
#define GPU_SM_L1_TAG_S2R_PIXPRF_ECC_UNCORRECTED	(17U)
#define GPU_SM_ICACHE_L1_PREDECODE_ECC_UNCORRECTED	(20U)
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
#define GPU_FECS_FALCON_DMEM_ECC_UNCORRECTED	(3U)
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
#define GPU_GPCCS_FALCON_DMEM_ECC_UNCORRECTED	(3U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_MMU
 * Macros used to assign unique index to errors reported from the MMU unit.
 * @{
 */
#define GPU_MMU_L1TLB_SA_DATA_ECC_UNCORRECTED	(1U)
#define GPU_MMU_L1TLB_FA_DATA_ECC_UNCORRECTED	(3U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_GCC
 * Macros used to assign unique index to errors reported from the GCC unit.
 * @{
 */
#define GPU_GCC_L15_ECC_UNCORRECTED		(1U)
/**
 * @}
 */


/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_PMU
 * Macros used to assign unique index to errors reported from the PMU unit.
 * @{
 */
#define GPU_PMU_FALCON_IMEM_ECC_CORRECTED	(0U)
#define GPU_PMU_FALCON_IMEM_ECC_UNCORRECTED	(1U)
#define GPU_PMU_FALCON_DMEM_ECC_UNCORRECTED	(3U)
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
#define GPU_LTC_CACHE_TSTG_ECC_UNCORRECTED	(3U)
#define GPU_LTC_CACHE_DSTG_BE_ECC_UNCORRECTED	(7U)
/**
 * @}
 */

/**
 * @defgroup LIST_OF_ERRORS_REPORTED_FROM_HUBMMU
 * Macros used to assign unique index to errors reported from the HUBMMU unit.
 * @{
 */
#define GPU_HUBMMU_L2TLB_SA_DATA_ECC_UNCORRECTED	(1U)
#define GPU_HUBMMU_TLB_SA_DATA_ECC_UNCORRECTED		(3U)
#define GPU_HUBMMU_PTE_DATA_ECC_UNCORRECTED		(5U)
#define GPU_HUBMMU_PDE0_DATA_ECC_UNCORRECTED		(7U)
#define GPU_HUBMMU_PAGE_FAULT_ERROR			(8U)


#ifdef CONFIG_NVGPU_SUPPORT_LINUX_ECC_ERROR_REPORTING
/**
 * @}
 */

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
	nvgpu_atomic_t err_count;

	/** Error ID. */
	u8 error_id;
};

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

struct gpu_ecc_error_info {
	struct gpu_err_header header;

	/** Number of ECC errors. */
	u64 err_cnt;
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

	/** Total number of errors reported from a given HW unit. */
	u32 num_errs;

	u32 base_ecc_service_id;

	/** Used to get error description from look-up table. */
	struct nvgpu_err_desc *errs;
};

struct nvgpu_ecc_reporting_ops {
	void (*report_ecc_err)(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, u64 err_addr, u64 err_count);
};

struct nvgpu_ecc_reporting {
	struct nvgpu_spinlock lock;
	/* This flag is protected by the above spinlock */
	bool ecc_reporting_service_enabled;
	const struct nvgpu_ecc_reporting_ops *ops;
};

 /**
  * This macro is used to initialize the members of nvgpu_err_desc struct.
  */
#define GPU_ERR(err, critical, id, threshold, ecount) \
{									\
		.name = (err),						\
		.is_critical = (critical),				\
		.error_id = (id),					\
		.err_threshold = (threshold),				\
		.err_count = NVGPU_ATOMIC_INIT(ecount),					\
}

/**
 * This macro is used to initialize critical errors.
 */
#define GPU_CRITERR(err, id, threshold, ecount) \
	GPU_ERR(err, true, id, threshold, ecount)

/**
 * This macro is used to initialize non-critical errors.
 */
#define GPU_NONCRITERR(err, id, threshold, ecount) \
	GPU_ERR(err, false, id, threshold, ecount)

/**
 * @brief GPU HW errors need to be reported to Safety_Services via SDL unit.
 *        This function provides an interface to report ECC erros to SDL unit.
 *
 * @param g [in]		- The GPU driver struct.
 * @param hw_unit [in]		- Index of HW unit.
 *				  - List of valid HW unit IDs
 *				    - NVGPU_ERR_MODULE_SM
 *				    - NVGPU_ERR_MODULE_FECS
 *				    - NVGPU_ERR_MODULE_GPCCS
 *				    - NVGPU_ERR_MODULE_MMU
 *				    - NVGPU_ERR_MODULE_GCC
 *				    - NVGPU_ERR_MODULE_PMU
 *				    - NVGPU_ERR_MODULE_LTC
 *				    - NVGPU_ERR_MODULE_HUBMMU
 * @param inst [in]		- Instance ID.
 *				  - In case of multiple instances of the same HW
 *				    unit (e.g., there are multiple instances of
 *				    SM), it is used to identify the instance
 *				    that encountered a fault.
 * @param err_id [in]		- Error index.
 *				  - For SM:
 *				    - Min: GPU_SM_L1_TAG_ECC_CORRECTED
 *				    - Max: GPU_SM_ICACHE_L1_PREDECODE_ECC_UNCORRECTED
 *				  - For FECS:
 *				    - Min: GPU_FECS_FALCON_IMEM_ECC_CORRECTED
 *				    - Max: GPU_FECS_INVALID_ERROR
 *				  - For GPCCS:
 *				    - Min: GPU_GPCCS_FALCON_IMEM_ECC_CORRECTED
 *				    - Max: GPU_GPCCS_FALCON_DMEM_ECC_UNCORRECTED
 *				  - For MMU:
 *				    - Min: GPU_MMU_L1TLB_SA_DATA_ECC_UNCORRECTED
 *				    - Max: GPU_MMU_L1TLB_FA_DATA_ECC_UNCORRECTED
 *				  - For GCC:
 *				    - Min: GPU_GCC_L15_ECC_UNCORRECTED
 *				    - Max: GPU_GCC_L15_ECC_UNCORRECTED
 *				  - For PMU:
 *				    - Min: GPU_PMU_FALCON_IMEM_ECC_CORRECTED
 *				    - Max: GPU_PMU_FALCON_DMEM_ECC_UNCORRECTED
 *				  - For LTC:
 *				    - Min: GPU_LTC_CACHE_DSTG_ECC_CORRECTED
 *				    - Max: GPU_LTC_CACHE_DSTG_BE_ECC_UNCORRECTED
 *				  - For HUBMMU:
 *				    - Min: GPU_HUBMMU_L2TLB_SA_DATA_ECC_UNCORRECTED
 *				    - Max: GPU_HUBMMU_PDE0_DATA_ECC_UNCORRECTED
 * @param err_addr [in]		- Error address.
 *				  - This is the location at which correctable or
 *				    uncorrectable error has occurred.
 * @param err_count [in]	- Error count.
 *
 * - Checks whether SDL is supported in the current GPU platform. If SDL is not
 *   supported, it simply returns.
 * - Validates both \a hw_unit and \a err_id indices. In case of a failure,
 *   invokes #nvgpu_sdl_handle_report_failure() api.
 * - Gets the current time of a clock. In case of a failure, invokes
 *   #nvgpu_sdl_handle_report_failure() api.
 * - Gets error description from internal look-up table using \a hw_unit and
 *   \a err_id indices.
 * - Forms error packet using details such as time-stamp, \a hw_unit, \a err_id,
 *   criticality of the error, \a inst, \a err_addr, \a err_count, error
 *   description, and size of the error packet.
 * - Performs compile-time assert check to ensure that the size of the error
 *   packet does not exceed the maximum allowable size specified in
 *   #MAX_ERR_MSG_SIZE.
 *
 * @return	None
 */
void nvgpu_report_ecc_err(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, u64 err_addr, u64 err_count);

void nvgpu_init_ecc_reporting(struct gk20a *g);
void nvgpu_enable_ecc_reporting(struct gk20a *g);
void nvgpu_disable_ecc_reporting(struct gk20a *g);
void nvgpu_deinit_ecc_reporting(struct gk20a *g);

#else

static inline void nvgpu_report_ecc_err(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, u64 err_addr, u64 err_count) {

}

#endif /* CONFIG_NVGPU_SUPPORT_LINUX_ECC_ERROR_REPORTING */

#endif /* NVGPU_NVGPU_ERR_H */