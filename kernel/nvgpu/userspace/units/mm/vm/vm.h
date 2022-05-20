/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef UNIT_VM_H
#define UNIT_VM_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-vm
 *  @{
 *
 * Software Unit Test Specification for mm.vm
 */

/**
 * Test specification for: test_map_buf
 *
 * Description: The VM unit shall be able to map a buffer of memory such that
 * the GPU may access that memory.
 *
 * Test Type: Feature, Boundary values
 *
 * Targets: nvgpu_vm_init, nvgpu_vm_get_buffers, nvgpu_big_pages_possible,
 * nvgpu_vm_area_alloc, nvgpu_vm_map, nvgpu_vm_find_mapped_buf_range,
 * nvgpu_vm_find_mapped_buf_less_than, nvgpu_get_pte, nvgpu_vm_put_buffers,
 * nvgpu_vm_unmap, nvgpu_vm_area_free, nvgpu_vm_put, nvgpu_vm_find_mapped_buf,
 * nvgpu_vm_area_find, nvgpu_vm_unmap_ref_internal, nvgpu_vm_unmap_system,
 * nvgpu_os_buf_get_size
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Ensure that no buffers are already mapped.
 * - Use nvgpu_big_pages_possible() to ensure big pages are possible in the
 *   current condition, and check its error handling.
 * - Map a 4KB buffer into the VM
 *   - Check that the resulting GPU virtual address is aligned to 4KB
 *   - Unmap the buffer
 * - Map a 64KB buffer into the VM
 *   - Check that the resulting GPU virtual address is aligned to 64KB
 *   - Unmap the buffer
 * - Check a few corner cases:
 *   - If big pages explicitly disabled at gk20a level, mapping should still
 *     succeed.
 *   - If big pages explicitly disabled at the VM level, mapping should still
 *     succeed.
 *   - If VAs are not unified, mapping should still succeed.
 *   - If IOMMU is disabled, mapping should still succeed.
 *   - If the buffer to map is smaller than the big page size, mapping should
 *     still succeed.
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_map_buf(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_map_buf_gpu_va
 *
 * Description: When a GPU virtual address is passed into the nvgpu_vm_map()
 * function the resulting GPU virtual address of the map does/does not match
 * the requested GPU virtual address.
 *
 * Test Type: Feature, Boundary values
 *
 * Targets: nvgpu_vm_init, nvgpu_vm_get_buffers, nvgpu_big_pages_possible,
 * nvgpu_vm_area_alloc, nvgpu_vm_map, nvgpu_vm_find_mapped_buf_range,
 * nvgpu_vm_find_mapped_buf_less_than, nvgpu_get_pte, nvgpu_vm_put_buffers,
 * nvgpu_vm_unmap, nvgpu_vm_area_free, nvgpu_vm_put,
 * nvgpu_gmmu_va_small_page_limit, nvgpu_vm_find_mapping
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Map a 4KB buffer into the VM at a specific GPU virtual address
 *   - Check that the resulting GPU virtual address is aligned to 4KB
 *   - Check that the resulting GPU VA is the same as the requested GPU VA
 *   - Unmap the buffer
 * - Ensure that requesting to map the same buffer at the same address still
 *   reports success and does not result in an actual extra mapping.
 * - Map a 64KB buffer into the VM at a specific GPU virtual address
 *   - Check that the resulting GPU virtual address is aligned to 64KB
 *   - Check that the resulting GPU VA is the same as the requested GPU VA
 *   - Unmap the buffer
 * - Check a few corner cases:
 *   - If VA is not unified, mapping should still succeed.
 *   - If VA is not unified, GPU_VA fixed below nvgpu_gmmu_va_small_page_limit,
 *     mapping should still succeed.
 *   - Do not allocate a VM area which will force an allocation with small
 *     pages.
 *   - Do not unmap the buffer so that nvgpu_vm_put can take care of the cleanup
 *     of both the mapping and the VM area.
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_map_buf_gpu_va(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_batch
 *
 * Description: This test exercises the VM unit's batch mode. Batch mode is used
 * to optimize cache flushes.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_vm_init, nvgpu_vm_mapping_batch_start, nvgpu_vm_area_alloc,
 * nvgpu_vm_map, nvgpu_vm_find_mapped_buf_range,
 * nvgpu_vm_find_mapped_buf_less_than, nvgpu_get_pte, nvgpu_vm_put_buffers,
 * nvgpu_vm_unmap, nvgpu_vm_area_free, nvgpu_vm_put,
 * nvgpu_vm_mapping_batch_finish, nvgpu_vm_mapping_batch_finish_locked
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Map/unmap 10 4KB buffers using batch mode
 * - Disable batch mode and verify cache flush counts
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_batch(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_init_error_paths
 *
 * Description: This test exercises the VM unit initialization code and covers
 * a number of error paths as well as reference counting mechanisms.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_vm_init, nvgpu_vm_do_init, nvgpu_vm_get, nvgpu_vm_put
 *
 * Input: None
 *
 * Steps:
 * - Create VM parameters with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Inject an error to make the allocation for struct vm_gk20a to fail and
 *   check that nvgpu_vm_init returns NULL.
 * - Set an invalid aperture size and ensure that nvgpu_vm_do_init asserts.
 * - Try to initialize a guest managed VM with kernel space and ensure that
 *   nvgpu_vm_do_init asserts.
 * - Set gk20a to report a virtual GPU and ensure that nvgpu_vm_do_init returns
 *   a failure when VM is guest managed.
 * - Ensure that nvgpu_vm_do_init reports a failure if the vm_as_alloc_share HAL
 *   fails.
 * - Set invalid parameters (low hole above the small page limit) and ensure
 *   that nvgpu_vm_do_init asserts.
 * - Inject an error to cause a failure within nvgpu_allocator_init for the user
 *   VMA and ensure that nvgpu_vm_do_init reports a failure.
 * - Inject an error to cause a failure within nvgpu_allocator_init for the
 *   kernel VMA and ensure that nvgpu_vm_do_init reports a failure.
 * - Set invalid parameters (low hole is 0 with a non unified VA) and ensure
 *   that nvgpu_vm_do_init reports a failure.
 * - Ensure that nvgpu_vm_do_init succeeds with big pages enabled and a non
 *   unified VA space.
 * - Ensure that nvgpu_vm_do_init succeeds with big pages disabled.
 * - Ensure that nvgpu_vm_do_init succeeds with no user VMA.
 * - Ensure that reference count of the VM is 1. Then increment it using
 *   nvgpu_vm_get and ensure it is 2. Decrement it with nvgpu_vm_put and ensure
 *   it is back to 1.
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_init_error_paths(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_map_buffer_error_cases
 *
 * Description: This test targets error handling within the nvgpu_vm_map API.
 *
 * Test Type: Error injection
 *
 * Targets: nvgpu_vm_init, nvgpu_vm_map, nvgpu_vm_put
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Ensure that if a non-fixed offset with userspace managed VM is in use,
 *   the nvgpu_vm_map API reports a failure.
 * - Ensure that if an invalid buffer size is provided,the nvgpu_vm_map API
 *   reports a failure.
 * - Inject a memory allocation error at allocation 0 and ensure that
 *   nvgpu_vm_map reports a failure of type ENOMEM. (This makes mapped_buffer
 *   memory allocation to fail.)
 * - Try to map an oversized buffer of 1GB and ensure that nvgpu_vm_map reports
 *   a failure of type EINVAL.
 * - Inject a memory allocation error at allocation 40 and ensure that
 *   nvgpu_vm_map reports a failure of type ENOMEM. (This makes the call to
 *   g->ops.mm.gmmu.map to fail.)
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_map_buffer_error_cases(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_map_buffer_security
 *
 * Description: This negative test targets mapping security within the
 * nvgpu_vm_map API.
 *
 * Test Type: Error injection, Security, Safety
 *
 * Targets: nvgpu_vm_init, nvgpu_vm_map, nvgpu_vm_put
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Obtain a buffer whose size would not fit in one set of PTEs that fit in
 *   the first allocated PD cache entry
 * - Prepare a fixed mapping address at the same address as the buffer size
 * - Check that a PTE that matches that virtual address is not valid to prepare
 *   for the check below.
 * - Inject a memory allocation error at allocation 6 and ensure that
 *   nvgpu_vm_map reports a failure of type ENOMEM. This makes the allocation
 *   of the second PD cache entry to fail.
 * - Check that a PTE that matches that virtual address is not valid. Because
 *   the PD allocation failed mid-update, the prior written entries must be
 *   undone.
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_map_buffer_security(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_map_buffer_security_error_cases
 *
 * Description: This negative test targets mapping security related error
 * conditions within the nvgpu_vm_map API.
 *
 * Test Type: Error injection, Security, Safety
 *
 * Targets: nvgpu_vm_init, nvgpu_vm_map, nvgpu_vm_put
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Obtain a buffer whose size would not fit in one set of PTEs that fit in
 *   the first allocated PD cache entry
 * - Prepare a fixed mapping address at the same address as the buffer size
 * - Inject a memory allocation error at allocation 6 to fail PD entries and a
 *   tlb invalidation error at next call to target the cache maint error after
 *   page table updates. Validate that nvgpu_vm_map reports -ENOMEM.
 * - Inject a tlb invalidation error at next call to target the cache maint
 *   error after successful page table updates, causing the entries to get
 *   unmapped. Validate that nvgpu_vm_map reports -ENOMEM.
 * - Check that a PTE that matches that virtual address is not valid.
 * - Inject two tlb invalidation errors at next calls to target the cache maint
 *   error after successful page table updates, causing the entries to get
 *   unmapped. Validate that nvgpu_vm_map reports -ENOMEM.
 * - Check that a PTE that matches that virtual address is not valid.
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_map_buffer_security_error_cases(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_nvgpu_vm_alloc_va
 *
 * Description: This test targets the nvgpu_vm_alloc_va API.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_vm_alloc_va, nvgpu_vm_free_va
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Set the VM as guest managed and call nvgpu_vm_alloc_va and ensure that it
 *   fails (returns NULL) as a guest managed VM cannot allocate VA spaces.
 * - Call nvgpu_vm_alloc_va with an invalid page size and ensure that it fails
 *   (returns NULL).
 * - Call nvgpu_vm_alloc_va with an unsupported page size index
 *   (GMMU_PAGE_SIZE_BIG) and ensure that it fails (returns NULL).
 * - Inject a memory allocation error at allocation 0 and ensure that
 *   nvgpu_vm_alloc_va reports a failure (returns NULL). (This makes the PTE
 *   memory allocation to fail.)
 * - Call nvgpu_vm_alloc_va with valid parameters and ensure that it succeeds
 *   (returns a non-NULL address.)
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_vm_alloc_va(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_vm_bind
 *
 * Description: This test targets the nvgpu_vm_bind_channel API.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_mm.vm_bind_channel, nvgpu_vm_bind_channel
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Create an empty nvgpu_channel instance.
 * - Call the nvgpu_vm_bind_channel with a NULL channel pointer and ensure it
 *   failed.
 * - Call the nvgpu_vm_bind_channel API with the empty channel instance.
 * - Ensure that after the call, the VM pointer in the nvgpu_channel structure
 *   points to the VM in use in the test.
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_vm_bind(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_vm_aspace_id
 *
 * Description: This test targets the vm_aspace_id API.
 *
 * Test Type: Feature
 *
 * Targets: vm_aspace_id
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Call vm_aspace_id on the test VM and ensure it reports an invalid value
 *   (-1) since the AS share is not set.
 * - Create an AS share structure and set its id to 0. Assign the AS share to
 *   the test VM.
 * - Call vm_aspace_id on the test VM and ensure it reports a value of 0.
 * - Uninitialize the VM
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_vm_aspace_id(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_vm_area_error_cases
 *
 * Description: This test targets the nvgpu_vm_area_validate_buffer and
 * nvgpu_vm_area_alloc APIs.
 *
 * Test Type: Feature, Error injection, Boundary values
 *
 * Targets: nvgpu_vm_area_validate_buffer, nvgpu_vm_area_alloc,
 * nvgpu_vm_area_free
 *
 * Input: None
 *
 * Steps:
 * - Initialize a VM with the following characteristics:
 *   - 64KB large page support enabled
 *   - Low hole size = 64MB
 *   - Address space size = 128GB
 *   - Kernel reserved space size = 4GB
 * - Try to validate a buffer of size 0 and ensure nvgpu_vm_area_validate_buffer
 *   returns -EINVAL.
 * - Try to validate a buffer where the address to be mapped is not aligned to
 *   the page size and ensure that it returns -EINVAL.
 * - Try to validate a buffer with a fixed address when the VM has no VM area
 *   and ensure that it returns -EINVAL.
 * - Try to create a VM area with an invalid page size and ensure that
 *   nvgpu_vm_area_alloc returns -EINVAL.
 * - Try to create a VM area with big page size in a VM that explicitly does
 *   not support big pages and ensure it returns -EINVAL.
 * - Inject memory allocation errors to target various allocations within
 *   the nvgpu_vm_area_alloc (or its subfunctions) and ensure that it returns
 *   the -ENOMEM value.
 * - Properly create a VM area and assign it to the test VM for the remainder
 *   of this test.
 * - Try to validate a buffer where the mapped size is bigger than the VA space
 *   and ensure it returns -EINVAL.
 * - Map a test buffer and ensure the mapping succeeded.
 * - Try to validate the same, already mapped, test buffer and ensure that it
 *   returns -EINVAL.
 * - Uninitialize the VM and VM area.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_vm_area_error_cases(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_gk20a_from_vm
 *
 * Description: Simple test to check gk20a_from_vm.
 *
 * Test Type: Feature
 *
 * Targets: gk20a_from_vm
 *
 * Input: None
 *
 * Steps:
 * - Create a test VM.
 * - Call gk20a_from_vm with the test vm pointer and ensure it returns a
 *   pointer on g.
 * - Uninitialize the VM.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gk20a_from_vm(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_insert_mapped_buf
 *
 * Description: Tests the logic of nvgpu_insert_mapped_buf
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_insert_mapped_buf, mapped_buffer_from_rbtree_node
 *
 * Input: None
 *
 * Steps:
 * - Create a test VM.
 * - Set an arbitrary test address.
 * - Search in the vm->mapped_buffers RBTree to ensure that the arbitrary test
 *   address has no mapped buffers already.
 * - Instantiate a struct nvgpu_mapped_buf and set its address to the arbitrary
 *   address with a size of 64KB and big pages.
 * - Call nvgpu_insert_mapped_buf on the struct nvgpu_mapped_buf.
 * - Search again the vm->mapped_buffers RBTree and ensure the buffer can be
 *   found.
 * - Uninitialize the VM.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_insert_mapped_buf(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_vm_pde_coverage_bit_count
 *
 * Description: Tests the logic of nvgpu_vm_pde_coverage_bit_count
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_vm_pde_coverage_bit_count
 *
 * Input: None
 *
 * Steps:
 * - Create a test VM.
 * - Call nvgpu_vm_pde_coverage_bit_count and ensure it returns the expected
 *   value of 21 (for GP10B and following chips).
 * - Uninitialize the VM.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_vm_pde_coverage_bit_count(struct unit_module *m, struct gk20a *g,
	void *args);
/** }@ */
#endif /* UNIT_VM_H */
