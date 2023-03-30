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

#ifndef NVGPU_ERRATA_H
#define NVGPU_ERRATA_H

struct gk20a;

#include <nvgpu/types.h>

/**
 * @defgroup errata
 * @ingroup unit-common-utils
 * @{
 */

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#define ERRATA_FLAGS_NEXT						\
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

/*
 * Available flags that describes an errata with details about where the issues
 * were first discovered. Each flag here is defined by it's offset
 * in a bitmap.
 */

#define ERRATA_FLAGS							\
	/* GM20B */							\
	DEFINE_ERRATA(NVGPU_ERRATA_MM_FORCE_128K_PMU_VM, "GM20B", "MM"),\
	DEFINE_ERRATA(NVGPU_ERRATA_1547668, "GM20B", "CG"),		\
	/* GP10B */							\
	DEFINE_ERRATA(NVGPU_ERRATA_LRF_ECC_OVERCOUNT, "GP10B", "GR ECC"),	\
	DEFINE_ERRATA(NVGPU_ERRATA_200391931, "GP10B", "GR Perf"),	\
	/* GV11B */							\
	DEFINE_ERRATA(NVGPU_ERRATA_2016608, "GV11B", "FIFO Runlist preempt"), \
	DEFINE_ERRATA(NVGPU_ERRATA_3524791, "GV11B", \
			"Non Virtualized GPC Exceptions"), \
	/* GV100 */							\
	DEFINE_ERRATA(NVGPU_ERRATA_1888034, "GV100", "Nvlink"),	\
	/* TU104 */							\
	DEFINE_ERRATA(NVGPU_ERRATA_INIT_PDB_CACHE, "TU104", "MM PDB"),	\
	DEFINE_ERRATA(NVGPU_ERRATA_FB_PDB_CACHE, "TU104", "FB PDB"),	\
	DEFINE_ERRATA(NVGPU_ERRATA_VBIOS_NVLINK_MASK, "TU104", "Nvlink VBIOS"),\
	/* GA100 */							\
	DEFINE_ERRATA(NVGPU_ERRATA_200601972, "GA100", "LTC TSTG"),     \
	DEFINE_ERRATA(NVGPU_ERRATA_2557724, "GA100", "L1TAG SURFACE CUT"), \
	/* GA10B */							\
	DEFINE_ERRATA(NVGPU_ERRATA_2969956, "GA10B", "FMODEL FB LTCS"),	\
	DEFINE_ERRATA(NVGPU_ERRATA_200677649, "GA10B", "UCODE"),	\
	DEFINE_ERRATA(NVGPU_ERRATA_3154076, "GA10B", "PROD VAL"),	\
	DEFINE_ERRATA(NVGPU_ERRATA_3288192, "GA10B", "L4 SCF NOT SUPPORTED"), \
	/* NvGPU Driver */						\
	DEFINE_ERRATA(NVGPU_ERRATA_SYNCPT_INVALID_ID_0, "SW", "Syncpt ID"),\
	DEFINE_ERRATA(NVGPU_MAX_ERRATA_BITS, "NA", "Marks max number of flags"),

/**
 * Enumerated array of flags
 */
#define DEFINE_ERRATA(flag, chip, desc) flag
enum enum_errata_flags {
	ERRATA_FLAGS_NEXT
	ERRATA_FLAGS
};
#undef DEFINE_ERRATA

/**
 * @brief Check if the passed flag is enabled.
 *
 * @param g [in]	The GPU.
 * @param flag [in]	Which flag to check.
 *
 * @return Boolean value to indicate the status of the bit.
 *
 * @retval TRUE if given errata is present.
 * @retval FALSE if given errata is absent.
 */
bool nvgpu_is_errata_present(struct gk20a *g, u32 flag);

/**
 * @brief Initialize and allocate memory for errata flags.
 *
 * @param g [in]	The GPU pointer.
 *
 * @return 0 for success, < 0 for error.
 *
 * @retval -ENOMEM if fails to allocate the necessary memory.
 */
int nvgpu_init_errata_flags(struct gk20a *g);

/**
 * @brief Free errata flags memory. Called during driver exit.
 *
 * @param g [in]	The GPU pointer.
 */
void nvgpu_free_errata_flags(struct gk20a *g);

/**
 * @brief Print errata flags value.
 *
 * @param g [in]	The GPU pointer.
 */
void nvgpu_print_errata_flags(struct gk20a *g);

/**
 * @brief Set state of a errata flag.
 *
 * @param g [in]	The GPU.
 * @param flag [in]	Flag index.
 * @param state [in]	The state to set the \a flag to.
 *
 * Set state of the given \a flag index to \a state.
 *
 * This is generally a somewhat low level operation with lots of potential
 * side effects. Be weary about where and when you use this. Typically a bunch
 * of calls to this early in the driver boot sequence makes sense (as
 * information is determined about the GPU at run time). Calling this in steady
 * state operation is probably an incorrect thing to do.
 */
void nvgpu_set_errata(struct gk20a *g, u32 flag, bool state);

/**
 * @}
 */
#endif /* NVGPU_ERRATA_H */
