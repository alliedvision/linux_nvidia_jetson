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

#ifndef __UNIT_FAULT_INJECTION_DMA_ALLOC_H__
#define __UNIT_FAULT_INJECTION_DMA_ALLOC_H__

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-posix-fault-injection
 *  @{
 */

/**
 * Test specification for: test_dma_alloc_init
 *
 * Description: Initialization required for dma alloc fault injection tests.
 *
 * Test Type: Other (Setup)
 *
 * Input: test_fault_injection_init() must have been called prior to this test.
 *
 * Steps:
 * - Get the pointer to the dma alloc fault injection object.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_dma_alloc_init(struct unit_module *m,
		        struct gk20a *g, void *__args);

/**
 * Test specification for: test_dma_alloc_fi_default
 *
 * Description: This test simply tests the default case of fault injection
 *              disabled for calling dma alloc routines.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_dma_alloc_init() must have been
 *        called prior to this test.
 *
 * Steps:
 * - Verify the dma alloc fault injection is disabled.
 * - Call nvgpu_dma_alloc() verify the call succeeded.
 * - Free the dma allocation.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_dma_alloc_fi_default(struct unit_module *m,
			      struct gk20a *g, void *__args);

/**
 * Test specification for: test_dma_alloc_fi_enabled
 *
 * Description: This test validates immediate fault injection for dma alloc
 *              routines.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_dma_alloc_init() must have been
 *        called prior to this test.
 *
 * Steps:
 * - Enable dma alloc fault injection immediately.
 * - Call nvgpu_dma_alloc() and verify an error is returned.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_dma_alloc_fi_enabled(struct unit_module *m,
			      struct gk20a *g, void *__args);

/**
 * Test specification for: test_dma_alloc_fi_delayed_enable
 *
 * Description: This test validates delayed enable of fault injection for dma
 *              alloc APIs.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_dma_alloc_init() must have been
 *        called prior to this test.
 *
 * Steps:
 * - Enable dma alloc fault injection for after 2 calls.
 * - Loop calling nvgpu_dma_alloc() and verify success until the 3rd call.
 * - Cleanup the dma allocation.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_dma_alloc_fi_delayed_enable(struct unit_module *m,
				     struct gk20a *g, void *__args);

#endif /* __UNIT_FAULT_INJECTION_DMA_ALLOC_H__ */
