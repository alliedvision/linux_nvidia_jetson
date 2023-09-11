/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_UTILS_H
#define NVGPU_GR_UTILS_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

/**
 * @file
 *
 * common.gr.utils unit interface
 */
struct gk20a;
struct nvgpu_gr;
struct nvgpu_gr_falcon;
struct nvgpu_gr_config;
struct nvgpu_gr_intr;
#ifdef CONFIG_NVGPU_DEBUGGER
struct nvgpu_gr_obj_ctx_golden_image;
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
struct nvgpu_gr_zbc;
struct nvgpu_gr_zcull;
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
struct nvgpu_gr_hwpm_map;
#endif
#ifdef CONFIG_NVGPU_FECS_TRACE
struct nvgpu_gr_global_ctx_buffer_desc;
#endif

/**
 * @brief Compute checksum.
 *
 * @param a [in]	First unsigned integer.
 * @param b [in]	Second unsigned integer.
 *
 * This function will calculate checksum of two unsigned integers and
 * return the result. This function is typically needed to calculate
 * checksum of falcon ucode boot binary.
 *
 * @return Checksum of two unsigned integers.
 */
u32 nvgpu_gr_checksum_u32(u32 a, u32 b);

/**
 * @brief Get GR falcon data struct pointer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function returns pointer to #nvgpu_gr_falcon structure.
 *
 * @return Pointer to GR falcon data struct.
 */
struct nvgpu_gr_falcon *nvgpu_gr_get_falcon_ptr(struct gk20a *g);

/**
 * @brief Get GR configuration struct pointer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function returns pointer to #nvgpu_gr_config structure.
 *
 * @return Pointer to GR configuration struct.
 */
struct nvgpu_gr_config *nvgpu_gr_get_config_ptr(struct gk20a *g);

/**
 * @brief Get GR configuration struct pointer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param gr_instance_id [in]	Gr instance Id.
 *
 * This function returns pointer to #nvgpu_gr_config structure.
 *
 * @return Pointer to GR configuration struct.
 */
struct nvgpu_gr_config *nvgpu_gr_get_gr_instance_config_ptr(struct gk20a *g,
		u32 gr_instance_id);

/**
 * @brief Get GR interrupt data struct pointer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function returns pointer to #nvgpu_gr_intr structure.
 *
 * @return Pointer to GR interrupt data struct.
 */
struct nvgpu_gr_intr *nvgpu_gr_get_intr_ptr(struct gk20a *g);

#ifdef CONFIG_NVGPU_NON_FUSA
/* gr variables */
u32 nvgpu_gr_get_override_ecc_val(struct gk20a *g);
void nvgpu_gr_override_ecc_val(struct nvgpu_gr *gr, u32 ecc_val);
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
struct nvgpu_gr_zcull *nvgpu_gr_get_zcull_ptr(struct gk20a *g);
struct nvgpu_gr_zbc *nvgpu_gr_get_zbc_ptr(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_CILP
u32 nvgpu_gr_get_cilp_preempt_pending_chid(struct gk20a *g);
void nvgpu_gr_clear_cilp_preempt_pending_chid(struct gk20a *g);
#endif
struct nvgpu_gr_obj_ctx_golden_image *nvgpu_gr_get_golden_image_ptr(
		struct gk20a *g);
bool nvgpu_gr_obj_ctx_golden_img_status(struct gk20a *g);
#ifdef CONFIG_NVGPU_DEBUGGER
struct nvgpu_gr_hwpm_map *nvgpu_gr_get_hwpm_map_ptr(struct gk20a *g);
void nvgpu_gr_reset_falcon_ptr(struct gk20a *g);
void nvgpu_gr_reset_golden_image_ptr(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_FECS_TRACE
struct nvgpu_gr_global_ctx_buffer_desc *nvgpu_gr_get_global_ctx_buffer_ptr(
							struct gk20a *g);
#endif

#endif /* NVGPU_GR_UTILS_H */
