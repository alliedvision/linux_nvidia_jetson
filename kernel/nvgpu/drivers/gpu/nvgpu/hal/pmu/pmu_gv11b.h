/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef PMU_GV11B_H
#define PMU_GV11B_H

#include <nvgpu/nvgpu_err.h>
#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_hw_err_inject_info;
struct nvgpu_hw_err_inject_info_desc;

bool gv11b_pmu_is_debug_mode_en(struct gk20a *g);
void gv11b_pmu_flcn_setup_boot_config(struct gk20a *g);
void gv11b_setup_apertures(struct gk20a *g);
bool gv11b_pmu_is_engine_in_reset(struct gk20a *g);
void gv11b_pmu_engine_reset(struct gk20a *g, bool do_reset);
u32 gv11b_pmu_falcon_base_addr(void);
bool gv11b_is_pmu_supported(struct gk20a *g);
void gv11b_pmu_handle_ext_irq(struct gk20a *g, u32 intr0);

#ifdef CONFIG_NVGPU_LS_PMU
int gv11b_pmu_bootstrap(struct gk20a *g, struct nvgpu_pmu *pmu,
	u32 args_offset);
void gv11b_pmu_init_perfmon_counter(struct gk20a *g);
void gv11b_pmu_setup_elpg(struct gk20a *g);
void gv11b_secured_pmu_start(struct gk20a *g);
void gv11b_write_dmatrfbase(struct gk20a *g, u32 addr);
u32 gv11b_pmu_queue_head_r(u32 i);
u32 gv11b_pmu_queue_head__size_1_v(void);
u32 gv11b_pmu_queue_tail_r(u32 i);
u32 gv11b_pmu_queue_tail__size_1_v(void);
u32 gv11b_pmu_mutex__size_1_v(void);
#endif

void gv11b_clear_pmu_bar0_host_err_status(struct gk20a *g);
int gv11b_pmu_bar0_error_status(struct gk20a *g, u32 *bar0_status,
	u32 *etype);
bool gv11b_pmu_validate_mem_integrity(struct gk20a *g);

#ifdef CONFIG_NVGPU_INJECT_HWERR
struct nvgpu_hw_err_inject_info_desc * gv11b_pmu_intr_get_err_desc(struct gk20a *g);
void gv11b_pmu_inject_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info);

#endif /* CONFIG_NVGPU_INJECT_HWERR */

int gv11b_pmu_ecc_init(struct gk20a *g);
void gv11b_pmu_ecc_free(struct gk20a *g);
u32 gv11b_pmu_get_irqdest(struct gk20a *g);
void gv11b_pmu_enable_irq(struct nvgpu_pmu *pmu, bool enable);

#endif /* PMU_GV11B_H */
