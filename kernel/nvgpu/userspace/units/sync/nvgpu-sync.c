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
#include <stdlib.h>
#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/types.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/hal_init.h>
#include <nvgpu/dma.h>
#include <nvgpu/posix/io.h>
#include <os/posix/os_posix.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/posix-nvhost.h>
#include <nvgpu/channel.h>
#include <nvgpu/channel_user_syncpt.h>

#include "../fifo/nvgpu-fifo-common.h"
#include "../fifo/nvgpu-fifo-gv11b.h"
#include "nvgpu-sync.h"

#define NV_PMC_BOOT_0_ARCHITECTURE_GV110        (0x00000015 << \
						NVGPU_GPU_ARCHITECTURE_SHIFT)
#define NV_PMC_BOOT_0_IMPLEMENTATION_B          0xB

#define assert(cond)	unit_assert(cond, goto done)

static struct nvgpu_channel *ch;

static int init_syncpt_mem(struct unit_module *m, struct gk20a *g)
{
	u64 nr_pages;
	int err;
	if (!nvgpu_mem_is_valid(&g->syncpt_mem)) {
		nr_pages = U64(DIV_ROUND_UP(g->syncpt_unit_size,
					    NVGPU_CPU_PAGE_SIZE));
		err = nvgpu_mem_create_from_phys(g, &g->syncpt_mem,
				g->syncpt_unit_base, nr_pages);
		if (err != 0) {
			nvgpu_err(g, "Failed to create syncpt mem");
			return err;
		}
	}

	return 0;
}

static int init_channel_vm(struct unit_module *m, struct nvgpu_channel *ch)
{
	u64 low_hole, aperture_size;
	struct gk20a *g = ch->g;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct mm_gk20a *mm = &g->mm;

	p->mm_is_iommuable = true;
	/*
	 * Initialize one VM space for system memory to be used throughout this
	 * unit module.
	 * Values below are similar to those used in nvgpu_init_system_vm()
	 */
	low_hole = SZ_4K * 16UL;
	aperture_size = GK20A_PMU_VA_SIZE;

	mm->pmu.aperture_size = GK20A_PMU_VA_SIZE;
	g->ops.mm.get_default_va_sizes(NULL, &mm->channel.user_size,
		&mm->channel.kernel_size);

	mm->pmu.vm = nvgpu_vm_init(g,
				   g->ops.mm.gmmu.get_default_big_page_size(),
				   low_hole,
				   0ULL,
				   nvgpu_safe_sub_u64(aperture_size, low_hole),
				   0ULL,
				   true,
				   false,
				   false,
				   "system");
	if (mm->pmu.vm == NULL) {
		unit_return_fail(m, "nvgpu_vm_init failed\n");
	}

	ch->vm = mm->pmu.vm;

	mm->bar1.aperture_size = bar1_aperture_size_mb_gk20a() << 20;
	mm->bar1.vm = nvgpu_vm_init(g,
			g->ops.mm.gmmu.get_default_big_page_size(),
			low_hole,
			0ULL,
			nvgpu_safe_sub_u64(mm->bar1.aperture_size, low_hole),
			0ULL,
			true, false, false,
			"bar1");
	if (mm->bar1.vm == NULL) {
		unit_return_fail(m, "nvgpu_vm_init failed\n");
	}

	if (nvgpu_pd_cache_init(g) != 0) {
		unit_return_fail(m, "pd cache initialization failed\n");
	}

	return UNIT_SUCCESS;
}

int test_sync_init(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = 0;

	test_fifo_setup_gv11b_reg_space(m, g);

	nvgpu_set_enabled(g, NVGPU_HAS_SYNCPOINTS, true);

	/*
	 * HAL init required for getting
	 * the sync ops initialized.
	 */
	ret = nvgpu_init_hal(g);
	if (ret != 0) {
		return -ENODEV;
	}

	/*
	 * Init g->nvhost containing sync metadata
	 */
	ret = nvgpu_get_nvhost_dev(g);
	if (ret != 0) {
		unit_return_fail(m, "nvgpu_sync_early_init failed\n");
	}

	/*
	 * Alloc memory for g->syncpt_mem
	 */
	ret = init_syncpt_mem(m, g);
	if (ret != 0) {
		nvgpu_free_nvhost_dev(g);
		unit_return_fail(m, "sync mem allocation failure");
	}

	/*
	 * Alloc memory for channel
	 */
	ch = nvgpu_kzalloc(g, sizeof(struct nvgpu_channel));
	if (ch == NULL) {
		nvgpu_free_nvhost_dev(g);
		unit_return_fail(m, "sync channel creation failure");
	}

	ch->g = g;

	/*
	 * Alloc and Init a VM for the channel
	 */
	ret = init_channel_vm(m, ch);
	if (ret != 0) {
		nvgpu_kfree(g, ch);
		nvgpu_free_nvhost_dev(g);
		unit_return_fail(m, "sync channel vm init failure");
	}

	return UNIT_SUCCESS;
}

int test_sync_create_destroy_sync(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_channel_user_syncpt *sync = NULL;
	u32 syncpt_value = 0U;
	int ret = UNIT_FAIL;

	sync = nvgpu_channel_user_syncpt_create(ch);
	if (sync == NULL) {
		unit_return_fail(m, "unexpected failure in creating sync points");
	}

	syncpt_value = g->nvhost->syncpt_value;

	unit_info(m, "Syncpt ID: %u, Syncpt Value: %u\n",
		g->nvhost->syncpt_id, syncpt_value);

	assert((g->nvhost->syncpt_id > 0U) &&
		(g->nvhost->syncpt_id <= NUM_HW_PTS));

	assert(syncpt_value < (UINT_MAX - SYNCPT_SAFE_STATE_INCR));

	nvgpu_channel_user_syncpt_destroy(sync);

	sync = NULL;

	ret = UNIT_SUCCESS;

done:
	if (sync != NULL)
		nvgpu_channel_user_syncpt_destroy(sync);

	if (nvgpu_mem_is_valid(&g->syncpt_mem) &&
			ch->vm->syncpt_ro_map_gpu_va != 0ULL) {
		nvgpu_gmmu_unmap_addr(ch->vm, &g->syncpt_mem,
				ch->vm->syncpt_ro_map_gpu_va);
		ch->vm->syncpt_ro_map_gpu_va = 0ULL;
	}

	return ret;
}

int test_sync_set_safe_state(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_channel_user_syncpt *sync = NULL;

	u32 syncpt_value, syncpt_id;
	u32 syncpt_safe_state_val;

	int ret = UNIT_FAIL;

	sync = nvgpu_channel_user_syncpt_create(ch);
	if (sync == NULL) {
		unit_return_fail(m, "unexpected failure in creating sync points");
	}

	syncpt_id = g->nvhost->syncpt_id;
	syncpt_value = g->nvhost->syncpt_value;

	unit_info(m, "Syncpt ID: %u, Syncpt Value: %u\n",
		syncpt_id, syncpt_value);

	assert((syncpt_id > 0U) && (syncpt_id <= NUM_HW_PTS));

	assert(syncpt_value < (UINT_MAX - SYNCPT_SAFE_STATE_INCR));

	nvgpu_channel_user_syncpt_set_safe_state(sync);

	syncpt_safe_state_val = g->nvhost->syncpt_value;

	if ((syncpt_safe_state_val - syncpt_value) != SYNCPT_SAFE_STATE_INCR) {
		unit_return_fail(m, "unexpected increment value for safe state");
	}

	nvgpu_channel_user_syncpt_destroy(sync);

	sync = NULL;

	ret = UNIT_SUCCESS;

done:
	if (sync != NULL)
		nvgpu_channel_user_syncpt_destroy(sync);

	if (nvgpu_mem_is_valid(&g->syncpt_mem) &&
			ch->vm->syncpt_ro_map_gpu_va != 0ULL) {
		nvgpu_gmmu_unmap_addr(ch->vm, &g->syncpt_mem,
				ch->vm->syncpt_ro_map_gpu_va);
		ch->vm->syncpt_ro_map_gpu_va = 0ULL;
	}

	return ret;
}

int test_sync_usermanaged_syncpt_apis(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_channel_user_syncpt *user_sync = NULL;

	u32 syncpt_id = 0U;
	u64 syncpt_buf_addr = 0ULL;

	int ret = UNIT_FAIL;

	user_sync = nvgpu_channel_user_syncpt_create(ch);
	if (user_sync == NULL) {
		unit_return_fail(m, "unexpected failure in creating user sync points");
	}

	syncpt_id = nvgpu_channel_user_syncpt_get_id(user_sync);
	assert((syncpt_id > 0U) && (syncpt_id <= NUM_HW_PTS));

	syncpt_buf_addr = nvgpu_channel_user_syncpt_get_address(user_sync);
	assert(syncpt_buf_addr > 0ULL);

	unit_info(m, "Syncpt ID: %u, Syncpt Shim GPU VA: %llu\n",
		syncpt_id, syncpt_buf_addr);

	nvgpu_channel_user_syncpt_destroy(user_sync);

	user_sync = NULL;

	ret = UNIT_SUCCESS;

done:
	if (user_sync != NULL)
		nvgpu_channel_user_syncpt_destroy(user_sync);

	if (nvgpu_mem_is_valid(&g->syncpt_mem) &&
			ch->vm->syncpt_ro_map_gpu_va != 0ULL) {
		nvgpu_gmmu_unmap_addr(ch->vm, &g->syncpt_mem,
				ch->vm->syncpt_ro_map_gpu_va);
		ch->vm->syncpt_ro_map_gpu_va = 0ULL;
	}

	return ret;
}

#define F_SYNC_GET_RO_MAP_PRE_ALLOCATED     0
#define F_SYNC_GET_RO_MAP                   1
#define F_SYNC_GET_RO_MAP_MAX               1

static const char *f_sync_get_ro_map[] = {
	"sync_get_ro_map_preallocated",
	"sync_get_ro_map",
};

static void syncpt_ro_map_gpu_va_clear(struct gk20a *g, struct nvgpu_channel *ch)
{
		if (nvgpu_mem_is_valid(&g->syncpt_mem) &&
				ch->vm->syncpt_ro_map_gpu_va != 0ULL) {
			nvgpu_gmmu_unmap_addr(ch->vm, &g->syncpt_mem,
					ch->vm->syncpt_ro_map_gpu_va);
			ch->vm->syncpt_ro_map_gpu_va = 0ULL;
		} else if (ch->vm->syncpt_ro_map_gpu_va != 0ULL) {
			ch->vm->syncpt_ro_map_gpu_va = 0ULL;
		} else {
			(void) memset(&g->syncpt_mem, 0, sizeof(struct nvgpu_mem));
		}
}

int test_sync_get_ro_map(struct unit_module *m, struct gk20a *g, void *args)
{
	u64 base_gpuva = 0U;
	u32 sync_size = 0U;
	u32 num_syncpoints = 0U;
	u32 branches;

	int err = 0;
	int ret = UNIT_FAIL;

	for (branches = 0U; branches <= F_SYNC_GET_RO_MAP_MAX; branches++) {
		if (branches == F_SYNC_GET_RO_MAP_PRE_ALLOCATED) {
			ch->vm->syncpt_ro_map_gpu_va = nvgpu_gmmu_map_partial(ch->vm,
					&g->syncpt_mem, g->syncpt_unit_size,
					0, gk20a_mem_flag_read_only,
					false, APERTURE_SYSMEM);
			if (ch->vm->syncpt_ro_map_gpu_va == 0U) {
				unit_return_fail(m, "Unable to preallocate mapping");
			}
		} else if (branches == F_SYNC_GET_RO_MAP) {
			ch->vm->syncpt_ro_map_gpu_va = 0U;
		}

		unit_info(m, "%s branch: %s\n", __func__, f_sync_get_ro_map[branches]);

		err = g->ops.sync.syncpt.get_sync_ro_map(ch->vm,
				&base_gpuva, &sync_size, &num_syncpoints);

		if (branches < F_SYNC_GET_RO_MAP_MAX) {
			if(err != 0) {
				unit_return_fail(m,
					"unexpected failure in get_sync_ro_map");
			} else {
				ret = UNIT_SUCCESS;
			}

			assert(base_gpuva > 0ULL);
			assert(sync_size > 0U);

			unit_info(m, "Syncpt Shim GPU VA: %llu\n", base_gpuva);

		}

		syncpt_ro_map_gpu_va_clear(g, ch);

		base_gpuva = 0U;
		sync_size = 0U;
	}
done:
	syncpt_ro_map_gpu_va_clear(g, ch);

	return ret;
}

#define F_SYNC_SYNCPT_ALLOC_FAILED		1
#define F_SYNC_STRADD_FAIL			3
#define F_SYNC_NVHOST_CLIENT_MANAGED_FAIL	4
#define F_SYNC_MEM_CREATE_PHYS_FAIL		5
#define F_SYNC_BUF_MAP_FAIL			6
#define F_SYNC_FAIL_LAST			7

static const char *f_syncpt_open[] = {
	"global_disable_syncpt",
	"syncpt_alloc_failed",
	"syncpt_user_managed_false",
	"syncpt_stradd_fail",
	"syncpt_get_client_managed_fail",
	"syncpt_create_phys_mem_fail",
	"syncpt_buf_map_fail",
};

/*
 * syncpt name is 32 chars big, including nul byte; the chid is 1 byte here
 * ("0") and nvgpu_strnadd_u32 needs that plus nul byte. A "_" is added after
 * g->name, so this would break just at the nul byte.
 */
#define FAIL_G_NAME_STR	"123456789012345678901234567890"

static void clear_test_params(struct gk20a *g,
		bool *fault_injection_enabled, u32 branch,
		struct nvgpu_posix_fault_inj *kmem_fi)
{
	if (*fault_injection_enabled) {
		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
		*fault_injection_enabled = false;
	}

	syncpt_ro_map_gpu_va_clear(g, ch);
}

int test_sync_create_fail(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_channel_user_syncpt *sync = NULL;
	struct nvgpu_posix_fault_inj *kmem_fi;
	u32 branches;
	bool fault_injection_enabled = false;
	int ret = UNIT_FAIL;
	const char *g_name = g->name;

	kmem_fi = nvgpu_kmem_get_fault_injection();

	ch->vm->syncpt_ro_map_gpu_va = 0U;

	for (branches = 0U; branches < F_SYNC_FAIL_LAST; branches++) {

		u32 syncpt_id, syncpt_value;

		/*
		 * This is normally not cleared when a syncpt's last ref
		 * is removed. Hence, explicitely zero it after every failure
		 */
		g->nvhost->syncpt_id = 0U;

		if (branches == F_SYNC_SYNCPT_ALLOC_FAILED) {
			/* fail first kzalloc call */
			nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
			fault_injection_enabled = true;
		} else if (branches == F_SYNC_STRADD_FAIL) {
			/*
			 * fill the entire buffer resulting in
			 * failure in nvgpu_strnadd_u32
			 */
			g->name = FAIL_G_NAME_STR;
		} else if (branches == F_SYNC_NVHOST_CLIENT_MANAGED_FAIL) {
			g->nvhost->syncpt_id = 20U; /* arbitary id */
		} else if (branches == F_SYNC_MEM_CREATE_PHYS_FAIL) {
			/*
			 * bypass map of g->syncpt_mem and fail at
			 * nvgpu_mem_create_from_phys after first kzalloc.
			 */
			ch->vm->syncpt_ro_map_gpu_va = 0x1000ULL;
			nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
			fault_injection_enabled = true;
		} else if (branches == F_SYNC_BUF_MAP_FAIL) {
			/*
			 * bypass map of g->syncpt_mem and fail at
			 * nvgpu_gmmu_map after first kzalloc and then two
			 * consequtive calls to kmalloc
			 */
			ch->vm->syncpt_ro_map_gpu_va = 1ULL;
			nvgpu_posix_enable_fault_injection(kmem_fi, true, 2);
			fault_injection_enabled = true;
		} else {
			continue;
		}

		unit_info(m, "%s branch: %s\n", __func__, f_syncpt_open[branches]);

		sync = nvgpu_channel_user_syncpt_create(ch);

		if (branches == F_SYNC_NVHOST_CLIENT_MANAGED_FAIL) {
			g->nvhost->syncpt_id = 0U;
		}

		/* restore the original name member of the gk20a device */
		g->name = g_name;

		if (sync != NULL) {
			nvgpu_channel_user_syncpt_destroy(sync);
			unit_return_fail(m, "expected failure in creating sync points");
		}

		syncpt_id = g->nvhost->syncpt_id;
		syncpt_value = g->nvhost->syncpt_value;

		assert(syncpt_id == 0U);
		assert(syncpt_value == 0U);

		clear_test_params(g, &fault_injection_enabled, branches, kmem_fi);
	}

	return UNIT_SUCCESS;

done:
	clear_test_params(g, &fault_injection_enabled, 0, kmem_fi);

	return ret;
}

int test_sync_deinit(struct unit_module *m, struct gk20a *g, void *args)
{

	nvgpu_vm_put(g->mm.pmu.vm);
	nvgpu_vm_put(g->mm.bar1.vm);

	if (ch != NULL) {
		nvgpu_kfree(g, ch);
	}

	if (g->nvhost == NULL) {
		unit_return_fail(m ,"no valid nvhost device exists\n");
	}

	nvgpu_free_nvhost_dev(g);

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_sync_tests[] = {
	UNIT_TEST(sync_init, test_sync_init, NULL, 0),
	UNIT_TEST(sync_create_destroy, test_sync_create_destroy_sync, NULL, 0),
	UNIT_TEST(sync_set_safe_state, test_sync_set_safe_state, NULL, 0),
	UNIT_TEST(sync_user_managed_apis, test_sync_usermanaged_syncpt_apis, NULL, 0),
	UNIT_TEST(sync_get_ro_map, test_sync_get_ro_map, NULL, 0),
	UNIT_TEST(sync_fail, test_sync_create_fail, NULL, 0),
	UNIT_TEST(sync_deinit, test_sync_deinit, NULL, 0),
};

UNIT_MODULE(nvgpu-sync, nvgpu_sync_tests, UNIT_PRIO_NVGPU_TEST);
