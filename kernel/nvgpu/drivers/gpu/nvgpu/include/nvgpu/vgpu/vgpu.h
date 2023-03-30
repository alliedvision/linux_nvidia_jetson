/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_VGPU_H
#define NVGPU_VGPU_H

#include <nvgpu/types.h>
#include <nvgpu/utils.h>
#include <nvgpu/thread.h>
#include <nvgpu/log.h>
#include <nvgpu/lock.h>
#include <nvgpu/vgpu/tegra_vgpu.h>

struct gk20a;
struct vgpu_ecc_stat;

struct vgpu_priv_data {
	u64 virt_handle;
	struct nvgpu_thread intr_handler;
	struct tegra_vgpu_constants_params constants;
	struct vgpu_ecc_stat *ecc_stats;
	int ecc_stats_count;
	u32 num_freqs;
	unsigned long *freqs;
	struct nvgpu_mutex vgpu_clk_get_freq_lock;
	struct tegra_hv_ivm_cookie *css_cookie;
};

struct vgpu_priv_data *vgpu_get_priv_data(struct gk20a *g);

static inline u64 vgpu_get_handle(struct gk20a *g)
{
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);

	if (unlikely(!priv)) {
		nvgpu_err(g, "invalid vgpu_priv_data in %s", __func__);
		return INT_MAX;
	}

	return priv->virt_handle;
}

#endif /* NVGPU_VGPU_H */
