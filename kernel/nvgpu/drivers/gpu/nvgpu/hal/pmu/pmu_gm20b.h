/*
 * Copyright (c) 2014-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef PMU_GM20B_H
#define PMU_GM20B_H

#include <nvgpu/types.h>

struct gk20a;

void gm20b_pmu_setup_elpg(struct gk20a *g);
void pmu_dump_security_fuses_gm20b(struct gk20a *g);
void gm20b_write_dmatrfbase(struct gk20a *g, u32 addr);
bool gm20b_pmu_is_debug_mode_en(struct gk20a *g);
void gm20b_pmu_ns_setup_apertures(struct gk20a *g);
void gm20b_pmu_setup_apertures(struct gk20a *g);
void gm20b_pmu_flcn_setup_boot_config(struct gk20a *g);
void gm20b_secured_pmu_start(struct gk20a *g);
bool gm20b_is_pmu_supported(struct gk20a *g);
void gm20b_clear_pmu_bar0_host_err_status(struct gk20a *g);
u32 gm20b_pmu_queue_head_r(u32 i);
u32 gm20b_pmu_queue_head__size_1_v(void);
u32 gm20b_pmu_queue_tail_r(u32 i);
u32 gm20b_pmu_queue_tail__size_1_v(void);
u32 gm20b_pmu_mutex__size_1_v(void);

#endif /* PMU_GM20B_H */
