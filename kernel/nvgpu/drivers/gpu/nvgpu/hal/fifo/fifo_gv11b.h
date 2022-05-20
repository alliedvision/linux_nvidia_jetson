/*
 * GV11B Fifo
 *
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FIFO_GV11B_H
#define NVGPU_FIFO_GV11B_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * FIFO HAL GV11B.
 */

struct gk20a;

int gv11b_init_fifo_reset_enable_hw(struct gk20a *g);
/**
 * @brief Update userd configuration and read FIFO chip settings.
 *
 * @param g [in]	The GPU driver struct.
 *                      - The function does not perform g parameter validation.
 *
 * Get fifo pointer from GPU pointer as g->fifo.
 * Set maximum number of VEIDs supported by chip in #nvgpu_fifo.max_subctx_count
 * using \ref gops_gr_init.get_max_subctx_count
 * "gops_gr_init.get_max_subctx_count()".
 * Set fifo_userd_writeback_timer_f() of register fifo_userd_writeback_r() to
 * fifo_userd_writeback_timer_100us_v().
 *
 * For gv11b, this function is mapped to \ref gops_fifo.init_fifo_setup_hw
 * "gops_fifo.init_fifo_setup_hw(g)".
 *
 * @retval 0 in case of success.
 */
int gv11b_init_fifo_setup_hw(struct gk20a *g);
u32 gv11b_fifo_mmu_fault_id_to_pbdma_id(struct gk20a *g, u32 mmu_fault_id);

#endif /* NVGPU_FIFO_GV11B_H */
