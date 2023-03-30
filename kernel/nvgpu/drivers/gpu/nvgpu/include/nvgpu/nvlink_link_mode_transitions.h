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

#ifndef NVGPU_NVLINK_LINK_MODE_TRANSITIONS_H
#define NVGPU_NVLINK_LINK_MODE_TRANSITIONS_H

struct gk20a;


enum nvgpu_nvlink_link_mode {
	nvgpu_nvlink_link_off,
	nvgpu_nvlink_link_hs,
	nvgpu_nvlink_link_safe,
	nvgpu_nvlink_link_fault,
	nvgpu_nvlink_link_rcvy_ac,
	nvgpu_nvlink_link_rcvy_sw,
	nvgpu_nvlink_link_rcvy_rx,
	nvgpu_nvlink_link_detect,
	nvgpu_nvlink_link_reset,
	nvgpu_nvlink_link_enable_pm,
	nvgpu_nvlink_link_disable_pm,
	nvgpu_nvlink_link_disable_err_detect,
	nvgpu_nvlink_link_lane_disable,
	nvgpu_nvlink_link_lane_shutdown,
	nvgpu_nvlink_link__last,
};

enum nvgpu_nvlink_sublink_mode {
	nvgpu_nvlink_sublink_tx_hs,
	nvgpu_nvlink_sublink_tx_enable_pm,
	nvgpu_nvlink_sublink_tx_disable_pm,
	nvgpu_nvlink_sublink_tx_single_lane,
	nvgpu_nvlink_sublink_tx_safe,
	nvgpu_nvlink_sublink_tx_off,
	nvgpu_nvlink_sublink_tx_common,
	nvgpu_nvlink_sublink_tx_common_disable,
	nvgpu_nvlink_sublink_tx_data_ready,
	nvgpu_nvlink_sublink_tx_prbs_en,
	nvgpu_nvlink_sublink_tx__last,
	/* RX */
	nvgpu_nvlink_sublink_rx_hs,
	nvgpu_nvlink_sublink_rx_enable_pm,
	nvgpu_nvlink_sublink_rx_disable_pm,
	nvgpu_nvlink_sublink_rx_single_lane,
	nvgpu_nvlink_sublink_rx_safe,
	nvgpu_nvlink_sublink_rx_off,
	nvgpu_nvlink_sublink_rx_rxcal,
	nvgpu_nvlink_sublink_rx__last,
};

enum nvgpu_nvlink_link_mode nvgpu_nvlink_get_link_mode(struct gk20a *g);
u32 nvgpu_nvlink_get_link_state(struct gk20a *g);
int nvgpu_nvlink_set_link_mode(struct gk20a *g,
					enum nvgpu_nvlink_link_mode mode);
void nvgpu_nvlink_get_tx_sublink_state(struct gk20a *g, u32 *state);
void nvgpu_nvlink_get_rx_sublink_state(struct gk20a *g, u32 *state);
enum nvgpu_nvlink_sublink_mode nvgpu_nvlink_get_sublink_mode(struct gk20a *g,
					bool is_rx_sublink);
int nvgpu_nvlink_set_sublink_mode(struct gk20a *g, bool is_rx_sublink,
					enum nvgpu_nvlink_sublink_mode mode);

#endif /* NVGPU_NVLINK_LINK_MODE_TRANSITIONS_H */
