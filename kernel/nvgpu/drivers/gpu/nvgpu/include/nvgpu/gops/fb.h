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
#ifndef NVGPU_GOPS_FB_H
#define NVGPU_GOPS_FB_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * common.fb interface.
 */
struct gk20a;

/**
 * common.fb intr subunit hal operations.
 *
 * This structure stores common.fb interrupt subunit hal pointers.
 *
 * @see gpu_ops
 */
struct gops_fb_intr {
	/**
	 * @brief Enable fb hub interrupts.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function enable the stalling interrupt for fb unit at MC level.
	 * This function enables following fb hub interrupts.
	 * MMU_ECC_UNCORRECTED_ERROR_NOTIFY: Uncorrected ECC errors.
	 * MMU_NONREPLAYABLE_FAULT_NOTIFY: non-replayable fault happened.
	 * MMU_NONREPLAYABLE_FAULT_OVERFLOW : non-replayable fault buffer
	 *				      overflow occurred.
	 * MMU_OTHER_FAULT_NOTIFY: All other fault notifications from MMU.
	 */
	void (*enable)(struct gk20a *g);

	/**
	 * @brief Disable fb hub interrupts.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function disables fb hub interrupts enabled in enable.
	 * This function disables the stalling interrupt for fb unit at
	 * MC level.
	 */
	void (*disable)(struct gk20a *g);

	/**
	 * @brief ISR for fb hub interrupts.
	 *
	 * @param g [in]			Pointer to GPU driver struct.
	 * @param intr_unit_bitmask [in]	Bitmask of the mmu intr_units.
	 *
	 * This is the entry point to handle fb hub interrupts. This function
	 * handles all the interrupts enabled in enable function.
	 */
	void (*isr)(struct gk20a *g, u32 intr_unit_bitmask);

	/**
	 * @brief Checks any mmu fault interrupt is pending
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function checks and returns information about mmu fault pending.
	 *
	 * @return true in case of mmu faults pending, false otherwise.
	 */
	bool (*is_mmu_fault_pending)(struct gk20a *g);

	/*
	 * @brief Handle fb ecc error interrupts.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function handles ecc errors generated from memories within
	 * the fb.
	 */
	void (*handle_ecc)(struct gk20a *g);
	/**
	 * @brief Handle l2tlb ecc errors.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function handles ecc faults in l2tlb memory.
	 */
	void (*handle_ecc_l2tlb)(struct gk20a *g, u32 status);
	/**
	 * @brief Handle hubmmu tlb ecc errors.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function handles ecc faults in hubmmu tlb memory.
	 */
	void (*handle_ecc_hubtlb)(struct gk20a *g, u32 status);
	/**
	 * @brief Handle hubmmu fillunit ecc errors.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function handles ecc faults in hubmmu fillunit memory.
	 */
	void (*handle_ecc_fillunit)(struct gk20a *g, u32 status);

};

struct gops_fb_ecc {
	/**
	 * @brief Initialize FB unit ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function allocates memory to track the ecc error counts
	 * for FB unit.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*init)(struct gk20a *g);

	/**
	 * @brief Free FB unit ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function deallocates memory allocated for ecc error counts
	 * for FB unit.
	 */
	void (*free)(struct gk20a *g);

	/**
	 * @brief Fetch bitmask for l2tlb corrected, uncorrcted errors.
	 *
	 * @param corrected_error_mask [out]	Pointer to write corrected error
	 *					mask.
	 * @param uncorrected_error_mask [out]	Pointer to write uncorrected
	 *					error mask.
	 *
	 * Fetchs a bit mask of all the corrected, uncorrected errors supported
	 * by l2tlb.
	 */
	void (*l2tlb_error_mask)(u32 *corrected_error_mask,
			u32 *uncorrected_error_mask);
};

#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
struct nvgpu_vab_range_checker;

struct gops_fb_vab {
	/**
	 * @brief Initialize VAB
	 *
	 */
	int (*init)(struct gk20a *g);

	/**
	 * @brief Set VAB buffer address in HW registers
	 *
	 */
	void (*set_vab_buffer_address)(struct gk20a *g, u64 buf_addr);

	/**
	 * @brief Initialize VAB range checkers and enable VAB tracking
	 *
	 */
	int (*reserve)(struct gk20a *g, u32 vab_mode, u32 num_range_checkers,
		struct nvgpu_vab_range_checker *vab_range_checker);

	/**
	 * @brief Trigger VAB dump, copy buffer to user and clear
	 *
	 */
	int (*dump_and_clear)(struct gk20a *g, u8 *user_buf,
		u64 user_buf_size);

	/**
	 * @brief Disable VAB
	 *
	 */
	int (*release)(struct gk20a *g);

	/**
	 * @brief Free VAB resources
	 *
	 */
	int (*teardown)(struct gk20a *g);

	/**
	 * @brief Recover from VAB MMU fault
	 *
	 */
	void (*recover)(struct gk20a *g);
};
#endif

/**
 * common.fb unit hal operations.
 *
 * This structure stores common.fb unit hal pointers.
 *
 * @see gpu_ops
 */
struct gops_fb {

	/**
	 * @brief Initializes frame buffer h/w configuration.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * Frame Buffer(FB) init h/w function:
	 *  - configures address that is used for issuing flush reads to
	      system memory.
	 *  - initializes mmu debugger buffer.
	 *  - enables fb unit stalling interrupt at MC level.
	 *  - enables fb interrupts related to mmu faults.
	 */
	void (*init_hw)(struct gk20a *g);

	/**
	 * @brief Intitializes controls for GMMU state.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function initializes chip specific fb state.
	 * This function sets platform atomic capability mode to the
	 * following configuration:
	 *  - NV_PFB_PRI_MMU_CTRL_ATOMIC_CAPABILITY_MODE to RMW MODE
	 *  - NV_PFB_PRI_MMU_CTRL_ATOMIC_CAPABILITY_SYS_NCOH_MODE to L2
	 *  - NV_PFB_HSHUB_NUM_ACTIVE_LTCS_HUB_SYS_ATOMIC_MODE to USE_RMW
	 */
	void (*init_fs_state)(struct gk20a *g);

	/**
	 * @brief Gets master MMU register control
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function returns register that controls all MMU h/w units.
	 *
	 * @return u32 register for MMU control.
	 */
	u32 (*mmu_ctrl)(struct gk20a *g);

	/**
	 * @brief Gets register control for MMU debug mode
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function returns register that controls MMU debug mode.
	 * In debug mode, MMU translates all faulting engine requests using
	 * two dummy pages.  One dummy page handles writes, the other dummy
	 * page handles reads.
	 *
	 * @return u32 register for MMU debug mode control.
	 */
	u32 (*mmu_debug_ctrl)(struct gk20a *g);

	/**
	 * @brief Gets register address to hold dummy page writes in debug mode.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function returns register containing the address of the dummy
	 * page write in debug mode on a fault.
	 *
	 * @return u32 register for dummy page write.
	 */
	u32 (*mmu_debug_wr)(struct gk20a *g);

	/**
	 * @brief Gets register address to hold dummy page reads in debug mode.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function returns register containing the address of the dummy
	 * page read in debug mode on a fault.
	 *
	 * @return u32 register for dummy page read.
	 */
	u32 (*mmu_debug_rd)(struct gk20a *g);

	/**
	 * @brief Dumps VPR information.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function error logs VPR information that the MMU fetches from
	 * memory controller. VPR info has following data:
	 *  - addr_lo displays the lower address of the VPR
	 *  - addr_hi displays the upper address of the VPR.
	 *  - cya_low and cya_hi displays CYA bits that controls the
	 *    trust level of each client.
	 */
	void (*dump_vpr_info)(struct gk20a *g);

	/**
	 * @brief Dumps WPR information.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function error logs WPR information that the MMU fetches from
	 * memory controller. WPR info has following data:
	 *  - allow_read displays the read access controls
	 *  - allow_write displays the write access controls
	 *  - wpr1_addr_lo displays the lower address of the WPR1
	 *  - wpr1_addr_hi displays the upper address of the WPR1.
	 *  - wpr2_addr_lo displays the lower address of the WPR2
	 *  - wpr2_addr_hi displays the upper address of the WPR2.
	 */
	void (*dump_wpr_info)(struct gk20a *g);

	/**
	 * @brief Trigger VPR fetch information.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function triggers VPR fetch and waits until VPR fetch is
	 * completed.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ETIMEDOUT if CPU  polling timeout during VPR fetch.
	 */
	int (*vpr_info_fetch)(struct gk20a *g);

	/**
	 * @brief Read WPR info.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param wpr_base [out]	WPR base address.
	 * @param wpr_size [out]	WPR region size.
	 *
	 * This function reads WPR info and returns WPR base address and
	 * WPR size.
	 */
	void (*read_wpr_info)(struct gk20a *g, u64 *wpr_base, u64 *wpr_size);

	/**
	 * @brief Invalidate TLB specific to pdb given.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 * @param pdb [in]	Pointer to pdb.
	 *
	 * This function invalidates all va addressed specified by pdb.
	 * It includes following steps:
	 *  - Wait until pri input fifo space available for tlb invalidation.
	 *  - Setup pdb address space for invalidation.
	 *  - Trigger invalidate of all va address and wait for completion.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ETIMEDOUT if CPU polling timeout during any tlb invalidate
	 *         operations.
	 */
	int (*tlb_invalidate)(struct gk20a *g, struct nvgpu_mem *pdb);

	/**
	 * @brief Setup mmu fault buffer.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param index [in]		Fault buffer index.
	 *
	 * This function configures mmu fault buffer for h/w and s/w use. When
	 * mmu fault occurs h/w will write fault info in the region set-up by
	 * s/w for s/w consumption.
	 */
	void (*fault_buf_configure_hw)(struct gk20a *g, u32 index);

	/**
	 * @brief Check if mmu fault buffer is enabled or not
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param index [in]		Fault buffer index. Valid index value:
	 *				NVGPU_MMU_FAULT_NONREPLAY_REG_INDX
	 *
	 * This function checks and returns whether fault buffer is enabled
	 * for specified index.
	 *
	 * @return true in case of fault buffer enabled, false otherwise.
	 */
	bool (*is_fault_buf_enabled)(struct gk20a *g, u32 index);

	/**
	 * @brief Setup mmu fault buffer state.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param index [in]		Fault buffer index. Valid index value:
	 *				NVGPU_MMU_FAULT_NONREPLAY_REG_INDX
	 * @param state [in]		NVGPU_MMU_FAULT_BUF_ENABLED or
	 *				NVGPU_MMU_FAULT_BUF_DISABLED
	 *
	 * This function sets up mmu fault buffer state.
	 *   NVGPU_MMU_FAULT_BUF_ENABLED : set the actual size of fault buffer.
	 *   NVGPU_MMU_FAULT_BUF_DISABLED : Clears fault buffer size.
	 */
	void (*fault_buf_set_state_hw)(struct gk20a *g, u32 index, u32 state);

	/**
	 * @brief Set atomic mode
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * This function sets platform atomic capability mode to rmw atomics
	 * for fb_mmu, fbhub and hshub(s). Also sets platform atomic capability
	 * non-coherent mode to l2 for fb_mmu.
	 *
	 * @return 0.
	 */
	int (*set_atomic_mode)(struct gk20a *g);

	struct gops_fb_ecc ecc;

	struct gops_fb_intr intr;

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	void (*set_mmu_page_size)(struct gk20a *g);

	u32 (*read_mmu_fault_buffer_get)(struct gk20a *g, u32 index);
	u32 (*read_mmu_fault_buffer_put)(struct gk20a *g, u32 index);
	u32 (*read_mmu_fault_buffer_size)(struct gk20a *g, u32 index);
	u32 (*read_mmu_fault_info)(struct gk20a *g);
	u32 (*read_mmu_fault_status)(struct gk20a *g);
	void (*write_mmu_fault_buffer_lo_hi)(struct gk20a *g, u32 index,
				u32 addr_lo, u32 addr_hi);
	void (*write_mmu_fault_buffer_get)(struct gk20a *g, u32 index,
				u32 reg_val);
	void (*write_mmu_fault_buffer_size)(struct gk20a *g, u32 index,
				u32 reg_val);
	void (*read_mmu_fault_addr_lo_hi)(struct gk20a *g,
				u32 *addr_lo, u32 *addr_hi);
	void (*read_mmu_fault_inst_lo_hi)(struct gk20a *g,
				u32 *inst_lo, u32 *inst_hi);
	void (*write_mmu_fault_status)(struct gk20a *g, u32 reg_val);

	struct nvgpu_hw_err_inject_info_desc * (*get_hubmmu_err_desc)
							(struct gk20a *g);

	u32 (*get_num_active_ltcs)(struct gk20a *g);

#ifdef CONFIG_NVGPU_COMPRESSION
	void (*cbc_configure)(struct gk20a *g, struct nvgpu_cbc *cbc);
	/**
	 * @brief Get cbc base and top alignment specs.
	 *
	 * @param g [in]	Pointer to GPU driver struct.
	 *
	 * The function calculates and returns required CBC base and top
	 * alignment values.
	 *
	 * @return None.
	 */
	void (*cbc_get_alignment)(struct gk20a *g,
		u64 *base_divisor, u64 *top_divisor);
	bool (*set_use_full_comp_tag_line)(struct gk20a *g);

	/*
	 * Check if comptagline mode is enabled.
	 * Legacy chips support only comptagline mode
	 */
	bool (*is_comptagline_mode_enabled)(struct gk20a *g);

	/*
	 * Compression tag line coverage. When mapping a compressible
	 * buffer, ctagline is increased when the virtual address
	 * crosses over the compression page boundary.
	 */
	u64 (*compression_page_size)(struct gk20a *g);

	/*
	 * Minimum page size that can be used for compressible kinds.
	 */
	unsigned int (*compressible_page_size)(struct gk20a *g);

	/*
	 * Compressible kind mappings: Mask for the virtual and physical
	 * address bits that must match.
	 */
	u64 (*compression_align_mask)(struct gk20a *g);
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
	bool (*is_debug_mode_enabled)(struct gk20a *g);
	void (*set_debug_mode)(struct gk20a *g, bool enable);
	void (*set_mmu_debug_mode)(struct gk20a *g, bool enable);
#endif

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	void (*handle_replayable_fault)(struct gk20a *g);
	int (*mmu_invalidate_replay)(struct gk20a *g,
						u32 invalidate_replay_val);
#endif

#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
#ifdef CONFIG_NVGPU_MIG
	int (*config_veid_smc_map)(struct gk20a *g, bool enable);
	int (*set_smc_eng_config)(struct gk20a *g, bool enable);
	int (*set_remote_swizid)(struct gk20a *g, bool enable);
#endif
	struct gops_fb_vab vab;
#endif

#ifdef CONFIG_NVGPU_DGPU
	/**
	 * @brief Initialize FBPA unit ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function allocates memory to track the ecc error counts
	 * for FBPA unit.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*fbpa_ecc_init)(struct gk20a *g);

	/**
	 * @brief Free FBPA unit ECC support.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * This function deallocates memory allocated for ecc error counts
	 * for FBPA unit.
	 */
	void (*fbpa_ecc_free)(struct gk20a *g);
	int (*mem_unlock)(struct gk20a *g);
	int (*init_nvlink)(struct gk20a *g);
	int (*enable_nvlink)(struct gk20a *g);
	size_t (*get_vidmem_size)(struct gk20a *g);
	int (*apply_pdb_cache_errata)(struct gk20a *g);
	int (*init_fbpa)(struct gk20a *g);
#endif
	void (*handle_fbpa_intr)(struct gk20a *g, u32 fbpa_id);
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

#endif /* NVGPU_GOPS_FB_H */

