/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/enabled.h>
#include <unit/unit.h>
#include <unit/io.h>
#include "posix-fault-injection.h"
#include "posix-fault-injection-kmem.h"
#include "posix-fault-injection-dma-alloc.h"

int test_fault_injection_init(struct unit_module *m,
			      struct gk20a *g, void *__args)
{
	nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, true);
	return UNIT_SUCCESS;
}

struct unit_module_test fault_injection_tests[] = {
	UNIT_TEST(fault_injection_init, test_fault_injection_init, NULL, 0),

	UNIT_TEST(init, test_kmem_init, NULL, 0),

	UNIT_TEST(cache_default, test_kmem_cache_fi_default, NULL, 0),
	UNIT_TEST(cache_enabled, test_kmem_cache_fi_enabled, NULL, 0),
	UNIT_TEST(cache_delayed_enable, test_kmem_cache_fi_delayed_enable,
		  NULL, 0),
	UNIT_TEST(cache_delayed_disable, test_kmem_cache_fi_delayed_disable,
		  NULL, 0),

	UNIT_TEST(kmalloc_default, test_kmem_kmalloc_fi_default, NULL, 0),
	UNIT_TEST(kmalloc_enabled, test_kmem_kmalloc_fi_enabled, NULL, 0),
	UNIT_TEST(kmalloc_delayed_enable,
		  test_kmem_kmalloc_fi_delayed_enable, NULL, 0),
	UNIT_TEST(kmalloc_delayed_disable,
		  test_kmem_kmalloc_fi_delayed_disable, NULL, 0),

	UNIT_TEST(dma_alloc_init, test_dma_alloc_init, NULL, 0),

	UNIT_TEST(dma_alloc_default, test_dma_alloc_fi_default, NULL, 0),
	UNIT_TEST(dma_alloc_enabled, test_dma_alloc_fi_enabled, NULL, 0),
	UNIT_TEST(dma_alloc_delayed_enable, test_dma_alloc_fi_delayed_enable,
		  NULL, 0),
};

UNIT_MODULE(fault_injection, fault_injection_tests, UNIT_PRIO_POSIX_TEST);
