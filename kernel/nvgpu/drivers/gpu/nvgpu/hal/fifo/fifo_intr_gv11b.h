/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FIFO_INTR_GV11B_H
#define NVGPU_FIFO_INTR_GV11B_H

#include <nvgpu/types.h>

/*
* ERROR_CODE_BAD_TSG indicates that Host encountered a badly formed TSG header
* or a badly formed channel type runlist entry in the runlist. This is typically
* caused by encountering a new TSG entry in the middle of a TSG definition.
* A channel type entry having wrong runqueue selector can also cause this.
* Additionally this error code can indicate when a channel is encountered on
* the runlist which is outside of a TSG.
*/
#define SCHED_ERROR_CODE_BAD_TSG           0x00000020U

struct gk20a;

void gv11b_fifo_intr_0_enable(struct gk20a *g, bool enable);
void gv11b_fifo_intr_0_isr(struct gk20a *g);

bool gv11b_fifo_handle_sched_error(struct gk20a *g);

void gv11b_fifo_intr_set_recover_mask(struct gk20a *g);
void gv11b_fifo_intr_unset_recover_mask(struct gk20a *g);

#endif /* NVGPU_FIFO_INTR_GV11B_H */
