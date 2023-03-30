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

#include <unit/unit.h>
#include <unit/io.h>

#include <nvgpu/posix/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/dma.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/global_ctx.h>

#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/dma.h>

#include "common/gr/gr_priv.h"
#include "common/gr/global_ctx_priv.h"

#include "../nvgpu-gr.h"
#include "nvgpu-gr-global-ctx.h"

#define DUMMY_SIZE	0xF0U

static int dummy_l2_flush(struct gk20a *g, bool invalidate)
{
	return -EINVAL;
}

int test_gr_global_ctx_alloc_error_injection(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err;
	u64 gpu_va;
	bool valid;
	struct nvgpu_mem *mem;
	struct nvgpu_gr_global_ctx_buffer_desc *desc;
	struct nvgpu_posix_fault_inj *dma_fi =
		nvgpu_dma_alloc_get_fault_injection();

	desc = nvgpu_gr_global_ctx_desc_alloc(g);
	if (!desc) {
		unit_return_fail(m, "failed to allocate desc");
	}

	/* No size is set in desc, should fail */
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	nvgpu_gr_global_ctx_set_size(desc, NVGPU_GR_GLOBAL_CTX_CIRCULAR,
		DUMMY_SIZE);
	/* Size of pagepool buffer is not set, should fail */
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	nvgpu_gr_global_ctx_set_size(desc, NVGPU_GR_GLOBAL_CTX_PAGEPOOL,
		DUMMY_SIZE);
	/* Size of attribute buffer is not set, should fail */
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	nvgpu_gr_global_ctx_set_size(desc, NVGPU_GR_GLOBAL_CTX_ATTRIBUTE,
		DUMMY_SIZE);
	/* Size of access map buffer is not set, should fail */
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	nvgpu_gr_global_ctx_set_size(desc, NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP,
		DUMMY_SIZE);
	/* Now, all the sizes are set in desc */

	/* Ensure mapping fails before buffers are allocated */
	gpu_va = nvgpu_gr_global_ctx_buffer_map(desc,
			NVGPU_GR_GLOBAL_CTX_CIRCULAR, NULL, 0, false);
	if (gpu_va != 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Ensure unmapping fails before buffers are allocated */
	nvgpu_gr_global_ctx_buffer_unmap(desc, NVGPU_GR_GLOBAL_CTX_CIRCULAR,
			NULL, 0);

	/* Ensure no memory handle is returned before buffers are allocated */
	mem = nvgpu_gr_global_ctx_buffer_get_mem(desc,
		NVGPU_GR_GLOBAL_CTX_CIRCULAR);
	if (mem != NULL) {
		unit_return_fail(m, "unexpected success");
	}

	/* Ensure buffer ready status is false before they are allocated */
	valid = nvgpu_gr_global_ctx_buffer_ready(desc,
		NVGPU_GR_GLOBAL_CTX_CIRCULAR);
	if (valid) {
		unit_return_fail(m, "unexpected success");
	}

	/* Fail circular ctx buffer allocation */
	nvgpu_posix_enable_fault_injection(dma_fi, true, 0);
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Fail pagepool ctx buffer allocation */
	nvgpu_posix_enable_fault_injection(dma_fi, true, 1);
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Fail attribute ctx buffer allocation */
	nvgpu_posix_enable_fault_injection(dma_fi, true, 2);
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Fail access map ctx buffer allocation */
	nvgpu_posix_enable_fault_injection(dma_fi, true, 3);
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Finally, verify successful context buffer allocation */
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err != 0) {
		unit_return_fail(m,
			"failed to allocate global context buffers");
	}

	/* Try to allocate them one more time and ensure no error */
	err = nvgpu_gr_global_ctx_buffer_alloc(g, desc);
	if (err != 0) {
		unit_return_fail(m, "failed double allocation");
	}

	/* Check buffer ready status again, should be set */
	valid = nvgpu_gr_global_ctx_buffer_ready(desc,
		NVGPU_GR_GLOBAL_CTX_CIRCULAR);
	if (!valid) {
		unit_return_fail(m, "global buffer is not ready");
	}

	valid = nvgpu_gr_global_ctx_buffer_ready(desc,
		NVGPU_GR_GLOBAL_CTX_PAGEPOOL);
	if (!valid) {
		unit_return_fail(m, "global buffer is not ready");
	}

	valid = nvgpu_gr_global_ctx_buffer_ready(desc,
		NVGPU_GR_GLOBAL_CTX_ATTRIBUTE);
	if (!valid) {
		unit_return_fail(m, "global buffer is not ready");
	}

	valid = nvgpu_gr_global_ctx_buffer_ready(desc,
		NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP);
	if (!valid) {
		unit_return_fail(m, "global buffer is not ready");
	}

	/* Cleanup */
	nvgpu_gr_global_ctx_buffer_free(g, desc);
	nvgpu_gr_global_ctx_desc_free(g, desc);

	return UNIT_SUCCESS;
}

int test_gr_global_ctx_local_ctx_error_injection(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_mem mem;
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image;
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image_bk;

	/* Allocate dummy memory */
	err = nvgpu_dma_alloc(g, DUMMY_SIZE, &mem);
	if (err) {
		unit_return_fail(m, "failed to allocate dummy memory");
	}

	/* Successful allocation of local golden context */
	err = nvgpu_gr_global_ctx_alloc_local_golden_image(g, &local_golden_image, DUMMY_SIZE);
	if (err != 0) {
		unit_return_fail(m, "failed to initialize local golden image");
	}

	nvgpu_gr_global_ctx_init_local_golden_image(g, local_golden_image, &mem, DUMMY_SIZE);

	/* Trigger flush error during context load */
	g->ops.mm.cache.l2_flush = dummy_l2_flush;
	nvgpu_gr_global_ctx_load_local_golden_image(g,
		local_golden_image, &mem);

	/* Allocate dummy local golden context image */
	err = nvgpu_gr_global_ctx_alloc_local_golden_image(g, &local_golden_image_bk, DUMMY_SIZE);
	if (err != 0) {
		unit_return_fail(m, "failed to initialize local golden image");
	}

	nvgpu_gr_global_ctx_init_local_golden_image(g, local_golden_image_bk, &mem, DUMMY_SIZE);

#ifdef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
	bool valid;

	/* Compare two images, they should match since both have zero's only */
	valid = nvgpu_gr_global_ctx_compare_golden_images(g, true, local_golden_image,
			local_golden_image_bk, DUMMY_SIZE);
	if (!valid) {
		unit_return_fail(m, "images do not match");
	}

	/* Try to match them in vidmem, should fail */
	valid = nvgpu_gr_global_ctx_compare_golden_images(g, false, local_golden_image,
			local_golden_image_bk, DUMMY_SIZE);
	if (valid) {
		unit_return_fail(m, "unexpected success");
	}

	/* Update dummy image and compare, now comparison should fail */
	*(local_golden_image_bk->context) = 0xFFU;
	valid = nvgpu_gr_global_ctx_compare_golden_images(g, true, local_golden_image,
			local_golden_image_bk, DUMMY_SIZE);
	if (valid) {
		unit_return_fail(m, "unexpected success");
	}
#endif

	/* Cleanup */
	nvgpu_gr_global_ctx_deinit_local_golden_image(g, local_golden_image);
	nvgpu_gr_global_ctx_deinit_local_golden_image(g, local_golden_image_bk);
	nvgpu_dma_free(g, &mem);

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_gr_global_ctx_tests[] = {
	UNIT_TEST(gr_global_ctx_setup, test_gr_init_setup, NULL, 0),
	UNIT_TEST(gr_global_ctx_alloc_errors, test_gr_global_ctx_alloc_error_injection, NULL, 0),
	UNIT_TEST(gr_global_ctx_local_ctx_errors, test_gr_global_ctx_local_ctx_error_injection, NULL, 0),
	UNIT_TEST(gr_global_ctx_cleanup, test_gr_remove_setup, NULL, 0),
};

UNIT_MODULE(nvgpu_gr_global_ctx, nvgpu_gr_global_ctx_tests, UNIT_PRIO_NVGPU_TEST);
