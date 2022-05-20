/*
 * GV11B L2 INTR
 *
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_LTC_INTR_GV11B
#define NVGPU_LTC_INTR_GV11B

#include <nvgpu/types.h>

struct gk20a;

void gv11b_ltc_intr_configure(struct gk20a *g);
void gv11b_ltc_intr_isr(struct gk20a *g, u32 ltc);
void gv11b_ltc_intr_en_illegal_compstat(struct gk20a *g, bool enable);

void gv11b_ltc_intr_init_counters(struct gk20a *g,
			u32 corrected_delta, u32 corrected_overflow,
			u32 uncorrected_delta, u32 uncorrected_overflow,
			u32 offset);
void gv11b_ltc_intr_handle_rstg_ecc_interrupts(struct gk20a *g,
			u32 ltc, u32 slice, u32 ecc_status, u32 ecc_addr);
void gv11b_ltc_intr_handle_tstg_ecc_interrupts(struct gk20a *g,
			u32 ltc, u32 slice, u32 ecc_status, u32 ecc_addr);
void gv11b_ltc_intr_handle_dstg_ecc_interrupts(struct gk20a *g,
			u32 ltc, u32 slice, u32 ecc_status, u32 dstg_ecc_addr,
			u32 ecc_addr);

#endif
