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

#ifndef NVGPU_ENABLED_H
#define NVGPU_ENABLED_H

struct gk20a;

#include <nvgpu/types.h>

/**
 * @defgroup enabled
 * @ingroup unit-common-utils
 * @{
 */

/*
 * Available flags that describe what's enabled and what's not in the GPU. Each
 * flag here is defined by it's offset in a bitmap.
 */

#define ENABLED_FLAGS							\
	DEFINE_FLAG(NVGPU_IS_FMODEL, "Running FMODEL Simulation"),	\
	DEFINE_FLAG(NVGPU_DRIVER_IS_DYING, "Driver is shutting down"),	\
	DEFINE_FLAG(NVGPU_GR_USE_DMA_FOR_FW_BOOTSTRAP,			\
		"Load Falcons using DMA because it's faster"),		\
	DEFINE_FLAG(NVGPU_FECS_TRACE_VA,				\
		"Use VAs for FECS Trace buffer (instead of PAs)"),	\
	DEFINE_FLAG(NVGPU_CAN_RAILGATE, "Can gate the power rail"),	\
	DEFINE_FLAG(NVGPU_KERNEL_IS_DYING, "OS is shutting down"),	\
	DEFINE_FLAG(NVGPU_FECS_TRACE_FEATURE_CONTROL,			\
		"Enable FECS Tracing"),					\
	/* ECC Flags */							\
	DEFINE_FLAG(NVGPU_ECC_ENABLED_SM_LRF, "SM LRF ECC is enabled"),	\
	DEFINE_FLAG(NVGPU_ECC_ENABLED_SM_SHM, "SM SHM ECC is enabled"),	\
	DEFINE_FLAG(NVGPU_ECC_ENABLED_TEX, "TEX ECC is enabled"),	\
	DEFINE_FLAG(NVGPU_ECC_ENABLED_LTC, "L2 ECC is enabled"),	\
	DEFINE_FLAG(NVGPU_ECC_ENABLED_SM_L1_DATA,			\
		"SM L1 DATA ECC is enabled"),				\
	DEFINE_FLAG(NVGPU_ECC_ENABLED_SM_L1_TAG,			\
		"SM L1 TAG ECC is enabled"),				\
	DEFINE_FLAG(NVGPU_ECC_ENABLED_SM_CBU, "SM CBU ECC is enabled"),	\
	DEFINE_FLAG(NVGPU_ECC_ENABLED_SM_ICACHE,			\
		"SM ICAHE ECC is enabled"),				\
	/* MM Flags */							\
	DEFINE_FLAG(NVGPU_MM_UNIFY_ADDRESS_SPACES,			\
		"Unified Memory address space"),			\
	DEFINE_FLAG(NVGPU_MM_HONORS_APERTURE,				\
		"false if vidmem aperture actually points to sysmem"),	\
	DEFINE_FLAG(NVGPU_MM_UNIFIED_MEMORY,				\
		"unified or split memory with separate vidmem?"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_USERSPACE_MANAGED_AS,			\
		"User-space managed address spaces support"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_IO_COHERENCE,				\
		"IO coherence support is available"),			\
	DEFINE_FLAG(NVGPU_SUPPORT_PARTIAL_MAPPINGS,			\
		"MAP_BUFFER_EX with partial mappings"),			\
	DEFINE_FLAG(NVGPU_SUPPORT_SPARSE_ALLOCS,			\
		"MAP_BUFFER_EX with sparse allocations"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_MAP_DIRECT_KIND_CTRL,			\
		"Direct PTE kind control is supported (map_buffer_ex)"),\
	DEFINE_FLAG(NVGPU_SUPPORT_MAP_BUFFER_BATCH,			\
		"Support batch mapping"),				\
	DEFINE_FLAG(NVGPU_SUPPORT_MAPPING_MODIFY,			\
		"Support mapping modify"),				\
	DEFINE_FLAG(NVGPU_SUPPORT_REMAP,				\
		"Support remap"),					\
	DEFINE_FLAG(NVGPU_USE_COHERENT_SYSMEM,				\
		"Use coherent aperture for sysmem"),			\
	DEFINE_FLAG(NVGPU_MM_USE_PHYSICAL_SG,				\
		"Use physical scatter tables instead of IOMMU"),	\
	DEFINE_FLAG(NVGPU_MM_BYPASSES_IOMMU,				\
		"Some chips (using nvlink) bypass the IOMMU on tegra"),	\
	DEFINE_FLAG(NVGPU_DISABLE_L3_SUPPORT,				\
		"Disable L3 alloc Bit of the physical address"),	\
	/* Host Flags */						\
	DEFINE_FLAG(NVGPU_HAS_SYNCPOINTS, "GPU has syncpoints"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_SYNC_FENCE_FDS,			\
		"sync fence FDs are available in, e.g., submit_gpfifo"),\
	DEFINE_FLAG(NVGPU_SUPPORT_CYCLE_STATS,				\
		"NVGPU_DBG_GPU_IOCTL_CYCLE_STATS is available"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_CYCLE_STATS_SNAPSHOT,			\
		"NVGPU_DBG_GPU_IOCTL_CYCLE_STATS_SNAPSHOT is available"),\
	DEFINE_FLAG(NVGPU_SUPPORT_TSG,					\
		"Both gpu driver and device support TSG"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_DETERMINISTIC_SUBMIT_NO_JOBTRACKING,	\
		"Support ast deterministic submits with no job tracking"),\
	DEFINE_FLAG(NVGPU_SUPPORT_DETERMINISTIC_SUBMIT_FULL,		\
		"Support Deterministic submits even with job tracking"),\
	DEFINE_FLAG(NVGPU_SUPPORT_RESCHEDULE_RUNLIST,			\
		"NVGPU_IOCTL_CHANNEL_RESCHEDULE_RUNLIST is available"),	\
									\
	DEFINE_FLAG(NVGPU_SUPPORT_DEVICE_EVENTS,			\
		"NVGPU_GPU_IOCTL_GET_EVENT_FD is available"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_FECS_CTXSW_TRACE,			\
		"FECS context switch tracing is available"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_DETERMINISTIC_OPTS,			\
		"NVGPU_GPU_IOCTL_SET_DETERMINISTIC_OPTS is available"),	\
	/* Security Flags */						\
	DEFINE_FLAG(NVGPU_SEC_SECUREGPCCS, "secure gpccs boot support"),\
	DEFINE_FLAG(NVGPU_SEC_PRIVSECURITY, "Priv Sec enabled"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_VPR, "VPR is supported"),		\
	/* Nvlink Flags */						\
	DEFINE_FLAG(NVGPU_SUPPORT_NVLINK, "Nvlink enabled"),		\
	/* PMU Flags */							\
	DEFINE_FLAG(NVGPU_PMU_PERFMON,					\
		"perfmon enabled or disabled for PMU"),			\
	DEFINE_FLAG(NVGPU_PMU_PSTATE, "PMU Pstates"),			\
	DEFINE_FLAG(NVGPU_PMU_ZBC_SAVE, "Save ZBC reglist"),		\
	DEFINE_FLAG(NVGPU_GPU_CAN_BLCG,					\
		"Supports Block Level Clock Gating"),			\
	DEFINE_FLAG(NVGPU_GPU_CAN_SLCG,					\
		"Supports Second Level Clock Gating"),			\
	DEFINE_FLAG(NVGPU_GPU_CAN_ELCG,					\
		"Supports Engine Level Clock Gating"),			\
	DEFINE_FLAG(NVGPU_SUPPORT_CLOCK_CONTROLS,			\
		"Clock control support"),				\
	DEFINE_FLAG(NVGPU_SUPPORT_GET_VOLTAGE,				\
		"NVGPU_GPU_IOCTL_GET_VOLTAGE is available"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_GET_CURRENT,				\
		"NVGPU_GPU_IOCTL_GET_CURRENT is available"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_GET_POWER,				\
		"NVGPU_GPU_IOCTL_GET_POWER is available"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_GET_TEMPERATURE,			\
		"NVGPU_GPU_IOCTL_GET_TEMPERATURE is available"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_SET_THERM_ALERT_LIMIT,		\
		"NVGPU_GPU_IOCTL_SET_THERM_ALERT_LIMIT is available"),	\
									\
	DEFINE_FLAG(NVGPU_PMU_RUN_PREOS,				\
		"whether to run PREOS binary on dGPUs"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_ASPM,					\
		"set if ASPM is enabled; only makes sense for PCI"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_TSG_SUBCONTEXTS,			\
		"subcontexts are available"),				\
	DEFINE_FLAG(NVGPU_SUPPORT_SCG,					\
		"Simultaneous Compute and Graphics (SCG) is available"),\
	DEFINE_FLAG(NVGPU_SUPPORT_SYNCPOINT_ADDRESS,			\
		"GPU_VA address of a syncpoint is supported"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_USER_SYNCPOINT,			\
		"Allocating per-channel syncpoint in user space is supported"),\
	DEFINE_FLAG(NVGPU_SUPPORT_USERMODE_SUBMIT,			\
		"USERMODE enable bit"),					\
	DEFINE_FLAG(NVGPU_SUPPORT_MULTIPLE_WPR, "Multiple WPR support"),\
	DEFINE_FLAG(NVGPU_SUPPORT_SEC2_RTOS, "SEC2 RTOS support"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_PMU_RTOS_FBQ, "PMU RTOS FBQ support"),\
	DEFINE_FLAG(NVGPU_SUPPORT_ZBC_STENCIL, "ZBC STENCIL support"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_PLATFORM_ATOMIC,			\
		"PLATFORM_ATOMIC support"),				\
	DEFINE_FLAG(NVGPU_SUPPORT_SEC2_VM, "SEC2 VM support"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_GSP_VM, "GSP VM support"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_PREEMPTION_GFXP,			\
		"GFXP preemption support"),				\
	DEFINE_FLAG(NVGPU_SUPPORT_PMU_SUPER_SURFACE, "PMU Super surface"),\
	DEFINE_FLAG(NVGPU_DRIVER_REDUCED_PROFILE,			\
		"Reduced profile of nvgpu driver"),			\
	DEFINE_FLAG(NVGPU_SUPPORT_SET_CTX_MMU_DEBUG_MODE,		\
		"NVGPU_GPU_IOCTL_SET_MMU_DEBUG_MODE is available"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_DGPU_THERMAL_ALERT,			\
		"DGPU Thermal Alert"),					\
	DEFINE_FLAG(NVGPU_SUPPORT_FAULT_RECOVERY,			\
		"Fault recovery support"),				\
	DEFINE_FLAG(NVGPU_DISABLE_SW_QUIESCE, "SW Quiesce"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_DGPU_PCIE_SCRIPT_EXECUTE,		\
		"DGPU PCIe Script Update"),				\
	DEFINE_FLAG(NVGPU_FMON_SUPPORT_ENABLE, "FMON feature Enable"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_COPY_ENGINE_DIVERSITY,		\
		"Copy Engine diversity enable bit"),			\
	DEFINE_FLAG(NVGPU_SUPPORT_SM_DIVERSITY,				\
		"SM diversity enable bit"),				\
	DEFINE_FLAG(NVGPU_ECC_ENABLED_SM_RAMS, "SM RAMS ECC is enabled"),\
	DEFINE_FLAG(NVGPU_SUPPORT_COMPRESSION, "Enable compression"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_SM_TTU, "SM TTU is enabled"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_POST_L2_COMPRESSION, "PLC Compression"),\
	DEFINE_FLAG(NVGPU_SUPPORT_MAP_ACCESS_TYPE,			\
		"GMMU map access type support"),			\
	DEFINE_FLAG(NVGPU_SUPPORT_2D, "2d operations support"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_3D, "3d graphics operations support"),\
	DEFINE_FLAG(NVGPU_SUPPORT_COMPUTE, "compute operations support"),\
	DEFINE_FLAG(NVGPU_SUPPORT_I2M, "inline methods support"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_ZBC, "zbc classes support"),		\
	DEFINE_FLAG(NVGPU_SUPPORT_MIG, "Multi Instance GPU support"),	\
	DEFINE_FLAG(NVGPU_SUPPORT_PROFILER_V2_DEVICE,			\
		"Profiler V2 device object support"),			\
	DEFINE_FLAG(NVGPU_SUPPORT_PROFILER_V2_CONTEXT,			\
		"Profiler V2 context object support"),			\
	DEFINE_FLAG(NVGPU_SUPPORT_SMPC_GLOBAL_MODE,			\
		"SMPC in global mode support"),				\
	DEFINE_FLAG(NVGPU_SUPPORT_GET_GR_CONTEXT,			\
		"Get gr context support"),				\
	DEFINE_FLAG(NVGPU_PMU_NEXT_CORE_ENABLED, "PMU NEXT CORE enabled"), \
	DEFINE_FLAG(NVGPU_ACR_NEXT_CORE_ENABLED,                        \
		"NEXT CORE availability for acr"),                      \
	DEFINE_FLAG(NVGPU_PKC_LS_SIG_ENABLED,                           \
		"PKC signature support"),                               \
	DEFINE_FLAG(NVGPU_ELPG_MS_ENABLED, "ELPG_MS support"),          \
	DEFINE_FLAG(NVGPU_L2_MAX_WAYS_EVICT_LAST_ENABLED,               \
			"Set L2 Max Ways Evict Last support"),		\
	DEFINE_FLAG(NVGPU_CLK_ARB_ENABLED, "CLK_ARB support"),          \
	DEFINE_FLAG(NVGPU_SUPPORT_VAB_ENABLED, "VAB feature supported"), \
	DEFINE_FLAG(NVGPU_SUPPORT_ROP_IN_GPC, "ROP is part of GPC"), \
	DEFINE_FLAG(NVGPU_SUPPORT_BUFFER_METADATA, "Buffer metadata support"), \
	DEFINE_FLAG(NVGPU_SUPPORT_NVS, "Domain scheduler support"), \
	DEFINE_FLAG(NVGPU_SUPPORT_TEGRA_RAW, \
			"TEGRA_RAW format support"), \
	DEFINE_FLAG(NVGPU_SUPPORT_EMULATE_MODE, \
			"Emulate mode support"), \
	DEFINE_FLAG(NVGPU_SUPPORT_PES_FS, \
			"PES Floorsweeping"), \
	DEFINE_FLAG(NVGPU_MAX_ENABLED_BITS, "Marks max number of flags"),

/**
 * Enumerated array of flags
 */
#define DEFINE_FLAG(flag, desc) flag
enum enum_enabled_flags {
	ENABLED_FLAGS
};
#undef DEFINE_FLAG

/**
 * @brief Check if the passed flag is enabled.
 *
 * Uses the function #nvgpu_test_bit() internally to check the status of the
 * bit position indicated by the parameter \a flag. The input parameter \a flag
 * and the variable \a enabled_flags in #gk20a are passed as parameters to the
 * function #nvgpu_test_bit().
 *
 * @param g [in] GPU super structure. Function does not perform any
 *		 validation of this parameter.
 * @param flag [in] Which flag to check. Function validates if this
 *		    parameter value is less than #NVGPU_MAX_ENABLED_BITS.
 *
 * @return Boolean value to indicate the status of the bit.
 *
 * @retval TRUE if the flag bit is enabled.
 * @retval FALSE if the flag bit is not enabled or if the flag value is greater
 * than or equal to #NVGPU_MAX_ENABLED_BITS.
 */
bool nvgpu_is_enabled(struct gk20a *g, u32 flag);

/**
 * @brief Set the state of a flag.
 *
 * Set the value of the passed \a flag to \a state.
 * This is a low level operation with lots of potential side effects.
 * Typically a bunch of calls to this early in the driver boot sequence makes
 * sense (as information is determined about the GPU at run time). Calling this
 * after GPU boot has completed is probably an incorrect thing to do.
 * Invokes the function #nvgpu_set_bit() or #nvgpu_clear_bit() based on the
 * value of \a state, and the parameters passed are \a flag and the variable
 * \a enabled_flags in #gk20a. The caller of this function needs to ensure that
 * the value of \a flag is less than #NVGPU_MAX_ENABLED_BITS.
 *
 * @param g [in] GPU super structure. Function does not perform any
 *		 validation of this parameter.
 * @param flag [in] Which flag to modify. Function validates if the value
 *		    of this parameter is less than #NVGPU_MAX_ENABLED_BITS.
 *		    If the value provided is not less than
 *		    #NVGPU_MAX_ENABLED_BITS, the function returns without
 *		    performing any operation.
 * @param state [in] The state to set the \a flag to. Function does not
 *		     perform any validation of this parameter.
 */
void nvgpu_set_enabled(struct gk20a *g, u32 flag, bool state);

/**
 * @brief Allocate the memory for the enabled flags.
 *
 * Allocates memory for the variable \a enabled_flags in #gk20a. Uses the
 * wrapper macro to invoke the function #nvgpu_kzalloc_impl() to allocate
 * the memory with \a g and the size required for allocation as parameters.
 * Variable \a enabled_flags in struct #gk20a is a pointer to unsigned long,
 * hence the size requested for allocation is equal to the size of number of
 * unsigned long variables required to hold #NVGPU_MAX_ENABLED_BITS.
 *
 * @param g [in] GPU super structure. Function does not perform any
 *		 validation of this parameter.
 *
 * @return 0 for success, < 0 for error.
 *
 * @retval 0 for success.
 * @retval -ENOMEM if fails to allocate the necessary memory.
 */
int nvgpu_init_enabled_flags(struct gk20a *g);

/**
 * @brief Free the memory for the enabled flags. Called during driver exit.
 *
 * Calls the wrapper macro to invoke the function #nvgpu_kfree_impl() with
 * \a g and variable \a enabled_flags in #gk20a as parameters.
 *
 * @param g [in] GPU super structure. Function does not perform any
 *		 validation of this parameter.
 */
void nvgpu_free_enabled_flags(struct gk20a *g);

/**
 * @brief Print enabled flags value.
 *
 * @param g [in] The GPU superstructure.
 */
void nvgpu_print_enabled_flags(struct gk20a *g);

/**
 * @}
 */
#endif /* NVGPU_ENABLED_H */
