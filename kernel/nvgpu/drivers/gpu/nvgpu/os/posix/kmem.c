/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <stdlib.h>

#include <nvgpu/bug.h>
#include <nvgpu/log.h>
#include <nvgpu/kmem.h>
#include <nvgpu/types.h>
#include <nvgpu/atomic.h>
#include <nvgpu/posix/kmem.h>
#include <nvgpu/posix/sizes.h>
#include <nvgpu/posix/bug.h>
#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
#include <nvgpu/posix/posix-fault-injection.h>
#endif

#ifdef __NVGPU_UNIT_TEST__
#define CACHE_NAME_LEN	128
#endif

struct nvgpu_kmem_cache {
	struct gk20a *g;
	size_t size;
#ifdef __NVGPU_UNIT_TEST__
	char name[CACHE_NAME_LEN];
#endif
};

#ifdef __NVGPU_UNIT_TEST__
static nvgpu_atomic_t kmem_cache_id;
#endif

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
struct nvgpu_posix_fault_inj *nvgpu_kmem_get_fault_injection(void)
{
	struct nvgpu_posix_fault_inj_container *c =
			nvgpu_posix_fault_injection_get_container();

	return &c->kmem_fi;
}
#endif

/*
 * kmem cache emulation: basically just do a regular malloc(). This is slower
 * but should not affect a user of kmem cache in the slightest bit.
 */
struct nvgpu_kmem_cache *nvgpu_kmem_cache_create(struct gk20a *g, size_t size)
{
	struct nvgpu_kmem_cache *cache;
#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
					nvgpu_kmem_get_fault_injection())) {
		return NULL;
	}
#endif
	cache = malloc(sizeof(struct nvgpu_kmem_cache));

	if (cache == NULL) {
		return NULL;
	}

	cache->g = g;
	cache->size = size;

#ifdef __NVGPU_UNIT_TEST__
	(void)snprintf(cache->name, sizeof(cache->name),
			"nvgpu-cache-0x%p-%lu-%d", g, size,
			nvgpu_atomic_inc_return(&kmem_cache_id));
#endif

	return cache;
}

void nvgpu_kmem_cache_destroy(struct nvgpu_kmem_cache *cache)
{
	free(cache);
}

void *nvgpu_kmem_cache_alloc(struct nvgpu_kmem_cache *cache)
{
	void *ptr;

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
					nvgpu_kmem_get_fault_injection())) {
		return NULL;
	}
#endif
	ptr = malloc(cache->size);

	if (ptr == NULL) {
		nvgpu_warn(NULL, "malloc returns NULL");
		return NULL;
	}

	return ptr;
}

void nvgpu_kmem_cache_free(struct nvgpu_kmem_cache *cache, void *ptr)
{
	free(ptr);
}

void *nvgpu_kmalloc_impl(struct gk20a *g, size_t size, void *ip)
{
	void *ptr;

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
					nvgpu_kmem_get_fault_injection())) {
		return NULL;
	}
#endif
	/*
	 * Since, the callers don't really need the memory region to be
	 * contiguous, use malloc here. If the need arises for this
	 * interface to return contiguous memory, we can explore using
	 * nvmap_page_alloc in qnx (i.e. using shm_open/shm_ctl_special/mmap
	 * calls).
	 */
	ptr = malloc(size);

	if (ptr == NULL) {
		nvgpu_warn(NULL, "malloc returns NULL");
		return NULL;
	}

	return ptr;
}

void *nvgpu_kzalloc_impl(struct gk20a *g, size_t size, void *ip)
{
	void *ptr;
	const size_t num = 1;

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
					nvgpu_kmem_get_fault_injection())) {
		return NULL;
	}
#endif
	ptr = calloc(num, size);

	if (ptr == NULL) {
		nvgpu_warn(NULL, "calloc returns NULL");
		return NULL;
	}

	return ptr;
}

void *nvgpu_kcalloc_impl(struct gk20a *g, size_t n, size_t size, void *ip)
{
	void *ptr;
	const size_t num = 1;

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
					nvgpu_kmem_get_fault_injection())) {
		return NULL;
	}
#endif
	ptr = calloc(num, (nvgpu_safe_mult_u64(n, size)));

	if (ptr == NULL) {
		nvgpu_warn(NULL, "calloc returns NULL");
		return NULL;
	}

	return ptr;
}

void *nvgpu_vmalloc_impl(struct gk20a *g, unsigned long size, void *ip)
{
	return nvgpu_kmalloc_impl(g, size, ip);
}

void *nvgpu_vzalloc_impl(struct gk20a *g, unsigned long size, void *ip)
{
	return nvgpu_kzalloc_impl(g, size, ip);
}

void nvgpu_kfree_impl(struct gk20a *g, void *addr)
{
	free(addr);
}

void nvgpu_vfree_impl(struct gk20a *g, void *addr)
{
	nvgpu_kfree_impl(g, addr);
}

void *nvgpu_big_alloc_impl(struct gk20a *g, size_t size, bool clear)
{
	if (clear) {
		return nvgpu_kzalloc(g, size);
	} else {
		return nvgpu_kmalloc(g, size);
	}
}

void nvgpu_big_free(struct gk20a *g, void *p)
{
	nvgpu_kfree_impl(g, p);
}

int nvgpu_kmem_init(struct gk20a *g)
{
#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
					nvgpu_kmem_get_fault_injection())) {
		return -ENOMEM;
	}
#endif
	/* Nothing to init at the moment. */
	return 0;
}

void nvgpu_kmem_fini(struct gk20a *g, int flags)
{
}
