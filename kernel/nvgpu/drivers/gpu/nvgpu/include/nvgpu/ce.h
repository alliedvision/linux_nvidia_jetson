/*
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_CE_H
#define NVGPU_CE_H

/**
 * @file
 * @page unit-ce Unit CE
 *
 * Overview
 * ========
 *
 * The CE unit is responsible for initializing the copy engines. The GPU has two
 * types of copy engines, GRCE and LCE.
 *
 * Data Structures
 * ===============
 * NA
 *
 * Static Design
 * =============
 *
 * CE Initialization
 * -----------------
 * The CE unit resets the copy engines at Master Control (MC) level and programs
 * the production clock gating and configuration options for copy engines.
 *
 * External APIs
 * -------------
 *   + nvgpu_ce_init_support()
 *
 * Dynamic Design
 * ==============
 *
 * At runtime, the CE stalling and non-stalling interrupts are handled through
 * CE unit hal interfaces. TSG initialization calls CE unit hal interface to
 * get the number of physical CEs.
 *
 * External APIs
 * -------------
 * Dynamic interfaces are HAL functions. They are documented here:
 *   + include/nvgpu/gops/ce.h
 */

struct gk20a;

/**
 * @brief Initialize the CE support.
 *
 * @param g [in] The GPU driver struct.
 *
 * This function is invoked during #nvgpu_finalize_poweron to initialize the
 * copy engines.
 *
 * Steps:
 * - Get the reset mask for all copy engines.
 * - Reset the engines at master control level through mc_enable_r.
 * - Load Second Level Clock Gating (SLCG) configuration for copy engine.
 * - Load Block Level Clock Gating (BLCG) configuration for copy engine.
 * - Set FORCE_BARRIERS_NPL configuration option for LCEs.
 * - Enable CE engines' stalling and non-stalling interrupts at MC level.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int nvgpu_ce_init_support(struct gk20a *g);

void nvgpu_ce_stall_isr(struct gk20a *g, u32 inst_id, u32 pri_base);
#endif /*NVGPU_CE_H*/
