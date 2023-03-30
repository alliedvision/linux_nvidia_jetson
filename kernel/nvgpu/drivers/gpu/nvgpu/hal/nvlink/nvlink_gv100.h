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

#ifndef NVGPU_NVLINK_GV100_H
#define NVGPU_NVLINK_GV100_H

#define GV100_CONNECTED_LINK_MASK		0x8

struct gk20a;

u32 gv100_nvlink_get_link_reset_mask(struct gk20a *g);
int gv100_nvlink_discover_link(struct gk20a *g);
int gv100_nvlink_init(struct gk20a *g);
void gv100_nvlink_get_connected_link_mask(u32 *link_mask);
void gv100_nvlink_set_sw_errata(struct gk20a *g, u32 link_id);
int gv100_nvlink_configure_ac_coupling(struct gk20a *g,
				unsigned long mask, bool sync);
void gv100_nvlink_prog_alt_clk(struct gk20a *g);
void gv100_nvlink_clear_link_reset(struct gk20a *g, u32 link_id);
void gv100_nvlink_enable_link_an0(struct gk20a *g, u32 link_id);
#endif
