/*
 * GA100 FB
 *
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/timers.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/sizes.h>

#include "hal/fb/fb_ga100.h"
#include <nvgpu/hw/ga100/hw_fb_ga100.h>

#define SIZE_256K			(SZ_1K << 8U)

#define HW_SCRUB_TIMEOUT_DEFAULT	100 /* usec */
#define HW_SCRUB_TIMEOUT_MAX		2000000 /* usec */

void ga100_fb_init_fs_state(struct gk20a *g)
{
	u32 retries = HW_SCRUB_TIMEOUT_MAX / HW_SCRUB_TIMEOUT_DEFAULT;
	/* wait for memory to be accessible */
	do {
		u32 w = nvgpu_readl(g, fb_niso_scrub_status_r());
		if (fb_niso_scrub_status_flag_v(w) != 0U) {
			nvgpu_log_fn(g, "done");
			break;
		}
		nvgpu_udelay(HW_SCRUB_TIMEOUT_DEFAULT);
		--retries;
	} while (retries != 0);

}

#ifdef CONFIG_NVGPU_COMPRESSION
u64 ga100_fb_compression_page_size(struct gk20a *g)
{
	return SIZE_256K;
}

bool ga100_fb_is_comptagline_mode_enabled(struct gk20a *g)
{
	u32 reg = 0U;
	bool result = true;

	gk20a_busy_noresume(g);
	if (nvgpu_is_powered_off(g)) {
		goto done;
	}

	reg = nvgpu_readl(g, fb_mmu_hypervisor_ctl_r());

	result = (fb_mmu_hypervisor_ctl_force_cbc_raw_mode_v(reg) ==
		fb_mmu_hypervisor_ctl_force_cbc_raw_mode_disable_v());

done:
	gk20a_idle_nosuspend(g);
	return result;
}
#endif
