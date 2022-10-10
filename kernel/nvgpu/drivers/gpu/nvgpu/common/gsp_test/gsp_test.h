/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef GSP_TEST_H
#define GSP_TEST_H

#define GSP_TEST_DEBUG_BUFFER_QUEUE	3U
#define GSP_TEST_DMESG_BUFFER_SIZE	0xC00U

#define GSPDBG_RISCV_STRESS_TEST_FW_MANIFEST  "gsp-stress.manifest.encrypt.bin.out.bin"
#define GSPDBG_RISCV_STRESS_TEST_FW_CODE      "gsp-stress.text.encrypt.bin"
#define GSPDBG_RISCV_STRESS_TEST_FW_DATA      "gsp-stress.data.encrypt.bin"

#define GSPPROD_RISCV_STRESS_TEST_FW_MANIFEST  "gsp-stress.manifest.encrypt.bin.out.bin.prod"
#define GSPPROD_RISCV_STRESS_TEST_FW_CODE      "gsp-stress.text.encrypt.bin.prod"
#define GSPPROD_RISCV_STRESS_TEST_FW_DATA      "gsp-stress.data.encrypt.bin.prod"

#define GSP_STRESS_TEST_MAILBOX_PASS 0xAAAAAAAA

struct gsp_stress_test {
	bool load_stress_test;
	bool enable_stress_test;
	bool stress_test_fail_status;
	u32 test_iterations;
	u32 test_name;
	struct nvgpu_mem gsp_test_sysmem_block;
};

/* GSP descriptor's */
struct nvgpu_gsp_test {
	struct nvgpu_gsp *gsp;
	struct gsp_stress_test gsp_test;
};

#endif /* GSP_TEST_H */
