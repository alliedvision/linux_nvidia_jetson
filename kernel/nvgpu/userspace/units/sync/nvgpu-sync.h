/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef UNIT_NVGPU_SYNC_H
#define UNIT_NVGPU_SYNC_H

#include <nvgpu/types.h>

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-nvgpu-sync
 *  @{
 *
 * Software Unit Test Specification for nvgpu-sync
 */

/**
 * Test specification for: test_sync_init
 *
 * Description: Environment initialization for tests
 *
 * Test Type: Feature
 *
 * Input: None
 *
 * Steps:
 * - init FIFO register space.
 * - init HAL parameters for gv11b.
 * - init required for getting the sync ops initialized.
 * - init g->nvhost containing sync metadata.
 * - alloc memory for g->syncpt_mem.
 * - alloc memory for channel.
 * - alloc and init a VM for the channel.
 *
 * Output: Returns PASS if all the above steps are successful. FAIL otherwise.
 */
int test_sync_init(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_sync_deinit
 *
 * Description: Environment de-initialization for tests
 *
 * Test Type: Feature
 *
 * Input: test_sync_init run for this GPU
 *
 * Steps:
 * - put reference to VM put.
 * - free channel memory.
 * - free memory for g->syncpt_mem.
 * - free g->nvhost.
 * - clear FIFO register space.
 *
 * Output: Returns PASS if all the above steps are successful. FAIL otherwise.
 */
int test_sync_deinit(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_sync_create_destroy_sync
 *
 * Description: Branch coverage for nvgpu_channel_syncpt_sync_{create/destroy}
 * success.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_has_syncpoints,
 *	    nvgpu_nvhost_get_syncpt_client_managed,
 *	    gv11b_syncpt_alloc_buf,
 *	    set_syncpt_ro_map_gpu_va_locked,
 *	    gv11b_syncpt_free_buf,
 *	    nvgpu_channel_user_syncpt_destroy,
 *	    nvgpu_channel_user_syncpt_create
 *
 * Input: test_sync_init run for this GPU
 *
 * Steps:
 * - Check valid cases for nvgpu_channel_user_syncpt_create:
 *    - Pass a valid channel to the API and pass usermanaged = true.
 *    	- vm->syncpt_ro_map_gpu_va is not already allocated.
 *      - vm->syncpt_ro_map_gpu_va is already allocated.
 * - Check valid cases for nvgpu_channel_user_syncpt_destroy:
 *    - Set set_safe_state = true.
 *    - Set set_safe_state = false.
 *
 * Output: Returns PASS if a valid syncpoint is created. FAIL otherwise.
 */
int test_sync_create_destroy_sync(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_sync_set_safe_state
 *
 * Description: Branch coverage for nvgpu_channel_user_syncpt_set_safe_state
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_channel_user_syncpt_destroy,
 * 	    nvgpu_channel_user_syncpt_set_safe_state,
 * 	    nvgpu_channel_user_syncpt_create
 *
 * Input: test_sync_init run for this GPU
 *
 * Steps:
 * - Check if the syncpoint_value is incremented by a predefined fixed amount
 *
 * Output: Returns PASS if the above increment occurs correctly. FAIL otherwise.
 */
int test_sync_set_safe_state(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_sync_usermanaged_syncpt_apis
 *
 * Description: Branch coverage for nvgpu_channel_sync_syncpt_* APIs
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_channel_user_syncpt_destroy,
 *	    nvgpu_channel_user_syncpt_get_address,
 *	    nvgpu_channel_user_syncpt_get_id,
 *	    nvgpu_channel_user_syncpt_create
 *
 * Input: test_sync_init run for this GPU
 *
 * Steps:
 * - Call nvgpu_channel_user_syncpt_get_address
 * - Assert the correct values for the syncpt ID and the syncpt buffer GPUVA.
 *
 * Output: Returns PASS if the above steps are successful, FAIL otherwise.
 */
int test_sync_usermanaged_syncpt_apis(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: get_sync_ro_map
 *
 * Description: Branch coverage for get_sync_ro_map HAL
 *
 * Test Type: Feature
 *
 * Targets: gv11b_syncpt_get_sync_ro_map,
 * gops_sync.gops_sync_syncpt.get_sync_ro_map,
 * gops_sync_syncpt.get_sync_ro_map
 *
 * Input: test_sync_init run for this GPU
 *
 * Steps:
 * - Check if a call to get_sync_ro_map HAL succeeds
 *   - Check when vm->syncpt_ro_map_gpu_va is preallocated
 *   - Check when vm->syncpt_ro_map_gpu_va is not preallocated
 * - Check if a call to get_sync_ro_map HAL fails
 *    - Check when vm->syncpt_ro_map_gpu_va is not preallocated and
 *      call to MAP fails
 *
 * Output: Returns PASS if NULL is returned. FAIL otherwise.
 */
int test_sync_get_ro_map(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_sync_create_fail
 *
 * Description: Branch coverage for nvgpu_channel_user_syncpt_create failure
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_has_syncpoints,
 *	    nvgpu_nvhost_get_syncpt_client_managed,
 *	    gv11b_syncpt_alloc_buf,
 *	    set_syncpt_ro_map_gpu_va_locked,
 *	    gv11b_syncpt_free_buf
 *
 * Input: test_sync_init run for this GPU
 *
 * Steps:
 * - Check failure cases for nvgpu_channel_user_syncpt_create:
 *    - pass user_managed = FALSE.
 *    - allocation of memory for struct nvgpu_channel_sync_syncpt fails.
 *    - nvgpu_nvhost_get_syncpt_client_managed() returns invalid syncpoint i.e.
 *	syncpt_id returned = 0.
 *    - failure of alloc_buf() HAL
 *	- syncpt read-only map failure.
 *      - failure of allocation of memory for syncpt_buf.
 *      - failure to map the memory allocated for syncpt_buf.
 *
 * Output: Returns PASS if NULL is returned. FAIL otherwise.
 */
int test_sync_create_fail(struct unit_module *m, struct gk20a *g, void *args);

/** @} */

#endif /* UNIT_NVGPU_SYNC_H */
