/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_GRMGR_H
#define NVGPU_GOPS_GRMGR_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * GR MANAGER unit HAL interface
 *
 */
struct gk20a;

/**
 * GR MANAGER unit HAL operations
 *
 * @see gpu_ops
 */
struct gops_grmgr {
	/**
	 * @brief Initialize GR Manager unit.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*init_gr_manager)(struct gk20a *g);

	/**
	 * @brief Query GPU physical->logical gpc ids.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param num_gpc [in]		Number of GPCs.
	 * @param gpcs [out]		Pointer to GPC Id information struct.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*discover_gpc_ids)(struct gk20a *g, u32 num_gpc,
		struct nvgpu_gpc *gpcs);

	/**
	 * @brief Remove GR Manager unit.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*remove_gr_manager)(struct gk20a *g);

	/**
	 * @brief Get gpc group information.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 */
	void (*get_gpcgrp_count)(struct gk20a *g);

#if defined(CONFIG_NVGPU_HAL_NON_FUSA) && defined(CONFIG_NVGPU_MIG)
	u32 (*get_max_sys_pipes)(struct gk20a *g);
	const struct nvgpu_mig_gpu_instance_config* (*get_mig_config_ptr)(
			struct gk20a *g);
	u32 (*get_allowed_swizzid_size)(struct gk20a *g);
	int (*get_gpc_instance_gpcgrp_id)(struct gk20a *g,
			u32 gpu_instance_id, u32 gr_syspipe_id, u32 *gpcgrp_id);
	int (*get_mig_gpu_instance_config)(struct gk20a *g,
		const char **config_name, u32 *num_config_supported);
#endif
	void (*load_timestamp_prod)(struct gk20a *g);
};

#endif /* NVGPU_GOPS_GRMGR_H */
