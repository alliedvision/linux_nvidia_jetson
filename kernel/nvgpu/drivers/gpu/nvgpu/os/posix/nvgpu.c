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

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include <nvgpu/bug.h>
#include <nvgpu/types.h>
#include <nvgpu/atomic.h>
#include <nvgpu/nvgpu_common.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/hal_init.h>
#include <nvgpu/os_sched.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/enabled.h>
#include <nvgpu/errata.h>

#include <nvgpu/posix/probe.h>
#include <nvgpu/posix/mock-regs.h>
#include <nvgpu/posix/io.h>

#include "os_posix.h"

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
#include <nvgpu/posix/posix-fault-injection.h>
#endif

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
struct nvgpu_posix_fault_inj *nvgpu_nvgpu_get_fault_injection(void)
{
	struct nvgpu_posix_fault_inj_container *c =
			nvgpu_posix_fault_injection_get_container();

	return &c->nvgpu_fi;
}
#endif

/*
 * Write callback. Forward the write access to the mock IO framework.
 */
static void writel_access_reg_fn(struct gk20a *g,
				 struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

/*
 * Read callback. Get the register value from the mock IO framework.
 */
static void readl_access_reg_fn(struct gk20a *g,
				struct nvgpu_reg_access *access)
{
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}

static struct nvgpu_posix_io_callbacks default_posix_reg_callbacks = {
	/* Write APIs all can use the same accessor. */
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,

	/* Likewise for the read APIs. */
	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,
};

#ifdef CONFIG_NVGPU_NON_FUSA
/*
 * Somewhat meaningless in userspace...
 */
void nvgpu_kernel_restart(void *cmd)
{
	(void)cmd;
	BUG();
}
#endif

void nvgpu_start_gpu_idle(struct gk20a *g)
{
	nvgpu_set_enabled(g, NVGPU_DRIVER_IS_DYING, true);
}

int nvgpu_enable_irqs(struct gk20a *g)
{
	(void)g;
	return 0;
}

void nvgpu_disable_irqs(struct gk20a *g)
{
	(void)g;
}

/*
 * We have no runtime PM stuff in userspace so these are really just noops.
 */
void gk20a_busy_noresume(struct gk20a *g)
{
	(void)g;
}

void gk20a_idle_nosuspend(struct gk20a *g)
{
	(void)g;
}

int gk20a_busy(struct gk20a *g)
{
#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
					nvgpu_nvgpu_get_fault_injection())) {
		return -ENODEV;
	}
#endif
#ifdef CONFIG_NVGPU_NON_FUSA
	nvgpu_atomic_inc(&g->usage_count);
#else
	(void)g;
#endif

	return 0;
}

void gk20a_idle(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_NON_FUSA
	nvgpu_atomic_dec(&g->usage_count);
#else
	(void)g;
#endif
}

static void nvgpu_posix_load_regs(struct gk20a *g)
{
	u32 i;
	int err;
	struct nvgpu_mock_iospace space;
	struct nvgpu_posix_io_reg_space *regs;

	for (i = 0; i < MOCK_REGS_LAST; i++) {
		err = nvgpu_get_mock_reglist(g, i, &space);
		if (err) {
			nvgpu_err(g, "Unknown IO regspace: %d; ignoring.", i);
			continue;
		}

		err = nvgpu_posix_io_add_reg_space(g, space.base, (u32)space.size);
		nvgpu_assert(err == 0);

		regs = nvgpu_posix_io_get_reg_space(g, space.base);
		nvgpu_assert(regs != NULL);

		if (space.data != NULL) {
			memcpy(regs->data, space.data, space.size);
		}
	}
}

static __thread struct gk20a *g_saved;
struct gk20a *nvgpu_posix_current_device(void)
{
	return g_saved;
}

/*
 * This function aims to initialize enough stuff to make unit testing worth
 * while. There are several interfaces and APIs that rely on the struct gk20a's
 * state in order to function: logging, for example, but there are many other
 * things, too.
 *
 * Initialize as much of that as possible here. This is meant to be equivalent
 * to the kernel space driver's probe function.
 */
struct gk20a *nvgpu_posix_probe(void)
{
	struct gk20a *g;
	struct nvgpu_os_posix *p;

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
					nvgpu_nvgpu_get_fault_injection())) {
		return NULL;
	}
#endif

	p = malloc(sizeof(*p));

	if (p == NULL) {
		return NULL;
	}

	(void) memset(p, 0, sizeof(*p));

	g = &p->g;
	g->log_mask = NVGPU_DEFAULT_DBG_MASK;
	g->mm.g = g;

	g_saved = g;

	g->regs = NVGPU_POSIX_REG_BAR0 << NVGPU_POSIX_REG_SHIFT;
	g->bar1 = NVGPU_POSIX_REG_BAR1 << NVGPU_POSIX_REG_SHIFT;
	g->usermode_regs = NVGPU_POSIX_REG_USERMODE << NVGPU_POSIX_REG_SHIFT;

	if (nvgpu_kmem_init(g) != 0) {
		goto fail_kmem;
	}

	if (nvgpu_init_errata_flags(g) != 0) {
		goto fail_errata_flags;
	}

	if (nvgpu_init_enabled_flags(g) != 0) {
		goto fail_enabled_flags;
	}

	/*
	 * Initialize a bunch of gv11b register values.
	 */
	nvgpu_posix_io_init_reg_space(g);
	nvgpu_posix_load_regs(g);

	/*
	 * Set up some default register IO callbacks that basically all
	 * unit tests will be OK with. Unit tests that wish to override this
	 * may do so.
	 *
	 * This needs to happen before the nvgpu_detect_chip() call below
	 * otherise we bug out when trying to do a register read.
	 */
	(void)nvgpu_posix_register_io(g, &default_posix_reg_callbacks);

	/*
	 * Detect chip based on the regs we filled above. Most unit tests
	 * will be fine with this; a few may have to undo a little bit of it
	 * in roder to fully test the nvgpu_detect_chip() function.
	 */
	nvgpu_assert(nvgpu_detect_chip(g) == 0);

	return g;

fail_enabled_flags:
	nvgpu_free_errata_flags(g);
fail_errata_flags:
	nvgpu_kmem_fini(g, 0);
fail_kmem:
	free(p);

	return NULL;
}

void nvgpu_posix_cleanup(struct gk20a *g)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	nvgpu_kmem_fini(g, 0);
	nvgpu_free_enabled_flags(g);
	nvgpu_free_errata_flags(g);
	free(p);
}
