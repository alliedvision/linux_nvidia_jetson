/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

/**
 * Here lie OS stubs that do not have an implementation yet nor has any plans
 * for an implementation.
 */

#include <nvgpu/ecc.h>
#include <nvgpu/debugger.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/types.h>

struct gk20a;

#ifdef CONFIG_NVGPU_DEBUGGER
void nvgpu_dbg_session_post_event(struct dbg_session_gk20a *dbg_s)
{
}
#endif

#ifdef CONFIG_NVGPU_SYSFS
int nvgpu_ecc_sysfs_init(struct gk20a *g)
{
	return 0;
}

void nvgpu_ecc_sysfs_remove(struct gk20a *g)
{
}
#endif

int nvgpu_cic_mon_report_err_safety_services(struct gk20a *g,
		u32 err_id)
{
	(void)g;
	(void)err_id;
	return 0;
}
