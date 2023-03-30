/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/cic_rm.h>

#include "cic_rm_priv.h"

int nvgpu_cic_rm_setup(struct gk20a *g)
{
	struct nvgpu_cic_rm *cic_rm;
	int err = 0;

	if (g->cic_rm != NULL) {
		cic_dbg(g, "CIC_RM already initialized");
		return 0;
	}

	cic_rm = nvgpu_kzalloc(g, sizeof(*cic_rm));
	if (cic_rm == NULL) {
		nvgpu_err(g, "Failed to allocate memory "
				"for struct nvgpu_cic_rm");
		return -ENOMEM;
	}

	g->cic_rm = cic_rm;
	cic_dbg(g, "CIC_RM unit initialization done.");
	return err;
}

int nvgpu_cic_rm_init_vars(struct gk20a *g)
{
	struct nvgpu_cic_rm *cic_rm;
	int err = 0;

	cic_rm = g->cic_rm;
	if (cic_rm == NULL) {
		nvgpu_err(g, "CIC_RM setup pending");
		return -EINVAL;
	}

	err = nvgpu_cond_init(&cic_rm->sw_irq_stall_last_handled_cond);
	if (err != 0) {
		nvgpu_err(g, "sw irq stall cond init failed\n");
		goto cleanup;
	}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
	err = nvgpu_cond_init(&cic_rm->sw_irq_nonstall_last_handled_cond);
	if (err != 0) {
		nvgpu_err(g, "sw irq nonstall cond init failed\n");
		goto cleanup_cond;
	}
#endif

	return 0;

#ifdef CONFIG_NVGPU_NONSTALL_INTR
cleanup_cond:
	nvgpu_cond_destroy(&cic_rm->sw_irq_stall_last_handled_cond);
#endif
cleanup:
	return err;
}

int nvgpu_cic_rm_deinit_vars(struct gk20a *g)
{
	struct nvgpu_cic_rm *cic_rm;

	cic_rm = g->cic_rm;

	if (cic_rm == NULL) {
		cic_dbg(g, "CIC_RM already removed");
		return 0;
	}

	nvgpu_cond_destroy(&cic_rm->sw_irq_stall_last_handled_cond);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	nvgpu_cond_destroy(&cic_rm->sw_irq_nonstall_last_handled_cond);
#endif

	return 0;
}

int nvgpu_cic_rm_remove(struct gk20a *g)
{
	struct nvgpu_cic_rm *cic_rm;

	cic_rm = g->cic_rm;

	if (cic_rm == NULL) {
		cic_dbg(g, "CIC_RM already removed");
		return 0;
	}

	nvgpu_kfree(g, cic_rm);
	g->cic_rm = NULL;

	return 0;
}
