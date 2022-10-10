/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_LTC_H
#define NVGPU_LTC_H

/**
 * @file
 *
 * common.ltc unit interface
 */
#include <nvgpu/types.h>
#include <nvgpu/lock.h>

struct gk20a;
struct nvgpu_ecc_stat;
/**
 * LTC data structure.
 *
 * This structure stores data related to ltc unit.
 */
struct nvgpu_ltc {
#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
	/**
	 * Spinlock to protect all ltc operations.
	 */
	struct nvgpu_spinlock ltc_enabled_lock;
#endif
	/** Maximum ltc count value is read from h/w top config register. */
	u32 max_ltc_count;
	/** Enumerated ltc count value is read from h/w priv ring register. */
	u32 ltc_count;
	/** Slices per ltc value is read from h/w ltc cbc register. */
	u32 slices_per_ltc;
	/** Cache line size in bytes is read from h/w ltc cbc register. */
	u32 cacheline_size;
};

/**
 * @brief Get enumerated ltcs count.
 *
 * @param g [in]            - The GPU driver struct.
 *                            - The function does not perform validation
 *                              of g parameter.
 *
 * This function returns enumerated number of ltcs after floorsweeping.
 * After floorsweeping enumerated ltcs may be less than maximum ltcs available.
 *
 * - Return value of g->ltc->ltc_count.
 *
 * @return Number of enumerated ltc count.
 */
u32 nvgpu_ltc_get_ltc_count(struct gk20a *g);

/**
 * @brief Get slices per ltc.
 *
 * @param g [in]            - The GPU driver struct.
 *                            - The function does not perform validation
 *                              of g parameter.
 *
 * This function returns slices per ltc.
 * Each ltc unit is constituted by h/w configured multiple physical slices.
 * Clients can use slice size info to make their cache requirement to
 * a slice for better bandwidth and/or utilization.
 *
 * - Return value of g->ltc->slices_per_ltc.
 *
 * @return Number of slices per ltc.
 */
u32 nvgpu_ltc_get_slices_per_ltc(struct gk20a *g);

/**
 * @brief Get cacheline size.
 *
 * @param g [in]            - The GPU driver struct.
 *                            - The function does not perform validation
 *                              of g parameter.
 *
 * This function returns cacheline size in bytes.
 * Cacheline is chunk of memory that can be handled in one go by cache.
 * Cacheline size is configured as multiple of 512 bytes in h/w.
 *
 * - Return value of g->ltc->cacheline_size.
 *
 * @return Cacheline size in bytes.
 */
u32 nvgpu_ltc_get_cacheline_size(struct gk20a *g);

#define NVGPU_L2_SECTOR_PROMOTE_FLAG_NONE		(1U << 0U)
#define NVGPU_L2_SECTOR_PROMOTE_FLAG_64B		(1U << 1U)
#define NVGPU_L2_SECTOR_PROMOTE_FLAG_128B		(1U << 2U)
#define NVGPU_L2_SECTOR_PROMOTE_FLAG_INVALID		(1U << 3U)

/**
 * @brief Release all LTC ECC stats counters.
 *
 * @param g [in]            - The GPU driver struct.
 *                            - The function does not perform validation
 *                              of g parameter.
 *
 * Frees all error counters associated with the LTC unit.
 *
 * - For each ltc from 0 to \ref nvgpu_ltc_get_ltc_count "nvgpu_ltc_get_ltc_count(g)" - 1:
 *   - Free dynamically allocated memory for following ECC counters for slices: SEC, DED,
 *     RSTG parity, TSTG parity, DSTG parity.
 * - Free container of the ECC counters for the LTCs.
 *
 */
void nvgpu_ltc_ecc_free(struct gk20a *g);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Initialize #nvgpu_ltc structure.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function reads ltc unit info from GPU h/w and stores
 * it in #nvgpu_ltc structure. This function allocates memory
 * to track the ecc error counts for the LTC unit and enables
 * LTC unit interrupts and stalling interrupt at MC level.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if memory allocation for #nvgpu_ltc fails.
 */
int nvgpu_init_ltc_support(struct gk20a *g);
/**
 * @brief Remove support for LTC.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function will free memory allocated for #nvgpu_ltc structure.
 * LTC unit data will be no longer accessible by s/w.
 */
void nvgpu_ltc_remove_support(struct gk20a *g);

/**
 * @brief Allocate and initialize a error counters for all ltc-lts instances.
 *
 * @param g [in] The GPU driver struct.
 * @param stat [out] Pointer to array of ltc-lts error counters.
 * @param name [in] Unique name for error counter.
 *
 * Calculates the total number of ltc-lts instances, allocates memory for each
 * instance of error counter, initializes the counter with 0 and the specified
 * string identifier. Finally the counter is added to the stats_list of
 * struct nvgpu_ecc.
 *
 * @return 0 in case of success, less than 0 for failure.
 */
int nvgpu_ecc_counter_init_per_lts(struct gk20a *g,
		struct nvgpu_ecc_stat ***stat, const char *name);

/*
 * @brief Allocate and initialize counters for memories within ltc-lts
 *
 * @param stat [in] Address of pointer to struct nvgpu_ecc_stat.
 *
 */
#define NVGPU_ECC_COUNTER_INIT_PER_LTS(stat) \
	nvgpu_ecc_counter_init_per_lts(g, &g->ecc.ltc.stat, #stat)

/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
/**
 * @brief Enable/Disable caching feature of L2.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function will enable/disable caching feature of L2 based on
 * #mm.ltc_enabled_target. With #mm.ltc_enabled_target set to true,
 * gpu l2 caching feature will be enabled. Gpu L2 caching is enabled with h/w
 * power-on and can only be changed after h/w reset, before the first
 * transaction received by L2.
 * With #mm.ltc_enabled_target set to false, Gpu L2 caching will be disabled.
 * With Gpu L2 cache disabled, all transactions will miss in L2 and data will
 * be always write-through to main memory.
 *
 */
void nvgpu_ltc_sync_enabled(struct gk20a *g);
#endif
#endif /* NVGPU_LTC_H */
