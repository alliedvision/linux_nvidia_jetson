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

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/io.h>
#include <nvgpu/io_usermode.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/page_allocator.h>
#include <nvgpu/pramin.h>
#include <nvgpu/enabled.h>
#include <nvgpu/dma.h>
#include <nvgpu/bug.h>
#include <nvgpu/gk20a.h>

#include "hal/bus/bus_gk20a.h"
#include "hal/pramin/pramin_init.h"

#include <nvgpu/hw/gk20a/hw_pram_gk20a.h>
#include <nvgpu/hw/gk20a/hw_bus_gk20a.h>

#ifdef CONFIG_NVGPU_DGPU

static u32 *rand_test_data;
static u32 *vidmem;

/*
 * VIDMEM_ADDRESS represents an arbitrary VIDMEM address that will be passed
 * to the PRAMIN module to set the PRAM window to.
 */
#define VIDMEM_ADDRESS 0x00100100
#define VIDMEM_SIZE    (8*SZ_1M)

/*
 * Amount of data to use in the tests.
 * Must be smaller or equal to VIDMEM_SIZE and RAND_DATA_SIZE.
 * To use multiple PRAM windows, TEST_SIZE should be > 1 MB
 */
#define TEST_SIZE (2*SZ_1M)

/* Size of the random data to generate, must be >= TEST_SIZE*/
#define RAND_DATA_SIZE (2*SZ_1M)

/* Simple pattern for memset operations */
#define MEMSET_PATTERN 0x12345678

static bool is_PRAM_range(struct gk20a *g, u32 addr)
{
	if ((addr >= pram_data032_r(0)) &&
	    (addr <= (pram_data032_r(0)+SZ_1M))) {
		return true;
	}
	return false;
}

static u32 PRAM_get_u32_index(struct gk20a *g, u32 addr)
{
	/* Offset is based on the currently set 1MB PRAM window */
	u32 offset = (g->mm.pramin_window) <<
		bus_bar0_window_target_bar0_window_base_shift_v();

	/* addr must be 32-bit aligned */
	BUG_ON(addr & 0x3);

	return (addr + offset)/sizeof(u32);
}

static u32 PRAM_read(struct gk20a *g, u32 addr)
{
	return vidmem[PRAM_get_u32_index(g, addr)];
}

static void PRAM_write(struct gk20a *g, u32 addr, u32 value)
{
	vidmem[PRAM_get_u32_index(g, addr)] = value;
}

/*
 * Write callback (for all nvgpu_writel calls). If address belongs to PRAM
 * range, route the call to our own handler, otherwise call the IO framework
 */
static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
	if (is_PRAM_range(g, access->addr)) {
		PRAM_write(g, access->addr - pram_data032_r(0), access->value);
	} else {
		nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
	}
	nvgpu_posix_io_record_access(g, access);
}

/*
 * Read callback, similar to the write callback above.
 */
static void readl_access_reg_fn(struct gk20a *g,
			    struct nvgpu_reg_access *access)
{
	if (is_PRAM_range(g, access->addr)) {
		access->value = PRAM_read(g, access->addr - pram_data032_r(0));
	} else {
		access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
	}
}

/*
 * Define all the callbacks to be used during the test. Typically all
 * write operations use the same callback, likewise for all read operations.
 */
static struct nvgpu_posix_io_callbacks pramin_callbacks = {
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

static int init_test_env(struct unit_module *m, struct gk20a *g)
{
	static bool first_init = true;
	int err = 0;

	if (!first_init) {
		/*
		 * If already initialized, just refill the test buffer with
		 * new random data
		 */
		init_rand_buffer();
		return 0;
	}
	first_init = false;

	nvgpu_init_pramin(&g->mm);

	/* Create a test buffer to be filled with random data */
	rand_test_data = (u32 *) malloc(RAND_DATA_SIZE);
	if (rand_test_data == NULL) {
		return -ENOMEM;
	}

	/* Create the VIDMEM */
	vidmem = (u32 *) malloc(VIDMEM_SIZE);
	if (vidmem == NULL) {
		err = -ENOMEM;
		goto clean_rand_data;
	}

	nvgpu_posix_register_io(g, &pramin_callbacks);

	/* Minimum HAL init for PRAMIN */
	g->ops.bus.set_bar0_window = gk20a_bus_set_bar0_window;
	nvgpu_pramin_ops_init(g);
	unit_assert(g->ops.pramin.data032_r != NULL, return -EINVAL);

	/* Register space: BUS_BAR0 */
	if (nvgpu_posix_io_add_reg_space(g, bus_bar0_window_r(), 0x100) != 0) {
		err = -ENOMEM;
		goto clean_vidmem;
	}

	init_rand_buffer();
	return 0;

clean_vidmem:
	free(vidmem);
clean_rand_data:
	free(rand_test_data);

	return err;
}

static int free_test_env(struct unit_module *m, struct gk20a *g,
				void *__args)
{
	free(rand_test_data);
	free(vidmem);
	nvgpu_posix_io_delete_reg_space(g, bus_bar0_window_r());
	return UNIT_SUCCESS;
}

static int create_alloc_and_sgt(struct unit_module *m, struct gk20a *g,
	struct nvgpu_mem *mem)
{
	struct nvgpu_sgt *sgt;

	mem->vidmem_alloc = (struct nvgpu_page_alloc *) malloc(sizeof(
		struct nvgpu_page_alloc));
	if (mem->vidmem_alloc == NULL) {
		unit_err(m, "Memory allocation failed\n");
		return -ENOMEM;
	}

	/* All we need from the SGT are the ops */
	sgt = nvgpu_sgt_create_from_mem(g, mem);
	if (sgt == NULL) {
		unit_err(m, "Memory allocation failed\n");
		free(mem->vidmem_alloc);
		return -ENOMEM;
	}
	mem->vidmem_alloc->sgt.ops = sgt->ops;
	mem->vidmem_alloc->sgt.sgl = (void *) mem;
	free(sgt);

	/* All PRAMIN accessed must have a VIDMEM aperture */
	mem->aperture = APERTURE_VIDMEM;

	return 0;
}

static struct nvgpu_mem_sgl *create_sgl(struct unit_module *m, u64 length,
	u64 phys)
{
	struct nvgpu_mem_sgl *sgl = (struct nvgpu_mem_sgl *) malloc(
		sizeof(struct nvgpu_mem_sgl));
	if (sgl == NULL) {
		unit_err(m, "Memory allocation failed\n");
		return NULL;
	}

	sgl->length = length;
	sgl->phys = phys;

	return sgl;
}

/*
 * Test case to exercize "nvgpu_pramin_rd_n". It will read TEST_SIZE bytes
 * from VIDMEM base address VIDMEM_ADDRESS. Only use one SGL in this test
 */
static int test_pramin_rd_n_single(struct unit_module *m, struct gk20a *g,
				void *__args)
{
	u32 *dest;
	struct nvgpu_mem mem = { };
	struct nvgpu_mem_sgl *sgl;
	u32 byte_cnt = TEST_SIZE;
	bool success = false;

	if (init_test_env(m, g) != 0) {
		unit_return_fail(m, "Module init failed\n");
	}

	unit_info(m, "Reading %d bytes via PRAMIN\n", byte_cnt);

	/*
	 * Get the first byte_cnt bytes from the test buffer and copy them
	 * into VIDMEM
	 */
	memcpy(((u8 *) vidmem) + VIDMEM_ADDRESS, (void *) rand_test_data,
		byte_cnt);

	/* PRAMIN will copy data into the buffer below */
	dest = malloc(byte_cnt);
	if (dest == NULL) {
		unit_return_fail(m, "Memory allocation failed\n");
	}

	if (create_alloc_and_sgt(m, g, &mem) != 0) {
		goto free_dest;
	}

	sgl = create_sgl(m, byte_cnt, VIDMEM_ADDRESS);
	if (sgl == NULL) {
		goto free_vidmem;
	}

	mem.vidmem_alloc->sgt.sgl = (void *)sgl;

	nvgpu_pramin_rd_n(g, &mem, 0, byte_cnt, (void *) dest);

	if (memcmp((void *) dest, (void *) rand_test_data, byte_cnt) != 0) {
		unit_err(m, "Mismatch comparing copied data\n");
	} else {
		success = true;
	}

	free(sgl);
free_vidmem:
	free(mem.vidmem_alloc);
free_dest:
	free(dest);

	if (success)
		return UNIT_SUCCESS;
	else
		return UNIT_FAIL;
}

/*
 * Test case to exercize "nvgpu_pramin_wr_n" with a couple of advanced cases:
 * - Use multiple SGLs
 * - Use a byte offset
 */
static int test_pramin_wr_n_multi(struct unit_module *m, struct gk20a *g,
				void *__args)
{
	u32 *src;
	struct nvgpu_mem mem = { };
	struct nvgpu_mem_sgl *sgl1, *sgl2, *sgl3;
	u32 byte_cnt = TEST_SIZE;
	u32 byte_offset = SZ_128K;
	void *vidmem_data;
	bool success = false;

	if (init_test_env(m, g) != 0) {
		unit_return_fail(m, "Module init failed\n");
	}

	/* This is where the written data should end up in VIDMEM */
	vidmem_data = ((u8 *) vidmem) + VIDMEM_ADDRESS + byte_offset;

	unit_info(m, "Writing %d bytes via PRAMIN\n", byte_cnt);

	/* Source data contains random data from test buffer */
	src = malloc(byte_cnt);
	if (src == NULL) {
		unit_return_fail(m, "Memory allocation failed\n");
	}

	memcpy((void *) src, (void *) rand_test_data, byte_cnt);

	if (create_alloc_and_sgt(m, g, &mem) != 0) {
		goto free_src;
	}

	/*
	 * If the PRAMIN access has an offset that is greater than the length
	 * of the first SGL, then PRAMIN will move to the next SGL, and so on
	 * until the the total length of encountered SGLs has reached offset.
	 * Practically for this test, it means that the total length of all
	 * SGLs + the byte offset must be greater or equal to the number of
	 * bytes to write.
	 * Below, the first SGL has a length of byte_offset, so PRAMIN will
	 * skip it. Then 2 more SGLs each cover half of the data to be copied.
	 */
	sgl1 = create_sgl(m, byte_offset, VIDMEM_ADDRESS);
	if (sgl1 == NULL) {
		goto free_vidmem;
	}

	sgl2 = create_sgl(m, byte_cnt / 2, sgl1->phys + sgl1->length);
	if (sgl2 == NULL) {
		goto free_sgl1;
	}

	sgl3 = create_sgl(m, byte_cnt / 2, sgl2->phys + sgl2->length);
	if (sgl3 == NULL) {
		goto free_sgl2;
	}

	sgl1->next = sgl2;
	sgl2->next = sgl3;
	sgl3->next = NULL;

	mem.vidmem_alloc->sgt.sgl = (void *) sgl1;

	nvgpu_pramin_wr_n(g, &mem, byte_offset, byte_cnt, (void *) src);

	if (memcmp((void *) src, vidmem_data, byte_cnt) != 0) {
		unit_err(m, "Mismatch comparing copied data\n");
	} else {
		success = true;
	}

	free(sgl3);
free_sgl2:
	free(sgl2);
free_sgl1:
	free(sgl1);
free_vidmem:
	free(mem.vidmem_alloc);
free_src:
	free(src);

	if (success)
		return UNIT_SUCCESS;
	else
		return UNIT_FAIL;
}

/*
 * Test case to exercize "nvgpu_pramin_memset"
 */
static int test_pramin_memset(struct unit_module *m, struct gk20a *g,
				void *__args)
{
	struct nvgpu_mem mem = { };
	struct nvgpu_mem_sgl *sgl;
	u32 byte_cnt = TEST_SIZE;
	u32 word_cnt = byte_cnt / sizeof(u32);
	u32 vidmem_index = VIDMEM_ADDRESS / sizeof(u32);
	bool success = false;
	u32 i;

	if (init_test_env(m, g) != 0) {
		unit_return_fail(m, "Module init failed\n");
	}

	unit_info(m, "Memsetting %d bytes in PRAM\n", byte_cnt);

	if (create_alloc_and_sgt(m, g, &mem) != 0) {
		return UNIT_FAIL;
	}

	sgl = create_sgl(m, byte_cnt, VIDMEM_ADDRESS);
	if (sgl == NULL) {
		goto free_vidmem;
	}

	mem.vidmem_alloc->sgt.sgl = (void *)sgl;

	nvgpu_pramin_memset(g, &mem, 0, byte_cnt, MEMSET_PATTERN);

	for (i = 0; i < word_cnt; i++) {
		if (vidmem[vidmem_index + i] != MEMSET_PATTERN) {
			unit_err(m,
				"Memset pattern not found at offset %d\n", i);
			goto free_sgl;
		}
	}
	success = true;

free_sgl:
	free(sgl);
free_vidmem:
	free(mem.vidmem_alloc);

	if (success)
		return UNIT_SUCCESS;
	else
		return UNIT_FAIL;
}

/*
 * Test case to exercize the special case where NVGPU is dying. In that case,
 * PRAM is not available and PRAMIN should handle the case by not trying to
 * access PRAM.
 */
static int test_pramin_nvgpu_dying(struct unit_module *m, struct gk20a *g,
				void *__args)
{
	if (init_test_env(m, g) != 0) {
		unit_return_fail(m, "Module init failed\n");
	}
	nvgpu_set_enabled(g, NVGPU_DRIVER_IS_DYING, true);
	/*
	 * When the GPU is dying, PRAMIN should prevent any accesses, so
	 * pointers to nvgpu_mem and destination data don't matter and can be
	 * left NULL. If the pramin call below causes a sigsegv, then it would
	 * be a test failure, otherwise it is a success.
	 */
	nvgpu_pramin_rd_n(g, NULL, 0, 1, NULL);

	/* Restore GPU driver state for other tests */
	nvgpu_set_enabled(g, NVGPU_DRIVER_IS_DYING, false);
	return UNIT_SUCCESS;
}
#endif

struct unit_module_test pramin_tests[] = {
#ifdef CONFIG_NVGPU_DGPU
	UNIT_TEST(nvgpu_pramin_rd_n_1_sgl, test_pramin_rd_n_single, NULL, 0),
	UNIT_TEST(nvgpu_pramin_wr_n_3_sgl, test_pramin_wr_n_multi, NULL, 0),
	UNIT_TEST(nvgpu_pramin_memset, test_pramin_memset, NULL, 0),
	UNIT_TEST(nvgpu_pramin_dying, test_pramin_nvgpu_dying, NULL, 0),
	UNIT_TEST(nvgpu_pramin_free_test_env, free_test_env, NULL, 0),
#endif
};

UNIT_MODULE(pramin, pramin_tests, UNIT_PRIO_NVGPU_TEST);
