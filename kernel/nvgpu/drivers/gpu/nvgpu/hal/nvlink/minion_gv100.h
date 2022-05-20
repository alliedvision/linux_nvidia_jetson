/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef MINION_GV100_H
#define MINION_GV100_H

#include <nvgpu/types.h>
enum nvgpu_nvlink_minion_dlcmd;
struct gk20a;

u32 gv100_nvlink_minion_base_addr(struct gk20a *g);
bool gv100_nvlink_minion_is_running(struct gk20a *g);
int gv100_nvlink_minion_is_boot_complete(struct gk20a *g, bool *boot_cmplte);
u32 gv100_nvlink_minion_get_dlcmd_ordinal(struct gk20a *g,
			enum nvgpu_nvlink_minion_dlcmd dlcmd);
int gv100_nvlink_minion_send_dlcmd(struct gk20a *g, u32 link_id,
			enum nvgpu_nvlink_minion_dlcmd dlcmd, bool sync);
void gv100_nvlink_minion_clear_intr(struct gk20a *g);
void gv100_nvlink_minion_init_intr(struct gk20a *g);
void gv100_nvlink_minion_enable_link_intr(struct gk20a *g, u32 link_id,
								bool enable);
void gv100_nvlink_minion_falcon_isr(struct gk20a *g);
void gv100_nvlink_minion_isr(struct gk20a *g);

#endif /* MINION_GV100_H */
