/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_GSP_TEST
#define NVGPU_GSP_TEST

#include <nvgpu/types.h>
struct gk20a;

u32 nvgpu_gsp_get_current_iteration(struct gk20a *g);
u32 nvgpu_gsp_get_current_test(struct gk20a *g);
bool nvgpu_gsp_get_test_fail_status(struct gk20a *g);
void nvgpu_gsp_set_test_fail_status(struct gk20a *g, bool val);
bool nvgpu_gsp_get_stress_test_start(struct gk20a *g);
int nvgpu_gsp_set_stress_test_start(struct gk20a *g, bool flag);
bool nvgpu_gsp_get_stress_test_load(struct gk20a *g);
int nvgpu_gsp_set_stress_test_load(struct gk20a *g, bool flag);
int nvgpu_gsp_stress_test_bootstrap(struct gk20a *g, bool start);
int nvgpu_gsp_stress_test_halt(struct gk20a *g, bool restart);
bool nvgpu_gsp_is_stress_test(struct gk20a *g);
int nvgpu_gsp_stress_test_sw_init(struct gk20a *g);
void nvgpu_gsp_test_sw_deinit(struct gk20a *g);
void nvgpu_gsp_write_test_sysmem_addr(struct gk20a *g);
void nvgpu_gsp_stest_isr(struct gk20a *g);
#endif /* NVGPU_GSP_TEST */
