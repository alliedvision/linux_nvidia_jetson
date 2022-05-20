/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param ltc [in]		LTC unit number
	 *
	 * This function handles ltc related ecc interrupts.
	 */
	void (*isr)(struct gk20a *g, u32 ltc);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	void (*configure)(struct gk20a *g);
	void (*en_illegal_compstat)(struct gk20a *g, bool enable);
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
	 * @brief Initialize LTC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function reads ltc unit info from GPU h/w and stores
	 * it in #nvgpu_ltc structure. This function also initializes
	 * LTC unit ecc counters.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ENOMEM if memory allocation fails for #nvgpu_ltc.
	 */
	int (*init_ltc_support)(struct gk20a *g);

	/**
	 * @brief Remove LTC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function will free memory allocated for #nvgpu_ltc structure.
	 */
	void (*ltc_remove_support)(struct gk20a *g);

	/**
	 * @brief Returns GPU L2 cache size.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function returns GPU L2 cache size by reading h/w ltc
	 * config register.
	 *
	 * @return Size of L2 cache in bytes.
	 */
	u64 (*determine_L2_size_bytes)(struct gk20a *g);

	/**
	 * @brief Flush GPU L2 cache.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function flushes all L2 cache data to main memory by cleaning
	 * and invaliding all cache sub-units. s/w will poll for completion of
	 * each ltc unit cache cleaning/invalidation for 5 msec. This 5 msec
	 * time out is based on following calculations:
	 * Lowest EMC clock rate will be around 102MHz and thus available
	 * bandwidth is 64b * 2 * 102MHz = 1.3GB/s. Of that bandwidth, GPU
	 * will likely get about half, so 650MB/s at worst. Assuming at most
	 * 1MB of GPU L2 cache, worst case it will take 1MB/650MB/s = 1.5ms.
	 * So 5ms timeout here should be more than enough.
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
	void (*set_enabled)(struct gk20a *g, bool enabled);
	void (*ltc_lts_set_mgmt_setup)(struct gk20a *g);
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
