/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <pthread.h>

#include <unit/unit.h>
#include <unit/io.h>

#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/firmware.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/hal_init.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/hw/gp10b/hw_fuse_gp10b.h>
#include <nvgpu/hw/gv11b/hw_falcon_gv11b.h>

#include "../falcon_utf.h"
#include "nvgpu-falcon.h"

#include  "common/acr/acr_priv.h"

struct utf_falcon *utf_falcons[FALCON_ID_END];

static struct nvgpu_falcon *pmu_flcn;
static struct nvgpu_falcon *gpccs_flcn;
static struct nvgpu_falcon *uninit_flcn;
static u8 *rand_test_data_unaligned;
static u32 *rand_test_data;

#define NV_PMC_BOOT_0_ARCHITECTURE_GV110	(0x00000015 << \
						 NVGPU_GPU_ARCHITECTURE_SHIFT)
#define NV_PMC_BOOT_0_IMPLEMENTATION_B		0xB
#define MAX_MEM_TYPE				(MEM_IMEM + 1)

#define RAND_DATA_SIZE				(SZ_4K)

static struct utf_falcon *get_utf_falcon_from_addr(struct gk20a *g, u32 addr)
{
	struct utf_falcon *flcn = NULL;
	u32 flcn_base;
	u32 i;

	for (i = 0; i < FALCON_ID_END; i++) {
		if (utf_falcons[i] == NULL || utf_falcons[i]->flcn == NULL) {
			continue;
		}

		flcn_base = utf_falcons[i]->flcn->flcn_base;
		if ((addr >= flcn_base) &&
		    (addr < (flcn_base + UTF_FALCON_MAX_REG_OFFSET))) {
			flcn = utf_falcons[i];
			break;
		}
	}

	return flcn;
}

static void writel_access_reg_fn(struct gk20a *g,
				 struct nvgpu_reg_access *access)
{
	struct utf_falcon *flcn = NULL;

	flcn = get_utf_falcon_from_addr(g, access->addr);
	if (flcn != NULL) {
		nvgpu_utf_falcon_writel_access_reg_fn(g, flcn, access);
	} else {
		nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
	}
	nvgpu_posix_io_record_access(g, access);
}

static void readl_access_reg_fn(struct gk20a *g,
				struct nvgpu_reg_access *access)
{
	struct utf_falcon *flcn = NULL;

	flcn = get_utf_falcon_from_addr(g, access->addr);
	if (flcn != NULL) {
		nvgpu_utf_falcon_readl_access_reg_fn(g, flcn, access);
	} else {
		access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
	}
}

static struct nvgpu_posix_io_callbacks utf_falcon_reg_callbacks = {
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,

	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,
};

static void utf_falcon_register_io(struct gk20a *g)
{
	nvgpu_posix_register_io(g, &utf_falcon_reg_callbacks);
}

static void init_rand_buffer(void)
{
	u32 i;

	/*
	 * Fill the test buffer with random data. Always use the same seed to
	 * make the test deterministic.
	 */
	srand(0);
	for (i = 0; i < RAND_DATA_SIZE/sizeof(u32); i++) {
		rand_test_data[i] = (u32) rand();
	}
}

static int init_falcon_test_env(struct unit_module *m, struct gk20a *g)
{
	int err = 0;

	utf_falcon_register_io(g);

	/*
	 * Fuse register fuse_opt_priv_sec_en_r() is read during init_hal hence
	 * add it to reg space
	 */
	if (nvgpu_posix_io_add_reg_space(g,
					 fuse_opt_priv_sec_en_r(), 0x4) != 0) {
		unit_err(m, "Add reg space failed!\n");
		return -ENOMEM;
	}

	/* HAL init parameters for gv11b */
	g->params.gpu_arch = NV_PMC_BOOT_0_ARCHITECTURE_GV110;
	g->params.gpu_impl = NV_PMC_BOOT_0_IMPLEMENTATION_B;

	/* HAL init required for getting the falcon ops initialized. */
	err = nvgpu_init_hal(g);
	if (err != 0) {
		return -ENODEV;
	}

	/* Initialize utf & nvgpu falcon for test usage */
	utf_falcons[FALCON_ID_PMU] = nvgpu_utf_falcon_init(m, g, FALCON_ID_PMU);
	if (utf_falcons[FALCON_ID_PMU] == NULL) {
		return -ENODEV;
	}

	utf_falcons[FALCON_ID_GPCCS] =
				nvgpu_utf_falcon_init(m, g, FALCON_ID_GPCCS);
	if (utf_falcons[FALCON_ID_GPCCS] == NULL) {
		return -ENODEV;
	}

	/* Set falcons for test usage */
	pmu_flcn = &g->pmu_flcn;
	gpccs_flcn = &g->gpccs_flcn;
	uninit_flcn = &g->fecs_flcn;

	/* Create a test buffer to be filled with random data */
	rand_test_data = (u32 *) nvgpu_kzalloc(g, RAND_DATA_SIZE);
	if (rand_test_data == NULL) {
		return -ENOMEM;
	}

	init_rand_buffer();
	return 0;
}

static int free_falcon_test_env(struct unit_module *m, struct gk20a *g,
				void *__args)
{
	if (pmu_flcn == NULL || !pmu_flcn->is_falcon_supported) {
		unit_return_fail(m, "test environment not initialized.");
	}

	nvgpu_kfree(g, rand_test_data);
	nvgpu_utf_falcon_free(g, utf_falcons[FALCON_ID_GPCCS]);
	nvgpu_utf_falcon_free(g, utf_falcons[FALCON_ID_PMU]);
	return UNIT_SUCCESS;
}

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
/*
 * This function will compare rand_test_data with falcon flcn's imem/dmem
 * based on type from offset src of size. It returns 0 on match else
 * non-zero value.
 */
static int falcon_read_compare(struct unit_module *m, struct gk20a *g,
			       enum falcon_mem_type type,
			       u32 src, u32 size, bool aligned_rand_data)
{
	u8 *dest = NULL, *destp, *cmp_test_data = NULL;
	u32 total_block_read = 0;
	u32 byte_read_count = 0;
	u32 byte_cnt = size;
	int err;

	dest = (u8 *) nvgpu_kzalloc(g, byte_cnt);
	if (dest == NULL) {
		unit_err(m, "Memory allocation failed\n");
		return -ENOMEM;
	}

	destp = dest;

	total_block_read = byte_cnt >> 8;
	do {
		byte_read_count =
				total_block_read ? FALCON_BLOCK_SIZE : byte_cnt;
		if (!byte_read_count) {
			break;
		}

		if (type == MEM_IMEM) {
			err = nvgpu_falcon_copy_from_imem(pmu_flcn, src,
					destp, byte_read_count, 0);
		} else if (type == MEM_DMEM) {
			err = nvgpu_falcon_copy_from_dmem(pmu_flcn, src,
					destp, byte_read_count, 0);
		} else {
			unit_err(m, "Invalid read type\n");
			err = -EINVAL;
			goto free_dest;
		}

		if (err) {
			unit_err(m, "Failed to copy from falcon memory\n");
			goto free_dest;
		}

		destp += byte_read_count;
		src += byte_read_count;
		byte_cnt -= byte_read_count;
	} while (total_block_read--);

	if (aligned_rand_data) {
		cmp_test_data = (u8 *) rand_test_data;
	} else {
		cmp_test_data = rand_test_data_unaligned;
	}

	if (memcmp((void *) dest, (void *) cmp_test_data, size) != 0) {
		unit_err(m, "Mismatch comparing copied data\n");
		err = -EINVAL;
	}

free_dest:
	nvgpu_kfree(g, dest);
	return err;
}
#endif

/*
 * This function will check that falcon memory read/write functions with
 * specified parameters complete with return value exp_err.
 */
static int falcon_check_read_write(struct gk20a *g,
				   struct unit_module *m,
				   struct nvgpu_falcon *flcn,
				   enum falcon_mem_type type,
				   u32 dst, u32 byte_cnt, int exp_err)
{
	u8 *dest = NULL;
	int ret = -1;
	int err;

	dest = (u8 *) nvgpu_kzalloc(g, byte_cnt);
	if (dest == NULL) {
		unit_err(m, "Memory allocation failed\n");
		goto out;
	}


	if (type == MEM_IMEM) {
		err = nvgpu_falcon_copy_to_imem(flcn, dst,
						(u8 *) rand_test_data,
						byte_cnt, 0, false, 0);
		if (err != exp_err) {
			unit_err(m, "Copy to IMEM should %s\n",
				 exp_err ? "fail" : "pass");
			goto free_dest;
		}

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
		err = nvgpu_falcon_copy_from_imem(flcn, dst,
						  dest, byte_cnt, 0);
		if (err != exp_err) {
			unit_err(m, "Copy from IMEM should %s\n",
				 exp_err ? "fail" : "pass");
			goto free_dest;
		}
#endif
	} else if (type == MEM_DMEM) {
		err = nvgpu_falcon_copy_to_dmem(flcn, dst,
						(u8 *) rand_test_data,
						byte_cnt, 0);
		if (err != exp_err) {
			unit_err(m, "Copy to DMEM should %s\n",
				 exp_err ? "fail" : "pass");
			goto free_dest;
		}

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
		err = nvgpu_falcon_copy_from_dmem(flcn, dst,
						  dest, byte_cnt, 0);
		if (err != exp_err) {
			unit_err(m, "Copy from DMEM should %s\n",
				 exp_err ? "fail" : "pass");
			goto free_dest;
		}
#endif
	}

	ret = 0;

free_dest:
	nvgpu_kfree(g, dest);
out:
	return ret;
}

static int verify_valid_falcon_sw_init(struct unit_module *m, struct gk20a *g,
					u32 flcn_id)
{
	int err;

	err = nvgpu_falcon_sw_init(g, flcn_id);
	if (err != 0) {
		unit_err(m, "falcon init with valid ID %d failed\n", flcn_id);
		return err;
	}

	nvgpu_falcon_sw_free(g, flcn_id);

	return 0;
}

/*
 * Valid/Invalid: Passing valid ID should succeed the call to function
 * nvgpu_falcon_sw_init|free. Otherwise it should fail with error.
 */
int test_falcon_sw_init_free(struct unit_module *m, struct gk20a *g,
			     void *__args)
{
	int err;

	/* initialize test setup */
	if (init_falcon_test_env(m, g) != 0) {
		unit_return_fail(m, "Module init failed\n");
	}

	err = nvgpu_falcon_sw_init(g, FALCON_ID_INVALID);
	if (err != -ENODEV) {
		unit_return_fail(m, "falcon with invalid id initialized\n");
	}

	nvgpu_falcon_sw_free(g, FALCON_ID_INVALID);

	err = verify_valid_falcon_sw_init(m, g, FALCON_ID_FECS);
	if (err != 0) {
		unit_return_fail(m, "FECS falcon sw not initialized\n");
	}

#ifdef CONFIG_NVGPU_DGPU
	err = verify_valid_falcon_sw_init(m, g, FALCON_ID_GSPLITE);
	if (err != 0) {
		unit_return_fail(m, "GSPLITE falcon sw not initialized\n");
	}

	err = verify_valid_falcon_sw_init(m, g, FALCON_ID_NVDEC);
	if (err != 0) {
		unit_return_fail(m, "NVDEC falcon sw not initialized\n");
	}

	err = verify_valid_falcon_sw_init(m, g, FALCON_ID_SEC2);
	if (err != 0) {
		unit_return_fail(m, "SEC2 falcon sw not initialized\n");
	}

	err = verify_valid_falcon_sw_init(m, g, FALCON_ID_MINION);
	if (err != 0) {
		unit_return_fail(m, "MINION falcon sw not initialized\n");
	}
#endif


	return UNIT_SUCCESS;
}

int test_falcon_get_id(struct unit_module *m, struct gk20a *g,
			     void *__args)
{
	if (nvgpu_falcon_get_id(gpccs_flcn) == FALCON_ID_GPCCS) {
		return UNIT_SUCCESS;
	} else {
		return UNIT_FAIL;
	}
}

static void flcn_mem_scrub_pass(void *data)
{
	struct nvgpu_falcon *flcn = (struct nvgpu_falcon *) data;
	u32 dmactl_addr = flcn->flcn_base + falcon_falcon_dmactl_r();
	struct gk20a *g = flcn->g;
	u32 unit_status;

	unit_status = nvgpu_posix_io_readl_reg_space(g, dmactl_addr);
	unit_status &= ~(falcon_falcon_dmactl_dmem_scrubbing_m() |
			 falcon_falcon_dmactl_imem_scrubbing_m());
	nvgpu_posix_io_writel_reg_space(g, dmactl_addr, unit_status);
}

static int flcn_reset_state_check(void *data)
{
	struct nvgpu_falcon *flcn = (struct nvgpu_falcon *) data;
	struct gk20a *g = flcn->g;
	u32 unit_status;

	unit_status = nvgpu_posix_io_readl_reg_space(g, flcn->flcn_base +
						     falcon_falcon_cpuctl_r());
	if (unit_status | falcon_falcon_cpuctl_hreset_f(1))
		return 0;
	else
		return -1;
}

static void flcn_mem_scrub_fail(void *data)
{
	struct nvgpu_falcon *flcn = (struct nvgpu_falcon *) data;
	u32 dmactl_addr = flcn->flcn_base + falcon_falcon_dmactl_r();
	struct gk20a *g = flcn->g;
	u32 unit_status;

	unit_status = nvgpu_posix_io_readl_reg_space(g, dmactl_addr);
	unit_status |= (falcon_falcon_dmactl_dmem_scrubbing_m() |
			falcon_falcon_dmactl_imem_scrubbing_m());
	nvgpu_posix_io_writel_reg_space(g, dmactl_addr, unit_status);
}

/*
 * Valid: Reset of initialized Falcon succeeds and if not backed by engine
 *        dependent reset then check CPU control register for bit
 *        falcon_falcon_cpuctl_hreset_f(1).
 * Invalid: Reset of uninitialized and null falcon fails with error -EINVAL.
 */
int test_falcon_reset(struct unit_module *m, struct gk20a *g, void *__args)
{
	struct {
		struct nvgpu_falcon *flcn;
		void (*pre_reset)(void *);
		int exp_err;
		int (*reset_state_check)(void *);
	} test_data[] = {{NULL, NULL, -EINVAL, NULL},
			 {uninit_flcn, NULL, -EINVAL, NULL},
			 {gpccs_flcn, flcn_mem_scrub_pass, 0,
			  flcn_reset_state_check} };
	int size = ARRAY_SIZE(test_data);
	void *data;
	int err, i;

	for (i = 0; i < size; i++) {
		if (test_data[i].pre_reset) {
			test_data[i].pre_reset((void *)test_data[i].flcn);
		}

		err = nvgpu_falcon_reset(test_data[i].flcn);
		if (err != test_data[i].exp_err) {
			unit_return_fail(m, "falcon reset err: %d "
					    "expected err: %d\n",
					 err, test_data[i].exp_err);
		}

		if (test_data[i].reset_state_check) {
			data = (void *)test_data[i].flcn;
			err = test_data[i].reset_state_check(data);
			if (err) {
				unit_return_fail(m, "falcon reset state "
						    "mismatch\n");
			}
		}
	}

	return UNIT_SUCCESS;
}

/*
 * Invalid: Calling this interface on uninitialized falcon should
 *          return -EINVAL.
 * Valid: Set the mem scrubbing status as done and call should return 0.
 *        Set the mem scrubbing status as pending and call should return
 *        -ETIMEDOUT.
 */
int test_falcon_mem_scrub(struct unit_module *m, struct gk20a *g, void *__args)
{
	struct {
		struct nvgpu_falcon *flcn;
		void (*pre_scrub)(void *);
		int exp_err;
	} test_data[] = {{uninit_flcn, NULL, -EINVAL},
			 {gpccs_flcn, flcn_mem_scrub_pass, 0},
			 {gpccs_flcn, flcn_mem_scrub_fail, -ETIMEDOUT} };
	int size = ARRAY_SIZE(test_data);
	int err, i;

	for (i = 0; i < size; i++) {
		if (test_data[i].pre_scrub) {
			test_data[i].pre_scrub(test_data[i].flcn);
		}

		err = nvgpu_falcon_mem_scrub_wait(test_data[i].flcn);
		if (err != test_data[i].exp_err) {
			unit_return_fail(m, "falcon mem scrub err: %d "
					    "expected err: %d\n",
					 err, test_data[i].exp_err);
		}
	}

	return UNIT_SUCCESS;
}

/*
 * FIXME: Following masks are not yet available in the hw headers.
 */
#define falcon_falcon_idlestate_falcon_busy_m()		(U32(0x1U) << 0U)
#define falcon_falcon_idlestate_ext_busy_m()		(U32(0x7fffU) << 1U)

static void flcn_idle_pass(void *data)
{
	struct nvgpu_falcon *flcn = (struct nvgpu_falcon *) data;
	u32 idlestate_addr = flcn->flcn_base + falcon_falcon_idlestate_r();
	struct gk20a *g = flcn->g;
	u32 unit_status;

	unit_status = nvgpu_posix_io_readl_reg_space(g, idlestate_addr);
	unit_status &= ~(falcon_falcon_idlestate_falcon_busy_m() |
			 falcon_falcon_idlestate_ext_busy_m());
	nvgpu_posix_io_writel_reg_space(g, idlestate_addr, unit_status);
}

/*
 * This is to cover the falcon CPU idle & ext units busy branch in if condition
 * in gk20a_is_falcon_idle.
 */
static void flcn_idle_fail_ext_busy(void *data)
{
	struct nvgpu_falcon *flcn = (struct nvgpu_falcon *) data;
	u32 idlestate_addr = flcn->flcn_base + falcon_falcon_idlestate_r();
	struct gk20a *g = flcn->g;
	u32 unit_status;

	unit_status = nvgpu_posix_io_readl_reg_space(g, idlestate_addr);
	unit_status |= falcon_falcon_idlestate_ext_busy_m();
	nvgpu_posix_io_writel_reg_space(g, idlestate_addr, unit_status);
}

static void flcn_idle_fail(void *data)
{
	struct nvgpu_falcon *flcn = (struct nvgpu_falcon *) data;
	u32 idlestate_addr = flcn->flcn_base + falcon_falcon_idlestate_r();
	struct gk20a *g = flcn->g;
	u32 unit_status;

	unit_status = nvgpu_posix_io_readl_reg_space(g, idlestate_addr);
	unit_status |= (falcon_falcon_idlestate_falcon_busy_m() |
			falcon_falcon_idlestate_ext_busy_m());
	nvgpu_posix_io_writel_reg_space(g, idlestate_addr, unit_status);
}

/*
 * Invalid: Calling this interface on uninitialized falcon should
 *          return -EINVAL.
 * Valid: Set the Falcon idle state as idle in falcon_falcon_idlestate_r and
 *        call should return 0. Set it to non-idle and call should return
 *        -ETIMEDOUT.
 */
int test_falcon_idle(struct unit_module *m, struct gk20a *g, void *__args)
{
	struct {
		struct nvgpu_falcon *flcn;
		void (*pre_idle)(void *);
		int exp_err;
	} test_data[] = {{uninit_flcn, NULL, -EINVAL},
			 {gpccs_flcn, flcn_idle_pass, 0},
			 {gpccs_flcn, flcn_idle_fail_ext_busy, -ETIMEDOUT},
			 {gpccs_flcn, flcn_idle_fail, -ETIMEDOUT} };
	int size = ARRAY_SIZE(test_data);
	int err, i;

	for (i = 0; i < size; i++) {
		if (test_data[i].pre_idle) {
			test_data[i].pre_idle(test_data[i].flcn);
		}

		err = nvgpu_falcon_wait_idle(test_data[i].flcn);
		if (err != test_data[i].exp_err) {
			unit_return_fail(m, "falcon wait for idle err: %d "
					    "expected err: %d\n",
					 err, test_data[i].exp_err);
		}
	}

	return UNIT_SUCCESS;
}

static void flcn_halt_pass(void *data)
{
	struct nvgpu_falcon *flcn = (struct nvgpu_falcon *) data;
	u32 cpuctl_addr = flcn->flcn_base + falcon_falcon_cpuctl_r();
	struct gk20a *g = flcn->g;
	u32 unit_status;

	unit_status = nvgpu_posix_io_readl_reg_space(g, cpuctl_addr);
	unit_status |= falcon_falcon_cpuctl_halt_intr_m();
	nvgpu_posix_io_writel_reg_space(g, cpuctl_addr, unit_status);
}

static void flcn_halt_fail(void *data)
{
	struct nvgpu_falcon *flcn = (struct nvgpu_falcon *) data;
	u32 cpuctl_addr = flcn->flcn_base + falcon_falcon_cpuctl_r();
	struct gk20a *g = flcn->g;
	u32 unit_status;

	unit_status = nvgpu_posix_io_readl_reg_space(g, cpuctl_addr);
	unit_status &= ~falcon_falcon_cpuctl_halt_intr_m();
	nvgpu_posix_io_writel_reg_space(g, cpuctl_addr, unit_status);
}

/*
 * Invalid: Calling this interface on uninitialized falcon should return
 *          -EINVAL.
 *
 * Valid: Set the Falcon halt state as halted in falcon_falcon_cpuctl_r and
 *        call should return 0. Set it to non-halted and call should return
 *        -ETIMEDOUT.
 */
int test_falcon_halt(struct unit_module *m, struct gk20a *g, void *__args)
{
#define FALCON_WAIT_HALT 200
	struct {
		struct nvgpu_falcon *flcn;
		void (*pre_halt)(void *);
		int exp_err;
	} test_data[] = {{uninit_flcn, NULL, -EINVAL},
			 {gpccs_flcn, flcn_halt_pass, 0},
			 {gpccs_flcn, flcn_halt_fail, -ETIMEDOUT} };
	int size = ARRAY_SIZE(test_data);
	int err, i;

	for (i = 0; i < size; i++) {
		if (test_data[i].pre_halt) {
			test_data[i].pre_halt(test_data[i].flcn);
		}

		err = nvgpu_falcon_wait_for_halt(test_data[i].flcn,
						 FALCON_WAIT_HALT);
		if (err != test_data[i].exp_err) {
			unit_return_fail(m, "falcon wait for halt err: %d "
					    "expected err: %d\n",
					 err, test_data[i].exp_err);
		}
	}

	return UNIT_SUCCESS;
}

/*
 * Valid/Invalid: Status of read and write from Falcon
 * Valid: Read and write of word-multiple and non-word-multiple data from
 *        initialized Falcon succeeds.
 * Invalid: Read and write for uninitialized Falcon fails
 *	    with error -EINVAL.
 */
int test_falcon_mem_rw_init(struct unit_module *m, struct gk20a *g,
			    void *__args)
{
	u32 dst = 0;
	int err = 0, i;

	/* write/read to/from uninitialized falcon */
	for (i = 0; i < MAX_MEM_TYPE; i++) {
		err = falcon_check_read_write(g, m, uninit_flcn, i, dst,
					      RAND_DATA_SIZE, -EINVAL);
		if (err) {
			return UNIT_FAIL;
		}
	}

	/* write/read to/from initialized falcon */
	for (i = 0; i < MAX_MEM_TYPE; i++) {
		err = falcon_check_read_write(g, m, pmu_flcn, i, dst,
					      RAND_DATA_SIZE, 0);
		if (err) {
			return UNIT_FAIL;
		}
	}

	/* write/read to/from initialized falcon with non-word-multiple data */
	for (i = 0; i < MAX_MEM_TYPE; i++) {
		err = falcon_check_read_write(g, m, pmu_flcn, i, dst,
					      RAND_DATA_SIZE - 1, 0);
		if (err) {
			return UNIT_FAIL;
		}
	}

	return UNIT_SUCCESS;
}

/*
 * Invalid: Read and write for invalid Falcon port should fail
 *	    with error -EINVAL.
 */
int test_falcon_mem_rw_inval_port(struct unit_module *m, struct gk20a *g,
				  void *__args)
{
	int err = 0, size = RAND_DATA_SIZE, port = 2;

	if (pmu_flcn == NULL || !pmu_flcn->is_falcon_supported) {
		unit_return_fail(m, "test environment not initialized.");
	}

	/* write to invalid port */
	unit_info(m, "Writing %d bytes to imem port %d\n", size, port);
	err = nvgpu_falcon_copy_to_imem(pmu_flcn, 0, (u8 *) rand_test_data,
					size, port, false, 0);
	if (err != -EINVAL) {
		unit_return_fail(m, "Copy to IMEM invalid port should fail\n");
	}

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
	err = nvgpu_falcon_copy_from_imem(pmu_flcn, 0,
					  NULL, size, port);
	if (err != -EINVAL) {
		unit_err(m, "Copy from IMEM invalid port should fail\n");
	}
#endif

	return UNIT_SUCCESS;
}

/*
 * Reading and writing data from/to unaligned data should succeed.
 */
int test_falcon_mem_rw_unaligned_cpu_buffer(struct unit_module *m,
					    struct gk20a *g,
					    void *__args)
{
	rand_test_data_unaligned = (u8 *)&rand_test_data[0] + 1;
	u32 byte_cnt = RAND_DATA_SIZE - 8;
	u32 dst = 0;
	int err = UNIT_FAIL;

	if (pmu_flcn == NULL || !pmu_flcn->is_falcon_supported) {
		unit_return_fail(m, "test environment not initialized.");
	}

	/* write data to valid range in imem from unaligned data */
	unit_info(m, "Writing %d bytes to imem\n", byte_cnt);
	err = nvgpu_falcon_copy_to_imem(pmu_flcn, dst,
					(u8 *) rand_test_data_unaligned,
					byte_cnt, 0, false, 0);
	if (err) {
		unit_return_fail(m, "Failed to copy to IMEM\n");
	}

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
	/* verify data written to imem matches */
	unit_info(m, "Reading %d bytes from imem\n", byte_cnt);
	err = falcon_read_compare(m, g, MEM_IMEM, dst, byte_cnt, false);
	if (err) {
		unit_err(m, "IMEM read data does not match %d\n", err);
		return UNIT_FAIL;
	}
#endif

	/* write data to valid range in dmem from unaligned data */
	unit_info(m, "Writing %d bytes to dmem\n", byte_cnt);
	err = nvgpu_falcon_copy_to_dmem(pmu_flcn, dst,
					(u8 *) rand_test_data_unaligned,
					byte_cnt, 0);
	if (err) {
		unit_return_fail(m, "Failed to copy to DMEM\n");
	}

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
	/* verify data written to dmem matches */
	unit_info(m, "Reading %d bytes from dmem\n", byte_cnt);
	err = falcon_read_compare(m, g, MEM_DMEM, dst, byte_cnt, false);
	if (err) {
		unit_err(m, "DMEM read data does not match %d\n", err);
		return UNIT_FAIL;
	}
#endif

	/*
	 * write data of size 1K to valid range in imem from unaligned data
	 * to verify the buffering logic in falcon_copy_to_dmem_unaligned_src.
	 */
	unit_info(m, "Writing %d bytes to imem\n", (u32) SZ_1K);
	err = nvgpu_falcon_copy_to_imem(pmu_flcn, dst,
					(u8 *) rand_test_data_unaligned,
					SZ_1K, 0, false, 0);
	if (err) {
		unit_return_fail(m, "Failed to copy to IMEM\n");
	}

	/*
	 * write data of size 1K to valid range in dmem from unaligned data
	 * to verify the buffering logic in falcon_copy_to_imem_unaligned_src.
	 */
	unit_info(m, "Writing %d bytes to dmem\n", (u32) SZ_1K);
	err = nvgpu_falcon_copy_to_dmem(pmu_flcn, dst,
					(u8 *) rand_test_data_unaligned,
					SZ_1K, 0);
	if (err) {
		unit_return_fail(m, "Failed to copy to DMEM\n");
	}

	return UNIT_SUCCESS;
}

/*
 * Valid/Invalid: Reading and writing data in accessible range should work
 *		  and fail otherwise.
 * Valid: Data read from or written to Falcon memory in bounds is valid
 *	  operation and should return success.
 * Invalid: Reading and writing data out of Falcon memory bounds should
 *	    return error -EINVAL.
 */
int test_falcon_mem_rw_range(struct unit_module *m, struct gk20a *g,
			     void *__args)
{
	u32 byte_cnt = RAND_DATA_SIZE;
	u32 dst = 0;
	int err = UNIT_FAIL;

	if (pmu_flcn == NULL || !pmu_flcn->is_falcon_supported) {
		unit_return_fail(m, "test environment not initialized.");
	}

	/* write data to valid range in imem */
	unit_info(m, "Writing %d bytes to imem\n", byte_cnt);
	err = nvgpu_falcon_copy_to_imem(pmu_flcn, dst, (u8 *) rand_test_data,
					byte_cnt, 0, false, 0);
	if (err) {
		unit_return_fail(m, "Failed to copy to IMEM\n");
	}

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
	/* verify data written to imem matches */
	unit_info(m, "Reading %d bytes from imem\n", byte_cnt);
	err = falcon_read_compare(m, g, MEM_IMEM, dst, byte_cnt, true);
	if (err) {
		unit_err(m, "IMEM read data does not match %d\n", err);
		return UNIT_FAIL;
	}
#endif

	/* write data to valid range in dmem */
	unit_info(m, "Writing %d bytes to dmem\n", byte_cnt);
	err = nvgpu_falcon_copy_to_dmem(pmu_flcn, dst, (u8 *) rand_test_data,
					byte_cnt, 0);
	if (err) {
		unit_return_fail(m, "Failed to copy to DMEM\n");
	}

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
	/* verify data written to dmem matches */
	unit_info(m, "Reading %d bytes from dmem\n", byte_cnt);
	err = falcon_read_compare(m, g, MEM_DMEM, dst, byte_cnt, true);
	if (err) {
		unit_err(m, "DMEM read data does not match %d\n", err);
		return UNIT_FAIL;
	}
#endif
	dst = UTF_FALCON_IMEM_DMEM_SIZE - RAND_DATA_SIZE;
	byte_cnt *= 2;

	/* write/read data to/from invalid range in imem */
	err = falcon_check_read_write(g, m, pmu_flcn, MEM_IMEM, dst,
				      byte_cnt, -EINVAL);
	if (err) {
		return UNIT_FAIL;
	}

	/* write/read data to/from invalid range in dmem */
	err = falcon_check_read_write(g, m, pmu_flcn, MEM_DMEM, dst,
				      byte_cnt, -EINVAL);
	if (err) {
		return UNIT_FAIL;
	}

	dst = UTF_FALCON_IMEM_DMEM_SIZE;

	/* write/read data to/from invalid offset in imem */
	err = falcon_check_read_write(g, m, pmu_flcn, MEM_IMEM, dst,
				      byte_cnt, -EINVAL);
	if (err) {
		return UNIT_FAIL;
	}

	/* write/read data to/from invalid offset in dmem */
	err = falcon_check_read_write(g, m, pmu_flcn, MEM_DMEM, dst,
				      byte_cnt, -EINVAL);
	if (err) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

/*
 * Writing data to falcon's DMEM should not succeed when DMEMC
 * read returns invalid value due to HW fault.
 */
int test_falcon_mem_rw_fault(struct unit_module *m, struct gk20a *g,
			     void *__args)
{
	struct nvgpu_posix_fault_inj *falcon_memcpy_fi =
				nvgpu_utf_falcon_memcpy_get_fault_injection();
	u32 byte_cnt = RAND_DATA_SIZE;
	u32 dst = 0;
	int err = UNIT_FAIL;

	if (pmu_flcn == NULL || !pmu_flcn->is_falcon_supported) {
		unit_return_fail(m, "test environment not initialized.");
	}

	/* cause write failure */
	nvgpu_posix_enable_fault_injection(falcon_memcpy_fi, true, 0);
	unit_info(m, "Writing %d bytes to dmem with hw fault injected.\n",
		  byte_cnt);
	err = nvgpu_falcon_copy_to_dmem(pmu_flcn, dst, (u8 *) rand_test_data,
					byte_cnt, 0);
	nvgpu_posix_enable_fault_injection(falcon_memcpy_fi, false, 0);

	if (err == 0) {
		unit_return_fail(m, "Copy to DMEM succeeded with faulty hw.\n");
	}

	return UNIT_SUCCESS;
}

/*
 * Valid/Invalid: Reading and writing data at offset that is word (4-byte)
 *		  aligned data should work and fail otherwise.
 * Valid: Data read/written from/to Falcon memory from word (4-byte) aligned
 *	  offset is valid operation and should return success.
 * Invalid: Reading and writing data out of non-word-aligned offset in Falcon
 *	    memory should return error -EINVAL.
 */
int test_falcon_mem_rw_aligned(struct unit_module *m, struct gk20a *g,
			       void *__args)
{
	u32 byte_cnt = RAND_DATA_SIZE;
	u32 dst = 0, i;
	int err = 0;

	if (pmu_flcn == NULL || !pmu_flcn->is_falcon_supported) {
		unit_return_fail(m, "test environment not initialized.");
	}

	for (i = 0; i < MAX_MEM_TYPE; i++) {
		/*
		 * Copy to/from offset dst = 3 that is not word aligned should
		 * fail.
		 */
		dst = 0x3;
		err = falcon_check_read_write(g, m, pmu_flcn, i, dst,
					      byte_cnt, -EINVAL);
		if (err) {
			return UNIT_FAIL;
		}

		/*
		 * Copy to/from offset dst = 4 that is word aligned should
		 * succeed.
		 */
		dst = 0x4;
		err = falcon_check_read_write(g, m, pmu_flcn, i, dst,
					      byte_cnt, 0);
		if (err) {
			return UNIT_FAIL;
		}
	}

	return UNIT_SUCCESS;
}

/*
 * Reading/writing zero bytes should return error -EINVAL.
 */
int test_falcon_mem_rw_zero(struct unit_module *m, struct gk20a *g,
			    void *__args)
{
	u32 byte_cnt = 0;
	u32 dst = 0, i;
	int err = 0;

	if (pmu_flcn == NULL || !pmu_flcn->is_falcon_supported) {
		unit_return_fail(m, "test environment not initialized.");
	}

	for (i = 0; i < MAX_MEM_TYPE; i++) {
		/* write/read zero bytes should fail*/
		err = falcon_check_read_write(g, m, pmu_flcn, i, dst,
					      byte_cnt, -EINVAL);
		if (err) {
			return UNIT_FAIL;
		}
	}

	return UNIT_SUCCESS;
}

/*
 * Invalid: Calling read interface on uninitialized falcon should return value 0
 *          and do nothing with write interface.
 * Invalid: Pass invalid mailbox number and verify that read returns zero and
 *          write does not fail.
 *
 * Valid: Write the value of a mailbox register through this interface and
 *        verify the expected value in register falcon_falcon_mailbox0_r.
 *        Read the value through this interface and verify that it matches
 *        the register value.
 */
int test_falcon_mailbox(struct unit_module *m, struct gk20a *g, void *__args)
{
#define	SAMPLE_MAILBOX_DATA	0xDEADBEED
	u32 val, reg_data, mailbox_addr, i;

	nvgpu_falcon_mailbox_write(uninit_flcn, FALCON_MAILBOX_0,
				   SAMPLE_MAILBOX_DATA);
	val = nvgpu_falcon_mailbox_read(uninit_flcn, FALCON_MAILBOX_0);
	if (val != 0) {
		unit_return_fail(m, "Invalid falcon's mailbox read"
				    "should return zero\n");
	}

	for (i = FALCON_MAILBOX_0; i <= FALCON_MAILBOX_COUNT; i++) {
		nvgpu_falcon_mailbox_write(gpccs_flcn, i, SAMPLE_MAILBOX_DATA);
		val = nvgpu_falcon_mailbox_read(gpccs_flcn, i);

		if (i == FALCON_MAILBOX_COUNT) {
			if (val != 0) {
				unit_return_fail(m, "Invalid mailbox read "
						    "should return zero\n");
			} else {
				continue;
			}
		}

		mailbox_addr = i != 0U ? falcon_falcon_mailbox1_r() :
					 falcon_falcon_mailbox0_r();
		mailbox_addr = gpccs_flcn->flcn_base + mailbox_addr;
		reg_data = nvgpu_posix_io_readl_reg_space(g, mailbox_addr);

		if (val != SAMPLE_MAILBOX_DATA || val != reg_data) {
			unit_return_fail(m, "Failed reading/writing mailbox\n");
		}
	}

	return UNIT_SUCCESS;
}

static bool falcon_check_reg_group(struct gk20a *g,
				   struct nvgpu_reg_access *sequence,
				   u32 size)
{
	u32 i;

	for (i = 0; i < size; i++) {
		if (nvgpu_posix_io_readl_reg_space(g, sequence[i].addr) !=
		    sequence[i].value) {
			break;
		}
	}

	return i == size;
}

/*
 * Invalid: Calling bootstrap interfaces on uninitialized falcon should return
 *	    -EINVAL.
 * Invalid: Invoke nvgpu_falcon_hs_ucode_load_bootstrap with invalid ucode data
 *	    and verify that call fails.
 *
 * Valid: Invoke nvgpu_falcon_hs_ucode_load_bootstrap with initialized
 *	  falcon with ACR firmware, verify the expected state of falcon
 *	  registers - falcon_falcon_dmactl_r, falcon_falcon_bootvec_r,
 *	  falcon_falcon_cpuctl_r.
 */
int test_falcon_bootstrap(struct unit_module *m, struct gk20a *g, void *__args)
{
	/* Define a group of expected register writes */
	struct nvgpu_reg_access bootstrap_group[] = {
		{ .addr = 0x0041a10c,
			.value = falcon_falcon_dmactl_require_ctx_f(0) },
		{ .addr = 0x0041a104,
			.value = falcon_falcon_bootvec_vec_f(0) },
		{ .addr = 0x0041a100,
			.value = falcon_falcon_cpuctl_startcpu_f(1) |
				 falcon_falcon_cpuctl_hreset_f(1) },
	};
	struct acr_fw_header *fw_hdr = NULL;
	struct bin_hdr *hs_bin_hdr = NULL;
	struct nvgpu_firmware *acr_fw;
	u32 *ucode_header = NULL;
#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
	u32 boot_vector = 0xF000;
#endif
	u32 *ucode = NULL;
	u32 valid_size;
	int err;

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
	/** Invalid falcon bootstrap. */
	err = nvgpu_falcon_bootstrap(uninit_flcn, boot_vector);
	if (err != -EINVAL) {
		unit_return_fail(m, "Invalid falcon bootstrap should "
				    "fail\n");
	}

	/** Valid falcon bootstrap. */
	err = nvgpu_falcon_bootstrap(gpccs_flcn, boot_vector);
	if (err) {
		unit_return_fail(m, "GPCCS falcon bootstrap failed\n");
	}
#endif

	if (!g->ops.pmu.is_debug_mode_enabled(g)) {
		acr_fw = nvgpu_request_firmware(g, HSBIN_ACR_PROD_UCODE, 0);
		if (acr_fw == NULL) {
			unit_return_fail(m, "%s ucode get fail for %s",
				 HSBIN_ACR_PROD_UCODE, g->name);
		}
	} else {
		acr_fw = nvgpu_request_firmware(g, HSBIN_ACR_DBG_UCODE, 0);
		if (acr_fw == NULL) {
			unit_return_fail(m, "%s ucode get fail for %s",
				 HSBIN_ACR_DBG_UCODE, g->name);
		}
	}

	hs_bin_hdr = (struct bin_hdr *)(void *)acr_fw->data;
	fw_hdr = (struct acr_fw_header *)(void *)(acr_fw->data +
			hs_bin_hdr->header_offset);
	ucode_header = (u32 *)(void *)(acr_fw->data + fw_hdr->hdr_offset);
	ucode = (u32 *)(void *)(acr_fw->data + hs_bin_hdr->data_offset);

	/** Invalid falcon hs_ucode_load_bootstrap. */
	err = nvgpu_falcon_hs_ucode_load_bootstrap(uninit_flcn,
						   ucode, ucode_header);
	if (err != -EINVAL) {
		unit_return_fail(m, "Invalid falcon bootstrap should "
				    "fail\n");
	}

	/** Valid falcon hs_ucode_load_bootstrap with falcon reset failure. */
	flcn_mem_scrub_fail(gpccs_flcn);

	err = nvgpu_falcon_hs_ucode_load_bootstrap(gpccs_flcn,
						   ucode, ucode_header);
	if (err == 0) {
		unit_return_fail(m, "ACR bootstrap should have failed "
				    "as falcon reset is failed.\n");
	}

	flcn_mem_scrub_pass(gpccs_flcn);

	/**
	 * Valid falcon hs_ucode_load_bootstrap with invalid non-secure
	 * code size.
	 */
	valid_size = ucode_header[OS_CODE_SIZE];

	ucode_header[OS_CODE_SIZE] = g->ops.falcon.get_mem_size(gpccs_flcn,
								MEM_IMEM);

	ucode_header[OS_CODE_SIZE] += 4;

	err = nvgpu_falcon_hs_ucode_load_bootstrap(gpccs_flcn,
						   ucode, ucode_header);
	if (err == 0) {
		unit_return_fail(m, "ACR bootstrap should have failed "
				    "as non-secure code size > IMEM size.\n");
	}

	ucode_header[OS_CODE_SIZE] = valid_size;

	/**
	 * Valid falcon hs_ucode_load_bootstrap with invalid secure
	 * code size.
	 */
	valid_size = ucode_header[APP_0_CODE_SIZE];

	ucode_header[APP_0_CODE_SIZE] = g->ops.falcon.get_mem_size(gpccs_flcn,
								   MEM_IMEM);
	ucode_header[APP_0_CODE_SIZE] += 4;

	err = nvgpu_falcon_hs_ucode_load_bootstrap(gpccs_flcn,
						   ucode, ucode_header);
	if (err == 0) {
		unit_return_fail(m, "ACR bootstrap should have failed "
				    "as secure code size > IMEM size.\n");
	}

	ucode_header[APP_0_CODE_SIZE] = valid_size;

	/**
	 * Valid falcon hs_ucode_load_bootstrap with invalid dmem data
	 * size.
	 */
	valid_size = ucode_header[OS_DATA_SIZE];

	ucode_header[OS_DATA_SIZE] = g->ops.falcon.get_mem_size(gpccs_flcn,
								MEM_DMEM);
	ucode_header[OS_DATA_SIZE] += 4;

	err = nvgpu_falcon_hs_ucode_load_bootstrap(gpccs_flcn,
						   ucode, ucode_header);
	if (err == 0) {
		unit_return_fail(m, "ACR bootstrap should have failed "
				    "as dmem data size > DMEM size.\n");
	}

	ucode_header[OS_DATA_SIZE] = valid_size;

	/** Valid falcon hs_ucode_load_bootstrap. */
	err = nvgpu_falcon_hs_ucode_load_bootstrap(gpccs_flcn,
						   ucode, ucode_header);
	if (err) {
		unit_return_fail(m, "GPCCS falcon bootstrap failed\n");
	}

	if (falcon_check_reg_group(g, bootstrap_group,
				   ARRAY_SIZE(bootstrap_group)) == false) {
		unit_return_fail(m, "Failed checking bootstrap sequence\n");
	}

	return UNIT_SUCCESS;
}

static void flcn_irq_not_supported(struct nvgpu_falcon *flcn)
{
	flcn->is_interrupt_enabled = false;
}

static void flcn_irq_supported(struct nvgpu_falcon *flcn)
{
	flcn->is_interrupt_enabled = true;
}

static bool check_flcn_irq_status(struct nvgpu_falcon *flcn, bool enable,
				  u32 irq_mask, u32 irq_dest)
{
	u32 tmp_mask, tmp_dest;

	if (enable) {
		tmp_mask = nvgpu_posix_io_readl_reg_space(flcn->g,
				flcn->flcn_base + falcon_falcon_irqmset_r());
		tmp_dest = nvgpu_posix_io_readl_reg_space(flcn->g,
				flcn->flcn_base + falcon_falcon_irqdest_r());

		if (tmp_mask != irq_mask || tmp_dest != irq_dest) {
			return false;
		} else {
			return true;
		}
	} else {
		tmp_mask = nvgpu_posix_io_readl_reg_space(flcn->g,
				flcn->flcn_base + falcon_falcon_irqmclr_r());

		if (tmp_mask != 0xffffffff) {
			return false;
		} else {
			return true;
		}
	}
}

int test_falcon_irq(struct unit_module *m, struct gk20a *g, void *__args)
{
	struct {
		struct nvgpu_falcon *flcn;
		bool enable;
		u32 intr_mask;
		u32 intr_dest;
		void (*pre_irq)(struct nvgpu_falcon *);
		bool (*post_irq)(struct nvgpu_falcon *, bool, u32, u32);
	} test_data[] = {{uninit_flcn, true, 0, 0, NULL, NULL},
			 {gpccs_flcn, true, 0, 0, flcn_irq_not_supported, NULL},
			 {gpccs_flcn, true, 0xdeadbeee, 0xbeeedead,
			  flcn_irq_supported, check_flcn_irq_status},
			 {gpccs_flcn, false, 0xdeadbeee, 0xbeeedead,
			  flcn_irq_supported, check_flcn_irq_status} };
	int size = ARRAY_SIZE(test_data);
	bool intr_enabled;
	bool err;
	int i;

	intr_enabled = gpccs_flcn->is_interrupt_enabled;

	for (i = 0; i < size; i++) {
		if (test_data[i].pre_irq) {
			test_data[i].pre_irq(test_data[i].flcn);
		}

		nvgpu_falcon_set_irq(test_data[i].flcn, test_data[i].enable,
				     test_data[i].intr_mask,
				     test_data[i].intr_dest);

		if (test_data[i].post_irq) {
			err = test_data[i].post_irq(test_data[i].flcn,
						    test_data[i].enable,
						    test_data[i].intr_mask,
						    test_data[i].intr_dest);
			if (!err) {
				unit_return_fail(m, "falcon set_irq err");
			}
		}
	}

	gpccs_flcn->is_interrupt_enabled = intr_enabled;

	return UNIT_SUCCESS;
}

struct unit_module_test falcon_tests[] = {
	UNIT_TEST(falcon_sw_init_free, test_falcon_sw_init_free, NULL, 0),
	UNIT_TEST(falcon_get_id, test_falcon_get_id, NULL, 0),
	UNIT_TEST(falcon_reset, test_falcon_reset, NULL, 0),
	UNIT_TEST(falcon_mem_scrub, test_falcon_mem_scrub, NULL, 0),
	UNIT_TEST(falcon_idle, test_falcon_idle, NULL, 0),
	UNIT_TEST(falcon_halt, test_falcon_halt, NULL, 0),
	UNIT_TEST(falcon_mem_rw_init, test_falcon_mem_rw_init, NULL, 0),
	UNIT_TEST(falcon_mem_rw_inval_port,
		  test_falcon_mem_rw_inval_port, NULL, 0),
	UNIT_TEST(falcon_mem_rw_unaligned_cpu_buffer,
		  test_falcon_mem_rw_unaligned_cpu_buffer, NULL, 0),
	UNIT_TEST(falcon_mem_rw_range, test_falcon_mem_rw_range, NULL, 0),
	UNIT_TEST(falcon_mem_rw_fault, test_falcon_mem_rw_fault, NULL, 0),
	UNIT_TEST(falcon_mem_rw_aligned, test_falcon_mem_rw_aligned, NULL, 0),
	UNIT_TEST(falcon_mem_rw_zero, test_falcon_mem_rw_zero, NULL, 0),
	UNIT_TEST(falcon_mailbox, test_falcon_mailbox, NULL, 0),
	UNIT_TEST(falcon_bootstrap, test_falcon_bootstrap, NULL, 0),
	UNIT_TEST(falcon_irq, test_falcon_irq, NULL, 0),

	/* Cleanup */
	UNIT_TEST(falcon_free_test_env, free_falcon_test_env, NULL, 0),
};

UNIT_MODULE(falcon, falcon_tests, UNIT_PRIO_NVGPU_TEST);
