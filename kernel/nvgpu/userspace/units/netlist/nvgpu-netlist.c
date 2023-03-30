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
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/hal_init.h>
#include <nvgpu/enabled.h>
#include <nvgpu/hw/gm20b/hw_mc_gm20b.h>
#include <nvgpu/netlist.h>

#include "hal/init/hal_gv11b.h"
#include "hal/netlist/netlist_gv11b.h"
#include "hal/gr/falcon/gr_falcon_gm20b.h"

#include "nvgpu-netlist.h"

#define NV_PMC_BOOT_0_ARCHITECTURE_GV110	(0x00000015 << \
					NVGPU_GPU_ARCHITECTURE_SHIFT)
#define NV_PMC_BOOT_0_IMPLEMENTATION_B		0xB

/*
 * Mock I/O
 */

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

static struct nvgpu_posix_io_callbacks netlist_test_reg_callbacks = {
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,
	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,
};

static bool test_netlist_fw_not_defined(void)
{
	return false;
}

static u32 test_gr_falcon_get_fecs_ctx_state_store_major_rev_id(struct gk20a *g)
{
	return 0xbad;
}

int test_netlist_init_support(struct unit_module *m,
						struct gk20a *g, void *args)
{

	int err = 0;

	if (nvgpu_posix_io_add_reg_space(g, mc_boot_0_r(), 0xfff) != 0) {
		unit_err(m, "%s: failed to create register space\n", __func__);
		return UNIT_FAIL;
	}

	(void)nvgpu_posix_register_io(g, &netlist_test_reg_callbacks);

	/*
	 * HAL init parameters for gv11b
	*/
	g->params.gpu_arch = NV_PMC_BOOT_0_ARCHITECTURE_GV110;
	g->params.gpu_impl = NV_PMC_BOOT_0_IMPLEMENTATION_B;

	/*
	 * HAL init required for getting
	 * the falcon ops initialized.
	 */
	err = nvgpu_init_hal(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_init_hal failed\n");
	}

	err = nvgpu_netlist_init_ctx_vars(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_netlist_init_ctx_vars_fw failed\n");
	}

	return UNIT_SUCCESS;

}

int test_netlist_query_tests(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct netlist_av_list *sw_non_ctx_load =
		nvgpu_netlist_get_sw_non_ctx_load_av_list(g);
	struct netlist_aiv_list *sw_ctx_load =
		nvgpu_netlist_get_sw_ctx_load_aiv_list(g);
	struct netlist_av_list *sw_method_init =
			nvgpu_netlist_get_sw_method_init_av_list(g);
	struct netlist_av_list *sw_bundle_init =
			nvgpu_netlist_get_sw_bundle_init_av_list(g);
	struct netlist_av_list *sw_veid_bundle_init =
		nvgpu_netlist_get_sw_veid_bundle_init_av_list(g);
	struct netlist_av64_list *sw_bundle64_init =
		nvgpu_netlist_get_sw_bundle64_init_av64_list(g);
	u32 count = 0;
	u32 *list = NULL;

	if (sw_non_ctx_load == NULL) {
		unit_return_fail(m, "get_sw_non_ctx_load_av_list failed\n");
	}

	if (sw_ctx_load == NULL) {
		unit_return_fail(m, "get_sw_ctx_load_aiv_list failed\n");
	}

	if (sw_method_init == NULL) {
		unit_return_fail(m, "get_sw_method_init_av_list failed\n");
	}

	if (sw_bundle_init == NULL) {
		unit_return_fail(m, "get_sw_bundle_init_av_list failed\n");
	}

	if (sw_veid_bundle_init == NULL) {
		unit_return_fail(m, "get_sw_veid_bundle_init_av_list failed\n");
	}

	if (sw_bundle64_init == NULL) {
		unit_return_fail(m, "get_sw_bundle64_init_av64_list failed\n");
	}

	count = nvgpu_netlist_get_fecs_inst_count(g);
	if (count == 0) {
		unit_return_fail (m, "get_fecs_inst_count failed\n");
	}

	count = nvgpu_netlist_get_fecs_data_count(g);
	if (count == 0) {
		unit_return_fail(m, "get_fecs_data_count failed\n");
	}

	count = nvgpu_netlist_get_gpccs_inst_count(g);
	if (count == 0) {
		unit_return_fail(m, "get_gpccs_inst_count failed\n");
	}

	count = nvgpu_netlist_get_gpccs_data_count(g);
	if (count == 0) {
		unit_return_fail(m, "get_gpccs_data_count failed\n");
	}

	list = nvgpu_netlist_get_fecs_inst_list(g);
	if (list == NULL) {
		unit_return_fail(m, "get_fecs_inst_list failed\n");
	}

	list = nvgpu_netlist_get_fecs_data_list(g);
	if (list == NULL) {
		unit_return_fail(m, "get_fecs_data_list failed\n");
	}

	list = nvgpu_netlist_get_gpccs_inst_list(g);
	if (list == NULL) {
		unit_return_fail(m, "get_gpccs_inst_list failed\n");
	}

	list = nvgpu_netlist_get_gpccs_data_list(g);
	if (list == NULL) {
		unit_return_fail(m, "get_gpccs_data_list failed\n");
	}

	return UNIT_SUCCESS;
}

static int test_netlist_alloc_failure(struct gk20a *g)
{
	int err, i;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	for (i = 0; i < 12; i++) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, i);
		err = nvgpu_netlist_init_ctx_vars(g);
		if (err == 0) {
			return UNIT_FAIL;
		}
		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	}

	return UNIT_SUCCESS;
}

int test_netlist_negative_tests(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err = 0;
	struct nvgpu_netlist_vars *netlist_vars = g->netlist_vars;

	err = nvgpu_netlist_init_ctx_vars(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_netlist_init_ctx_vars_fw failed\n");
	}

	/* unload netlist info */
	/* with NULL pointer */
	g->netlist_vars = NULL;
	nvgpu_netlist_deinit_ctx_vars(g);
	/* restore valid pointer */
	g->netlist_vars = netlist_vars;
	nvgpu_netlist_deinit_ctx_vars(g);

	err = test_netlist_alloc_failure(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_netlist_init_ctx_vars_fw failed\n");
	}

	/* Set up HAL for invalid netlist checks */
	g->ops.netlist.is_fw_defined = test_netlist_fw_not_defined;
	g->ops.gr.falcon.get_fecs_ctx_state_store_major_rev_id =
		test_gr_falcon_get_fecs_ctx_state_store_major_rev_id;
	err = nvgpu_netlist_init_ctx_vars(g);
	if (err == 0) {
		unit_return_fail(m, "nvgpu_netlist_init_ctx_vars_fw failed\n");
	}

	/* Restore orginal HALs */
	g->ops.netlist.is_fw_defined = gv11b_netlist_is_firmware_defined;
	g->ops.gr.falcon.get_fecs_ctx_state_store_major_rev_id =
			gm20b_gr_falcon_get_fecs_ctx_state_store_major_rev_id;
	err = nvgpu_netlist_init_ctx_vars(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_netlist_init_ctx_vars_fw failed\n");
	}

	return UNIT_SUCCESS;
}

int test_netlist_remove_support(struct unit_module *m,
		struct gk20a *g, void *args)
{
	nvgpu_netlist_deinit_ctx_vars(g);

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_netlist_tests[] = {
	UNIT_TEST(netlist_init_support, test_netlist_init_support, NULL, 0),
	UNIT_TEST(netlist_query_tests, test_netlist_query_tests, NULL, 0),
	UNIT_TEST(netlist_negative_tests, test_netlist_negative_tests, NULL, 0),
	UNIT_TEST(netlist_remove_support, test_netlist_remove_support, NULL, 0),
};

UNIT_MODULE(nvgpu-netlist, nvgpu_netlist_tests, UNIT_PRIO_NVGPU_TEST);
