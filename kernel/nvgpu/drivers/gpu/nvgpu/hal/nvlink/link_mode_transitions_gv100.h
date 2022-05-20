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

#ifndef LINK_MODE_TRANSITIONS_GV100_H
#define LINK_MODE_TRANSITIONS_GV100_H

#include <nvgpu/types.h>

struct gk20a;

#define NVLINK_PLL_ON_TIMEOUT_MS        30
#define NVLINK_SUBLINK_TIMEOUT_MS       200

int gv100_nvlink_setup_pll(struct gk20a *g, unsigned long link_mask);
int gv100_nvlink_data_ready_en(struct gk20a *g,
					unsigned long link_mask, bool sync);
enum nvgpu_nvlink_link_mode gv100_nvlink_get_link_mode(struct gk20a *g,
								u32 link_id);
u32 gv100_nvlink_get_link_state(struct gk20a *g, u32 link_id);
int gv100_nvlink_set_link_mode(struct gk20a *g, u32 link_id,
					enum nvgpu_nvlink_link_mode mode);
enum nvgpu_nvlink_sublink_mode gv100_nvlink_link_get_sublink_mode(
	struct gk20a *g, u32 link_id, bool is_rx_sublink);
u32 gv100_nvlink_link_get_tx_sublink_state(struct gk20a *g, u32 link_id);
u32 gv100_nvlink_link_get_rx_sublink_state(struct gk20a *g, u32 link_id);
int gv100_nvlink_link_set_sublink_mode(struct gk20a *g, u32 link_id,
	bool is_rx_sublink, enum nvgpu_nvlink_sublink_mode mode);
#endif
