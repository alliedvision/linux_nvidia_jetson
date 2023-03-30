/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef LTC_TU104_H
#define LTC_TU104_H

#include <nvgpu/types.h>

struct gk20a;

#ifdef CONFIG_NVGPU_DEBUGGER
/*
 * These macros are based on the TSTG registers present in the refmanual.
 * The first address of register of the TSTG block is:
 * NV_PLTS_TSTG_CFG_0 (0x90) and final register is: NV_PLTS_TSTG_REDUCE_REPLAY
 * (0x114).
 */
#define LTS_TSTG_BASE		(0x90U)
#define LTS_TSTG_EXTENT		(0x114U)
#endif

void ltc_tu104_init_fs_state(struct gk20a *g);
#ifdef CONFIG_NVGPU_DEBUGGER
u32 tu104_ltc_pri_is_lts_tstg_addr(struct gk20a *g, u32 addr);
int tu104_set_l2_sector_promotion(struct gk20a *g, struct nvgpu_tsg *tsg,
		u32 policy);
#endif

#endif /* LTC_TU104_H */
