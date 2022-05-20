/*
 * GK20A graphics fifo (gr host)
 *
 * Copyright (c) 2011-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_FIFO_GK20A_H
#define NVGPU_FIFO_GK20A_H

#include <nvgpu/types.h>

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
int gk20a_init_fifo_reset_enable_hw(struct gk20a *g);
int gk20a_init_fifo_setup_hw(struct gk20a *g);
void gk20a_fifo_bar1_snooping_disable(struct gk20a *g);
#endif
u32 gk20a_fifo_get_runlist_timeslice(struct gk20a *g);
u32 gk20a_fifo_get_pb_timeslice(struct gk20a *g);
bool gk20a_fifo_find_pbdma_for_runlist(struct gk20a *g,
				       u32 runlist_id, u32 *pbdma_mask);

#endif /* NVGPU_FIFO_GK20A_H */
