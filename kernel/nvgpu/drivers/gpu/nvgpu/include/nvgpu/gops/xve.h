/*
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
#ifndef NVGPU_GOPS_XVE_H
#define NVGPU_GOPS_XVE_H

#ifdef CONFIG_NVGPU_DGPU
struct gops_xve {
	int (*get_speed)(struct gk20a *g, u32 *xve_link_speed);
	int (*set_speed)(struct gk20a *g, u32 xve_link_speed);
	void (*available_speeds)(struct gk20a *g, u32 *speed_mask);
	u32 (*xve_readl)(struct gk20a *g, u32 reg);
	void (*xve_writel)(struct gk20a *g, u32 reg, u32 val);
	void (*disable_aspm)(struct gk20a *g);
	void (*reset_gpu)(struct gk20a *g);
#if defined(CONFIG_PCI_MSI)
	void (*rearm_msi)(struct gk20a *g);
#endif
	void (*enable_shadow_rom)(struct gk20a *g);
	void (*disable_shadow_rom)(struct gk20a *g);
	u32 (*get_link_control_status)(struct gk20a *g);
	void (*devinit_deferred_settings)(struct gk20a *g);
};
#endif

#endif /* NVGPU_GOPS_XVE_H */
