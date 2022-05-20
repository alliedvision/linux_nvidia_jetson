/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GSP
#define NVGPU_GSP
struct gk20a;
struct nvgpu_gsp;

int nvgpu_gsp_sw_init(struct gk20a *g);
int nvgpu_gsp_bootstrap(struct gk20a *g);
void nvgpu_gsp_suspend(struct gk20a *g);
void nvgpu_gsp_sw_deinit(struct gk20a *g);
void nvgpu_gsp_isr_support(struct gk20a *g, bool enable);
void nvgpu_gsp_isr_mutex_aquire(struct gk20a *g);
void nvgpu_gsp_isr_mutex_release(struct gk20a *g);
bool nvgpu_gsp_is_isr_enable(struct gk20a *g);
u32 nvgpu_gsp_get_last_cmd_id(struct gk20a *g);
struct nvgpu_falcon *nvgpu_gsp_falcon_instance(struct gk20a *g);
int nvgpu_gsp_process_message(struct gk20a *g);
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
int nvgpu_gsp_stress_test_bootstrap(struct gk20a *g, bool start);
int nvgpu_gsp_stress_test_halt(struct gk20a *g, bool restart);
bool nvgpu_gsp_is_stress_test(struct gk20a *g);
#endif
#endif /* NVGPU_GSP */
