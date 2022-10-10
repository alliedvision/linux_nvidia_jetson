/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_ECC_H
#define NVGPU_ECC_H

/**
 * @file
 * @page unit-ecc Unit ECC(Error Control Codes)
 *
 * Acronyms
 * ========
 * ECC     - Error Control Codes
 * SEC     - Single Error Correction
 * SEC-DED - Standard single-error correction with double-error detection
 * SED     - Single Error Detection
 *
 * Overview
 * ========
 * The memories within the GPU are protected using data integrity protection
 * mechanism like ecc or parity. This unit is responsible for maintaining
 * error counters for all memories which support ecc/parity protection in
 * a list.
 *
 * + Initialization:
 *   This unit concatenates error counters (corrected and uncorrected) for
 *   each memory into a list.
 *
 * Data Structures
 * ===============
 *
 * The data structures exposed by the ECC unit, conveys to the user information
 * regarding the corrected, uncorrected errors encountered in the constituent
 * memories in the GPU hardware units like (gr, ltc, pmu, etc).
 *
 * The following are the list of data structures:
 *
 * + struct nvgpu_ecc_stat
 *
 *
 * + struct nvgpu_ecc
 *
 *
 * Static Design
 * =============
 *
 *	TODO
 *
 * Dynamic Design
 * =============
 *
 *	TODO
 *
 * External APIs
 * -------------
 *
 */

#include <nvgpu/types.h>
#include <nvgpu/list.h>
#include <nvgpu/lock.h>

#define NVGPU_ECC_STAT_NAME_MAX_SIZE	100UL

struct gk20a;

/**
 * This struct holds the ecc/parity error information associated with each
 * memory. The error information includes a string that can be used to
 * uniquely identity the memory, error type. In addition it has a  32 bit
 * counter to track the number of instances of the errors.
 */
struct nvgpu_ecc_stat {
	/** The unique name associated with error */
	char name[NVGPU_ECC_STAT_NAME_MAX_SIZE];
	/** The 32-bit error counter */
	u32 counter;
	/**
	 * The embedded list element, this is used to link the counters into
	 * linked list.
	 */
	struct nvgpu_list_node node;
};

/**
 * @brief	Helper function to get struct nvgpu_ecc_stat from list node.
 *
 * @param node [in] List element node.
 *
 * @return Pointer to struct nvgpu_ecc_stat.
 *
 */
static inline struct nvgpu_ecc_stat *nvgpu_ecc_stat_from_node(
		struct nvgpu_list_node *node)
{
	return (struct nvgpu_ecc_stat *)(
			(uintptr_t)node - offsetof(struct nvgpu_ecc_stat, node)
		);
}

/**
 * The structure contains the error statistics assocaited with constituent
 * memories within each gpu hardware unit. All statistics are linked together
 * into a list, the head of this list is stored in stats_list.
 */
struct nvgpu_ecc {
	/**
	 * Contains error statistics for each memory contained within the gr
	 * unit.
	 */
	struct {
		/** SM register file SEC count. */
		struct nvgpu_ecc_stat **sm_lrf_ecc_single_err_count;
		/** SM register file DED count. */
		struct nvgpu_ecc_stat **sm_lrf_ecc_double_err_count;

		/** SM shared memory SEC count. */
		struct nvgpu_ecc_stat **sm_shm_ecc_sec_count;
		/** SM shared memory SED count. */
		struct nvgpu_ecc_stat **sm_shm_ecc_sed_count;
		/** SM shared memory DED count. */
		struct nvgpu_ecc_stat **sm_shm_ecc_ded_count;

		/** TEX pipe0 total SEC count. */
		struct nvgpu_ecc_stat **tex_ecc_total_sec_pipe0_count;
		/** TEX pipe0 total DED count. */
		struct nvgpu_ecc_stat **tex_ecc_total_ded_pipe0_count;
		/** TEX pipe0 unique SEC count. */
		struct nvgpu_ecc_stat **tex_unique_ecc_sec_pipe0_count;
		/** TEX pipe0 unique DED count. */
		struct nvgpu_ecc_stat **tex_unique_ecc_ded_pipe0_count;
		/** TEX pipe1 total SEC count. */
		struct nvgpu_ecc_stat **tex_ecc_total_sec_pipe1_count;
		/** TEX pipe1 total DED count. */
		struct nvgpu_ecc_stat **tex_ecc_total_ded_pipe1_count;
		/** TEX pipe1 unique SEC count. */
		struct nvgpu_ecc_stat **tex_unique_ecc_sec_pipe1_count;
		/** TEX pipe1 unique DED count. */
		struct nvgpu_ecc_stat **tex_unique_ecc_ded_pipe1_count;

		/** SM l1-tag correct error count. */
		struct nvgpu_ecc_stat **sm_l1_tag_ecc_corrected_err_count;
		/** SM l1-tag uncorrected error count. */
		struct nvgpu_ecc_stat **sm_l1_tag_ecc_uncorrected_err_count;
		/** SM CBU corrected error count. */
		struct nvgpu_ecc_stat **sm_cbu_ecc_corrected_err_count;
		/** SM CBU uncorrected error count. */
		struct nvgpu_ecc_stat **sm_cbu_ecc_uncorrected_err_count;
		/** SM l1-data corrected error count. */
		struct nvgpu_ecc_stat **sm_l1_data_ecc_corrected_err_count;
		/** SM l1-data uncorrected error count. */
		struct nvgpu_ecc_stat **sm_l1_data_ecc_uncorrected_err_count;
		/** SM icache corrected error count. */
		struct nvgpu_ecc_stat **sm_icache_ecc_corrected_err_count;
		/** SM icache uncorrected error count. */
		struct nvgpu_ecc_stat **sm_icache_ecc_uncorrected_err_count;
		/** SM RAMS corrected error count. */
		struct nvgpu_ecc_stat **sm_rams_ecc_corrected_err_count;
		/** SM RAMS uncorrected error count. */
		struct nvgpu_ecc_stat **sm_rams_ecc_uncorrected_err_count;

		/** GCC l1.5-cache corrected error count. */
		struct nvgpu_ecc_stat *gcc_l15_ecc_corrected_err_count;
		/** GCC l1.5-cache uncorrected error count. */
		struct nvgpu_ecc_stat *gcc_l15_ecc_uncorrected_err_count;

		/** GPCCS falcon i-mem, d-mem corrected error count. */
		struct nvgpu_ecc_stat *gpccs_ecc_corrected_err_count;
		/** GPCCS falcone i-mem, d-mem uncorrected error count. */
		struct nvgpu_ecc_stat *gpccs_ecc_uncorrected_err_count;

		/** GMMU l1tlb corrected error count. */
		struct nvgpu_ecc_stat *mmu_l1tlb_ecc_corrected_err_count;
		/** GMMU l1tlb uncorrected error count. */
		struct nvgpu_ecc_stat *mmu_l1tlb_ecc_uncorrected_err_count;

		/** FECS falcon i-mem, d-mem corrected error count. */
		struct nvgpu_ecc_stat *fecs_ecc_corrected_err_count;
		/** FECS falcon i-mem, d-mem uncorrected error count. */
		struct nvgpu_ecc_stat *fecs_ecc_uncorrected_err_count;
	} gr;

	/**
	 * Contains error statistics for each memory contained within the ltc
	 * unit.
	 */
	struct {
		/** L2 cache slice RSTG ECC PARITY error count. */
		struct nvgpu_ecc_stat **rstg_ecc_parity_count;
		/** L2 cache slice TSTG ECC PARITY error count. */
		struct nvgpu_ecc_stat **tstg_ecc_parity_count;
		/** L2 cache slice DSTG BE ECC PARITY error count. */
		struct nvgpu_ecc_stat **dstg_be_ecc_parity_count;
		/** L2 cache slice SEC error count. */
		struct nvgpu_ecc_stat **ecc_sec_count;
		/** L2 cache slice DED error count. */
		struct nvgpu_ecc_stat **ecc_ded_count;
	} ltc;

	/**
	 * Contains error statistics for each memory contained within the fb
	 * unit.
	 */
	struct {
		/** hubmmu l2tlb corrected error count. */
		struct nvgpu_ecc_stat *mmu_l2tlb_ecc_corrected_err_count;
		/** hubmmu l2tlb uncorrected error count. */
		struct nvgpu_ecc_stat *mmu_l2tlb_ecc_uncorrected_err_count;
		/** hubmmu hubtlb corrected error count. */
		struct nvgpu_ecc_stat *mmu_hubtlb_ecc_corrected_err_count;
		/** hubmmu hubtlb uncorrected error count. */
		struct nvgpu_ecc_stat *mmu_hubtlb_ecc_uncorrected_err_count;
		/** hubmmu fillunit corrected error count. */
		struct nvgpu_ecc_stat *mmu_fillunit_ecc_corrected_err_count;
		/** hubmmu fillunit uncorrected error count. */
		struct nvgpu_ecc_stat *mmu_fillunit_ecc_uncorrected_err_count;
		/* Leave extra tab to fit into nvgpu_ecc.fb structure */
		struct nvgpu_ecc_stat *mmu_l2tlb_ecc_corrected_unique_err_count;
		/** hubmmu l2tlb uncorrected unique error count. */
		struct nvgpu_ecc_stat *mmu_l2tlb_ecc_uncorrected_unique_err_count;
		/** hubmmu hubtlb corrected unique error count. */
		struct nvgpu_ecc_stat *mmu_hubtlb_ecc_corrected_unique_err_count;
		/** hubmmu hubtlb uncorrected unique error count. */
		struct nvgpu_ecc_stat *mmu_hubtlb_ecc_uncorrected_unique_err_count;
		/** hubmmu fillunit corrected unique error count. */
		struct nvgpu_ecc_stat *mmu_fillunit_ecc_corrected_unique_err_count;
		/** hubmmu fillunit uncorrected unique error count. */
		struct nvgpu_ecc_stat *mmu_fillunit_ecc_uncorrected_unique_err_count;
	} fb;

	/**
	 * Contains error statistics for each memory contained within the pmu
	 * unit.
	 */
	struct {
		/** PMU falcon imem, dmem corrected error count. */
		struct nvgpu_ecc_stat *pmu_ecc_corrected_err_count;
		/** PMU falcon imem, dmem uncorrected error count. */
		struct nvgpu_ecc_stat *pmu_ecc_uncorrected_err_count;
	} pmu;

	/**
	 * Contains error statistics for each memory contained within the fbpa
	 * unit.
	 */
	struct {
		/** fbpa sec count. */
		struct nvgpu_ecc_stat *fbpa_ecc_sec_err_count;
		/** fbpa ded count. */
		struct nvgpu_ecc_stat *fbpa_ecc_ded_err_count;
	} fbpa;

	/** Contains the head to the list of error statistics. */
	struct nvgpu_list_node stats_list;
	/** Lock to protect the stats_list updates. */
	struct nvgpu_mutex stats_lock;
	/** Contains the number of error statistics. */
	int stats_count;
	/**
	 * Indicates if ECC initialization (counters allocation and sysfs
	 * setup) is completed.
	 */
	bool initialized;
};

/**
 * @brief Allocates, initializes an error counter with specified name.
 *
 * @param g [in] The GPU driver struct.
 * @param statp [out] Pointer to error counter pointer.
 * @param name [in] Unique name for error counter.
 *
 * Allocate memory for one error counter, initializes the counter with 0 and the
 * specified string identifier. Finally the counter is added to the stats_list
 * of struct nvgpu_ecc.
 *
 * @return 0 in case of success, less than 0 for failure.
 * @return -ENOMEM if there is not enough memory to allocate ecc stats.
 */
int nvgpu_ecc_counter_init(struct gk20a *g,
		struct nvgpu_ecc_stat **statp, const char *name);

/**
 * @brief Deallocates an error counter.
 *
 * @param g [in] The GPU driver struct.
 * @param statp [in] Pointer to error counter pointer.
 *
 * Delete the counter from the nvgpu_ecc stats_list. Deallocate memory for the
 * error counter.
 */
void nvgpu_ecc_counter_deinit(struct gk20a *g, struct nvgpu_ecc_stat **statp);

/**
 * @brief Concatenates the error counter to stats list.
 *
 * @param g [in] The GPU driver struct.
 * @param stat [in] Pointer to error counter.
 *
 * The counter is added to the stats_list of struct nvgpu_ecc.
 */
void nvgpu_ecc_stat_add(struct gk20a *g, struct nvgpu_ecc_stat *stat);

/**
 * @brief Deletes the error counter from the stats list.
 *
 * @param g [in] The GPU driver struct.
 * @param stat [in] Pointer to error counter.
 *
 * The counter is removed from the stats_list of struct nvgpu_ecc.
 */
void nvgpu_ecc_stat_del(struct gk20a *g, struct nvgpu_ecc_stat *stat);

/**
 * @brief Release memory associated with all error counters.
 *
 * @param g [in] The GPU driver struct.
 *
 * Releases memory associated with all error counters associated with a hardware
 * unit, this is done for every instance of the hardware unit.
 */
void nvgpu_ecc_free(struct gk20a *g);

/**
 * @brief Initializes error counters list.
 *
 * @param g [in] The GPU driver struct.
 *
 * Initializes the error counters list g->ecc.stats_list.
 *
 * @return 0 in case of success, less than 0 for failure.
 */
int nvgpu_ecc_init_support(struct gk20a *g);

/**
 * @brief Destroys, frees up memory allocated to ecc/parity error counters.
 *
 * @param g [in] The GPU driver struct.
 *
 * Frees up memory allocated to ecc error counters for all units.
 */
void nvgpu_ecc_remove_support(struct gk20a *g);

/**
 * @brief Finish ECC support initialization.
 *
 * @param g [in] The GPU driver struct.
 *
 * Sets ecc.initialized to true.
 *
 * @return 0 in all cases.
 */
int nvgpu_ecc_finalize_support(struct gk20a *g);

#ifdef CONFIG_NVGPU_SYSFS
int nvgpu_ecc_sysfs_init(struct gk20a *g);
void nvgpu_ecc_sysfs_remove(struct gk20a *g);
#endif

#endif
